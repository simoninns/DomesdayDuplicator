# KiCad format migration

The project was authored in KiCad 4.0.7 in 2018 and has been moved to KiCad 10. This file
records what was done, how each step was checked, and what is still outstanding.

## Status

The migration is complete. Every file is on a current KiCad format.

| Part | Format |
| --- | --- |
| `Domesday Duplicator.kicad_pcb` | `version 20260206`, generator 10.0 |
| `Domesday Duplicator.kicad_sch` and 4 sub-sheets | S-expression schematics |
| `Domesday Duplicator.kicad_pro` | current project format |
| 7 symbol libraries (`*.kicad_sym`) | current symbol format |

The legacy `*.sch`, `*.lib`, `*.dcm` and `*.pro` files have been deleted, along with the
`rescue-backup/` directory KiCad wrote during the symbol rescue. All of it remains
recoverable at the `hw/rev1.0-production` tag, which is the point of tagging it.

## How it was done

The board went first, via `kicad-cli pcb upgrade`, which converts it headlessly so the
conversion is scriptable, reviewable and repeatable.

The schematics could not go the same way. `kicad-cli sch upgrade` only rewrites a file
already in the S-expression format and errors out on a legacy `.sch`, because it tries to
save the new format back to the `.sch` path. They were converted in the GUI, which also ran
the symbol rescue.

The symbol libraries were then converted with `kicad-cli sym upgrade`, one `.lib` to one
`.kicad_sym`, and `sym-lib-table` was repointed at them with `type "KiCad"`. The library
nicknames were kept exactly as they were, so every `lib_id` in the schematics stayed valid
without touching a single symbol instance.

### A caveat about baselines

A netlist exported from a legacy `.sch` by KiCad 10 comes back with empty `(components)`
and `(nets)`, against 59 components and 148 nets from the converted schematics. Do not use
`kicad-cli` output taken from legacy schematics as a before-and-after baseline; it is not
reliable enough to compare against.

Do not read too much into a *plotted* legacy schematic either, but not because it is empty:
plotting a legacy `.sch` produces a PDF that does contain text. If you rasterise one of
these PDFs to eyeball it, note that Inkscape silently drops the text and shows only symbol
bodies and wires. That is an Inkscape artifact, not a KiCad one. Check for text with the
PDF's show-text operators rather than by looking at a rendered PNG.

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

## How the schematic conversion was verified

The board is proven hardware, so it is the reference the schematics are checked against,
not the other way round. Every reference designator in the converted schematics appears on
the board, and the only thing on the board with no schematic symbol is `G***`, the logo
graphic — which is correct, as it has no electrical existence.

```
schematic refdes: 59    board refdes: 60
in board only: ['G***']    in schematic only: []
```

The schematics also now export a netlist of 59 components and 148 nets, where the legacy
files gave 0 and 0.

ERC reports 62 violations: 58 `footprint_link_issues`, 2 `power_pin_not_driven` and 2
`ground_pin_not_ground`. See below for the footprint links; the four power and ground pin
violations are pre-existing and need a `PWR_FLAG` or equivalent, not a schematic fix.

## Outstanding

1. **Footprint library links.** The board's footprints name KiCad 4-era *global* libraries
   — `Capacitors_SMD`, `Resistors_SMD`, `Housings_SSOP` and so on — which current KiCad
   renamed (`Capacitor_SMD`, `Resistor_SMD`, `Package_SO`). That is what the 58 ERC
   `footprint_link_issues` and 59 DRC `lib_footprint_issues` are reporting.

   This does not affect manufacturing. Footprints are embedded in the `.kicad_pcb`, so the
   board is complete and plots correctly; only the link back to a library is broken, which
   matters when you want to update a footprint from its library. Fixing it means remapping
   around 60 footprints to current library names, which is a deliberate change deserving
   its own commit and its own before-and-after connectivity check.

   The two *project* footprint libraries, `Logo` and `BNC_Rosenberger`, are both correctly
   registered in `fp-lib-table`.

2. **Refill the zones** (Edit → Fill All Zones) and re-run DRC. That should clear the 409
   clearance violations left by the best-effort zone conversion.

3. **Set the revision on the silkscreen**, below. `HW_REV` is defined but not yet
   referenced from the board or the title blocks.

## Revision tracking

The revision is defined **once**, as the project text variable `HW_REV` in
`Domesday Duplicator.kicad_pro`. It is currently `1.0`, because the design is still
electrically identical to the board that was fabricated in 2018. Bump it with the first
real design change.

Defining it once matters: the schematic and the board have separate title blocks, so a
revision typed into each will eventually disagree with itself.

Still to wire up, in the GUI:

- Reference `${HW_REV}` in the schematic title blocks and the board title block.
- Add a text item reading `Rev ${HW_REV}` on **F.Silkscreen**. This is the single highest
  value change here: a rev 1.0 board tells you nothing about which revision it is, and
  every board made from now on should say so on the silkscreen.

Then, per fabricated revision:

- `tools/plot-fab.sh <REV>` writes `fab/rev<REV>/` — Gerbers, drill, `schematic.pdf`,
  `bom.csv` and `SHA256SUMS`. It refuses to write into a directory that already exists, so
  a proven revision cannot be clobbered. Use `--scratch DIR` to plot for comparison.
- Write `fab/rev<REV>/MANIFEST.md` from the rev 1.0 template.
- Add an entry to `CHANGELOG.md`.
- Tag the sources once boards are ordered: `git tag -a hw/rev<REV> -m "..."`.

Hardware tags use the `hw/` prefix. The bare `V1.0`…`V2.4` tags are whole-project software
releases and mean something different.
