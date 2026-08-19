# Release pipeline

Every artefact a user installs is built by CI from a tagged commit. Nothing in a release is built on a maintainer's machine, and nothing is attached by hand.

That is a change from how this project used to work: the FPGA bitstream used to be built locally and attached to a draft release, because Quartus is unfree, multi-gigabyte and cannot be served from a binary cache. Those facts have not changed — what changed is where the cost is paid. Quartus stays out of the per-commit tier that every contributor runs, and runs in dedicated workflows instead.

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
| `domesday-duplicator-update-<ver>.dddfw` | **the bundle**, and the only signed asset. Four payloads: the firmware and application gateware a working device installs over USB, plus the JTAG vectors and factory image a bring-up needs on top |
| `firmware.img` / `.elf` / `.map` | the FX3 image and its debug companions |
| `fx3-programmer-<ver>-linux-x64` | bench recovery over the USB bootloader |
| `DomesdayDuplicatorProvisioning.jic` / `.map` | first provisioning of a new board over JTAG: both gateware images |
| `DomesdayDuplicatorFactoryConfigure.svf` | the factory image as JTAG vectors — what `ddd-jtag` and the bring-up wizard play |
| `DomesdayDuplicator_auto.rpd`, `boot-block.bin` | the raw application image and its boot block, published for inspection |
| `DomesdayDuplicatorFactory.sof` | the factory image, for bench JTAG configuration |
| `bitstream-provenance.txt` | Quartus version and per-artefact digests, release and canonical |
| `SHA256SUMS`, `PROVENANCE.txt` | one manifest over every asset, and what built them |

Three gates stand between the build and the published release, and each of them has failed for real reasons in this project's history:

1. **The firmware carries the commit.** A `.img` containing the string `unknown`, or not containing this tag's short hash, fails the release. The version reaches the USB product descriptor, so it can be checked without a device.
2. **The bitstream was built from the tag.** Its provenance record must name the tag's commit. The build itself already refuses to produce a record with an unknown commit.
3. **The bundle verifies against the pinned public key.** Verified independently after assembly, with stock `minisign`, against `tools/keys/release.pub` — the same bytes the application compiles in. A bundle signed with anything else is a bundle the application would refuse, so this is a failure and not a warning. The same step checks that all four payloads are present, because a release that quietly lost the two bring-up ones would update a working device perfectly well and stop being able to bring a board up — which nobody discovers until they are standing in front of a bare board.

The manifest's compatibility fields are read out of the sources that implement them at release time — the protocol version from the FX3 descriptor, the register map version from `spiRegisters.v`, the EPCS layout version from the boot-block encoder — so a manifest cannot claim a protocol the firmware does not speak.

The manifest also carries a minimum application version, from `tools/release/compatibility.env`, and it is no longer read by the application. It was compared against the application's own dotted release version, which no longer exists: every part of a Duplicator now stamps the commit it was built from, and a commit orders nothing. The field stays because the manifest schema requires it and every application already in the field parses it. **A bundle that needs a newer application should say so by advertising a protocol version or register map version outside the range the old application knows** — that refusal is still enforced, it is derived from the sources rather than decided, and it describes what the old application actually cannot do rather than how old it is.

### Reproducibility audit

Weekly, and on demand. It takes the most recent `fw-v*` release, rebuilds it from scratch through the same bitstream workflow, and compares:

| Artefact | Compared how |
| --- | --- |
| `firmware.img`, `.jic`, `.rpd`, boot block | byte for byte |
| `.sof` files | **canonical** digest only — a `.sof` embeds a compile timestamp and a per-run design hash, so its release digest is expected to differ |
| the `.dddfw` bundle | its manifest's payload digests against the rebuilt payloads, plus a signature check against the key committed *at that tag* |

The bundle archive itself is not compared byte for byte: re-signing it would need the release secret key, which this job deliberately does not have. Comparing the payload digests is the stronger check in any case — it asks whether the published bundle carries the bytes this source produces.

A failure means one of three things, all worth knowing within days rather than at the next release: the toolchain has drifted, a published asset does not match its source, or the reproducibility this project claims has stopped being true.

## The bundled update file

The one place the two release streams touch, and the design is shaped by keeping that touch as small as possible.

A board being brought up **cannot be updated over USB** — that is the whole reason [the bring-up wizard](../capture-gui/bringing-up-a-board.md) exists — so the machine beside it may be one that has just been built and has no network. A packaged build of the capture application therefore installs one update bundle beside itself, and the wizard preselects it.

That bundle is a firmware-stream artefact, so a `gui-v*` packaging job must not build it (§9: every artefact CI-built, and the two streams are separate). It **pins** one instead:

```
ddd-gui/packaging/bundled-update.env
  BUNDLED_UPDATE_TAG      the fw-v* release it came from
  BUNDLED_UPDATE_URL      the published asset
  BUNDLED_UPDATE_SHA256   its digest
```

`tools/fetch-bundled-update.sh` reads that pin, downloads, and refuses anything whose digest differs — one script, called by all three packaging workflows, because a fetch-and-verify written out three times is three places for the verify to go missing. CMake takes the file as `-DDDD_BUNDLED_UPDATE_FILE` and installs it under one fixed name in whichever place the platform keeps read-only application data; `ddd-gui/src/gui/bundled_update.cpp` is the matching search.

Three properties worth stating, because each of them is a decision:

- **An empty pin is a legitimate build.** The packaging jobs skip the fetch and the wizard opens with its file picker and no preselection. An application that pretended to carry a file it did not have would fail at the one moment its user has no network to fix it with — the same reasoning as the unpinned release key.
- **The pin protects packaging, not installation.** The digest is what makes an unattended download safe in a job with no key and nobody watching. The application still verifies the signature and every payload digest before it programs anything, bundled or picked. Being bundled is never a shortcut through that.
- **Each packaging job checks its own output.** Flatpak, MSI and DMG all assert the set survived into the installed application when one was pinned — the failure this guards against is an installer that looks complete and then cannot bring a board up on the machine that cannot fetch anything.

Updating the pin is a deliberate commit after a firmware release: take the digest from that release's `SHA256SUMS`, set the three values, and say which firmware tag in the commit message.

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

The maintainer's release act is to push a tag. Everything below is either what to check
before pushing it, or what to do with what comes back.

### The two streams

They are separate on purpose, and a tag in one never builds the other. Two streams that both
rebuilt everything would be one stream with extra steps — and a documentation fix would
recompile Quartus.

| | Firmware and gateware | Capture application |
| --- | --- | --- |
| Tag | `fw-v<version>` | `gui-v<version>` |
| Workflow | `release-firmware.yml` | `release-gui.yml` |
| Builds | FX3 firmware, programmer, both gateware images | Flatpak, DMG, MSI |
| Signed | yes — the `.dddfw` manifest, with the release key | no; the installers are unsigned |
| Takes | tens of minutes, longer on a cold Quartus cache | around half an hour |

The **update bundle a user installs comes only from the firmware stream**. A `gui-v*` tag
produces installers and nothing else, so if the point of a release is to put a `.dddfw` in
people's hands, the firmware tag is the one that does it.

### Tag naming, and the two rules that bite

**A `fw-v*` tag must be plain dotted numeric.** `fw-v1.4.0` is fine; `fw-v1.4.0-beta1` is
rejected. The version goes into the bundle manifest, and the manifest reader refuses a
`version` that is not a dotted sequence of decimal numbers — so a suffixed tag would produce
a bundle no application could open. The check is real and it fails the release — but it runs
*after* the gateware has compiled, so a mistyped tag costs a Quartus run before it tells
you.

**A `gui-v*` tag may carry a suffix.** `gui-v1.0.0-beta1` is fine. Nothing downstream orders
it: the MSI falls back to a `0.0.<run>.0` product version, which is deliberately lower than
any numbered release and so cannot block a later upgrade.

Both streams mark a release as a **pre-release** when its version says it is one — a `0.x`
version in either stream, or a suffix in the GUI stream. That matters more than it looks:
an unmarked release becomes the repository's "Latest release", which is what GitHub's
release banner points people at. This repository still has the legacy `V2.x` releases, and
`V2.4` currently holds that position; a pre-release will not displace it, and a `1.0.0` will.

### Before you tag

Tag a commit CI has already been green on. The release workflows rebuild everything from the
tag, so a tag on an untested commit finds out the hard way, slowly.

```bash
git switch main
git pull
gh run list --branch main --limit 5        # the Build run must be green
nix flake check                            # optional second opinion, locally
```

Check too that `UPDATE_SIGNING_KEY` is still set, because a firmware release without it
fails at the signing step after everything else has succeeded:

```bash
gh secret list
```

### Releasing firmware and gateware

```bash
git tag fw-v1.4.0
git push origin fw-v1.4.0

gh run list --workflow release-firmware.yml --limit 1
gh run watch <run-id>
```

What runs, in order: the gateware compiles from the tag; the firmware and programmer build
from the same tag; the `.dddfw` is assembled and signed; and the three gates described above
run before anything is published. There is no draft stage — a release either appears
complete or does not appear.

Then check what was published, rather than assuming it:

```bash
gh release view fw-v1.4.0

mkdir -p /tmp/fw-check && cd /tmp/fw-check
gh release download fw-v1.4.0
sha256sum -c SHA256SUMS

tar -xf domesday-duplicator-update-1.4.0.dddfw manifest.json manifest.minisig
minisign -Vm manifest.json -x manifest.minisig \
         -p ~/Coding/domesdayduplicator/tools/keys/release.pub
```

The run summary also prints the manifest. Read the compatibility fields in it: they are
read out of the sources at release time, so a value you did not expect is a real
disagreement between the manifest and the firmware or gateware, not a formatting quirk.

Then install it onto bench hardware through the application's file-picker path, and confirm
the device reports the identities the manifest names. Nothing in CI touches hardware.

### Pinning the bundle into the application

This is the one place the two streams touch, and it is a deliberate commit rather than
anything automatic. Do it when the next application release should carry this firmware
bundle for offline bring-up.

```bash
tag=fw-v1.4.0

gh release view "$tag" --json assets \
  -q '.assets[] | select(.name | endswith(".dddfw")) | .url'

gh release download "$tag" --pattern SHA256SUMS --output - \
  | awk '/\.dddfw$/ {print $1}'
```

Put those two values, and the tag, into `ddd-gui/packaging/bundled-update.env`, and commit
with the firmware tag in the message. `tools/fetch-bundled-update.sh --check` will tell you
whether the pin is well formed without downloading anything:

```bash
./tools/fetch-bundled-update.sh --check
```

Leaving the pin empty is a legitimate choice, not a postponement: the installers are smaller
and the bring-up wizard simply opens with its file picker. What it costs is the one case the
bundling exists for — a machine with no network, next to a board that cannot be updated over
USB.

### Releasing the capture application

```bash
git tag gui-v1.0.0-beta1
git push origin gui-v1.0.0-beta1

gh run list --workflow release-gui.yml --limit 1
gh run watch <run-id>
```

Three packaging jobs run in parallel, each of which **installs and launches what it built** —
the Flatpak from its own bundle, the MSI through `msiexec` and then uninstalled again, the
DMG's bundle checked for any surviving Homebrew path. Only then does a single publish job
attach everything, and it refuses to publish unless all three installers are present and
every file name carries the tagged version.

The installers are unsigned. macOS will show a Gatekeeper prompt and Windows a SmartScreen
warning on first run; both installation pages document this, and it is worth saying again in
any announcement.

### A first beta, end to end

The order matters, because the application can only bundle a firmware bundle that already
exists:

```bash
# 1. firmware and gateware first — this is what produces the .dddfw
git tag fw-v0.9.0 && git push origin fw-v0.9.0

# 2. pin that release's bundle into the application, and commit it
$EDITOR ddd-gui/packaging/bundled-update.env
./tools/fetch-bundled-update.sh --check
git commit -am 'Pin the fw-v0.9.0 update bundle' && git push

# 3. the application, tagged on the commit that carries the pin
git tag gui-v0.9.0 && git push origin gui-v0.9.0
```

Both are `0.x`, so both publish as pre-releases and neither becomes "Latest". Testers get a
signed update bundle, three installers that already carry it, and a legacy `V2.4` release
still sitting where it was for anyone who is not testing.

Skipping step 2 is a reasonable shortcut for a first beta — the installers then carry no
bundle, and testers download the `.dddfw` from the firmware release and open it by hand.

### When something goes wrong

**A packaging job failed.** Fix it on a branch, let the branch build prove it — `build.yml`
runs the same three packaging workflows on every push — then delete the tag and start again.

**The publish job failed but the packaging succeeded.** Re-run without re-tagging. The one
thing that matters is that the dispatch happens **on the tag**: the packaging workflows read
their version from the ref they are called on, not from the input, so a dispatch from a
branch would stamp `git-<sha>` into every file name. The workflows now check this and stop in
seconds rather than half an hour, but the correct invocation is:

```bash
gh workflow run release-gui.yml --ref gui-v1.0.0-beta1 -f tag=gui-v1.0.0-beta1
```

**The tag was wrong.** Delete the release and the tag, and start again. Nothing in this
pipeline is idempotent by accident, and re-running against a re-created tag rebuilds
everything from that commit:

```bash
gh release delete gui-v1.0.0-beta1 --yes
git push --delete origin gui-v1.0.0-beta1
git tag -d gui-v1.0.0-beta1
```

**A gate rejected an artefact.** Do not work around it. Each of the three firmware gates
exists because the failure it names has happened: an artefact reporting `unknown`, a
bitstream built from something other than the tag, a bundle signed with the wrong key. A
gate firing means the release would have been wrong.

### After announcing

Watch the next reproducibility audit, or dispatch it against the new tag directly. It
rebuilds the release from scratch and compares digests, so a drift between what was
published and what the source produces surfaces within days rather than at the next release.

## What this replaces

If you have worked on this repository before the bitstream moved into CI, three things are no longer true:

- ~~the bitstream is not built by CI~~ — it is, in its own workflow;
- ~~firmware releases are published as drafts so the bitstream can be attached by hand~~ — they publish directly, with the gateware already in them;
- ~~`SHA256SUMS` is extended by hand after the fact~~ — CI writes it over the complete asset set.

The fallback design, if Quartus-in-CI proves unsustainable (cache eviction economics, runner disk, Intel CDN rot), is preserved in this repository's history: maintainer-built bitstreams committed to a tracked `fpga/prebuilt/` directory behind a digest-and-source-tree-hash gate. It reshapes neither the bundle, the signing, nor the integrity chain.
