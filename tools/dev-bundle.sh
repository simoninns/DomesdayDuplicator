#!/usr/bin/env bash
#
# Package whatever is built locally into a development update bundle.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The packaging step of the developer update loop. Releases are built by CI from a tag;
# this is how someone working on firmware or gateware gets the same kind of artefact onto
# bench hardware in one command, without cutting a release and without taking a path so
# different from the real one that it tests nothing.
#
#   ./tools/dev-bundle.sh
#
# It builds nothing itself, deliberately: it collects what is already built. Building the
# firmware is `cmake --build fx3/firmware/build` or `nix build .#fx3-firmware`, and the
# gateware is `./fpga/build-local.sh` or `nix build .#bitstream`, and each has its own
# flags, toolchain and failure modes that this script has no business restating.
#
# What it finds, in the order it looks:
#
#   firmware   fx3/firmware/build/firmware.img, then result-firmware/firmware.img,
#              then result/firmware.img
#   gateware   fpga/build/application/*.rpd, then result-bitstream/application/*.rpd
#   vectors    fpga/build/factory/DomesdayDuplicatorFactoryConfigure.svf, then
#              result-bitstream/factory/DomesdayDuplicatorFactoryConfigure.svf
#   factory    fpga/build/factory/DomesdayDuplicatorFactory_auto.rpd, then
#              result-bitstream/factory/DomesdayDuplicatorFactory_auto.rpd
#
# A firmware-only bundle is legal by schema, so a firmware developer never needs Quartus
# for this loop; a gateware-only bundle is legal too. Nothing found at all is an error.
#
# Two kinds of set, because the release stream publishes two:
#
#   --kind update        (the default) firmware and the application gateware — what a
#                        working device installs over USB
#   --kind provisioning  firmware, the vectors that configure the factory image into an
#                        FPGA, and that image as raw flash bytes — what the bring-up
#                        wizard needs for a board with no working gateware, and what a
#                        packaged build bundles. Needs both Quartus outputs
#
# The vectors and the factory image go together and are deliberately not added to an
# update bundle: nothing on the ordinary update path plays vectors, and the firmware
# refuses the factory region to anything but the bring-up and rollback paths.
#
# The vectors configure and write nothing. What writes the flash is the firmware itself,
# over USB, once configuration has given the board a flash bridge — which is why a
# provisioning set carries an image as well as the vectors that make it reachable.
#
# The bundle is signed with the **development key**, whose secret half is committed to
# this repository (tools/keys/development.key) and is therefore public. A development
# signature proves the bundle is well formed and proves nothing whatever about where it
# came from. That is exactly why it is a separate key and a separate channel: a release
# build of the application refuses it outright, and a build that accepts it says so in the
# interface every time. There is no unsigned path — the development key *is* the
# unsigned-equivalent, made explicit and impossible to confuse with a release.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(dirname "$here")"

output=""
firmware=""
gateware=""
provisioning=""
factory=""
version=""
kind="update"

usage() {
    cat >&2 <<'EOF'
Usage: dev-bundle.sh [--kind update|provisioning] [--output FILE]
                     [--firmware FILE] [--gateware FILE] [--provisioning FILE]
                     [--factory-gateware FILE]
                     [--version X.Y.Z]

With no arguments it finds what is built locally and writes
build/domesday-duplicator-update-<version>-dev.dddfw at the repository root.

  --kind          update (default) bundles the firmware and the application gateware;
                  provisioning bundles the firmware and the JTAG vectors, which is what
                  the bring-up wizard needs and what a packaged build carries
  --firmware      use this FX3 image instead of searching
  --gateware      use this raw EPCS image instead of searching
  --provisioning  use this SVF instead of searching (--kind provisioning only)
  --factory-gateware
                  use this raw image instead of searching (--kind provisioning only)
  --version       the version to stamp; defaults to 0.0.0, which no release will ever
                  carry and which therefore cannot be mistaken for one
EOF
    exit 2
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output) output="$2"; shift 2 ;;
        --kind) kind="$2"; shift 2 ;;
        --firmware) firmware="$2"; shift 2 ;;
        --gateware) gateware="$2"; shift 2 ;;
        --provisioning) provisioning="$2"; shift 2 ;;
        --factory-gateware) factory="$2"; shift 2 ;;
        --version) version="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "unknown argument: $1" >&2; usage ;;
    esac
done

die() { echo "dev-bundle: $*" >&2; exit 1; }

case "$kind" in
    update|provisioning) ;;
    *) die "--kind must be update or provisioning, not '$kind'" ;;
esac

# First existing path, or nothing.
first_existing() {
    local candidate
    for candidate in "$@"; do
        if [[ -f "$candidate" ]]; then
            printf '%s' "$candidate"
            return 0
        fi
    done
    return 0
}

if [[ -z "$firmware" ]]; then
    firmware="$(first_existing \
        "$root/fx3/firmware/build/firmware.img" \
        "$root/result-firmware/firmware.img" \
        "$root/result/firmware.img")"
fi

if [[ "$kind" == "update" && -z "$gateware" ]]; then
    # Globbed rather than named, because the raw image's filename follows the Quartus
    # project rather than a convention this script gets to set.
    #
    # The application directory specifically, in both layouts. That is the half a device
    # update rewrites; the factory image is written by JTAG once and a bundle must never
    # carry it. It emits no .rpd today, so this is a statement of intent rather than a
    # filter — but the intent is the part worth writing down.
    for candidate in "$root"/fpga/build/application/*.rpd \
                     "$root"/result-bitstream/application/*.rpd; do
        if [[ -f "$candidate" ]]; then
            gateware="$candidate"
            break
        fi
    done
fi

if [[ "$kind" == "provisioning" ]]; then
    # An update bundle never carries vectors and a provisioning set never carries the
    # application image: the two sets are for different devices in different states, and
    # mixing them would put megabytes of SVF in front of every ordinary update.
    gateware=""

    if [[ -z "$provisioning" ]]; then
        provisioning="$(first_existing \
            "$root/fpga/build/factory/DomesdayDuplicatorFactoryConfigure.svf" \
            "$root/result-bitstream/factory/DomesdayDuplicatorFactoryConfigure.svf")"
    fi

    [[ -n "$provisioning" ]] || die "no configuration vectors are built locally.
The SVF comes out of the Quartus build beside the factory image:
  ./fpga/build-local.sh        or        nix build .#bitstream
then run this again, or pass --provisioning explicitly."

    if [[ -z "$factory" ]]; then
        factory="$(first_existing \
            "$root/fpga/build/factory/DomesdayDuplicatorFactory_auto.rpd" \
            "$root/result-bitstream/factory/DomesdayDuplicatorFactory_auto.rpd")"
    fi

    [[ -n "$factory" ]] || die "no factory image is built locally.
The raw image comes out of the same Quartus build as the vectors:
  ./fpga/build-local.sh        or        nix build .#bitstream
then run this again, or pass --factory-gateware explicitly."

    # Both halves, always. A bring-up programs the FX3 first — the ordering is what keeps
    # the original firmware from ever running underneath the current gateware — so a set
    # without firmware is one the wizard would refuse on the page that opens it.
    [[ -n "$firmware" ]] || die "a provisioning set needs the firmware as well as the vectors.
Build it with:   cmake --build fx3/firmware/build   or   nix build .#fx3-firmware
then run this again, or pass --firmware explicitly."
fi

if [[ -z "$firmware" && -z "$gateware" && -z "$provisioning" && -z "$factory" ]]; then
    die "nothing is built locally.
Build the firmware with:   cmake --build fx3/firmware/build
or the gateware with:      ./fpga/build-local.sh
then run this again, or pass --firmware / --gateware explicitly."
fi

# 0.0.0 rather than something plausible. A development bundle must be impossible to
# mistake for a release at a glance, and a version no release will ever carry does that
# more reliably than a suffix somebody might not read.
[[ -n "$version" ]] || version="0.0.0"

# The commit stamped into the manifest as the bundle's own. It describes the tree the
# bundle was assembled from and nothing else — what each payload will report is read from
# that payload below. A dirty tree is marked, because a bare hash from one names a commit
# that does not contain the code being packaged.
commit="$(git -C "$root" rev-parse --short=8 HEAD 2>/dev/null || echo unknown)"
if [[ -n "$(git -C "$root" status --porcelain 2>/dev/null)" ]]; then
    commit="$commit-dirty"
fi

[[ -n "$output" ]] ||
    output="$root/build/domesday-duplicator-$kind-$version-dev.dddfw"

arguments=(
    --output "$output"
    --version "$version"
    --commit "$commit"
    --channel development
    --secret-key "$root/tools/keys/development.key"
    --public-key "$root/tools/keys/development.pub"
    --notes "Development build from $commit — not a release."
)

# The identity a payload carries is a property of the payload, not of the tree it is
# packaged from, and the two are only the same when everything was just rebuilt. Taking it
# from git instead cost a bench run: HEAD moved between the build and the packaging, the
# manifest promised a commit nothing inside it could report, and a multi-minute gateware
# update failed at its last step with the flash already correctly written.
#
# So each identity is read out of the artefact that will have to report it.

# What the firmware will put in its USB product string, read from the image that will be
# installed. The descriptor is UTF-16, so the nulls come out before the match; tr and grep
# rather than strings, because this script depends on nothing binutils provides.
firmware_identity=""
if [[ -n "$firmware" ]]; then
    # `|| true` because no match is an answer, not an error: under pipefail an
    # empty grep would abort the script here and the explanation below would
    # never be printed.
    firmware_identity="$(LC_ALL=C tr -d '\0' <"$firmware" |
        LC_ALL=C grep -aoE 'Domesday Duplicator \([0-9A-Fa-f]{7,8}(-dirty)?\)' |
        head -1 | sed -E 's/^.*\((.*)\)$/\1/' || true)"
    [[ -n "$firmware_identity" ]] || die "cannot read a version out of $firmware.
It names no build, so a bundle carrying it could not say what installing it produces.
Rebuild the firmware, or use make-update-bundle.sh directly with --firmware-identity."
fi

# What the gateware will report through its identity registers. bitstream-provenance.txt
# sits one level above the image in both layouts — fpga/build/ and result-bitstream/ — and
# is generated from the compile rather than typed. The vectors are read the same way,
# because they come out of the same compile and land one directory over.
bitstream_identity_of() {
    local artefact="$1" provenance identity
    provenance="$(dirname "$(dirname "$artefact")")/bitstream-provenance.txt"
    if [[ -f "$provenance" ]]; then
        identity="$(sed -n 's/^[[:space:]]*commit[[:space:]]\{1,\}\([^[:space:]]\{1,\}\)[[:space:]]*$/\1/p' \
            "$provenance" | head -1 || true)"
    fi
    [[ -n "${identity:-}" ]] || die "cannot tell which commit $artefact was built from.
Expected a provenance record at $provenance, which ./fpga/build-local.sh and
nix build .#bitstream both write beside the images they compile.
Rebuild the gateware, or use make-update-bundle.sh directly with an explicit identity."
    printf '%s' "$identity"
}

gateware_identity=""
if [[ -n "$gateware" ]]; then
    gateware_identity="$(bitstream_identity_of "$gateware")"
fi

provisioning_identity=""
if [[ -n "$provisioning" ]]; then
    provisioning_identity="$(bitstream_identity_of "$provisioning")"
fi

factory_identity=""
if [[ -n "$factory" ]]; then
    factory_identity="$(bitstream_identity_of "$factory")"
fi

# Packaging artefacts older than the tree is legitimate — it is what happens whenever the
# gateware is left alone while the firmware is worked on — so this is worth saying and not
# worth refusing.
for pair in "firmware:$firmware_identity" "gateware:$gateware_identity" \
            "provisioning:$provisioning_identity" "factory:$factory_identity"; do
    name="${pair%%:*}"
    identity="${pair#*:}"
    if [[ -n "$identity" && "$identity" != "$commit" ]]; then
        echo "note: the $name was built from $identity, and this tree is at $commit." >&2
    fi
done

if [[ -n "$firmware" ]]; then
    arguments+=(--firmware "$firmware" --firmware-identity "$firmware_identity")
fi
if [[ -n "$gateware" ]]; then
    arguments+=(--gateware "$gateware" --gateware-identity "$gateware_identity")
fi
if [[ -n "$provisioning" ]]; then
    arguments+=(--provisioning "$provisioning"
                --provisioning-identity "$provisioning_identity")
fi
if [[ -n "$factory" ]]; then
    arguments+=(--factory-gateware "$factory"
                --factory-gateware-identity "$factory_identity")
fi

echo "Packaging a development $kind set from what is built locally:"
if [[ -n "$firmware" ]]; then
    echo "  firmware  ${firmware#"$root"/}"
fi
if [[ -n "$gateware" ]]; then
    echo "  gateware  ${gateware#"$root"/}"
fi
if [[ -n "$provisioning" ]]; then
    echo "  vectors   ${provisioning#"$root"/}"
fi
if [[ -n "$factory" ]]; then
    echo "  factory   ${factory#"$root"/}"
fi
if [[ "$kind" == "update" && ( -z "$firmware" || -z "$gateware" ) ]]; then
    echo "  (the other half is not built; a bundle with one component is complete)"
fi
echo

exec "$root/tools/make-update-bundle.sh" "${arguments[@]}"
