#!/usr/bin/env bash
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Plot the fabrication outputs for a board revision into fab/rev<REV>/.
#
# Each revision's outputs are written once and then left alone: they are the record of
# what was actually sent to the fabricator. The script therefore refuses to write into a
# directory that already exists. That is deliberate — it is what stops a stray re-plot
# from destroying the reference set for a board that has already been made.
#
# Usage:
#   tools/plot-fab.sh 2.0            # plot into fab/rev2.0/
#   tools/plot-fab.sh 2.0 --scratch  # plot somewhere temporary to compare, changing nothing

set -euo pipefail

usage() {
    echo "usage: $(basename "$0") REVISION [--scratch DIR]" >&2
    exit 2
}

[ $# -ge 1 ] || usage
REV="$1"
shift

PCB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BOARD="$PCB_DIR/Domesday Duplicator.kicad_pcb"
SCHEMATIC="$PCB_DIR/Domesday Duplicator.kicad_sch"

SCRATCH=""
case "${1:-}" in
    --scratch) [ $# -eq 2 ] || usage; SCRATCH="$2" ;;
    "")        ;;
    *)         usage ;;
esac

if [ -n "$SCRATCH" ]; then
    OUT="$SCRATCH"
    mkdir -p "$OUT"
else
    OUT="$PCB_DIR/fab/rev$REV"
    if [ -e "$OUT" ]; then
        echo "error: $OUT already exists." >&2
        echo "Fabrication outputs are written once and kept as the record of what was" >&2
        echo "sent out. To compare a fresh plot against it, use --scratch instead." >&2
        exit 1
    fi
    mkdir -p "$OUT"
fi

[ -f "$BOARD" ] || { echo "error: board file not found: $BOARD" >&2; exit 1; }
[ -f "$SCHEMATIC" ] || { echo "error: schematic not found: $SCHEMATIC" >&2; exit 1; }

echo "Plotting revision $REV to $OUT"

kicad-cli pcb export gerbers --no-x2 --no-netlist -o "$OUT" "$BOARD"
# Millimetres, unlike the rev 1.0 drill file which went out in inches. Both are valid
# Excellon; state the units in the revision's MANIFEST.md so the fabricator is not guessing.
kicad-cli pcb export drill --format excellon --excellon-units mm -o "$OUT/" "$BOARD"

# Documentation for the revision, so a fab directory is self-contained: what was sent,
# and the schematic and parts list it was built from.
kicad-cli sch export pdf -o "$OUT/schematic.pdf" "$SCHEMATIC"
kicad-cli sch export bom --group-by Value,Footprint -o "$OUT/bom.csv" "$SCHEMATIC"

( cd "$OUT" && sha256sum ./* 2>/dev/null | grep -v 'SHA256SUMS' | sed 's|\./||' \
    | sort -k2 > SHA256SUMS.tmp && mv SHA256SUMS.tmp SHA256SUMS )

echo
echo "Wrote $(find "$OUT" -type f ! -name SHA256SUMS | wc -l) files and SHA256SUMS."

if [ -z "$SCRATCH" ]; then
    cat <<EOF

Next:
  - Write $OUT/MANIFEST.md, using fab/rev1.0/MANIFEST.md as the template.
  - Record the revision in CHANGELOG.md.
  - Tag the source once the boards are ordered:  git tag -a hw/rev$REV -m "..."
EOF
fi
