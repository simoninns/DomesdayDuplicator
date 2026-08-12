# Implementation Plan

Execution detail for [reorganisation-plan.md](reorganisation-plan.md). Every task below is
concrete: which file, what change, how you know it worked.

**Two rules apply to every phase below, and to anyone — human or agent — executing them:**

1. **No git operation that changes repository state without an explicit request.** No
   `git add`/`commit`/`push`/`stash`/`rebase`/`reset`/`merge`/`tag`, no `gh pr create`.
   Completing a task is not a trigger to commit. This matters most in Phase 1, which
   involves history rewrites and a force-push.
2. **No AI attribution in anything the project produces** — no `Co-Authored-By:` trailers
   naming a tool, no "Generated with", no 🤖, in commit messages, PR bodies, code comments,
   documentation or release notes.

Both are stated in full, with explicit command and forbidden-string lists, in
[agents-and-testing.md](agents-and-testing.md), and P2-15 puts them at the top of `AGENTS.md`.

**Branching (decided 2026-08-12)**

The whole reorganisation runs on the long-lived branch **`20260812-002`**, which already
carries Phase 1. `master` is not touched until every phase is complete.

- One phase = one or more commits on this branch, **not** a PR per phase. Phase gates still
  apply — they just gate the next batch of commits rather than the next merge.
- `master` is currently **0 commits ahead**, so the final landing stays a clean fast-forward:
  `git checkout master && git merge --ff-only 20260812-002`. Keep it that way — anything
  committed to `master` during the reorganisation forces a real merge and has to be ported
  across the re-layout by hand.
- **No CI runs on this branch.** The three inherited workflows sit at `docs/.github/`,
  `fx3/.github/` and `gui/.github/`, and GitHub only reads the repository root, so nothing
  triggers. That is convenient while paths are moving — no red
  crosses from workflows pointing at directories that no longer exist — but it means there is
  no automated safety net until P7-1 creates the root workflow. Until then, verification is
  local and manual.
- Consequence for anyone cloning: `master` still has the old submodule layout, including the
  `git clone --recursive` instructions that fail without an SSH key (D11). That stays true
  for the duration.

**Conventions**

- Each phase has a **gate**. Do not start the next phase's commits until the gate passes on
  `20260812-002`.
- Tasks are `P<phase>-<n>`. Size is S (< 1 hr), M (a few hours), L (a day or more, or
  blocked on hardware/external answers).
- `HW` marks a task that cannot be signed off without testing on real hardware.

**Dependency order**

```
P0 (decisions) ──> P1 (history merge) ──> P2 (layout + fixes) ──┬──> P3 (nix core + easy flakes) ──> P7 (CI) ──> P8
                                                                 ├──> P4 (docs flake)
                                                                 ├──> P5 (fx3 firmware flake)   [needs P0-2]
                                                                 └──> P6 (fpga flake)           [needs P0-3]
```

P3–P6 are independent of each other once P2 lands; they can be done in any order or in
parallel. P7 needs whichever flakes exist.

## Defect register

Ten issues found during the survey, plus the two found while writing this plan, plus those
found during execution (D19). Each is assigned to a phase.

| # | Issue | Where | Phase | Status |
| --- | --- | --- | --- | --- |
| D1 | CI `working-directory: fx3-firmware`, actual path `fx3/fx3-firmware` — job cannot pass | `firmware/.github/workflows/build-firmware.yml` | P2-3 | **Closed** P2 |
| D2 | Two near-identical CMake front-ends; only one is used | `gui-app/CMakeLists.txt`, `gui-app/tools/CMakeLists.txt` | P2-4 | **Closed** P2 |
| D3 | `install(FILES ... DESTINATION /etc/udev/rules.d)` escapes the install prefix | `fx3-programmer/CMakeLists.txt` | P2-5 | **Closed** P2 |
| D4 | `git rev-parse` at configure time → version silently becomes `unknown` in any sandbox | `fx3-firmware/CMakeLists.txt` | P2-6 | **Closed** P2 |
| D5 | `elf2img` built via `ExternalProject_Add` (nested configure, rebuilt per firmware build) | `fx3-firmware/CMakeLists.txt` | P2-7 | **Closed** P2 |
| D6 | ~45 MB of unreferenced SDK library profiles committed | `cyfx3sdk/fw_lib/1_3_5/fx3_{debug,profile_debug,profile_release}` | P2-8 | **Closed** P2 |
| D7 | `version.c` is dead code: not in `C_SOURCES`, and `#include "version.h"` — that header does not exist anywhere in the tree | `fx3-firmware/firmware/version.c` | P2-6 | **Closed** P2 |
| D8 | `TOSTRING(FIRMWARE_GIT_COMMIT)` double-stringifies an already-quoted macro, so `firmware_version_string` reads `Domesday Duplicator ("abc12345")` with literal quote characters. **Confirmed by build:** the symbol is 0x21 (33) bytes — the length *with* the stray quotes. **Latent only** — it is unreferenced and `--gc-sections` discards it, so it never reaches the device | `fx3-firmware/firmware/usb-descriptor.c:233` | P2-6 | **Closed** P2 |
| D9 | `remote_theme: just-the-docs/just-the-docs` fetches the theme over the network at build time — impossible in a Nix sandbox | `docs/wiki-default/_config.yml:3` | P4-4 (dissolved by the MkDocs move) | **Closed** P4 |
| D10 | `baseurl: "/DomesdayDuplicator-docs"` hard-codes the *old repo's* Pages URL; after the merge the site moves and every external link to it breaks | `docs/wiki-default/_config.yml:4`, `README.md:3,37` | P0-4, P4-8 | **Closed** P4 |
| D11 | Three of four submodule URLs are SSH, so the README's `git clone --recursive` fails without a GitHub key | `.gitmodules` | P1 (dissolved) | **Closed** P1 |
| D12 | `build-local.sh` injects front matter that `_config.yml` `defaults:` already supplies, and its error message names `jekyll-theme-cayman` — a theme the site no longer uses | `docs/build-local.sh` | P4-4 (dissolved by the MkDocs move) | **Closed** P4 |
| D13 | Permanent (EEPROM/SPI) programming needs a Cypress secondary loader, `cyfxflashprog.img`. **File now vendored** (2026-08-12) at the programmer's directory root, where the existing `../cyfxflashprog.img` candidate finds it from `build/`. **Code half remains:** every candidate path is working-directory-relative, so installed binaries still cannot locate it | `fx3-programmer/src/fx3-programmer.c:136` | P2-10, P3-3 | **Closed** P2 (hardware check in P5) |
| D14 | Four qmake `.pro` files duplicate the CMake build definition and exist only for Qt Creator; `BUILD.md` steers contributors to them | `gui-app/tools/**/*.pro` | P2-11 | **Closed** P2 |
| D15 | No `CMAKE_EXPORT_COMPILE_COMMANDS` anywhere, so no `compile_commands.json` and no working clangd in any editor | all `CMakeLists.txt` | P2-12 | **Closed** P2 |
| D16 | Sole `.editorconfig` is buried in `gui-app/tools/DomesdayDuplicator/`; `.vscode`/`.idea` ignore rules exist only in `gui-app/.gitignore` | repo root | P2-13 | **Closed** P2 |
| D17 | The two licence names are **transposed**: `LICENSE` is GPLv3 and the hardware file is CC BY-SA 4.0, but the README labels software as CC BY-SA (linking to the GPLv3 file) and hardware as GPLv3 (linking to the CC BY-SA URL) | `README.md` licence block | P2-14 | **Closed** P2 |
| D18 | No test infrastructure of any kind — no `enable_testing()`, `add_test()`, GoogleTest, Catch2 or QTest anywhere in the tree | repo-wide | P3-6 | **Closed** P3 |
| D19 | udev rules match Cypress VID `04b4` only, so the device is root-only once firmware is loaded and it re-enumerates as `1d50:603b` — the capture GUI cannot open it. Both rules also `RUN+=` a `cy_renumerate.sh` that is never installed and belongs to a daemon this project does not ship | `fx3/programmer/configs/88-cyusb.rules` | P3-3 | **Closed** P3 |
| D20 | Raw `<img src="assets/…">` tags are passed through by MkDocs without path rewriting, so under directory URLs they resolve one level too shallow and 404. `--strict` cannot detect this because MkDocs never parses those paths — 18 tags across 3 pages were silently broken | `docs/content/{general,ordering}/*.md` | P4-10 | **Closed** P4 |
| D21 | The GUI carries no version information: `project(DomesdayDuplicator VERSION 1.0)` is hardcoded and no commit hash reaches the binary or the About dialog, so a released GUI artefact cannot be traced back to the commit that produced it | `gui/CMakeLists.txt`, `gui/src/DomesdayDuplicator/aboutdialog.cpp` | P5-6 | Open |

---

## Tool acquisition strategy

The crux of the Nix work. Five mechanisms, in descending order of preference:

| Mechanism | Reproducible | Cacheable | Use when |
| --- | --- | --- | --- |
| nixpkgs package | yes | yes | The tool is free and packaged (`kicad`, `qt6`, `gcc-arm-embedded`, `libusb1`, `ruby`) |
| nixpkgs unfree package | yes | **no** (`redistributable = false`) | Vendor publishes an installer nixpkgs can fetch unattended (`quartus-prime-lite`) |
| `fetchurl`/`fetchzip` FOD | yes | yes | Artefact is at a stable public URL with no click-through |
| **Vendored in-tree** | yes | yes | Download needs a login or the artefact may vanish (**CyFX3 SDK**) |
| `requireFile` | yes | yes, once added | Redistribution is *not* permitted — user must fetch it and `nix store add` it manually |

Applied to this project:

### Quartus Prime Lite — nixpkgs unfree

Verified from `pkgs/by-name/qu/quartus-prime-lite/quartus.nix` in current nixpkgs:
`platforms = [ "x86_64-linux" ]`, `license = unfree`, `redistributable = false`, version
25.1. It accepts `supportedDevices` (default six families, of which we need only
`"Cyclone IV"` for the EP4CE22F17C6) and `withQuesta ? true` (we need `false`).

*How it is brought in:* `import nixpkgs { config.allowUnfree = true; }` **inside the fpga
flake itself**, so consumers need neither `--impure` nor `NIXPKGS_ALLOW_UNFREE`. The
installer is fetched from Altera on first build and cannot come from any binary cache, so
budget a multi-gigabyte, slow first build. Overriding `supportedDevices` to a single family
removes five component downloads and is the single biggest size win available.

*Risk:* Altera withdraws the 25.1 installer and the FOD hash stops resolving. Mitigation is
documentation, not code — `fpga/README.md` describes the manual-install fallback, and the
dev shell matters more than the packaged bitstream build.

### CyFX3 SDK 1.3.5 — stays vendored

**There is no CyFX3 SDK package in nixpkgs.** Verified by searching for `cyfx3`, `fx3`,
`cypress`, `cyusb`, `ez-usb` and `infineon`. What exists is adjacent but not a substitute:

| Package | What it is | Use to us |
| --- | --- | --- |
| `fxload` 1.0.29 | Host-side firmware uploader, `-t` accepts `fx3` | Uploads to RAM only — see below |
| `libfx2`, `python3Packages.fx2` 0.14 | Chip support for EZ-USB **FX2** | None — different chip family, different SDK |

Nothing packages the parts that actually matter for building: the ARM libraries
(`libcyfxapi.a`, `libcyu3threadx.a`, `libcyu3lpp.a`), the headers in `fw_lib/1_3_5/inc`, the
`fx3.ld` linker script, or `elf2img`. That absence is structural rather than an oversight —
Infineon's download is behind a login, so nixpkgs has no URL it can fetch unattended, and
nixpkgs does not vendor multi-megabyte binary blobs. The nixpkgs-idiomatic answer for exactly
this situation is `requireFile`, which is the fallback below.

So the SDK stays in-tree (71 MB): `fw_build/fx3_fw/fx3.ld`,
`fw_lib/1_3_5/{inc,fx3_release,…}`, `util/elf2img/`.

**P0-2 decided this: vendor it regardless of the licence review**, on the basis that the SDK
is already widely mirrored. The maintainer refreshes it from the official vendor download —
target paths, required subtrees and the two preservation traps are in
[decisions.md](decisions.md). Two points that matter for the Nix work:

- The build's `FATAL_ERROR` checks make the required subset explicit:
  `fw_lib/1_3_5/inc`, `fw_lib/1_3_5/fx3_release` and `fw_build/fx3_fw/fx3.ld`. Only
  `libcyfxapi.a`, `libcyu3threadx.a` and `libcyu3lpp.a` are linked, out of the seven archives
  shipped.
- `util/elf2img/CMakeLists.txt` is **project-authored** (added in `19bdb88`), not vendor.
  Overwriting `util/elf2img/` wholesale from the SDK deletes it and breaks the firmware
  build — which matters to P5-1, since that file is the derivation's build system.

Ship the vendor's own `license/license.txt` as `fx3/sdk/LICENSE.txt` alongside it — every SDK
header points at that path, so including it makes the reference resolve — plus a
`fx3/sdk/README.md` recording version, origin URL and refresh date.

*Retained for reference only, not the path taken.* Had redistribution been ruled out, the
SDK would have become a `requireFile` derivation:

```nix
cyfx3sdk = pkgs.requireFile {
  name = "cyfx3sdk-1.3.5.tar.gz";
  sha256 = "…";
  message = ''
    The Cypress/Infineon FX3 SDK cannot be redistributed. Download
    "EZ-USB FX3 SDK 1.3.5 for Linux" from Infineon (login required), then:
      nix store add-file cyfx3sdk-1.3.5.tar.gz
  '';
};
```

…with the firmware build becoming opt-in and absent from CI. Since P0-2 chose vendoring, the
firmware build stays in `nix flake check` and in CI as normal.

`elf2img` is a single 13 KB C file with a project-authored `CMakeLists.txt` — it becomes its
own tiny derivation regardless of which path is taken (D5).

### `cyfxflashprog.img` — a second Cypress artefact, and it is missing (D13)

Found while checking whether `fxload` could replace the project's own programmer. It cannot,
and the reason matters:

`fx3-programmer` does three things — RAM download (`0xA0`), I2C EEPROM programming (`0xBA`/
`0xBB`) and SPI flash programming (`0xC2`/`0xC4`). The latter two are the ones that make
firmware *persist* across power cycles, and they work by first downloading a Cypress
**secondary loader**, `cyfxflashprog.img`, into RAM and then talking to it. `fxload` only
does the RAM path, so it covers the volatile case and not the production one. Keep the
project's programmer.

**Update 2026-08-12: the file is now vendored** at `fx3-programmer/cyfxflashprog.img`,
extracted from the SDK archive's `cyusb_linux_1.0.5` component (LGPL-2.1). That position is
chosen so the existing `../cyfxflashprog.img` candidate resolves when the tool runs from
`build/`. The code problem described below is unchanged and P2-10 still applies.

The original problem: **`cyfxflashprog.img` was not in this repository.**
`find_flashprog_image()` (`src/fx3-programmer.c:136`) looks for it via `$FX3_FLASH_PROG`,
then at six CWD-relative paths, three of which point at a sibling `cyusb_linux` checkout:

```c
"cyfxflashprog.img",
"../cyfxflashprog.img",
"../../../../../cyusb_linux/fx3_images/cyfxflashprog.img",
…
```

Neither `README.md` nor `BUILD.md` mentions the file or the environment variable, so
permanent programming currently works only for someone who happens to have `cyusb_linux`
checked out next door. CWD-relative lookup also cannot work for *any* installed binary — Nix,
`.deb`, or otherwise.

Unlike the SDK, this artefact is publicly downloadable: the README already cites
[Cypress-Semiconductor/cyusb_linux](https://github.com/Cypress-Semiconductor/cyusb_linux) on
GitHub, with no login. That makes a `fetchFromGitHub` FOD viable — subject to the same
licence check as P0-2, which should cover both artefacts in one pass.

Fix (P2-10 + P3-3): add `$out/share/domesday-duplicator/cyfxflashprog.img` to the candidate
list, ahead of the relative paths and behind `$FX3_FLASH_PROG`, and have the derivation
install the image there. Document the variable either way.

### GPIF II Designer — not a build dependency

`fx3/firmware/gpif/DomesdayDuplicator.cydsn/` is a Cypress GPIF II Designer project
(Windows-only, proprietary). Its *output*, `domesday-duplicator-gpif.h`, is committed and is
what the build consumes. So there is nothing to package: the designer is a design-time tool
used a handful of times per decade. P2-9 documents it as a manual, Windows/Wine-only step
and records which `.cydsn` revision produced the committed header.

### Documentation theme — nixpkgs Python packages (D9)

The current `remote_theme: just-the-docs/just-the-docs` is a `jekyll-remote-theme` feature
that clones the theme from GitHub *at build time*. It works under
`actions/jekyll-build-pages` and cannot work in a Nix sandbox.

Rather than work around it, the site converts to **MkDocs + Material +
`mkdocs-awesome-nav`**, matching decode-orc. All three are ordinary nixpkgs Python packages
(`mkdocs` 1.6.1, `mkdocs-material` 9.7.6, `mkdocs-awesome-nav` 3.3.0), pulled in with
`python312.withPackages`. Nothing is fetched at build time, and there is no Ruby gem set to
author or keep in sync with GitHub's `github-pages` bundle. Full detail:
[docs-theme-migration.md](docs-theme-migration.md).

---

## Release artefacts and provenance

**Requirement (maintainer, 2026-08-12):** every artefact a user needs — FPGA bitstream, FX3
firmware, GUI application — is produced by CI, and *a release contains exactly the artefacts
built from the release commit*. Not "a build of roughly that source": the specific binaries
produced at that pin.

This section defines the model. The tasks that implement it live in Phases 5, 6 and 7.

### 1. What a release contains

| Artefact | Built by | Platforms | Cadence |
| --- | --- | --- | --- |
| `DomesdayDuplicator`, `dddutil`, `dddconv` | CI | Linux x64, Linux ARM64, Windows x64, macOS x64, macOS ARM64 | **Every commit** |
| `firmware.img`, `firmware.elf`, `firmware.map` | CI (`nix build .#fx3-firmware`) | n/a (cross-compiled) | **Every commit** |
| `fx3-programmer` | CI (`nix build .#fx3-programmer`) | Linux x64, ARM64 | **Every commit** |
| Documentation site | CI (`nix build .#docs-site`) | n/a | Every commit to `master` |
| `DomesdayDuplicator.sof` / `.jic` | **Local build, attached manually** | n/a | Per release |

The FPGA bitstream is the exception, and §4 explains why.

### 2. Why "the exact artefacts" and not "rebuild from the tag"

For the GUI and the FX3 firmware, rebuilding from the tag gives the same result — the Nix
derivations pin their inputs, so given the same `flake.lock` the output is reproducible.

**For the FPGA, the position is more nuanced than an earlier draft of this document claimed.**
That draft said "Quartus is not bit-reproducible, because place-and-route seeds and timestamps
vary". The seed part is **wrong**: the Fitter seed is a fixed project setting (default 1), not
something that varies between runs. Correcting it, from Altera's own guidance and from
published work on Cyclone bitstream formats:

- **Fitting is deterministic.** For a given Fitter seed and `Maximum processors allowed`
  setting, the fit is the same run to run, *independent of the machine and its core count*.
  Same source and same seed means same placement and same routing.
- **The caveats are about the toolchain, not the run.** Identical results require the same
  Quartus version, the same 32/64-bit build, and the same CPU architecture — floating-point
  differences across architectures can perturb results. Change any of those and you get
  "seed noise": a different but equivalent-quality fit.
- **File byte-identity is a separate question.** Quartus embeds a compile timestamp in the
  bitstream header, so two runs can produce identical *configuration content* inside
  non-identical *files*. The configuration state machine never reads those header bytes.

So the honest statement is: **the configuration content is reproducible given a pinned
toolchain; the `.sof` file may not be byte-identical.** This has not been verified for this
project — Quartus is not installed here and Phase 6 has not run. **P6-9 makes it an
experiment** rather than an assumption.

The archive-the-artefact model survives, but on weaker and different grounds than that draft
claimed. It is not "you cannot regenerate this". It is:

- regenerating requires pinning an unfree, GB-scale, `x86_64-linux`-only toolchain, which is
  a much higher bar than `nix build` from a lock file; and
- if the file is not byte-identical, a plain hash comparison against a rebuild **fails even
  when the rebuild is correct** — so hash equality is the wrong verification method unless
  the digest is taken over a canonical form (§5).

So: **build once, archive the result, and publish digests that make the artefact verifiable.**
The requirement is stricter than "CI builds things" — artefacts must be *retained and
published*, not produced and discarded after the 30-day `actions/upload-artifact` window.

### 3. Traceability: every artefact names its commit

An archived artefact is only useful if you can tell which commit produced it. Current state:

| Component | Carries the commit? |
| --- | --- |
| FX3 firmware | **Yes** — `FIRMWARE_VERSION` reaches the USB product descriptor, so `lsusb -v` on a running device reports it (D4, fixed in P2-6) |
| GUI | **No** — `project(DomesdayDuplicator VERSION 1.0)` is hardcoded and there is no commit stamp anywhere. **D21** |
| FPGA bitstream | **No** — nothing in the `.sof`/`.jic` identifies the source |

P5 and P7 close the GUI gap. The FPGA gap is handled by recording provenance alongside the
artefact rather than inside it (§4).

Every release also publishes a `SHA256SUMS` manifest and a short provenance note: the commit,
the `flake.lock` nixpkgs revision, and — for the bitstream — the Quartus version and the
machine that built it.

### 4. The FPGA bitstream stays out of CI, for now

**Decided 2026-08-12: leave the FPGA firmware out of CI for the time being.** The bitstream
continues to be built locally and attached to releases by hand, as the plan already had it.

The blocker is not technical difficulty, it is cost and a licence judgement. Recorded so the
decision can be picked up later without re-deriving it:

- `quartus-prime-lite` is `x86_64-linux` only, unfree, and **`redistributable = false`** — so
  it can never be served from `cache.nixos.org`. Every cold CI run must fetch it afresh.
- The fetch itself is unattended-friendly: plain `fetchurl` from
  `downloads.intel.com/akdlm/software/acdsinst/…`, no login, no click-through. So CI *can*
  do it.
- Restricted to `supportedDevices = [ "Cyclone IV" ]` the download is still GB-scale, and the
  unpacked store path is larger again — tight against a GitHub-hosted runner's ~14 GB disk.

The three ways it could reach CI, when the time comes:

| Option | Speed | Cost | Licence |
| --- | --- | --- | --- |
| **Self-hosted runner** with a warm Nix store | Minutes | A machine to run and maintain | Clean — nothing is redistributed |
| GitHub-hosted, fetch from Intel each run | 20–40 min | None | Clean — fetched from source |
| GitHub-hosted + private binary cache | 5–10 min | Cachix/S3 and credentials | **Judgement call** — `redistributable = false` is precisely about not redistributing these binaries |

**Intended shape when adopted:** GUI and FX3 per commit; the bitstream on tags and manual
dispatch only, so a release still gets a bitstream built from the release commit without
paying for Quartus on every push.

Until then, `docs-tech/` records the manual procedure and P8-3 covers attaching the artefact.

### 5. Bitstream digests: verifiable without building it in CI

Since the bitstream is not built by CI (§4), a release must still let someone confirm that
what they downloaded is what was built, and — ideally — that a rebuild of the tagged source
agrees with it. Two digests, because they answer different questions:

| Digest | Over | Answers |
| --- | --- | --- |
| **Release digest** | The shipped `.sof` and `.jic`, byte for byte | "Did I download the file that was released, intact and untampered?" |
| **Canonical digest** | The configuration payload with the header excluded | "Does a rebuild from this commit produce the same configuration, despite the embedded timestamp?" |

The release digest lands in `SHA256SUMS` alongside every other asset and costs nothing. The
canonical digest is what makes the FPGA artefact independently verifiable **without CI ever
running Quartus** — the maintainer builds locally, publishes both digests, and anyone with
the same pinned Quartus version can rebuild and compare the canonical one.

If P6-9 finds the `.sof` *is* byte-identical across rebuilds on a pinned toolchain, the
canonical digest becomes redundant and the release digest alone does both jobs. That is the
better outcome, and it is worth measuring before building machinery for the harder case.


---

## Phase 0 — Decisions and spikes

No code. Produces [decisions.md](decisions.md) — the decision log, with evidence and
rationale per entry. Everything after this depends on these answers.

**Status: all decisions taken (2026-08-12).** Only P0-3's hardware verification is
outstanding, and that is a Phase 6 gate rather than a blocker — **Phase 1 can begin.**

| | Outcome |
| --- | --- |
| P0-1 | Land `fpgaupdate-202512` before Phase 1; leave `release-2.x` untouched. Import **only each submodule's default branch**, no other branches and no tags |
| P0-2 | **Vendor the Cypress SDK regardless of the licence review.** Refresh from the vendor download; placement instructions in [decisions.md](decisions.md) |
| P0-3 | Accept Quartus 25.1 (already done on `fpgaupdate-202512`); hardware verification deferred to the P6-5 gate |
| P0-4 | Move docs to `/domesdayduplicator`; **no** redirect stub at the old URL |
| P0-5 | Accept ~400 MB of history; no blob pruning, no LFS |
| P0-6 | **Leave the four old repositories alone** — not archived, not deleted; cleaned up separately, outside this plan |
| P0-7 | Nix-on-macOS is best-effort, not a CI gate |

| Task | Size | Detail |
| --- | --- | --- |
| Task | Size | Status |
| --- | --- | --- |
| **P0-1** Outstanding branches | S | **Decided.** Land `fpgaupdate-202512` (4 ahead, 0 behind — a clean fast-forward carrying the Quartus 25.1 upgrade, a hand-written FIFO replacing the Intel `dcfifo` IP, and a 333-line testbench). Leave `release-2.x` alone: 2022-era, pre-split flat layout, content already in `master`. **Action outstanding:** fast-forward `firmware`'s `master`, push, update the superproject pointer — all *before* Phase 1 |
| **P0-2** Cypress SDK | M | **Decided.** Vendor it regardless of the licence review — it is already widely mirrored. **Action outstanding:** refresh from the official download into `firmware/fx3/fx3-firmware/cyfx3sdk/` (later `fx3/sdk/`), preserving the project-authored `util/elf2img/CMakeLists.txt` and the `fw_lib/1_3_5/` version path. Exact layout in [decisions.md](decisions.md) |
| **P0-3** Quartus version | M, HW | **Decided (verification outstanding).** 25.1 accepted — the upgrade already exists on `fpgaupdate-202512`. What remains: run the capture-integrity procedure on real hardware, since that branch's last commit says "need to test". A **Phase 6 gate**, not a blocker |
| **P0-4** Docs site URL | M | **Decided.** Move to `simoninns.github.io/domesdayduplicator`; **no redirect stub**. Old deep links will 404 — accepted. Simplifies P4-8 to `site_url` plus four in-repo links |
| **P0-5** History size | S | **Decided.** Accept ~400 MB. `filter-repo` does path prefixing only — no `--strip-blobs-bigger-than`, no LFS |
| **P0-6** Old repositories | S | **Decided.** Leave all four alone — not archived, not deleted. The maintainer will clean them up separately. **P8-1 is removed from this plan** |
| **P0-7** macOS support | S | **Decided (default).** Expose `packages.aarch64-darwin.gui`; do not gate CI on it. The existing Homebrew macOS jobs remain the authoritative coverage |

**Gate:** [decisions.md](decisions.md) records all seven — **met.** Two maintainer actions
carry into Phase 1 prep: fast-forward the firmware submodule (P0-1) and refresh the SDK
(P0-2). P0-3's hardware verification is a Phase 6 gate.

## Phase 1 — Merge the submodules — **DONE**

Executed 2026-08-12 on branch `20260812-002`, branched from `20260812-001` at `bcd53a0`, and
since pushed to `origin`. `master` is untouched.

Imported from the **local** submodule object stores at the superproject's pinned commits, per
P0-1's revision — not from GitHub, and not at the remote tips. Sources were reduced to a
single ref each before rewriting, so no other branch or tag entered the monorepo.

| | Pinned | Commits | Result |
| --- | --- | --- | --- |
| `docs` | `9e57a729` | 19 | `docs/` |
| `firmware` | `83a98bbc` | 115 | `firmware/` |
| `gui-app` | `8036eaf1` | 299 | `gui-app/` |
| `hardware` | `76099311` | 40 | `hardware/` |

Result: **936 commits** (458 + 19 + 115 + 299 + 40 + 1 removal + 4 merges), `.gitmodules`
deleted, no gitlink entries remain.

Verification, all passing:

- `diff -r` — all four trees identical to their import sources
- **Blob-for-blob** comparison against the original `.git/modules/<m>` trees at each pinned
  SHA — identical, so the rewrite changed paths only
- `git log --follow` walks through the rewrite: 96 commits on `mainwindow.cpp`, 28 on
  `DomesdayDuplicator.v`, 15 on the KiCad schematic, 6 on `docs/wiki-default/index.md`
- Clean `git clone` with **no `--recursive` and no SSH key** yields the complete tree —
  the point of the whole exercise, and the fix for D11
- Clone size **180 MB**, well under the ~400 MB estimate, because only single branches and no
  tags were imported

Rollback: `git checkout 20260812-001 && git branch -D 20260812-002`. Pre-merge HEAD was
`bcd53a0`; `origin` is untouched.

Still untracked, deliberately left for a separate commit: `docs-tech/` and the five vendored
Cypress files.

Full procedure in [submodule-migration.md](submodule-migration.md). Summary: tag each
submodule `pre-monorepo`, rewrite each with
`git filter-repo --to-subdirectory-filter <path>`, merge each into the superproject with
`--allow-unrelated-histories`, delete `.gitmodules`. Paths do **not** change in this phase.

**Gate:**

- `git clone` with no `--recursive` and no SSH key yields a complete tree (fixes D11).
- `git log --follow` works on a file from each former submodule.
- `diff -r` against the pre-merge checkouts is empty for all four.
- Merged to `master` with a **merge commit** — squash or rebase destroys the imported
  histories.

## Phase 2 — Re-layout and defect fixes — **DONE**

Executed 2026-08-12 on `20260812-002`. All fifteen tasks complete; **D1–D8 and D13–D17 are
closed**. Four deviations from the plan as written are recorded at the end of this section.

The renames must land in a single commit so `git log --follow` stays usable; the fixes can be
separate commits on the same branch.

### P2-1 Directory renames (M)

Use `git mv` so rename detection works:

```
firmware/DE0-NANO/DomesdayDuplicator/  →  fpga/src/
firmware/fx3/fx3-firmware/firmware/    →  fx3/firmware/src/
firmware/fx3/fx3-firmware/GPIF_II/     →  fx3/firmware/gpif/
firmware/fx3/fx3-firmware/cyfx3sdk/    →  fx3/sdk/
firmware/fx3/fx3-programmer/           →  fx3/programmer/
gui-app/tools/DomesdayDuplicator/      →  gui/src/DomesdayDuplicator/
gui-app/tools/dddconv/                 →  gui/src/dddconv/
gui-app/tools/dddutil/                 →  gui/src/dddutil/
gui-app/tools/cmake_modules/           →  gui/cmake/
hardware/KiCAD/Domesday Duplicator/    →  hardware/pcb/
hardware/Documentation/                →  hardware/doc/
docs/wiki-default/                     →  docs/content/
docs/*.sh                              →  (deleted in P4-4, not moved)
```

`docs/content/`, **not** `docs/site/`: MkDocs' `site_dir` defaults to `site`, so a `docs_dir`
of that name collides and the build refuses to run. P4-1 reorganises the content *within*
`content/` to match the navigation.

The `hardware/pcb/` rename removes the space from the path, which currently forces quoting in
every script that touches it.

### P2-2 Path fixups after the renames (M)

Every `add_subdirectory`, `CMAKE_MODULE_PATH`, `CYFX3SDK_PATH` default, `.qsf`/`.qpf`/`.cof`
relative reference, `fp-lib-table` entry, and README link. Verify with a from-scratch build of
each component using the *existing* non-Nix instructions — P2 must not require Nix.

### P2-3 Fix the firmware CI paths (S) — D1

`working-directory: fx3-firmware` → `fx3/firmware`; `fx3/fx3-programmer` → `fx3/programmer`;
update the artefact paths in the `Verify build artifacts` and upload steps. This workflow
gets replaced wholesale in P7, but it should not be left knowingly broken in the interim.

### P2-4 Dedup the GUI CMake front-ends (S) — D2

Delete `gui-app/tools/CMakeLists.txt`; keep the root one at `gui/CMakeLists.txt` (it has the
higher `cmake_minimum_required`, the explicit `Qt6 6.2` constraint and the
`qt_standard_project_setup` fallback). Update `CMAKE_MODULE_PATH` to `${CMAKE_CURRENT_LIST_DIR}/cmake`.

### P2-5 Fix the udev rule install (S) — D3

`fx3/programmer/CMakeLists.txt`:

```cmake
-install(FILES configs/88-cyusb.rules DESTINATION /etc/udev/rules.d)
+install(FILES configs/88-cyusb.rules DESTINATION lib/udev/rules.d)
```

Add a note in `fx3/programmer/README.md` that non-Nix packagers should symlink or copy from
`$PREFIX/lib/udev/rules.d`. Verify with
`cmake --install build --prefix /tmp/x` — nothing must appear outside `/tmp/x`.

### P2-6 Fix firmware version stamping (M) — D4, D7, D8

Three related problems in one change.

**D4** — make the version injectable:

```cmake
if(NOT DEFINED FIRMWARE_VERSION OR FIRMWARE_VERSION STREQUAL "")
  execute_process(COMMAND git rev-parse --short=8 HEAD
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  OUTPUT_VARIABLE FIRMWARE_VERSION
                  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(NOT FIRMWARE_VERSION)
  set(FIRMWARE_VERSION "unknown")
endif()
```

Keep passing it as `FIRMWARE_GIT_COMMIT="${FIRMWARE_VERSION}"` so no C changes are needed
beyond D8. P5 will pass `-DFIRMWARE_VERSION=<flake shortRev>`.

**D7** — delete `fx3/firmware/src/version.c`. It is not in `C_SOURCES`, and it
`#include`s a `version.h` that does not exist in the tree, so it has never compiled. The live
path is `usb-descriptor.c` consuming the generated `USB_DESC_PRODUCT_BYTES`. (If the intent
was to build the descriptor at *runtime* rather than at build time, that is a design change,
not a cleanup — raise it as an issue rather than reviving the file silently.)

**D8** — `usb-descriptor.c:233`. `FIRMWARE_GIT_COMMIT` is already a string literal, so
`TOSTRING()` stringifies it a second time and yields `Domesday Duplicator ("abc12345")` with
literal quote characters:

```c
-const char firmware_version_string[] = "Domesday Duplicator (" TOSTRING(FIRMWARE_GIT_COMMIT) ")";
+const char firmware_version_string[] = "Domesday Duplicator (" FIRMWARE_GIT_COMMIT ")";
```

The `STRINGIFY`/`TOSTRING` macros then have no remaining users and can go.

**Scope, confirmed by building the firmware (2026-08-12):** the USB descriptor the host
actually sees comes from `USB_DESC_PRODUCT_BYTES` and is correct. `firmware_version_string`
is not merely harmless but *unreferenced* — the linker map lists
`.rodata.firmware_version_string` at 0x21 (33) bytes under **Discarded input sections**, so
`-ffunction-sections -fdata-sections -Wl,--gc-sections` strips it entirely. 33 bytes is
exactly the length with the two stray quote characters, which both proves the defect and
proves it never ships. Treat D8 as a latent source defect, not a field bug.

*Verification:* after the fix, `.rodata.firmware_version_string` should be 31 bytes if it is
ever referenced again. The meaningful check for D4 is separate — `lsusb -v` on a flashed
device showing the real commit hash rather than `unknown`, which exercises the
`USB_DESC_PRODUCT_BYTES` path.

### P2-7 Split out `elf2img` (S) — D5

Delete the `ExternalProject_Add(elf2img_build …)` block. Locate the tool with
`find_program(ELF2IMG elf2img)` and fall back to `add_subdirectory(../sdk/util/elf2img)` for
non-Nix builders, so the standalone CMake build keeps working. The custom command then invokes
`${ELF2IMG}` instead of `${CMAKE_CURRENT_BINARY_DIR}/tools/bin/elf2img`.

### P2-8 Prune unused SDK profiles (S) — D6

Delete `fx3/sdk/fw_lib/1_3_5/{fx3_debug,fx3_profile_debug,fx3_profile_release}` — ~45 MB, and
`CMakeLists.txt` only ever references `fx3_release`. Shrinks the checkout, not the history
(history pruning was P0-5's call, and had to happen in P1).

### P2-10 Make the flash-programmer image findable (M) — D13

Add a compiled-in install path to `find_flashprog_image()`'s candidate list, so an installed
binary can locate the secondary loader without a same-directory copy:

```c
const char *candidates[] = {
    env,                                    /* $FX3_FLASH_PROG wins */
    FLASHPROG_INSTALL_PATH,                 /* -DFLASHPROG_INSTALL_PATH from CMake */
    "cyfxflashprog.img",
    …                                       /* existing relative paths, kept for in-tree use */
};
```

with `target_compile_definitions(... FLASHPROG_INSTALL_PATH="${CMAKE_INSTALL_FULL_DATADIR}/domesday-duplicator/cyfxflashprog.img")`
and a matching `install(FILES …)`. Then vendor the image (`fetchFromGitHub` from
`Cypress-Semiconductor/cyusb_linux`, subject to P0-2's licence check) or commit it.

Also: emit a diagnostic naming `$FX3_FLASH_PROG` when the file is not found — the current
failure mode gives the user nothing to act on — and document the requirement in
`fx3/programmer/README.md`, which does not currently mention it at all.

*Verification:* `cmake --install` to a clean prefix, `cd /` and run a flash operation. It
must find the image with no `cyusb_linux` checkout anywhere and no `FX3_FLASH_PROG` set.

### P2-11 Delete the qmake project files (S) — D14

`gui/src/**/*.pro` and `gui/src/ddd-tools.pro` are a second build definition maintained only
for Qt Creator, and `BUILD.md` currently instructs contributors to open them. They will drift
from `CMakeLists.txt` — the same failure mode as D2, one level up. Delete all four and update
`BUILD.md`; Qt Creator opens CMake projects natively, so its users lose nothing.
(`hardware/pcb/Domesday Duplicator.pro` is a KiCad file that shares the extension — leave it.)

### P2-12 Emit `compile_commands.json` (S) — D15

`set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` in each component's top-level `CMakeLists.txt`;
symlink `build/compile_commands.json` to the component root; gitignore it and
`.cache/clangd/`. Add `fx3/firmware/.clangd` pinning `Compiler: arm-none-eabi-gcc` so clangd
does not analyse bare-metal ARM sources as host code. See
[ide-independence.md](ide-independence.md) §5.

### P2-13 Root editor-neutral configuration (S) — D16

Promote `gui-app/tools/DomesdayDuplicator/.editorconfig` to the repo root and extend it to
Verilog, CMake, Python and YAML. Promote `gui-app/.gitignore`'s `.vscode/`, `.idea/`,
`*.user`, `*.creator.user*` entries to the root `.gitignore`, and add Eclipse's `.project`,
`.cproject`, `.settings/`, `*.launch` so they cannot drift back into the FX3 tree. Add an
`.envrc` containing `use flake`. See [ide-independence.md](ide-independence.md) §6.

### P2-14 Fix the transposed licence statements (S) — D17

`README.md`'s licence block names software as CC BY-SA 4.0 (while linking to `LICENSE`,
which is GPLv3) and hardware as GPLv3 (while linking to the CC BY-SA URL). The actual
position, confirmed from the files and the source headers: **software is GPLv3, hardware is
CC BY-SA 4.0**. Swap the names so each matches its link. Do this before P2-15, so AGENTS.md
does not codify the error.

### P2-15 Author `AGENTS.md` (M)

Per [agents-and-testing.md](agents-and-testing.md) §2, describing the post-reorganisation
layout. **The two non-negotiable rules go first**, before any architecture or testing
content:

1. **No automatic git operations.** No `git add`/`commit`/`push`/`stash`/`rebase`/`reset`/
   `merge`/`tag`, and no `gh pr create`/`merge`, unless the user explicitly asks for that
   action in that message. Read-only git is always fine. Completing a task is not a trigger
   to commit; permission for one commit does not carry to the next.
2. **No AI attribution anywhere.** No `Co-Authored-By:` trailers naming a tool, no
   "Generated with", no 🤖, no tool or vendor branding — in commit messages, PR bodies, code
   comments, documentation or release notes. Commit messages describe the change, not how it
   was written.

Both need the explicit command and forbidden-string lists from
[agents-and-testing.md](agents-and-testing.md); the abbreviated phrasing is read loosely, and
several assistants add attribution trailers by default unless told not to. Rule 1 matters
especially during Phase 1, where an unrequested commit or push lands in the middle of a
history rewrite and force-push.

### P2-9 Documentation sweep (M)

- `README.md`: drop the `git clone --recursive` block and the submodule list. Add a short
  note that this repository is now the only place to work — per P0-6 the four old
  repositories stay writable and unmarked, so contributors could otherwise keep pushing to
  them.
- `CONTRIBUTING.md`: it currently routes doc changes to the separate docs repo, which stays
  live and writable under P0-6 — so the redirect matters.
- `fx3/firmware/gpif/README.md`: record that the `.cydsn` needs GPIF II Designer
  (Windows-only), that `domesday-duplicator-gpif.h` is the committed output, and which
  revision produced it.
- `fx3/sdk/README.md`: SDK version, origin, and the P0-2 licence verdict.
- Add per-component `README.md` stubs; keep `BUILD.md`'s non-Nix instructions accurate.

### Deviations from the plan as written

**1. `elf2img` is host-compiled directly, not `add_subdirectory`d (P2-7).** The plan's
fallback cannot work: `CMAKE_TOOLCHAIN_FILE` applies to the entire build tree, so an
`add_subdirectory(../sdk/util/elf2img)` inside the firmware project would cross-compile the
tool for ARM and produce a binary that cannot run on the build host. That is precisely why
the original author reached for `ExternalProject_Add`. The implemented form keeps
`find_program(ELF2IMG elf2img)` as the primary path — which is what the P5 flake will
satisfy — and falls back to one `add_custom_command` invoking a host compiler found via
`find_program(HOST_CC NAMES cc gcc clang)` on the single self-contained `elf2img.c`. This
removes the nested configure that D5 objected to without introducing a broken binary.
`fx3/sdk/util/elf2img/CMakeLists.txt` is retained for standalone builds and for P5.

**2. `.envrc` is deferred to Phase 3 (P2-13).** The plan has P2-13 add `.envrc` containing
`use flake`, but no flake exists until P3-1. Committing it now gives every direnv user an
error on entering the directory. It lands with the root flake instead.

**3. `compile_commands.json` is located by `.clangd`, not by a committed symlink (P2-12).**
The plan called for symlinking `build/compile_commands.json` to each component root, but that
file is gitignored — it embeds absolute paths — so the symlink could not be committed and
would have to be recreated by hand after every clone. Each component instead carries a
`.clangd` with `CompilationDatabase: build`, which clangd resolves relative to the config
file. Same result, nothing generated, nothing to recreate. `fx3/firmware/.clangd` also sets
`Compiler: arm-none-eabi-gcc`, as the plan specified.

**4. The three inherited workflows moved with their components.** P2-1 did not say where
`firmware/.github/` and `gui-app/.github/` should go once those directories were dissolved.
They are now at `fx3/.github/` and `gui/.github/`, alongside what they build. All three remain
inert — GitHub only reads the repository root — until P7-1 replaces them. Their paths were
fixed anyway so none is knowingly broken, and two stale branch triggers
(`fxsdk-202512`, `automation001-202512`, both submodule-era branches that no longer exist)
were dropped.

### Gate — **met**

All three buildable components were built from source with the *existing* non-Nix
instructions, using nothing but a compiler, CMake and their declared dependencies:

| Component | Result |
| --- | --- |
| `fx3/firmware` | `firmware.elf`, `firmware.img`, `firmware.map` produced; `elf2img` host-compiled in one step |
| `fx3/programmer` | `fx3-programmer` links against libusb-1.0 |
| `gui` | `DomesdayDuplicator`, `dddconv`, `dddutil` all link against Qt 6.11 |

`cmake --install` to a clean prefix wrote **nothing outside that prefix** for either CMake
component — the D3 check.

Defect-by-defect evidence:

- **D4** — `cmake -DFIRMWARE_VERSION=deadbeef` produces `// Commit: deadbeef` in the
  generated descriptor; with the flag absent, the git fallback still yields the real hash.
- **D5** — configure log reads `elf2img: building the SDK copy with .../bin/cc`, and the
  build shows a single `Building host tool elf2img` step. No nested configure.
- **D8** — `.rodata.firmware_version_string` is now **0x1f (31) bytes** in the linker map,
  down from 0x21 (33): exactly the two stray quote characters removed, as predicted. It
  remains in *Discarded input sections*, so this was and is a latent source defect.
- **D13** — an installed `fx3-programmer` carries
  `/<prefix>/share/domesday-duplicator/cyfxflashprog.img` as a compiled-in candidate, and
  `make install` places the image there. Verified by extracting the string table of the
  installed binary and confirming the file exists at that path. The full flash operation
  needs hardware and is a Phase 5 check.
- **D15** — `compile_commands.json` is emitted by all three CMake components.

CI is not part of this gate: no workflow runs on this branch (see *Branching* above).

## Phase 3 — Nix foundation and the easy flakes — **DONE**

Executed 2026-08-12 on `20260812-002`. All seven tasks complete; **D18 is closed** — the
repository has automated tests for the first time, 44 of them across two components.

Deviations and findings are recorded after P3-7 below.

### P3-1 `nix/lib.nix` and the root flake (M)

`nix/lib.nix` exports `forAllSystems` / `forLinux` helpers plus the shared `pkgs` config.
Root `flake.nix` `callPackage`s each component's `package.nix` directly — no cross-flake
inputs. Structure and sketches: [nix-flake-design.md](nix-flake-design.md) §1.

### P3-2 `gui/` packaging (M)

`gui/package.nix` + `gui/shell.nix`, both reached from the root flake. `qt6Packages.callPackage` with
`wrapQtAppsHook` (without it the binary dies on the missing `xcb` platform plugin).
Use `lib.fileset` for `src` so README edits do not trigger rebuilds.
*Watch:* the hand-rolled `cmake/FindLibUSB.cmake` is the first suspect if configure fails —
`pkg-config` finds libusb-1.0 cleanly under Nix.

**Gate:** `nix build .#gui` then `./result/bin/DomesdayDuplicator` opens a window on a clean
machine; `nix develop .#gui` gives a working CMake build tree.

### P3-3 `fx3/programmer/` packaging + NixOS udev module (M)

Depends on P2-5 and P2-10. `nix/modules/udev.nix` exposing
`hardware.domesdayDuplicator.enable` → `services.udev.packages`. Sketch in
[nix-flake-design.md](nix-flake-design.md) §3. This is the biggest quality-of-life win for
NixOS users and is independent of everything else.

The derivation must install `cyfxflashprog.img` into
`$out/share/domesday-duplicator/` and point `FLASHPROG_INSTALL_PATH` at it (D13) — otherwise
`nix run .#fx3-programmer` can download to RAM but cannot program the EEPROM, which is the
operation users actually need.

**Gate:** `nix build .#fx3-programmer` succeeds; on a NixOS host with the module enabled, a
plugged-in FX3 gets the expected permissions, and a **flash** (not just RAM) operation
completes from the Nix-installed binary with no `cyusb_linux` checkout present (HW).

### P3-4 `hardware/` dev shell (S)

Dev shell with `kicad` only. Packaged `kicad-cli` export stays blocked on the KiCad 5 → 10
file-format migration (`kicad-cli` cannot read legacy `.sch`); file that as a follow-up
issue, not part of this plan.

**Gate:** `nix develop .#hardware` opens `hardware/pcb/` in KiCad.

### P3-5 Editor-agnostic tooling in the dev shells (M)

Add the language servers, formatters and linters to each `shell.nix` so tooling arrives with
the shell rather than from a per-developer install: `clang-tools` for the C/C++ shells,
`verible` + `verilator` + `iverilog` for the FPGA shell. Note the FPGA shell's LSP and
simulation tools are all **free** packages — someone editing Verilog gets linting and
simulation without pulling Quartus at all.

Write `docs-tech/editor-setup.md` with sections for VS Code **and** Neovim/Emacs/Helix and
Qt Creator, so the documentation stays as editor-neutral as the repository. Each section is
short: `nix develop` (or direnv), then clangd and `verible-verilog-ls`.

**Gate:** on a clean machine, `nix develop` plus a text editor with a clangd client gives
completion and diagnostics in the GUI, the FX3 firmware and the programmer, with no IDE
installed. See [ide-independence.md](ide-independence.md) §7.

### P3-6 Test scaffolding and first host unit tests (M) — D18

The project has **no test infrastructure at all** today. Add `enable_testing()`, CTest with
the T1–T5 labels from [agents-and-testing.md](agents-and-testing.md) §3, and GoogleTest from
nixpkgs (matching decode-orc, so contributors moving between projects meet one convention).
Wire the T1–T4 lanes into `nix flake check`.

First tests, chosen to prove the harness and to cover the highest-consequence logic:

- `StringUtilities` — header-only and pure; the smoke test for the harness itself
- `dddconv` 10-bit ↔ 16-bit conversion — round-trip property tests plus golden files. A bug
  here silently corrupts every converted capture
- `find_flashprog_image()` — pure path resolution, and the site of D13
- I2C/SPI paging arithmetic in the programmer — off-by-one across a page boundary bricks a
  device

**Gate:** `nix flake check` runs the suite for `gui` and `fx3/programmer` on a clean machine.

### P3-7 Author `TESTING.md` (M)

Per [agents-and-testing.md](agents-and-testing.md) §3 and §4: the five tiers, what each
component can and cannot be tested with, and — most importantly — the
**hardware-in-the-loop capture-integrity procedure** built on the FPGA's test-pattern
generator and `dddutil`'s analyser (§4). Mark clearly what exists versus what is planned;
do not describe tests that have not been written.

### Deviations and findings

**1. Testable logic had to be extracted before it could be tested (P3-6).** The plan named
four test targets without noting that three of them were unreachable: `dddconv`'s conversion
lives inside `packFile()`/`unpackFile()` behind 20 MiB of buffering and file I/O, and
`find_flashprog_image()` plus the paging arithmetic were `static` inside a 793-line file with
`main()` and a libusb dependency. Three small, behaviour-preserving extractions were made:

| New file | Contents | Extracted from |
| --- | --- | --- |
| `gui/src/dddconv/samplecodec.h` | Pure 4-sample ↔ 5-byte pack/unpack | `dataconversion.cpp` |
| `fx3/programmer/src/fx3-paging.h` | Page padding, slave chunking, transfer sizing, sector counts | `fx3-programmer.c` |
| `fx3/programmer/src/fx3-flashprog.{c,h}` | Secondary-loader path resolution | `fx3-programmer.c` |

The `dddconv` extraction was verified as behaviour-preserving two ways: a 4000-sample
pack/unpack round trip through the real binary is byte-identical, and all 5000 packed bytes
match an independent implementation of the *original* expressions. This is the "push new logic
into testable helpers" shape AGENTS.md §5.2 asks for, applied retroactively.

**2. The udev rules did not cover the device (new defect, D19).** P3-3's gate is "a plugged-in
FX3 gets the expected permissions", which the shipped rules could not meet. Two problems:

- The rules matched Cypress VID `04b4` only. Once the FX3 has firmware it re-enumerates as
  `1d50:603b` and stops being a Cypress device, so **the capture GUI could not open the
  running hardware without root** — on every Linux distribution, not just NixOS.
- Both rules ran `RUN+="/usr/local/bin/cy_renumerate.sh"`, a script that signals a `cyusb`
  daemon belonging to the full cyusb_linux suite. That suite is not in this project and CMake
  never installed the script, so the hook only ever produced udev errors.

Rewritten to match both VID/PID pairs and to add `TAG+="uaccess"` for systemd-logind systems,
keeping `MODE="0666"` so behaviour does not regress where logind is absent.

**3. `x86_64-darwin` was dropped from the supported systems.** nixpkgs 26.11 removed support
for it, and *evaluating any attribute* for that system now throws — so including it broke
`nix flake show` outright. Intel Macs remain covered by the Homebrew-based CI jobs, which
P0-7 already made the authoritative macOS coverage.

**4. `fx3-firmware` and `docs-site` are not in the root flake yet**, as planned — they arrive
with P5 and P4. `bitstream` arrives with P6, guarded to `x86_64-linux` and excluded from
`checks`: Quartus is unfree, x86_64-linux only and not redistributable, so it is fed by a
second `import nixpkgs { config.allowUnfree = true; }` of the same locked input rather than
by a flake of its own. `fpga/shell.nix` exists now and is free-tools-only, so Verilog can be
edited, linted and simulated without Quartus.

*(Superseded: this paragraph originally said the bitstream would stay behind `fpga/flake.nix`.
Component flakes were removed after Phase 4 — each carried an unpinned `nixos-unstable` input
and its own lock, so entering the tree through a component diverged from the root pin. See
[nix-flake-design.md](nix-flake-design.md) §1.)*

**5. Two CMake traps worth recording**, both found by testing rather than by reading:

- `gtest_discover_tests(... PROPERTIES LABELS "unit;golden")` silently applies **only the
  first label**. CMake expands the list in the generated `set_tests_properties()` call and
  reads `golden` as a property name. The separator must be escaped: `string(REPLACE ";" "\\;" ...)`.
- The root flake's `forAllSystems (...) // forLinux (...)` looked right and was wrong: `//`
  is a shallow update, so the Linux attrset replaced the portable one wholesale and `gui`
  disappeared on Linux — the only systems that can build both. Merged per system instead.

### Gate — **met**

| Gate | Result |
| --- | --- |
| P3-2 `nix build .#gui` | Builds; `wrapQtAppsHook` applied (`.DomesdayDuplicator-wrapped` present); starts under `QT_QPA_PLATFORM=offscreen` with no xcb plugin error |
| P3-2 `nix develop .#gui` | Enters; gives a working CMake build tree |
| P3-3 `nix build .#fx3-programmer` | Builds. Installs `bin/fx3-programmer`, `share/domesday-duplicator/cyfxflashprog.img` and `lib/udev/rules.d/88-cyusb.rules` |
| P3-4 `nix develop .#hardware` | Enters with KiCad on `PATH` |
| P3-5 clangd, LSP tooling | `clangd` 21.1.8, `verible-verilog-ls`, `verilator` 5.050, `iverilog` 13.0, `arm-none-eabi-gcc` 15.2.rel1 all present in `nix develop` |
| P3-6 `nix flake check` | **all checks passed** — both suites run inside the sandbox: 21/21 GUI, 23/23 programmer |
| All five dev shells | `default`, `gui`, `fx3`, `fpga`, `hardware` all enter cleanly |

D13 end-to-end under Nix: the built binary's compiled-in loader path points into its own
store path, and the file exists there — so `nix run .#fx3-programmer` can find the secondary
loader from any working directory. The remaining half of D13 is a hardware check (P5).

The free FPGA shell lints `statusLED.v`, `dataGenerator.v` and `fx3StateMachine.v` cleanly.
`buffer.v` does not elaborate standalone — it reaches `IPfifo.v`'s `dcfifo` instantiation,
which needs Altera simulation models. That is the documented limitation from
[nix-flake-design.md](nix-flake-design.md) §6, not a defect, and it is why P6-7's testbench
plan covers the hand-written modules rather than the whole design.

**Not verified, and cannot be here:** the NixOS half of P3-3's gate — that a plugged-in FX3
gets the expected permissions on a host with `hardware.domesdayDuplicator.enable = true`, and
that a **flash** (not RAM) operation completes from the Nix-installed binary. Both need real
hardware and a NixOS rebuild. The module evaluates (`nix flake check` checks it as a NixOS
module) and the rule installs to the right location, but that is as far as it goes.

## Phase 4 — docs: convert to MkDocs Material, then flake it — **DONE**

Executed 2026-08-12 on `20260812-002`. All ten tasks complete; **D9, D10 and D12 are closed**.
The site builds under `--strict` and all 22 pages render with working images.

Deviations and findings are recorded after the task table.

**Full detail: [docs-theme-migration.md](docs-theme-migration.md).** Summarised here.

The site moves from Jekyll + `just-the-docs` (via `remote_theme`) to **MkDocs + Material +
`mkdocs-awesome-nav`**, matching decode-orc (`/home/sdi/Coding/decode-orc`). Converting is
less work than packaging the current Jekyll site, and it *resolves* D9 and D12 rather than
working around them: Material comes from nixpkgs, so there is no build-time theme fetch, no
`Gemfile`/`gemset.nix` to author, and the deployed site becomes the same derivation output as
the local one.

Feasibility is good: all 24 markdown files have **no front matter and no Liquid**. Every
Jekyll-specific file gets deleted rather than converted.

| Task | Size | Detail |
| --- | --- | --- |
| **P4-1** Reorganise content into nav-shaped directories | M | `Sidebar.md`'s grouping does not match the folder structure — `Misc/` currently backs four different sidebar sections. `awesome-nav` derives nav from the tree, so the tree has to match |
| **P4-2** Author `docs/mkdocs.yml` | S | Theme block copied from decode-orc; only identity fields differ. **`docs_dir: content`, never `site`** — it would collide with MkDocs' default `site_dir` |
| **P4-3** Author the `.nav.yml` files | S | Transcribe `Sidebar.md`, then delete it. Per-directory files preserve the LD-V4300D page order, which alphabetical sorting would destroy |
| **P4-4** Delete the Jekyll machinery | S | `_layouts/`, `_includes/`, `_config.yml`, `Sidebar.md`, `Footer.md`, `search.json`, and the four shell scripts. Resolves **D9**, **D12** |
| **P4-5** Delete `Unused-Assets/` | S | 6 MB that MkDocs would otherwise copy into the published site |
| **P4-6** `docs/{package,shell,flake}.nix` | M | `python312.withPackages [ mkdocs mkdocs-material mkdocs-awesome-nav ]` — all three in nixpkgs today. `mkdocs build --strict` |
| **P4-7** Branding | S | Logo into `content/assets/`, `custom.css`, favicon |
| **P4-8** `site_url` and every inbound link | S | **D10.** Per P0-4: set `site_url` to `https://simoninns.github.io/domesdayduplicator`, fix `README.md:3`, `README.md:37`, `docs/README.md:7` and `CONTRIBUTING.md:13`. **No redirect stub** — old deep links 404, accepted |
| **P4-9** Replace the Pages workflow | S | decode-orc's: build with Nix, upload `./result`. Drops `actions/jekyll-build-pages` |
| **P4-10** Page-by-page visual review | M | Kramdown-GFM → Python-Markdown differences show up in tables, images and code blocks |

**Sequencing note:** URLs change twice over if this is split from the repo move. Jekyll
produced `…/Related-Projects/The-ld-decode-Family.html`; MkDocs produces
`…/related-projects/the-ld-decode-family/`. D10 already moves the site off
`/DomesdayDuplicator-docs`. **Do both in this one phase** so inbound links break exactly once.

`check-internal-linkage.sh` and `check-orphans.sh` are not ported — `mkdocs build --strict`
fails on broken internal links, and `awesome-nav` errors on `.nav.yml` entries pointing at
missing files. The sidebar external-link check becomes moot once `Sidebar.md` is gone.

### Deviations and findings

**1. Raw `<img>` tags silently broke every image on three pages — and `--strict` could not
see it.** This is the substantive finding of the phase, and exactly what P4-10 exists for.

MkDocs rewrites relative paths inside markdown `![](…)` syntax, but passes raw HTML through
verbatim. Because pages are served from **directory** URLs (`general/foo.md` →
`general/foo/index.html`), a raw `src="assets/x.png"` that Jekyll resolved from `general/`
now resolves from `general/foo/` — one level too shallow — and 404s. MkDocs never parses
those paths, so `--strict` reports nothing and the build passes with 18 broken images.

Found by walking the *built* site and resolving every `href`/`src` against the output tree,
not by reading the markdown. All 18 tags were converted to markdown images with `attr_list`
sizing (`![](path){ width="600" }`), which preserves the dimensions and lets MkDocs rewrite
the path. `attr_list` was already in the extensions list.

The trap is documented in [docs/README.md](../docs/README.md) so it is not reintroduced.

**2. Two extension-less wiki links.** `--strict` caught these immediately: Jekyll's
`jekyll-relative-links` resolved `[text](User-Guide)` without an extension; MkDocs does not.
Two occurrences, both now explicit relative `.md` paths. This is precisely the class of
defect `check-internal-linkage.sh` was written to find and missed.

**3. Content reorganised with lowercased filenames.** The plan specified nav-shaped
directories but not the file naming. Filenames are lowercased so URLs are consistent
(`/ldv4300d/rf-output/`), and the `LDV4300D-` prefix is dropped inside `ldv4300d/` where it
was redundant. Asset *directory* names are unchanged and moved beside the pages that
reference them, so the relative image links inside the markdown needed no edits at all.

**4. `Misc/assets/DdD-Firmware/` is unreferenced but kept.** 824 KB, 16 images, referenced by
no page — evidently from an earlier revision of the firmware-flashing guide. Moved to
`ordering/assets/DdD-Firmware/`, beside the page it belongs with, rather than deleted. It
does ship with the site. `Unused-Assets/` (6 MB) was deleted as planned.

**5. Four more docs-submodule leftovers removed.** Not in the plan, but all three gave
actively wrong instructions once docs folded into the monorepo:

| File | Why |
| --- | --- |
| `docs/CONTRIBUTING.md` | Told contributors to clone the old separate docs repository |
| `docs/TESTING.md` | The Ruby/Jekyll local-preview guide that `mkdocs serve` replaces |
| `docs/.gitignore` | Every path it named (`mockup/`, `assets/medianguide/`) is gone |
| `docs/content/favicon.ico` | Byte-identical duplicate of `content/assets/favicon.ico` |

`docs/README.md` was rewritten as the component README.

**Left alone, and flagged:** `docs/LICENSE` is a GPLv3 copy inherited from the docs
submodule, but the site's own footer — now `copyright:` in `mkdocs.yml` — states the content
is **CC BY-SA 4.0**. The two contradict each other. It is not byte-identical to the root
`LICENSE`, so it is not simply a stray duplicate. Changing a licence file is the maintainer's
call, so it is noted in `docs/README.md` rather than edited.

**6. `flake.lock` created.** Without it, every CI run resolves `nixos-unstable` afresh and
the deployed site is not reproducible. Pinned to `2fcb964d` (2026-08-10).

**7. The Pages workflow now sits at the repository root.** `docs/.github/workflows/` never
ran — GitHub only reads the root. `.github/workflows/deploy-docs.yml` is the first workflow
this repository has that will actually execute, and it triggers only on `push` to `master`,
so it stays inert on this branch as intended.

### Gate — **met**

| Gate | Result |
| --- | --- |
| `nix build .#docs-site` under `--strict` | Builds clean |
| All pages render | **22 pages**, 119 MB site |
| Working images and links | **969 local links resolved, 0 missing** — verified against the built output, not the source |
| Navigation matches the old sidebar | All 8 sections in `Sidebar.md` order, same labels, same page order — including the deliberate LD-V4300D Overview → Cleaning → RF-output → Calibration → PSU-recap progression |
| Deployed artefact is the `nix build` output | The workflow uploads `./result` directly |
| `nix flake check` | all checks passed |
| `nix develop .#docs` | `mkdocs serve` works; `mkdocs build` completes in 0.52 s |

Page-by-page render review (P4-10): 7 tables across 5 pages, 5 code blocks, and every page's
images all render; no literal markdown or unrendered `attr_list` braces leak into the output.

The duplicated `User-Guide.md` nav entry is resolved as the plan suggested — it lives under
*Capture Application*, and *Overview* cross-links to it.

**URL change.** Inbound deep links break exactly once, as designed:

```
was:  simoninns.github.io/DomesdayDuplicator-docs/Related-Projects/The-ld-decode-Family.html
now:  simoninns.github.io/domesdayduplicator/related-projects/the-ld-decode-family/
```

Per P0-4 there is **no redirect stub**. All five in-repo inbound links were updated
(`README.md` ×2, `docs/README.md`, `fpga/`, `gui/` and `hardware/README.md`).

## Phase 5 — FX3 firmware flake

Depends on P0-2 (licence) and P2-6/P2-7/P2-8.

| Task | Size | Detail |
| --- | --- | --- |
| **P5-1** `elf2img` derivation | S | `fx3/firmware/elf2img.nix` over `fx3/sdk/util/elf2img` |
| **P5-2** Firmware derivation | M | `gcc-arm-embedded`, `-DCMAKE_TOOLCHAIN_FILE=…`, `-DCYFX3SDK_PATH=${../sdk}`, `-DFIRMWARE_VERSION=${self.shortRev or "dirty"}`, `python3` in `nativeBuildInputs` for `generate-descriptor.sh`, `dontStrip`/`dontPatchELF`. If the link fails on `-nostartfiles`, add `hardeningDisable = [ "all" ]` — a freestanding ARM target and nixpkgs' default hardening flags do not mix. |
| **P5-3** SDK provenance | S | `fx3/sdk/README.md` (version, origin URL, refresh date) + `LICENSE.txt` copied from the SDK's `license/license.txt`. Mechanism settled by P0-2 — this is record-keeping only |
| **P5-4** Hardware verification | M, **HW** | Flash the `nix build` output with `fx3-programmer`; device enumerates; `lsusb -v` product string shows the real commit hash (proves **D4**; D8 is unreachable dead code and not observable here); then run the **capture-integrity procedure** from TESTING.md — zero sequence breaks required |
| **P5-5** Descriptor golden test | S | Host-side T2 test over `generate-descriptor.sh`: fixed commit string in, byte-for-byte comparison against a committed reference header. Protects the descriptor byte layout — the path the host actually reads, including the computed length byte. Note this does **not** cover D8, which lives on a separate, dead code path |
| **P5-6** Stamp the GUI with its commit | S | **D21.** The GUI is the one shipped artefact that cannot be traced to a source revision. Mirror what the firmware already does: a `DDD_VERSION` cache variable defaulting to `git rev-parse --short=8 HEAD`, falling back to `"unknown"`, passed through `target_compile_definitions`, surfaced in the About dialog and in `--version`. The flake passes `-DDDD_VERSION=${self.shortRev or "dirty"}`. Without this, "the exact version produced at the release commit" is unverifiable for the component most users actually run |

**Gate:** P5-4 passes. A firmware image that compiles but has not been flashed and
capture-verified is not done.

Additionally, `nix build .#fx3-firmware` must produce `firmware.img`, `firmware.elf` and
`firmware.map` in `$out`, and the built image's descriptor must report the commit it was
built from — that is what makes the CI artefact in P7 traceable.

## Phase 6 — FPGA flake

Depends on P0-3.

| Task | Size | Detail |
| --- | --- | --- |
| **P6-1** Quartus flake | M | `import nixpkgs { config.allowUnfree = true; }` internally; `.override { supportedDevices = [ "Cyclone IV" ]; withQuesta = false; }`. Add the **USB-Blaster udev rule** to `nix/modules/udev.nix` alongside the FX3 one — the nixpkgs Quartus package ships no udev rules. Sketch in [nix-flake-design.md](nix-flake-design.md) §6 |
| **P6-2** Headless compile, convert and program flow | M | `quartus_sh --flow compile DomesdayDuplicator`, then `quartus_cpf -c DomesdayDuplicator.cof`, then `quartus_pgm` driven by the already-committed `DomesdayDuplicator_write_{sof,jic}.cdf`. `export HOME=$TMPDIR` (Quartus needs a writable home). No GUI at any step — [ide-independence.md](ide-independence.md) §2.1 |
| **P6-3** IP regeneration | L | **Contingency only.** `IPfifo.v` and `IPpllGenerator.v` are committed plain Verilog instantiating `dcfifo`/`altpll` with explicit `defparam`s, so nothing runs MegaWizard at build time. Needed only if 25.1 rejects the 2017-era parameters — see [ide-independence.md](ide-independence.md) §2.2 |
| **P6-4** `fpga/README.md` | S | Canonical Quartus version, manual-install fallback, what reproducibility can and cannot be relied on (P6-9's finding), that the dev shell is the deliverable, and that the generated `.v` files are **source of truth** rather than wizard output to be regenerated |
| **P6-5** Hardware verification | L, **HW** | Run the TESTING.md capture-integrity procedure with the flake-built bitstream (zero sequence breaks), then capture a known disc and compare against a capture from the shipped 18.0-built one. Keep the released `.jic` in-tree until this passes |
| **P6-6** `verilator --lint-only` check | S | Lint the hand-written modules (`DomesdayDuplicator.v`, `buffer.v`, `dataGenerator.v`, `fx3StateMachine.v`, `statusLED.v`) as a `nix flake check`. Free, fast, cross-platform — so gateware gets *some* CI coverage even though bitstream builds cannot run there |
| **P6-7** Gateware testbenches | M | T3 simulation for `dataGenerator.v` (assert the 0…1020 test ramp — the same sequence P6-5 verifies on silicon), `fx3StateMachine.v` (the handshake, highest-risk module) and `statusLED.v`. Note in TESTING.md that whole-design simulation needs vendor `dcfifo`/`altpll` models |

| **P6-8** Bitstream provenance record | S | The bitstream is built outside CI, so the artefact must carry its own provenance. Emit `bitstream-provenance.txt` alongside the `.sof`/`.jic`: source commit, **exact Quartus version and build (32/64-bit)**, host CPU architecture, Fitter seed, `Maximum processors allowed`, `.qsf` device string, and the SHA-256 of each output. `nix build .#bitstream` writes it into `$out`; a manual build writes it by hand. This is what P8-3 attaches to the release |
| **P6-9** Measure reproducibility, do not assume it | S | Compile the same commit **twice** on the same pinned toolchain and `cmp` the `.sof`. Two possible findings, both useful: byte-identical (so a plain SHA-256 is a complete verification method, and P6-10 collapses to nothing), or differing only in the header region where Quartus embeds a compile timestamp (so the digest must be taken over a canonical form). Record the answer in `fpga/README.md`. This settles a question earlier drafts of this plan got wrong by assuming |
| **P6-10** Publishable bitstream digest | S | Depends on P6-9. If the `.sof` is byte-identical, publish its SHA-256 and stop. If not, add a small script emitting a **canonical digest** over the configuration payload with the timestamped header excluded, so a third party with the same pinned Quartus can rebuild and verify without CI ever running Quartus. Both digests go in `bitstream-provenance.txt` and in the release `SHA256SUMS` |
| **P6-11** Pin the determinism-relevant settings | S | Determinism depends on settings that are currently implicit. Set the Fitter seed explicitly in the `.qsf` rather than relying on the default, and record `Maximum processors allowed`. Consider `ROUTER_TIMING_OPTIMIZATION_LEVEL` if P6-9 shows routing variance. An unpinned seed is a reproducibility claim resting on a default that a future Quartus could change |

**Gate:** P6-5 passes, and every step from source to programmed device runs from a shell.
P6-9 has been run and its answer recorded, so the project states what reproducibility it
actually has rather than assuming in either direction. The hardware gate remains functional
equivalence — a capture with zero sequence breaks — not hash equality.

### The bitstream is not built by CI

Per the decision in [Release artefacts and provenance](#release-artefacts-and-provenance) §4,
**the FPGA bitstream is built locally and attached to releases by hand.** Everything in this
phase is still worth doing — `nix build .#bitstream` is what makes a *local* build
repeatable and scriptable, and P6-6's `verilator --lint-only` check does run in CI, so the
gateware is not entirely uncovered there.

What makes this workable without CI is P6-10: **publish digests instead of building it
there.** The maintainer builds locally and attaches the artefact plus its digests; anyone with
the same pinned Quartus version can rebuild and check. That gets most of the value of a CI
build — an independently verifiable artefact — without putting a GB-scale unfree toolchain on
a runner. It depends on P6-9 having measured what a rebuild actually produces.

## Phase 7 — CI: build every artefact on commit, publish them on release

This phase carries the maintainer requirement from
[Release artefacts and provenance](#release-artefacts-and-provenance): the GUI, the FX3
firmware and the FX3 programmer are built by CI **on every commit**, and a release contains
exactly the artefacts built from the release commit. The FPGA bitstream is deliberately
excluded for now (§4 of that section); it is attached by hand in P8-3.

### 7a. Build on commit

| Task | Size | Detail |
| --- | --- | --- |
| **P7-1** Single path-filtered `build.yml` | M | `nix-installer-action` + `magic-nix-cache-action`, then `nix build .#gui .#fx3-firmware .#fx3-programmer .#docs-site`. Replaces the three per-submodule workflows (including the one fixed in P2-3). Path filters so a `docs/`-only change does not rebuild firmware — but note the filters must **not** apply on `master` or on tags, where a complete artefact set is the point |
| **P7-2** Keep the native build matrix | M | The current GUI workflow builds Linux x64/ARM64, macOS x64/ARM64 and Windows x64. **Nix cannot produce the Windows binary**, so those five jobs stay, driven from the new paths. Nix is additive here, not a replacement. This is why the GUI has two build paths and the firmware only one |
| **P7-3** Pages deploy | S | Already done in P4-9: `nix build .#docs-site` → `upload-pages-artifact` with `path: ./result` → `deploy-pages`. Confirm the path filter still matches `docs/**` |
| **P7-4** `nix flake check` in CI | S | Runs T1–T4 for every component. Excludes `bitstream` — unfree, GB-scale, `x86_64-linux` only |
| **P7-5** Delete per-component `.github/` dirs | S | `fx3/.github/` and `gui/.github/` are inert (GitHub only reads the repository root) but actively misleading once `build.yml` exists |
| **P7-6** Test lanes | S | Tiers T1–T4 in the consolidated workflow. **T5 never runs in CI** — it needs a physical DdD, and a test that silently "passes" because no hardware was attached is worse than no test |
| **P7-7** Retain artefacts from every commit build | S | `actions/upload-artifact` with a name carrying the short SHA, so a build from any commit can be fetched without re-running CI. Default retention is 30 days; set it explicitly rather than inheriting it, and note in the workflow that this is **not** the release archive — GitHub expires these |

### 7b. Publish on release

| Task | Size | Detail |
| --- | --- | --- |
| **P7-8** `release.yml`, triggered by `v*` tags | M | Checks out **the tag**, builds every CI-buildable artefact from it, and attaches them to the GitHub Release. Tag-triggered rather than branch-triggered so the artefacts are provably from the release commit and not from whatever `master` moved to afterwards. `workflow_dispatch` as well, for re-running a failed publish without re-tagging |
| **P7-9** Version stamping is a release gate | S | The job fails if any artefact reports `unknown` as its version. Nix builds from a tag have no `.git`, so `-DFIRMWARE_VERSION=`/`-DDDD_VERSION=` must be passed explicitly (D4, D21) — a silent fallback to `unknown` would produce untraceable release binaries, which is exactly what this phase exists to prevent |
| **P7-10** `SHA256SUMS` and a provenance note | S | Generated over every attached asset, plus a short note recording the source commit, the `flake.lock` nixpkgs revision, and the toolchain versions. Attached to the release alongside the binaries |
| **P7-11** Document the FPGA hand-off | S | The release checklist states plainly that the bitstream is **not** produced by this workflow: build it locally per P6-2, then attach the `.sof`, `.jic`, P6-8's `bitstream-provenance.txt` and P6-10's digests by hand. Fold those digests into the release `SHA256SUMS` so every asset is covered by one manifest regardless of where it was built. A release must not be published with the other artefacts present and the bitstream quietly missing |

### Release asset set

What a complete `v*` release carries:

```
DomesdayDuplicator-<ver>-linux-x64.tar.gz        CI  (nix + native)
DomesdayDuplicator-<ver>-linux-arm64.tar.gz      CI  (native)
DomesdayDuplicator-<ver>-windows-x64.zip         CI  (native — Nix cannot build this)
DomesdayDuplicator-<ver>-macos-x64.zip           CI  (native)
DomesdayDuplicator-<ver>-macos-arm64.zip         CI  (native)
firmware.img / firmware.elf / firmware.map       CI  (nix build .#fx3-firmware)
fx3-programmer-<ver>-linux-x64                   CI  (nix build .#fx3-programmer)
DomesdayDuplicator.sof / .jic                    MANUAL — local Quartus build
bitstream-provenance.txt                         MANUAL — provenance + digests (P6-8, P6-10)
SHA256SUMS                                       CI
PROVENANCE.txt                                   CI
```

**Gate:** one `build.yml` and one `release.yml`; no job references a non-existent path; a PR
touching only `docs/` does not trigger firmware jobs; a `v*` tag produces a release whose
assets all report the tagged commit and none of which report `unknown`; `SHA256SUMS` covers
every attached file.

## Phase 8 — Cleanup and release

| Task | Size | Detail |
| --- | --- | --- |
| ~~**P8-1** Archive the four upstream repos~~ | — | **Removed** per P0-6 — the old repositories are left alone and cleaned up separately, outside this plan |
| **P8-2** README rewrite | M | Nix quick-start per component alongside the existing native instructions |
| **P8-3** Tag a release | M | First monorepo release, and the first exercise of the P7-8 release workflow end to end. Tagging `v*` publishes the GUI binaries (5 platforms), `firmware.img`, `fx3-programmer`, `SHA256SUMS` and `PROVENANCE.txt` automatically. Then **build the bitstream locally and attach `.sof`, `.jic` and `bitstream-provenance.txt` by hand** (P7-11) — the release is not complete without them. Verify every asset reports the tagged commit and none reports `unknown` |
| **P8-4** Update this plan | S | Mark it executed; fold anything still outstanding into issues |
| **P8-5** SPDX header convention | M | Only 8 of 67 source files carry SPDX identifiers; the rest use long-form GPL notices. Adopt SPDX (machine-checkable, and matches decode-orc), add the T4 presence check, and convert files as they are touched rather than in one sweeping commit |
| **P8-6** `--analyse-test-data` CLI mode | M | Optional follow-up: make step 4 of the capture-integrity procedure scriptable rather than GUI-driven, so the T5 gate becomes semi-automated |

## Summary of what needs hardware

Four gates cannot be signed off from a green build alone: **P0-3** (Quartus spike), **P3-3**
(udev rules), **P5-4** (firmware flash and enumerate), **P6-5** (bitstream capture). Schedule
them together if bench access is limited — P0-3 and P6-5 in particular are the same setup.

## Deliberately out of scope

- KiCad 5 → 10 file-format migration (blocks `kicad-cli` export; separate change, separate review)
- Any firmware, gateware or application behaviour change
- Windows/macOS packaging beyond what the existing CI already produces
- Git LFS or history pruning, unless P0-5 chose it — in which case it happens *in* P1 or not at all
