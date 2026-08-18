# Board Bring-Up — Plan

## Purpose

One flow: a user with a board that is not running current firmware — a bare SuperSpeed
Explorer Kit, a bare DE0-Nano still holding Terasic's demo image, an assembled Duplicator
running the legacy `1d50:603b` firmware, or a unit in any half-programmed state in
between — opens `ddd-gui`, follows one wizard, and ends with a **fully current unit**:
FX3 running the release firmware, EPCS holding the factory and application images, the
application image answering on the bus. Nothing follows it; there is no second step.

This is the downsized revision of 2026-08-18. The previous revision of this plan
(`board-bringup-and-rollback-plan.md`, implemented through its Phase 6) carried three
things this one deliberately does not, and the decisions are recorded in *What was
withdrawn* below:

- a **rollback flow** returning a unit to the legacy firmware, with a frozen legacy image
  set committed to the repository;
- a **separate provisioning set** — a second `.dddfw` kind carrying different payloads
  from the update bundle;
- a **two-step exit** — bring-up ended at "recovery gateware, now run one ordinary
  update".

The downsizing follows one principle, which is the sentence the rest of this document
unpacks:

> **Bring-up is the ordinary update, plus the minimum that makes the ordinary update
> possible.** Two volatile preliminaries — configure the FPGA over JTAG, RAM-load the
> firmware into the FX3's boot ROM — put a device on the bus that the update path already
> knows how to program. Everything after that *is* the update path: the same bundle, the
> same verifier, the same agent, the same targets.

## Authoritative references (in-tree)

- Conventions: [AGENTS.md](../AGENTS.md) (§2 protocol-change rule, §4 hardware safety —
  nothing automated ever writes EEPROM or EPCS, §6 naming, §9 provenance)
- The update mechanism this composes with:
  [device-update-implementation-plan.md](device-update-implementation-plan.md)
- FX3 boot behaviour, J4 jumper, identities: [fx3/README.md](../fx3/README.md)
- EPCS layout, factory/application split:
  [docs/content/development/epcs-layout-and-boot-flow.md](../docs/content/development/epcs-layout-and-boot-flow.md),
  [fpga/README.md](../fpga/README.md)
- Bench procedures: [TESTING.md](../TESTING.md) §6 (U5, U6, G0, G1, B-V1)
- The implemented machinery this revision reshapes: `board_bringup_wizard.*`,
  `bringup_worker.*`, `bringup_orchestrator.*`, `device_recovery.*`,
  `update_orchestrator.*`, `jtag_cable.*`, `svf_player.*`

## Why the starting state does not matter

The previous revision opened with a table of eight bus personalities and what the wizard
should do about each. The downsized flow replaces the table with one physical fact:
**jumper J4 forces the FX3 into its boot ROM from any state whatsoever**, and a JTAG
configuration replaces whatever the FPGA is running regardless of what the flash holds.
The wizard therefore never diagnoses the board. It asks for the jumper, waits for
`04b4:00f3`, and proceeds identically whether the unit was blank, legacy, current,
Cypress-example, or abandoned halfway through a previous run.

Two consequences are worth their own sentences:

- **The flow is idempotent.** Every state a stopped run can leave behind is a state the
  next run accepts, because the next run assumes nothing. "Just run it again" is the
  whole of the failure recovery story.
- **The only conditional left is a courtesy.** A board already sitting in its boot ROM
  (blank EEPROM, or J4 already fitted) satisfies the jumper page on arrival; the page is
  skipped rather than performed. Nothing else branches.

## The electrical reading, and the one ordering rule

The FX3 and the FPGA share one interconnect, and one line changed direction between the
legacy design and the current one: `CTL_07`/`GPIO_24` is a push-pull FX3 **output** under
legacy firmware and the FPGA's **MISO output** under modern gateware. Legacy firmware
over modern gateware is therefore two output stages on one wire — bounded by the Explorer
Kit's 22 Ω series resistors to roughly forty milliamps, which is past the per-pin DC
maximum of both dies and sustained rather than transient. The other mixed pairing, modern
firmware over legacy gateware, leaves the net floating from both ends and is safe. (The
full source reading is preserved in TESTING.md's B-V1 notes and the git history of the
previous plan revision.)

The previous revision needed an ordering rule in each direction and a bench measurement
(B-V0) gating two flows. The downsized flow needs one rule and no measurement, because
the dangerous pairing is now **unreachable by construction**:

> **The FX3 reaches its boot ROM before the FPGA is touched, and only ever runs the new
> firmware afterwards.** In the boot ROM the FX3's GPIF and GPIO pins are unconfigured
> and undriven, so the FPGA can change identity under it freely; by the time any firmware
> runs again, both the volatile configuration and everything being written to flash are
> modern.

Legacy firmware never executes again after the jumper page's power cycle — there is no
rollback to bring it back — so no sequence of wizard pages, stops, or reruns can put
legacy firmware over modern gateware. One write ordering inside the install preserves
this even for a run abandoned half way and power-cycled with the jumper removed: **the
EEPROM is written before either flash image** (see the flow, page 6), so every partial
state boots either the old firmware over the old gateware or the new firmware over
something, and both of those float rather than fight.

## The case and the cables

With the unit in its enclosure the FX3's `PMODE J4` header can be reached but the
DE0-Nano's mini-USB connector cannot, and bring-up needs both. So the case comes off at
the start, both cables go on at the start, and the first page says so — no adaptive
"maybe you won't need the screwdriver" path, for the same reason as before: a board that
needs bring-up at all needs all of it.

The assembled unit is powered through **either** cable alone, which sets the one trap
worth repeating everywhere it applies: **a power cycle means unplugging both cables.**
Pulling only one leaves the unit powered, the FX3 never re-reads its boot source, the
FPGA never reloads from flash, and the one step everything depends on silently does
nothing while the board stays lit. Every power-cycle page says *both*, and a
re-enumeration that never arrives is diagnosed as "did both cables come out?" before
anything else.

## Menu

The **Legacy** sub-menu goes — it existed to hold two entries and now holds one:

```
Tools
 └─ Firmware ▸
     ├─ Update firmware…                      ← unchanged
     ├─ ────────────────
     └─ Bring up a new or legacy board…
```

The isolation rule stands: the normal update flow (`FirmwareDialog`, `UpdatePage`,
`UpdateOrchestrator` happy path) is not modified beyond the menu hosting. The `kLegacy`
personality and its status-bar text stay — a legacy Duplicator is shown by name, and the
text points at the one flow that serves it.

## The bring-up flow

Nine pages, same chassis as today (`QDialog` + `QStackedWidget`, null-controller
buildable, every page widget-testable). The order changes: **the FPGA is configured
before the firmware is loaded**, so that the moment the new firmware first runs, the
gateware it expects is already answering — there is no window in which modern firmware
polls an absent register interface, and everything from page 6 onward is the shared
update machinery.

1. **What this does, and what it will ask of you.** Case off, both cables on, a jumper
   fitted and removed, both cables pulled twice (once, for a board already in its boot
   ROM). States the end plainly: *"your Duplicator will be fully up to date when this
   finishes"* — and states that it does not matter what the board is running now, in
   those words, because that is the fact that lets a user with a mystery board proceed.
2. **Connectivity and permissions.** Two live rows, each device **opened** rather than
   enumerated so permission failures surface here and nowhere later: the FX3 row (any
   recognised personality is green — the row informs, it no longer decides anything) and
   the Blaster row (`09fb:6001`, with the charge-only-cable diagnosis leading the
   not-found text, since the board is lit from the other cable no matter what the
   mini-USB is doing). Linux failure text names the two udev rules files; Windows text
   the driver-binding page.
3. **The update file.** The same `.dddfw` the update dialog installs — the bundled copy
   preselected when the build carries one, a file picker otherwise. Signature and digest
   verification with the same key policy and the same development-channel banner as the
   update page. Bring-up additionally requires all four payloads (firmware, configure
   vectors, factory image, application gateware); a bundle from before the vectors were
   added is refused with a sentence saying it can update but not bring up.
4. **Reach the boot ROM.** *"Fit jumper J4, then unplug both USB cables and reconnect
   them"* — photograph `fx3-j4-fitted`. Polls until `04b4:00f3` enumerates; satisfied on
   arrival (and skipped) if it already has. This page is what makes the starting state
   irrelevant, and it is deliberately the only page that cares.
5. **Configure the FPGA.** The factory image is played into the FPGA over the Blaster —
   `DomesdayDuplicatorFactoryConfigure.svf`, 2.6 seconds, volatile, writes nothing. The
   FX3 is sitting in its boot ROM with every shared pin undriven, which is what makes
   this safe to do first. On a bare board the factory image stays resident with its flash
   bridge; on a board whose flash already holds a valid application image it hands over
   to it — either way something modern with a flash bridge is now answering.
6. **Program everything.** One page, one weighted progress bar, three writes — and from
   the first byte this is the ordinary update path, driven by the ordinary orchestrator
   in its deferred-restart mode:
   - RAM-load the bundle's `firmware.img` into the boot ROM (`RecoveryInstaller`, the
     bench-proved U5/U6 path) and wait for the application personality;
   - the running firmware writes its own **EEPROM** (target 0) — first, so that every
     partial state a stopped run can leave boots a safe pairing;
   - the firmware writes the **factory image** to `0x000000` (target 2, unlocked by the
     begin-flags magic);
   - the firmware writes the **application image and its boot block** to `0x200000`
     (target 1) — the same write, with the same held-back-first-page commit ordering, as
     any gateware update.
   No reset at the end: with J4 fitted a reset lands back in the boot ROM, and both
   halves are meant to change identity together at the power cycle or not at all.
7. **Remove the jumper.** Photograph `fx3-j4-removed` — same board, same framing, so the
   difference the user must make is the only difference between the pictures.
8. **Power cycle.** *"Unplug both USB cables, wait a moment, then reconnect them."* One
   cycle discharges every obligation: the FX3 re-reads its boot source, the FPGA drops
   the volatile configuration and loads from flash — factory image first, which validates
   the application image written moments ago and hands over to it. Polls for
   `1209:2347`; a timeout leads with "did both cables come out?".
9. **Verify.** What the wizard can prove, shown as ticks: personality `1209:2347`,
   `bcdDevice` matching the installed release, product-string commit matching the bundle,
   the register interface answering, the gateware reporting the **application** image.
   Then: *"Bring-up complete. The case can go back on — nothing after this point is
   physical."* Full stop; there is no hand-over button because there is nothing to hand
   over to.

A stopped or failed run is safe at every point and the failure page says so: the FX3
cannot be bricked (J4 always reaches the boot ROM), the DE0-Nano cannot be bricked (its
soldered-on USB-Blaster always reaches the EPCS), every partial state boots a safe
pairing (the EEPROM-first ordering above), and the whole flow can simply be run again
from any of them.

### The photographs

Unchanged from the previous revision, and already staged: `fx3-j4-fitted`,
`fx3-j4-removed` (pages 4 and 7) and `fpga-usb-port` (page 1 and the Blaster row's
guidance). One source serving two destinations — the docs page and the wizard's qrc —
with the qrc copies downscaled; the wizard's vocabulary is *fitted*/*removed*
throughout.

## The FPGA path — JTAG configures, USB writes

Established by B-V1 on the bench (2026-08-17) and unchanged by the downsizing; recorded
here because it is the load-bearing design fact:

> **JTAG configures; it never writes flash.** The project's own factory image is
> configured into the FPGA volatilely over the DE0-Nano's on-board Blaster, and the flash
> is then written by the firmware's own update agent over USB — the path G1
> bench-proved.

The `quartus_cpf`-derived flash-writing `.svf` the original design relied on is not
self-contained — it speaks Virtual JTAG to Altera's Serial Flash Loader, which
`quartus_pgm` supplies from its own installation and the file does not carry — and
shipping Altera's loader raises a redistribution question this project has no need to
answer. Using our own factory image instead means every artefact played or written is
ours, there is exactly one flash writer (the update agent, with its erase, program,
readback and digest discipline), and the JTAG side is 2.6 seconds of configuration
rather than minutes of vector-encoded writing.

The measured numbers: configure `.svf` 1,450,428 bytes playing in 2.6 s; factory image
216,882 bytes and application image ~212 KB as compressed EPCS bytes (the `.cof` files
enable Cyclone IV bitstream compression); G1 measured a 212 KB flash write at 17
seconds. The whole FPGA half of bring-up is under a minute.

The engine pieces are built and stay as they are: `IJtagCable`/`UsbBlasterCable` (libusb,
Qt-free, with the byte-shift-read defect B-V1 found already fixed), the SVF player (pure
parser + TAP state machine, fixture-tested, configuration only), and the build's factory
artefacts (`DomesdayDuplicatorFactoryConfigure.svf`, `DomesdayDuplicatorFactory_auto.rpd`,
both under provenance).

## The firmware target

Also unchanged by the downsizing — bring-up is now its only caller, but it was designed
for exactly this write. Recorded here because AGENTS.md §2 requires the protocol change
be stated in one place:

| | |
| --- | --- |
| `UPDATE_TARGET_EPCS_FACTORY (2u)` | base address `0x000000`, with the same erase, program, readback and digest discipline target 1 has |
| Unlocked by `UPDATE_FLAG_FACTORY_WRITE (0x57464444)` in `updateBegin_t.flags` | the reserved-flags-must-be-zero rule is the guard; the flag is refused on targets 0 and 1 and required on target 2 |
| No boot block | `updateBootBlockEncode()` describes the application image; target 2 skips the boot-block write entirely |
| Length ceiling is the boot block | the factory region ends at `UPDATE_EPCS_BOOT_BLOCK_ADDRESS`, not at the end of the device |
| Base address from the target | `updateEpcsTargetBase(target)` defaults to the application address, so a dispatch bug lands on the repairable image |

**Why the flags word and not something stronger** (settled 2026-08-17): recovery from a
half-written factory region needs JTAG, and the DE0-Nano has a USB-Blaster soldered to
it — every board this can run against carries its own recovery cable, connected by
definition during the one flow that uses the target. What is left worth defending
against is a mistaken host, not a determined one, and a magic word in a field already
required to be zero is sized for exactly that. A separate unlock request and an
`IMAGE_ROLE` gate were considered and rejected (the latter would refuse re-running
bring-up on a working board, whose factory image hands over to the application).

**No protocol version bump**, on the firmware's own rule stated where the field is
defined (`usb-descriptor.c:45`): an additive change does not bump it, and a new target is
additive — no existing field changes meaning and an old host never asks for target 2.

**And there is no compatibility case to design for.** No version of this firmware has been
released: `main` still carries the original firmware, which enumerates as `1d50:603b` and
has no update agent at all. The first release will carry all three targets from its first
day, so a device that speaks the protocol but lacks target 2 exists only as an intermediate
build inside this repository. Earlier drafts of this plan promised that such firmware would
answer `UPDATE_ERROR_TARGET` and let a host discover what it was talking to; that promise
was both unnecessary and **false** — firmware older than the flags word fails in
`updateBeginDecode()` before the target is examined and reports `UPDATE_ERROR_LENGTH`,
which the host renders as *"the device refused the update's size"* (met on the bench,
2026-08-18, from a stale development build). Nothing is designed around it: bring-up
RAM-loads the firmware out of the bundle it just verified, so it always talks to firmware
that has the target.

## One bundle

The separate provisioning set is withdrawn. **The ordinary update bundle carries
everything**, and it is the only `.dddfw` the project publishes:

| Payload | Component kind | Update path | Bring-up |
| --- | --- | --- | --- |
| `firmware.img` | `firmware` | written to the EEPROM by the running firmware | RAM-loaded first, then written to the EEPROM by itself |
| `DomesdayDuplicator_auto.rpd` | `gateware` | written to `0x200000` + boot block | same write, same code |
| `DomesdayDuplicatorFactoryConfigure.svf` | `gateware-provisioning-svf` | ignored | played over JTAG, volatile |
| `DomesdayDuplicatorFactory_auto.rpd` | `gateware-factory` | ignored | written to `0x000000` by target 2 |

About 2 MB all told — the vectors dominate. The manifest schema already makes components
optional and readers refuse unknown kinds, so this is the mechanism working as designed:
the update page installs the two components it always has and ignores the rest; the
bring-up wizard requires all four and refuses a bundle that predates the extension with a
sentence. The component kinds keep their implemented names; the `purpose` field
(update/rollback) is withdrawn along with the rollback flow it existed to mark, which
returns the manifest writer to byte-identical output against the committed signed
fixtures.

Packaging follows, all simpler than what it replaces:

- **The release workflow builds one bundle** instead of three, passing the two new
  payloads to the same `make-update-bundle.sh` invocation it already runs. The release
  gate verifies the one bundle carries all four payloads. The raw `.svf` stays attached
  separately for `ddd-jtag` and the bench.
- **The GUI bundles the update bundle** — the pin file, fetch-by-digest script, CMake
  install for the three platform layouts, and run-time search built in Phase 4 all
  survive with one retargeting rename (`bundled-update` → `bundled-update`
  throughout), and the bundled file now serves offline bring-up and offline update
  alike. The wizard still verifies the bundled copy identically to a picked one. B2 (the
  offline bench item) stays, with the stronger exit state.
- **`tools/dev-bundle.sh` loses `--kind`.** One bundle, searching out all four artefacts;
  when the Quartus-built vectors or factory image are absent locally it says so and
  builds a bundle that can update but not bring up, which is the honest local state.

## What was withdrawn, and why

Recorded with the same discipline as the decisions that survive, because "we considered
this and took it out" is the part of a plan that stops the next person re-adding it
without the history.

- **The rollback flow, whole** (2026-08-18): the wizard, its orchestrator, the
  `CheckRollbackGate`, the `purpose` manifest field, the frozen `legacy/` image set and
  its release packaging, and the R0/R1/B-V2 bench items. The implementation had grown a
  second wizard, a second orchestrator, a second bundle kind, a committed-binaries
  directory with a provenance regime, and a reversed hardware-safety ordering — all in
  service of a flow whose two justifications were thin: users who need the original
  software ecosystem still have every historical release and `quartus_pgm`, and testing
  bring-up against a legacy board is served by keeping one genuinely legacy unit on the
  bench rather than by manufacturing them. **Consequence accepted:** bench item B1 needs
  a real legacy unit, and once brought up it does not go back.
- **The separate provisioning set** (2026-08-18): a second bundle kind meant a second
  packaging path, a second release asset, a second gate, and a `--kind` axis through
  every tool, for a saving of ~1.7 MB on the update bundle. One bundle that serves both
  consumers is smaller in every dimension that costs anything.
- **The recovery-ready exit state** (2026-08-18): ending bring-up at the factory image
  and handing over to "now run one ordinary update" made the wizard shorter at the price
  of a two-step user journey and a half-way device state to document and support. The
  firmware the wizard RAM-loads is the release firmware and the bundle carries the
  application image; writing it then and there costs seventeen measured seconds.
- **Adaptive board-state handling** (2026-08-18): the eight-row personality table and its
  branches are replaced by the jumper page. The wizard reads the bus to inform and to
  skip a satisfied page, never to decide a route.
- **B-V0 as a bench gate** (2026-08-18): the pairing it measured is unreachable in the
  one flow that remains — legacy firmware never executes after the FPGA changes, because
  nothing brings it back. The source reading stays recorded (*The electrical reading*,
  above) as the justification for the boot-ROM-first rule.

## Testing

- **Never automated flashing** (AGENTS.md §4): unchanged. The SVF player's tests end at
  the vector stream against the fake cable; the wizard's widget tests run every page,
  branch and failure against fakes; nothing in CI touches an EEPROM or an EPCS.
- **Unit tier, deltas:** the manifest loses its `purpose` round-trips and the gate its
  rollback cases; the bring-up gate gains the four-payload completeness check and the
  refusal text for a pre-extension bundle. The firmware's host-tested target-2 suite is
  unchanged.
- **Widget tier, deltas:** the wizard's page-order test asserts the new safety property —
  the boot-ROM page precedes the configure page, the configure page precedes the
  firmware page, and every EEPROM write precedes every flash write. The J4-skip branch,
  permission failures, a stopped SVF play, and the power-cycle timeout diagnosis all
  stay. The rollback widget tests go with the wizard.
- **Bench tier:**
  - **B-V1 steps 4–6** — still open, still first: nothing has yet written a byte to an
    EPCS by the target-2 route. The flash write, its duration, and the comparison
    against a `quartus_pgm`-provisioned board close it.
  - **B0** — bring-up of a bare pair to the verified *fully current* state, then the T5
    capture integrity run. The exit criterion strengthens with the exit state: the
    wizard's last page shows the application image running, and no separate update is
    performed between bring-up and capture.
  - **B1** — bring-up of a genuinely legacy unit (`1d50:603b`), same exit. Needs the
    bench's legacy unit, and consumes it — see *What was withdrawn*.
  - **B2** — the offline bring-up: a release build on a machine with no network arrives
    at the image page with the bundled update file already chosen and named.

## Documentation deliverables

Corrected in the same change as the implementation, because all of them currently
describe the withdrawn shape:

- **Delete** *Rolling back to legacy firmware* and its `.nav.yml` entry.
- **Rewrite** *Bringing up a new or legacy board*: the new page order, the one bundle in
  place of "the provisioning set", the state-agnostic framing ("it does not matter what
  the board is running"), and the new ending (a finished unit, not a hand-over to the
  update dialog).
- **Correct** *The main window* (menu structure), *Update bundle format* (four
  components, no `purpose`), *Device update mechanism* (target 2's caller is bring-up
  alone; rollback references go), and TESTING.md (R0/R1/B-V2/B-V0 removed, B0/B1/B2
  retargeted, the plan link renamed).

## Phases

The previous revision's phases 1–6 are implemented and green: recognition and menu; the
JTAG cable and SVF player; the bring-up wizard; the bundled set; the legacy set and
rollback wizard; and the own-factory-image rewrite with the target-2 firmware. This
revision is one further phase — mostly subtraction, one reordering — landing as a single
change that leaves the tree shippable:

**Phase 7 — Downsize.**

Remove:

- `ddd-gui/src/gui/legacy_rollback_wizard.{h,cpp}`, `rollback_text.{h,cpp}`,
  `rollback_worker.{h,cpp}`; `ddd-gui/src/capture/rollback_orchestrator.{h,cpp}`; their
  tests (`test_legacy_rollback_wizard.cpp`, `test_rollback_orchestrator.cpp`); the
  rollback entry, `ShowRollbackWizard()` and the wizard pointer in `main_window.*`; the
  **Legacy** sub-menu level (bring-up moves up one).
- `CheckRollbackGate()` and the update path's rollback-bundle refusal; `UpdatePurpose`
  and the `purpose` field from `update_manifest.*` and `update_bundle.*`; `--purpose`
  from `make-update-bundle.sh`; `--kind` from `dev-bundle.sh`.
- The `legacy/` directory, the release workflow's rollback packaging and its legacy digest
  verification, and the workflow's separate provisioning set. The frozen images were
  committed at `3446894` before this revision was written, so they are removed from the
  tree rather than never added to it — recoverable from that commit if the decision is ever
  revisited, which is the whole reason *What was withdrawn* records why it was taken.
- The docs rollback page and nav entry.

Change:

- The bring-up wizard and its worker: page order (boot ROM → configure → program
  everything), the EEPROM-before-flash write ordering, the four-payload bundle check,
  wording, the verify page's application-image expectation, and the order test.
- The bring-up sequencing in the engine (`bringup_orchestrator.*`, renamed to match
  its one caller if that reads better): configure over JTAG, release the cable, RAM-load,
  then one deferred-restart install of targets 0, 2, 1 in that order through the ordinary
  orchestrator.
- The release workflow: one bundle carrying four payloads; the gate checks it.
- The bundled-file machinery: `bundled-update` → `bundled-update` renames across
  the pin file, fetch script, CMake variable and `src/gui` locator.
- The documentation set listed above.

Keep untouched: the firmware (target 2 as shipped), the gateware build and its factory
artefacts, the JTAG cable and SVF player, `RecoveryInstaller`, the deferred-restart
orchestrator option, the `kLegacy` personality and status text, and the photographs.

*Exit:* the full test suite green with the rollback suites gone; a dev bundle built by
`tools/dev-bundle.sh` carries four payloads and installs normally through the update
dialog; the wizard's widget tests assert the new order; the docs build with the rollback
page gone. Bench items B-V1 (steps 4–6), B0, B1 and B2 then close on hardware in that
order.

## Risks and open questions

- **B-V1's remaining steps are still the gating proof.** Everything about target 2 is
  implemented and host-tested, and nothing has written a byte to an EPCS through it. The
  first bench run of the new page 6 is the moment this plan touches reality, and it is
  deliberately the first bench item in the exit list.
- **The factory-region freeze stays deliberately broken**, with the same recorded
  reasoning: every unit carries its recovery cable on the DE0-Nano itself, and the
  begin-flags magic is sized for the mistaken-host risk that remains. If the design ever
  moves to a carrier without an on-board Blaster, this is the first decision to revisit.
- **Legacy units become finite.** With rollback withdrawn, every B1 run converts a legacy
  unit into a current one. The bench keeps at least one never-updated unit aside for
  regression work on the bring-up flow, and that is a bench-inventory rule rather than a
  code one.
- **The partial power cycle** remains the likeliest way for a user to get stuck — either
  cable alone keeps the unit powered. Mitigated as before: the wording says *both*, the
  timeout diagnosis asks about it first, and the flow asks for at most two cycles.
- **Windows driver surface** is unchanged from the previous revision: `09fb:6001` needs
  WinUSB binding like `04b4:00f3`, and a machine with Quartus installed has `jtagd`
  contending for the cable. The connectivity page distinguishes the two; the docs carry
  both remedies.
