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
# Two cases, because the interesting byte is computed rather than fixed:
#
#   0123abcd   a realistic 8-character short hash
#   unknown    the fallback, one character shorter, so the size byte must differ
#
# Usage: descriptor-golden.sh <generate-descriptor.sh> <golden-dir>

set -euo pipefail

generator="$1"
golden_dir="$2"

workdir="$(mktemp -d)"
trap 'rm -rf "$workdir"' EXIT

status=0

for commit in 0123abcd unknown; do
    golden="$golden_dir/descriptor-$commit.h"
    actual="$workdir/descriptor-$commit.h"

    bash "$generator" "$workdir" "$commit" > "$actual"

    if diff -u "$golden" "$actual"; then
        echo "ok: descriptor for commit '$commit' matches $golden"
    else
        echo "FAIL: descriptor for commit '$commit' differs from $golden" >&2
        status=1
    fi
done

exit "$status"
