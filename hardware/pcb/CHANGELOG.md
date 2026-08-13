# Hardware changelog

One entry per board revision. A revision is added here when boards are ordered, alongside
its frozen fabrication outputs in `fab/rev<REV>/` and a `hw/rev<REV>` git tag.

Note that this tracks the *hardware*. The repository's `V1.0`…`V2.4` tags are software
releases and are numbered independently.

## Unreleased

No board changes. Design files only:

- Board file converted from the KiCad 4 format to KiCad 10 (`version 20260206`). Verified
  to change no geometry other than a 42 ppm area change in the B.Cu ground pour, with
  IPC-D-356 connectivity identical before and after and DRC reporting 0 unconnected items.
  See [MIGRATION.md](MIGRATION.md).
- Schematics converted to the KiCad 10 format, with the symbol rescue applied. Verified
  against the proven board: all 59 schematic reference designators appear on it, and the
  only board item without a schematic symbol is the `G***` logo graphic.
- The 7 legacy symbol libraries converted to `.kicad_sym`, keeping their nicknames so every
  `lib_id` stayed valid. `Domesday Duplicator.kicad_pro` created, defining `HW_REV`.
  `BNC_Rosenberger.pretty` registered in `fp-lib-table`, which had been missing.
- Legacy `*.sch`, `*.lib`, `*.dcm` and `*.pro` files, KiCad's `rescue-backup/` directory,
  and the stale `PDF/` plots removed. All recoverable at the `hw/rev1.0-production` tag.
- Fabrication outputs moved from `Gerber/` to `fab/rev1.0/` and documented with provenance
  and checksums, so future revisions can sit alongside them without ambiguity.
- Added `tools/gerber-compare.py` and `tools/plot-fab.sh`, the latter now also plotting the
  schematic PDF and BOM into a revision's directory.

## rev 1.0 — fabricated 2018

The production board. Plotted with KiCad 4.0.7 on 2018-06-19, fabricated, and proven in
use — this is the board in `../Domesday Duplicator top.jpg` and `… bottom.jpg`.

- Fabrication outputs: [`fab/rev1.0/`](fab/rev1.0/) (frozen — never regenerate)
- Sources as fabricated: git tag `hw/rev1.0-production`

Analogue front end, ADC and interconnect between the RF source and the DE0-NANO and FX3
boards. No silkscreen revision marking, which is why `${HW_REV}` on F.Silkscreen is planned
for the next revision.
