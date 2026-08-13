# Domesday Duplicator (DdD)

Please see the [Project Documentation](https://simoninns.github.io/domesdayduplicator) for details of the project and for access to the project documentation.

For contributing guidelines, see [CONTRIBUTING.md](CONTRIBUTING.md).

![Domesday Duplicator Overview](graphics/DdD-overview.png)

The Domesday Duplicator is a LaserDisc capture focused, USB 3.0 based DAQ capable of 40 million samples per second acquisition of analogue RF data at 10-bits resolution.

Capture is via a easy to use GUI capture application.

![GUI Demo](graphics/DdD-gui-demo.gif)

# Cloning the DomesdayDuplicator GitHub

Everything is in this one repository. There are no git submodules, so a plain clone gives
you the complete project:

```
git clone https://github.com/simoninns/DomesdayDuplicator.git
```

The project was previously split across five repositories
(`DomesdayDuplicator-hardware`, `-firmware`, `-gui-app` and `-gui-docs`). Their history has
been imported here, and **this repository is now the only place to work** — changes pushed
to the old repositories will not reach the project.

## Repository layout

| Directory | Contents |
| --- | --- |
| [hardware/](hardware/) | KiCad design for the custom DdD board, plus hardware documentation |
| [fpga/](fpga/) | Verilog gateware for the Terasic DE0-NANO (Cyclone IV) |
| [fx3/firmware/](fx3/firmware/) | Cypress FX3 USB 3.0 controller firmware |
| [fx3/programmer/](fx3/programmer/) | Host-side tool for programming the FX3 |
| [fx3/sdk/](fx3/sdk/) | Vendored Cypress FX3 SDK the firmware builds against |
| [gui/](gui/) | Qt 6 capture application, and the Flatpak/DMG/MSI packaging under `gui/packaging/` |
| [docs/](docs/) | Source of the project documentation website |
| [docs-tech/](docs-tech/) | Engineering-process documentation for this repository |

Each component builds with ordinary distribution packages — see its `README.md`. There is
also a Nix flake if you prefer it. **Run these from anywhere in the working tree** — there is
one `flake.nix`, at the repository root, and Nix walks up to find it:

```
nix develop                          # all components, one shell
nix develop .#gui                    # or .#fx3, .#fpga, .#hardware, .#docs
nix build .#gui .#fx3-programmer     # build the host software
nix flake check                      # build everything and run the tests
```

A bare `nix develop` gives the all-components shell whichever directory you are in; use
`.#name` to select a single component.

[AGENTS.md](AGENTS.md) records the project conventions and [TESTING.md](TESTING.md) the test
tiers, including the hardware-in-the-loop capture-integrity procedure.

# The Decode Family 

The samples the DdD capture can be used with the whole family of decoders that make the FM RF Archival workflow ready to use today.

The original design was for the wide bandwidth of LaserDisc RF - making it suitable for all of the more bandwidth restricted mediums too (that have a single stream of RF).

[Please see the documentation for more details](https://simoninns.github.io/domesdayduplicator/related-projects/the-ld-decode-family/)

# 3D Printed Case 

The DomesDay Duplicator also has a [3D models](https://github.com/simoninns/DomesdayDuplicator-Case) and ready to use STL files for producing 3D printed cases, to protect from dust or line with copper tape for affordable EMI shielding for example.

<img src="graphics/DdD-case1.png" width="400" height="">

## Authors

Domesday Duplicator was written & designed by [Simon Inns](https://github.com/simoninns).

Additional documentation supplied by [Harry Munday](https://github.com/harrypm). 


## Licences

- [Software Licence — GPLv3](LICENSE)
- [Hardware Licence — Creative Commons BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
  (full text in [hardware/pcb/LICENSE.txt](hardware/pcb/LICENSE.txt))
