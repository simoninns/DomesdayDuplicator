#!/usr/bin/env bash
#
# interop-ldf.sh — check that what the capture application writes really is an .ldf (P7-23)
#
# The unit tests prove the writer and the reader agree with each other. They cannot prove
# the file is the format ld-decode consumes, because that claim is about a different
# project's tools. This script closes that gap by making those tools do the checking:
#
#   1. flac -t              the stream decodes and its checksums verify
#   2. ld-compress --uncompress  the .ldf turns back into the exact .lds the removed packed
#                                path would have written from the same samples
#   3. metadata             mono, 16-bit, Ogg-encapsulated, sample rate stamped 40000
#   4. --analyse-test-data  the ported ramp check reads a real .ldf and reaches a verdict
#
# Step 2 is the one that matters most: byte equality against the old format means the
# samples inside the new container are, sample for sample, what the old one carried.
#
# Not part of ctest: it needs flac and ld-decode's ld-compress on PATH, and a test that
# silently passes because a tool was missing is worse than one that is run deliberately.
#
# Usage:  gui/tests/interop-ldf.sh [build-directory]
#
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

BUILD_DIR="${1:-build}"
SAMPLE_COUNT="${SAMPLE_COUNT:-4000000}"

LDFGEN="${BUILD_DIR}/tests/tools/ddd-ldfgen"
CAPTURE_APP="${BUILD_DIR}/bin/DomesdayDuplicator"

fail() { echo "FAIL: $*" >&2; exit 1; }
skip() { echo "SKIP: $*" >&2; exit 77; }

[[ -x "${LDFGEN}" ]] || fail "${LDFGEN} not found — configure with -DBUILD_TESTING=ON and build first"
command -v flac >/dev/null || skip "flac is not on PATH"
command -v ld-compress >/dev/null || skip "ld-compress is not on PATH (install ld-decode)"

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

echo "== Generating a ${SAMPLE_COUNT}-sample capture through the production FLAC writer"
"${LDFGEN}" "${WORK_DIR}/capture" "${SAMPLE_COUNT}"

echo
echo "== 1. flac -t: the stream decodes and verifies"
# --ogg is required, not optional: the flac tool infers Ogg encapsulation from the file
# extension, and .ldf is not one it knows, so without this it reads the file as native FLAC
# and reports a lost sync. Worth knowing before concluding a capture is corrupt.
flac -t --ogg "${WORK_DIR}/capture.ldf" || fail "flac -t rejected the file"

echo
echo "== 2. Stream shape: mono, 16-bit, 40000 stamped"
# Read back through a decode rather than through metaflac, which cannot open Ogg FLAC at
# all. Decoding to WAV and asking file(1) what it got checks the values that actually
# reach a consumer.
flac --ogg -d -s -o "${WORK_DIR}/capture.wav" "${WORK_DIR}/capture.ldf" || fail "the stream would not decode"
SHAPE="$(file "${WORK_DIR}/capture.wav")"
echo "   ${SHAPE#*: }"
grep -q "16 bit, mono 40000 Hz" <<<"${SHAPE}" || fail "expected 16-bit mono at a stamped 40000 Hz, got: ${SHAPE#*: }"
rm -f "${WORK_DIR}/capture.wav"

echo
echo "== 3. ld-compress round-trip against the packed 10-bit reference"
# ld-compress --uncompress writes <stem>.lds into the current directory, which is exactly
# the name the reference already has, so the reference moves out of the way first.
mv "${WORK_DIR}/capture.lds" "${WORK_DIR}/reference.lds"
( cd "${WORK_DIR}" && ld-compress --uncompress capture.ldf )
[[ -f "${WORK_DIR}/capture.lds" ]] || fail "ld-compress produced no .lds"

if cmp "${WORK_DIR}/reference.lds" "${WORK_DIR}/capture.lds"; then
  echo "   byte-identical to the .lds the removed packed path would have written"
else
  fail "the round-tripped .lds differs from the reference — the samples inside the .ldf are not what the old format carried"
fi

REFERENCE_SIZE=$(stat -c %s "${WORK_DIR}/reference.lds" 2>/dev/null || stat -f %z "${WORK_DIR}/reference.lds")
FLAC_SIZE=$(stat -c %s "${WORK_DIR}/capture.ldf" 2>/dev/null || stat -f %z "${WORK_DIR}/capture.ldf")
echo "   .lds ${REFERENCE_SIZE} bytes, .ldf ${FLAC_SIZE} bytes ($(( (FLAC_SIZE * 100) / REFERENCE_SIZE ))% of the packed size)"

echo
echo "== 4. --analyse-test-data reads a real .ldf"
if [[ -x "${CAPTURE_APP}" ]]; then
  "${LDFGEN}" "${WORK_DIR}/ramp" "${SAMPLE_COUNT}" --ramp
  QT_QPA_PLATFORM=offscreen "${CAPTURE_APP}" --analyse-test-data "${WORK_DIR}/ramp.ldf" \
    || fail "the ramp check rejected a clean test-pattern capture"
else
  echo "   (capture application not built; skipped)"
fi

echo
echo "PASS: the .ldf written by this build is readable by ld-decode's own tools"
