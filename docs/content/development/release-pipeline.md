# Release pipeline

Every artefact a user installs is built by CI from a tagged commit. Nothing in a release is built on a maintainer's machine, and nothing is attached by hand.

That is a change from how this project worked until Phase 6 of the device-update plan: the FPGA bitstream used to be built locally and attached to a draft release, because Quartus is unfree, multi-gigabyte and cannot be served from a binary cache. Those facts have not changed — what changed is where the cost is paid. Quartus stays out of the per-commit tier that every contributor runs, and runs in dedicated workflows instead.

**The maintainer's release act is: tag.**

## The four tiers

| Workflow | Runs on | What it proves |
| --- | --- | --- |
| `build.yml` | every push, every branch | the whole tree builds and its tests pass; the update tooling assembles a real development bundle |
| `bitstream.yml` | gateware changes, manual dispatch, and from the tag | both gateware images compile, with digests and a provenance record |
| `release-firmware.yml` | `fw-v*` tags | a signed update bundle and the provisioning artefacts, all built from that tag |
| `reproducibility-audit.yml` | weekly, and on dispatch | the published release still rebuilds to the same bytes |

`release-gui.yml` is the fourth release stream and is unchanged by this: the capture application releases separately under `gui-v*` tags.

### Per-commit tier — `nix flake check` and `build.yml`

It builds the firmware, the programmer, the capture application and the documentation site; it lints and simulates **both** gateware images with the free tools; it runs the bundle tooling's own self-test; and it assembles a firmware-only *development bundle* from that commit's firmware, so every commit has an artefact somebody can take to bench hardware.

**Nix is the only supported development environment, and the CI shape follows from that** (maintainer, 2026-08-15). There are no native build jobs: each component is built and tested by its Nix package, and the three platform toolchains survive only inside the packaging workflows, where they are unavoidable — Nix produces neither an MSI nor a DMG. Those jobs build, install and launch what they package, so platform coverage comes from the artefacts a user actually receives rather than from a build nobody installs.

The capture application's two quality gates — `clang-format` and `clang-tidy` — run in a separate job, inside `nix develop`. That is deliberate and was learned the hard way: both are version-sensitive, and when CI used a runner's toolchain while developers used the pinned one, the project spent a day on a failure nobody could reproduce locally, because the two disagreed about which checks existed. A gate that can disagree with the shell it enforces reports on the toolchain, not on the code.

The application under `gui/` is **not** built, tested or packaged by any of this. It remains in the repository as a reference until its replacement reaches feature parity; `nix build .#gui` still works, and nothing in CI verifies it.

`nix flake check` deliberately contains no Quartus. A contributor fixing a typo must not need an unfree-enabled Nix configuration and a multi-gigabyte download to find out whether their change is sound.

### Bitstream workflow

`nix build .#bitstream` on a GitHub-hosted runner. The mechanics that make that viable are in the workflow rather than in anyone's head:

- **Closure caching.** Quartus is `redistributable = false`, so no public substituter will ever have it. The workflow keeps the project's own copy of the closure in the Actions cache, keyed on `flake.lock` — a nixpkgs bump gets a cold run, everything else gets a warm one. A cold run fetches the installer from Intel's CDN, which nixpkgs pins by hash: slow, never wrong. A project-private binary cache (Cachix, S3) is the alternative if Actions cache eviction ever makes the cold path the common path.
- **Disk preparation.** A stock runner has around 14 GB free, and the closure plus two Quartus compiles needs most of it. The workflow removes the preinstalled toolchains it will never use (.NET, Android, GHC, CodeQL, the cached Docker images) before Nix is installed.
- **Device support restricted to Cyclone IV**, in `flake.nix`. That single override is the largest size saving available — the default six families are six multi-hundred-megabyte downloads.

It uploads everything `fpga/package.nix` produces, plus a `SHA256SUMS`, and prints the provenance record into the run summary. So every gateware change has a CI-built bitstream, not only every release.

#### Licence position

Stated plainly, because a workflow that downloads an unfree toolchain should not leave anyone guessing:

- Quartus Prime **Lite** needs no licence file. Nothing here has one, or wants one.
- Installing it in CI from Altera's own installer is ordinary use of a free-of-charge tool.
- Caching its closure so that *our own* CI does not re-download it is ordinary use. **Publishing that cache would be redistribution**, which the licence does not permit — so the cache is private to this repository, and any binary cache that replaces it must be private too.
- Third-party Docker images containing Quartus are rejected for the same reason: they are redistribution, whoever performs it.

### Release workflow

Triggered by a `fw-v*` tag. It calls the bitstream workflow with the tag as its ref — rather than repeating it — builds the firmware and the programmer from the same tag, and then assembles the release:

| Asset | What it is for |
| --- | --- |
| `domesday-duplicator-update-<ver>.dddfw` | **the update bundle** — the only asset a device installs by itself |
| `firmware.img` / `.elf` / `.map` | the FX3 image and its debug companions |
| `fx3-programmer-<ver>-linux-x64` | bench recovery over the USB bootloader |
| `DomesdayDuplicatorProvisioning.jic` / `.map` | first provisioning of a new board over JTAG: both gateware images |
| `DomesdayDuplicator_auto.rpd`, `boot-block.bin` | the raw application image and its boot block, published for inspection |
| `DomesdayDuplicatorFactory.sof` | the factory image, for bench JTAG configuration |
| `bitstream-provenance.txt` | Quartus version and per-artefact digests, release and canonical |
| `SHA256SUMS`, `PROVENANCE.txt` | one manifest over every asset, and what built them |

Three gates stand between the build and the published release, and each of them has failed for real reasons in this project's history:

1. **The firmware carries the commit.** A `.img` containing the string `unknown`, or not containing this tag's short hash, fails the release. The version reaches the USB product descriptor, so it can be checked without a device.
2. **The bitstream was built from the tag.** Its provenance record must name the tag's commit. The build itself already refuses to produce a record with an unknown commit.
3. **The bundle verifies against the pinned public key.** Verified independently after assembly, with stock `minisign`, against `tools/keys/release.pub` — the same bytes the application compiles in. A bundle signed with anything else is a bundle the application would refuse, so this is a failure and not a warning.

The manifest's compatibility fields are read out of the sources that implement them at release time — the protocol version from the FX3 descriptor, the register map version from `spiRegisters.v`, the EPCS layout version from the boot-block encoder — so a manifest cannot claim a protocol the firmware does not speak. The one field that is a *decision* rather than a fact, the minimum application version, lives in `tools/release/compatibility.env`, which the tag pins along with everything else.

### Reproducibility audit

Weekly, and on demand. It takes the most recent `fw-v*` release, rebuilds it from scratch through the same bitstream workflow, and compares:

| Artefact | Compared how |
| --- | --- |
| `firmware.img`, `.jic`, `.rpd`, boot block | byte for byte |
| `.sof` files | **canonical** digest only — a `.sof` embeds a compile timestamp and a per-run design hash, so its release digest is expected to differ |
| the `.dddfw` bundle | its manifest's payload digests against the rebuilt payloads, plus a signature check against the key committed *at that tag* |

The bundle archive itself is not compared byte for byte: re-signing it would need the release secret key, which this job deliberately does not have. Comparing the payload digests is the stronger check in any case — it asks whether the published bundle carries the bytes this source produces.

A failure means one of three things, all worth knowing within days rather than at the next release: the toolchain has drifted, a published asset does not match its source, or the reproducibility this project claims has stopped being true.

## Key custody

The update bundle is signed with **minisign** (Ed25519), and the signature is over `manifest.json` — which carries the SHA-256 of every payload. Signing the manifest rather than the archive is what keeps the bundle a single self-verifying file: the offline file-picker path and the online path verify identically.

| | Release key | Development key |
| --- | --- | --- |
| Secret half | repository secret `UPDATE_SIGNING_KEY` | **committed** at `tools/keys/development.key` |
| Public half | committed at `tools/keys/release.pub`, compiled into the application | committed, compiled into the application |
| A signature proves | the bundle came from this project | the bundle is well formed, and nothing else |
| Accepted by a release build | yes, and nothing else | only on an explicit per-invocation opt-in, and bannered every time |

The public key is a **build** input, never a file the application reads at run time: a key loaded from disk is a key an attacker can replace, and then the whole integrity chain proves nothing. An in-tree build picks up `tools/keys/release.pub` automatically; anything building from a source tree without it (the Nix package closes over `ddd-gui/` alone) is passed `-DDDD_RELEASE_UPDATE_KEY_FILE`. A build with no key pinned can install development bundles and honestly cannot verify a release one — it says so rather than pretending to a trust it does not have.

### Generating the release key

Once, by the maintainer:

```bash
minisign -G -W -p tools/keys/release.pub -s release.key
```

`-W` leaves the secret key without a passphrase, because a passphrase held in a second CI secret protects nothing the first secret does not already expose. Then:

1. paste the contents of `release.key` into the repository secret `UPDATE_SIGNING_KEY`;
2. commit `tools/keys/release.pub`;
3. keep the only other copy of `release.key` offline, and delete it from the working tree.

### Rotation

A matching pair: the new public key committed here, the new secret set in the repository settings. Older applications keep verifying older bundles against the older key, which is why the audit checks each release against the key committed *at its own tag* — and why the release notes for a rotation have to say that a key changed.

The fallback, if holding the key in CI is ever uncomfortable, is that the maintainer signs the CI-assembled manifest locally and uploads the signature. Nothing else in the chain changes shape.

## Cutting a release

```bash
git tag fw-v1.4.0
git push origin fw-v1.4.0
```

That is the whole procedure. What follows from it:

1. `bitstream.yml` compiles both gateware images from the tag.
2. `release-firmware.yml` builds the firmware and the programmer from the same tag, assembles the `.dddfw`, signs it with the release key, and runs the three gates above.
3. The release is published with every asset and its digests.

Then, before announcing it:

- install the bundle onto bench hardware from the application's file-picker path, and confirm the device reports the identities the manifest names;
- check the run summary's manifest for the versions you expected — the compatibility fields are read from the sources, so a surprise there is a real disagreement;
- watch the next reproducibility audit, or dispatch it against the new tag directly.

If the tag was wrong, delete the release and the tag and start again: nothing in this pipeline is idempotent-by-accident, and re-running the workflow against a re-created tag rebuilds everything from that commit.

## What this replaces

If you have worked on this repository before Phase 6, three things are no longer true:

- ~~the bitstream is not built by CI~~ — it is, in its own workflow;
- ~~firmware releases are published as drafts so the bitstream can be attached by hand~~ — they publish directly, with the gateware already in them;
- ~~`SHA256SUMS` is extended by hand after the fact~~ — CI writes it over the complete asset set.

The fallback design, if Quartus-in-CI proves unsustainable (cache eviction economics, runner disk, Intel CDN rot), is preserved in this repository's history: maintainer-built bitstreams committed to a tracked `fpga/prebuilt/` directory behind a digest-and-source-tree-hash gate. It reshapes neither the bundle, the signing, nor the integrity chain.
