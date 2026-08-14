#!/usr/bin/env bash
#
# Lint the hand-written gateware (T4).
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2018-2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs from the dev shell and from the Nix check, so both take exactly the same
# path:
#
#   nix develop .#fpga -c fpga/tests/run-lint.sh
#   nix build .#checks.x86_64-linux.fpga-lint
#
# This is the only automated check the gateware gets in CI. Bitstream builds
# need Quartus, which is unfree, x86_64-linux only and cannot come from a
# binary cache, so it never runs on a runner — see fpga/README.md, "Why this is
# not built by CI".

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
fpga="$(dirname "$here")"
src="${1:-$fpga/src}"
waivers="$fpga/verilator-waivers.vlt"

# The project-authored modules. IPpllGenerator.v is deliberately absent: it
# instantiates Altera's altpll, which has no free simulation model, so there is
# nothing to lint it against. The black-box declaration beside it is enough for
# the top level, which is the only module that instantiates the IP.
#
# The FIFO used to need the same treatment. fifo.v replaced the dcfifo it was
# built on, so it is an ordinary module in the list below.
modules=(
    DomesdayDuplicator
    buffer
    dataGenerator
    fifo
    fx3StateMachine
    spiRegisters
)

blackboxes=(
    "$src/IPpllGenerator_bb.v"
)

failed=0

for module in "${modules[@]}"; do
    printf '%-20s ' "$module"

    # -Wall, because the default set finds almost nothing in a design this
    # small. Every warning -Wall reports on this source is either waived with a
    # reason in verilator-waivers.vlt or is a new finding that should fail.
    if output=$(verilator --lint-only -Wall \
        -I"$src" \
        --top-module "$module" \
        "$waivers" \
        "$src/$module.v" \
        "${blackboxes[@]}" 2>&1); then
        echo "OK"
    else
        echo "FAIL"
        echo "$output"
        failed=1
    fi
done

if [ "$failed" -ne 0 ]; then
    echo
    echo "Lint failed. If the finding is genuinely benign, add a waiver to" >&2
    echo "$waivers with the reason — not a bare rule name." >&2
    exit 1
fi

echo
echo "All ${#modules[@]} modules lint clean."
