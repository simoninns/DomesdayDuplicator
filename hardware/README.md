# Domesday Duplicator Hardware

Design files for the custom Domesday Duplicator PCB — the analogue front end, ADC and
interconnect that sit between the RF source and the DE0-NANO/FX3 boards.

## Contents

| Path | Contents |
| --- | --- |
| [pcb/](pcb/) | KiCad project: schematics, PCB layout, symbol and footprint libraries, Gerbers, plotted PDFs |
| [doc/](doc/) | Hardware documentation, including the gain and filter calculations |

## Design

The Domesday Duplicator hardware is based on:

- Terasic DE0-NANO FPGA development board (see [fpga/](../fpga/))
- Cypress FX3 USB 3.0 controller (see [fx3/](../fx3/))
- High-speed ADC for RF signal capture
- Custom analogue front-end circuitry

## Manufacturing

`pcb/Gerber/` holds the fabrication outputs and `pcb/PDF/` the plotted schematics. Both are
generated from the KiCad project and should be regenerated from it rather than edited.

## Licence

The hardware design is licensed under **Creative Commons Attribution-ShareAlike 4.0
International**. The full text is in [pcb/LICENSE.txt](pcb/LICENSE.txt).

This is different from the software in this repository, which is GPLv3 — see the root
[LICENSE](../LICENSE).

## Documentation

For detailed documentation, please see the
[main project documentation](https://simoninns.github.io/DomesdayDuplicator-docs).
