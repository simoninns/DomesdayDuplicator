#!/usr/bin/env bash
#
# Run the gateware testbenches (T3).
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2018-2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs from the dev shell and from the Nix check, so both take exactly the same
# path:
#
#   nix develop .#fpga -c fpga/tests/run-sim.sh
#   nix build .#checks.x86_64-linux.fpga-sim
#
# Icarus Verilog rather than Verilator: these are event-driven testbenches with
# clock delays and no C++ harness, which is what iverilog is for.
#
# There is no whole-design testbench and there cannot be a free one. The top
# level instantiates dcfifo and altpll through IPfifo/IPpllGenerator, and those
# need Altera's altera_mf simulation library — so buffer.v, which is nothing but
# two of those FIFOs and the logic that switches between them, is untested here.
# What it does is covered on hardware instead, by the capture-integrity
# procedure in TESTING.md section 5.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
fpga="$(dirname "$here")"
src="${1:-$fpga/src}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# testbench:module under test
benches=(
    "tb_dataGenerator:dataGenerator"
    "tb_fx3StateMachine:fx3StateMachine"
    "tb_spiRegisters:spiRegisters"
)

failed=0

for bench in "${benches[@]}"; do
    tb="${bench%%:*}"
    dut="${bench##*:}"

    echo "=== $tb ==="

    # -g2005: Verilog-2001/2005. The sources predate SystemVerilog and Quartus
    # compiles them as Verilog, so simulating them as anything else would test
    # a different language to the one being synthesised.
    # -Wno-timescale: the design sources carry no `timescale directive, because
    # Quartus does not need one and adding one would be a gateware edit made for
    # the benefit of a simulator. The testbenches declare it instead, and
    # iverilog warns that the DUT inherited it. That is the intended
    # arrangement, not a finding.
    if ! iverilog -g2005 -Wall -Wno-timescale -o "$work/$tb.vvp" "$here/$tb.v" "$src/$dut.v"; then
        echo "$tb: compilation failed"
        failed=1
        continue
    fi

    # The testbenches call $fatal on failure, so a non-zero exit is the signal.
    # The PASS line is checked as well, so a testbench that exits early without
    # running its assertions cannot pass by saying nothing.
    if output=$(vvp "$work/$tb.vvp" 2>&1) && grep -q "^$tb: PASS\$" <<<"$output"; then
        echo "$output"
    else
        echo "$output"
        echo "$tb: FAILED"
        failed=1
    fi
done

if [ "$failed" -ne 0 ]; then
    exit 1
fi

echo
echo "All ${#benches[@]} testbenches passed."
