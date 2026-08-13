# Domesday Duplicator Hardware

Design files for the custom Domesday Duplicator PCB — the analogue front end, ADC and
interconnect that sit between the RF source and the DE0-NANO/FX3 boards.

## Contents

| Path | Contents |
| --- | --- |
| [pcb/](pcb/) | KiCad project: schematics, PCB layout, symbol and footprint libraries |
| [pcb/fab/](pcb/fab/) | Fabrication outputs, one frozen directory per board revision |
| [pcb/tools/](pcb/tools/) | Plotting and Gerber comparison scripts |
| [pcb/PDF/](pcb/PDF/) | Plotted schematics and layers |
| [doc/](doc/) | Hardware documentation, including the gain and filter calculations |

Board revisions are recorded in [pcb/CHANGELOG.md](pcb/CHANGELOG.md). The KiCad format
migration in progress is described in [pcb/MIGRATION.md](pcb/MIGRATION.md).

## Design

The Domesday Duplicator hardware is based on:

- Terasic DE0-NANO FPGA development board (see [fpga/](../fpga/))
- Cypress FX3 USB 3.0 controller (see [fx3/](../fx3/))
- High-speed ADC for RF signal capture
- Custom analogue front-end circuitry

## Manufacturing

Each fabricated board revision has its own directory under `pcb/fab/`, holding the Gerbers
and drill file exactly as sent out, with a `MANIFEST.md` recording provenance and a
`SHA256SUMS`. These are a historical record, not build output: they are written once and
never regenerated, because they are the reference for what the working board actually is.

`pcb/fab/rev1.0/` is the proven production board, plotted with KiCad 4.0.7 in 2018. A
modern KiCad cannot reproduce it byte for byte — it encodes zone fills differently — which
is why it is preserved rather than rebuilt.

To plot a new revision:

```sh
nix develop .#hardware
hardware/pcb/tools/plot-fab.sh 2.0
```

The script refuses to write into a revision directory that already exists. To compare a
fresh plot against an existing one, plot with `--scratch` and use
`pcb/tools/gerber-compare.py`, which compares geometry rather than text and so ignores the
aperture renumbering and object reordering that make a plain diff useless.

## Licence

The hardware design is licensed under **Creative Commons Attribution-ShareAlike 4.0
International**. The full text is in [pcb/LICENSE.txt](pcb/LICENSE.txt).

This is different from the software in this repository, which is GPLv3 — see the root
[LICENSE](../LICENSE).

## Documentation

For detailed documentation, please see the
[main project documentation](https://simoninns.github.io/domesdayduplicator).
