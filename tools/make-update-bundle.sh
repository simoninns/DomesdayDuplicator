#!/usr/bin/env bash
#
# Assemble and sign a device update bundle (.dddfw).
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This is the producer half of the format the "Update bundle format" page of the
# documentation site specifies; ddd-gui/src/capture/update_bundle.cpp is the consumer half,
# and the two are held together by that page rather than by shared code — one is a shell
# script run once per release, the other is C++ that runs on a user's machine.
#
#   ./tools/make-update-bundle.sh --output build/domesday-duplicator-update-1.4.0.dddfw \
#       --version 1.4.0 --commit "$(git rev-parse --short=8 HEAD)" \
#       --channel release --secret-key "$KEY" \
#       --firmware result-firmware/firmware.img --firmware-identity 0123abcd \
#       --notes "Jumper-free firmware updates."
#
# What it produces, in this order — the order is part of the format, because a reader that
# searched for the manifest could verify one entry while an extractor that took the first
# match used another:
#
#   manifest.json       the description, and the SHA-256 of every payload
#   manifest.minisig    a detached Ed25519 signature over manifest.json
#   firmware.img        present when --firmware was given
#   gateware-app.rpd    present when --gateware was given
#
# Requirements: bash, coreutils, GNU tar and minisign. All four are in `nix develop`; on a
# distribution they are the tar and minisign packages. Nothing here is Nix-only.
#
# Reproducibility: the archive fixes every timestamp and ownership field, so the same
# inputs produce the same bytes — except for the manifest's "created", which defaults to
# now. Pass --created (or set SOURCE_DATE_EPOCH) to make a build byte-reproducible.

set -euo pipefail

output=""
version=""
commit=""
channel=""
secret_key=""
public_key=""
notes=""
created=""
firmware=""
firmware_identity=""
firmware_interface_version="1"
gateware=""
gateware_identity=""
gateware_interface_version="2"
minimum_application_version=""
minimum_register_map_version="1"
epcs_layout_version="1"

usage() {
    cat >&2 <<'EOF'
Usage: make-update-bundle.sh --output FILE --version X.Y.Z --commit HASH
                             --channel release|development --secret-key FILE
                             [--public-key FILE]
                             [--notes LINE] [--created ISO8601]
                             [--firmware FILE --firmware-identity HASH
                              --firmware-interface-version N]
                             [--gateware FILE --gateware-identity HASH
                              --gateware-interface-version N]
                             [--minimum-application-version X.Y.Z]
                             [--minimum-register-map-version N]
                             [--epcs-layout-version N]

At least one of --firmware and --gateware is required; a firmware-only bundle is a
complete bundle and is what the development loop produces.

  --firmware-interface-version   the USB protocol version this firmware advertises in
                                 bcdDevice once installed
  --gateware-interface-version   the register-map version this gateware reports at
                                 register 0x01 once installed
  --minimum-application-version  the oldest ddd-gui that may install this bundle;
                                 defaults to --version, which is the safe reading
  --public-key                   the matching public key, for the self-check at the end.
                                 Derived from the secret key when not given, which costs
                                 a second unlock if the key has a password
EOF
    exit 2
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output) output="$2"; shift 2 ;;
        --version) version="$2"; shift 2 ;;
        --commit) commit="$2"; shift 2 ;;
        --channel) channel="$2"; shift 2 ;;
        --secret-key) secret_key="$2"; shift 2 ;;
        --public-key) public_key="$2"; shift 2 ;;
        --notes) notes="$2"; shift 2 ;;
        --created) created="$2"; shift 2 ;;
        --firmware) firmware="$2"; shift 2 ;;
        --firmware-identity) firmware_identity="$2"; shift 2 ;;
        --firmware-interface-version) firmware_interface_version="$2"; shift 2 ;;
        --gateware) gateware="$2"; shift 2 ;;
        --gateware-identity) gateware_identity="$2"; shift 2 ;;
        --gateware-interface-version) gateware_interface_version="$2"; shift 2 ;;
        --minimum-application-version) minimum_application_version="$2"; shift 2 ;;
        --minimum-register-map-version) minimum_register_map_version="$2"; shift 2 ;;
        --epcs-layout-version) epcs_layout_version="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "unknown argument: $1" >&2; usage ;;
    esac
done

die() { echo "make-update-bundle: $*" >&2; exit 1; }

[[ -n "$output" ]] || die "--output is required"
[[ -n "$version" ]] || die "--version is required"
[[ -n "$commit" ]] || die "--commit is required"
[[ -n "$secret_key" ]] || die "--secret-key is required"
[[ -n "$firmware" || -n "$gateware" ]] || die "nothing to bundle: pass --firmware, --gateware or both"

case "$channel" in
    release|development) ;;
    "") die "--channel is required" ;;
    # Refused rather than defaulted. The channel is the promise the signature makes, and
    # a bundle that got it wrong by omission would be a development build claiming to be
    # a release or the reverse.
    *) die "--channel must be release or development, not '$channel'" ;;
esac

# A dotted numeric version, because this is the one thing in the update chain that orders.
# Commit hashes identify a build and order nothing, which is why they are a separate field.
[[ "$version" =~ ^[0-9]+(\.[0-9]+)*$ ]] || die "--version must be dotted numbers, not '$version'"

[[ -n "$minimum_application_version" ]] || minimum_application_version="$version"
[[ "$minimum_application_version" =~ ^[0-9]+(\.[0-9]+)*$ ]] ||
    die "--minimum-application-version must be dotted numbers"

[[ -z "$firmware" || -n "$firmware_identity" ]] ||
    die "--firmware needs --firmware-identity: the commit the device will report once it is installed"
[[ -z "$gateware" || -n "$gateware_identity" ]] ||
    die "--gateware needs --gateware-identity"

for tool in tar minisign sha256sum; do
    command -v "$tool" >/dev/null 2>&1 ||
        die "$tool is not on PATH (nix develop, or install tar/minisign/coreutils)"
done

if [[ -z "$created" ]]; then
    # SOURCE_DATE_EPOCH is the convention for "pretend it is this moment", and honouring it
    # is what lets a release be rebuilt byte for byte.
    if [[ -n "${SOURCE_DATE_EPOCH:-}" ]]; then
        created="$(date -u -d "@$SOURCE_DATE_EPOCH" +%Y-%m-%dT%H:%M:%SZ)"
    else
        created="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    fi
fi

# JSON string escaping. The manifest's strings are filenames, hashes, versions and one
# release-notes line, so backslash and quote are the whole of what needs escaping — and a
# control character is refused rather than escaped, because there is no legitimate way for
# one to reach a field of this manifest.
json_escape() {
    local text="$1"
    if [[ "$text" == *[$'\x01'-$'\x1f']* ]]; then
        die "a control character in '$text' — manifest strings are one line of plain text"
    fi
    text="${text//\\/\\\\}"
    text="${text//\"/\\\"}"
    printf '%s' "$text"
}

digest_of() { sha256sum "$1" | cut -d' ' -f1; }
length_of() { wc -c <"$1" | tr -d ' '; }

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

# Payloads are copied under the names the format fixes, so the manifest's "file" fields and
# the archive's entry names cannot drift apart.
#
# Made writable after the copy, because cp takes the source's mode and a payload built by
# Nix arrives from the store read-only. The verification pass below extracts the finished
# bundle back over this directory, which cannot overwrite a file it is not allowed to
# write — and result-firmware/firmware.img and result-bitstream/… are the documented way
# to build one, so this is the ordinary path rather than an odd one.
if [[ -n "$firmware" ]]; then
    [[ -f "$firmware" ]] || die "no such firmware image: $firmware"
    cp "$firmware" "$stage/firmware.img"
    chmod u+w "$stage/firmware.img"
fi
if [[ -n "$gateware" ]]; then
    [[ -f "$gateware" ]] || die "no such gateware image: $gateware"
    cp "$gateware" "$stage/gateware-app.rpd"
    chmod u+w "$stage/gateware-app.rpd"
fi

# The manifest. Written by hand rather than by a JSON library so that this script needs
# nothing but coreutils — and laid out exactly as ddd-gui's writer lays it out, so the two
# can be diffed against each other when one of them is wrong.
{
    printf '{\n'
    printf '  "manifest_version": 1,\n'
    printf '  "channel": "%s",\n' "$(json_escape "$channel")"
    printf '  "version": "%s",\n' "$(json_escape "$version")"
    printf '  "commit": "%s",\n' "$(json_escape "$commit")"
    printf '  "created": "%s",\n' "$(json_escape "$created")"
    printf '  "release_notes": "%s",\n' "$(json_escape "$notes")"
    printf '  "components": {\n'

    component_separator=""
    if [[ -n "$firmware" ]]; then
        printf '    "firmware": {\n'
        printf '      "file": "firmware.img",\n'
        printf '      "length": %s,\n' "$(length_of "$stage/firmware.img")"
        printf '      "sha256": "%s",\n' "$(digest_of "$stage/firmware.img")"
        printf '      "identity": "%s",\n' "$(json_escape "$firmware_identity")"
        printf '      "interface_version": %s\n' "$firmware_interface_version"
        printf '    }'
        component_separator=",\n"
    fi
    if [[ -n "$gateware" ]]; then
        printf "%b" "$component_separator"
        printf '    "gateware": {\n'
        printf '      "file": "gateware-app.rpd",\n'
        printf '      "length": %s,\n' "$(length_of "$stage/gateware-app.rpd")"
        printf '      "sha256": "%s",\n' "$(digest_of "$stage/gateware-app.rpd")"
        printf '      "identity": "%s",\n' "$(json_escape "$gateware_identity")"
        printf '      "interface_version": %s\n' "$gateware_interface_version"
        printf '    }'
    fi

    printf '\n  },\n'
    printf '  "compatibility": {\n'
    printf '    "minimum_application_version": "%s",\n' \
        "$(json_escape "$minimum_application_version")"
    printf '    "minimum_register_map_version": %s,\n' "$minimum_register_map_version"
    printf '    "epcs_layout_version": %s\n' "$epcs_layout_version"
    printf '  }\n'
    printf '}\n'
} >"$stage/manifest.json"

bundle_name="$(basename "$output")"

# The trusted comment is covered by a second signature, so it is the only text in a
# signature file the application may show a user. It says what the bundle claims to be, so
# that a mislabelled file is visible before it is opened.
minisign -Sm "$stage/manifest.json" \
    -s "$secret_key" \
    -x "$stage/manifest.minisig" \
    -c "Domesday Duplicator update bundle $version" \
    -t "$bundle_name version $version channel $channel" >/dev/null

# Order is fixed by naming the members explicitly; --sort would put manifest.json third.
# Every ownership and timestamp field is pinned so that the same inputs give the same
# archive: a bundle is content addressed, and who assembled it is not part of its meaning.
entries=(manifest.json manifest.minisig)
if [[ -n "$firmware" ]]; then
    entries+=(firmware.img)
fi
if [[ -n "$gateware" ]]; then
    entries+=(gateware-app.rpd)
fi

mkdir -p "$(dirname "$output")"
tar --create --file "$output" \
    --format=ustar --numeric-owner --owner=0 --group=0 --mtime=@0 \
    --directory "$stage" "${entries[@]}"

# --- self-check ---------------------------------------------------------------------
#
# Everything below re-reads the finished file with stock tools. It is not ceremony: this
# is the point in the chain where a wrong payload would be bundled with a right manifest,
# and the check costs milliseconds against an artefact that is about to be signed into a
# release and written to somebody's hardware.

listed="$(tar --list --file "$output" | tr '\n' ' ')"
expected="${entries[*]} "
[[ "$listed" == "$expected" ]] ||
    die "the bundle's entries came out as '$listed', expected '$expected'"

# The public key is regenerated from the secret key when one was not supplied, so the
# verification cannot accidentally be done against a key that did not sign this.
if [[ -z "$public_key" ]]; then
    public_key="$stage/derived.pub"
    minisign -R -f -s "$secret_key" -p "$public_key" >/dev/null
fi

tar --extract --file "$output" --directory "$stage" --overwrite
minisign -Vm "$stage/manifest.json" -p "$public_key" -x "$stage/manifest.minisig" \
    >/dev/null || die "the bundle's own signature does not verify"

# And every payload, as it came back out of the archive, against the digest the manifest
# records for it.
check_payload() {
    local entry="$1" digest
    digest="$(digest_of "$stage/$entry")"
    grep -q "\"sha256\": \"$digest\"" "$stage/manifest.json" ||
        die "$entry in the bundle does not match its digest in the manifest"
}

echo "$output"
echo "  channel   $channel"
echo "  version   $version ($commit)"
if [[ -n "$firmware" ]]; then
    check_payload firmware.img
    echo "  firmware  $(digest_of "$stage/firmware.img")"
fi
if [[ -n "$gateware" ]]; then
    check_payload gateware-app.rpd
    echo "  gateware  $(digest_of "$stage/gateware-app.rpd")"
fi
echo "  verified  signature and every payload digest, re-read from the finished file"
