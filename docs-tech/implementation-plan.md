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
- **No CI ran on this branch until Phase 7.** The three inherited workflows sat at
  `docs/.github/`, `fx3/.github/` and `gui/.github/`, and GitHub only reads the repository
  root, so nothing triggered. That was convenient while paths were moving — no red crosses
  from workflows pointing at directories that no longer exist — but it meant there was no
  automated safety net, and verification was local and manual.

  **P7-1 ends that.** `.github/workflows/build.yml` runs on every push to every branch,
  including this one, and builds and packages everything each time; only publishing waits for
  a tag. That is the point of doing it this way round — the whole chain gets exercised on the
  branch, for as long as the branch lives, rather than being first tried out during a release.
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
| D13 | Permanent (EEPROM/SPI) programming needs a Cypress secondary loader, `cyfxflashprog.img`. **File now vendored** (2026-08-12) at the programmer's directory root, where the existing `../cyfxflashprog.img` candidate finds it from `build/`. **Code half remains:** every candidate path is working-directory-relative, so installed binaries still cannot locate it | `fx3-programmer/src/fx3-programmer.c:136` | P2-10, P3-3 | **Closed** — code in P2, **hardware-verified in P5**: the loader was found via the installed store path with `$FX3_FLASH_PROG` unset, from `/`, and an EEPROM write completed and verified |
| D14 | Four qmake `.pro` files duplicate the CMake build definition and exist only for Qt Creator; `BUILD.md` steers contributors to them | `gui-app/tools/**/*.pro` | P2-11 | **Closed** P2 |
| D15 | No `CMAKE_EXPORT_COMPILE_COMMANDS` anywhere, so no `compile_commands.json` and no working clangd in any editor | all `CMakeLists.txt` | P2-12 | **Closed** P2 |
| D16 | Sole `.editorconfig` is buried in `gui-app/tools/DomesdayDuplicator/`; `.vscode`/`.idea` ignore rules exist only in `gui-app/.gitignore` | repo root | P2-13 | **Closed** P2 |
| D17 | The two licence names are **transposed**: `LICENSE` is GPLv3 and the hardware file is CC BY-SA 4.0, but the README labels software as CC BY-SA (linking to the GPLv3 file) and hardware as GPLv3 (linking to the CC BY-SA URL) | `README.md` licence block | P2-14 | **Closed** P2 |
| D18 | No test infrastructure of any kind — no `enable_testing()`, `add_test()`, GoogleTest, Catch2 or QTest anywhere in the tree | repo-wide | P3-6 | **Closed** P3 |
| D19 | udev rules match Cypress VID `04b4` only, so the device is root-only once firmware is loaded and it re-enumerates as `1d50:603b` — the capture GUI cannot open it. Both rules also `RUN+=` a `cy_renumerate.sh` that is never installed and belongs to a daemon this project does not ship | `fx3/programmer/configs/88-cyusb.rules` | P3-3 | **Closed** P3 |
| D20 | Raw `<img src="assets/…">` tags are passed through by MkDocs without path rewriting, so under directory URLs they resolve one level too shallow and 404. `--strict` cannot detect this because MkDocs never parses those paths — 18 tags across 3 pages were silently broken | `docs/content/{general,ordering}/*.md` | P4-10 | **Closed** P4 |
| D25 | `fx3-programmer -r` claims to "Reset device" but `fx3_reset_device()` is a stub: it prints "Device will reset automatically after firmware download completes", sleeps 2 seconds and returns 0. No reset is issued and no vendor command is sent, so a caller cannot get a running device back into bootloader mode without a physical power cycle | `fx3/programmer/src/fx3-programmer.c:630` | P5-10 | **Closed** P5 — option removed; using it now explains the power-cycle requirement |
| D24 | `fx3-programmer`'s help text says `-p` programs **SPI flash** in four places, but `fx3_program_prom()` programs the **I2C EEPROM** (`0xBA`/`0xBB`) and says so in its own output. The SPI vendor commands `FX3_SPI_FLASH_CMD` (`0xC2`) and `FX3_SPI_FLASH_ERASE` (`0xC4`) are defined and never referenced — there is no SPI code path at all. Misleading documentation on a *destructive, permanent* operation, and it disagrees with the project's own flashing guide, which correctly says I2C EEPROM. The same help block also shows `-d 0 -v` as a standalone "Verify device 0 firmware" example, but `-v` is only a modifier for `-p` and errors out on its own | `fx3/programmer/src/fx3-programmer.c:649,656,662,664` | P5-10 | **Closed** P5 — help text corrected, dead SPI defines removed, guarded by a CLI contract test |
| D23 | The udev rule's `TAG+="uaccess"` never took effect: the file was named `88-cyusb.rules`, but systemd consumes that tag in `73-seat-late.rules`, and udev processes rule files in lexical order — so the tag was set after anything looked for it and no ACL was ever applied. Found on live hardware, with the rule installed and active and `getfacl` on the device node showing no user entry. Only the `MODE="0666"` fallback was doing any work, which is why it stayed invisible. Renamed to `70-domesday-duplicator.rules` | `fx3/programmer/configs/70-domesday-duplicator.rules` | P5-9 | **Closed** P5 |
| D22 | `fx3-programmer.c` states in a comment that it derives from Cypress `cyusb_linux` but carries **no copyright or licence header at all**. `cyusb_linux` is LGPL-2.1, which permits the relicensing to GPLv3 this project relies on, so this is a compliance gap rather than a licence problem. (`fx3/programmer/VENDOR.md` previously mis-numbered this as D20, which is the MkDocs image defect) | `fx3/programmer/src/fx3-programmer.c` | P8-5 | Open |
| D21 | The GUI carries no version information: `project(DomesdayDuplicator VERSION 1.0)` is hardcoded and no commit hash reaches the binary or the About dialog, so a released GUI artefact cannot be traced back to the commit that produced it | `gui/CMakeLists.txt`, `gui/src/DomesdayDuplicator/aboutdialog.cpp` | P5-6 | **Closed** P5 |

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

So the SDK stays in-tree: `fw_build/fx3_fw/fx3.ld` and `fw_lib/1_3_5/{inc,fx3_release}`.
(`util/elf2img/` was part of this list until P5-7 replaced it; the directory is now 17 MB
after that removal and the pruning in P5-8.)

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

*Superseded in Phase 5.* This section originally continued: "`elf2img` is a single 13 KB C
file with a project-authored `CMakeLists.txt` — it becomes its own tiny derivation regardless
of which path is taken (D5)." It did, briefly. It has since been **replaced by
`fx3/mkimage`**, a from-scratch GPLv3 implementation written against Infineon's public
application note AN76405, and the vendored `elf2img` has been deleted. See P5-7 below. The
SDK itself is unaffected and remains vendored: the ARM libraries, headers and `fx3.ld` are
the binding constraint, and no host tool rewrite touches them.

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

### 1. Two release streams

**Decided 2026-08-12: the artefacts split into two independently versioned, independently
tagged streams.** They are consumed differently and change at different rates — the GUI is
installed on a desktop and updated freely, while firmware and gateware are flashed onto a
board and paired with each other. Folding them into one tag would mean a GUI typo fix
reissues a bitstream, and a gateware change reissues three installers.

| Stream | Tag | Contains |
| --- | --- | --- |
| **GUI** | `gui-v*` | Linux Flatpak, macOS DMG, Windows MSI, `SHA256SUMS`, `PROVENANCE.txt` |
| **Firmware/gateware** | `fw-v*` | `firmware.img`/`.elf`/`.map`, FPGA `.sof`/`.jic` + `bitstream-provenance.txt`, `fx3-programmer`, `SHA256SUMS`, `PROVENANCE.txt` |

The two streams are cut from the same monorepo commit graph, so a `fw-v*` tag and a `gui-v*`
tag both name a commit and both remain fully traceable (§3). The docs site is a third
consumer of CI but not a release stream — it deploys per commit to `master`.

What is built, and how often:

| Artefact | Stream | Built by | Platforms | Cadence |
| --- | --- | --- | --- | --- |
| `DomesdayDuplicator-<ver>.flatpak` | GUI | CI (`flatpak-builder`) | Linux x64 | Every commit; **published on `gui-v*`** |
| `DomesdayDuplicator-<ver>.dmg` | GUI | CI (Homebrew Qt + `hdiutil`) | macOS arm64 | Every commit; **published on `gui-v*`** |
| `DomesdayDuplicator-<ver>.msi` | GUI | CI (MSYS2 UCRT64 + WiX) | Windows x64 | Every commit; **published on `gui-v*`** |
| `DomesdayDuplicator` raw binaries | GUI | CI (`nix build .#gui` + the native matrix) | Linux x64/ARM64, macOS x64/ARM64, Windows x64 | Every commit — **build artefacts only; not release assets** |
| `firmware.img`, `firmware.elf`, `firmware.map` | Firmware | CI (`nix build .#fx3-firmware`) | n/a (cross-compiled) | Every commit; **published on `fw-v*`** |
| `fx3-programmer` | Firmware | CI (`nix build .#fx3-programmer`) | Linux x64, ARM64 | Every commit; **published on `fw-v*`** |
| `DomesdayDuplicator.sof` / `.jic` | Firmware | **Local build, attached manually** | n/a | Per `fw-v*` release |
| Documentation site | — | CI (`nix build .#docs-site`) | n/a | Every commit to `master` |

Two consequences of that table, both decided 2026-08-12:

- **The GUI ships as installers, not archives.** The five `tar.gz`/`zip` per-platform archives
  the current workflow publishes are replaced by one Flatpak, one DMG and one MSI — the same
  three formats decode-orc ships. The native matrix still *builds* every platform on every
  commit (it is the compile-side regression check, and it feeds the packaging jobs), but its
  output is a 30-day CI artefact rather than a release asset. Rationale: an unpacked tarball
  of Qt binaries has no desktop entry, no icon, no dependency resolution and no uninstall,
  and every support question about "it says it can't find libQt6Core" traces back to one.
- **`dddconv` and `dddutil` are both removed** (P7-12), leaving `DomesdayDuplicator` as the
  only shipped GUI binary. The two removed tools between them held three implementations of
  the same 10-bit ↔ 16-bit transform, and neither fits an installer-only release cleanly — a
  CLI tool has nowhere to go inside a Flatpak, and a second `.app`/Start-menu entry for a
  utility is a support liability. One capture application, one icon, one shortcut.

  **One capability must survive the removal.** `dddutil`'s test-data analysis is step 4 of
  the capture-integrity procedure ([TESTING.md](../TESTING.md) §5) — the T5 gate that P5-4 and
  P6-5 are still blocked on. It is ported into the capture application first (P7-19), and only
  then does `dddutil` go. `dddutil`'s file-conversion feature is *not* ported: the decode
  toolchain reads the packed 10-bit capture format directly, so converting it up-front is a
  path nobody is asked to take any more. If that assumption is wrong for some workflow, say so
  before P7-12 runs rather than after.

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
toolchain; the `.sof` file may not be byte-identical.**

**Phase 6 measured it, and that statement is exactly right.** Four compiles of the same
commit — two locally, one with the Fitter settings pinned, one inside a Nix build sandbox —
produced a **byte-identical `.jic`** and a `.sof` differing in 32–34 bytes of 704,015: a
per-run design hash in two encodings, two copies of a compile timestamp, and the checksum
covering them. No configuration data differs, and the identical `.jic` — which `quartus_cpf`
derives from the `.sof`'s configuration payload — is the independent proof of that. Full
detail in Phase 6, "P6-9 in full".

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
| GUI | **Yes** — `DDD_VERSION` reaches `--version` and the About dialog of both tools (D21, fixed in P5-6). The installers carry it too: the Flatpak's AppStream `<release>`, the DMG's `CFBundleShortVersionString` and the MSI's `ProductVersion` (P7-9) |
| FPGA bitstream | **No** — nothing in the `.sof`/`.jic` identifies the source |

P5-6 closed the GUI gap; P7-9 makes it a release gate. The FPGA gap is handled by recording
provenance alongside the artefact rather than inside it (§4) — P6-8 emits
`bitstream-provenance.txt` into the build output, and the derivation fails rather than
installing one that reports an `unknown` commit.

Every release — of either stream — also publishes a `SHA256SUMS` manifest and a short
provenance note: the commit, the `flake.lock` nixpkgs revision, and, for the firmware stream's
bitstream, the Quartus version and the machine that built it. Each manifest covers only its
own stream's assets, so `SHA256SUMS` is complete with respect to the release it is attached
to rather than to the repository as a whole.

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

**Intended shape when adopted:** GUI and FX3 per commit; the bitstream on `fw-v*` tags and
manual dispatch only, so a firmware release still gets a bitstream built from the release
commit without paying for Quartus on every push. Note that the two-stream split makes this
cheaper than it would have been under a single tag — Quartus would run only on firmware
releases, not on every GUI release.

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

**P6-9 found both answers at once, one per artefact.** The `.jic` *is* byte-identical across
rebuilds, so its release digest and its canonical digest are the same number and it needs no
machinery at all — and since the `.jic` is the file programmed into the EPCS64 flash, the
artefact that matters is the one that came out reproducible. The `.sof` is not, so it gets
the canonical form: `fpga/bitstream-provenance.py` masks the per-run design hash, the two
copies of the compile timestamp and the file checksum, and refuses to emit a digest at all if
a future Quartus moves any of them.


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
| **P0-2** Cypress SDK | M | **Decided.** Vendor it regardless of the licence review — it is already widely mirrored. **Action outstanding:** refresh from the official download into `firmware/fx3/fx3-firmware/cyfx3sdk/` (later `fx3/sdk/`), preserving the project-authored `util/elf2img/CMakeLists.txt` and the `fw_lib/1_3_5/` version path. Exact layout in [decisions.md](decisions.md). *(P5-7 removed `util/` entirely — do not restore it on a future refresh; decisions.md carries the current instructions)* |
| **P0-3** Quartus version | M, HW | **Decided; software half verified in Phase 6, hardware half outstanding.** 25.1 accepted — the upgrade already exists on `fpgaupdate-202512`. Phase 6 showed 25.1 compiles the committed sources unchanged (`0 errors`, no upgrade prompt, no rejected parameters). What remains is the capture-integrity procedure on real hardware, since that branch's last commit says "need to test". That is the **P6-5 gate** |
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

*Update, 2026-08-12 (Phase 5 bench session).* **The permissions half is now verified, and
verifying it found D23** — the rule had never actually granted anything through `uaccess`.
With the corrected rule installed, both device identities get an ACL:

```
$ ls -la /dev/bus/usb/007/012            # 04b4:00f3, FX3 bootloader
crw-rw-rw-+ 1 root root 189, 779
$ getfacl /dev/bus/usb/007/012
user:sdi:rw-
```

and the same for `1d50:603b` in application mode, which is what the capture GUI opens. A RAM
download from a non-root shell then succeeded. **The flash (EEPROM/SPI) half of the gate
remains outstanding** — that is a separate, deliberate act and was not exercised.

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

## Phase 5 — FX3 firmware flake — **DONE except the hardware gate**

Executed 2026-08-12 on `20260812-002`. Seven of the eight tasks are complete and **D21 is
closed**. Two tasks — P5-7 and P5-8 — were added mid-phase at the maintainer's request and
removed the SDK's proprietary `elf2img` from the build path entirely. **P5-4 is outstanding
and cannot be closed here** — it needs a physical Domesday Duplicator. The phase gate is
P5-4, so *the gate is not met*; see the gate section below for exactly what has and has not
been shown.

Deviations and findings are recorded after the task table.

Depends on P0-2 (licence) and P2-6/P2-7/P2-8.

| Task | Size | Detail |
| --- | --- | --- |
| **P5-1** ELF-to-image derivation ✅ | S | Done as written (`fx3/firmware/elf2img.nix` over `fx3/sdk/util/elf2img`), then **superseded within the phase by P5-7**, which replaced the vendor tool with `fx3/mkimage/package.nix` and deleted both |
| **P5-2** Firmware derivation ✅ | M | `gcc-arm-embedded`, `-DCMAKE_TOOLCHAIN_FILE=…`, `-DCYFX3SDK_PATH=${../sdk}`, `-DFIRMWARE_VERSION=${self.shortRev or "dirty"}`, `python3` in `nativeBuildInputs` for `generate-descriptor.sh`, `dontStrip`/`dontPatchELF`. If the link fails on `-nostartfiles`, add `hardeningDisable = [ "all" ]` — a freestanding ARM target and nixpkgs' default hardening flags do not mix. |
| **P5-3** SDK provenance ✅ | S | `fx3/sdk/README.md` (version, origin URL, refresh date) + `LICENSE.txt` copied from the SDK's `license/license.txt`. Mechanism settled by P0-2 — this is record-keeping only |
| **P5-4** Hardware verification — **OUTSTANDING, needs a bench** | M, **HW** | Flash the `nix build` output with `fx3-programmer`; device enumerates; `lsusb -v` product string shows the real commit hash (proves **D4**; D8 is unreachable dead code and not observable here); then run the **capture-integrity procedure** from TESTING.md — zero sequence breaks required |
| **P5-5** Descriptor golden test ✅ | S | Host-side T2 test over `generate-descriptor.sh`: fixed commit string in, byte-for-byte comparison against a committed reference header. Protects the descriptor byte layout — the path the host actually reads, including the computed length byte. Note this does **not** cover D8, which lives on a separate, dead code path |
| **P5-7** Replace `elf2img` with project-authored code ✅ | M | **Added mid-phase at the maintainer's request.** `fx3/mkimage/` — a from-scratch GPLv3 ELF-to-boot-image converter written against Infineon's public application note AN76405 §4.4, replacing the SDK's proprietary `elf2img`. Accepted on byte-identical output against the tool it replaces; the vendored copy is deleted |
| **P5-10** Make the programmer's documented behaviour match its actual behaviour ✅ | M | **D24, D25**, found while attempting the P5-4 flash test. Help text and README rewritten to describe I2C EEPROM programming rather than nonexistent SPI flash; dead SPI vendor-command defines removed; `-r` removed; `-u` now refuses outside bootloader mode instead of failing obscurely; `-d` validated; a `cli-contract` test now fails if the help text promises what the tool cannot do |
| **P5-9** Fix the udev rule filename ✅ | S | **D23**, found while preparing the P5-4 bench session. `88-cyusb.rules` → `70-domesday-duplicator.rules`, so the `uaccess` tag is set before `73-seat-late.rules` consumes it |
| **P5-8** Prune unneeded vendored material ✅ | S | Four unused SDK archives (`libcy_as0260.a`, `libcy_ov5640.a`, `libcyu3mipicsi.a`, `libcyu3sport.a` — image sensor, MIPI-CSI, serial port; ~1.8 MB) and two dead `cyusb_linux` config files (`cy_renumerate.sh`, `cyusb.conf`) |
| **P5-6** Stamp the GUI with its commit ✅ | S | **D21.** The GUI is the one shipped artefact that cannot be traced to a source revision. Mirror what the firmware already does: a `DDD_VERSION` cache variable defaulting to `git rev-parse --short=8 HEAD`, falling back to `"unknown"`, passed through `target_compile_definitions`, surfaced in the About dialog and in `--version`. The flake passes `-DDDD_VERSION=${self.shortRev or "dirty"}`. Without this, "the exact version produced at the release commit" is unverifiable for the component most users actually run |

### Deviations and findings

**1. `find_program` in a CMake toolchain file does not survive nixpkgs' cmake hook.** The
design in [nix-flake-design.md](nix-flake-design.md) §4 said the toolchain file's
`find_program(CMAKE_C_COMPILER arm-none-eabi-gcc)` would work as long as `gcc-arm-embedded`
was in `nativeBuildInputs`. It does not: the setup hook passes `-DCMAKE_C_COMPILER=gcc` (plus
host `CMAKE_AR`/`RANLIB`/`STRIP`) *before* the derivation's own `cmakeFlags`, and
`find_program` is a no-op once the cache variable is set. The result is a configure that
finds `arm-none-eabi-gcc` for ASM and host `gcc` for C, then dies on
`The CMAKE_C_COMPILER: /build/source/build/gcc is not a full path`. All six tools are now
named outright with absolute store paths, which also pins the derivation to that exact
toolchain rather than to whatever is first on `PATH`. Full write-up in §4 of the design doc.

**2. The `elf2img` licence problem was worked around, then removed outright (P5-7).** Worth
recording as a sequence, because the intermediate state was shipped and then withdrawn within
the phase.

`elf2img` was pure vendor code carrying the Cypress "UNPUBLISHED... CONFIDENTIAL AND
PROPRIETARY" header. Neither the project's GPLv3 nor any entry in `lib.licenses` described
it, and nixpkgs treats anything not marked free *as* unfree — so a truthful `meta.license`
would have made `nix build .#fx3-firmware` fail for every user without `allowUnfree`,
contradicting P0-2's stated consequence that "the FX3 firmware build stays in `nix flake
check` and in CI".

1. **First it was left absent**, on the grounds that silence claimed nothing.
2. **Then it was marked free** at the maintainer's direction — a named custom licence,
   `shortName = "cypress-fx3-sdk"`, with `free = true`, rather than borrowing a real free
   licence like MIT that would have stated something false about the file's origin. This was
   a project decision, documented as such, not a legal determination.
3. **Then the tool was replaced and the problem ceased to exist.** `fx3/mkimage` is the
   project's own GPLv3 code and declares `gpl3Plus` truthfully. Nothing in `fx3/sdk/` is a
   package any more — it is consumed as a `lib.fileset` input to the firmware derivation — so
   no `meta.license` in the tree now describes vendor code.

The lesson generalises: when packaging metadata cannot be both truthful and functional, that
is a signal about the dependency, not about the metadata. Here the dependency turned out to
be replaceable in a day. **The SDK itself is not**, and P0-2 stands unchanged.

**3. P5-3 is complete, but its `LICENSE.txt` half is impossible.** The task says to copy the
SDK's `license/license.txt` to `fx3/sdk/LICENSE.txt`. **That file does not exist in the
vendor archive** — the installer generates it, and the headers point at a path
(`<install>/license/license.txt`) that only exists after installation. `fx3/sdk/README.md`
already recorded this; P5-3 added the vendor, product name and gated-portal origin URL, the
refresh date, and a new section on how the Nix build reaches the directory.

**4. The GUI version stamp went into three binaries, not one.** P5-6 names the About dialog
and `--version`. `dddconv` already had a `QCommandLineParser` and needed one line; `dddutil`
had neither a parser nor an application name, so it gained a minimal parser. All three now
answer `--version` uniformly, which is what lets P7-9 check them with one loop rather than
special-casing the tool that cannot be asked.

**5. The About dialog needed a layout change, not just a new label.** Both dialogs use
absolute positioning. In the capture GUI's dialog the existing children already reached
y=281 in a tab page that is ~303 px tall, so a version label appended below would have been
clipped by a few pixels — invisibly on some styles and not others. The logo label was
trimmed from 141 to 133 px (the pixmap is exactly 250×133, so nothing is lost) and the text
browser moved up 20 px, putting the version at y=265..284 with room to spare. Verified by
rendering both dialogs offscreen and reading the resulting images, not by arithmetic.

**6. `fx3/shell.nix` now ships the image tool.** Without it, every fresh build tree in the
dev shell re-compiled it through the `find_program` fallback. Now the packaged tool is on
`PATH`, so the interactive route takes the same path as the Nix build. (This first shipped
the SDK's `elf2img`; after P5-7 it ships `fx3-mkimage`.)

**7. The firmware output is flat, and the CMake default is unchanged.**
`FIRMWARE_INSTALL_DIR` is a new cache variable defaulting to `bin`, so `cmake --install`
behaves exactly as it did in Phase 2; the derivation passes `.` so `firmware.{img,elf,map}`
sit at the root of `$out`.

**8. `patchelf: cannot find section '.dynamic'` in the build log is expected.** nixpkgs'
`auditTmpdir` hook runs `patchelf --print-rpath` over every ELF in the output, and
`firmware.elf` is a statically linked bare-metal image with no dynamic section. It is a
message on stderr, not a failure, and `dontPatchELF` does not suppress it.

**9. The flake's hash is 7 characters, CMake's git fallback asks for 8.** `self.shortRev` is
Nix's own abbreviation and is not configurable; `git rev-parse --short=8` is what
`CMakeLists.txt` uses when nothing is passed. So a Nix-built artefact reports e.g.
`(6ad9891)` and a local developer build of the same commit reports `(6ad9891a)`. Both
identify the commit unambiguously and both are non-`unknown`, so P7-9's gate is unaffected —
but a check written as string equality against `git rev-parse --short=8` would fail, and
should compare prefixes instead.

**10. `fx3-mkimage` reproduced the vendor tool byte for byte, and three of its behaviours are
not in the specification (P5-7).** AN76405 §4.4 defines the container completely — signature,
control and type bytes, `{length, address, payload}` sections, a zero-length terminator
carrying the entry point, and a checksum explicitly excluding lengths, addresses and header.
What it does *not* define is how an ELF becomes one. Three decisions were determined
empirically from the vendor tool's own output and are required for byte-identical results:

- the 0x00–0x100 ARM vector area is dropped by default (this one *is* in the SDK readme);
- sections are split at **64 KiB** — AN76405 places no limit on section length, so this is
  the vendor tool's choice, not the bootloader's;
- **`p_memsz` is used, not `p_filesz`**, with the difference zero-filled — so `.bss` ships
  pre-zeroed in the image.

All three are documented in `fx3/mkimage/README.md` and pinned by tests. The SDK's own
`readme.txt` was **not** usable as the specification: it documents the command line and the
EEPROM control byte and contains no binary layout at all.

**11. Deleting four SDK archives changed the firmware image not at all, and the ELF by 93
bytes (P5-8).** The `.img` is byte-identical. The `.elf` differs in exactly 93 of 2,247,868
bytes, all inside a DWARF line-table path string: pruning the archives changed the SDK
fileset's hash and therefore its store path, which `-g` records in the debug info. Since the
`.img` is derived solely from `PT_LOAD` content and the entry point, its identity is the
proof that nothing loadable moved. Worth knowing before someone diffs two ELFs and concludes
a prune broke the link.

### Gate — **not met; blocked on hardware**

The gate is P5-4, and P5-4 needs a physical device. It has not been run. Everything that can
be verified without hardware has been, and is listed below so the remaining work is exactly
one item and not a re-audit.

| Check | Result |
| --- | --- |
| `nix build .#fx3-firmware` | Builds. `$out` contains `firmware.elf` (2.2 MB), `firmware.img` (111 KB), `firmware.map` (1.8 MB) and nothing else |
| Version reaches the descriptor | `-- Firmware version: deadbeef` at configure; `Domesday Duplicator (deadbeef)` found **in `firmware.img` as UTF-16LE at offset 0x4b96**, preceded by size byte `0x3e` (62) and type `0x03` — 62 is the correct computed length for a 30-character string |
| `fx3-mkimage` comes from its own derivation | `-- fx3-mkimage: using /nix/store/…-fx3-mkimage-1.0/bin/fx3-mkimage`. No host-compile fallback, so D5 is fully resolved on the Nix path |
| P5-7 byte identity | `firmware.img` built with `fx3-mkimage` is byte-for-byte the image built with the vendored Cypress `elf2img`: both SHA-256 `4938a7d1…4bcd4`, 111,316 bytes, 4 sections. **Not repeatable** — the vendor tool has been deleted, which was the point |
| P5-7 test suite | 32 tests pass, including one that reproduces AN76405's own worked checksum example (`0x6AF37AF2`) and two full-image golden vectors |
| P5-8 pruning is inert | After removing four SDK archives and two dead config files, `firmware.img` is unchanged; `firmware.elf` differs in 93 of 2,247,868 bytes, all in a DWARF path string |
| SDK from a narrowed store path | `-- Using CyFX3 SDK at: /nix/store/…-source`, containing only `inc`, `fx3_release` and `fx3.ld` |
| P5-5 descriptor golden test | Passes, in the sandbox and in the dev shell. Two cases of different lengths, so the computed size byte is actually exercised |
| `nix flake check` | **all checks passed** — 77 tests across four components |
| Non-Nix build still works | `cmake` + `cmake --build` + `ctest` + `cmake --install` in `nix develop .#fx3`: firmware built, test passed, artefacts installed to `<prefix>/bin` as before. The git fallback produced the real hash (`d0566b3e`) |
| `nix build .#gui` (P5-6) | Builds. `installCheckPhase` runs all three binaries: `DomesdayDuplicator 2.1 (deadbeef)`, `dddconv 1.0 (deadbeef)`, `dddutil 1.0 (deadbeef)` |
| GUI non-Nix build (P5-6) | `-- Application version: d0566b3e`; all 21 tests pass; both `--version` outputs carry the real hash |
| About dialogs (P5-6) | Rendered offscreen and inspected: "Version 2.1 (deadbeef)" and "Version 1.0 (deadbeef)" both fully visible, nothing clipped |

**12. The udev `uaccess` tag was dead for ordering reasons, and the `MODE="0666"` fallback
hid it (P5-9, D23).** P3-3's NixOS gate was left "not verified, and cannot be here" in Phase
3. Verifying it on real hardware in Phase 5 found the rule was installed, active, matching —
and applying no ACL at all. `udev` processes rule files in lexical order; systemd consumes
the tag in `73-seat-late.rules`; the file was `88-cyusb.rules`. The tag was being set 15
files too late.

`MODE` is not affected by ordering — udev accumulates it across all matching rules and
applies it when the node is created — so the rule *worked*, by the fallback the comments
described as being for "systems without logind". The half that was supposed to be the modern,
correct mechanism had never once run. **A rule that half-works is harder to find than one
that fails**, which is the general lesson: the observable behaviour was right for the wrong
reason on every machine it had been tried on.

Renamed to `70-domesday-duplicator.rules`. The filename is now load-bearing and says so, in
the rule file, in `package.nix`'s install check and in `nix/modules/udev.nix`.

**13. A commit hash from a dirty tree names a commit that does not contain the build.** The
version fallback added in P2-6 ran `git rev-parse --short=8 HEAD` and stopped there, so any
developer build with uncommitted work stamped a clean commit hash. Caught at the bench: the
tree had 42 changed or untracked paths — the whole of Phase 5 — and the firmware would have
gone onto the device claiming to be `d0566b3e`, a commit predating all of it. Both
`fx3/firmware/CMakeLists.txt` and `gui/CMakeLists.txt` now append `-dirty` when
`git status --porcelain` is non-empty. Untracked files count deliberately: in this very tree
`fx3/mkimage/` was untracked and part of the build. This does not affect Nix or CI builds,
which pass the version in explicitly — it affects exactly the local builds most likely to end
up on someone's bench.

### Bench session, 2026-08-12 — P5-4 part one

Run on a NixOS host (`titan`) with the corrected udev rules installed, PMODE jumper J4
fitted, board on USB 3.

| Step | Result |
| --- | --- |
| Device in bootloader mode | `04b4:00f3` — "FX3 micro-controller (DFU mode)" |
| Non-root device access | `crw-rw-rw-+`, `getfacl` shows `user:sdi:rw-`. **First time the `uaccess` half of the rule has ever worked** — see D23 |
| `fx3-programmer -l` | `[0] VID:PID=04b4:00f3 Bus=007 Device=012 Mode=Bootloader (FX3)`, from a non-root shell |
| RAM download | `Successfully uploaded 111300 bytes`, program entry `0x400074e8` |
| Re-enumeration | `04b4:00f3` → `1d50:603b`, negotiated **5000 Mbps (USB 3.00 SuperSpeed)** |
| Product string | `iProduct 2 Domesday Duplicator (d0566b3e-dirty)` — the exact version built, replacing the `460d2a3f` previously flashed |
| Application-mode permissions | `1d50:603b` also gets `user:sdi:rw-`, so the capture GUI can open it |

**What this proves, and it is the thing byte-identity could not:** an image built by the
project's own `fx3-mkimage` is accepted and executed by the FX3 boot ROM on real silicon.
The checksum, the section layout, the vector trim, the 64 KiB split and the entry-point
record are all correct against the hardware, not merely against the tool that used to
produce them.

It also closes the last observable part of **D4**: the commit reaches the USB product
descriptor and `lsusb -v` reports it from a running device.

**Still outstanding for the P5-4 gate:**

1. **The capture-integrity procedure** from [TESTING.md](../TESTING.md) §5 — a capture with
   the FPGA test-pattern generator and `dddutil` analysis, zero sequence breaks. Enumeration
   proves the device boots; it says nothing about whether the capture path is intact.
2. ~~**A flash (EEPROM/SPI) operation**~~ — **done, see below.**

### Bench session, 2026-08-12 — permanent programming, and D13 closed

Run at the maintainer's explicit request. The hardware is a **Cypress FX3 SuperSpeed Explorer
Kit (CYUSB3KIT-003)** plugged into the main board's GPIF II headers; it boots from an **I2C
EEPROM** on the kit, and there is no SPI flash in this setup (D24).

| Step | Result |
| --- | --- |
| Secondary loader located | `/nix/store/…-fx3-programmer-1.0/share/domesday-duplicator/cyfxflashprog.img`, with `$FX3_FLASH_PROG` **unset** and the command run from `/`. **This is D13's fix working on hardware** — every candidate path used to be working-directory relative, so an installed binary could not find it at all |
| Loader downloaded and detected | 106,408 bytes to RAM; device re-enumerated `04b4:00f3` → `04b4:4720`; `Found FX3 flash programmer` |
| EEPROM programmed | 111,348 bytes padded to 111,360, written as **two 64 KB I2C slave chunks** — so the slave-address rollover in `fx3-paging.h` was genuinely exercised, not bypassed |
| Inline verify | Each chunk read back and compared during the write |
| Independent verify | `-p <file> -v`: `Verification successful: EEPROM matches` |

**D13 is closed.** The remaining half of that defect was always "can an installed binary find
the secondary loader?", and it now demonstrably can, from a store path, from any working
directory, with no `cyusb_linux` checkout present.

The paging arithmetic tested in `fx3/programmer/tests/test_paging.cpp` is now backed by a
hardware run that crosses the boundary those tests describe.

A firmware image that boots but has not been capture-verified is not done.

## Phase 6 — FPGA flake — **DONE except the hardware gate**

Executed 2026-08-12 on `20260812-002`. Ten of the eleven tasks are complete. **P6-5 is
outstanding and cannot be closed here** — it needs a physical Domesday Duplicator and a
known disc. The phase gate is P6-5, so *the gate is not met*; the gate section below lists
exactly what has and has not been shown.

P6-3 was not needed. Quartus 25.1 compiles the committed 2017-era sources unchanged, with no
upgrade prompt and no rejected parameters, which is what the contingency existed for.

Depends on P0-3, whose software half this phase verified.

| Task | Size | Detail |
| --- | --- | --- |
| **P6-1** Quartus flake ✅ | M | A second `import nixpkgs { config.allowUnfree = true; }` of the *same locked* input inside the root flake — no second `flake.lock`, no `--impure`. `.override { supportedDevices = [ "Cyclone IV" ]; withQuesta = false; }`. `packages.x86_64-linux.bitstream` and `devShells.x86_64-linux.fpga-quartus`, both guarded by system. The USB-Blaster rule is `fpga/configs/70-altera-usb-blaster.rules`, installed by `nix/modules/udev.nix` under a new `usbBlaster` option |
| **P6-2** Headless compile, convert and program flow ✅ | M | `fpga/package.nix` runs `quartus_sh --flow compile` then `quartus_cpf -c`, with `HOME=$TMPDIR`; `fpga/build-local.sh` does the same out of tree for interactive work. No GUI at any step, and `quartus_pgm` reads the committed `.cdf` files, which the derivation installs alongside the bitstream so `$out` is enough on its own to program a board |
| **P6-3** IP regeneration | L | **Not needed.** 25.1 accepted the committed `dcfifo`/`altpll` instantiations as they stand. Retained as a contingency for a future Quartus |
| **P6-4** `fpga/README.md` ✅ | S | Rewritten: canonical version 25.1, the manual-install fallback, P6-9's measured answer in full, the dev shell as the deliverable, the generated `.v` files as source of truth, and what the lint waivers mean |
| **P6-5** Hardware verification — **part one done, capture outstanding** | L, **HW** | The flake-built bitstream loads onto real silicon and the board survives it — bench session below. What remains is the TESTING.md capture-integrity procedure (zero sequence breaks) and a known-disc capture compared against one from the shipped 18.0-built bitstream |
| **P6-6** `verilator --lint-only` check ✅ | S | `-Wall` over the five hand-written modules, via `fpga/tests/run-lint.sh`, as the `fpga-lint` flake check. Waivers with written reasons in `fpga/verilator-waivers.vlt` |
| **P6-7** Gateware testbenches ✅ | M | `tb_dataGenerator.v`, `tb_fx3StateMachine.v` and `tb_statusLED.v` under Icarus Verilog, via `fpga/tests/run-sim.sh`, as the `fpga-sim` flake check. TESTING.md §6 records that `buffer.v` is consequently untested and why |
| **P6-8** Bitstream provenance record ✅ | S | `fpga/bitstream-provenance.py` emits `bitstream-provenance.txt` into `$out`: commit, device and family, Quartus version and word size, host architecture, Fitter seed, parallel processors, and both digests per artefact |
| **P6-9** Measure reproducibility, do not assume it ✅ | S | **Answered.** Four compiles of the same commit — two locally, one with the P6-11 settings pinned, one inside a Nix build sandbox — produce a **byte-identical `.jic`** and a `.sof` differing in 32–34 bytes of 704,015, every one of them header metadata. Detail below |
| **P6-10** Publishable bitstream digest ✅ | S | Both digests, per artefact. For the `.jic` they are the same number; for the `.sof` the canonical one masks the four fields Quartus varies per run. Fail-loud, and the offsets are fixed by `fpga/tests/test_provenance.py` (the `fpga-provenance` check) |
| **P6-11** Pin the determinism-relevant settings ✅ | S | `SEED 1` and `NUM_PARALLEL_PROCESSORS 4` in the `.qsf`. Both are Quartus' defaults; pinning them was verified inert — the `.jic` is unchanged. `ROUTER_TIMING_OPTIMIZATION_LEVEL` left alone, because P6-9 showed no routing variance to control |

### P6-9 in full: what a rebuild actually produces

The question earlier drafts of this plan got wrong by assuming. The answer is better than
either alternative the task anticipated.

| Artefact | Across four builds |
| --- | --- |
| `DomesdayDuplicator.jic` | **Byte-identical.** 8,388,833 bytes, `sha256:95480a5f…` every time |
| `DomesdayDuplicator.sof` | 32–34 differing bytes of 704,015. **Zero of them configuration data** |

The `.sof` differences are enumerable in full:

| Where | What | Bytes |
| --- | --- | --- |
| After `design_hash.bin` | A per-run design hash | 10 |
| `md5_digest_80b="…"` | The same hash again, as ASCII hex in the SLD project info | 20 |
| Two places | A 32-bit little-endian Unix compile timestamp, twice | 4 + 4 |
| End of file | The file checksum, which covers all of the above | 2 |

The timestamps decode to the wall-clock times of the builds, 54 seconds apart — which is what
identifies them as timestamps rather than as anything to do with the design.

**The identical `.jic` is the proof, and it was free.** `quartus_cpf` derives the `.jic` from
the `.sof`'s configuration payload and drops the header, so `.jic` identity says the
configuration content is identical without anyone having to trust a hand-derived mask. It is
also the artefact that is actually programmed into the EPCS64 flash, so the reproducible one
is the one that matters.

Consequence for P6-10: **the `.jic`'s plain SHA-256 is already a canonical digest.** The
`.sof` canonicaliser exists so the volatile-configuration file is verifiable too, not because
the release depends on it.

### Deviations and findings

**1. Quartus 25.1 compiles the 2017-era project unchanged — P0-3's software half is
verified.** `0 errors, 52–56 warnings`, in 16 seconds. No upgrade prompt, no rejected
`intended_device_family`, no deprecated-parameter errors. P6-3 was written for the case where
this failed, and it did not. P0-3's remaining half is the hardware capture, which is P6-5.

**2. Quartus rewrites the `.qsf` in place, so no build may run in `fpga/src/`.** Every compile
updates `LAST_QUARTUS_VERSION` from `18.0.0 Lite Edition` to `25.1std.0 Lite Edition` and
drops about thirty build products beside the sources. That would dirty a tracked file on
every build. Both build routes copy to a build directory first: the derivation into its
sandbox, `build-local.sh` into `fpga/build/`. The README says so, and so does the Quartus
shell's banner.

**3. Quartus runs inside the Nix build sandbox, which was not a given.** nixpkgs packages it
as a `buildFHSEnv`, which uses `bubblewrap`, and nested user namespaces inside the Nix
sandbox are a common failure mode for FHS-env packages. It works here: `nix build .#bitstream`
compiles, converts and installs, and produces a `.jic` byte-identical to a build run outside
the sandbox entirely. That is a stronger reproducibility result than the plan asked for — two
different environments, same output.

**4. The design sketch's `installPhase` was wrong about where the outputs land.**
[nix-flake-design.md](nix-flake-design.md) §6 has `cp output_files/*.sof`. There is no
`output_files/` — the project sets no `PROJECT_OUTPUT_DIRECTORY`, so Quartus writes
`DomesdayDuplicator.sof` into the project directory itself. The derivation installs from
there, plus the reports and the two `.cdf` files.

**5. One gateware change, and it is provably bit-neutral.** `statusLED.v` gained
`parameter timerLimit = 32'd4000000;` in place of the hardcoded constant, because a testbench
for it otherwise needs 56 million clock edges to see one full pattern — minutes of simulation
for a module that can be checked in a few thousand cycles with the limit overridden. The
default is the original value, and the claim that this changes nothing is not an argument:
the `.jic` built after the change is byte-identical to the one built before it. The trailing
newlines added to four sources for POSIX conformance are covered by the same comparison.

**6. Lint findings are waived with reasons, not fixed.** `-Wall` reports nine things on this
source: a blocking assignment in a clocked block (three sites), two incomplete `case`
statements, three implicit width promotions, and unused `fx3_control` bits. All are correct
as written, and all are in gateware that has been in the field since 2018. Fixing them is a
change whose gate is a capture-integrity run, not a change that accompanies adding a linter —
so `fpga/verilator-waivers.vlt` records each with its reason and each is pinned by a
testbench. The blocking-assignment waiver is the one that carries real weight, and its
rationale is the packet-length assertion in `tb_fx3StateMachine.v` rather than a claim.

Waivers match on rule, file and message text rather than line number, so they survive edits
above them; and a `.vlt` file cannot contain a comment whose first word is the linter's own
name, which is parsed as a pragma.

**7. The `.sof` canonicaliser was wrong on first write, and only a real pair of bitstreams
caught it.** The ASCII-hash mask double-counted its anchor length and zeroed 20 bytes
starting 16 too far in. Every symptom of that bug is silent: the anchor is found, the right
number of bytes is zeroed, a well-formed digest comes out, and it matches nothing. It was
found by running the tool over two real builds and noticing the canonical digests disagreed
when the `.jic` files were identical. `tests/test_provenance.py` now fixes each field's
offset against a synthetic header, and asserts the converse too — that a change to the
configuration payload is *not* masked, which is what stops the masking growing until every
bitstream looks the same.

**8. `checks` is no longer "the packages".** It was `checks = forAllSystems (pkgs:
self.packages.${system})`. `bitstream` must not be in it (P7-4), and the gateware checks are
not packages, so it is now `removeAttrs … [ "bitstream" ]` plus the three `fpga-*` checks.
`removeAttrs` rather than never adding it, so a future unfree package cannot reach `checks`
by being forgotten about.

**9. The USB-Blaster rule is a package, not `services.udev.extraRules`.** `extraRules` writes
`99-local.rules`, and systemd consumes the `uaccess` tag in `73-seat-late.rules` — so the
convenient route would have reintroduced D23 exactly, in a new file, with the same invisible
half-working symptom. The rule is `70-`prefixed and installed through
`services.udev.packages`.

**10. `buffer.v` is untested, and that is the honest cost of free simulation.** It is two
`dcfifo` instances and the ping-pong logic between them, and `dcfifo` has no free simulation
model. It is one of the two modules where a defect shows up as dropped samples rather than as
a device that does not work, so this is worth stating rather than leaving to be inferred from
a list of three testbenches. Recorded in TESTING.md §6 with what it would take to close.

### Gate — **not met; blocked on hardware**

The gate is P6-5, and P6-5 needs a physical device and a known disc. Everything that can be
verified without one has been, and is listed below so the remaining work is exactly one item
and not a re-audit.

| Check | Result |
| --- | --- |
| Quartus 25.1 compiles the committed sources | Yes, unchanged, `0 errors`, 16 seconds |
| `nix build .#bitstream` | Builds in the sandbox. `$out` has `.sof` (704,015 bytes), `.jic` (8,388,833), `.map`, both `.cdf` files, `bitstream-provenance.txt` and eight compilation reports |
| Bitstream reproducibility (P6-9) | `.jic` byte-identical across four builds in two environments; `.sof` differs only in 32–34 bytes of header metadata |
| `.qsf` pinning is inert (P6-11) | `.jic` unchanged after adding `SEED 1` and `NUM_PARALLEL_PROCESSORS 4` on a 16-core machine — so the processor count does not affect the fit either |
| Gateware edits are inert | `.jic` unchanged after the `statusLED.v` parameter and four trailing newlines |
| Canonical digest agrees across rebuilds | `sha256:254e3535…` from all four `.sof` files, whose release digests are all different |
| `installCheckPhase` | Fails the build if the `.sof`, `.jic` or provenance record is missing or empty, or if the record says `unknown` for the commit |
| `fpga-lint` | 5 modules clean under `-Wall` |
| `fpga-sim` | 3 testbenches pass, ~4.5 s |
| `fpga-provenance` | 12 checks pass |
| `nix flake check` | **all checks passed**, with `bitstream` correctly absent and the three `fpga-*` checks present |
| NixOS module | Evaluates with `usbBlaster` on; `services.udev.packages` carries `altera-usb-blaster-udev-rules`, which installs `lib/udev/rules.d/70-altera-usb-blaster.rules` |
| `nixfmt --check` | Clean on every new and modified `.nix` file |

**Partly verified on live hardware, 2026-08-12.** A DE0-NANO was attached during the phase,
which allowed the *need* for the rule to be demonstrated rather than assumed:

| Check | Result |
| --- | --- |
| The board's blaster enumerates | `09fb:6001 Altera Blaster`, on bus 007 |
| The rule's match keys against the real device | `SUBSYSTEM=="usb"`, `ATTR{idVendor}=="09fb"`, `ATTR{idProduct}=="6001"` all present on the device itself — so `ATTR{}` is correct and `ATTRS{}` would be wrong |
| `udevadm verify` | Passes |
| Any existing rule matching `09fb` | **None** on this machine, confirming nixpkgs' Quartus ships none |
| Device node before the rule | `crw-rw-r-- root root`, `getfacl` shows no user entry, `CURRENT_TAGS=:seat:` — no `uaccess` |
| `jtagconfig` as a non-root user | `1) USB-Blaster variant [7-3.2]` / `Unable to lock chain - Insufficient port permissions` |

So the failure this rule fixes is observed, not predicted.

**Then the rule was installed, the machine rebooted, and the same checks repeated — it
works.** This is the claim D23 showed cannot be taken from a reading of the file, so it is
the before-and-after that matters rather than the after alone:

| Check | Before | After |
| --- | --- | --- |
| Device node | `crw-rw-r-- root root` | `crw-rw-rw-+` — note the `+` |
| `getfacl` | no user entry | `user:sdi:rw-` |
| `CURRENT_TAGS` | `:seat:` | `:uaccess:usb_blaster:seat:` |
| `jtagconfig` | `USB-Blaster variant [7-3.2]` / `Unable to lock chain - Insufficient port permissions` | `USB-Blaster [7-3.2]` / `020F30DD  10CL025(Y\|Z)/EP3C25/EP4CE22` |

Three details worth keeping:

- **The `uaccess` half is doing the work, not the `MODE` fallback.** The ACL entry is present,
  which is precisely what was absent for years under D23. A `0666` node with no user entry
  would have looked identical to a caller and been the same latent bug in a new file.
- **"USB-Blaster *variant*" was itself a permissions symptom.** Quartus could not open the
  device far enough to identify the cable, so it fell back to a generic name. It reports the
  real one now.
- **The scanned IDCODE `020F30DD` is an `EP4CE22`** — the device the `.qsf` targets. So the
  chain is not merely reachable, it is the expected part.

The FX3 in application mode (`1d50:603b`) was checked in the same pass and also carries
`user:sdi:rw-` and `:uaccess:ddd_dev:seat:`, so the Phase 5 rule is still intact after the
change.

The system side is `/etc/nixos/modules/domesday-duplicator.nix`, which now carries the
USB-Blaster rules alongside the FX3 ones. They are duplicated by hand there rather than
imported from `nixosModules.udev` because that module can only be reached through a
resolvable flake URL, and this work is on an unpushed local branch; the module's own comment
already forbids pointing a system config at a home directory. The repository's own rule file
and NixOS module are the ones that ship — they are what a user of this project gets — and
this machine is a hand-kept mirror of them until the branch lands.

### Bench session, 2026-08-12 — P6-5 part one

Run immediately after the udev verification above, on `titan`, with a DE0-NANO attached via
its on-board USB-Blaster and the FX3 already running the Phase 5 firmware.

The artefact programmed was **the derivation's own output**, copied out of
`/nix/store/…-domesday-duplicator-bitstream-0/` — not a local compile — so this exercises
`nix build .#bitstream` end to end rather than just Quartus.

| Step | Result |
| --- | --- |
| Cable and device | `Using programming cable "USB-Blaster [7-3.2]"`, non-root |
| Programming file | `./DomesdayDuplicator.sof`, checksum `0x001D67A1`, for `EP4CE22F17@1` |
| JTAG ID read back | `0x020F30DD` — matches the `.qsf`'s `EP4CE22F17C6` |
| Configuration | `Configuration succeeded -- 1 device(s) configured`, `0 errors, 0 warnings`, 1 second |
| Board after reconfiguration | Still enumerated: `1d50:603b`, `bcdUSB 3.00`, **5000 Mbps SuperSpeed**, `iProduct Domesday Duplicator (d0566b3e-dirty)` |

**What this proves:** a bitstream produced by the packaged, hermetic build loads onto real
silicon through the documented headless path, and the board does not fall over when it does.
Combined with the byte-identical `.jic` across five builds, the artefact the flake produces
is both reproducible and accepted by the hardware.

**What it explicitly does not prove, and the distinction matters:** that the capture path is
intact. The FX3 had already enumerated *before* the FPGA was reconfigured — USB enumeration
happens when the FX3 boots — so the device still being present at SuperSpeed afterwards is
consistent with, but not evidence of, correct gateware. Only the capture-integrity procedure
tests the path that actually carries samples, and dropped samples are the failure mode that
does not announce itself. **P6-5 is not closed.**

The `.sof` is volatile: a power cycle restores whatever the EPCS64 flash holds, so the board
is not left in a modified state by this session. Nothing was written to flash.

**Outstanding for the maintainer before the flake evaluates:** Nix reads only git-tracked
files, so the new `fpga/*.nix`, `fpga/tests/`, `fpga/configs/` and `fpga/*.py`/`*.sh` files
must be added to the index before `nix flake check` works from the repository itself. Every
result above was obtained by building the same derivations from the working tree directly.

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

## Phase 7 — CI: build every artefact on commit, publish two release streams — **BUILT; CI AND HARDWARE GATES OUTSTANDING**

This phase carries the maintainer requirement from
[Release artefacts and provenance](#release-artefacts-and-provenance): the GUI, the FX3
firmware and the FX3 programmer are built by CI **on every commit**, and a release contains
exactly the artefacts built from the release commit. The FPGA bitstream is deliberately
excluded from CI for now (§4 of that section); it is attached by hand to the firmware
release (P7-11, P8-3).

**Decided 2026-08-12, and it reshapes this phase:**

1. **Two release streams, two tag prefixes.** `gui-v*` publishes the GUI; `fw-v*` publishes
   the FX3 firmware, the FPGA bitstream and the programmer. Independent versions, independent
   cadence, independent release notes and `SHA256SUMS`.
2. **The GUI ships as a Linux Flatpak, a macOS DMG and a Windows MSI**, matching decode-orc's
   packaging. These replace the five per-platform archives as release assets.
3. **`dddconv` and `dddutil` are removed** from the tree (P7-12), leaving `DomesdayDuplicator`
   as the only GUI binary — but not before `dddutil`'s test-data analysis is ported into the
   capture application (P7-19), because that analysis is step 4 of the T5 gate.
4. **The capture application stops writing packed 10-bit `.lds` and writes `.ldf` instead** —
   the same Ogg FLAC container ld-decode already consumes (§7b). This lands in this phase
   because it changes what a release *is*, and because it adds libFLAC to every build and
   packaging path, so it has to be settled before the installers are written.
5. **Everything runs on every branch push; only publishing waits for a tag**
   (maintainer, 2026-08-13). The three installers are built, installed and launched on every
   push rather than first exercised on the way to a release. Path filtering is kept for pull
   requests alone. This reverses the earlier "path-filtered on every ref" position and costs
   CI minutes deliberately: an installer is only proven by being built, and Phase 7 exists to
   know that the whole chain works, not to know it cheaply.

The task numbering below is not in document order: P7-1…P7-11 predate these decisions and are
referenced from elsewhere in this document, so they keep their numbers, and the new work
continues from P7-12 rather than renumbering everything. Within each subsection the tasks are
in the order they must be done.

### 7a. Build on commit

| Task | Size | Detail |
| --- | --- | --- |
| **P7-1** Single `build.yml` | M | `nix-installer-action` + `magic-nix-cache-action`, then `nix build .#gui .#fx3-firmware .#fx3-programmer .#docs-site`. Replaces the three per-submodule workflows (including the one fixed in P2-3). **Revised 2026-08-13:** every push on every branch builds and packages everything, including the three installers, so the whole chain is validated end to end on the branch rather than first exercised on the way to a release. Path filtering survives for **pull requests only**, where the branch push for the same commits has already produced a complete run. Tags do not trigger this workflow at all — the release workflows call the same reusable packaging jobs, and listing tags here too would build every installer twice per release |
| **P7-2** Keep the native build matrix | M | The current GUI workflow builds Linux x64/ARM64, macOS x64/ARM64 and Windows x64. **Nix cannot produce the Windows binary**, so those five jobs stay, driven from the new paths. Nix is additive here, not a replacement. This is why the GUI has two build paths and the firmware only one. **Changed by the packaging decision:** the matrix is now a compile-side regression check *and* the input to the packaging jobs — its archives stop being release assets (P7-14…P7-16 produce those instead) and become 30-day CI artefacts under P7-7. Drop the `dddconv` and `dddutil` copy and verify steps when P7-12 lands, leaving one binary per platform |
| **P7-3** Pages deploy | S | Already done in P4-9: `nix build .#docs-site` → `upload-pages-artifact` with `path: ./result` → `deploy-pages`. Confirm the path filter still matches `docs/**` |
| **P7-4** `nix flake check` in CI | S | Runs T1–T4 for every component. Excludes `bitstream` — unfree, GB-scale, `x86_64-linux` only |
| **P7-5** Delete per-component `.github/` dirs | S | `fx3/.github/` and `gui/.github/` are inert (GitHub only reads the repository root) but actively misleading once `build.yml` exists |
| **P7-6** Test lanes | S | Tiers T1–T4 in the consolidated workflow. **T5 never runs in CI** — it needs a physical DdD, and a test that silently "passes" because no hardware was attached is worse than no test |
| **P7-7** Retain artefacts from every commit build | S | `actions/upload-artifact` with a name carrying the short SHA, so a build from any commit can be fetched without re-running CI. Default retention is 30 days; set it explicitly rather than inheriting it, and note in the workflow that this is **not** the release archive — GitHub expires these |

### 7b. Capture output format — FLAC (`.ldf`)

New work, added 2026-08-12. **The capture application drops packed 10-bit output and writes
Ogg FLAC instead**, mimicking ld-decode's `.ldf` so the file it produces is the file the decode
toolchain wants, with no conversion step in between.

The groundwork is already there, which is why this is a contained change rather than a rewrite.
The app's existing "16-bit Signed Scaled" format
([UsbDeviceBase.cpp:1015](../gui/src/DomesdayDuplicator/UsbDeviceBase.cpp#L1015)) computes
`(tenbit - 512) << 6`, and ld-decode's `lddecode/lds.py` documents that exact expression as
"the DdD 16-bit format" and unpacks `.lds` into it before handing it to `flac`. **The sample
values are already correct; what is missing is the container.** So the change is: stop packing,
keep the 16-bit conversion the app already performs, and put a FLAC encoder on the end of it.

The target format, byte-exact, from `lddecode/compress.py`'s encoder invocation — mono,
16-bit signed little-endian, Ogg-encapsulated FLAC, sample rate stamped `40000` (FLAC cannot
express 40 MHz, so the field is a label, not a rate). Reproducing those settings is what makes
the output a real `.ldf` rather than something that merely resembles one.

This subsection comes before packaging because it adds a dependency — libFLAC — to the Nix
package, the Flatpak manifest, the DMG bundle and the MSI harvest.

| Task | Size | Detail |
| --- | --- | --- |
| **P7-21** Write `.ldf` from the capture application | L | Add a `FlacWriter` on the disk-writer side of the existing buffer pipeline, fed by the `Signed16Bit` conversion that already exists. **Encode in-process with libFLAC (≥ 1.5), not by piping to a `flac` binary** — three installers would each have to bundle, locate and version-check an external executable, which is the exact trap `cyfxflashprog.img` set in D13, and a subprocess on the capture path is one more way a capture dies at minute 40. libFLAC 1.5 has multithreaded encoding (`FLAC__stream_encoder_set_num_threads`), which is what `ld-compress`'s `-j` uses; Ogg encapsulation is `FLAC__stream_encoder_init_ogg_*`. Its BSD licence is GPLv3-compatible. **Compression level is a setting, chosen by measurement, not by copying `ld-compress`'s default of 8** — that tool post-processes a file at leisure, whereas this one has a hard real-time budget; start at 1 and raise it only as far as the numbers allow |
| **P7-22** Drop the packed 10-bit formats | M | Remove `Unsigned10Bit` and `Unsigned10Bit4to1Decimation` from `UsbDeviceBase::CaptureFormat` and `Configuration::CaptureFormat`, the `.lds` branch at [mainwindow.cpp:1390](../gui/src/DomesdayDuplicator/mainwindow.cpp#L1390), and the combo box in `configurationdialog.cpp`. Two traps. **(a) Settings migration:** `configuration.cpp` persists the format as a bare int — `0` = `tenBitPacked`, `1` = `sixteenBitSigned`, `2` = `tenBitCdPacked` — so every existing installation has `0` or `2` stored, and renumbering the enum silently reinterprets it. Map the old integers explicitly on read and log the migration; do not let a stale config quietly select a different capture format than the user chose. **(b) CD decimation is a feature, not a packing.** 4:1 decimation for CD RF is welded to the packed format today, and dropping the packing must not drop it: re-express decimation as an orthogonal option feeding the same writer, and stamp the FLAC sample rate `10000` on a decimated capture so the file says what it is |
| **P7-23** Read `.ldf` back, and prove it is really `.ldf` | M | P7-19's `--analyse-test-data` has to read what the app now writes, so it decodes `.ldf` (libFLAC is already linked) plus legacy `.lds`/`.raw`. Keep the *unpack* half of `samplecodec.h` and its T1/T2 tests for the legacy path; the *pack* half goes with P7-22. The interop test is the one that matters and it is cheap: a short capture written by the app must (i) pass `flac -t`, (ii) uncompress via `ld-compress --uncompress` to exactly the `.lds` the old path would have written from the same samples, byte for byte, and (iii) be readable by ld-decode itself. (i) and (ii) are golden-file T2 work; (iii) is a manual cross-check at the T5 bench, and it is the only one that proves the file is usable by the tool it exists for |
| **P7-24** libFLAC into every build and packaging path | M | `gui/package.nix` and `gui/shell.nix` (`flac`), Homebrew `flac` on the DMG runner, `mingw-w64-ucrt-x86_64-flac` in MSYS2, and the Flatpak — check whether the freedesktop runtime under `org.kde.Platform` already carries libFLAC before adding a module for it. CMake picks it up by pkg-config. **`macdeployqt` and `windeployqt` only follow Qt**, so the libFLAC dylib/DLL must be bundled explicitly — `dylibbundler` on macOS, an explicit harvest entry in WiX — and this is precisely the class of omission that builds green in CI and fails on a user's machine, so P7-14…P7-16's install checks must launch the packaged app and complete a short capture, not merely launch it |
| **P7-25** Provenance inside the capture file | S | The app already writes a `.json` sidecar ([mainwindow.cpp:785](../gui/src/DomesdayDuplicator/mainwindow.cpp#L785)), which is lost the moment someone moves the capture. Ogg FLAC carries Vorbis comments, so write the application version and commit (D21), the real sample rate, the decimation setting and the capture date into the file itself. Cheap, and it extends §3's traceability from the binaries to the data they produce. Decoders ignore comments they do not know, so this cannot break ld-decode |

**Consequences to carry into the rest of the plan.** The capture-integrity procedure
([TESTING.md](../TESTING.md) §5) now produces an `.ldf`, so P7-19's analysis must handle it
before the T5 gate can be run again. `.lds` stops being produced but does not stop existing —
years of captures and every third-party document assume it — so the documentation (P7-20) has
to name `ld-compress --uncompress` as the way back for anything that still wants one. And the
throughput arithmetic is worth stating plainly, because it is the argument for the whole
change: at 40 Msps the packed path writes 50 MB/s and the raw 16-bit path 80 MB/s, while
`.ldf` runs about half the size of `.lds` — roughly 25 MB/s. This *halves* the write rate and
removes a conversion step, at the cost of putting a compressor on the real-time path. That
trade is the risk, and P7-21's gate is where it gets measured rather than assumed.

### 7c. GUI packaging — one application, three installers

New work, added 2026-08-12. The model is decode-orc's: one reusable `workflow_call` workflow
per format, invoked from the main workflow on every non-PR push so the installers are testable
per branch, and invoked again by the release job on a `gui-v*` tag. Everything packaging-related
lives under **`gui/packaging/`** — `flatpak/`, `macos/`, `windows/` and a shared `assets/` — so
none of it litters the repository root.

| Task | Size | Detail |
| --- | --- | --- |
| **P7-19** Port test-data analysis into the capture application | M | **Prerequisite for P7-12.** `dddutil/analysetestdata.cpp` is step 4 of the capture-integrity procedure ([TESTING.md](../TESTING.md) §5): it walks a capture taken with the FPGA test-pattern generator running and checks the ramp is unbroken. That is the T5 gate P5-4 and P6-5 are blocked on, so it cannot be deleted along with its host application. Move it into `gui/src/DomesdayDuplicator/` behind **both** a menu item and a `--analyse-test-data <file>` command-line mode — which is P8-6, promoted from optional follow-up to a prerequisite, because a headless mode is what lets the gate be scripted instead of clicked. It needs the 10-bit unpacker, so it is the production consumer that keeps `samplecodec.h` alive. Verify against a known-good capture and a deliberately corrupted copy: the analysis must pass the first and report the exact break offset in the second |
| **P7-12** Remove `dddconv` and `dddutil` | M | Delete `gui/src/dddconv/` and `gui/src/dddutil/`, their `add_subdirectory` lines in `gui/CMakeLists.txt`, and their references in `gui/package.nix` (`installCheckPhase` runs all three binaries), `gui/README.md`, `gui/BUILD.md`, root `README.md`, `AGENTS.md`, `TESTING.md`, `docs-tech/agents-and-testing.md`, `docs/content/development/hardware-programming/fpga-bitstream.md` and the native CI matrix. **`samplecodec.h` must not go with them:** it is the only unit-tested (T1) and golden-file-tested (T2) code in the GUI. Move it to `gui/src/common/samplecodec.h`, have P7-19's analysis mode use it, and keep `test_samplecodec`. Note that `dddutil` open-codes the same unpack by hand in `inputsample.cpp` — port the analysis onto the *tested* codec, not the hand-rolled copy, and if the two disagree, stop and file that as a defect rather than forcing the refactor through. Every procedure that currently says "open the file in `dddutil`" is rewritten to the new mode in the same commit; leaving a test procedure pointing at a deleted binary is how a gate silently stops being run |
| **P7-13** Desktop metadata and icons | M | `gui/packaging/assets/`: a `.desktop` file, an AppStream `metainfo.xml` (Flathub requires it; it is also what gives the app its name, summary, screenshots and `<releases>` history in GNOME Software and Discover), PNG icons at 64/128/256 px derived from `graphics/`, plus the `.ico` for WiX and the `.icns` for the app bundle. Add `appstreamcli validate` and `desktop-file-validate` to the T4 lane so a malformed manifest fails CI rather than a Flathub review. The `<release version=…>` entry has to be updated per `gui-v*` tag — generate it from the tag in the packaging job rather than hand-editing, or it will silently go stale |
| **P7-14** Linux Flatpak | L | `gui/packaging/flatpak/<app-id>.yml`, built by `flatpak-builder` on `ubuntu-latest` and exported with `flatpak build-bundle` to a single-file `.flatpak`. Runtime `org.kde.Platform` 6.9 + `org.kde.Sdk` (Qt 6, same as decode-orc). `finish-args`: `--share=ipc --socket=wayland --socket=x11 --filesystem=home` plus `/run/media`, `/media` and `/mnt` for captures on external drives, and **`--device=all`** — Flatpak has no narrower static USB permission, and the capture path is a raw libusb bulk transfer, so this one is unavoidable and needs justifying in the Flathub submission if that ever happens. Build `libusb-1.0` as a module unless the KDE runtime already carries it (check with `pkg-config --modversion libusb-1.0` inside `org.kde.Sdk//6.9` before writing the manifest). **App ID is a maintainer choice:** `io.github.simoninns.DomesdayDuplicator` follows the repository host and needs no domain claim; `com.waitingforfriday.DomesdayDuplicator` is also available and is the stronger identity. Pick before the first release — changing an app ID afterwards is a migration, not an edit |
| **P7-15** macOS DMG | L | `macos-latest` (arm64), Homebrew `qt@6 libusb cmake ninja pkg-config dylibbundler`. Produce a single `DomesdayDuplicator.app`, run `macdeployqt` on it, then `hdiutil create` over a staging folder containing the bundle and an `/Applications` symlink. Copy decode-orc's retry loop around `hdiutil create` — transient volume-attach contention on hosted runners is common and not a real failure. **Unsigned and un-notarised** (a Developer ID is a paid, personal credential and is out of scope), so first launch hits Gatekeeper; the install page must document the right-click → Open path. The bundle carries `CFBundleShortVersionString` from the tag (P7-9) |
| **P7-16** Windows MSI | L | `windows-latest`, MSYS2 UCRT64 (the toolchain the existing native job already uses), `windeployqt` to gather Qt runtime, then WiX v3 via `choco install wixtoolset` — `heat` to harvest the deployed tree, `candle`/`light` to build. Per-machine install into `ProgramFiles64`, one Start-menu shortcut, `MajorUpgrade` for in-place upgrades, and the GPLv3 text as RTF in the licence dialog. **MSI versions must be numeric `x.x.x.x`:** map `gui-vX.Y.Z` → `X.Y.Z.0`, and non-tag builds → `0.0.<run-number>.0` so a development MSI can never out-version a real release and block its upgrade. **The MSI does not install a device driver.** The Windows build has a WinUSB back-end (`UsbDeviceWinUsb.cpp`) and the device needs WinUSB bound to it, which today means Zadig; shipping a driver package would require a signed `.inf` and is out of scope. The installer's docs page covers Zadig, and the app should say so plainly when it cannot open the device. Also unsigned, so SmartScreen will warn — document that too |
| ~~**P7-17** Installation documentation~~ | — | **Merged into P7-20**, which owns the whole capture-application documentation section. Splitting the installer pages from the section they live in would have given two tasks the same files |
| **P7-20** Rework the capture-application documentation | M | `docs/content/capture-application/` currently holds a Linux-only `user-guide.md` — which doubles as the build instructions — and two-line `windows-releases.md`/`macos-releases.md` stubs that only link to the GitHub releases page. Once the GUI ships as three installers, that is wrong in every direction. New shape: an `index.md` that says what the application is and which package to take, one page per package (`install-flatpak.md`, `install-dmg.md`, `install-msi.md`), and a `building-from-source.md`. Delete the two stubs and update `.nav.yml`; per P0-4 the site takes no redirects, so fix the inbound links instead — `development/software-guide.md:235` and `general/overview.md:35`. **`user-guide.md` is left as-is for now** (maintainer, 2026-08-12), so its "Linux Installation" section temporarily duplicates the new build page; that duplication is deliberate and gets resolved when the user guide is rewritten, which is not part of this phase |
| **P7-20a** … the per-package pages | — | Each covers: which file to download, how to verify it against the release `SHA256SUMS`, install, first run, update and uninstall. Then the platform-specific things that packaging cannot fix and that generate every support question — **Linux:** a Flatpak cannot install udev rules, so the user still installs the rules on the host (or the NixOS module from P3-3) before the device is reachable, and the rules file must be a release-adjacent download rather than something buried in the source tree; also the `--filesystem` grants and where captures may be written. **macOS:** the Gatekeeper right-click → Open path for an unsigned, un-notarised app (P7-15). **Windows:** the SmartScreen prompt, and that WinUSB must be bound to the device with Zadig because the MSI ships no driver (P7-16) |
| **P7-20b** … `building-from-source.md` | — | Nix first, since it is the reproducible path: `nix build .#gui` and `nix develop .#gui`. Then the native routes the packaging jobs themselves use, which makes this page the thing that keeps CI and the documentation honest with each other — Ubuntu with the `apt` dependency line (now including libFLAC, P7-24), macOS with Homebrew `qt@6`/`libusb`/`flac`, Windows with MSYS2 UCRT64. State plainly that building from source is for development and for platforms the installers do not cover, and that ordinary users want a package. Link on to the packaging manifests under `gui/packaging/` for anyone building an installer rather than a binary |
| **P7-20c** … the capture format change | — | The `.ldf` switch (7b) is the most user-visible change in this phase and the one most likely to generate "where did my `.lds` go" reports, so it needs saying in the documentation, not just the release notes: what an `.ldf` is, that ld-decode and vhs-decode read it directly with no conversion step, that it is roughly half the size of the `.lds` it replaces, and that `ld-compress --uncompress` converts back for anything that still needs the old format. Also state what the *uncompressed* 16-bit option is for — a machine that cannot sustain the encoder — and how to tell that has happened, since a dropped-sample capture that nobody noticed is the worst outcome here |

### 7d. Publish on release

Two workflows, because there are two streams (§1 of *Release artefacts and provenance*). Both
check out the tag rather than a branch, so the assets are provably from the release commit.

| Task | Size | Detail |
| --- | --- | --- |
| **P7-8** Tag-triggered release workflows | M | **`release-gui.yml`** on `gui-v*` and **`release-firmware.yml`** on `fw-v*`, each with `workflow_dispatch` as well so a failed publish can be re-run without re-tagging. Each builds its own stream's artefacts from the tag and attaches them to one GitHub Release. Following decode-orc, the packaging workflows are `workflow_call`-reusable and the release job is a single job that gathers all three installers and publishes once — parallel jobs each calling create-release race each other and leave partially populated releases |
| **P7-9** Version stamping is a release gate | S | The job fails if any artefact reports `unknown` as its version. Nix builds from a tag have no `.git`, so `-DFIRMWARE_VERSION=`/`-DDDD_VERSION=` must be passed explicitly (D4, D21) — a silent fallback to `unknown` would produce untraceable release binaries, which is exactly what this phase exists to prevent. Extended by the packaging work: the check runs on the *installers*, not just the binaries inside them — MSI `ProductVersion`, DMG `CFBundleShortVersionString` and the Flatpak's AppStream `<release version>` must all equal the tag |
| **P7-10** `SHA256SUMS` and a provenance note | S | Generated over every attached asset of **that stream**, plus a short note recording the source commit, the `flake.lock` nixpkgs revision, and the toolchain versions (Qt, WiX, flatpak runtime for the GUI stream; `arm-none-eabi-gcc` and Quartus for the firmware stream). Attached alongside the binaries |
| **P7-11** Document the FPGA hand-off | S | Now specific to the **firmware** release. The checklist states plainly that the bitstream is **not** produced by this workflow: build it locally per P6-2, then attach the `.sof`, `.jic`, P6-8's `bitstream-provenance.txt` and P6-10's digests by hand. Fold those digests into that release's `SHA256SUMS` so every asset is covered by one manifest regardless of where it was built. An `fw-v*` release must not be published with the firmware present and the bitstream quietly missing |
| **P7-18** Stream independence is enforced, not assumed | S | `gui-v*` must not trigger firmware jobs and `fw-v*` must not trigger packaging jobs — otherwise the split is cosmetic and every release still pays for everything. Tag-pattern `on:` filters, and a release-notes template per stream. Also decide and document what a version *means* per stream: the GUI's is a plain application version, while `fw-v*` covers two artefacts that must be flashed as a pair, so the release notes state which bitstream and which firmware image go together |

### Release asset sets

A complete `gui-v*` release:

```
DomesdayDuplicator-<ver>.flatpak                 CI  (flatpak-builder, Linux x64)
DomesdayDuplicator-<ver>-macos-arm64.dmg         CI  (hdiutil; unsigned)
DomesdayDuplicator-<ver>-windows-x64.msi         CI  (WiX; unsigned)
SHA256SUMS                                       CI
PROVENANCE.txt                                   CI
```

A complete `fw-v*` release:

```
firmware.img / firmware.elf / firmware.map       CI  (nix build .#fx3-firmware)
fx3-programmer-<ver>-linux-x64                   CI  (nix build .#fx3-programmer)
fx3-programmer-<ver>-linux-arm64                 CI  (nix build .#fx3-programmer)
DomesdayDuplicator.sof / .jic                    MANUAL — local Quartus build
bitstream-provenance.txt                         MANUAL — provenance + digests (P6-8, P6-10)
SHA256SUMS                                       CI  (covers the manual assets too, P7-11)
PROVENANCE.txt                                   CI
```

The five per-platform GUI archives are **not** in either list: they are still built every
commit as a compile check and retained as 30-day CI artefacts (P7-2, P7-7), but the installers
are what a release ships.

**Gate:** one `build.yml`, one `release-gui.yml`, one `release-firmware.yml`; no job references
a non-existent path; every push to any branch builds and installs all three GUI installers,
and a pull request touching only `docs/` does not trigger firmware jobs; a `gui-v*` tag
produces a release carrying exactly the three installers plus manifests, and no firmware job
runs for it; an `fw-v*` tag produces a firmware release and no packaging job runs for it; every
asset in both releases reports the tagged commit and none reports `unknown`; each stream's
`SHA256SUMS` covers every file attached to that release. Plus the packaging-specific checks:
the Flatpak installs from its bundle and launches on a machine with no Qt 6 installed; the DMG
mounts and the app launches after the documented Gatekeeper step; the MSI installs, creates its
shortcut, launches, and uninstalls cleanly, and installing a newer MSI over an older one
upgrades rather than side-by-side installs. And for the removals: `grep -ri dddconv\|dddutil`
returns nothing outside this document's historical phase records, and
`DomesdayDuplicator --analyse-test-data` reproduces the verdict `dddutil` gave on the same
capture file — checked before `dddutil` is deleted, not after.

For the documentation: `nix build .#docs-site` still succeeds under `--strict` (so no page
links to a file P7-20 deleted), and every install step on the three package pages has been
followed on the matching platform against the actual release asset. A packaging caveat that
only exists in a workflow file and not on its install page will be found by a user instead.

For the capture format: a sustained capture on the reference machine produces an `.ldf` with
**zero buffer overruns** — measured, not assumed, because a compressor on the real-time path is
the risk this change introduces; the file passes `flac -t`; `ld-compress --uncompress` turns it
into byte-identical `.lds` to what the old path would have written from the same samples; and
ld-decode reads it. No build option, menu entry, config value or documentation page still
offers packed 10-bit output, and an installation upgraded from a pre-change version reports the
capture format the user actually chose rather than whatever the old stored integer now means.

### Deviations and findings

**1. `flac` cannot read a `.ldf` without being told it is Ogg.** The first interop run failed
with `FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC`, which reads exactly like a corrupt file.
The cause is that the `flac` command-line tool infers Ogg encapsulation from the file
*extension*, and `.ldf` is not one it knows, so it parses an Ogg stream as native FLAC.
`flac -t --ogg capture.ldf` works. Worth knowing before anyone concludes a capture is
damaged; it is on the capture-formats documentation page for that reason. libFLAC's own API
has no such problem — the reader calls `init_ogg_file` explicitly.

**2. libFLAC's types cannot be forward-declared.** `FLAC__StreamEncoder` and
`FLAC__StreamMetadata` are anonymous struct typedefs, so `struct FLAC__StreamEncoder;` in a
header is a different type and does not compile. The writer and reader therefore use a pimpl
rather than opaque pointers, which also keeps FLAC's headers out of everything that captures.
The C callbacks are static members of the `Impl` struct, not free functions: a free function
cannot name a private nested type.

**3. `libogg` has to be listed explicitly.** `flac.pc` declares `Requires: ogg` and nixpkgs
does not propagate it, so `pkg_check_modules(flac)` fails without libogg in the inputs — and
then `FindFLAC.cmake` silently succeeds through flac's CMake config instead. The build works
either way here, which is precisely the problem: it would have failed on a platform shipping
only the `.pc` file. Both `shell.nix` and `package.nix` name libogg.

**4. The compression figure is weaker than the plan claimed.** The plan quoted "roughly half
the size of `.lds`" from ld-decode's own documentation. Measured on the synthetic interop
fixture, the `.ldf` is **74.4%** of the packed size (3,720,477 bytes against 5,000,000), not
50%. That fixture is a sine with bounded random jitter, which is more entropic than real
LaserDisc RF, so real captures should do better — but *this* number is what has actually been
measured, and the ~50% figure remains unverified on real signal. The documentation and the
free-space estimate in the application both use the conservative figure, so a pessimistic
remaining-time estimate is the worst case rather than a disk that fills mid-capture.

**5. The `nix` job is a matrix, not one job.** There is one `build.yml`, but `nix flake check`
builds every check there is, so a single Nix job could never honour a path filter at all. The
job is per component; the whole-tree `nix flake check` is a second job. After the 2026-08-13
revision both run on every push anyway, and the split now earns its keep for a different
reason — a failure names the component that failed instead of one job going red for the
whole tree.

**6. Path filtering ended up applying to pull requests alone.** It was originally written to
apply everywhere except `master`, tags and manual dispatch, which needed an explicit override
step because `on: paths:` cannot be made conditional. The maintainer then reversed the
underlying decision (see §7's decision 5), so the override became the normal case: everything
builds unless the event is a `pull_request`. The filter is kept rather than deleted because a
PR's commits have already had a complete run from their branch push, so the filtered run is a
second opinion rather than the only evidence.

Tags were also removed from `build.yml`'s triggers at the same time. `release-gui.yml` calls
the same reusable packaging workflows, so a tag that triggered both would have built every
installer twice.

**7. `.cds` is gone with the format that used it.** The decimated CD capture used to get its
own extension, which nothing downstream could read. Decimation is now a setting rather than a
format, so a CD capture is an ordinary `.ldf` with `10000` stamped as its sample rate and
`DDD_DECIMATION=4` in its tags.

**8. Two things were added that the plan did not list.** `gui/tests/tools/ldfgen.cpp` writes a
capture through the production encoder, and `gui/tests/interop-ldf.sh` drives ld-decode's
tools against it — the format claim cannot be checked without a file the real encoder
produced. Neither is installed. A `capture-formats.md` documentation page was also added:
P7-20c asked for the format change to be documented, and it needed more room than a section
inside an installation page.

**9. Nothing was committed and nothing was staged.** Per the rules at the top of this
document. One consequence matters for verification: Nix reads only git-tracked files, so
`nix build .#gui` cannot see the new sources until the maintainer adds them. Every Nix result
below was obtained by building `gui/package.nix` from the working tree directly, the same way
Phase 6 did.

### Gate — **partly met; the CI half is unexercised**

What was verified, on this machine:

| Check | Result |
| --- | --- |
| `cmake --build` in `nix develop .#gui` | Clean, no warnings |
| `ctest` | **37/37 pass**, including 8 new analyser tests and 8 new FLAC round-trip and reader tests |
| GUI package built from the working tree | Builds; `checkPhase` runs the suite; `installCheckPhase` reports `DomesdayDuplicator 2.1 (worktree)` and finds the `.desktop`, AppStream and icon files |
| `--analyse-test-data` on a clean ramp `.ldf` | `PASSED … 4,000,000 samples checked, no breaks, test sequence length 1021`, exit 0 |
| `--analyse-test-data` on a corrupted capture | `FAILED … breaks at sample 80,000, where 362 was expected but 0 was read`, exit 1 |
| `flac -t --ogg` on a written `.ldf` | `ok` |
| Decoded stream shape | `16 bit, mono 40000 Hz` |
| `ld-compress --uncompress` round-trip | **Byte-identical** to the `.lds` the removed packed path would have written from the same samples |
| `mkdocs build --strict` | Builds; no page links to a deleted file |
| `grep -ri 'dddconv\|dddutil'` outside this document | Only the note in `gui/README.md` recording that they were removed |

The `ld-compress` result is the one that matters most: it is the plan's own test for whether
this is really an `.ldf`, and it passed against ld-decode's own tool rather than against a
reading of the format.

**What is not met, and cannot be from here:**

1. **No workflow has ever run.** `build.yml`, the three `package-*.yml` and both release
   workflows are authored and validate as YAML, and nothing more. The Flatpak, the DMG and
   the MSI have **never been built** — let alone installed, launched or uninstalled, which is
   what P7-14…P7-16 actually require. Expect the first push to find things; that is what the
   first push is for, and after the 2026-08-13 revision the first push to this branch is
   exactly where it happens, rather than at the first tag.

   Note that the *Branching* section above still says no CI runs on this branch. That was
   true of the inherited per-submodule workflows, which sat in directories GitHub never
   reads; it stops being true the moment `.github/workflows/` at the repository root is
   committed.
2. **The throughput measurement is outstanding**, and it is P7-21's real gate: a sustained
   capture with zero buffer overruns, on hardware. Everything above tests correctness of the
   format. Nothing above tests whether a compressor on the real-time path can keep up at
   40 Msps, which is the risk the change introduces.
3. **The T5 capture-integrity run is outstanding** and now covers more than it did: the same
   bench session should confirm a real capture is written as `.ldf`, analysed by the ported
   check, and decoded by ld-decode end to end.
4. **The application ID is provisional.** `io.github.simoninns.DomesdayDuplicator` is used
   throughout; `com.waitingforfriday.DomesdayDuplicator` is the alternative. Changing it after
   the first release is a migration, so it wants deciding before P8-3.

## Phase 8 — Cleanup and release

| Task | Size | Detail |
| --- | --- | --- |
| ~~**P8-1** Archive the four upstream repos~~ | — | **Removed** per P0-6 — the old repositories are left alone and cleaned up separately, outside this plan |
| **P8-2** README rewrite | M | Nix quick-start per component alongside the existing native instructions |
| **P8-3** Tag the first releases of both streams | M | First monorepo releases, and the first end-to-end exercise of both P7-8 workflows. **`gui-v*`** publishes the Flatpak, DMG and MSI with `SHA256SUMS` and `PROVENANCE.txt`; install each one on a clean machine before calling it done, since a release-only packaging bug is invisible in CI. **`fw-v*`** publishes `firmware.img`, `fx3-programmer` and the manifests automatically — then **build the bitstream locally and attach `.sof`, `.jic` and `bitstream-provenance.txt` by hand** (P7-11); that release is not complete without them. Verify every asset in both reports the tagged commit and none reports `unknown` |
| **P8-4** Update this plan | S | Mark it executed; fold anything still outstanding into issues |
| **P8-5** SPDX header convention | M | Only 8 of 67 source files carry SPDX identifiers; the rest use long-form GPL notices. Adopt SPDX (machine-checkable, and matches decode-orc), add the T4 presence check, and convert files as they are touched rather than in one sweeping commit |
| ~~**P8-6** `--analyse-test-data` CLI mode~~ | — | **Moved to Phase 7 as part of P7-19.** It stops being an optional follow-up once `dddutil` is removed: the capture application has to carry the analysis anyway, and a headless mode is what makes the T5 gate scriptable rather than clicked |

## Summary of what needs hardware

Four gates cannot be signed off from a green build alone: **P0-3** (Quartus spike), **P3-3**
(udev rules), **P5-4** (firmware flash and enumerate), **P6-5** (bitstream capture).

Three are now partly or wholly closed on real hardware. What is left is one bench session:

| Outstanding | What it needs |
| --- | --- |
| **P5-4**, second half | The capture-integrity procedure ([TESTING.md](../TESTING.md) §5) with the flake-built firmware. The device enumerates and reports its commit; the capture path is unproven |
| **P6-5** | The same procedure with a flake-built bitstream, then a known-disc capture compared against one from the shipped 18.0-built bitstream |
~~**P6-1**, permissions half~~ | **Closed 2026-08-12.** With `70-altera-usb-blaster.rules` installed, `getfacl` shows `user:sdi:rw-`, the node carries `:uaccess:`, and `jtagconfig` locks the chain and reads back an `EP4CE22` IDCODE. Before/after table in the Phase 6 gate section |

These are one session, not two — P5-4 and P6-5 are the same procedure on the same bench with
the same disc. P0-3's software half was closed in Phase 6; its hardware half *is* P6-5.

**Sequencing note against Phase 7.** Step 4 of that procedure runs `dddutil`'s test-data
analysis, and P7-12 deletes `dddutil`. Either run the bench session before P7-12 lands, or run
it after P7-19 has moved the analysis into the capture application — but not in between, and
do not treat "the tool it names no longer exists" as grounds for skipping the step. Whichever
order it happens in, [TESTING.md](../TESTING.md) §5 must name a binary that actually ships.

The same applies to the file it produces: after P7-21 the procedure captures an `.ldf`, not an
`.lds`. Running the bench session *before* the format change is the simpler order, because it
keeps the outstanding P5-4/P6-5 gates about firmware and gateware rather than entangling them
with a new encoder on the capture path. If it happens after, add P7-23's ld-decode cross-check
to the same session — the disc is already on the bench and it is the only test that proves the
new format is usable end to end.

## Deliberately out of scope

- KiCad 5 → 10 file-format migration (blocks `kicad-cli` export; separate change, separate review)
- Any firmware, gateware or application behaviour change — with two exceptions, both in
  Phase 7 and both requested by the maintainer. **P7-12/P7-19** remove `dddconv` and `dddutil`
  and move their one gate-critical capability, the test-data analysis, into the capture
  application; `dddutil`'s file conversion is dropped rather than moved. **P7-21…P7-25** change
  the capture output format from packed 10-bit `.lds` to Ogg FLAC `.ldf`. The second is a
  deliberate breach of this line: it alters what the application writes, and it is scoped
  accordingly — the sample values are unchanged (the app already computes them), the FPGA and
  FX3 are untouched, and nothing else about capture behaviour moves with it
- **Code signing and notarisation** of the macOS DMG and the Windows MSI. Both need paid,
  personally held credentials that cannot live in a public repository's CI without a decision
  about who holds them. The installers ship unsigned and the docs explain the resulting
  Gatekeeper and SmartScreen prompts (P7-15, P7-16, P7-20)
- **Flathub submission**, Homebrew casks, winget/Chocolatey manifests, `.deb`/`.rpm`. Phase 7
  produces the three installer files and attaches them to a release; getting them into
  third-party distribution channels is separate work with separate review cycles
- A Windows driver package for the WinUSB back-end — needs a signed `.inf`; Zadig stays the
  documented route (P7-16)
- Git LFS or history pruning, unless P0-5 chose it — in which case it happens *in* P1 or not at all
