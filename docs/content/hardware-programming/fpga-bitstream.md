# FPGA bitstream

!!! warning "Placeholder — not yet written"

    This page does not yet describe the FPGA programming procedure. It records what the task
    involves so the gap is visible, rather than leaving the section looking complete.

    Until it is written, follow the Terasic DE0-NANO user manual and the notes below, or ask
    on the [community channels](../support/community.md).

## What gets programmed

The gateware lives on a **Terasic DE0-NANO** carrying an Intel (Altera) **Cyclone IV
EP4CE22F17C6**. It drives the ADC, packs samples and feeds the FX3 over the GPIF II bus.

![](assets/DE0.jpg){ width="500" }

The board has an onboard **USB-Blaster**, so no separate programming pod is needed — the
DE0-NANO's mini-USB connector is the programming interface.

## Two kinds of programming

| Target | Tool | Survives a power cycle |
| --- | --- | --- |
| **FPGA volatile configuration** (`.sof`) | `quartus_pgm` | No — reconfigured on every power-up |
| **EPCS serial configuration device** (`.jic`) | `quartus_pgm` | Yes |

As with the FX3, the volatile path is the safe one to try first.

## What you need

- **Intel Quartus Prime Lite.** Free, but large, `x86_64` Linux or Windows only. The project
  currently builds against version 25.1.
- A **udev rule for the USB-Blaster** so Quartus can reach it without root. This is the same
  class of problem as [Linux device access](linux-device-access.md), and the Quartus
  installer does not create one on most distributions.
- The bitstream, either from a release or built from `fpga/` in the repository.

## Why this is not finished

The FPGA toolchain is not yet packaged the way the FX3 one is. Quartus is unfree,
`x86_64-linux` only, and cannot be redistributed, so it is not built in CI and the bitstream
is produced locally and attached to releases by hand. The repository's `fpga/` directory has
a development shell with the free tools — Verilog linting and simulation — but building a
bitstream still needs a local Quartus installation.

Until that work lands, this page cannot honestly describe a reproducible procedure.

## In the meantime

- Prebuilt `.sof` and `.jic` files are attached to
  [releases](https://github.com/simoninns/DomesdayDuplicator/releases).
- The Quartus project is in `fpga/` in the repository.
- The DE0-NANO's LEDs show a distinctive moving pattern once the gateware is running
  correctly, which is a quick confirmation that programming worked.

## Related

- [FX3 firmware](fx3-firmware.md) — the other half, and it must be kept in step with this one
- [Linux device access](linux-device-access.md) — the USB-Blaster has the same permissions
  problem as the FX3
