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

**Conventions**

- One phase = one PR (except phase 1, which is its own merge commit — see
  [submodule-migration.md](submodule-migration.md)).
- Each phase has a **gate**. Do not open the next PR until the gate passes on `master`.
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

Ten issues found during the survey, plus the two additional ones found while writing this
plan. Each is assigned to a phase.

| # | Issue | Where | Phase |
| --- | --- | --- | --- |
| D1 | CI `working-directory: fx3-firmware`, actual path `fx3/fx3-firmware` — job cannot pass | `firmware/.github/workflows/build-firmware.yml` | P2-3 |
| D2 | Two near-identical CMake front-ends; only one is used | `gui-app/CMakeLists.txt`, `gui-app/tools/CMakeLists.txt` | P2-4 |
| D3 | `install(FILES ... DESTINATION /etc/udev/rules.d)` escapes the install prefix | `fx3-programmer/CMakeLists.txt` | P2-5 |
| D4 | `git rev-parse` at configure time → version silently becomes `unknown` in any sandbox | `fx3-firmware/CMakeLists.txt` | P2-6 |
| D5 | `elf2img` built via `ExternalProject_Add` (nested configure, rebuilt per firmware build) | `fx3-firmware/CMakeLists.txt` | P2-7 |
| D6 | ~45 MB of unreferenced SDK library profiles committed | `cyfx3sdk/fw_lib/1_3_5/fx3_{debug,profile_debug,profile_release}` | P2-8 |
| D7 | `version.c` is dead code: not in `C_SOURCES`, and `#include "version.h"` — that header does not exist anywhere in the tree | `fx3-firmware/firmware/version.c` | P2-6 |
| D8 | `TOSTRING(FIRMWARE_GIT_COMMIT)` double-stringifies an already-quoted macro, so `firmware_version_string` reads `Domesday Duplicator ("abc12345")` with literal quote characters. **Confirmed by build:** the symbol is 0x21 (33) bytes — the length *with* the stray quotes. **Latent only** — it is unreferenced and `--gc-sections` discards it, so it never reaches the device | `fx3-firmware/firmware/usb-descriptor.c:233` | P2-6 |
| D9 | `remote_theme: just-the-docs/just-the-docs` fetches the theme over the network at build time — impossible in a Nix sandbox | `docs/wiki-default/_config.yml:3` | P4-4 (dissolved by the MkDocs move) |
| D10 | `baseurl: "/DomesdayDuplicator-docs"` hard-codes the *old repo's* Pages URL; after the merge the site moves and every external link to it breaks | `docs/wiki-default/_config.yml:4`, `README.md:3,37` | P0-4, P4-8 |
| D11 | Three of four submodule URLs are SSH, so the README's `git clone --recursive` fails without a GitHub key | `.gitmodules` | P1 (dissolved) |
| D12 | `build-local.sh` injects front matter that `_config.yml` `defaults:` already supplies, and its error message names `jekyll-theme-cayman` — a theme the site no longer uses | `docs/build-local.sh` | P4-4 (dissolved by the MkDocs move) |
| D13 | Permanent (EEPROM/SPI) programming needs a Cypress secondary loader, `cyfxflashprog.img`. **File now vendored** (2026-08-12) at the programmer's directory root, where the existing `../cyfxflashprog.img` candidate finds it from `build/`. **Code half remains:** every candidate path is working-directory-relative, so installed binaries still cannot locate it | `fx3-programmer/src/fx3-programmer.c:136` | P2-10, P3-3 |
| D14 | Four qmake `.pro` files duplicate the CMake build definition and exist only for Qt Creator; `BUILD.md` steers contributors to them | `gui-app/tools/**/*.pro` | P2-11 |
| D15 | No `CMAKE_EXPORT_COMPILE_COMMANDS` anywhere, so no `compile_commands.json` and no working clangd in any editor | all `CMakeLists.txt` | P2-12 |
| D16 | Sole `.editorconfig` is buried in `gui-app/tools/DomesdayDuplicator/`; `.vscode`/`.idea` ignore rules exist only in `gui-app/.gitignore` | repo root | P2-13 |
| D17 | The two licence names are **transposed**: `LICENSE` is GPLv3 and the hardware file is CC BY-SA 4.0, but the README labels software as CC BY-SA (linking to the GPLv3 file) and hardware as GPLv3 (linking to the CC BY-SA URL) | `README.md` licence block | P2-14 |
| D18 | No test infrastructure of any kind — no `enable_testing()`, `add_test()`, GoogleTest, Catch2 or QTest anywhere in the tree | repo-wide | P3-6 |

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

## Phase 1 — Merge the submodules — **DONE (local, unpushed)**

Executed 2026-08-12 on local branch `monorepo-merge`, branched from `20260812-001` at
`bcd53a0`. **Nothing has been pushed.**

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

Rollback: `git checkout 20260812-001 && git branch -D monorepo-merge`. Pre-merge HEAD was
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

## Phase 2 — Re-layout and defect fixes

One PR. The renames must be a single commit so `git log --follow` stays usable; the fixes can
be separate commits within the same PR.

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

**Gate:** all four components build from source using the *existing* non-Nix instructions,
on a machine with no Nix. `cmake --install` writes nothing outside its prefix. CI (still the
old workflows, path-fixed) is green.

## Phase 3 — Nix foundation and the easy flakes

### P3-1 `nix/lib.nix` and the root flake (M)

`nix/lib.nix` exports `forAllSystems` / `forLinux` helpers plus the shared `pkgs` config.
Root `flake.nix` `callPackage`s each component's `package.nix` directly — no cross-flake
inputs. Structure and sketches: [nix-flake-design.md](nix-flake-design.md) §1.

### P3-2 `gui/` flake (M)

`gui/package.nix` + `gui/shell.nix` + `gui/flake.nix`. `qt6Packages.callPackage` with
`wrapQtAppsHook` (without it the binary dies on the missing `xcb` platform plugin).
Use `lib.fileset` for `src` so README edits do not trigger rebuilds.
*Watch:* the hand-rolled `cmake/FindLibUSB.cmake` is the first suspect if configure fails —
`pkg-config` finds libusb-1.0 cleanly under Nix.

**Gate:** `nix build .#gui` then `./result/bin/DomesdayDuplicator` opens a window on a clean
machine; `nix develop .#gui` gives a working CMake build tree.

### P3-3 `fx3/programmer/` flake + NixOS udev module (M)

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

### P3-4 `hardware/` flake (S)

Dev shell with `kicad` only. Packaged `kicad-cli` export stays blocked on the KiCad 5 → 10
file-format migration (`kicad-cli` cannot read legacy `.sch`); file that as a follow-up
issue, not part of this plan.

**Gate:** `nix develop ./hardware` opens `hardware/pcb/` in KiCad.

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

## Phase 4 — docs: convert to MkDocs Material, then flake it

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

**Gate:** `nix build .#docs-site` succeeds under `--strict`; all 24 pages render with working
images; navigation matches the old sidebar's grouping and order; the deployed artefact is the
`nix build` output.

## Phase 5 — FX3 firmware flake

Depends on P0-2 (licence) and P2-6/P2-7/P2-8.

| Task | Size | Detail |
| --- | --- | --- |
| **P5-1** `elf2img` derivation | S | `fx3/firmware/elf2img.nix` over `fx3/sdk/util/elf2img` |
| **P5-2** Firmware derivation | M | `gcc-arm-embedded`, `-DCMAKE_TOOLCHAIN_FILE=…`, `-DCYFX3SDK_PATH=${../sdk}`, `-DFIRMWARE_VERSION=${self.shortRev or "dirty"}`, `python3` in `nativeBuildInputs` for `generate-descriptor.sh`, `dontStrip`/`dontPatchELF`. If the link fails on `-nostartfiles`, add `hardeningDisable = [ "all" ]` — a freestanding ARM target and nixpkgs' default hardening flags do not mix. |
| **P5-3** SDK provenance | S | `fx3/sdk/README.md` (version, origin URL, refresh date) + `LICENSE.txt` copied from the SDK's `license/license.txt`. Mechanism settled by P0-2 — this is record-keeping only |
| **P5-4** Hardware verification | M, **HW** | Flash the `nix build` output with `fx3-programmer`; device enumerates; `lsusb -v` product string shows the real commit hash (proves **D4**; D8 is unreachable dead code and not observable here); then run the **capture-integrity procedure** from TESTING.md — zero sequence breaks required |
| **P5-5** Descriptor golden test | S | Host-side T2 test over `generate-descriptor.sh`: fixed commit string in, byte-for-byte comparison against a committed reference header. Protects the descriptor byte layout — the path the host actually reads, including the computed length byte. Note this does **not** cover D8, which lives on a separate, dead code path |

**Gate:** P5-4 passes. A firmware image that compiles but has not been flashed and
capture-verified is not done.

## Phase 6 — FPGA flake

Depends on P0-3.

| Task | Size | Detail |
| --- | --- | --- |
| **P6-1** Quartus flake | M | `import nixpkgs { config.allowUnfree = true; }` internally; `.override { supportedDevices = [ "Cyclone IV" ]; withQuesta = false; }`. Add the **USB-Blaster udev rule** to `nix/modules/udev.nix` alongside the FX3 one — the nixpkgs Quartus package ships no udev rules. Sketch in [nix-flake-design.md](nix-flake-design.md) §6 |
| **P6-2** Headless compile, convert and program flow | M | `quartus_sh --flow compile DomesdayDuplicator`, then `quartus_cpf -c DomesdayDuplicator.cof`, then `quartus_pgm` driven by the already-committed `DomesdayDuplicator_write_{sof,jic}.cdf`. `export HOME=$TMPDIR` (Quartus needs a writable home). No GUI at any step — [ide-independence.md](ide-independence.md) §2.1 |
| **P6-3** IP regeneration | L | **Contingency only.** `IPfifo.v` and `IPpllGenerator.v` are committed plain Verilog instantiating `dcfifo`/`altpll` with explicit `defparam`s, so nothing runs MegaWizard at build time. Needed only if 25.1 rejects the 2017-era parameters — see [ide-independence.md](ide-independence.md) §2.2 |
| **P6-4** `fpga/README.md` | S | Canonical Quartus version, manual-install fallback, that Quartus is not bit-reproducible, that the dev shell is the deliverable, and that the generated `.v` files are **source of truth** rather than wizard output to be regenerated |
| **P6-5** Hardware verification | L, **HW** | Run the TESTING.md capture-integrity procedure with the flake-built bitstream (zero sequence breaks), then capture a known disc and compare against a capture from the shipped 18.0-built one. Keep the released `.jic` in-tree until this passes |
| **P6-6** `verilator --lint-only` check | S | Lint the hand-written modules (`DomesdayDuplicator.v`, `buffer.v`, `dataGenerator.v`, `fx3StateMachine.v`, `statusLED.v`) as a `nix flake check`. Free, fast, cross-platform — so gateware gets *some* CI coverage even though bitstream builds cannot run there |
| **P6-7** Gateware testbenches | M | T3 simulation for `dataGenerator.v` (assert the 0…1020 test ramp — the same sequence P6-5 verifies on silicon), `fx3StateMachine.v` (the handshake, highest-risk module) and `statusLED.v`. Note in TESTING.md that whole-design simulation needs vendor `dcfifo`/`altpll` models |

**Gate:** P6-5 passes, and every step from source to programmed device runs from a shell.
Note the packaged bitstream is *not* reproducible across runs — that is a property of
Quartus, and the gate is functional equivalence, not hash equality.

## Phase 7 — CI consolidation

| Task | Size | Detail |
| --- | --- | --- |
| **P7-1** Single path-filtered workflow | M | `nix-installer-action` + `magic-nix-cache-action`, then `nix build .#gui .#fx3-firmware .#fx3-programmer .#docs-site`. Replaces the three per-submodule workflows (including the one fixed in P2-3) |
| **P7-2** Keep the native build matrix | M | The current GUI workflow builds Linux x64/ARM64, macOS and Windows artefacts. Nix does not cover Windows — **keep those jobs**, driven from the new paths. Nix is additive here, not a replacement |
| **P7-3** Pages deploy | S | Already replaced in P4-9 with decode-orc's Nix-based workflow (`nix build .#docs-site` → `upload-pages-artifact` with `path: ./result` → `deploy-pages`). Nothing to do here beyond confirming the path filter matches `docs/**` |
| **P7-4** `nix flake check` in CI | S | Excluding `bitstream`: unfree, multi-gigabyte, `x86_64-linux`-only, and too slow for hosted runners. Build it locally and attach to releases |
| **P7-5** Delete per-component `.github/` dirs | S | Inert after P1 (GitHub only reads the repo root), but confusing |
| **P7-6** Test lanes | S | Run tiers T1–T4 in the consolidated workflow. **T5 never runs in CI** — it needs a physical DdD, and a test that silently "passes" because no hardware was attached is worse than no test |

**Gate:** one workflow file; no job references a non-existent path; a PR touching only
`docs/` does not trigger firmware jobs.

## Phase 8 — Cleanup and release

| Task | Size | Detail |
| --- | --- | --- |
| ~~**P8-1** Archive the four upstream repos~~ | — | **Removed** per P0-6 — the old repositories are left alone and cleaned up separately, outside this plan |
| **P8-2** README rewrite | M | Nix quick-start per component alongside the existing native instructions |
| **P8-3** Tag a release | S | First monorepo release; attach the GUI binaries, the FX3 `.img` and the FPGA `.jic` |
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
