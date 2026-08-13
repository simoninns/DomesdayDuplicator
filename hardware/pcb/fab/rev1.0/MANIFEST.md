# Fabrication outputs — hardware rev 1.0

**These files are a historical record. Do not regenerate them.**

This is the exact fabrication data for the proven production board — the Domesday
Duplicator PCB that was manufactured and is known to work. It is the reference against
which any future re-plot is checked. If a newer KiCad produces different output, this
directory is the arbiter of what the working board actually looks like, so it must stay
byte-for-byte as sent to fabrication.

New revisions go in a sibling directory (`fab/rev2.0/`, …). Nothing is ever written back
into this one.

## Provenance

| | |
| --- | --- |
| Board revision | 1.0 |
| Plotted by | KiCad 4.0.7 (PCBNEW 4.0.7) |
| Plot date | 2018-06-19 15:52 |
| Source format | `kicad_pcb` version 4 / `EESchema Schematic File Version 2` |
| Source state | git tag `hw/rev1.0-production` — the last commit before the KiCad 10 format migration |
| Gerber format | RS-274X, 4.6, leading zeros omitted, absolute, millimetres |
| Drill format | Excellon, absolute, **inch**, decimal, trailing zeros |

Note the unit mismatch between the Gerbers (mm) and the drill file (inch). That is how the
set was sent out and how it was fabricated; it is recorded here so nobody "fixes" it.

## Layer map

The KiCad 4 layer names below are the ones in these filenames. Current KiCad uses different
names (`F.SilkS` → `F.Silkscreen`, `Edge.Cuts` → `Edge_Cuts`), so a re-plot will not produce
matching filenames even when the geometry is identical. Compare geometry, not names.

| File | Layer |
| --- | --- |
| `Domesday Duplicator-F.Cu.gbr` | Top copper |
| `Domesday Duplicator-B.Cu.gbr` | Bottom copper |
| `Domesday Duplicator-F.Mask.gbr` | Top solder mask |
| `Domesday Duplicator-B.Mask.gbr` | Bottom solder mask |
| `Domesday Duplicator-F.SilkS.gbr` | Top silkscreen |
| `Domesday Duplicator-B.SilkS.gbr` | Bottom silkscreen |
| `Domesday Duplicator-Edge.Cuts.gbr` | Board outline |
| `Domesday Duplicator.drl` | Excellon drill |

No paste layers were plotted for this revision.

## Checksums

Recorded in [SHA256SUMS](SHA256SUMS). Verify with:

```sh
cd hardware/pcb/fab/rev1.0 && sha256sum -c SHA256SUMS
```

## Fabricator

Not recorded. If you know which house produced the boards photographed in
`hardware/pcb/Domesday Duplicator top.jpg` and `… bottom.jpg`, add it here.
