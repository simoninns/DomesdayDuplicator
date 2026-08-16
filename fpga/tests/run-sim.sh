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
# There is still no whole-design testbench, because the top level instantiates
# altpll through IPpllGenerator and that needs Altera's altera_mf simulation
# library. Everything below the top level is covered: buffer.v used to be
# exempt for the same reason — it was two dcfifo instances — and replacing that
# IP with fifo.v is what brought the capture path into this suite.
#
# The pin-level behaviour of the whole design is still covered on hardware, by
# the capture-integrity procedure in TESTING.md section 5.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
fpga="${1:-$(dirname "$here")}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# testbench:path/to/module[,path/to/submodule...]
#
# A testbench compiles against its module and anything that module
# instantiates, named rather than globbed so that a testbench cannot silently
# start depending on a module nobody meant it to reach - and so that the
# three trees stay visibly separate here as well as on disk.
#
# tb_bootLoader is the exception in size and the reason the list is explicit:
# it builds the factory image's boot path exactly as the top level wires it,
# down to a model of the EPCS64, because the decision it tests is the one
# thing in this repository that a field update can never repair.
benches=(
    "tb_buffer:application/buffer,application/fifo,application/bufferMonitor"
    "tb_bufferMonitor:application/bufferMonitor"
    "tb_dataGenerator:application/dataGenerator"
    "tb_halfBandDecimator:application/halfBandDecimator"
    "tb_fifo:application/fifo"
    "tb_fx3StateMachine:application/fx3StateMachine"
    "tb_spiRegisters:common/spiRegisters"
    "tb_crc32:factory/crc32"
    "tb_flashBridge:common/flashBridge,common/sim/epcsFlashModel"
    "tb_bootLoader:factory/bootLoader,factory/crc32,common/flashBridge,common/asmiBlock,common/remoteUpdate,common/sim/cycloneive_asmiblock,common/sim/epcsFlashModel,common/sim/altremote_update"
)

failed=0

for bench in "${benches[@]}"; do
    tb="${bench%%:*}"

    sources=()
    IFS=',' read -r -a duts <<<"${bench##*:}"
    for dut in "${duts[@]}"; do
        sources+=("$fpga/$dut.v")
    done

    echo "=== $tb ==="

    # -g2005: Verilog-2001/2005. The sources predate SystemVerilog and Quartus
    # compiles them as Verilog, so simulating them as anything else would test
    # a different language to the one being synthesised.
    # -Wno-timescale: the design sources carry no `timescale directive, because
    # Quartus does not need one and adding one would be a gateware edit made for
    # the benefit of a simulator. The testbenches declare it instead, and
    # iverilog warns that the DUT inherited it. That is the intended
    # arrangement, not a finding.
    if ! iverilog -g2005 -Wall -Wno-timescale -o "$work/$tb.vvp" "$here/$tb.v" "${sources[@]}"; then
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
