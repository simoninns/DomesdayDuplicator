# Board Bring-Up and Legacy Rollback — Investigation and Plan

## Purpose

The device-update work ([device-update-implementation-plan.md](device-update-implementation-plan.md))
gives a Duplicator running current firmware a complete self-service update path. What it
does not yet give is a guided way to get a board **into** that world, or back **out** of
it:

1. **Bring-up.** A user with boards that are not yet running current firmware — a bare
   SuperSpeed Explorer Kit fresh from the distributor, a bare DE0-Nano still holding
   Terasic's demo image, or an assembled Duplicator running the legacy firmware that
   enumerates as `1d50:603b` — should be able to open `ddd-gui`, follow a wizard, and end
   with both boards in the "ready for a normal firmware load" state: FX3 running current
   firmware, FPGA provisioned with the factory/application flash layout. From there the
   ordinary update path finishes the job.
2. **Rollback.** The reverse should also exist: a deliberate "factory reset" that returns
   a unit to the original legacy firmware and gateware (`1d50:603b`, single-image EPCS),
   for users who need the old software ecosystem and for testing the bring-up path against
   a genuinely legacy board without hoarding never-updated hardware.

Both flows live in `ddd-gui` under a new **Tools ▸ Firmware** sub-menu, with the normal
update path at the top and the bring-up/rollback paths grouped below under **Legacy** —
and none of this complexity may leak into the normal update flow, which stays exactly as
it is.

This plan also answers the two questions raised with the request: yes, the images needed
for bring-up can be bundled with `ddd-gui` so first provisioning works offline; and yes, a
legacy image set can be produced — generated **once** from a pinned historical commit and
committed to the repository as tracked binaries, so nothing ever needs to build the legacy
tree again.

## Authoritative references (in-tree)

- Conventions: [AGENTS.md](../AGENTS.md) (§2 protocol-change rule, §4 hardware safety —
  nothing automated ever writes EEPROM or EPCS, §6 naming, §9 provenance)
- The update mechanism this builds beside:
  [device-update-implementation-plan.md](device-update-implementation-plan.md), especially
  *Phase 3* (recovery personality, first-time FX3 programming — **done**) and *Phase 8*
  (provisioning from the application — the FPGA half of which this plan takes up)
- FX3 boot behaviour, J4 jumper, identities: [fx3/README.md](../fx3/README.md)
- EPCS layout, factory/application split, provisioning procedure:
  [docs/content/development/epcs-layout-and-boot-flow.md](../docs/content/development/epcs-layout-and-boot-flow.md),
  [fpga/README.md](../fpga/README.md)
- Bench procedures this extends: [TESTING.md](../TESTING.md) §6 (U0, U5, U6, G0, G1)
- GUI code this extends: `ddd-gui/src/gui/main_window.cpp` (menus),
  `ddd-gui/src/capture/device_recovery.{h,cpp}` (bare-board FX3 path),
  `ddd-gui/src/capture/wire_protocol.h` (identities),
  `ddd-gui/src/gui/auto_capture_wizard.h` (the wizard pattern to copy)

## What exists today (investigation summary)

### Already built, and reused as-is

- **The bare-FX3 path is done.** `RecoveryInstaller`
  (`ddd-gui/src/capture/device_recovery.h:89`) parses the FX3 boot image
  (`boot_image.h:46-81`), RAM-loads it into a device sitting in the Cypress boot ROM via
  `0xA0` (`device_programmer.cpp:48` — deliberately *without* the Cypress secondary
  loader), waits for the application personality, and then hands over to the ordinary
  `UpdateOrchestrator`, whose target-0 path has the *running firmware's own update agent*
  write the EEPROM. Bench-proved as U5 and U6 (TESTING.md §6): a kit with a blank or
  corrupted EEPROM enumerates as `04b4:00f3` by itself, no jumper, and is programmed
  entirely from the GUI.
- **Personality recognition** exists for `04b4:00f3` (kRecovery) and the transient
  `04b4:4720` (kFlashProgrammer) at the three match sites (`libusb_device.cpp:62-70`,
  `winusb_device.cpp:60-68`, enum at `usb_device_info.h:46-64`).
- **The update wizard UX machinery** — planned step list (`update_steps.h`), one weighted
  progress bar, wording in one place (`update_text.h`), worker thread, everything drivable
  against fakes — and the hand-built wizard pattern (`QDialog` + `QStackedWidget`,
  rationale at `auto_capture_wizard.h:51-58`; **not** `QWizard`).
- **Provisioning artefacts** are CI-built: `nix build .#bitstream` emits
  `provisioning/DomesdayDuplicatorProvisioning.jic` (factory at `0x000000`, application
  at `0x200000`), its `.map`, the `_write_jic.cdf`, and `boot-block.bin`
  (`fpga/package.nix:119-155`). A freshly provisioned unit boots the **factory** image —
  the `.jic` deliberately leaves the boot-block sector alone — so its defined state is
  *recovery gateware running*, which the existing update flow already recognises and
  repairs. That state is precisely "ready for the normal firmware load".

### The gaps this plan fills

- **The FPGA half of first provisioning still needs Quartus.** Legacy and shipped-blank
  gateware has no flash bridge, so the update protocol cannot reach the EPCS; the only
  electrical route is the DE0-Nano's own USB-Blaster (`09fb:6001`) on its mini-USB
  connector, driven today by `quartus_pgm` by hand (TESTING.md G0). Phase 8 of the
  device-update plan already designed the replacement — a small libusb USB-Blaster driver
  plus an SVF player, with the device-specific programming sequence exported by
  `quartus_cpf` at build time — and verified the `quartus_cpf` invocations exist. Nothing
  of it is implemented.
- **A device with a valid legacy EEPROM image cannot reach the boot ROM in software.**
  The legacy firmware has no reset/reboot vendor command (the original defect D25), so the
  path to `04b4:00f3` is the physical one: fit the J4 PMODE jumper and pull the cable.
  The GUI has no flow that asks for this.
- **`1d50:603b` is invisible.** The legacy VID/PID appears nowhere in `ddd-gui`; a legacy
  Duplicator is indistinguishable from no device.
- **No images ship with the GUI.** The update page is file-picker only
  (`update_page.cpp:319-330`); nothing installs or embeds `firmware.img` or any gateware
  artefact, so first provisioning currently depends on the user fetching files.
- **No legacy image set exists.** No legacy binary is tracked (the V1.8-era `.img`/`.jic`
  that once were have been removed, and predate the last legacy releases), and there is no
  defined way to roll a unit back.

### Where "legacy" is pinned in history

Commit `97f7dec` ("Move the VID:PID to pid.codes assigned 1209:2347") retired
`1d50:603b`. Its parent, **`97f7dec^`**, is therefore the last tree whose firmware and
gateware present the legacy identity — and, decisively, that tree is already the modern
monorepo: `fx3/mkimage` exists, `fpga/package.nix` and `build-local.sh` exist, and
`nix build .#fx3-firmware` / `.#bitstream` work from the root flake. The VID/PID move
predates the SPI register interface, the update agent and the factory/application split,
so the `97f7dec^` firmware and gateware are functionally the original architecture with
build-system cleanups. A legacy image set can therefore be built from that pinned ref
with today's toolchain, **once**, and committed — no resurrection of the V2.4-era build
system, and no obligation to keep the historical tree buildable afterwards. (The V1.8-era binaries recoverable from git history —
`3b531cc^:DE0-NANO/DomesdayDuplicator/DomesdayDuplicator.jic`,
`02a0d0c^:FX3-Firmware/domesdayDuplicator/Release/domesdayDuplicator.img` — were
considered and rejected as the source: they are older than the last legacy state and
carry no provenance.)

## Starting states (what the wizard can meet on the bus)

The bring-up flow must make sense of whatever is plugged in. The FX3 column below decides
only whether the *FX3* half has work to do and whether the jumper is needed; the FPGA half
runs regardless, for the reason given under *Two constraints*.

| Seen on the bus | Meaning | Bring-up action |
| --- | --- | --- |
| `04b4:00f3` | FX3 boot ROM — blank/corrupt EEPROM, or J4 fitted | FX3 half proceeds directly, no jumper step |
| `1d50:603b` | Legacy Duplicator firmware | J4 + replug to reach the boot ROM |
| `1209:2347`, `bcdDevice < 1.00` | Current-era VID but firmware predating the update agent (the U0 case) | J4 + replug (no reboot command either) |
| `1209:2347`, `bcdDevice ≥ 1.00` | Current firmware | FX3 half skipped (or offered as reinstall) |
| `04b4:00f1` or other `04b4` app PIDs | Cypress example firmware as shipped on some kits | J4 + replug |
| `04b4:0007` (alone or alongside) | The kit's USB-UART debug bridge — proves the kit is powered and cabled | Diagnostic only |
| `09fb:6001` | DE0-Nano on-board USB-Blaster | FPGA half proceeds |
| nothing relevant | Not connected, not powered, or no permissions | Connectivity/permissions page |

Two boundary facts shape the flow, both bench-established: changing FX3 boot mode always
requires a **physical power cycle** (a warm reset does not cut bus power —
`fx3/programmer/README.md:174-177`), and after any JTAG programming pass the FPGA is left
running Altera's serial flash loader, so a **power cycle is mandatory** there too
(`fpga/README.md:255-259`). The wizard therefore ends with one deliberate
pull-the-cables step covering both.

## Two constraints that decide the design

Everything below follows from these. They were not obvious at the outset and they are
what makes this a design rather than a script.

### The case: J4 is reachable, the DE0-Nano's USB is not

With the unit in its enclosure the FX3's `PMODE J4` header can be reached, but the
DE0-Nano's mini-USB connector cannot. **Any FPGA work therefore means opening the
case**, and no FX3 work does.

That tempts an adaptive design — do the FX3 first, ask the resulting firmware whether
the gateware needs anything, and only then send the user for a screwdriver. It is
rejected, and the reason is the whole shape of this flow: **a device whose FX3 firmware
is out of date is overwhelmingly likely to have gateware of the same vintage**, because
the two are updated together and always have been. An adaptive wizard would therefore
almost always arrive at "now open the case" *after* telling the user they would not need
to — making them power down, disassemble and start again. The wizard assumes the safe
path instead: **if bring-up is needed at all, both halves are done, and the first page
says so.** Both cables go on at the start, the case comes off at the start, and no
physical act is ever performed twice.

**Both cables stay connected for the whole of both flows**, and that is a requirement
rather than a convenience. The assembled unit is powered through the FX3 kit's USB 3.0
connector, and it can *also* be powered through the DE0-Nano's mini-USB — either one
alone keeps the whole assembly alive. Two consequences follow, and the second is a trap:

- Requiring both from the start means the FPGA is powered and enumerable whenever the
  wizard needs it, so an unpowered board is never mistaken for an unprogrammed one, and
  the user is never sent back for a cable mid-flow.
- **A power cycle means unplugging *both* cables.** Pulling only the USB 3.0 cable
  leaves the unit powered from the mini-USB, so the FX3 never re-reads its boot source
  and the FPGA never reloads from flash — the one step the whole procedure depends on
  silently does nothing, while the board stays lit and looks fine. Every power-cycle page
  says *both*, in those words, and the wizard treats a re-enumeration that never arrives
  as "check that both cables came out" before any other diagnosis. (The documentation's
  statement that the mini-USB is the DE0-Nano's power supply — `fpga-bitstream.md:13` —
  describes the board on its own, not the assembled unit; the page gains that
  distinction, since it is exactly what makes the partial power cycle plausible.)

### The electrical one: never run legacy firmware over modern gateware

The FX3 and the FPGA share one interconnect, and **one line changed direction** between
the legacy design and the current one:

| | `CTL_07` / `GPIO_24` |
| --- | --- |
| Legacy gateware (`97f7dec^:fpga/src/DomesdayDuplicator.v:96`) | FPGA **input** (`assign fx3_control[07] = GPIO1[13]`), unused |
| Legacy firmware (`97f7dec^:fx3/firmware/src/domesday-duplicator.c:268-279`) | FX3 **output**, claimed with `CyU3PDeviceGpioOverride(24, …)` and configured `driveLowEn = driveHighEn = CyTrue` — actively driven |
| Modern gateware (`fpga/application/DomesdayDuplicator.v:90,177`) | FPGA **output** — this is `spi_miso` |
| Modern firmware | FX3 **input** — it reads MISO |

Each pairing as shipped is consistent, and one mixed pairing is safe, but the fourth is
not:

- **modern firmware + legacy gateware** — both ends treat `CTL_07` as an input. The net
  floats and reads as noise, which is *exactly* the diagnosis the wizard wants ("this
  gateware has no register interface"). Every other line agrees too: the legacy gateware
  drives `CTL_00/03/04/11/12`, which modern firmware reads, and modern firmware drives
  the SPI clock, MOSI and chip-select, which legacy gateware reads. **Safe.**
- **legacy firmware + modern gateware** — the FPGA drives `CTL_07` as MISO while the FX3
  drives the same net push-pull. Two outputs on one wire, whenever they disagree.
  **Must not happen.**

  How badly, corrected on the schematic: the SuperSpeed Explorer Kit carries a **22 Ω
  series resistor on every `CTL` line** between the FX3's pin and the GPIF II header, so
  the fault current is not left to the two output stages alone. With a resistor that size
  in a loop of two CMOS drivers, contention is of the order of forty milliamps rather than
  the hundred-plus a bare short would draw.
  
  That is a real mitigation and it is not protection. Twenty-two ohms on a 100 MHz bus
  header is **source-series termination**, put there for signal integrity, and it is an
  order of magnitude below the value anyone would choose to make contention survivable.
  Forty milliamps is still past the per-pin DC maximum of both dies, and in this pairing
  the disagreement is sustained rather than transient — the legacy firmware claims the pin
  and holds it. So the ordering rule stands unchanged; what changes is the failure it
  averts, which is out-of-spec stress on two devices rather than a board that dies the
  moment it is powered.

So the ordering is not a matter of taste in either direction:

> **The FX3 is always the first thing to become modern and the last thing to become
> legacy.** Bring-up programs the FX3 first, then the FPGA. Rollback programs the FPGA
> first, then the FX3.

Both flows then pass only through the safe mixed state — and the ordering is not the whole
of why. **What a board is *running* only changes at a power cycle**: the FPGA reloads from
flash and the FX3 re-reads its boot source, both at that moment and not before. Each flow
has exactly one power cycle and it comes after both halves are programmed, so the two
change together and there is no window in between at all.

Reaching the bad pairing therefore needs the wrong order *and* an intermediate power
cycle. The orchestrator refuses the first; the flows' shape forbids the second, and there
is a widget test asserting that every power cycle either flow asks for falls before both
halves or after both. During bring-up the FX3 is additionally sitting in its boot ROM with
J4 fitted, driving nothing at all, which makes the FPGA work safe a third time over.

So this is a property of the design rather than a rule the procedure has to be trusted to
follow, and B-V0 is correspondingly not a gate — it measures the mixed state the flows
deliberately sit in, which is the one that lasts minutes on every board.

This is a reading of two source trees, not a measurement, and it concerns possible
hardware damage — so it is **verification item B-V0**, to be settled before either
wizard is pointed at real hardware, and the one item on this plan that is worth an
oscilloscope rather than a log file. It also deserves review against the legacy GPIF
configuration, which drives `CTL` lines of its own outside the GPIO overrides read
above. Nothing else in this plan changes shape if it is confirmed; if it is *not*
confirmed the orderings are still correct, just for weaker reasons.

## Menu restructure

`MainWindow::BuildToolsMenu()` (`main_window.cpp:488`) currently ends with a single
**&Firmware…** action (line 517) opening `FirmwareDialog`. That action becomes a
**Firmware** sub-menu:

```
Tools
 ├─ …player section, test data mode, analyse test data… (unchanged)
 └─ Firmware ▸
     ├─ Update firmware…              ← the existing FirmwareDialog, unchanged
     ├─ ────────────────
     └─ Legacy ▸
         ├─ Bring up a new or legacy board…
         └─ Roll back to legacy firmware…
```

The normal path stays first and keeps its behaviour, shortcut and status-bar hint (the
"Tools ▸ Firmware… can program it" text at `main_window.cpp:167` gains the new word
order). The two legacy actions open the new wizards described below and are always
enabled — they begin with their own connectivity checks, so there is no device-state
precondition to grey them out on.

**Isolation rule:** the normal update flow (`FirmwareDialog`, `UpdatePage`,
`UpdateOrchestrator` happy path) is not modified by this plan beyond the menu hosting
and the small orchestrator option in *Engine additions* below, which is inert unless a
wizard sets it. Every new screen lives in new files.

## The bring-up flow

A hand-built wizard on the `AutoCaptureWizard` pattern (`QDialog` + `QStackedWidget`,
object-name constants, buildable with null controllers so every page is
widget-testable). Pages, in order:

1. **What this does, and what it will ask of you.** Everything physical, listed before
   anything starts, because this is the page that decides whether the user does the job
   once: **take the unit out of its enclosure** (the DE0-Nano's USB connector cannot be
   reached otherwise), **connect both cables** — the kit's USB 3.0 cable and the
   DE0-Nano's mini-USB, shown by `fpga-usb-port` — and expect to **fit and then remove a
   jumper, and to unplug both cables twice** along the way (once, for a board already in
   its boot ROM). Names a realistic total time, and states the end state honestly: *"your
   Duplicator will be running current firmware with the recovery gateware; one ordinary
   firmware update completes it."* Both halves are always performed; the wizard does not
   offer to skip the FPGA on the strength of a guess.
2. **Connectivity and permissions.** Live-polling status rows, one per board:
   - **FX3 row** — searches for any row of the personality table above. Found →
     personality named in plain words. Nothing but `04b4:0007` → "kit is powered but not
     answering — check the USB 3.0 cable". Nothing at all → cable/power guidance.
   - **FPGA row** — searches for `09fb:6001`. A **charge-only cable** is the documented
     failure of this exact step (`fpga-bitstream.md:279`), and in the assembled unit it
     is especially misleading: the board is lit from the USB 3.0 side no matter what the
     mini-USB is doing, so "the lights are on" says nothing at all about whether the
     Blaster is connected. The row's "not found" text names the charge-only cable first,
     ahead of the not-connected case.
   - Each row's device is *opened*, not just enumerated, so permission failures surface
     here and nowhere later: Linux failure text points at the two udev rules files
     (`fx3/programmer/configs/70-domesday-duplicator.rules`,
     `fpga/configs/70-altera-usb-blaster.rules`) exactly as the existing messaging does
     (`libusb_device.cpp:526`); Windows text points at the driver-binding page (Zadig for
     `04b4:00f3`, and the same for `09fb:6001`).
   - Next enables when both rows are green.
3. **Image source.** The bundled provisioning set (see below) is preselected when
   present; a file picker accepts a downloaded one. Signature and digest verification run
   here, with the same key policy and development-channel banner as the update page.
4. **Reach the FX3 boot ROM.** The FX3 goes first, so that the legacy firmware is never
   running when the gateware becomes modern (*Two constraints*, above). Skipped when the
   FX3 row already shows `04b4:00f3`; otherwise *"Fit jumper J4 on the FX3 board, then
   unplug **both** USB cables and reconnect them"*, showing `fx3-j4-fitted`, and the page
   polls until the boot ROM enumerates. Both cables again, and for the same reason: the
   jumper only takes effect on a boot, and the unit does not boot while either cable
   still feeds it.
5. **Program the FX3.** The existing `RecoveryInstaller` path: RAM-load the current
   `firmware.img`, wait for the application personality, then the firmware's own update
   agent writes and verifies the EEPROM. One difference from the recovery flow: when J4
   is fitted, the orchestrator runs in the new *deferred-restart* mode (below) — it
   verifies the EEPROM readback but does not reset-and-confirm, because a reset with J4
   fitted lands back in the boot ROM, not the new firmware.
6. **Remove the jumper.** *"Remove jumper J4"*, showing `fx3-j4-removed` — the same board
   in the same framing as the previous photo, so the difference the user must make is the
   only difference between the two pictures. Skipped if step 4 was. No power cycle here:
   there is one at the end, and it serves both halves.
7. **Program the FPGA.** Plays the provisioning SVF through the USB-Blaster: writes the
   factory image, leaving the unit's defined boot state as *factory/recovery*. Progress
   is honest (the SVF player knows its position); the page states the expected duration
   from a measured per-byte cost. The FX3 is at this moment either in its boot ROM or
   running freshly written modern firmware over gateware that predates it — the safe
   pairing, and the only mixed state this flow visits.
8. **Power cycle.** *"Unplug **both** USB cables, wait a moment, then reconnect them."*
   One cycle discharges both obligations — the FX3's boot-source change and the mandatory
   post-JTAG cycle that clears Altera's serial flash loader — and both cables must come
   out, because either one alone keeps the unit powered and nothing reboots. The page
   says why, not just what. Polls until `1209:2347` returns, and if it does not, leads
   with "did both cables come out?".
9. **Verify and hand over.** Reads what the wizard can prove: personality `1209:2347`,
   `bcdDevice ≥ 1.00`, product-string commit matching the installed image, gateware
   registers reporting the factory image (`IMAGE_ROLE 0x00`, register map v2). Shows the
   result as ticks, then: *"Bring-up complete. The device is in its recovery gateware;
   run **Tools ▸ Firmware ▸ Update firmware…** to install the current firmware bundle."*
   — with a button that opens the normal dialog directly, and the note that the case can
   go back on first, because nothing after this point is physical.

A stopped or failed run is safe at every point and the failure page says so, in the same
calm register as the update page: the FX3 cannot be bricked (J4 always reaches the boot
ROM), the DE0-Nano cannot be bricked (JTAG always reaches the EPCS), and every step can
simply be run again.

### The photographs

The wizard asks for two physical acts — move a jumper, find a connector — and a
photograph is the only reliable way to ask for either. Three exist, taken 2026-08-17 and
staged in `docs-tech/` pending the phase that places them:

| Staged file | Becomes | Shows |
| --- | --- | --- |
| `FX3_J4_Closed.jpeg` | `fx3-j4-fitted` | The Explorer Kit with a shunt across the `PMODE J4` header, circled — the USB-boot position, wanted at step 5 |
| `FX3_J4_Open.jpeg` | `fx3-j4-removed` | The same board, same framing, header bare — the EEPROM-boot position, wanted at step 7 |
| `FPGA_USB.jpeg` | `fpga-usb-port` | The assembled stack with an arrow at the DE0-Nano's mini-USB connector, below the FX3 kit — wanted at step 1 and by the FPGA connectivity row |

Three things to get right when they move:

- **Rename to the project's vocabulary, not the bench's.** The documentation, the
  firmware README and the wizard all say the jumper is *fitted* or *removed*; the staged
  filenames say closed/open. The asset names above are the ones the plan uses throughout,
  and the wizard's alt text and captions say fitted/removed too, so a user never has to
  translate between two vocabularies for one jumper.
- **Two destinations, one source.** The docs site copies want
  `docs/content/development/hardware-programming/assets/` (beside the existing `FX3.png`
  and `DE0.jpg`, referenced as `![](assets/…)`); the wizard wants them in
  `ddd-gui/src/gui/resources/` and listed in `ddd_gui_resources.qrc`, which today holds
  only artwork. The same image serves both, so the docs page and the wizard page cannot
  drift apart — which is the whole point of using photographs rather than prose.
- **Downscale for the binary.** The originals are ~550 KB each at 1024×1365. The docs
  site can carry them as they are; the qrc copies should be resized to what the wizard
  actually displays, since three full-resolution JPEGs is a real addition to every
  Flatpak, MSI and DMG for pixels nobody sees.

The set is complete as it stands, and complete for a reason worth stating: because both
flows begin by taking the unit out of its enclosure, every photograph the wizard shows is
of an opened assembly — which is exactly what the user will be looking at when the page
appears. An enclosed-unit photograph would show a state the wizard never asks anyone to
work in.

## The FPGA JTAG path (adopting Phase 8's design)

This plan adopts the device-update plan's Phase 8 FPGA design unchanged in substance,
and makes it concrete:

- **`IJtagCable`** — a new engine seam over the USB-Blaster: libusb, Qt-free, a few
  hundred lines (FT245-style bit-bang + byte-shift modes, the protocol independently
  implemented in urjtag/OpenOCD/openFPGALoader; none of their code is used — see the
  licence position in the Phase 8 write-up). A fake implementation records the vector
  stream for tests.
- **SVF player** — a pure parser + JTAG TAP state machine from text to vectors,
  unit-tested against committed fixtures with no hardware. JBC (Jam STAPL byte-code) is
  the recorded fallback if SVF throughput or size disappoints; the seam is the same.
- **Build artefacts** — `fpga/package.nix` (and `build-local.sh`) gain one step each:

  ```
  quartus_cpf -c -q 4.5MHz -g 3.3 -n p \
      DomesdayDuplicatorProvisioning_write_jic.cdf DomesdayDuplicatorProvisioning.svf
  ```

  emitting `provisioning/DomesdayDuplicatorProvisioning.svf` beside the `.jic`, covered
  by `bitstream-provenance.py` like every other output. The Cyclone IV and serial flash
  loader knowledge stays inside Quartus, at build time, where it already lives.
- **First bench task, before anything else is built on it:** prove on hardware that a
  `.cdf`-derived SVF programs the EPCS end-to-end through the on-board Blaster, and
  measure its size and wall-clock time. Every sizing decision below (bundling, JBC,
  compression) waits for these two numbers. This was already flagged as Phase 8's first
  bench task; it is now this plan's gating item **B-V1**.

## The bundled provisioning set

So that first bring-up works offline, release builds of `ddd-gui` carry a **provisioning
set**: the current `firmware.img` plus the provisioning SVF, packaged as an ordinary
signed `.dddfw` (the manifest schema already makes components optional; it gains one new
component kind, `gateware-provisioning-svf`, and the reader refuses unknown kinds today,
so this is a versioned, gated extension). One format, one verifier, one key policy —
whether the set arrives bundled, hand-downloaded or (later) fetched.

Packaging mechanics, respecting §9 provenance (every artefact CI-built, GUI releases and
firmware releases are separate streams):

- The firmware release workflow assembles and attaches
  `domesday-duplicator-provisioning-<version>.dddfw` alongside the update bundle.
- The GUI pins the provisioning set it bundles in one committed file
  (`ddd-gui/packaging/bundled-provisioning.env`: release tag, asset URL, SHA-256). The
  packaging workflows fetch by that pin, verify the digest, and install the file into the
  platform data directory (Flatpak `/app/share/…`, MSI/DMG equivalents). CMake accepts
  `-DDDD_BUNDLED_PROVISIONING_FILE` for local builds; a build with no set bundled simply
  shows the file picker with no preselection — the honest state, same convention as the
  unpinned release key.
- The GUI never trusts the bundle for being bundled: the wizard verifies signature and
  digests identically for bundled and picked files.

**Size caveat:** the `.dddfw` container is uncompressed ustar by design, and SVF is
verbose hex text. If B-V1's measured SVF is large enough to embarrass the installers,
the recorded options are, in preference order: gzip the SVF payload inside the bundle
(engine gains a small vendored inflate — a new dependency, so it is raised, not assumed),
or switch the artefact to JBC. Decision deferred until the number exists.

## The legacy image set and the rollback flow

### Producing the set — once, then committed

The legacy build is **not** kept buildable in perpetuity and no workflow ever checks out
the historical tree. Instead the two legacy images are generated **one time**, by a
documented maintainer act, and committed to the repository as tracked binaries — the
same standing the vendored `cyfxflashprog.img` already has, with the same discipline: a
README recording exactly where they came from.

The one-time generation (task **B-V2**, performed on a branch and then committed):

- check out the pinned ref `97f7dec^` — the last tree presenting `1d50:603b`, already
  carrying the root flake, `fx3/mkimage` and `fpga/package.nix` — and run
  `nix build .#fx3-firmware` and `.#bitstream` from that tree, yielding the legacy
  `firmware.img` and the legacy single-image `.jic`. (If the historical flake lock no
  longer evaluates, build with the current lock over the historical source and say so
  in the provenance record.)
- commit the results under a new **`legacy/`** directory at the repository root:

  ```
  legacy/
    README.md        ← provenance: the pinned ref, the toolchain used, the date, the
                       SHA-256 of every file, and the regeneration procedure — plus the
                       statement that these files are frozen and never rebuilt by CI
    firmware.img     ← legacy FX3 firmware (enumerates 1d50:603b)
    gateware.jic     ← legacy single-image EPCS content (also the bench/JTAG reference)
  ```

  `.gitignore` gains the explicit exception (it has ignored `*.jic` since the original
  binaries were removed), and the licence-header check already exempts non-source files.

From then on the release workflow only **packages** what is tracked: it converts
`legacy/gateware.jic` to the rollback SVF with the pinned Quartus it already has (a
hand-authored `.cdf` naming the legacy `.jic`, committed beside the existing one in
`fpga/provisioning/`), assembles `domesday-duplicator-legacy-rollback.dddfw` — manifest
`channel: "rollback"`, components `firmware.img` (legacy) and `gateware-provisioning-svf`
(legacy) — signs it with the release key, and attaches it to the release. If keeping
even that conversion out of Quartus's hands proves worthwhile, the derived `.svf` can be
committed alongside the `.jic` after B-V1 has settled its size; that is a packaging
choice, not a design one.

The set is a **release asset, not bundled with the GUI** — rollback is a rare,
deliberate act, and the file picker (or the Phase 7 fetcher, later) serves it. Nothing
prevents bundling it later if B-V1's sizes make that free.

### The rollback wizard

Same chassis and pages as bring-up, reversed payloads — and **reversed order**, for the
electrical reason in *Two constraints*: the FX3 is the last thing to become legacy, so
the FPGA goes first. Rollback also opens the case and takes both cables, on the same
"never twice" principle.

1. Intro — the physical asks first, exactly as in bring-up (out of the enclosure, both
   cables, one power cycle), then what will be lost: self-update, the update protocol,
   this application's capture support. States what restores it (the bring-up flow).
   Requires an explicit typed confirmation; this is the one deliberately frightening
   screen in the application.
2. Connectivity and permissions — identical page, reused.
3. Image source — the legacy rollback `.dddfw`, file-picked (or fetched later);
   `channel: "rollback"` is what distinguishes it, and the wizard refuses a normal
   update bundle here just as the update page refuses a rollback bundle.
4. **FPGA first**, while modern firmware is still running: play `legacy-gateware.svf` —
   the legacy single image lands at `0x000000`, overwriting the factory image, and the
   boot-block sector is explicitly erased by the same SVF so no stale modern state
   survives. The unit is now modern-firmware-over-legacy-gateware: the safe pairing.
5. **FX3 second, and much simpler than it first appears.** A rollback target is by
   definition running the modern firmware, which is its own flasher — so the legacy
   image is written by an ordinary update-protocol transfer to target 0. **No jumper, no
   RAM load, no boot ROM.** The agent streams and SHA-verifies what the host sends, and
   the held-back-first-page commit ordering protects it like any other write; the legacy
   image is a valid `'CY'` boot image of the same ~110 KiB scale as the current one (the
   V1.8-era blob is 108,136 bytes), well inside the verified two-bank capacity. The one
   thing the wizard must do differently is aim the install-time compatibility gate the
   other way: a rollback bundle is *meant* to install something older than the running
   GUI understands, so the gate is satisfied by the `rollback` channel rather than
   bypassed — the rule stays machine-checked, only its direction changes. This step also
   uses *deferred restart*: the EEPROM is written and verified but the FX3 is **not**
   reset, because resetting it alone would start the legacy firmware while the FPGA is
   still in whatever state JTAG left it in. Both halves change identity together, at the
   power cycle, or not at all.
6. Power cycle — unplug **both** cables, reconnect both. One cycle serves the whole
   flow: it clears Altera's serial flash loader so the FPGA loads the legacy image, and
   it is the boot on which the FX3 first runs the legacy firmware.
7. Verify — the one check a legacy device permits: it enumerates as `1d50:603b`. The
   wizard says what this application can and cannot do from here, and points at the
   legacy software.

After rollback, `ddd-gui` recognises the device (new `kLegacy` personality) and shows
*"Legacy firmware — Tools ▸ Firmware ▸ Legacy can bring this board up to date"* instead
of "no device" — which also closes the loop for testing: rollback, then bring-up, on the
same bench unit, forever.

## Engine additions (summary)

All Qt-free, all behind seams, all with fakes — the pattern the engine already enforces:

| Addition | Where | Notes |
| --- | --- | --- |
| `kLegacyVendorId 0x1d50` / `kLegacyProductId 0x603b`, `DevicePersonality::kLegacy` | `wire_protocol.h`, `usb_device_info.h`, the three match sites | Detection only — the GUI never opens or drives legacy firmware, so no udev change is required (enumeration works unprivileged) |
| `IJtagCable` + `UsbBlasterCable` + fake | new `jtag_cable.{h,cpp}` | libusb; `09fb:6001` (Blaster I; IDs `6002/6003/6010/6810` recognised, out of scope to drive) |
| SVF player | new `svf_player.{h,cpp}` | pure; fixtures under `tests/unit/` |
| `UpdateOrchestrator` deferred-restart option | `update_orchestrator.h` | stop after on-device verify: no `0xD4`, no confirm; the wizard owns the power cycle and the post-cycle check. Inert unless set |
| Provisioning/rollback orchestrator | new `provisioning_orchestrator.{h,cpp}` | sequences the pages' device work in the fixed order each flow requires; drives `RecoveryInstaller` (bring-up only), the deferred-restart update and the SVF player; reports the same stage/progress shape the update worker already consumes. Rollback's FX3 step needs none of the recovery machinery — it is an ordinary target-0 transfer to firmware that is its own flasher |
| Manifest: `gateware-provisioning-svf` component, `rollback` channel | `update_manifest.{h,cpp}`, `make-update-bundle.sh` | versioned schema extension; readers that predate it refuse cleanly |
| Compatibility gate: `rollback` channel verdict | `update_gate.h` | a rollback bundle installs something older than the running application by design; the gate is *satisfied* by the channel, never bypassed, so the refusal of an ordinary too-old bundle is unchanged |
| Bundled-set locator | new small helper in `src/gui/` | platform data dir + `-DDDD_BUNDLED_PROVISIONING_FILE`; absent = no preselection |

GUI side: `firmware` menu restructure in `main_window.cpp`; two wizards in new files
(`board_bringup_wizard.*`, `legacy_rollback_wizard.*` — sharing the connectivity page,
step list and wording files); wording added to `update_text.*`-style tables, never
inline.

## Testing

- **Never automated flashing** (AGENTS.md §4): the SVF player's tests end at the vector
  stream against the fake cable; the wizards' widget tests run every page, branch and
  failure against fakes; nothing in CI touches an EEPROM or an EPCS.
- Unit tier: SVF parsing (fixtures including Quartus-emitted files), TAP state
  transitions, Blaster framing against the fake, manifest extension round-trips,
  rollback-vs-update bundle refusal in both directions, personality mapping for
  `1d50:603b`, deferred-restart orchestrator behaviour.
- Widget tier: both wizards end-to-end against fakes — including the J4-needed and
  J4-not-needed branches, permission-failure pages, a stopped SVF play, and the
  post-power-cycle timeout with its "did both cables come out?" first diagnosis. One
  test per wizard asserts the **step order** itself, since the order is a hardware-safety
  property and not a presentation choice: bring-up must never touch the FPGA before the
  FX3, rollback never the FX3 before the FPGA.
- Bench tier, added to TESTING.md §6 as they are first performed:
  - **B0** — bring-up of a bare pair (never-programmed kit, DE0-Nano with Terasic demo)
    to the verified recovery-ready state, then a normal update, then the T5 capture
    integrity run.
  - **B1** — bring-up of a legacy unit (`1d50:603b`), same exit.
  - **R0** — rollback of a current unit to legacy; verify `1d50:603b` enumeration and a
    capture under the legacy software.
  - **R1** — bring-up of the R0 unit straight back; proves the loop is closed.
  - **B-V1** — the gating SVF proof and measurements (size, duration) described above.
  - **B-V2** — the one-time pinned-ref legacy build, after which the images are
    committed and the task never runs again.
  - **B-V0** — the interconnect-direction check described in *Two constraints*: confirm
    that modern firmware over legacy gateware leaves `CTL_07`/`GPIO_24` undriven from
    both ends, and that the reverse pairing would contend. This gates every bench item
    above it, and it is the one measurement in this plan that protects hardware rather
    than data.

## Documentation deliverables (land with their phases)

- Development: a *USB-Blaster and SVF programming* page (the cable driver, the player,
  the artefact provenance); updates to *EPCS layout and boot flow* (the SVF route
  beside `quartus_pgm`) and to the hardware-programming appendix ("what the wizard
  replaces").
- User section: *Bringing up a new or legacy board* (cables, the jumper and connector
  photographs, what each wizard page means) and *Rolling back to legacy firmware* (what
  is lost, how to return). Windows driver-binding instructions extended to cover
  `09fb:6001`. The photographs land here in Phase 3 — moved out of `docs-tech/` into the
  hardware-programming assets directory and into the GUI's qrc in the same change, so the
  two copies enter the tree together.
- `fx3/programmer/configs/70-domesday-duplicator.rules` keeps its legacy-ID comment;
  the linux-device-access page notes that bring-up needs both rules files installed.

## Phases

Each lands independently and leaves the tree shippable.

1. **Recognition and menu.** `kLegacy` personality + the Firmware sub-menu (legacy
   entries present but opening a "not yet available" single page is *not* acceptable —
   the entries appear only when their wizards do; this phase ships only the sub-menu
   hosting the existing dialog and the legacy-device status text). Small, zero risk to
   the normal flow. *Exit:* legacy device shown by name; normal update path visibly
   unchanged (existing widget tests untouched and green).
2. **Cable and player.** `IJtagCable`, `UsbBlasterCable`, SVF player, fakes, fixtures;
   `.svf` added to the bitstream build and provenance. **B-V1 on the bench before the
   phase closes.** *Exit:* a developer programs a DE0-Nano's EPCS from a shell test
   harness using only the engine code and a CI-built SVF; sizes and durations recorded
   in TESTING.md.
3. **Bring-up wizard.** The full flow, file-picker image source only; deferred-restart
   orchestrator option; manifest component extension; `make-update-bundle.sh` learns
   the new component. **B-V0 is settled before any bench item in this phase runs** — it
   is the item that decides whether the orderings are safe. *Exit:* B0 and B1 pass on
   the bench; docs pages live.
4. **Bundled provisioning set.** Release workflow attaches the provisioning `.dddfw`;
   packaging pin + fetch-by-digest; wizard preselection. *Exit:* a clean machine with no
   network installs a release build and brings up a bare pair fully offline.
5. **Legacy set and rollback wizard.** The one-time B-V2 generation and the `legacy/`
   commit with its provenance README; release-workflow packaging of the rollback
   `.dddfw` from the tracked images; the wizard; the `kLegacy` post-rollback messaging.
   *Exit:* R0 and R1 pass on the bench; the legacy set is a published release asset
   whose payloads match the committed digests.

Phases 1 and 2 are independent; 3 needs both; 4 and 5 need 3 and are independent of each
other.

## Risks and open questions

- **B-V1 is load-bearing** (inherited from Phase 8): if a `.cdf`-derived SVF does not
  program the EPCS end-to-end, or is absurdly slow through the bit-banged Blaster, the
  fallback is the JBC interpreter — more code, same seams. Nothing else in this plan
  changes shape. Verify first, build second.
- **SVF size vs the uncompressed bundle format** — measured in B-V1; options recorded
  under *The bundled provisioning set*; any new decompression dependency is raised
  before it is vendored.
- **Windows driver surface doubles**: `09fb:6001` needs WinUSB binding like
  `04b4:00f3`, and a machine with Quartus installed has Altera's own driver and
  `jtagd` contending for the cable. The connectivity page must detect an
  un-openable Blaster and say which of the two problems it is; the docs page carries
  both remedies. macOS and Linux have no equivalent problem.
- **The interconnect direction change is the one risk here that can stress hardware**,
  and it is why both orderings are fixed rather than chosen: legacy firmware drives
  `CTL_07` push-pull while modern gateware drives the same net as SPI MISO. The Explorer
  Kit's 22 Ω series resistor on that line bounds the resulting current without making it
  acceptable (see *Two constraints*). The orderings mean neither flow ever creates that
  pairing, but the analysis is a source reading and B-V0 should confirm it before either
  wizard meets hardware. The tolerated mixed state — modern firmware over legacy gateware — lasts
  minutes in both flows; the firmware must already survive an unresponsive register
  interface (the factory and absent-gateware cases), and R0 is where that is proved, so
  R0 runs early in Phase 5 rather than last.
- **The partial power cycle** is the likeliest way for a user to get stuck: either cable
  alone keeps the assembly powered, so pulling one changes nothing while the board stays
  lit. Mitigated by wording, by the wizard's first diagnosis on a re-enumeration
  timeout, and by asking as few times as the hardware allows — once for rollback, and
  for bring-up twice only when the jumper has to be fitted, which is the one case the
  FX3's boot-source rule makes unavoidable.
- **Pinned-ref buildability** (B-V2) is a one-time risk, retired the day the images are
  committed: if the historical flake lock no longer evaluates, the current lock is
  applied over the historical source and the provenance README says so. After the
  commit, nothing ever builds that tree again.
- **The J4 photo problem is solved.** The wizard's jumper instruction is only as good as
  its illustration, and prose describing pin positions is not good enough. All three
  photographs now exist and match the state the user will actually be working in (see
  *The photographs*).
