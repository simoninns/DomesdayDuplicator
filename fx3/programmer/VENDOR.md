# Vendored Cypress material in this directory

## `cyfxflashprog.img` — the secondary loader

`fx3-programmer` can do three things: download firmware to FX3 RAM (vendor command `0xA0`),
program an I2C EEPROM (`0xBA`/`0xBB`), and program SPI flash (`0xC2`/`0xC4`). The latter two
— the ones that make firmware persist across a power cycle — work by first pushing a Cypress
**secondary loader** into RAM and then issuing commands to it. That loader is
`cyfxflashprog.img`.

Until now the file was **absent from this repository**, so permanent programming only worked
for someone who happened to have a `cyusb_linux` checkout in a sibling directory. It is now
vendored here.

| | |
| --- | --- |
| Source | `ezusbfx3sdk_1.3.5_Linux_x32-x64.tar.gz` → `cyusb_linux_1.0.5.tar.gz` → `fx3_images/cyfxflashprog.img` |
| File date | 18 December 2017 |
| Size | 106,456 bytes |
| SHA-256 | `818fff4f1c28bf1cf707c0de0ff9e0d624ea3d6115b7be9c2b8a22f34db30c30` |

`cyfxflashprog.txt` is the vendor's description of it, kept alongside because it documents the
vendor command protocol that `fx3-programmer.c` implements (the SPI protocol is "based on the
Micron M25PXX devices").

### How the programmer finds it

`find_flashprog_image()` in `src/fx3-programmer.c` searches, in order:

1. `$FX3_FLASH_PROG`, if set
2. `cyfxflashprog.img` — relative to the working directory
3. `../cyfxflashprog.img`
4. three paths pointing at a sibling `cyusb_linux` checkout

Placing the file at this directory's root means candidate 3 resolves when the programmer is
run from `build/`, which is how it is normally invoked during development — so it works today
with no code change.

That is **not** sufficient for an installed binary, since all the candidates are
working-directory-relative. Task **P2-10** adds a compiled-in `FLASHPROG_INSTALL_PATH` and a
matching `install(FILES …)`, so a packaged programmer finds the loader wherever it is run
from. Until then, installed builds need `FX3_FLASH_PROG` set explicitly.

## `LICENSE.cyusb_linux.txt`

The `cyusb_linux` package, from which both `cyfxflashprog.img` and this project's
`fx3-programmer.c` derive, ships under the **GNU Lesser General Public License, version 2.1**
— the full text is in that file, copied verbatim from `cyusb_linux_1.0.5/license.txt`.

Two consequences worth noting:

- LGPL-2.1 permits relicensing under the GNU GPL (LGPL-2.1 §3), so the derived
  `fx3-programmer.c` sits comfortably under this project's GPLv3.
- `src/fx3-programmer.c` currently carries **no copyright or licence header at all**, despite
  stating in a comment that it derives from `cyusb_linux`. That is tracked as **D22** and is
  fixed in P8-5's SPDX rollout. (An earlier revision of this file called it D20, which is a
  different defect entirely — the MkDocs raw-`<img>` breakage closed in Phase 4.)

`cyfxflashprog.img` itself is a compiled Cypress SDK example (`cyfxflashprog.txt` identifies
it as such) that ships inside the LGPL-licensed `cyusb_linux` package; it is vendored here
under the same project decision that covers the SDK — see
[`docs-tech/decisions.md`](../../docs-tech/decisions.md) (P0-2).

## Vendored files removed in Phase 5

Two files came across from `cyusb_linux` with the rest of `configs/` and were dead on
arrival in this project:

| File | Why it went |
| --- | --- |
| `configs/cy_renumerate.sh` | Signals a running `cyusb` daemon with `SIGUSR1`. This project does not ship that daemon, CMake never installed the script, and the `RUN+=` hooks that invoked it were removed from the udev rules in Phase 3 (D19) |
| `configs/cyusb.conf` | The `cyusb` daemon's VID/PID device list. Nothing in this project reads it |

`cyfxflashprog.img` and `cyfxflashprog.txt` stay: the secondary loader is required for
EEPROM and SPI programming and cannot be replaced without writing FX3 firmware to do the
same job, and both are LGPL-2.1.
