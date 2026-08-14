# In-Application Device Updates (FX3 Firmware and FPGA Gateware) — Investigation and Plan

## Purpose

Today, updating a Domesday Duplicator requires special tools and physical intervention:
the FX3 firmware needs `fx3-programmer` plus the J4 boot jumper and a power cycle, and the
FPGA gateware needs Quartus, the DE0-Nano's mini-USB cable and `quartus_pgm`. Both
procedures live on the documentation site under *hardware programming* and both are
developer workflows, not user workflows.

The goal of this plan is that a user with an assembled, cased unit can update **both** the
FX3 firmware and the FPGA gateware from `ddd-gui`, over the single USB 3.0 cable, with no
jumpers, no second cable, no Quartus and no command line. This document records what the
hardware actually permits, the options considered (including the "bootloader-style
load-at-connection" alternative), and the recommended design with an implementation
sequence.

This plan deliberately targets `ddd-gui` only. The legacy `gui/` application is being
retired before the next release and gains none of this.

## Authoritative references (in-tree)

- Conventions, protocol-change rule, programming policy: [AGENTS.md](../AGENTS.md)
  (§2 "any FPGA↔FX3↔host protocol change touches three components", §4 "never write to
  the FX3 EEPROM or the FPGA EPCS flash as part of an automated test")
- FX3 boot behaviour and current programming procedure: [fx3/README.md](../fx3/README.md),
  [docs/content/development/hardware-programming/fx3-firmware.md](../docs/content/development/hardware-programming/fx3-firmware.md)
- Existing host-side flashing logic to reuse:
  [fx3/programmer/src/fx3-programmer.c](../fx3/programmer/src/fx3-programmer.c),
  [fx3-paging.h](../fx3/programmer/src/fx3-paging.h) (pure, unit-tested)
- Image format ground truth: [fx3/mkimage/](../fx3/mkimage/) (AN76405 §4.4),
  `fx3/mkimage/src/fx3-bootimage.h`
- FPGA↔FX3 register link: [docs/content/development/fpga-register-interface.md](../docs/content/development/fpga-register-interface.md),
  [fpga/common/spiRegisters.v](../fpga/common/spiRegisters.v),
  [fx3/firmware/src/fpga-registers.c](../fx3/firmware/src/fpga-registers.c)
- GUI device layer the updater plugs into:
  [ddd-gui/src/capture/usb_device.h](../ddd-gui/src/capture/usb_device.h),
  [device_monitor.h](../ddd-gui/src/capture/device_monitor.h),
  and the deferred-design note in
  [ddd-gui-implementation-plan.md](ddd-gui-implementation-plan.md) ("nothing in the
  application may assume the capture personality is the only one the hardware presents")

## What exists today (investigation summary)

### FX3 side

- The FX3 is the CYUSB3KIT-003 Explorer Kit, booting from its onboard **I2C EEPROM**
  (jumper J4 removed). With J4 fitted it boots from USB and enumerates as the Cypress ROM
  bootloader `04b4:00f3`. There is no SPI flash anywhere in this design
  (`fx3/README.md:34-60`).
- The ROM bootloader implements vendor request `0xA0` (load to RAM, then jump). EEPROM
  programming is done by first RAM-loading Cypress's secondary flash-programmer image,
  which re-enumerates and answers `0xB0` (identity probe, magic `"FX3PROG"`), `0xBA`
  (I2C write) and `0xBB` (I2C read/verify). `fx3-programmer` drives all of this with
  plain libusb (`fx3-programmer.c:48-58,163-224,491-576`).
- The **running application firmware can do none of this**. It initialises no I2C block
  at all (`useI2C = CyFalse` in the IO matrix, `domesday-duplicator.c:89-103`), exposes
  only `0xB5` (start/stop, dormant), `0xB7` (FPGA register read) and `0xB8` (FPGA
  register write), and has **no reset, reboot or enter-bootloader command** — this is
  recorded as defect D25 (`fx3/programmer/README.md:152-166`). `libusb_reset_device()`
  re-enumerates USB without rebooting the FX3, so the host cannot reach the boot ROM by
  any software means today.
- The vendored SDK subset exports `CyU3PDeviceReset` (whole-chip reset, cold reset
  re-reads the boot source) and the `JumpBackToBooter` family, but the latter requires a
  Cypress two-stage booter that is neither vendored nor used here.
- The `.img` format is boot-source-agnostic: the same `fx3-mkimage` output is valid for
  RAM download and for EEPROM programming (`fx3-bootimage.h:50-82`).
- The FX3's I2C pins are dedicated (not stolen from GPIF or the UART), so the
  application firmware *can* bring up the I2C block and talk to the boot EEPROM itself —
  it simply doesn't today.

### FPGA side

- Terasic DE0-Nano, Cyclone IV E `EP4CE22F17C6`, configured by Active Serial from the
  board's **EPCS64** (8 MB) flash. Programming today is JTAG via the DE0-Nano's onboard
  USB-Blaster: `.sof` to SRAM, or `.jic` (JTAG-indirect) into the EPCS
  (`fpga/README.md:163-176`).
- **The FX3 has no electrical path to the FPGA's configuration circuitry.** A net trace
  of the schematics shows the interconnect headers carry only: the 16-bit GPIF data bus
  (FPGA→FX3 only), `USB_PCLK`, the CTL lines (including the 4-wire bit-banged SPI
  register link and `nReset`), 16 further wired-but-unused data lines
  (`USB_DATA16..31`), and one reserved spare (`USB_CTL9`). No JTAG, no AS pins, no
  `nCONFIG`/`nSTATUS`/`CONF_DONE`, no MSEL. The EPCS64 sits on the DE0-Nano's dedicated
  AS pins, which never leave that board.
- Therefore **the only possible route to the EPCS from the host is through the FPGA
  fabric**: Cyclone IV user logic can reach the dedicated AS pins via the `asmiblock`
  WYSIWYG primitive, and can force reconfiguration via the remote-system-upgrade
  `rublock` primitive. Neither is used anywhere in the current gateware; the Quartus
  project has no configuration-related assignments at all, and the `.cof` produces no
  raw image (`auto_create_rpd 0`, single page).
- The gateware's register map (`spiRegisters.v`) has free address space `0x12–0x7F` and
  the link auto-increments addresses, so multi-byte streaming over the existing SPI is a
  protocol-compatible extension.
- Scale of the payload: an EP4CE22 uncompressed configuration is 5,748,552 bits
  (≈ 702 KiB); the built image is compressed (`.cof: compress_bitstream 1`), roughly
  halving that. Streamed as a continuous SPI burst at the current ~100 kHz bit-banged
  rate, the raw shift time is ~30–60 s — entirely workable. (What would *not* work is
  today's one-register-per-control-transfer usage; the transport must stream.)

### GUI side

- `ddd-gui` talks libusb (or WinUSB) directly behind the narrow `IUsbDevice` seam, with
  a full in-tree fake for testing. Device discovery is a 200 ms polling `DeviceMonitor`
  explicitly designed so that a re-enumerated or firmware-changed device is reported as
  a change, and it can be suspended around phases that must own the device
  (`device_monitor.h:29-82`).
- Enumeration currently matches only `1209:2347` — a device sitting in the Cypress
  bootloader is invisible to the GUI (`libusb_device.cpp:177-180`).
- Version reporting already exists end-to-end: the FX3 commit rides in the USB product
  string, the gateware commit in registers `0x03..0x0A`, and **Help → Firmware…** shows
  both, with a once-per-connection mismatch warning. There is no update code anywhere.
- The `fx3-programmer` udev rules already grant access to both personalities (the
  `04b4` wildcard and `1209:2347`).

## Options considered

### FX3

**Option F1 — self-hosted EEPROM update (recommended).** Teach the *application*
firmware to be its own flasher: bring up the I2C block, accept the new image over EP0 in
chunks, write and verify the EEPROM, then cold-reset so the new firmware boots. No
personality change, no jumper, no new driver binding on any OS (the update happens
entirely under the existing `1209:2347` WinUSB/libusb association). The EEPROM write is
~114 KiB of I2C pages — a few seconds.

**Option F2 — ROM-bootloader flow from the GUI.** Add only a "reboot" command to the
firmware, have the GUI then drive the existing `0xA0`/flash-programmer sequence exactly
as `fx3-programmer` does. Rejected as the primary path for two reasons: a cold
`CyU3PDeviceReset` returns to the *same* boot source (EEPROM), so reaching the ROM
bootloader still requires the jumper unless the EEPROM image is first invalidated — an
ugly deliberately-brick-then-recover dance; and on Windows the Cypress ROM bootloader
(`04b4:00f3`) presents no MS OS descriptors, so it does not bind WinUSB automatically —
a driver-installation problem the self-hosted path avoids entirely. The *mechanism*,
however, is still wanted in the GUI as the **recovery path** (see below).

**Option F3 — bootloader-style RAM load at every connection.** Keep either the ROM
bootloader (J4 fitted) or a minimal custom booter in EEPROM, and have `ddd-gui` download
the full firmware to RAM on every connect, so the running firmware always matches the
application. This is the "load-at-connection" idea from the brief. It is attractive
(firmware can never be stale, EEPROM never rewritten in the field) but loses to F1 on
practical grounds: with the ROM bootloader it inherits the Windows driver problem and a
jumper change on every existing unit; with a custom booter it requires vendoring the
Cypress boot library (`cyfx3_boot`, not in the vendored SDK subset) and writing and
maintaining a second firmware whose own rare updates still need an EEPROM writer; and in
both cases a unit is non-functional without a host application present. F1 keeps the
device standalone and needs one firmware, not two. F3 is recorded here as the fallback
should F1 prove fragile in practice — nothing in the F1 protocol design precludes it.

### FPGA

**Option G1 — EPCS access through the fabric, SPI-bridge style (recommended).** Add a
small gateware block that, when explicitly unlocked, bridges the existing FX3↔FPGA SPI
register link through the `asmiblock` primitive to the EPCS pins. The FX3 firmware then
speaks the EPCS/M25P64 command set (read ID, sector erase, page program, read) directly
through the bridge, and the GUI orchestrates at file level. Gateware stays tiny and
dumb; all sequencing lives in C, where it is testable. A companion `rublock` register
triggers reconfiguration from a chosen flash address.

**Option G2 — ASMI command engine in gateware.** The gateware implements erase/program/
read as register-level commands with an internal page buffer, and the FX3 merely relays.
More Verilog (a flash state machine, a page FIFO), less C. Functionally equivalent;
rejected in favour of G1 because the project's testing strength is exactly where G1 puts
the complexity (pure C, unit-testable paging/CRC logic, precedent in `fx3-paging.h`),
and G1's gateware surface is small enough to review line-by-line.

**Option G3 — new parallel transport first.** Use the 16 wired-but-unused data lines to
build a fast host→FPGA path before doing any of this. Rejected: it needs a new GPIF
state machine (Windows-only GPIF II Designer) and buys nothing that matters — the
payload is under a megabyte and the existing link moves it in about a minute.

**Load-at-connection for the FPGA** (SRAM configuration from the host on every connect)
is **not possible**: passive-serial/JTAG configuration pins are simply not wired to the
FX3. Volatile configuration via JTAG remains a bench activity on the DE0-Nano's own
USB-Blaster. The EPCS is the only field-writable configuration source, so field updates
are flash updates; the safety story therefore comes from a dual-image layout, not from
avoiding flash writes.

## Recommended architecture

One update protocol, two targets, one GUI flow:

```
ddd-gui ──EP0 vendor requests──▶ FX3 application firmware
                                    │
                                    ├─ target FX3:  I2C block ──▶ boot EEPROM (M24M02)
                                    │
                                    └─ target FPGA: bit-banged SPI ──▶ spiRegisters
                                                        │ (flash-bridge unlocked)
                                                        └──▶ asmiblock ──▶ EPCS64
                                                             rublock  ──▶ reconfigure
```

The FX3 application firmware is the single on-device update agent. The GUI never needs a
second USB personality for a normal update; the Cypress ROM bootloader is recognised by
the GUI only as a *recovery* personality.

### New vendor protocol (FX3 EP0)

Existing codes `0xA0` (ROM), `0xB0/0xBA/0xBB` (Cypress flash programmer), `0xB5–0xB8`
(this firmware) stay untouched. New requests, `wIndex` = target (`0` = FX3 EEPROM,
`1` = FPGA EPCS):

| Request | Dir | Purpose |
| --- | --- | --- |
| `0xD0` UPDATE_STATUS | IN | State machine phase, byte counter, last error, capability bits. Polled by the GUI for progress. |
| `0xD1` UPDATE_BEGIN | OUT + 12-byte data | Total length, CRC32, flags. Enters update mode (refused while capture is running; capture refused while updating). |
| `0xD2` UPDATE_DATA | OUT + ≤2 KiB data | Sequenced image chunks (`wValue` = chunk index). Firmware streams them to the target as they arrive. |
| `0xD3` UPDATE_FINISH | OUT | Firmware completes writes, reads back the target, verifies CRC32, then commits (see safety, below). Result read via `0xD0`. |
| `0xD4` DEVICE_RESET | OUT | `CyU3PDeviceReset(CyFalse)` — cold reset, device re-enumerates on the (new) EEPROM image. Also closes D25. |
| `0xD5` FPGA_RECONFIG | OUT | Writes the rublock trigger register: FPGA reconfigures from the application image. Followed by `0xD4`, since reconfiguration stops `USB_PCLK` under the GPIF. |

Chunks are acknowledged by the control transfer itself; per-target flow control (I2C page
timing, EPCS busy polling) happens inside the firmware between chunks. CRC32 verification
is computed on-device during readback, so nothing large ever travels device→host.

### FX3 self-update specifics

- IO matrix change: `useI2C = CyTrue` (dedicated pins; no conflict with the 16-bit GPIF
  or the UART). I2C block at 400 kHz, page writes of 64 bytes rolling the slave address
  every 64 KiB — the exact layout `fx3-flashprog` uses; `fx3-paging.h` is pure and can
  be shared or mirrored with its tests.
- **Commit ordering is the safety mechanism**: the first EEPROM page (containing the
  `'CY'` signature) is held back, the rest of the image is written and verified, and the
  first page is written last. An interrupted update leaves an image the boot ROM
  rejects, and the kit then falls back to the USB bootloader — which the GUI recognises
  and can recover from. (The kit demonstrably enumerates as the bootloader with a blank
  EEPROM; the precise PMODE fallback behaviour is verification item V1 below.)
- The J4-jumper procedure remains documented as the recovery of last resort.

### Gateware specifics

- New `flashBridge` block: a register-map extension (map version `0x01 → 0x02`) in the
  free space —
  - `0x20` bridge unlock (magic byte sequence; the bridge is inert otherwise, so no
    stray register write can ever touch the EPCS),
  - `0x21` bridge control (CS assert/deassert),
  - `0x22` bridge data port (non-incrementing: each SPI byte written is shifted out to
    the EPCS, the byte simultaneously shifted in is latched for reading back through the
    same address — a plain full-duplex SPI pass-through),
  - `0x23` rublock control (arm + trigger reconfiguration to the application image;
    watchdog tickle).
- `asmiblock` and `rublock` are instantiated directly as WYSIWYG primitives in plain
  Verilog, consistent with the project's committed-IP policy (no Quartus-generated
  megafunction files). Both simulate as simple stubs for lint/Verilator.
- **Dual-image layout in the EPCS64** (8 MB is ample): the **factory image** at `0x0`
  is written by JTAG only, never in the field; the **application image** lives at a
  fixed sector-aligned offset (e.g. `0x400000`). The factory gateware is the full
  capture gateware *plus* the bridge and a small boot decision: read a boot block from
  flash; if it marks the application image valid, trigger reconfiguration to it with
  the remote-update watchdog enabled. A field update writes the application image, then
  writes the boot block last; a failed or bad application image (config error or
  watchdog expiry) falls back to factory automatically. The unit is therefore never
  bricked by a gateware update — worst case it boots the factory gateware, which still
  captures and can still be updated.
- Build outputs gain a raw application image (`.rpd`-style byte stream, exact bit order
  fixed and covered by provenance tooling) alongside the existing `.jic`, whose layout
  changes to carry both pages. The documented "well under a second" configuration-time
  assumption in `fpga-registers.h` is re-verified with the factory→application double
  configuration (item V5).

### GUI specifics

- A new engine-side seam next to `IUsbDevice` — `IDeviceUpdater` — with a fake, so the
  entire update UI is drivable in tests with no hardware, matching the existing
  `FakeUsbDevice` pattern.
- Enumeration extends to recognise the bootloader personality (`04b4:00f3`, and the
  transient flash-programmer identity by its `0xB0` probe): `DeviceInfo` gains a
  `personality` field; the three VID/PID match sites (libusb enumerate/open, WinUSB) are
  the known touch points. A device in bootloader mode is shown as "Domesday Duplicator
  (recovery mode)" rather than "no device".
- Update flow is a deliberate, modal, user-initiated action (per AGENTS.md §4 — never
  automatic, never part of any test): user opens **Help → Firmware… → Update**, picks a
  release bundle (or uses one shipped with the app), the dialog shows
  current-vs-bundle versions for firmware and gateware, and updates whichever the user
  confirms. `DeviceMonitor` is suspended for the duration; progress comes from `0xD0`
  polling on a worker thread (never the GUI thread, and never with infinite-timeout
  control transfers).
- After an update: `0xD4` reset → re-enumeration → `DeviceMonitor` reports the change →
  the existing identity read confirms the new commit strings. That closes the loop with
  evidence, not hope.
- Recovery flow: if the monitor sees the bootloader personality, the GUI offers to
  restore firmware — a port of `fx3-programmer`'s RAM-load + flash-programmer logic
  (GPLv3, pure libusb, directly reusable) into the engine. On Windows this personality
  needs a driver association (no MS OS descriptors in the Cypress ROM); the recovery
  page documents Zadig/driver options, and this is another reason recovery is the
  exception path, not the normal one.

### Release bundle

A single file (e.g. `ddd-update-<version>.dddfw`: a tar with a JSON manifest) carrying
`firmware.img`, the gateware application image, per-file CRC32s, commit identifiers and
the minimum compatible register-map version. Assembled by a script in `tools/` at
release time — necessarily partly by hand, because the bitstream is built locally with
unfree Quartus and attached to releases manually, exactly as today. The GUI validates
the manifest before offering any button.

## Testing policy

- **Never** as part of automated tests does anything write the EEPROM or the EPCS
  (AGENTS.md §4). All flashing is user- or developer-initiated.
- Unit tier: image parsing (`'CY'` header walk), chunking/paging arithmetic, CRC32,
  manifest parsing, protocol framing, updater state machine against the fake — all pure
  and hardware-free, following the `fx3-paging.h` precedent.
- Hardware tier: a manual, documented procedure per target (update to a known image,
  verify identity registers/product string, then deliberately interrupt an update at
  50 % and confirm the recovery behaviour — USB-bootloader fallback for the FX3,
  factory-image fallback for the FPGA). Added to TESTING.md alongside the T5 capture
  integrity procedure; a full capture-integrity run follows any successful update pair.

## Implementation phases

> **Superseded.** Options F1 and G1 were adopted, and the detailed phasing now lives in
> [device-update-implementation-plan.md](device-update-implementation-plan.md), which
> also revises several decisions made in this document: the factory image is a minimal
> boot/rescue gateware (cleanly separated in the repository from the flashable
> application gateware), not a full capture gateware; SHA-256 plus a signed manifest
> replaces CRC32 throughout the verification chain; and Quartus is admitted to the
> release CI, so release bitstreams are CI-built rather than built locally and
> attached by hand. The outline below is kept for the record of how the options were
> first cut.

Each phase lands independently and leaves the tree shippable. Protocol changes touch
fpga + fx3 + ddd-gui + docs together (AGENTS.md §2).

1. **Protocol and formats (docs first).** Write the update-protocol page in
   `docs/content/development/`, fix the vendor request numbers, register map v2, EPCS
   layout, bundle manifest. Add `.cof`/`quartus_cpf` output for the raw application
   image and extend `bitstream-provenance.py` to cover it.
2. **FX3 self-update.** I2C bring-up, `0xD0–0xD4`, held-back-first-page commit, CRC
   readback verify. GUI: `IDeviceUpdater` + fake + minimal dialog for the firmware
   target only. Hardware verification V1–V3. This phase alone already removes the
   jumper-and-`fx3-programmer` procedure for routine firmware updates, and closes D25.
3. **Recovery personality.** GUI recognition of `04b4:00f3`, engine port of the
   RAM-load/flash-programmer sequence, "recovery mode" UI, Windows driver
   documentation. From here, an interrupted firmware update is recoverable in-GUI.
4. **Gateware flash bridge and dual-image boot.** `flashBridge` + `asmiblock` +
   `rublock`, factory/application layout, watchdog fallback, new `.jic`; FX3 learns the
   EPCS command set behind target `1` and `0xD5`. Hardware verification V4–V6. Existing
   units get the dual-image `.jic` once, by the current JTAG procedure — the last time
   a cable is needed.
5. **Gateware update in the GUI + release bundle.** Second target in the update dialog,
   bundle assembly script, release-process and user-facing documentation ("updating
   your Domesday Duplicator" moves from the development section to the user section).

## Hardware verification checklist (open questions)

- **V1** — Confirm the CYUSB3KIT-003 PMODE strapping falls back to USB boot when the
  EEPROM holds an invalid image (expected: yes — a blank-EEPROM kit enumerates as the
  bootloader; must hold for *corrupt*, not just blank).
- **V2** — Confirm I2C block bring-up (`useI2C = CyTrue`) coexists with the 16-bit GPIF
  + UART IO matrix on running hardware, and measure EEPROM page-write timing.
- **V3** — Measure achievable EP0 chunk size (2 KiB assumed) and end-to-end firmware
  update time (expected: well under a minute).
- **V4** — Confirm `asmiblock`/`rublock` primitive names and port lists for Cyclone IV E
  under Quartus Prime Lite, and that compressed images are compatible with the
  remote-update application page.
- **V5** — Measure factory→application double-configuration time against the FX3's
  "FPGA ready well under a second" power-up assumption.
- **V6** — Measure sustained bit-banged SPI throughput for the EPCS path and the full
  gateware update duration (expected: a few minutes including erase and verify; if the
  100 kHz bit-bang dominates, direct GPIO register access in the FX3 firmware is the
  contained optimisation).
- **V7** — Verify the raw application image byte/bit order against what `asmiblock`
  reads back, once, with provenance tooling locking it in.
