#!/usr/bin/env bash
#
# Build both gateware images locally, out of tree.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2018-2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   nix develop .#fpga-quartus -c ./fpga/build-local.sh [build-dir] [commit]
#
# Out of tree for a specific reason: quartus_sh rewrites the .qsf in place to
# record LAST_QUARTUS_VERSION, so compiling in the source directories dirties a
# tracked file on every build — and then scatters thirty-odd build products
# beside the sources. This copies fpga/common, fpga/application and
# fpga/factory into the build directory, keeping the layout, and works there.
#
# Two images come out, and they are not interchangeable:
#
#   the factory image      the resident boot loader, written by JTAG once when
#                          a unit is provisioned and never again;
#   the application image  the capture gateware, which is what a device update
#                          rewrites over USB.
#
# What is programmed is the provisioning .jic, which carries both at the
# addresses the EPCS layout documents. There is deliberately no .jic containing
# the application image alone: programming one would write the capture gateware
# over the factory image, leaving a unit with nothing to fall back to.
#
# `nix build .#bitstream` does the same thing hermetically and is the better
# route for a release. This script exists for the iterate-on-the-gateware case,
# where a full derivation rebuild for each attempt is the wrong trade, and for
# anyone running Quartus from a manual install rather than from Nix.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build="${1:-$here/build}"
commit="${2:-}"

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

echo "Source:  $here"
echo "Build:   $build"
echo "Quartus: $(quartus_sh --version | sed -n 2p)"
echo

rm -rf "$build"
mkdir -p "$build"
cp -r "$here/common" "$here/application" "$here/factory" "$here/provisioning" "$build/"
chmod -R u+w "$build"

# Stamp the build with the commit it came from, overwriting the copy of
# version.vh that was just copied out of common. Generated into the build
# directory and never back into the sources, so that compiling the gateware
# does not dirty a tracked file — which would then be reported by the
# provenance record as an uncommitted change, on every build, for ever.
#
# Both images include the same file, so both report the same commit. They are
# built together and provisioned together; a unit whose two halves came from
# different commits is a unit nobody can reason about.
"$here/generate-version.sh" "$build/common" $commit

echo "=== Factory image ==="
(cd "$build/factory" && quartus_sh --flow compile DomesdayDuplicatorFactory)

echo
echo "=== Application image ==="
(cd "$build/application" && quartus_sh --flow compile DomesdayDuplicator)

echo
echo "=== Provisioning image ==="
# The converter reads its inputs from beside the .cof, so the two images are
# copied in and removed again once it has run: leaving them would put a second
# copy of each .sof in the output with a different path, and a provenance
# record listing the same bitstream twice is one that has to be read carefully
# to be read at all.
cp "$build/factory/DomesdayDuplicatorFactory.sof" "$build/application/DomesdayDuplicator.sof" \
    "$build/provisioning/"
(cd "$build/provisioning" &&
    quartus_cpf -c DomesdayDuplicatorProvisioning.cof &&
    rm -f DomesdayDuplicatorFactory.sof DomesdayDuplicator.sof)

echo
echo "=== Programming vectors for the on-board USB-Blaster ==="
# The same provisioning content again, as the JTAG vectors that write it.
#
# quartus_pgm reads the .jic through Quartus; ddd-jtag reads this through
# libusb, so a board can be provisioned on a machine that has never had
# Quartus installed. It is the same conversion either way and the device
# knowledge stays here, at build time, in the tool that has it.
#
# The frequency matters more than it looks: the converter turns every wait in
# the sequence into a count of TCK cycles at the rate named here, so the same
# file emitted at 6 MHz has a third more cycles in it and exactly the same
# hundred-second erase. The player restores the intended durations from this
# declaration, so what the number costs is cycles to clock, not correctness.
(cd "$build/provisioning" &&
    quartus_cpf -c -q 4.5MHz -g 3.3 -n p \
        DomesdayDuplicatorProvisioning_write_jic.cdf \
        DomesdayDuplicatorProvisioning.svf)

echo
echo "=== Application image bytes, and the boot block that describes them ==="
#
# The raw programming data is the application image exactly as it sits in the
# flash: it is what an update writes, and what the factory image checksums
# before it hands over. quartus_cpf will not emit it without also emitting a
# .jic, and that .jic would write the application image at address zero, over
# the factory image — so it is removed here rather than left lying about for
# somebody to program by mistake.
(cd "$build/application" && quartus_cpf -c DomesdayDuplicator.cof && rm -f DomesdayDuplicator.jic)

"$here/make-boot-block.py" \
    --image "$build/application/DomesdayDuplicator_auto.rpd" \
    --output "$build/provisioning/boot-block.bin"

"$here/bitstream-provenance.py" \
    --build-dir "$build" \
    --source-dir "$here" \
    --output "$build/bitstream-provenance.txt"

echo
echo "Done."
echo
sed -n '/^Digests/,$p' "$build/bitstream-provenance.txt"
echo "Full record: $build/bitstream-provenance.txt"
echo
echo "Flash layout (from the converter, not from this script):"
sed -n '1,6p' "$build/provisioning/DomesdayDuplicatorProvisioning.map"
echo
echo "Provision a board from $build/provisioning:"
echo "  quartus_pgm DomesdayDuplicatorProvisioning_write_jic.cdf   permanent (EPCS64), both images"
echo "  ddd-jtag DomesdayDuplicatorProvisioning.svf                the same, with no Quartus"
echo
echo "Load one image volatilely over JTAG, for development:"
echo "  quartus_pgm $build/application/DomesdayDuplicator_write_sof.cdf"
echo "  quartus_pgm $build/factory/DomesdayDuplicatorFactory_write_sof.cdf"
