#!/usr/bin/env python3
"""Generate the half-band decimation filter's coefficient table.

Domesday Duplicator - LaserDisc RF sampler
SPDX-FileCopyrightText: 2026 Simon Inns
SPDX-License-Identifier: GPL-3.0-or-later

The anti-alias filter in front of the 2:1 decimator in
fpga/application/halfBandDecimator.v. Decimating without it folds everything
above 10 MHz down on top of the signal - 15 MHz would land on 5 MHz, which for
tape RF is directly on top of the luma FM carrier - so the filter is not a
refinement of the decimation, it is the half of it that makes the other half
honest.

The table this prints is committed into the Verilog rather than read at build
time, because gateware cannot open a file. What keeps the two from drifting is
fpga/tests/test_halfband_coefficients.py, which regenerates the table and fails
if the committed one differs.

    fpga/make-halfband-coefficients.py            # print the Verilog table
    fpga/make-halfband-coefficients.py --response # print the frequency response

Why a half-band filter, and why this one:

A half-band FIR is the filter 2:1 decimation is shaped for. Every second
coefficient either side of the centre is exactly zero and the centre is exactly
one half, so a 63-tap filter costs 16 multipliers rather than 32 - and the
centre tap, being 2^14 of a 2^15 scale, costs a shift rather than a multiply.
The cutoff is fixed at exactly a quarter of the sampling rate by the form of
the filter, which for a 40 MHz sampling rate is the 10 MHz this needs.

The length and the window were chosen by measuring, and
--response prints the measurement:

    N=63, Kaiser beta=7   0.001 dB ripple to 8 MHz, -75 dB beyond 12 MHz

Shorter filters were tried and rejected. A 31-tap reaches only -35 dB by
12 MHz, which puts an alias of a strong 13 MHz component back into the picture
at a level a decode can see; 47 taps reach -69 dB but give up 10 dB in the
11-12 MHz corner, which is the part of the band a tape's upper sidebands
actually occupy.

What no half-band can do is protect the band edge itself. The response is
antisymmetric about 10 MHz and passes exactly -6 dB there, so energy just above
10 MHz aliases to just below it at a comparable level. That is a property of
2:1 decimation rather than of this filter: the only remedy is not to decimate a
signal with content up there.
"""

import argparse
import math
import sys

# The device's sampling rate. The filter's cutoff is a quarter of this by
# construction, which is the 10 MHz the decimated stream needs.
SAMPLE_RATE_HZ = 40_000_000

# Taps, and the window that shapes them. Both are the measured choice described
# in the module docstring; changing either changes the committed table and the
# test will say so.
TAP_COUNT = 63
KAISER_BETA = 7.0

# Coefficients are scaled by 2^15 and held as signed 16-bit values. Fifteen
# bits is far more than a 10-bit converter can use - the quantisation floor of
# the table sits around -90 dB, well below the -75 dB stopband it has to
# express - and 16-bit signed is what fits the device's 18x18 multipliers with
# the pre-added sample on the other input.
COEFFICIENT_SCALE_BITS = 15


def sinc(x):
    """sin(pi x) / (pi x), and 1 at zero."""
    if abs(x) < 1e-12:
        return 1.0
    return math.sin(math.pi * x) / (math.pi * x)


def bessel_i0(x):
    """Modified Bessel function of the first kind, order zero.

    The series converges quickly for the arguments a Kaiser window uses, and
    writing it out keeps this script dependency-free - which matters because it
    runs as a test in an environment that has no numpy.
    """
    total = 1.0
    term = 1.0
    for k in range(1, 64):
        term *= (x / (2.0 * k)) ** 2
        total += term
        if term < 1e-18 * total:
            break
    return total


def kaiser_window(index, length, beta):
    last = length - 1
    position = (2.0 * index / last) - 1.0
    return bessel_i0(beta * math.sqrt(max(0.0, 1.0 - position * position))) / bessel_i0(beta)


def coefficients():
    """The quantised half-band table, most negative index first.

    The zeros are forced rather than rounded to zero. They fall out of the
    ideal response exactly, and a rounding error that left one of them at +/-1
    would cost a multiplier in the fabric to express a coefficient whose true
    value is nothing.
    """
    centre = (TAP_COUNT - 1) // 2
    scale = 1 << COEFFICIENT_SCALE_BITS

    quantised = []
    for index in range(TAP_COUNT):
        offset = index - centre
        if offset != 0 and offset % 2 == 0:
            quantised.append(0)
            continue
        ideal = 0.5 * sinc(0.5 * offset) * kaiser_window(index, TAP_COUNT, KAISER_BETA)
        quantised.append(int(round(ideal * scale)))

    # DC gain of exactly one, so that a decimated capture sits at the same black
    # level as an undecimated one. Rounding leaves the sum a few counts adrift
    # and the correction goes on the centre tap, which is the only coefficient
    # large enough to absorb it without changing the shape of the response.
    quantised[centre] += scale - sum(quantised)
    return quantised


def symmetric_pairs(table):
    """The 16 (coefficient, low index, high index) triples the fabric multiplies.

    The filter is symmetric, so each coefficient multiplies the sum of the two
    samples it applies to and one multiplier does the work of two.
    """
    centre = (TAP_COUNT - 1) // 2
    pairs = []
    for index in range(0, centre):
        if table[index] == 0:
            continue
        pairs.append((table[index], index, TAP_COUNT - 1 - index))
    return pairs


def frequency_response(table, frequency_hz):
    scale = float(1 << COEFFICIENT_SCALE_BITS)
    real = 0.0
    imaginary = 0.0
    for index, value in enumerate(table):
        angle = -2.0 * math.pi * frequency_hz * index / SAMPLE_RATE_HZ
        real += (value / scale) * math.cos(angle)
        imaginary += (value / scale) * math.sin(angle)
    return math.hypot(real, imaginary)


def decibels(magnitude):
    if magnitude <= 0.0:
        return float("-inf")
    return 20.0 * math.log10(magnitude)


def verilog_table():
    """The committed Verilog, as text.

    Emitted whole rather than as a list of numbers so that the comparison the
    test makes is against exactly the characters in the source file, which is
    the only comparison that cannot pass while the file is wrong.
    """
    table = coefficients()
    pairs = symmetric_pairs(table)
    centre = (TAP_COUNT - 1) // 2

    lines = []
    lines.append("    // Coefficient table, generated by fpga/make-halfband-coefficients.py")
    lines.append("    // and checked against it by fpga/tests/test_halfband_coefficients.py.")
    lines.append("    // Do not hand-edit: run the generator.")
    lines.append("    //")
    lines.append(f"    // {TAP_COUNT} taps, Kaiser window beta {KAISER_BETA}, scaled by 2^{COEFFICIENT_SCALE_BITS}.")
    lines.append("    // Each entry multiplies the sum of the two samples it is symmetric")
    lines.append("    // across, so 16 multipliers cover 32 taps; the centre tap is exactly")
    lines.append(f"    // half of full scale and is applied as a shift rather than a multiply.")
    lines.append("    //")
    lines.append("    //   tap pair        coefficient")
    for coefficient, low, high in pairs:
        lines.append(f"    //   {low:2d}, {high:2d}          {coefficient:6d}")
    lines.append(f"    //   {centre} (centre)     {table[centre]:6d}")
    lines.append("")

    # Packed so that a part select at 16*i reads the coefficient for taps 2i and
    # N-1-2i: index 0 is the outermost pair and index 15 the pair either side of
    # the centre. Verilog concatenation puts the first element in the most
    # significant bits, so the list is reversed to put the outermost pair at the
    # bottom - which is where the fabric's generate loop looks for it.
    parts = [f"-16'sd{-c}" if c < 0 else f"16'sd{c}" for c, _, _ in reversed(pairs)]

    lines.append(f"    localparam [{16 * len(pairs) - 1}:0] Coefficients = {{")
    # Four per line keeps it inside the 100-column limit the formatter enforces.
    for start in range(0, len(parts), 4):
        chunk = ", ".join(parts[start:start + 4])
        comma = "," if start + 4 < len(parts) else ""
        lines.append(f"        {chunk}{comma}")
    lines.append("    };")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--response", action="store_true",
                        help="print the frequency response instead of the table")
    arguments = parser.parse_args()

    table = coefficients()

    if not arguments.response:
        print(verilog_table())
        return 0

    print(f"{TAP_COUNT} taps, Kaiser beta {KAISER_BETA}, "
          f"scale 2^{COEFFICIENT_SCALE_BITS}, "
          f"{len(symmetric_pairs(table))} multipliers")
    print(f"DC gain {sum(table)} of {1 << COEFFICIENT_SCALE_BITS}")
    print()
    print("   frequency      response")
    for megahertz in (0, 1, 2, 4, 6, 8, 9, 9.5, 10, 10.5, 11, 12, 13, 15, 18, 20):
        magnitude = frequency_response(table, megahertz * 1e6)
        print(f"   {megahertz:5.1f} MHz     {decibels(magnitude):8.2f} dB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
