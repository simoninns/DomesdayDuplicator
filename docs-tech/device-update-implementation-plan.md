# In-Application Device Updates — Implementation Plan

## Purpose and adopted decisions

This is the implementation plan for in-application firmware and gateware updates,
building on the investigation in [device-update-plan.md](device-update-plan.md). The
following decisions are now **adopted**, not open:

- **F1** — the FX3 application firmware is its own flasher: it brings up the I2C block
  and rewrites the boot EEPROM in place, commanded over EP0 by `ddd-gui`. No jumper, no
  personality change for a normal update.
- **G1** — EPCS access goes through the FPGA fabric: a small, explicitly-unlocked SPI
  bridge (`asmiblock`) plus a reconfiguration trigger (`rublock`). The FX3 firmware
  speaks the EPCS command set through the bridge; the gateware stays dumb.

Three requirements are added on top of the investigation document:

1. **The build environment produces the update artefact.** CI/CD assembles a **single
   release bundle file** containing both the FX3 firmware and the FPGA application
   gateware, with enough metadata to install and verify both.
2. **The GUI can fetch the bundle itself** from the project's GitHub releases (as well
   as open a locally downloaded file), and installation + verification is automatic
   after one deliberate user confirmation.
3. **The factory gateware is a separate, deliberately static component.** The FPGA
   needs a resident "boot loader" image that is never field-updated; it must be cleanly
   separated in the repository from the flashable application gateware so maintainers
   can see at a glance which is which, and so a change to the static half is loud.

Where this document and the investigation document disagree, this one wins. The one
substantive change of direction: the investigation sketched the factory image as a full
capture gateware; requirement 3 replaces that with a **minimal boot/rescue factory
image** (rationale under *Factory / application split*).

## End state (the user story this plan builds)

A user runs `ddd-gui` with their Duplicator connected. **Help → Firmware… → Check for
updates** (or an opt-in automatic check at startup) finds the latest release bundle on
GitHub, shows *installed vs available* versions for firmware and gateware, and offers
**Update**. One confirmation later: the GUI streams the bundle to the device, the FX3
rewrites its EEPROM and the EPCS application image, everything is CRC-verified on the
device, the device resets, re-enumerates, and the GUI confirms the new commit
identities match the bundle manifest. Total time: a few minutes, one cable, no tools.
If anything is interrupted, the device comes back in a rescue state (USB bootloader for
the FX3, factory gateware for the FPGA) that the GUI recognises and can repair.

## Factory / application split (requirement 3)

### Why the factory image is minimal

A factory image that contained the full capture gateware would change every time the
capture logic changed — the opposite of static. Instead the factory image is a **boot
loader in the honest sense**: the smallest gateware that can (a) identify itself,
(b) give the FX3 access to the EPCS through the bridge, and (c) decide whether to jump
to the application image. It does not capture. If a unit falls back to factory, the GUI
shows a clear "recovery gateware running — reinstall gateware" state with a one-click
repair, which is a better failure mode than silently running stale capture logic.

Frozen-ness is enforced by consequence and by layout: the factory image is written
**only by JTAG at provisioning time**; changing it means re-provisioning every fielded
unit with a cable, so after its first release it is effectively immutable. The
repository layout makes that boundary physical:

```
fpga/
  factory/          ← the static boot-loader image. Own Quartus project, own README
                      stating the freeze policy and that changes require bench JTAG
                      re-provisioning of every unit. Compiled in remote-update
                      "factory" mode. Expected to change ~never after v1.
  application/      ← the flashable capture gateware (today's fpga/src moves here).
                      Own Quartus project, compiled in remote-update "application"
                      mode. Changes freely; this is what the bundle carries.
  common/           ← Verilog shared by both images: spiRegisters core, flashBridge,
                      asmiblock/rublock wrappers and their simulation stubs, version
                      generation. A change here rebuilds both images — CI flags that
                      a factory rebuild implies re-provisioning, so common/ changes
                      get the same scrutiny as factory/ changes.
```

Both projects keep the existing build discipline (out-of-tree build dirs, committed
plain-Verilog IP, provenance tooling, reproducible outputs). `fpga/README.md` gains a
section explaining the two-image model; `fpga/factory/README.md` opens with the freeze
policy in bold.

### What each image contains

| | factory | application |
| --- | --- | --- |
| PLL, safe idle drive of GPIF pins (`USB_PCLK` running, `dataAvailable` low) | yes | yes (full GPIF) |
| `spiRegisters` (identity, commit, map version) | yes | yes |
| `IMAGE_ROLE` register (`0x0B`: `0x00` factory / `0x01` application) | yes | yes |
| `flashBridge` (unlock + EPCS pass-through via `asmiblock`) | yes | yes |
| `rublock` control (trigger reconfig; watchdog) | boot logic + trigger | tickle + trigger |
| Capture path (ADC, FIFO, GPIF state machine, test generator) | **no** | yes |

The application image carries the bridge too, so routine gateware updates are done from
the running application image; the factory image is only exercised when things go wrong
(and once per update cycle at power-on, see boot flow).

### EPCS64 layout and boot flow

```
0x000000  factory image          (JTAG-provisioned once; never field-written)
0x100000  boot block             (one 64 KiB sector: magic, app start address,
                                  app image length + CRC32, layout version)
0x200000  application image      (field-written by the update flow)
0x400000… free                   (future: second application slot for A/B)
```

Power-on: the FPGA loads the factory image from `0x0`. Factory boot logic reads the
boot block through `asmiblock`; if the magic and CRC are valid it arms the remote-update
watchdog and triggers reconfiguration to the application address. The application image
tickles the watchdog **only after its SPI register interface has decoded a first valid
transaction** — and the FX3 firmware performs an identity read during its own
initialisation, so on a healthy device the tickle happens within a second of power-up,
host or no host. A wedged application image (configures, but its fabric/SPI is dead)
therefore times out and the device reverts to factory. A corrupt application image
fails configuration CRC and reverts to factory without the watchdog's help.

Two things the bench added to this account. The factory image must **relock the flash
bridge** before it hands over, or the fabric is still driving the pins the configuration
engine needs, and it must write the `Osc_int` and `Cd_early` option bits the handbook
requires of a factory configuration. And "reverts to factory" is not the same as "parks in
factory": the factory image makes the same decision again from the same flash, so an image
that configures and is dead cycles at about three seconds a lap rather than stopping. The
deliberate refusal to make a second attempt is outstanding pre-freeze work.

The update flow writes the application image first, verifies it by readback CRC, and
writes the boot block **last**; an interrupted gateware update leaves an invalid boot
block and the unit simply stays in factory. Rollback is "erase the boot block sector".

## The release bundle (requirements 1 and 2)

One file, attached to each GitHub release:

```
domesday-duplicator-update-<version>.dddfw     (uncompressed ustar archive)
  manifest.json      — always the first entry
  manifest.minisig   — detached Ed25519 (minisign) signature over manifest.json
  firmware.img       — FX3 image (fx3-mkimage output, boot-source-agnostic)
  gateware-app.rpd   — raw EPCS byte stream of the application image only
```

Signing the *manifest* (which carries the SHA-256 of every payload) rather than the
archive keeps the bundle a single self-verifying file: the offline file-picker path and
the online path verify identically, and nothing has to sign a file that would contain
its own signature.

`manifest.json` (versioned schema, `"manifest_version": 1`):

- release version and git commit;
- per-component entries: filename, byte length, SHA-256 (the one digest used at every
  later link of the chain, including on-device verification), commit identity as the
  device will report it after install (product-string commit for the FX3, register
  commit for the gateware);
- compatibility: minimum register-map version, EPCS layout version, minimum GUI
  version;
- human-readable release notes line.

Design points:

- **ustar, uncompressed**: inspectable with stock `tar`, parseable by ~100 lines of
  pure C++ in the Qt-free engine (unit-tested, no dependency). Payloads are already
  small/compressed.
- The gateware payload is **application-image-only**. The factory image and the
  combined provisioning `.jic` are release artefacts too (for bench provisioning), but
  they are not in the bundle — the bundle contains only what the device may write to
  itself.
- Integrity and authenticity are first-class, not later hardening: every hand-off from
  source to flashed device is covered by SHA-256 (with one justified CRC32 exception in
  factory fabric), and the bundle carries a detached **minisign (Ed25519) signature**
  verified against a public key pinned in the GUI source. The full chain is laid out in
  *The integrity chain* below.
- Components are individually optional in the schema (a firmware-only bundle is legal),
  which lets early phases ship real bundles before the gateware path exists.

### Build pipeline

**Policy decision adopted with this plan: Quartus Prime Lite runs in CI.** The
repository's standing rule that the bitstream stays out of CI is amended: it stays out
of the *per-commit* `nix flake check` tier (so contributors without an unfree-enabled
Nix configuration are untouched), but dedicated workflows build it. Every release
artefact — firmware and both bitstream images — is then built hermetically by CI from
the tagged commit, with no maintainer-built binary anywhere in the chain. This is an
amendment to statements in [README.md](../README.md), [fpga/README.md](../fpga/README.md)
and [AGENTS.md](../AGENTS.md), and updating those documents is an explicit Phase 6
task.

- **Per-commit CI** (existing `nix flake check` tier, unchanged in cost): builds
  `fx3-firmware`, runs gateware lint/simulation with the free tools for **both**
  images, runs all bundle tooling self-tests, and assembles a firmware-only
  *development bundle* to prove the tooling end-to-end.
- **Bitstream workflow** (runs on gateware changes, on tags, and on manual dispatch):
  `nix build .#bitstream` on a GitHub-hosted runner with unfree allowed for that
  derivation. The mechanics that make this viable, encoded in the workflow rather
  than folklore:
  - the Quartus closure (device support restricted to Cyclone IV, which keeps it to a
    few GB) is **cached** — GitHub Actions cache or a **project-private** binary
    cache; on a cold miss, Nix fetches the official installer from Intel's CDN,
    pinned by hash in nixpkgs, so a miss is slow but never wrong;
  - a disk-preparation step (the stock runner needs its unused toolchains cleared to
    fit the Quartus closure plus the build);
  - licence position stated in the workflow header: Prime Lite needs no licence
    file, installing from Intel's own installer in CI is ordinary use, and the
    closure cache must remain private because *redistributing* Quartus is not
    permitted — which is also why third-party Docker images of Quartus (the MiSTer
    community's route) are rejected here.
  On non-tag runs the workflow uploads the artefacts + provenance as workflow
  artifacts and records digests, giving every gateware change a CI-built bitstream.
- **Release workflow** (tag-triggered): everything comes from the tagged commit and
  nowhere else — firmware and both bitstream images built hermetically, the
  provenance file produced by CI in the same run, `…​.dddfw` assembled with
  `tools/make-update-bundle.sh`, the manifest signed, and bundle + provisioning
  `.jic` + provenance attached to the release. The tag alone pins the entire
  release, and no human hand touches any byte between source and published bundle.
- **Reproducibility audit** (scheduled and on dispatch): rebuilds the most recent
  release tag from scratch and compares digests against the published provenance.
  The bitstreams are byte-identical across rebuilds; this job turns that property
  from a claim into a monitored invariant, and its failure is loud.

Local builds (`./fpga/build-local.sh`, `nix build .#bitstream`) remain first-class
for development and bench work — Nix-never-required still holds for *developing*; it
is only the *release* artefacts that are defined as CI-built. Fallback if CI Quartus
proves unsustainable (cache eviction economics, runner disk, Intel CDN rot): the
previous revision of this plan specified maintainer-built bitstreams committed to a
tracked `fpga/prebuilt/` directory with a digest-and-source-tree-hash gate in the
release workflow — auditable because of the same byte-identical reproducibility. That
design is preserved in git history and drops back in without reshaping the bundle,
the signing, or the integrity chain.

### GUI fetch and install

- New engine-side (Qt-free) pieces: bundle reader + manifest validation; update
  orchestrator state machine driving the `0xD0–0xD5` protocol through the updater
  seam; all fakeable and unit-tested.
- New Qt-side pieces: the update page of the firmware dialog; a fetcher using
  `QNetworkAccessManager` against the GitHub releases API
  (`…/repos/simoninns/DomesdayDuplicator/releases/latest`), selecting the `.dddfw`
  asset. Network access is **opt-in** (a "check for updates automatically" setting,
  off by default, plus an always-available manual check) — the app makes no network
  request the user hasn't asked for. Downloads land in the user cache dir; the manifest
  signature and every per-file SHA-256 are verified before the Update button enables.
  A file-picker path remains for offline use; both paths run the identical
  verification, because it all lives inside the bundle.
- Install is automatic after one confirmation: firmware target, then gateware target,
  then `0xD5` (FPGA reconfig) + `0xD4` (FX3 reset), then re-enumeration and identity
  check against the manifest. `DeviceMonitor` is suspended during programming and its
  change-reporting drives the "device came back" step. Failure at any point leaves a
  clear state: FX3 rescue = USB bootloader personality (GUI recognises and repairs),
  gateware rescue = factory image (GUI recognises `IMAGE_ROLE` and offers reinstall).
- Flatpak/MSI/DMG packaging gains whatever network permission the fetch needs
  (Flatpak: `--share=network` is already required for nothing else — verify in
  packaging phase).
- **The application itself is part of the check, not the install.** The same
  releases-API query that finds a device bundle also notices a newer `ddd-gui`
  release; the update page then shows all three versions (application, firmware,
  gateware) and, for the application, routes to the platform's own channel rather
  than self-updating — Flathub for the Flatpak (a sandboxed app cannot replace
  itself, and should not try), the release MSI/DMG downloads for Windows/macOS.
  The manifest's `minimum GUI version` is enforced, not advisory: a bundle requiring
  a newer application disables the device-update button and says "update the
  application first", so the ordering users must follow is the only ordering the UI
  permits.

### Update experience (what the user sees)

The update flow is written for the archivist, not the developer: at every moment the
screen answers *what is happening, how long it will take, and what I should (not) do*.
This is a requirement on the GUI phases, not a polish item for the end.

- **A staged, wizard-like flow** with plain-language stages, each with its own
  progress and a one-line description of what the device is doing:
  1. *Checking* — versions found, what will change (application / firmware /
     gateware), release notes line shown;
  2. *Downloading* — bytes progress, verified tick when the signature and digests
     pass ("update verified as authentic");
  3. *Updating firmware* / 4. *Updating gateware* — per-stage progress bars fed by
     `0xD0` (transfer, write, verify shown distinctly), a realistic time estimate
     up front ("about 4 minutes"), and the one instruction that matters, stated
     before the first byte moves and shown throughout: **"Leave the device plugged
     in and powered."**;
  5. *Restarting device* — "the Duplicator will disconnect and reconnect by itself;
     this is normal";
  6. *Confirming* — "your device now reports firmware X / gateware Y — update
     complete."
- **Progress is honest**: stage-level bars from real device state, never a fake
  spinner over a minutes-long silence; if a stage's duration is unknowable the UI
  says what it is waiting for instead of guessing.
- **Errors speak user, not errno**: every failure state names what happened, whether
  the device is safe (it always is — say so), and the exact next step, which is
  usually a single button ("Try again", "Repair firmware", "Reinstall gateware").
  The rescue states are described in the same calm terms the *If an update fails*
  documentation page uses — the dialog and the docs tell one story.
- **Interruption-proofing in the UI as well as the protocol**: the window prevents
  capture, sleep and accidental close while programming, and if the user tries to
  quit it explains why not now (and when it will be safe).
- Every state of this flow is drivable through the `IDeviceUpdater` fake, so the
  complete wizard — including every error and rescue branch — is exercised in
  widget tests with no hardware attached.

### Compatibility gates (no way to flash past what the GUI can use)

Version compatibility is enforced machine-to-machine at two moments, in both
directions — never inferred from commit strings, which identify builds but order
nothing:

- **Machine-readable capability versions on the device.** The gateware already
  advertises its register-map version (register `0x01`); the FX3 firmware starts
  advertising a **protocol version in `bcdDevice`** (currently a dead `0x0000` in the
  descriptor), readable by the GUI without any vendor command. Both are integers with
  defined bump rules — additive changes don't bump, breaking changes do — and the GUI
  is built knowing the *range* of each it supports, not a single expected value.
- **Install-time gate (the one this section exists for):** before any byte is
  streamed, the running GUI checks the bundle manifest's `minimum GUI version` and
  the bundle components' declared protocol/map versions against its own supported
  ranges. A bundle whose firmware or gateware requires a newer application than the
  one performing the update **disables the install with "update the application
  first"** — a user cannot use the current GUI to flash the device beyond that same
  GUI's understanding. The GUI likewise refuses manifests whose schema version it
  does not know.
- **Connect-time gate (the second-order case):** a device can meet an old GUI having
  been updated elsewhere by a newer one. On every connect the GUI compares the
  device's advertised versions against its ranges: device *newer* than the GUI
  understands → a clear "this firmware requires a newer application" state with
  capture disabled and the application-update routing offered (a wrong-protocol
  capture must not limp along); device *older* → the existing
  mismatch warning, now with the device-update offer attached. The current
  commit-prefix `FirmwareVersionCheck` remains as a freshness hint only; it no longer
  carries compatibility weight.
- **Downgrades** are permitted deliberately (rollback is a feature, and the archive
  of release bundles on GitHub is the rollback source) but pass through the same two
  gates, so a downgrade below the running GUI's minimum is refused the same way an
  overreaching upgrade is.

### Developer loop (no release required)

Releases are CI-built, but a developer iterating on firmware or gateware must be able
to build, package and load onto bench hardware in one short local step — never by
cutting a release, and never by a path so different from the real one that it tests
nothing. Three tiers, fastest first:

- **RAM load (seconds, volatile)** — `fx3-programmer -u` into the ROM bootloader (J4
  fitted) remains the fastest firmware-iteration path and is untouched by this plan.
  It bypasses the update mechanism entirely; use it when iterating on firmware logic,
  not when testing updates.
- **Development bundle (the real path, local)** — one command, `tools/dev-bundle.sh`
  (works natively and under `nix develop`, per the repository's two-ways rule): builds
  what is available locally — firmware via the normal build, gateware images if a
  local Quartus build output exists — and assembles a bundle signed with the
  **in-tree development keypair**, marking `"channel": "development"` in the
  manifest. Firmware-only bundles are legal by schema, so firmware developers never
  need Quartus for this loop. Because the dev private key is committed, a dev
  signature proves format validity only, never authenticity — which is exactly why
  release GUI builds refuse it (below).
- **Headless install** — the updater engine is Qt-free, so a thin CLI target
  (`ddd-update <bundle.dddfw>`) drives the identical engine code path as the GUI:
  verify, program, reset, re-read identity, exit non-zero on any failure. This is the
  scriptable half of the bench procedures in TESTING.md and makes
  `dev-bundle.sh && ddd-update` the whole edit-to-running-device loop.

GUI/CLI acceptance rules: a release build pins the release public key and accepts
nothing else. Accepting the development key requires an explicit, per-invocation
opt-in (`--dev-update-key`, or a debug build which implies it), and any
development-signed bundle is bannered prominently as such in the update UI. There is
no unsigned path at all — the development key *is* the unsigned-equivalent, made
explicit and impossible to confuse with a release.

## The integrity chain

The rule: **one digest, SHA-256, computed once at build time, checked at every
hand-off, all the way to the flash readback** — no link trusts the previous link's
verification. Authenticity (nothing *replaced*) comes from the pinned-key manifest
signature; integrity (nothing *corrupted*) from re-checking the same SHA-256 at each
link. There is exactly one place a weaker check is used, and it is justified where it
appears.

| # | Hand-off | Threat | Check |
| --- | --- | --- | --- |
| 1 | Source → binaries | Tampered or irreproducible build | Every payload — firmware and both bitstream images — is built hermetically by CI (Nix) from the tagged commit; no maintainer-built binary enters the chain; SHA-256 digests land in the CI-produced provenance file published with the release |
| 2 | Build environment → binaries over time | Environment drift, silent non-reproducibility | The bitstreams are byte-identical across rebuilds; the scheduled reproducibility audit rebuilds the latest release tag and compares digests against the published provenance, failing loudly on drift |
| 3 | Bundle assembly | Wrong payload bundled | Workflow computes each payload's SHA-256 into `manifest.json`, cross-checks against links 1–2, signs the manifest with the release minisign key |
| 4 | GitHub → GUI (or hand-download → file picker) | Replaced asset, corrupted download | TLS in transit; then, identically for both paths: minisign verification of `manifest.json` against the public key pinned in the GUI source, then per-file SHA-256 of every payload. No Update button until both pass |
| 5 | GUI → FX3 (EP0 stream) | Corruption in transit or in host memory | `UPDATE_BEGIN` carries the payload length and SHA-256; the firmware hashes the chunk stream as it arrives and aborts before commit on mismatch |
| 6 | FX3 → EEPROM / EPCS | Write errors, power loss, flash defects | Full readback of the written region; SHA-256 recomputed **from the medium** and compared to the `UPDATE_BEGIN` digest; only on match is the commit record (FX3 signature page / FPGA boot block) written |
| 7 | Every subsequent boot | Flash corruption in the field | FX3: boot-ROM image checksum + `'CY'` signature (falls back to USB bootloader). FPGA: configuration-CRC fallback to factory, plus the factory boot logic checking the boot block's **CRC32** — the one deliberate non-SHA link, because it runs in factory fabric where a SHA-256 core is unjustifiable, and it defends against *corruption only*: authenticity was settled at links 4–6, before the boot block was ever written |
| 8 | After update | Wrong image actually running | Commit identities read back from the live device (product string, gateware registers) compared to the manifest's expected identities |

Key management for link 3–4: the minisign secret key lives as a GitHub Actions secret;
the public key is committed to the repository and compiled into the GUI. Rotation is a
GUI release that pins the new key (old bundles verify against the old GUI, so the
release notes must say so). The fallback model, if holding the key in CI is ever
uncomfortable, is that the maintainer signs the CI-assembled manifest locally and
uploads the signature — nothing else in the chain changes shape.

## Protocol recap (updated from the investigation document)

Vendor requests `0xD0` STATUS / `0xD1` BEGIN / `0xD2` DATA / `0xD3` FINISH / `0xD4`
RESET / `0xD5` FPGA_RECONFIG, with `wIndex` selecting target 0 (FX3 EEPROM) or 1 (FPGA
EPCS); register map v2 adds the bridge block (`0x20–0x23`) and `IMAGE_ROLE` (`0x0B`).
One change from the investigation document, following from the integrity chain: the
`0xD1` BEGIN data stage carries the payload **length + SHA-256** (not CRC32), the
firmware hashes both the incoming stream (link 5) and the post-write readback (link 6)
with SHA-256, and `0xD0` STATUS reports which check failed. CRC32 survives only inside
the FPGA boot block (link 7). Commit-ordering safety is unchanged: FX3 image signature
page written last; FPGA boot block written last. Full protocol details in
[device-update-plan.md](device-update-plan.md).

## Documentation (mkdocs site)

The documentation site (`docs/`) is a deliverable of this plan, for both audiences,
and each page lands **in the phase that builds the thing it documents** — never as a
catch-up at the end. The site builds in CI (`nix build .#docs-site`), so a phase whose
feature merges without its page is visibly incomplete. Final page set:

**User section** (`docs/content/capture-application/` — written for someone who has
never opened a terminal):

- *Updating your Domesday Duplicator* — checking for updates, what the version
  comparison shows (application, firmware and gateware — and that the application
  updates through Flathub or the release installers, not from inside the app), the
  one-confirmation device install, what each progress stage means and roughly how
  long it takes, and the post-update "your device now reports…" confirmation. (Lands
  with Phase 7, when the online flow completes; a file-picker interim version lands
  with Phase 2.)
- *If an update fails* — what "recovery mode" (FX3 bootloader) and "recovery gateware
  running" (factory image) look like in the GUI, the one-click repairs, and the
  reassurance that an interrupted update cannot brick the unit. Ends with a pointer
  to the bench appendix for the cases the GUI cannot fix. (Lands with Phases 3
  and 5.)

**Development section** (`docs/content/development/`):

- *Device update mechanism* — the architecture: one on-device update agent, the
  `0xD0–0xD5` protocol, register map v2, targets, commit ordering, and the integrity
  chain end-to-end. (Phase 1, normative; updated as hardware verification refines
  it.)
- *Update bundle format* — manifest schema, signing, channels, and how to inspect a
  bundle with stock `tar`. (Phase 1.)
- *EPCS layout and boot flow* — factory/application split, the freeze policy and its
  rationale, boot block, watchdog arming, fallback behaviour. (Phase 4.)
- *Developer update loop* — the three tiers (RAM load / `dev-bundle.sh` /
  `ddd-update`), the development key and its acceptance rules. (Phases 1–2.)
- *Release pipeline* — the three workflows, Quartus-in-CI mechanics and licence
  position, key custody and rotation, the reproducibility audit, and "how to cut a
  release" as a checklist. (Phase 6.)
- The existing *hardware-programming* pages (`fx3-firmware.md`, `fpga-bitstream.md`)
  are reframed in Phase 7 as a **provisioning and bench-recovery appendix**: first
  provisioning of a new build (JTAG `.jic`, EEPROM via `fx3-programmer`), the J4
  jumper path, and Windows driver binding for the recovery personality — explicitly
  introduced as "what the update mechanism replaces for everyday use".

TESTING.md (repo, not the site) gains the manual update/interruption/recovery bench
procedures alongside T5, phase by phase as they are first performed.

## Phases

Each phase lands independently, leaves the tree shippable, and states its exit
criterion. Protocol-touching phases change fpga + fx3 + ddd-gui + docs together
(AGENTS.md §2). Nothing automated ever writes EEPROM or EPCS (AGENTS.md §4): every
hardware exit criterion below is a deliberate manual bench procedure, documented as it
is performed.

### Phase 1 — Specifications and bundle tooling

- Write the normative docs: update protocol page and register map v2 in
  `docs/content/development/`; EPCS layout and boot-block format; bundle/manifest
  schema; the factory freeze policy; the compatibility model (protocol version in
  `bcdDevice`, map version register, bump rules, GUI supported ranges, the
  install-time and connect-time gates).
- Implement `tools/make-update-bundle.sh` + a pure manifest/ustar library in
  `ddd-gui/src/capture/` with unit tests (reader and writer round-trip).
- Vendor the crypto once, small and shared: a public-domain SHA-256 (used by the
  engine, the bundle tool and — Phase 2 — the FX3 firmware) and an Ed25519/minisign
  *verifier* for the engine (monocypher-class, licence-header policy respected).
  `make-update-bundle.sh` signs with a development key in-tree; unit tests cover
  sign/verify round-trip, a tampered payload, and a tampered manifest.
- Implement `tools/dev-bundle.sh` (the developer loop's packaging step: build what is
  present locally, assemble, dev-sign, `"channel": "development"`), working both
  natively and under `nix develop`.
- Per-commit CI assembles a firmware-only development bundle.
- **Exit:** schema and protocol documents merged; `nix flake check` builds and
  validates a development bundle; a developer can produce one locally with a single
  command.

### Phase 2 — FX3 self-update (target 0)

- Firmware: I2C block bring-up in the IO matrix; `0xD0–0xD4` handlers; streaming
  EEPROM page writes with the 64 KiB slave-address roll (sharing/mirroring
  `fx3-paging.h` and its tests); held-back first page; the vendored SHA-256 hashing
  both the incoming stream and the post-write readback (integrity links 5–6);
  capture/update mutual exclusion; identity read during init (needed later for the
  watchdog tickle, harmless now); `bcdDevice` starts carrying the protocol version.
- GUI: `IDeviceUpdater` seam + fake; update page in the firmware dialog with
  file-picker bundles only; worker-thread orchestrator with progress from `0xD0`;
  post-reset identity confirmation; the release/development key acceptance rules
  (dev key by explicit opt-in only, bannered in the UI). The staged wizard of
  *Update experience* ships here in its firmware-only form — stages, honest
  progress, "leave the device plugged in", plain-language errors, widget tests over
  the fake for every branch — it is not deferred to a polish phase.
- `ddd-update` CLI over the same engine path (verify → program → reset → identity
  check, non-zero exit on failure) — completing the developer loop
  (`dev-bundle.sh && ddd-update`) and giving the bench procedures their scriptable
  form.
- **Exit (bench):** firmware updated GUI-to-device from a bundle file, and
  headlessly via `ddd-update`; deliberate mid-update power pull leaves the device
  falling back to the USB bootloader (this is verification item V1) and a subsequent
  J4-free retry succeeds after Phase 3; D25 closed.

### Phase 3 — Recovery personality, and first-time FX3 programming

- GUI recognises `04b4:00f3` (and the transient flash-programmer identity by its
  `0xB0` probe); `DeviceInfo` gains a personality field; the three VID/PID match sites
  are updated; "recovery mode" presentation in the device list.
- Engine port of `fx3-programmer`'s RAM-load + flash-programmer sequence (GPLv3, pure
  libusb) behind the updater seam; Windows driver-binding documentation for the
  recovery personality.
- **The same code path provisions a new board, and the interface says so.** A blank
  EEPROM and an EEPROM corrupted by an interrupted update are indistinguishable on the
  wire — the boot ROM rejects both and the kit enumerates as `04b4:00f3` either way — so
  a device that has never been programmed is already reachable by everything above. What
  is added is the wording: an unprogrammed unit is offered *"program this device"*, not
  *"repair"*, because a user holding a kit they have just soldered has not broken
  anything. This is the FX3 half of Phase 8.
- **Exit (bench):** a unit with a deliberately invalidated EEPROM is restored to
  working firmware entirely from the GUI; and a kit that has never been programmed is
  brought to working firmware the same way.

### Phase 4 — Gateware restructure: factory / application / common

- Mechanical move of `fpga/src/` → `fpga/application/` + extraction of `fpga/common/`;
  build scripts, provenance, lint and simulation follow. No functional change;
  application `.jic` remains byte-comparable to before the move.
- New `fpga/factory/` project: PLL, safe GPIF idle, `spiRegisters` + `IMAGE_ROLE`,
  `flashBridge`, boot-block reader and rublock boot logic, watchdog arming. Both
  images build in their respective remote-update modes; combined provisioning `.jic`
  produced; free-tool simulation covers the boot decision (valid block → jump;
  invalid → stay) with asmiblock/rublock stubs.
- Application image gains `flashBridge`, `IMAGE_ROLE = 0x01`, and the
  first-valid-SPI-transaction watchdog tickle.
- Bench: JTAG-provision a unit with the dual-image `.jic` — the last time a cable is
  required. Verify factory→application boot, watchdog fallback with a deliberately
  wedged application image, and the FX3 "FPGA ready" timing assumption (items V4, V5).
- **Exit: met for the boot path, 2026-08-15.** A unit was provisioned and cold-boots
  factory→application with no host attached (TESTING.md §6, G0); V4 is confirmed twice
  over, from the block's own read-back and from an analyser watching the configuration
  engine read at `0x200000`. Still outstanding: **V5**, the double-configuration timing
  against the FX3's readiness assumption, and the other half of the exit criterion —
  corrupting the boot block by JTAG and confirming the GUI's recovery state — which is
  grouped with the Phase 5 interruption tests below because it is the same sitting.

### Phase 5 — Gateware update path (target 1)

**The clean path is proved on hardware (2026-08-15); the interruption half is outstanding.**

- FX3: EPCS driver through the bridge (`epcs-flash.c`: silicon-ID sanity check, sector
  erase, page program, read, reconfiguration trigger) behind `0xD1–0xD3` target 1 and
  `0xD5`; SHA-256 readback verify from the EPCS (integrity link 6) and the boot block's
  CRC-32 accumulated over the same pass; the boot block written, read back and compared
  last. Sectors are erased as the write first enters them, so an update abandoned before
  its first chunk leaves the previous gateware running. Register map v2 is now what the
  firmware is built against, and the bridge registers are not host-writable — the
  firmware owns them.
- GUI: second target throughout — per-component stage titles, the multi-minute estimate
  derived from the bridge's cost per byte, a transfer message that explains the erase
  pauses, an install-time gate that refuses a gateware update to a device whose gateware
  has no flash bridge, and the factory-image repair flow: *recovery gateware running*
  named in the dialog, the gateware row reading **Recovery gateware**, and a single
  **Reinstall gateware** button that runs an ordinary update.
- Chunk alignment is now 256 bytes on the host, which satisfies both media for any
  advertised chunk size.
- **Done on the bench, 2026-08-15** (TESTING.md §6, G1): a full gateware update from the
  running application image, ending in the handover, with the identity verified after the
  reconfiguration and a capture run over the result. **V6**: 17 s to send a 212 KB
  compressed image and 59 s for the device-side readback verify — the read path dominates
  as predicted, and it is in the frozen image, so this is the number the freeze decision
  rests on. **V7**: both halves — the silicon identifier (`0x16`) seen on the wire, and the
  raw-image bit order, which was **wrong**. `rpd_little_endian` had the updater writing
  every image bit-reversed with respect to what the configuration engine reads; nothing in
  the integrity chain can see this, because the flash matches what was sent. Four further
  gateware defects were found and fixed in the same session; all five are recorded in
  TESTING.md §6 and the two that are policy are on the EPCS layout page.
- **Outstanding — bench:** the interruption cases. Power pulled mid-write (which must leave
  the previous image bootable), power pulled during the readback verify, the boot block
  sector erased by JTAG, and the *recovery gateware* → **Reinstall gateware** repair driven
  from each of those states.
- **Exit:** gateware updated GUI-to-device in a few minutes, survivable at any
  interruption point, identity-verified after reboot. **Met except for survivability**,
  which is what the outstanding tests above are for.

### Phase 6 — Release pipeline

**Workflows and policy complete; two maintainer acts outstanding (the release keypair,
and the first tagged release), and the exit criterion cannot be met without them.**

- Extend the bitstream build to emit `gateware-app.rpd` + dual-image `.jic` + the
  provenance file covering them. *(Already delivered by Phase 4: `fpga/package.nix`
  emits `application/DomesdayDuplicator_auto.rpd`, `provisioning/…​.jic`, the boot
  block and a provenance record whose digest set covers `.sof`, `.jic`, `.rpd` and
  `.bin` alike.)*
- Author the **bitstream workflow**: `nix build .#bitstream` with unfree allowed,
  Quartus-closure caching (Actions cache or project-private binary cache; cold miss
  falls back to the hash-pinned Intel installer), runner disk preparation, artefact +
  digest upload; triggered by gateware changes, tags and manual dispatch.
- Author the **release workflow**: firmware + bitstreams built from the tag, bundle
  assembly, manifest signing with the release key (Actions secret); the pinned public
  key replaces the development key in the GUI; bundle + `.jic` + provenance attached.
- Author the **reproducibility audit** (scheduled): rebuild the latest release tag,
  compare digests against published provenance.
- Amend the policy prose that this decision supersedes: the bitstream-excluded-from-CI
  caveats in `README.md` and `fpga/README.md`, and the released-bitstreams-built-
  locally statement in `AGENTS.md` — all now describe the per-commit tier only, with
  the dedicated workflows called out. The maintainer's release act becomes: tag.
- The release key is pinned at *build* time, never read at run time: CMake compiles
  `tools/keys/release.pub` into the application (auto-detected in-tree,
  `-DDDD_RELEASE_UPDATE_KEY_FILE` otherwise, and refused if it looks like the
  development key). A build with no key pinned installs development bundles only and
  says so — the honest state, not a placeholder.
- **Outstanding — maintainer:** generate the release keypair (`minisign -G -W`), set
  `UPDATE_SIGNING_KEY` as a repository secret and commit `tools/keys/release.pub`; then
  cut the first `fw-v*` tag. The release workflow refuses to run without both, by
  design — a release signed with anything else is a release the application rejects.
  `tools/release/compatibility.env` carries a deliberate floor of `0.0.0` for the
  minimum application version until there is a numbered application release to name;
  raise it in the commit that first makes a higher floor true.
- **Exit:** a tagged release candidate produces a signed `.dddfw` attached by CI with
  every payload CI-built from the tag, installable via the Phase 2/5 file-picker
  path; the audit job has run green at least once against a real release. **Not yet
  met** — the pipeline is in place and everything testable without a key and a tag
  passes; no release has been cut.

### Phase 7 — Online fetch and user-facing polish

- GitHub releases fetcher, opt-in automatic check, download + SHA-256 verify —
  completing the wizard's *Checking* and *Downloading* stages (release-notes line,
  "verified as authentic" tick) — and the unified install UX with per-component
  version comparison covering all three versions — application, firmware, gateware —
  including the application-update notice routed to the platform channel and the
  enforced `minimum GUI version` gate ("update the application first"); packaging
  permission review.
- Documentation moves: "Updating your Domesday Duplicator" becomes a user-section
  page; the JTAG/jumper procedures move to a provisioning/recovery appendix.
- **Exit:** end-to-end rehearsal — a real release, discovered and installed from
  inside the GUI on Linux, Windows and macOS, including one simulated failure and
  recovery per target.

### Phase 8 — Provisioning from the application

Phases 1–7 answer "how does a working Duplicator get a newer one". This phase answers
the three questions they leave open, which turn out to be one question:

1. a **brand-new** build — two boards that have never been programmed;
2. an **old Duplicator** running firmware and gateware that predate all of this;
3. a unit whose recovery has gone far enough that the update path cannot reach it.

In all three the device cannot update itself, and today all three need a Quartus install,
`fx3-programmer` from a shell, and a procedure written for a developer. The end state of
this phase is that they need `ddd-gui`, the release artefacts, and the two cables the
hardware already has.

**Correcting an assumption before it is built on: there is no FPGA programmer in this
repository.** `fx3/programmer/` programs the FX3 and nothing else. The whole of the
project's FPGA programming story is `quartus_pgm` invoked by hand, plus the udev rules in
`fpga/configs/70-altera-usb-blaster.rules`. The FPGA half below is a new component, not
the wiring-up of an existing one, and it is the larger half by a wide margin.

#### The FX3 half — nearly free, and it belongs in Phase 3

A kit with a **blank** EEPROM and a kit whose EEPROM was corrupted by an interrupted
update are indistinguishable on the wire: the boot ROM finds no valid image and both
enumerate as `04b4:00f3`. Phase 3's engine port of the flash-programmer sequence therefore
already programs a new board; all that is missing is an interface that offers it as
*provisioning* and not only as *repair*. Phase 3's scope is widened to say so.

#### The FPGA half — the route, and why it is the only one

The FX3 cannot reach the FPGA's configuration circuitry: there is no JTAG, no AS pins, no
`nCONFIG`/`nSTATUS`/`CONF_DONE` and no MSEL between them, which is the fact the whole
factory/application split exists to work around. Writing the EPCS through the fabric needs
working gateware already in place, so a blank FPGA is unreachable that way *by
construction* and no amount of firmware work changes it.

What can reach it is the DE0-Nano's **own on-board USB-Blaster**, which enumerates as
`09fb:6001` on the board's mini-USB connector. It is an FTDI-style device that libusb can
drive directly, and this repository already ships the udev rules for it.

**The programming sequence is a build artefact, not something this project implements.**
That is the decision that makes this phase reasonable in size. `quartus_cpf` converts the
build's output into a file that *contains* the configuration and flash-programming
sequence as JTAG vectors, in one of two public formats. Both were checked against the
Quartus this project pins:

```bash
# Jam STAPL Byte Code (JEDEC JESD71), straight from the .jic the release already ships
quartus_cpf -c DomesdayDuplicator.jic DomesdayDuplicator.jbc

# Serial Vector Format, from the chain description build-local.sh already produces
quartus_cpf -c -q 4.5MHz -g 3.3 -n p \
    DomesdayDuplicator_write_jic.cdf DomesdayDuplicator.svf
```

`.jbc` accepts a `.jic` directly, which is the artefact the release publishes from Phase 6
— so the EPCS-programming case needs no new build step at all, only one more `quartus_cpf`
line. `.svf` goes through the `.cdf`, which `build-local.sh` already writes.

Every Cyclone IV-specific and serial flash loader-specific decision therefore stays inside
Quartus, at build time, in the one place that already has it and already runs it —
`quartus_cpf` even takes `--sfl_device` for exactly that purpose. What the application
needs is only:

- a **USB-Blaster driver** in the engine, Qt-free and behind a seam like every other
  device access. Generic, a few hundred lines of libusb: the cable is an FT245 in
  bit-bang and byte-shift modes, a protocol not published by Altera but reverse-engineered
  and stable for two decades, and implemented in urjtag, OpenOCD and openFPGALoader;
- an **SVF player** — a parser and a JTAG TAP state machine, with no knowledge of what
  device is on the far end. This is a pure function from a text file to a vector stream,
  so it unit-tests against committed fixtures with no cable and no board, which is the
  pattern the rest of the engine already follows;
- a **provisioning page** in the application that recognises an unprogrammed or
  half-programmed unit and offers to bring both halves up in one pass.

CI gains one artefact beside the `.jic` it already publishes from Phase 6. Which of the
two formats is the trade to weigh when this phase is taken: **SVF** is a text file and its
player is trivial, but it is verbose and slow for something the size of a flash image;
**JBC** is compact and comes straight from the `.jic`, but playing it means implementing a
small JESD71 byte-code interpreter rather than a parser. Start with SVF, whose player can
be written and tested in an afternoon, and move to JBC only if throughput demands it.

The commands above are verified as available. What is *not* yet verified, and needs a
board rather than a shell, is that a `.cdf`-derived SVF actually programs the EPCS through
the on-board USB-Blaster end to end — that is this phase's first bench task and should be
done before any of it is estimated.

The alternative — implementing Cyclone IV configuration and the serial flash loader
natively — is rejected. It is more code, all of it device-specific, all of it untestable
without hardware, and it would duplicate knowledge the build already holds.

What this buys is the removal of **Quartus from provisioning entirely** — today a
multi-gigabyte, unfree, x86_64-linux-only install whose only role for a board-builder is to
write a file the project already publishes. What it does not buy is the removal of the
second cable: the DE0-Nano's mini-USB has to be connected once per unit, and that is a
wiring fact rather than a software one. Phase 4's "the last time a cable is required"
stands; this phase only removes the toolchain from the other end of it.

**Licence position.** `openFPGALoader` does exactly this job and is **AGPL-3.0-only**;
GPLv3 §13 permits combining it with a GPLv3 work, but the AGPL terms travel with that
half, which is a change to this project's licence position (AGENTS.md §10 says raise it
first). The route above avoids the question entirely: SVF and Jam STAPL are public
formats, the USB-Blaster protocol is documented in several independent open-source
implementations, and neither needs a line of AGPL code copied. openFPGALoader remains
useful as a **reference to check behaviour against**, in the same way `minisign` is used
in the bundle tests — comparing against an independent implementation, not borrowing from
one.

- **Exit (bench):** two never-programmed boards — a bare SuperSpeed Explorer Kit and a
  bare DE0-Nano — assembled and brought to a working, capturing Duplicator using only
  `ddd-gui`, the published release artefacts and the two cables. Repeated for a Duplicator
  running pre-update firmware and gateware, and for a unit with both its EEPROM and its
  EPCS deliberately erased.

## Dependencies and ordering

Phases 2–3 (firmware path) and Phase 4 (gateware restructure) are independent and can
proceed in parallel; Phase 5 needs both 2 and 4; Phase 6 needs 1 and benefits from 5;
Phase 7 needs 6. The earliest user-visible win is at Phase 2 (jumper-free firmware
updates), which is also where the protocol design gets its first hardware proof before
the gateware work builds on it.

Phase 8 is deliberately last-numbered and is not last in dependency: its FX3 half is
folded into Phase 3 and lands with it, and its FPGA half depends only on Phase 6 having
published a programming artefact — the `.svf` alongside the `.jic` — to play back. It touches neither the update protocol nor the gateware
images, so it can be taken whenever it is worth more than the next phase in the sequence —
which, for anyone building boards rather than updating one, it may well be from the moment
Phase 6 exists.

## Risks

- **V1 is load-bearing**: if the kit does not fall back to USB boot on a *corrupt*
  (not just blank) EEPROM, Phase 2's safety story changes — mitigation is to verify V1
  on the bench *first*, before any of Phase 2 merges; the held-back-first-page order
  makes fallback overwhelmingly likely but it must be demonstrated.
- **Factory image quality**: the factory image is frozen after provisioning, so it
  gets disproportionate review and bench soak in Phase 4 — it is the one component a
  field update can never fix.
- **Watchdog policy**: the tickle-after-first-SPI-transaction design needs the
  watchdog period comfortably above worst-case FX3 boot + identity read. The mechanism
  is proved (an application image ran well past the ~54 s timeout with the watchdog
  enabled) but the period is still the field's maximum rather than a measured margin;
  it was not measured in Phase 4 and remains outstanding before the freeze.
- **The integrity chain cannot see the flash's bit orientation**, and this bit the
  project once. Every check compares the flash against what was sent, and an image
  written in the wrong bit order matches itself perfectly; only the configuration engine
  ever sees the difference, and it cannot report. The orientation is decided by
  `rpd_little_endian` in the application `.cof` and is now commented as load-bearing, but
  the general lesson stands: **anything the host and device agree on cannot be validated
  by them agreeing**, and this is the only such thing left in the chain.
- **Quartus-in-CI sustainability**: the release path now depends on a multi-gigabyte
  unfree toolchain materialising on a hosted runner — cache eviction makes releases
  slow, Intel CDN link rot makes them fail (visible immediately, since the fetch is
  hash-pinned), and runner disk limits need active management. Mitigations: the
  Cyclone IV-restricted closure keeps size down; the audit job doubles as a canary
  that the cold-miss path still works; and the previous revision's design —
  maintainer-built bitstreams committed to `fpga/prebuilt/` behind a
  digest-and-source-tree-hash gate — is preserved in git history as a drop-in
  fallback that reshapes nothing downstream. The closure cache must stay
  project-private: caching Quartus for our own CI is ordinary use, redistributing it
  is not.
- **A JTAG driver is new ground for this project**: Phase 8's FPGA half is the first code
  here that drives a programming cable rather than a peripheral, and its failure modes are
  unfamiliar. The risk is bounded by the medium — the EPCS is rewritable over JTAG
  unconditionally, so a failed attempt is retried rather than recovered from, and a board
  cannot be put beyond the reach of Quartus by anything this code does. Taking the
  programming sequence from a Quartus-exported SVF rather than implementing it removes
  most of what would otherwise be unfamiliar: what is left is a cable driver and a file
  player, both of which fail loudly. The open question is not feasibility but throughput —
  SVF is verbose, and whether flash programming through it is measured in seconds or in
  minutes is a bench fact.
- **Signing-key custody**: the release secret key in GitHub Actions is the one secret
  in the chain; its compromise would let signed-but-hostile bundles verify. Mitigation
  is scope (the key signs release manifests only, in a tag-triggered workflow), the
  documented rotation path (a GUI release pinning a new key), and the recorded
  fallback of maintainer-local signing if CI custody ever becomes uncomfortable.
