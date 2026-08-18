#!/usr/bin/env bash
#
# Fetch the pinned update bundle for a packaging job.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The three packaging workflows — Flatpak, MSI, DMG — all need the same file in the same
# way, and a fetch-and-verify repeated three times is three places for a digest check to
# be omitted from. So it is here, once.
#
#   ./tools/fetch-bundled-update.sh --output build/update.dddfw
#
# What it does, and what it deliberately does not:
#
#   - reads ddd-gui/packaging/bundled-update.env, which pins one published release
#     asset by URL and SHA-256;
#   - downloads it and refuses it if the digest differs — the packaging step has no
#     signing key and nobody watching, so the digest is the whole of what makes an
#     unattended download safe;
#   - checks the first archive member is manifest.json, which is what catches the
#     download that succeeded and returned an error page;
#   - prints the path it wrote, so the caller can pass it to CMake.
#
# It does NOT build an update bundle. Firmware and the capture application are separate
# release streams (AGENTS.md §9): a gui-v* packaging job assembling a firmware artefact
# would be a second, unsigned way for one to come into existence.
#
# **An empty pin is success, not failure.** A build that bundles nothing is a legitimate
# and honest build — its bring-up wizard opens with a file picker — so an unpinned tree
# exits 0, prints nothing on stdout, and says why on stderr. Callers test for an empty
# path rather than for an exit status.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(dirname "$here")"

pin="$root/ddd-gui/packaging/bundled-update.env"
output=""
check_only=""

usage() {
    cat >&2 <<'EOF'
Usage: fetch-bundled-update.sh --output FILE [--pin FILE]
       fetch-bundled-update.sh --check [--pin FILE]

Writes the pinned update bundle to FILE and prints that path. With nothing pinned it
writes nothing, prints nothing, and exits 0.

  --check  read the pin and say whether it is well formed, without downloading anything.
           What the per-commit check runs: a half-filled pin should fail on the commit
           that made it rather than in a packaging job weeks later, and a check that
           fetched would need a network the sandbox does not have
EOF
    exit 2
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output) output="$2"; shift 2 ;;
        --pin) pin="$2"; shift 2 ;;
        --check) check_only="yes"; shift ;;
        -h|--help) usage ;;
        *) echo "unknown argument: $1" >&2; usage ;;
    esac
done

die() { echo "fetch-bundled-update: $*" >&2; exit 1; }

[[ -n "$output" || -n "$check_only" ]] || die "--output is required"
[[ -f "$pin" ]] || die "no pin file at $pin"

BUNDLED_UPDATE_TAG=""
BUNDLED_UPDATE_URL=""
BUNDLED_UPDATE_SHA256=""
# shellcheck source=/dev/null
source "$pin"

if [[ -z "$BUNDLED_UPDATE_URL" && -z "$BUNDLED_UPDATE_SHA256" ]]; then
    echo "No update bundle is pinned; this build will bundle none." >&2
    exit 0
fi

if [[ -n "$check_only" ]]; then
    # Everything below the fetch, and nothing else: the shape of the pin is what a
    # per-commit check can know without a network.
    [[ -n "$BUNDLED_UPDATE_URL" ]] ||
        die "the pin gives a SHA-256 but no URL. A digest with nothing to check is not a pin."
    [[ -n "$BUNDLED_UPDATE_SHA256" ]] ||
        die "the pin gives a URL but no SHA-256. An unverified download is not a pin."
    [[ "$BUNDLED_UPDATE_SHA256" =~ ^[0-9a-f]{64}$ ]] ||
        die "the pinned SHA-256 is not 64 lowercase hex characters: $BUNDLED_UPDATE_SHA256"
    [[ -n "$BUNDLED_UPDATE_TAG" ]] ||
        die "the pin names no release tag. A packaged build has to be traceable to one."

    echo "Pinned to ${BUNDLED_UPDATE_TAG}: ${BUNDLED_UPDATE_URL}" >&2
    exit 0
fi

# Half a pin is the dangerous state: a URL with no digest is an unverified download, and a
# digest with no URL is a check that will never run. Both are a mistake in the commit that
# made them rather than a state to work around.
[[ -n "$BUNDLED_UPDATE_URL" ]] ||
    die "the pin gives a SHA-256 but no URL. A digest with nothing to check is not a pin."
[[ -n "$BUNDLED_UPDATE_SHA256" ]] ||
    die "the pin gives a URL but no SHA-256. An unverified download is not a pin."

[[ "$BUNDLED_UPDATE_SHA256" =~ ^[0-9a-f]{64}$ ]] ||
    die "the pinned SHA-256 is not 64 lowercase hex characters: $BUNDLED_UPDATE_SHA256"

for tool in curl sha256sum; do
    command -v "$tool" >/dev/null 2>&1 || die "$tool is not on PATH"
done

mkdir -p "$(dirname "$output")"

echo "Fetching the update bundle pinned to ${BUNDLED_UPDATE_TAG:-an unnamed release}" >&2
echo "  ${BUNDLED_UPDATE_URL}" >&2

# --fail so an HTTP error is an error here rather than a file full of HTML two steps
# later; retries because a packaging job should not fail on one bad minute at a CDN.
curl --location --fail --silent --show-error \
     --retry 3 --retry-delay 5 \
     --output "$output" \
     "$BUNDLED_UPDATE_URL"

actual="$(sha256sum "$output" | cut -d' ' -f1)"
if [[ "$actual" != "$BUNDLED_UPDATE_SHA256" ]]; then
    rm -f "$output"
    die "the fetched file is not the pinned one.
  expected  $BUNDLED_UPDATE_SHA256
  got       $actual
Either the pin is stale or the asset changed under it; neither is a thing to package."
fi

# The same shape check CMake makes, made here as well so that the failure names the
# download rather than the configure step that consumed it. A bundle is an uncompressed
# ustar whose first member is manifest.json, and a tar header begins with that name.
first="$(head -c 13 "$output")"
if [[ "$first" != "manifest.json" ]]; then
    rm -f "$output"
    die "the fetched file is not an update bundle: its first archive member is not manifest.json"
fi

echo "  verified  $actual" >&2
printf '%s\n' "$output"
