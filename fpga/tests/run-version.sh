#!/usr/bin/env bash
#
# Check the gateware version stamp generator (T2).
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs from the dev shell and from the Nix check, so both take exactly the same
# path:
#
#   nix develop .#fpga -c fpga/tests/run-version.sh
#   nix build .#checks.x86_64-linux.fpga-version
#
# generate-version.sh is the only path by which a commit reaches the gateware's
# identity registers, and nothing downstream would catch it getting this wrong:
# a bad stamp compiles, programs and captures perfectly, and merely reports the
# wrong provenance for as long as nobody checks by hand. That is the same
# argument fx3/firmware/tests/descriptor-golden.sh makes for the firmware's
# product descriptor.
#
# The interesting part is the mapping from a commit string to eight bytes of
# ASCII and two flag bits, and in particular the cases that are not an
# eight-character hash: a seven-character one from Nix's shortRev, a dirty
# tree, and a build that has no commit at all.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
fpga="$(dirname "$here")"
generator="$fpga/generate-version.sh"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

failed=0

# commit | expected commit text | expected build flags
cases=(
    # The ordinary case: CMake's --short=8, clean tree. "7713495d" in ASCII,
    # commit valid, not dirty.
    "7713495d:3737313334393564:02"

    # Nix passes self.shortRev, which is seven characters. The eighth byte is
    # null rather than invented, which is the whole reason this field is text.
    "7713495:3737313334393500:02"

    # A dirty build still names the commit it started from, with the flag set.
    "7713495d-dirty:3737313334393564:03"

    # No commit: every byte null and the valid bit clear, so a reader is never
    # told about commit 00000000 as though it were a fact.
    "unknown:0000000000000000:00"

    # A full-length hash is truncated to the eight characters the field holds,
    # rather than rejected — the host compares on the common prefix anyway.
    "7713495dcafebabe0123456789abcdef01234567:3737313334393564:02"

    # Not hex, so not a commit. Notably the dirty flag is cleared too: there is
    # no commit for it to qualify.
    "v1.2.3-dirty:0000000000000000:00"
)

for entry in "${cases[@]}"; do
    commit="${entry%%:*}"
    rest="${entry#*:}"
    want_text="${rest%%:*}"
    want_flags="${rest##*:}"

    bash "$generator" "$work" "$commit"

    got_text="$(sed -n "s/^\`define GATEWARE_COMMIT_TEXT 64'h\(.*\)$/\1/p" "$work/version.vh")"
    got_flags="$(sed -n "s/^\`define GATEWARE_BUILD_FLAGS 8'h\(.*\)$/\1/p" "$work/version.vh")"

    if [ "$got_text" = "$want_text" ] && [ "$got_flags" = "$want_flags" ]; then
        echo "ok: '$commit' -> 64'h$got_text, 8'h$got_flags"
    else
        echo "FAIL: '$commit' -> 64'h$got_text, 8'h$got_flags" >&2
        echo "      expected 64'h$want_text, 8'h$want_flags" >&2
        failed=1
    fi
done

# The generated file has to be valid Verilog, not merely the right text. A
# stamp that does not parse breaks the bitstream build and nothing else here
# would notice, because every check above reads it with sed.
if command -v iverilog >/dev/null; then
    cat > "$work/parses.v" <<'EOF'
`include "version.vh"
module parses;
initial begin
	if (`GATEWARE_COMMIT_TEXT === 64'bx) $display("unusable");
	if (`GATEWARE_BUILD_FLAGS === 8'bx) $display("unusable");
end
endmodule
EOF
    if iverilog -g2005 -Wall -I"$work" -o "$work/parses.vvp" "$work/parses.v"; then
        echo "ok: the generated stamp parses as Verilog"
    else
        echo "FAIL: the generated stamp does not parse as Verilog" >&2
        failed=1
    fi
fi

if [ "$failed" -ne 0 ]; then
    exit 1
fi

echo
echo "All ${#cases[@]} version stamps generated correctly."
