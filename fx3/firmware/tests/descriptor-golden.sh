#!/usr/bin/env bash
#
# T2 golden test for generate-descriptor.sh.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The generated header is the *only* path by which a version reaches the device: the FX3
# serves USB_DESC_PRODUCT_BYTES verbatim as its product string descriptor, so a wrong length
# byte or a wrong encoding is a defect the host sees and nothing in the firmware build would
# catch. (It is not the path the earlier dead-code defect lived on — firmware_version_string
# is separate, unreferenced
# and discarded by --gc-sections. This test does not cover it, and cannot.)
#
# Three cases, because the interesting byte is computed rather than fixed:
#
#   0123abcd          a realistic 8-character short hash
#   unknown           the fallback, one character shorter, so the size byte must differ
#   1.5.0 0123abcd    a release build, which names its version before the bracket
#
# The third case is also the compatibility check. A firmware built from a tag says
# "Domesday Duplicator 1.5.0 (0123abcd)" and one built from anything else says
# "Domesday Duplicator (0123abcd)" — the commit stays in brackets at the end either way,
# which is what lets a host that only knows the older shape go on reading it. The first
# two goldens are unchanged from before the version was added, and that they still match
# byte for byte is the point of keeping them.
#
# Usage: descriptor-golden.sh <generate-descriptor.sh> <golden-dir>

set -euo pipefail

generator="$1"
golden_dir="$2"

workdir="$(mktemp -d)"
trap 'rm -rf "$workdir"' EXIT

status=0

# name:commit:release, with an empty release for the two forms that carry none.
for case in "0123abcd:0123abcd:" "unknown:unknown:" "1.5.0-0123abcd:0123abcd:1.5.0"; do
    name="${case%%:*}"
    rest="${case#*:}"
    commit="${rest%%:*}"
    release="${rest#*:}"

    golden="$golden_dir/descriptor-$name.h"
    actual="$workdir/descriptor-$name.h"

    bash "$generator" "$workdir" "$commit" "$release" > "$actual"

    if diff -u "$golden" "$actual"; then
        echo "ok: descriptor for '$name' matches $golden"
    else
        echo "FAIL: descriptor for '$name' differs from $golden" >&2
        status=1
    fi
done

exit "$status"
