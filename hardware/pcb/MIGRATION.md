# KiCad format migration

The project was authored in KiCad 4.0.7 in 2018 and is being moved to KiCad 10. This file
records what has been done, what is left, and how each step was checked.

## Status

| Part | Format | State |
| --- | --- | --- |
| `Domesday Duplicator.kicad_pcb` | `version 20260206`, generator 10.0 | **converted** |
| `Domesday Duplicator.sch` and sub-sheets | `EESchema Schematic File Version 2` | **not converted** — needs the GUI |
| `Domesday Duplicator.pro` | KiCad 4 project | **not converted** — becomes `.kicad_pro` with the schematics |
| Symbol libraries (`*.lib`, `*-cache.lib`) | legacy | **not converted** — become `.kicad_sym` with the schematics |

## Why the board went first

`kicad-cli pcb upgrade` converts the board headlessly, so the conversion is scriptable,
reviewable and repeatable. `kicad-cli sch upgrade` cannot do the same job: it only rewrites
a file that is already in the S-expression format, and errors out on a legacy `.sch`
because it tries to save the new format back to the `.sch` path.

Do not trust `kicad-cli` output taken from the legacy schematics either. It reads them well
enough to plot symbol bodies and wires, but a netlist exported from the legacy `.sch` comes
back with empty `(components)` and `(nets)`, and the plotted PDF has no text at all — no
reference designators, no pin names, no title block. Anything derived from the legacy
schematics through the CLI is unreliable as a before-and-after baseline.

## How the board conversion was verified

Both plots were made with the *same* KiCad 10 plotter, one from the legacy board and one
from the converted board, then compared with `tools/gerber-compare.py`, which cancels out
aperture renumbering and object reordering so that only real geometry differences remain.

Every layer is identical — copper, mask, silkscreen, paste, courtyard, fab, outline — with
one exception:

- **B.Cu ground pour.** Same vertex count (2795), one vertex differs, area changed by
  0.16 mm² out of 3775 mm², i.e. 42 ppm. This is the "legacy zone fill strategy is not
  supported anymore" warning that `pcb upgrade` prints. The zone is refilled on any edit
  anyway, so it carries no design significance.

Connectivity was checked directly rather than inferred. An IPC-D-356 netlist exported from
the legacy board and from the converted board gives **398 records each, identical** — every
pad, net and test point in the same place on the same net. DRC on the converted board
independently reports **0 unconnected items**.

Structural element counts also match: 766 track segments, 49 vias, 2 zones, 349 pads, and
66 footprints (KiCad 4's `module`, renamed to `footprint`).

Reproduce with:

```sh
kicad-cli pcb export ipcd356 -o old.d356 <legacy board>
kicad-cli pcb export ipcd356 -o new.d356 "Domesday Duplicator.kicad_pcb"
diff <(grep '^3' old.d356 | sort) <(grep '^3' new.d356 | sort)
```

DRC also reports 492 rule violations. These are not migration damage:

- 409 `clearance` — the refilled zone sits ~0.02 mm inside its own declared 0.6 mm zone
  clearance. Refilling the zones in the GUI (Edit → Fill All Zones) clears these.
- 59 `lib_footprint_issues` — footprint libraries such as `Pin_Headers` are not in the
  library table. A library-table problem, not a board problem.
- The remaining 24 are silkscreen-over-copper, starved thermals and similar, all
  pre-existing on a board that was fabricated and works.

## What the as-fabricated Gerbers can and cannot tell you

`fab/rev1.0/` holds the Gerbers plotted by KiCad 4.0.7 that produced the working board.
They are frozen and must never be regenerated.

They cannot be used as a direct comparison target for a modern plot. KiCad 4 expressed a
zone fill as a filled region plus a large number of stroked outline segments; KiCad 10
emits regions alone. Replotting the *unmodified* legacy board with KiCad 10 gives 1031
objects on F.Cu against the original's 3439, and silkscreen text baselines shift by up to
0.045 mm from stroke-font changes. The copper is in the same place; the encoding is not
comparable object by object. Answering "is the copper area the same?" across that gap needs
polygon boolean operations, which `gerber-compare.py` deliberately does not attempt.

This is exactly why the originals are preserved rather than regenerated.

## Remaining work

1. **Convert the schematics in the GUI.** Open the project in KiCad 10, accept the
   conversion, work through the symbol rescue dialogue, and save. This produces
   `.kicad_sch` files, a `.kicad_pro`, and a project symbol library.

2. **Check the schematics against the board.** The board is proven, so use it as the
   reference: run Tools → Update PCB from Schematic and confirm it reports no net or
   footprint changes. That validates the converted schematics against known-good
   connectivity, which is a far stronger check than anything the legacy exporter offers.

3. **Refill the zones** (Edit → Fill All Zones) and re-run DRC. The clearance count should
   drop close to zero.

4. **Fix the footprint library table** so the `lib_footprint_issues` violations clear.

5. **Set up revision tracking**, below.

6. **Delete the legacy files** once the converted project is committed and working:
   `*.sch`, `*.pro`, `*.lib`, `*.dcm`, `*-cache.lib`, `*-rescue.lib`. They stay recoverable
   at the `hw/rev1.0-production` tag.

7. **Extend `tools/plot-fab.sh`** to plot schematic PDFs and the BOM, which only becomes
   possible once the schematics are converted.

## Revision tracking, once the project file exists

Define the revision **once** as a project text variable rather than typing it into each
title block. The schematic and the board have separate title blocks and they will drift.

- File → Project Settings → Text Variables: add `HW_REV` = `2.0`.
- Reference `${HW_REV}` in the schematic title blocks and the board title block.
- Add a text item reading `Rev ${HW_REV}` on **F.Silkscreen**. This is the single highest
  value change here: a rev 1.0 board tells you nothing about which revision it is, and
  every board made from now on should say so on the silkscreen.

Then, per fabricated revision:

- `tools/plot-fab.sh <REV>` writes `fab/rev<REV>/` and its `SHA256SUMS`. It refuses to
  write into a directory that already exists, so a proven revision cannot be clobbered.
- Write `fab/rev<REV>/MANIFEST.md` from the rev 1.0 template.
- Add an entry to `CHANGELOG.md`.
- Tag the sources once boards are ordered: `git tag -a hw/rev<REV> -m "..."`.

Hardware tags use the `hw/` prefix. The bare `V1.0`…`V2.4` tags are whole-project software
releases and mean something different.
