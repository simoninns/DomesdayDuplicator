#!/usr/bin/env python3
"""Check the committed half-band coefficients against their generator (T1).

Domesday Duplicator - LaserDisc RF sampler
SPDX-FileCopyrightText: 2026 Simon Inns
SPDX-License-Identifier: GPL-3.0-or-later

The filter's coefficients live in two places: fpga/make-halfband-coefficients.py,
which derives them, and fpga/application/halfBandDecimator.v, which the fabric
is built from. Gateware cannot open a file, so the table has to be committed -
and a committed table is one somebody can edit without rederiving it.

So this regenerates the table and compares it with the characters in the
Verilog. It also asserts the two properties the fabric depends on structurally
rather than numerically: that the DC gain is exactly one, and that the centre
tap is exactly half of full scale. The module applies the centre tap as a shift
instead of a seventeenth multiplier, which is only correct while the second of
those holds.

The frequency response is checked here too, at the frequencies the decision to
decimate actually turns on. tb_halfBandDecimator.v measures the same response
in simulation, through the fabric and a 10-bit output; this checks the
arithmetic the fabric was given, which is where a wrong window or a wrong
length would show first and most precisely.
"""

import importlib.util
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
GENERATOR = HERE.parent / "make-halfband-coefficients.py"
MODULE = HERE.parent / "application" / "halfBandDecimator.v"

spec = importlib.util.spec_from_file_location("make_halfband", GENERATOR)
make_halfband = importlib.util.module_from_spec(spec)
spec.loader.exec_module(make_halfband)

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def main():
    table = make_halfband.coefficients()
    centre = (make_halfband.TAP_COUNT - 1) // 2
    scale = 1 << make_halfband.COEFFICIENT_SCALE_BITS

    # --- The two properties the fabric is built around ----------------------

    # A DC gain of anything but one puts a level shift on every decimated
    # capture, which reads as a black-level error rather than as a filter
    # fault.
    check(sum(table) == scale,
          f"DC gain is {sum(table)}, not {scale}")

    # halfBandDecimator.v applies the centre tap with `<<< CentreShift` rather
    # than with a multiplier. That is only the right answer while the centre
    # coefficient is exactly half of full scale.
    check(table[centre] == scale // 2,
          f"centre tap is {table[centre]}, not {scale // 2} — "
          "halfBandDecimator.v applies it as a shift and would be wrong")

    # Every second coefficient either side of the centre is exactly zero. This
    # is what makes it a half-band filter and what halves the multiplier count;
    # a table without it would need thirty-two multipliers, not sixteen.
    for index, value in enumerate(table):
        offset = index - centre
        if offset != 0 and offset % 2 == 0:
            check(value == 0, f"tap {index} should be zero and is {value}")

    check(len(make_halfband.symmetric_pairs(table)) == 16,
          "expected 16 multiplier pairs")

    # Symmetric, which is what makes the pre-add legal and the phase linear.
    for index in range(make_halfband.TAP_COUNT):
        check(table[index] == table[make_halfband.TAP_COUNT - 1 - index],
              f"tap {index} is not symmetric with tap "
              f"{make_halfband.TAP_COUNT - 1 - index}")

    # Sixteen signed bits, because that is the multiplier input the device has.
    for index, value in enumerate(table):
        check(-32768 <= value <= 32767,
              f"tap {index} is {value}, which does not fit a signed 16-bit coefficient")

    # --- The response -------------------------------------------------------
    #
    # Passband flat where a tape's signal is, stopband deep where an alias
    # would land on it. 15 MHz is the one that matters most: undecimated it is
    # noise above the signal, decimated without a filter it lands on 5 MHz,
    # directly on the luma FM carrier.
    for megahertz, low_db, high_db in [
        (0.0, -0.01, 0.01),
        (4.0, -0.01, 0.01),
        (8.0, -0.05, 0.05),
        (10.0, -6.1, -5.9),     # -6 dB at the edge, by construction
        (12.0, -1000.0, -70.0),
        (15.0, -1000.0, -70.0),
        (18.0, -1000.0, -70.0),
    ]:
        magnitude = make_halfband.frequency_response(table, megahertz * 1e6)
        response = make_halfband.decibels(magnitude)
        check(low_db <= response <= high_db,
              f"{megahertz} MHz response is {response:.2f} dB, "
              f"expected {low_db} to {high_db} dB")

    # --- The committed Verilog ----------------------------------------------

    source = MODULE.read_text(encoding="utf-8")
    expected = make_halfband.verilog_table()

    if expected not in source:
        failures.append(
            f"{MODULE.name} does not contain the generated table.\n"
            "Regenerate it with:\n"
            "    fpga/make-halfband-coefficients.py\n"
            "and paste the output over the block in the module.\n"
            "Expected to find:\n" + expected)

    # And that the module's own constants agree with the generator's, since
    # they are written out separately on both sides.
    for name, value in [
        ("TapCount", make_halfband.TAP_COUNT),
        ("ScaleBits", make_halfband.COEFFICIENT_SCALE_BITS),
    ]:
        match = re.search(rf"localparam\s+integer\s+{name}\s*=\s*(\d+)\s*;", source)
        if match is None:
            failures.append(f"{MODULE.name} does not declare {name}")
        else:
            check(int(match.group(1)) == value,
                  f"{MODULE.name} has {name} = {match.group(1)}, generator has {value}")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        print(f"\n{len(failures)} failure(s)", file=sys.stderr)
        return 1

    print(f"half-band coefficients OK "
          f"({make_halfband.TAP_COUNT} taps, "
          f"{len(make_halfband.symmetric_pairs(table))} multipliers, "
          f"DC gain {sum(table)}/{scale})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
