#!/usr/bin/env python3
"""Check the SDC constrains every pin the top level maps (T4).

Domesday Duplicator - LaserDisc RF sampler
SPDX-FileCopyrightText: 2026 Simon Inns
SPDX-License-Identifier: GPL-3.0-or-later

An SDC that names fifteen of the sixteen databus pins constrains fifteen of
them and says nothing at all about the sixteenth. Quartus does not complain —
an unconstrained path is not an error, it is a path nobody asked about — so
the failure is silent, and it surfaces as a capture that is intermittently
wrong on one bit.

The only way to catch that without Quartus is to compare the two files that
have to agree: the pin mapping in the top level, and the port groups in the
SDC. This does that, by role rather than by pin number, so it also fails when
a pin is constrained as the wrong kind of thing.

It is deliberately strict about pins it has never been told about. Adding a
signal to the top level and not deciding how it is timed is exactly the
omission worth failing a build over — the fix is either a constraint or a
false path with a reason, and both are one line.
"""

import pathlib
import re
import sys

# The roles the FX3 control bus carries, from the signal map in the top level.
DATA_AVAILABLE, BUFFER_ERROR, SPI_MISO = 0, 3, 7
READ_DATA, SPI_CLOCK, SPI_MOSI, SPI_CHIP_SELECT_N, RESET_N = 1, 5, 6, 8, 10
UNUSED_OUTPUTS = (4, 11, 12)
UNUSED_INPUTS = (2, 9)

# The two pins that carry a clock out of the FPGA. They are constrained by
# create_generated_clock rather than by a delay, so they are checked for
# presence separately.
CLOCK_OUTPUT_PINS = ("GPIO1[31]", "GPIO0[33]")


def normalise(pin):
    """GPIO1[02] and GPIO1[2] name the same pin."""
    bus, index = re.match(r"(GPIO\d)\[(\d+)\]", pin).groups()
    return f"{bus}[{int(index)}]"


def read_pin_mapping(top):
    """The pins the top level drives or reads, grouped by what they carry."""
    databus = {normalise(p) for p in re.findall(r"(GPIO1\[\d+\])\s*= fx3_databus", top)}
    adc_data = {normalise(p) for p in re.findall(r"adc_databus\[\d\] = (GPIO0\[\d+\])", top)}

    control_out = {
        int(i): normalise(p)
        for p, i in re.findall(r"(GPIO1\[\d+\])\s*= fx3_control\[(\d+)\]", top)
    }
    control_in = {
        int(i): normalise(p)
        for i, p in re.findall(r"fx3_control\[(\d+)\] = (GPIO1\[\d+\])", top)
    }

    if not databus or not adc_data or not control_out or not control_in:
        sys.exit("could not read the pin mapping from the top level")

    return databus, adc_data, control_out, control_in


def sdc_port_group(sdc, name):
    """The ports named in `set <name> [get_ports { ... }]`."""
    match = re.search(rf"set {name} \[get_ports \{{(.*?)\}}\]", sdc, re.S)
    if not match:
        sys.exit(f"the SDC has no port group called {name}")
    body = match.group(1).replace("\\\n", " ")
    return {normalise(p) for p in re.findall(r"GPIO\d\[\d+\]", body)}


def sdc_false_paths(sdc):
    found = re.findall(
        r"set_false_path -(?:from|to) \[get_ports \{(GPIO\d\[\d+\]|LED\[\*\])\}\]", sdc
    )
    return {p if p == "LED[*]" else normalise(p) for p in found}


def main():
    # The source directory, so that this takes the same argument the runners
    # beside it do and can be pointed at a copy of the tree.
    if len(sys.argv) > 1:
        src = pathlib.Path(sys.argv[1])
    else:
        src = pathlib.Path(__file__).resolve().parent.parent / "src"

    top = (src / "DomesdayDuplicator.v").read_text()
    sdc = (src / "DomesdayDuplicator.SDC").read_text()

    databus, adc_data, control_out, control_in = read_pin_mapping(top)

    expected = {
        "FX3 synchronous outputs": (
            databus | {control_out[DATA_AVAILABLE], control_out[BUFFER_ERROR]},
            sdc_port_group(sdc, "fx3_synchronous_outputs"),
        ),
        "FX3 synchronous inputs": (
            {control_in[READ_DATA]},
            sdc_port_group(sdc, "fx3_synchronous_inputs"),
        ),
        "ADC databus": (
            adc_data,
            sdc_port_group(sdc, "adc_data_inputs"),
        ),
        "false-pathed pins": (
            {
                control_in[SPI_CLOCK],
                control_in[SPI_MOSI],
                control_in[SPI_CHIP_SELECT_N],
                control_in[RESET_N],
                control_out[SPI_MISO],
                "LED[*]",
            }
            | {control_in[i] for i in UNUSED_INPUTS}
            | {control_out[i] for i in UNUSED_OUTPUTS},
            sdc_false_paths(sdc),
        ),
    }

    failed = False
    for label, (want, got) in expected.items():
        missing, extra = want - got, got - want
        status = "OK" if not missing and not extra else "FAIL"
        if status == "FAIL":
            failed = True
        print(f"  {status:4}  {label}: {len(got)} of {len(want)} ports")
        if missing:
            print(f"          missing from the SDC: {sorted(missing)}")
        if extra:
            print(f"          not mapped by the top level: {sorted(extra)}")

    # Nothing the top level maps may go unmentioned
    for pin in CLOCK_OUTPUT_PINS:
        if f"[get_ports {{{pin}}}]" not in sdc:
            print(f"  FAIL  {pin} carries a clock but the SDC never names it")
            failed = True

    mapped = databus | adc_data | set(control_out.values()) | set(control_in.values())
    accounted = set().union(*(got for _, got in expected.values())) | set(CLOCK_OUTPUT_PINS)
    unmentioned = mapped - accounted
    if unmentioned:
        print(f"  FAIL  mapped by the top level, absent from the SDC: {sorted(unmentioned)}")
        failed = True
    else:
        print(f"  OK    every one of the {len(mapped)} mapped FX3 and ADC pins is accounted for")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
