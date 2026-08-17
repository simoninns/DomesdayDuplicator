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
`fx3/programmer/README.md:174-177`), and a JTAG configuration is volatile, so what the
FPGA is running is not what the flash holds until it has reloaded. The wizard therefore
ends with one deliberate pull-the-cables step covering both. (Under the design this plan
originally carried, the FPGA's power cycle was mandatory for a different reason — JTAG
programming left Altera's serial flash loader resident, `fpga/README.md:255-259`. That
route is gone, and the cycle is now what *proves* the flash rather than what escapes a
vendor loader. `quartus_pgm` still leaves the loader behind, so G0 keeps the old reason.)

## Two constraints that decide the design

Everything below follows from these. They were not obvious at the outset and they are
what makes this a design rather than a script.

### The case: J4 is reachable, the DE0-Nano's USB is not

With the unit in its enclosure the FX3's `PMODE J4` header can be reached, but the
DE0-Nano's mini-USB connector cannot. **Any work needing JTAG therefore means opening the
case**, and no FX3 work does. That is bring-up only: rollback reaches the FPGA through
firmware that is already running, so it stays closed.

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

**Both cables stay connected for the whole of bring-up**, and that is a requirement
rather than a convenience. (Rollback needs only the USB 3.0 cable and never opens the
case — see *The rollback wizard*, where the reason is that it writes both images through
firmware that is already running.) The assembled unit is powered through the FX3 kit's
USB 3.0 connector, and it can *also* be powered through the DE0-Nano's mini-USB — either
one alone keeps the whole assembly alive. Two consequences follow, and the second is a
trap:

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
halves or after both. During bring-up the FX3 is additionally running the modern firmware
by the time the FPGA is touched — it has to be, since that firmware is what writes the
EPCS — so the pairing at that moment is the safe one by construction rather than by
sequencing alone.

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
7. **Program the FPGA.** Two operations behind one page (*The FPGA path*, below): the
   factory image is configured into the FPGA over the USB-Blaster — volatile, 2.6 seconds,
   nothing written — and the firmware that step 5 installed then writes that same image
   into the EPCS at `0x000000` through its own flash bridge, on the ordinary update path.
   The unit's defined boot state afterwards is *factory/recovery*. Progress is honest for
   both halves: the SVF player knows its position, and the update agent reports bytes
   written exactly as it does for a gateware update. The FX3 is at this moment running
   freshly installed modern firmware over gateware that predates it — the safe pairing,
   and the only mixed state this flow visits. It must be **running** that firmware and not
   sitting in its boot ROM, which is what fixes this step after the jumper comes out
   rather than before.
8. **Power cycle.** *"Unplug **both** USB cables, wait a moment, then reconnect them."*
   One cycle discharges both obligations — the FX3 re-reads where it boots from, and the
   FPGA drops the volatile configuration JTAG gave it and loads the one just written to
   flash — and both cables must come
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

## The FPGA path — JTAG configures, USB writes

**Rewritten 2026-08-17, after B-V1 disproved the design it replaces.** The original —
adopted from the device-update plan's Phase 8 — had a `quartus_cpf`-derived provisioning
`.svf` write the EPCS directly. The bench showed that file is only the second half of the
job: it speaks Altera's **Virtual JTAG** protocol (`SIR 00E` is USER1, `SIR 00C` is USER0)
to Altera's **Serial Flash Loader**, a soft design that has to be configured into the FPGA
before any of it means anything, and it carries no configuration of its own — its largest
scan is 2,108 bits against the 5,748,760 an EP4CE22 needs. `quartus_pgm` supplies the
loader from its own installation. `quartus_cpf` does not put it in the file. Supplying it
by hand made the same file play clean, which is the proof. Evidence in TESTING.md B-V1.

Shipping Altera's loader inside a provisioning set was one way out, and it raises a
redistribution question this project has no need to answer. The other way is to use **this
project's own factory image**, which is the design from here on:

> **JTAG configures; it never writes flash.** The factory image is configured into the
> FPGA volatilely over the DE0-Nano's on-board Blaster, and the flash is then written by
> the firmware's own update agent over USB — the path G1 has already bench-proved.

Bench-established, 2026-08-17: `DomesdayDuplicatorFactory.sof` converts to a 1.4 MB
configure-only `.svf` that plays in **2.6 seconds**, after which the factory image does
exactly what it does at power-on — it validates whatever the flash holds and either hands
over to it or stays resident with its bridge. On a bare board there is nothing valid to
hand over to, so the bridge is there, and that is the bring-up case; on a board that
already works the application image takes over and answers instead. Either way the
firmware has a route to the EPCS, which is all this needs.

What the change is worth, beyond being the version that works:

- **Every artefact is ours.** No vendor loader is redistributed, and the one thing played
  over JTAG is a file this project compiles.
- **One flash writer, not two.** The EPCS is written by the update agent and by nothing
  else, so the erase/program/readback/digest discipline that protects a field update
  protects a bring-up too — instead of a second implementation living inside a vector file
  nobody here can read.
- **Minutes become seconds on the JTAG side.** 5.7 Mbit shifted once, at 2.6 s, replaces
  73.3 Mbit and ~105 seconds of declared waits.
- **The bundle shrinks by an order of magnitude.** 1.4 MB of configure vectors plus a
  ~200 KB image, against the 18.4 MB flash-writing `.svf` — which is what made compression
  and JBC live questions. Both are now moot; the *Size caveat* below is withdrawn.
- **The whole FPGA step is about twenty seconds.** 2.6 s to configure, then a flash write
  the size of G1's: the `.cof` files enable Cyclone IV bitstream compression, so an image
  lands in the flash at around 200 KB rather than 718 KB, and G1 measured 212 KB at 17
  seconds. The page that was going to warn about several minutes of erasing now warns
  about nothing.

The engine pieces are unchanged and already built — they were never the part that was
wrong:

- **`IJtagCable`** — the engine seam over the USB-Blaster: libusb, Qt-free (FT245-style
  bit-bang + byte-shift modes, the protocol independently implemented in
  urjtag/OpenOCD/openFPGALoader; none of their code is used — see the licence position in
  the Phase 8 write-up). A fake implementation records the vector stream for tests. One
  defect found on hardware and fixed: it never byte-shifts a scan whose TDO is captured
  (B-V1).
- **SVF player** — a pure parser + JTAG TAP state machine from text to vectors,
  unit-tested against committed fixtures with no hardware. Its job is now strictly
  configuration, which is the smaller half of what it was written for. JBC is no longer a
  recorded fallback: nothing about a 2.6-second play needs one.
- **Build artefacts** — `fpga/package.nix` (and `build-local.sh`) gain two steps and lose
  one:

  | Change | Why |
  | --- | --- |
  | `quartus_cpf -c -q 4.5MHz -g 3.3 -n p factory/DomesdayDuplicatorFactory_write_sof.cdf factory/DomesdayDuplicatorFactoryConfigure.svf` | the vectors that configure the factory image. The `.cdf` is already committed and already installed |
  | a committed `factory/DomesdayDuplicatorFactory.cof` emitting `DomesdayDuplicatorFactory_auto.rpd` | the factory image as raw EPCS bytes, exactly as the application already publishes `DomesdayDuplicator_auto.rpd`. This is what target 2 below writes |
  | drop `provisioning/DomesdayDuplicatorProvisioning.svf` | nothing can play it, and keeping an unplayable file in the release invites someone to try. The `.jic`, the `.map` and the `.cdf` stay: `quartus_pgm` is still the documented Quartus route (G0) |

  Both new outputs are covered by `bitstream-provenance.py` like every other artefact.

### The firmware change this needs

Bring-up writes the factory region at `0x000000`, and the firmware refuses to — by design
and in writing (`update-protocol.h:93`):

> The factory image at `0x000000` is never written from here by any path: it is
> JTAG-provisioned once and the freeze policy in `fpga/factory/README.md` is what keeps it
> that way.

The host cannot do it instead. A register write packs an address and one byte into
`wValue`, so driving the flash bridge from the host is one USB control transfer per byte,
and 8 MB of EPCS is millions of round trips — which is precisely why the agent shifts the
bytes on the device (G1: 212 KB in 17 seconds).

So the freeze stops being *"no path exists"* and becomes *"the field path cannot, and the
bring-up path is exempt"* — a third target:

| | |
| --- | --- |
| `UPDATE_TARGET_EPCS_FACTORY (2u)` | base address `0x000000`, with the same erase, program, readback and digest discipline target 1 already has |
| Unlocked by a magic in `updateBegin_t.flags` | the reserved-flags-must-be-zero rule already enforced in `updateBeginDecode()` becomes the guard, at the cost of no new request and no new state |
| No boot block | `updateBootBlockEncode()` describes the *application* image, so target 2 skips the boot-block write at `update-agent.c:573` entirely |
| Length ceiling is the boot block | `updateGatewareIsPlausible()` takes the target: the factory region ends at `UPDATE_EPCS_BOOT_BLOCK_ADDRESS`, not at the end of the device |
| Address arithmetic takes its base from the target | the `UPDATE_EPCS_APPLICATION_ADDRESS` sites in `update-agent.c` (lines 253, 340, 540) become one `updateEpcsTargetBase(target)`, which is host-testable arithmetic and therefore tested |

**Why the flags word and not something stronger.** Settled 2026-08-17: recovery from a
half-written factory region needs JTAG, and the DE0-Nano has a USB-Blaster *soldered to
it*. Every board this can be run against carries its own recovery cable, and the one flow
that uses target 2 has that cable connected by definition — so the loss the freeze policy
was defending against is an inconvenience on this hardware rather than a dead board. What
is left worth defending against is a **mistaken** host, not a determined one: a magic word
in a field already required to be zero means a host that means target 1 and sends a 2 is
refused, which is the whole of the realistic risk.

Two guards were considered and rejected. A separate unlock request (like the flash
bridge's own `44 44 55 AA`) buys nothing the flags word does not, at the cost of a new
request and a new piece of device state. Gating on the running gateware reporting
`IMAGE_ROLE 0x00` looked precise — it permits exactly the bare-board case — until the
bench showed a re-provisioned working board hands over to its *application* image, which
would then refuse the write; a guard that breaks re-running bring-up on a board that
already works is worse than the one it replaces.

**This is a protocol change across three components** (AGENTS.md §2, §13), and it is
additive, which is what the version range was built for. `PROTOCOL_VERSION_H` becomes
`0x02` and `kProtocolVersionMaximum` becomes 2, so this build accepts 1 and 2 and a v1
device keeps working untouched. The cost lands in the other direction and is the intended
one: an application build predating the bump refuses a bundle carrying v2 firmware with
*"update the application first"*. It still captures from a v2 device — `protocol_version`
gates the update path and the bring-up wizard's verify row, not the capture path — so the
consequence is an ordering requirement between the two release streams and nothing worse.
The bring-up wizard is unaffected either way: it RAM-loads the firmware out of the set it
has just verified, so it knows what it is talking to without having to ask.

### What plays, in what order

The FPGA half of bring-up becomes four device operations with no power cycle between
them, all while the FX3 runs the firmware the wizard RAM-loaded a page earlier:

1. play `DomesdayDuplicatorFactoryConfigure.svf` over the Blaster — 2.6 s, volatile,
   writes nothing;
2. the firmware's register bridge is now answering, so `UPDATE_BEGIN` target 2 writes
   `DomesdayDuplicatorFactory_auto.rpd` to `0x000000`, verified by readback digest like
   any other update;
3. nothing else. The application image and its boot block are not written here — the exit
   state stays *factory/recovery gateware*, finished by one ordinary update, exactly as
   before;
4. the single power cycle at the end of the wizard, which the FX3 needs anyway, is what
   makes the flash content live.

**B-V1's remaining steps are the proof of 2**, and nothing has yet written a byte to an
EPCS by this route.

## The bundled provisioning set

So that first bring-up works offline, release builds of `ddd-gui` carry a **provisioning
set**: an ordinary signed `.dddfw` carrying three payloads —

| Payload | Component kind | What it is for |
| --- | --- | --- |
| `firmware.img` | `firmware` | RAM-loaded into the boot ROM, then written to the EEPROM by itself |
| `DomesdayDuplicatorFactoryConfigure.svf` | `gateware-provisioning-svf` | configures the factory image into the FPGA so the flash bridge exists |
| `DomesdayDuplicatorFactory_auto.rpd` | `gateware-factory` | the factory image as EPCS bytes — around 200 KB, since the `.cof` compresses it as it already does the application's 212 KB — written to `0x000000` by target 2 |

The manifest schema already makes components optional and the reader refuses unknown
kinds, so each addition is a versioned, gated extension. One format, one verifier, one key
policy — whether the set arrives bundled, hand-downloaded or (later) fetched. The third
kind is what this rewrite adds; the vectors are no longer the thing that writes flash, so
the set now carries the image they used to encode.

Packaging mechanics, respecting §9 provenance (every artefact CI-built, GUI releases and
firmware releases are separate streams):

- The firmware release workflow assembles and attaches
  `domesday-duplicator-provisioning-<version>.dddfw` alongside the update bundle, signed
  with the same key, and the release gate verifies both against the committed public key —
  plus the one property that separates them, that the provisioning set carries vectors and
  the update bundle does not. The `.svf` itself is attached too, for `ddd-jtag` and the
  bench.
- The GUI pins the provisioning set it bundles in one committed file
  (`ddd-gui/packaging/bundled-provisioning.env`: release tag, asset URL, SHA-256).
  `tools/fetch-bundled-provisioning.sh` is the one implementation of fetch-and-verify, so
  the three packaging workflows cannot differ in whether they check the digest; each then
  passes `-DDDD_BUNDLED_PROVISIONING_FILE` and asserts, after installing what it built,
  that the set is where the application will look for it. CMake installs it under one fixed
  name per platform layout (`share/<app-id>/` on Linux, `Contents/Resources` on macOS,
  beside the executable on Windows) and `ddd-gui/src/gui/bundled_provisioning.cpp` is the
  matching search. A build with no set bundled simply shows the file picker with no
  preselection — the honest state, same convention as the unpinned release key.
- The GUI never trusts the bundle for being bundled: the wizard verifies signature and
  digests identically for bundled and picked files, and reads the bundled one on the image
  page rather than at construction, so it is judged by the key policy the application
  finished choosing rather than the default it started with.
- `tools/dev-bundle.sh --kind provisioning` produces the same file locally, which is what
  makes the whole path testable without a release.

**Size caveat, withdrawn 2026-08-17.** The concern was the 18.4 MB flash-writing `.svf`
in an uncompressed ustar container, and the recorded options were a vendored inflate or a
switch to JBC. Neither is needed: the configure-only vectors are 1.4 MB and the factory
image 212 KB, so a complete set is about 1.8 MB — no new dependency, and nothing that
embarrasses an installer.

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

The legacy gateware is committed as **raw EPCS bytes** (`gateware.rpd`, emitted by a
`.cof` at generation time) rather than as a `.jic`, because that is what a rollback now
writes: the image goes to `0x000000` through the firmware's flash bridge, not through
JTAG. The `.jic` is committed too, as the reference a bench can hand to `quartus_pgm`.

From then on the release workflow only **packages** what is tracked: it assembles
`domesday-duplicator-legacy-rollback.dddfw` — manifest `channel: "rollback"`, components
`firmware.img` (legacy) and `gateware-factory` (legacy) — signs it with the release key,
and attaches it to the release. No Quartus is involved at all, which is one more thing the
rewrite below removes from this flow.

The set is a **release asset, not bundled with the GUI** — rollback is a rare,
deliberate act, and the file picker (or the Phase 7 fetcher, later) serves it. Nothing
prevents bundling it later if B-V1's sizes make that free.

### The rollback wizard

Same chassis and pages as bring-up, reversed payloads — and **reversed order**, for the
electrical reason in *Two constraints*: the FX3 is the last thing to become legacy, so
the FPGA goes first.

**Rollback needs no JTAG cable and no screwdriver**, and that follows from the same
rewrite: a rollback target is by definition running the modern firmware over modern
gateware, so the flash bridge is already there and both images are written over the USB 3.0
cable. Nothing here needs the DE0-Nano's mini-USB, so nothing here needs the case off.

1. Intro — the physical asks first (one cable, one power cycle), then what will be lost:
   self-update, the update protocol, this application's capture support. States what
   restores it — and states the asymmetry honestly, because it is the thing a user will
   want to know before agreeing: rollback is done through one cable in a closed case, and
   coming back is not. Requires an explicit typed confirmation; this is the one
   deliberately frightening screen in the application.
2. Connectivity and permissions — the same page, with the FPGA row omitted: this flow
   never opens the Blaster, so a missing one is not a fault to report.
3. Image source — the legacy rollback `.dddfw`, file-picked (or fetched later);
   `channel: "rollback"` is what distinguishes it, and the wizard refuses a normal
   update bundle here just as the update page refuses a rollback bundle.
4. **FPGA first**, while modern firmware is still running and doing the writing: an
   ordinary update transfer to **target 2** puts the legacy single image at `0x000000`,
   over the factory image. An EP4CE22 image is 718 KB of raw configuration and about
   212 KB with the compression the `.cof` files enable — either way inside the 1 MB the
   factory region allows, which is worth checking at B-V2 rather than assuming, since the
   legacy `.cof` is whatever that tree shipped. The boot block and the application image are left
   exactly as they are — the legacy gateware is a single image with no bootloader and
   never reads either, and leaving them costs a rollback nothing while giving a later
   bring-up a device that comes straight back up in its application image. The unit is now
   modern-firmware-over-legacy-gateware: the safe pairing.
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
   still running the modern gateware it was configured with — the one pairing that must
   not happen. Both halves change identity together, at the power cycle, or not at all.
6. Power cycle — unplug the cable, reconnect it. One cycle serves the whole
   flow: the FPGA reloads from flash and so comes up on the legacy image, and
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
| SVF player | new `svf_player.{h,cpp}` | pure; fixtures under `tests/unit/`. Configuration only — no flash-writing file is ever played |
| `UpdateTarget::kEpcsFactory` and its unlock flag | `device_updater.h`, `update_orchestrator.cpp` | target 2 with the factory-write magic in the begin flags. Offered by the two wizards and by nothing else; the update dialog cannot reach it |
| `UpdateOrchestrator` deferred-restart option | `update_orchestrator.h` | stop after on-device verify: no `0xD4`, no confirm; the wizard owns the power cycle and the post-cycle check. Inert unless set |
| Provisioning/rollback orchestrator | new `provisioning_orchestrator.{h,cpp}` | sequences the pages' device work in the fixed order each flow requires; drives `RecoveryInstaller` (bring-up only), the deferred-restart update and the SVF player; reports the same stage/progress shape the update worker already consumes. Rollback's FX3 step needs none of the recovery machinery — it is an ordinary target-0 transfer to firmware that is its own flasher |
| Manifest: `gateware-provisioning-svf` and `gateware-factory` components, `rollback` channel | `update_manifest.{h,cpp}`, `make-update-bundle.sh` | versioned schema extension; readers that predate it refuse cleanly |
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
  `1d50:603b`, deferred-restart orchestrator behaviour. Phase 6 adds, on the firmware's
  host-testable side: target 2 refused without the unlock flag, refused with the flag on
  targets 0 and 1, the factory region's length ceiling at the boot block, the base address
  each target resolves to, and that target 2 writes no boot block.
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
  - **B-V1** — the gating JTAG proof and its measurements. Partly performed 2026-08-17: it
    found the cable's byte-shift read defect and then found that the flash-writing `.svf`
    is not self-contained, which is what Phase 6 answers. Steps 4–6 — the flash write
    itself, its duration, and the comparison against `quartus_pgm` — are what close it.
  - **B2** — the offline bring-up: a release build installed on a machine with no
    network at all, whose wizard must arrive at the image page with the set it was
    packaged with already chosen and named. Every part of that path — pin, fetch, install
    layout, search — is a claim about a machine other than the one it was built on.
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
- With Phase 6, three pages describe something that is no longer true and are corrected in
  the same change: *Bringing up a new or legacy board* (step 7 is now configure-then-write
  and takes seconds rather than minutes; the set carries an image as well as vectors; step
  8's power cycle no longer clears "Altera's flash loader", because none is ever loaded),
  *Rolling back to legacy firmware* (no mini-USB, no case off, one cable throughout), and
  the *Device update mechanism* page, which is where the protocol is specified and
  therefore where target 2, its unlock flag and version 2 are defined. The firmware and
  host copies of those constants cite it, as §2 requires.

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
   network installs a release build and brings up a bare pair fully offline — bench item
   **B2**, which cannot run until a firmware release has published a set and the pin names
   it. Everything either side of that is built and tested: the release workflow's two
   bundles and their gate, the pin file, the fetch script, the CMake install for all three
   layouts, the run-time search, and the wizard's four preselection states.
5. **Legacy set and rollback wizard.** The one-time B-V2 generation and the `legacy/`
   commit with its provenance README; release-workflow packaging of the rollback
   `.dddfw` from the tracked images; the wizard; the `kLegacy` post-rollback messaging.
   *Exit:* R0 and R1 pass on the bench; the legacy set is a published release asset
   whose payloads match the committed digests.
6. **Own factory image over JTAG** — the rewrite this plan took on 2026-08-17, after B-V1
   found that the design phases 2 and 3 shipped cannot work (*The FPGA path*, above).
   Three components in one change, which AGENTS.md §2 requires be said in those words:
   - **firmware** — `UPDATE_TARGET_EPCS_FACTORY`, the begin-flags unlock, the target-based
     base address and length ceiling, no boot block on target 2, `PROTOCOL_VERSION_H`
     `0x02`. All the arithmetic is in `update-protocol.c`, which is host-tested;
   - **gateware build** — the factory configure `.svf` and the factory `_auto.rpd`; the
     unplayable provisioning `.svf` withdrawn; provenance covers both new outputs;
   - **host** — `kProtocolVersionMaximum` 2, the `gateware-factory` component kind,
     `UpdateTarget::kEpcsFactory`, and the bring-up wizard's FPGA step becoming
     configure-then-update. The rollback wizard loses its JTAG step entirely.

   *Exit:* B-V1 steps 4–6 pass — a board provisioned by this route is indistinguishable
   from one `quartus_pgm` provisioned — and B0 passes on a bare pair.

Phases 1 and 2 are independent; 3 needs both; 4 and 5 need 3 and are independent of each
other. **Phase 6 is a correction to 2 and 3, and bring-up does not work on hardware until
it lands** — 1 to 5 are otherwise complete and the tree is shippable, because the wizard
fails cleanly at the vectors rather than half-programming anything.

## Risks and open questions

- **B-V1 was load-bearing, and it bore.** The risk was recorded as *"if a `.cdf`-derived
  SVF does not program the EPCS end-to-end, the fallback is the JBC interpreter"*, and the
  answer turned out to be neither: the file cannot program the EPCS at all, and JBC would
  have inherited the same defect, being the same content in a denser encoding. The
  replacement design is in *The FPGA path*. Two lessons worth keeping: an IDCODE probe is
  step 1 of any first cable session, and **the phase closed on unit tests where its own
  exit criterion asked for a bench run** — B-V1 was written into the plan as Phase 2's gate
  and the phase shipped without it.
- **The factory-region freeze is deliberately broken**, and the reasoning is recorded
  rather than assumed: a firmware that can write `0x000000` can, if interrupted, leave a
  board that only JTAG can recover. Accepted because the DE0-Nano carries a USB-Blaster on
  the board itself, so that cable is present on every unit this could happen to and is
  already connected during bring-up. The residual risk is a mistaken host, and the
  begin-flags magic is sized for that. If the board ever ships on a carrier without an
  on-board Blaster, this decision is the first thing to revisit.
- **The protocol bump strands old application builds against new firmware** in one
  direction only: an application predating `kProtocolVersionMaximum = 2` refuses a bundle
  carrying v2 firmware with *"update the application first"*. It still captures from such a
  device. This is the versioning mechanism working, but it does mean the GUI release has to
  reach users before the firmware release that needs it.
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
