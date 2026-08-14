#!/usr/bin/env bash
#
# Check the timing constraints (T4).
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs from the dev shell and from the Nix check, so both take exactly the same
# path:
#
#   nix develop .#fpga -c fpga/tests/run-sdc.sh
#   nix build .#checks.x86_64-linux.fpga-sdc
#
# The SDC only ever reaches a tool during a Quartus compile, and Quartus is
# unfree, x86_64-linux only and never runs in CI — so before this check existed
# a mistyped constraint could sit in the tree until someone happened to build a
# bitstream. Two things can be checked without Quartus, and both matter:
#
#   1. It parses. An SDC is plain Tcl, so sourcing it under stubs for the
#      constraint commands proves the braces, brackets, line continuations,
#      variable references and expr are all well formed.
#   2. It is complete. check-sdc.py compares the port groups against the pin
#      mapping in the top level, because a constraint that names fifteen of
#      sixteen databus pins leaves the sixteenth unanalysed and nothing else
#      would say so.
#
# What neither can check is whether the numbers are right, or whether the
# design meets them. That needs Quartus, and the SDC's own header says which
# values are placeholders and how to confirm them.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
fpga="${1:-$(dirname "$here")}"

# Both images, because both have constraints and only one of them has ever had
# a bitstream built from it. The factory image's are shorter - it has no
# capture path to time - but it is the image that can never be repaired in the
# field, so an unconstrained pin in it is worth more attention rather than less.
projects=(
    "application:DomesdayDuplicator"
    "factory:DomesdayDuplicatorFactory"
)

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# Stubs for every constraint command the SDC uses. They record their arguments
# rather than ignoring them, so an argument list that does not parse is an
# error here rather than something Quartus finds later.
cat >"$work/stubs.tcl" <<'TCL'
set ::calls {}

proc record {name args} {
    lappend ::calls [list $name {*}$args]
}

foreach cmd {
    create_clock derive_pll_clocks create_generated_clock derive_clock_uncertainty
    set_output_delay set_input_delay set_multicycle_path set_false_path
} {
    proc $cmd {args} [format {record %s {*}$args} $cmd]
}

# The collection commands echo their argument back. Nothing here knows the
# netlist, so a port name that does not exist is Quartus's to find.
proc get_ports     {args} { return [list PORTS {*}$args] }
proc get_pins      {args} { return [list PINS {*}$args] }
proc get_clocks    {args} { return [list CLOCKS {*}$args] }
proc get_registers {args} { return [list REGISTERS {*}$args] }

source [lindex $argv 0]

array set counts {}
foreach call $::calls {
    incr counts([lindex $call 0])
}
foreach name [lsort [array names counts]] {
    puts [format "  %-24s %d" $name $counts($name)]
}

if {[llength $::calls] == 0} {
    puts stderr "the SDC parsed but issued no constraints"
    exit 1
}
TCL

for entry in "${projects[@]}"; do
    directory="${entry%%:*}"
    revision="${entry##*:}"
    sdc="$fpga/$directory/$revision.SDC"

    echo
    echo "=== $directory/$revision.SDC ==="
    echo "Parsing"

    if ! tclsh "$work/stubs.tcl" "$sdc"; then
        echo
        echo "The SDC is not valid Tcl. Quartus would fail the same way." >&2
        exit 1
    fi

    echo
    echo "Coverage"
    python3 "$here/check-sdc.py" "$fpga/$directory" "$revision"
done

echo
echo "Both images' constraints parse and cover every mapped pin."
