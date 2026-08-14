#!/usr/bin/env bash
#
# Build the bitstream locally, out of tree.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2018-2025 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   nix develop .#fpga-quartus -c ./fpga/build-local.sh
#
# Out of tree for a specific reason: quartus_sh rewrites the .qsf in place to
# record LAST_QUARTUS_VERSION, so compiling in fpga/src dirties a tracked file
# on every build — and then scatters thirty-odd build products beside the
# sources. This copies fpga/src to fpga/build first and works there.
#
# `nix build .#bitstream` does the same thing hermetically and is the better
# route for a release. This script exists for the iterate-on-the-gateware case,
# where a full derivation rebuild for each attempt is the wrong trade, and for
# anyone running Quartus from a manual install rather than from Nix.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
src="$here/src"
build="${1:-$here/build}"

for tool in quartus_sh quartus_cpf; do
    if ! command -v "$tool" >/dev/null; then
        echo "$tool is not on PATH." >&2
        echo >&2
        echo "  nix develop .#fpga-quartus -c ${BASH_SOURCE[0]}" >&2
        echo >&2
        echo "or install Quartus Prime Lite by hand — see fpga/README.md." >&2
        exit 1
    fi
done

# Quartus needs a writable home for its settings and lock files.
export HOME="${HOME:-${TMPDIR:-/tmp}}"

echo "Source:  $src"
echo "Build:   $build"
echo "Quartus: $(quartus_sh --version | sed -n 2p)"
echo

rm -rf "$build"
mkdir -p "$build"
cp "$src"/* "$build/"
chmod -R u+w "$build"

# Stamp the build with the commit it came from, overwriting the copy of
# version.vh that was just copied out of src. Generated into the build
# directory and never back into src, so that compiling the gateware does not
# dirty a tracked file — which would then be reported by the provenance record
# as an uncommitted change, on every build, for ever.
"$here/generate-version.sh" "$build"

cd "$build"

quartus_sh --flow compile DomesdayDuplicator
quartus_cpf -c DomesdayDuplicator.cof

"$here/bitstream-provenance.py" \
    --build-dir . \
    --source-dir "$here" \
    --output bitstream-provenance.txt

echo
echo "Done."
echo
sed -n '/^Digests/,$p' bitstream-provenance.txt
echo "Full record: $build/bitstream-provenance.txt"
echo
echo "Program the board from $build:"
echo "  quartus_pgm DomesdayDuplicator_write_sof.cdf   volatile"
echo "  quartus_pgm DomesdayDuplicator_write_jic.cdf   permanent (EPCS64)"
