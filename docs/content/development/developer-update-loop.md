# Developer update loop

Releases are built by CI from a tag. This page is about the other case: you have changed the firmware or the gateware and you want it running on the hardware in front of you, now, without cutting a release — and without taking a path so different from the real one that it proves nothing.

Three tiers, fastest first. Pick by what you are working on.

## 1. RAM load — seconds, volatile

Fit the J4 jumper, power-cycle, and `fx3-programmer -u firmware.img` downloads the image into the FX3's RAM and jumps to it. Nothing is written to the EEPROM; the next power cycle forgets it entirely.

This is the fastest firmware-iteration path there is and this work does not change it. Use it when you are iterating on firmware *logic* — a GPIF timing question, a descriptor, a control-request handler — and you want the edit-build-run cycle as short as it goes.

It bypasses the update mechanism completely, so it proves nothing about updating. When you are working on the update path itself, use tier 2.

The procedure is on the [FX3 firmware programming](hardware-programming/fx3-firmware.md) page.

## 2. Development bundle — the real path, locally

```bash
./tools/dev-bundle.sh
```

Collects whatever is built locally, packages it as a real `.dddfw` update bundle signed with the development key, and writes it to `build/`.

It builds nothing itself, deliberately. Building the firmware is `cmake --build fx3/firmware/build` or `nix build .#fx3-firmware`; building the gateware is `./fpga/build-local.sh` or `nix build .#bitstream`. Each has its own toolchain, flags and failure modes, and a wrapper that restated them would be a second, worse copy of two build systems. What this script does is find the outputs and package them:

| | Looked for, in order |
| --- | --- |
| firmware | `fx3/firmware/build/firmware.img`, `result-firmware/firmware.img`, `result/firmware.img` |
| gateware | `fpga/build/application/*.rpd`, `result-bitstream/application/*.rpd` |
| vectors | `fpga/build/factory/DomesdayDuplicatorFactoryConfigure.svf`, `result-bitstream/factory/…` |
| factory image | `fpga/build/factory/DomesdayDuplicatorFactory_auto.rpd`, `result-bitstream/factory/…` |

The **application** directory specifically for the gateware, in both layouts: that is the half a device update rewrites. The two factory artefacts beside it are separate payloads with separate flags.

### One bundle, four payloads

There is no second kind of file. The first two payloads are what an ordinary update writes to a working device over USB; the other two are what [a bring-up](../capture-gui/bringing-up-a-board.md) needs on top, and only a bring-up reads them. The update window installs the two components it knows and ignores the rest, so one file serves both.

Whatever is built locally goes in, and **what is missing is named**:

```
  (an update needs the firmware and the gateware; this bundle has one of them)
  (no factory pair: this bundle can update a device but cannot bring a board up.
   Both come from the Quartus build: ./fpga/build-local.sh or nix build .#bitstream)
```

That is a note rather than a refusal. A bundle missing a payload is legal and useful — it is what a firmware developer with no Quartus produces every day — but which payload is missing decides what the file can do, and saying so here beats a refusal on the wizard's image page later. **A firmware developer never needs Quartus for this loop**, and a gateware developer never needs the ARM cross-toolchain. Nothing built at all is an error, with a reminder of the two build commands.

This is also how to try the offline path locally, since it produces exactly the file the installers bundle:

```bash
cmake -B build -S ddd-gui \
    -DDDD_BUNDLED_UPDATE_FILE="$PWD/build/domesday-duplicator-update-0.0.0-dev.dddfw"
cmake --build build
```

The build then places the file beside the binary under the name an installed application searches for, so `build/bin/ddd-gui` opens its bring-up wizard with a file already chosen — and, being development-signed, bannered as such every time.

### Each payload's identity is read out of the payload

The commit a bundle *claims* for each component is read from the artefact that will have to report it — the firmware's USB product string out of `firmware.img`, the gateware's commit out of the `bitstream-provenance.txt` beside the image — and not from the working tree.

They are only the same thing when everything was just rebuilt, and taking the tree's commit instead cost a bench run: HEAD moved between a build and the packaging, the manifest promised a commit nothing inside it could report, and a multi-minute gateware update failed at its very last step with the flash already correctly written. The install-time check that caught it was right; the bundle was wrong.

So packaging an artefact older than the tree is legitimate — it is what happens whenever the gateware is left alone while the firmware is worked on — and the script prints a note saying which commit each half really came from. What it will not do is guess: an artefact it cannot read an identity out of is refused, with the rebuild command to run.

It runs in `nix develop`, which puts `minisign` and GNU `tar` on `PATH`.

The bundle is stamped `"channel": "development"`, version `0.0.0` — a version no release will ever carry, so it cannot be mistaken for one at a glance — and the commit of your working tree, marked `-dirty` if it is.

### The development key

The bundle is signed with `tools/keys/development.key`, whose secret half is **committed to this repository and is therefore public**.

A development signature proves the bundle is well formed and proves nothing whatever about where it came from. That is the entire point of it being a separate key and a separate channel:

- a **release build** of the application pins the release public key and accepts nothing else;
- accepting the development key requires an explicit, per-invocation opt-in — `--dev-update-key`, which both `ddd-update` and the application itself take, or a debug build, which implies it;
- a development-signed bundle is bannered prominently as such in the update interface, every time.

So on a build that pins a release key — which is what a release is, and what `nix build .#ddd-gui` produces once `tools/keys/release.pub` exists — installing your own bundle from the **Update** tab means launching the application as `ddd-gui --dev-update-key`. Without it the file verifies against nothing the build trusts and the install button stays disabled, which is the correct answer to a file the application has no reason to trust and a surprising one if you built the file yourself five minutes ago.

The flag is per-run and never persisted, and it widens the policy rather than replacing it: a release-signed bundle still verifies against the release key first, and the development key is never consulted for one.

There is no unsigned path at all. The development key *is* the unsigned-equivalent, made explicit and impossible to confuse with a release. An actually-unsigned format would have needed a second route through verification, and a second route through verification is where the bugs live.

## 3. Headless install — `ddd-update`

```bash
./tools/dev-bundle.sh && ddd-update build/domesday-duplicator-update-0.0.0-dev.dddfw
```

The updater engine is Qt-free, so a thin command-line tool drives the identical engine code path the application's update dialog does: verify the bundle, program the device, reset it, re-read its identity, exit non-zero on any failure.

Two things follow from it being the same code path rather than a parallel implementation. It is the scriptable half of the bench procedures in `TESTING.md`, and — more importantly — a bug found here is a bug in the code the application runs, not in a test harness that resembles it.

`dev-bundle.sh && ddd-update` is the whole edit-to-running-device loop.

```
Usage:
  ddd-update [options] <bundle.dddfw>

Options:
  --device <path>       Update the device at this path, when several are attached
  --dev-update-key      Accept a bundle signed with the development key, whose
                        secret half is public. Proves the file is well formed and
                        nothing about where it came from
  --dry-run             Verify the bundle and check it against the device, then
                        stop without sending anything
  --help                Show this text

A device with no working firmware — one that has never been programmed, or one
whose update was interrupted — enumerates as the FX3 boot ROM and is programmed
by this same command, from this same bundle. There is nothing different to run.

Exit codes: 0 success, 2 usage, 3 bundle, 4 no device, 5 update failed.
```

`ddd-update` handles a device in recovery mode without being told to. It notices the personality during enumeration, hands the boot ROM the bundle's own `firmware.img`, waits for the device to come back as a Duplicator, and then runs the ordinary install over it — which is what makes `dev-bundle.sh && ddd-update` the loop for bringing up a bare Explorer Kit as well as for iterating on a working one. The mechanism is on the [Device update mechanism](device-update-mechanism.md) page.

The exit codes are distinct on purpose: a script driving a bench procedure wants to know whether it has a bad file (3) or a bad device (4, 5). `--dry-run` is the one mode that touches no device state at all — it verifies the signature, checks every payload digest, runs the compatibility gate against the attached device, and stops. It is what a script checks a bundle with.

`ddd-update` is built by `cmake --build ddd-gui/build` alongside the application, and by `nix build .#ddd-gui`. It links no Qt, which is not a detail: that is the enforcement of the rule that the updater engine is Qt-free. If a Qt dependency ever creeps into the update path, this target stops linking.

!!! note "Both halves install"

    Both targets install, and `ddd-update` drives them without being told which is which. The firmware half has worked the longest; the gateware half was first performed on hardware on 2026-08-15 (TESTING.md §6). Expect the gateware target to take minutes rather than seconds, and to end in a reconfiguration rather than a device reset.

    The one prerequisite is that the unit has been **provisioned** with the dual-image flash over JTAG, once, before any of this works — a unit whose FPGA carries a single old image has nothing behind the capture gateware to be updated *by*, and the firmware says so rather than trying. That procedure is on the [EPCS layout and boot flow](epcs-layout-and-boot-flow.md#provisioning-a-unit) page.

## What CI does with the same tooling

Every commit that touches the firmware or the tooling runs the same two scripts:

- `nix flake check` assembles a bundle from a synthetic payload, takes it apart with stock `tar`, `minisign` and `sha256sum`, and rebuilds it to confirm the output is byte-for-byte reproducible;
- the build workflow then packages *that commit's real firmware* into a development bundle and uploads it, so every commit has an artefact you can download and take to the bench without re-running anything.

Nothing in CI ever writes to a device. Assembling a file is automated; installing one is a deliberate human act, and that line is not moved.

Related pages: [Update bundle format](update-bundle-format.md), [Device update mechanism](device-update-mechanism.md), [Building locally](building-locally.md).
