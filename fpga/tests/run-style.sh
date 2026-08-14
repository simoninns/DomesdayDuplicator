#!/usr/bin/env bash
#
# Check the style of the hand-written gateware (T4).
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs from the dev shell and from the Nix check, so both take exactly the same
# path:
#
#   nix develop .#fpga -c fpga/tests/run-style.sh
#   nix build .#checks.x86_64-linux.fpga-style
#
# Two checks over the same files, because they catch different things:
#
#   1. verible-verilog-format --verify — is the file exactly what the formatter
#      would produce from ../.verible-format? This is the whitespace and layout
#      half, and it is not a matter of opinion: there is one answer and the tool
#      knows it. Run ./run-format.sh to make it so.
#   2. verible-verilog-lint — the style rules of ../.rules.verible_lint, which
#      are Verible's defaults (the lowRISC style guide) plus the naming rule and
#      minus the departures recorded there, each with a reason.
#
# This is style. verilator --lint-only -Wall is correctness and lives in
# run-lint.sh; neither check replaces the other, and both must pass.
#
# See docs-tech/fpga-verilog-style-plan.md for the style guide itself.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
fpga="$(dirname "$here")"
src="${1:-$fpga/src}"
tests="${2:-$fpga/tests}"

format_flags="$fpga/.verible-format"
lint_rules="$fpga/.rules.verible_lint"
lint_waivers="$fpga/verible-waivers"

# The project-authored sources, named rather than globbed.
#
# Globbing src/*.v would pull in IPpllGenerator.v and IPpllGenerator_bb.v. Those are
# MegaWizard output that AGENTS.md section 3 treats as source of truth and says not to
# reformat, so a regeneration must not be able to drag vendor output into this gate by
# appearing in a wildcard.
#
# version.vh is absent for a different reason: it is written by generate-version.sh,
# so the generator's heredoc is what has to produce the right layout. The
# fpga-version check is what covers it.
files=(
    "$src/DomesdayDuplicator.v"
    "$src/buffer.v"
    "$src/dataGenerator.v"
    "$src/fifo.v"
    "$src/fx3StateMachine.v"
    "$src/spiRegisters.v"
    "$tests/tb_buffer.v"
    "$tests/tb_dataGenerator.v"
    "$tests/tb_fifo.v"
    "$tests/tb_fx3StateMachine.v"
    "$tests/tb_spiRegisters.v"
)

failed=0
unformatted=()

echo "Formatting"
for file in "${files[@]}"; do
    printf '  %-24s ' "$(basename "$file")"

    if verible-verilog-format --flagfile="$format_flags" --verify "$file" >/dev/null 2>&1; then
        echo "OK"
    else
        echo "NEEDS FORMATTING"
        unformatted+=("$file")
        failed=1
    fi
done

echo
echo "Style rules"
for file in "${files[@]}"; do
    printf '  %-24s ' "$(basename "$file")"

    if output=$(verible-verilog-lint \
        --rules_config="$lint_rules" \
        --waiver_files="$lint_waivers" \
        "$file" 2>&1); then
        echo "OK"
    else
        count=$(printf '%s\n' "$output" | grep -c . || true)
        echo "FAIL ($count)"
        printf '%s\n' "$output" | sed 's/^/    /'
        failed=1
    fi
done

if [ "$failed" -ne 0 ]; then
    echo
    if [ "${#unformatted[@]}" -ne 0 ]; then
        echo "Formatting differs from ${format_flags#"$fpga"/}. Fix it with:" >&2
        echo "  ./fpga/tests/run-format.sh" >&2
        echo >&2
    fi
    echo "For a style finding, fix the code. If the finding is genuinely wrong about" >&2
    echo "what this code should be, waive it in ${lint_waivers#"$fpga"/} scoped to the" >&2
    echo "exact case, with the reason — not a bare rule name." >&2
    exit 1
fi

echo
echo "All ${#files[@]} files are formatted and style clean."
