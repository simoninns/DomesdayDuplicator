# Cypress FX3 USB 3.0 Controller

Everything to do with the FX3, which moves sampled data from the FPGA to the host over
USB 3.0.

| Path | Contents |
| --- | --- |
| [firmware/](firmware/) | The firmware that runs on the FX3 — build it with `arm-none-eabi-gcc` and CMake |
| [firmware/gpif/](firmware/gpif/) | GPIF II Designer project for the parallel interface state machine |
| [mkimage/](mkimage/) | `fx3-mkimage`, the host tool converting the linked ELF into the boot-loadable image |
| [programmer/](programmer/) | `fx3-programmer`, the host-side libusb tool that loads firmware onto the device |
| [sdk/](sdk/) | Vendored subset of the Cypress EZ-USB FX3 SDK 1.3.5 that the firmware links against |

## Typical workflow

```bash
# Build the firmware image
cmake -B firmware/build -S firmware \
      -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake
cmake --build firmware/build

# Build the programmer
cmake -B programmer/build -S programmer
cmake --build programmer/build

# Load it onto a connected device
./programmer/build/fx3-programmer -l
./programmer/build/fx3-programmer -d 0 -u firmware/build/firmware.img
```

See [firmware/README.md](firmware/README.md) and [programmer/README.md](programmer/README.md)
for prerequisites, permanent (EEPROM) programming and troubleshooting.

## The board this runs on

The FX3 is **not on the Domesday Duplicator PCB**. It is a **Cypress/Infineon EZ-USB FX3
SuperSpeed Explorer Kit (CYUSB3KIT-003)** that plugs into the two GPIF II connectors on the
main board — `J201` and `J202` on `hardware/pcb/superspeedinf.sch`, which is a sheet
containing those connectors and power and nothing else. Everything the FX3 half of this
project touches lives on that kit.

Confirmed with the maintainer, 2026-08-12, and consistent with the schematic.

| | |
| --- | --- |
| Kit | CYUSB3KIT-003, EZ-USB FX3 SuperSpeed Explorer Kit |
| Part | CYUSB3014 (ARM926EJ-S) |
| Boot memory | **I2C EEPROM on the kit.** There is no SPI flash in this setup |
| `J4` | The PMODE jumper, tied to the FX3's `PMODE0` pin |

### Boot modes, and what each looks like on the host

| `J4` | Boots from | Enumerates as |
| --- | --- | --- |
| **Fitted** | USB — the FX3 boot ROM waits for a host to download to RAM | `04b4:00f3`, "FX3 micro-controller (DFU mode)" |
| **Removed** | The onboard I2C EEPROM | `1d50:603b`, running this project's firmware |

A third identity appears mid-operation: once `fx3-programmer` has pushed the Cypress
secondary loader (`cyfxflashprog.img`) into RAM to reach the EEPROM, the device re-enumerates
as **`04b4:4720`**. That is expected and transient.

So the loop is: fit `J4`, power cycle, program (RAM or EEPROM), remove `J4`, power cycle to
run from EEPROM. `fx3-programmer -r` does **not** substitute for the power cycle — it is a
stub that resets nothing (D25).

### A warning about the programmer's help text

`fx3-programmer -h` says `-p` programs "SPI flash". **It does not, and there is no SPI flash
here.** The implementation programs the I2C EEPROM and reports so, which is correct for this
hardware and matches the project's own
[FX3 programming guide](../docs/content/hardware-programming/fx3-firmware.md). The vendor commands for
SPI (`0xC2`, `0xC4`) are defined in the source and never used. Tracked as **D24**; the help
text is the thing that is wrong.

### Verified capacity

A 111,360-byte image programs and verifies successfully, spanning **two 64 KB I2C slave
banks** — the programmer rolls the slave address every 64 KB, so anything over that size
exercises the paging arithmetic in `fx3/programmer/src/fx3-paging.h`. Measured on hardware,
2026-08-12.
