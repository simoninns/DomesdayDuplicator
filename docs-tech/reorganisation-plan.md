# Domesday Duplicator — Repository Reorganisation Plan

**Status:** proposal, nothing executed.
**Baseline:** commit `bcd53a0`, branch `20260812-001`, surveyed 2026-08-12.

## 1. Goals

1. **One repository.** Fold the four submodules back into the main repo, keeping history.
2. **One command per component to get a working build environment**, on any Linux machine
   with Nix, including for the proprietary tools (Intel/Altera Quartus, the Cypress/Infineon
   FX3 SDK).
3. **Per-component outputs from a single flake.** Someone who only wants to build the GUI
   should not have to evaluate — let alone download — a 20 GB unfree FPGA toolchain. That is
   achieved by giving each component its own `package.nix` / `shell.nix` and its own flake
   *attribute*, not its own `flake.nix`: one `flake.lock` means the pinned nixpkgs is the
   same however you enter the tree. (Goal 3 originally read "per-component flakes, not one
   mega-flake"; those flakes were built in Phase 3 and removed after Phase 4 — see §4.)
4. **No dependency on any IDE.** Quartus ships a proprietary GUI and the FX3 flow is
   traditionally Eclipse-based; neither may be required to build, test, program or
   contribute. Every step runs from a shell, and language support comes from LSP so any
   editor works. Details: [ide-independence.md](ide-independence.md).
5. **Stop the layout from drifting.** Today the CI workflows, the READMEs and the actual
   directory layout disagree with each other in several places (see §3).
6. **Every shipped artefact comes from CI, and a release contains exactly the artefacts
   built from the release commit.** The GUI, the FX3 firmware and the programmer are built on
   every commit and published on tag; each names the commit it came from. This is stricter
   than "CI builds things" — it means artefacts are *retained and published*, because for the
   FPGA in particular, regenerating means pinning an unfree GB-scale toolchain, so the
   archived artefact plus published digests is the practical route. Model and tasks:
   [implementation-plan.md](implementation-plan.md) → *Release artefacts and provenance*,
   Phase 7.

Non-goals for this plan: rewriting any firmware or application code, migrating the KiCad
project to a newer file format (flagged as a follow-up), or changing what the project does.

Deferred, deliberately: **building the FPGA bitstream in CI.** Quartus is unfree,
`redistributable = false` and GB-scale, so it can never come from a binary cache. Decided
2026-08-12 to leave it out for now and keep attaching a locally built bitstream to releases;
the options and their trade-offs are recorded so the decision can be revisited.

## 2. Current state

### 2.1 Repository structure

The top-level repo (`simoninns/domesdayduplicator`, 458 commits) contains only
`README.md`, `CONTRIBUTING.md`, `LICENSE`, `graphics/` (11 MB) and four submodules:

| Submodule | Upstream | Commits | Worktree | History (`.git/modules`) | Pinned at |
| --- | --- | --- | --- | --- | --- |
| `docs` | `DomesdayDuplicator-docs` (https) | 19 | 122 MB | 140 MB | `9e57a72` (detached, `main`) |
| `firmware` | `DomesdayDuplicator-firmware` (ssh) | 115 | 72 MB | 26 MB | `83a98bb` (`master`) |
| `gui-app` | `DomesdayDuplicator-gui-app` (ssh) | 299 | 3.1 MB | 2.8 MB | `8036eaf` (detached, `master`) |
| `hardware` | `DomesdayDuplicator-hardware` (ssh) | 40 | 3.2 MB | 5.6 MB | `7609931` (`master`) |

Total on-disk git data is already 211 MB, 174 MB of which is submodule history — so merging
the submodules in does **not** meaningfully increase what a developer already clones with
`--recursive`. It just stops it being four moving targets.

Three of the four submodule URLs are `git@github.com:` (SSH), so an anonymous
`git clone --recursive` — the exact command the README tells people to run — fails for
anyone without a GitHub SSH key. This alone justifies the merge.

### 2.2 The five real components

| Component | Path today | Toolchain | Build |
| --- | --- | --- | --- |
| FPGA gateware | `firmware/DE0-NANO/DomesdayDuplicator/` | Quartus Prime **Lite**, Cyclone IV E `EP4CE22F17C6` on a Terasic DE0-Nano | GUI-only; no script in-tree |
| FX3 firmware | `firmware/fx3/fx3-firmware/` | `arm-none-eabi-gcc` + vendored CyFX3 SDK 1.3.5 (71 MB) | CMake + toolchain file |
| FX3 programmer | `firmware/fx3/fx3-programmer/` | C99 + libusb-1.0 | CMake |
| GUI application | `gui-app/tools/{DomesdayDuplicator,dddconv,dddutil}` | Qt 6.2+, libusb-1.0, C++20 | CMake |
| PCB | `hardware/KiCAD/Domesday Duplicator/` | KiCad (files are KiCad 5-era: `.sch`, `.pro`, `-cache.lib`) | manual export |
| Website | `docs/wiki-default/` | Jekyll + `just-the-docs` (via `remote_theme`) — **converting to MkDocs Material**, see [docs-theme-migration.md](docs-theme-migration.md) | GH Pages action + `build-local.sh` |

### 2.3 Concrete problems found during the survey

These are worth fixing *as part of* the reorganisation, because most of them are symptoms
of the split layout:

1. **`firmware` CI is broken/stale.** `.github/workflows/build-firmware.yml` uses
   `working-directory: fx3-firmware`, but the directory is `fx3/fx3-firmware`. The
   firmware job cannot be passing as written.
2. **Duplicated CMake front-ends in `gui-app`.** `CMakeLists.txt` and
   `tools/CMakeLists.txt` are near-identical copies (different `cmake_minimum_required`,
   different Qt version constraint, different `qt_standard_project_setup` handling). Only
   one is used; the other rots.
3. **`fx3-programmer` installs to an absolute path.** `install(FILES configs/88-cyusb.rules
   DESTINATION /etc/udev/rules.d)` escapes any install prefix. It breaks Nix, and it breaks
   `DESTDIR` packaging generally.
4. **`fx3-firmware` shells out to `git rev-parse` at configure time** to stamp the USB
   descriptor. There is no `.git` inside a Nix build sandbox, so this silently degrades to
   `"unknown"` — the version has to be injected instead.
5. **`fx3-firmware` builds `elf2img` via `ExternalProject_Add`** — a nested CMake configure
   inside the main build. Fine on a workstation, awkward under Nix, and it means a host
   tool is rebuilt per firmware build.
6. **71 MB of prebuilt `.a` files are committed** (`cyfx3sdk/fw_lib/1_3_5/`, four profile
   variants of `libcyfxapi.a`/`libcyu3threadx.a` at ~6–9 MB each). Three of the four
   variants (`fx3_debug`, `fx3_profile_debug`, `fx3_profile_release`) are unused by the
   build, which only links `fx3_release`.
7. **Version drift on the FPGA project.** `.qpf` says Quartus 16.0.2; `.qsf` says last
   saved with 18.0 Lite. nixpkgs currently ships 25.1. Nothing records which version is
   *supposed* to produce the shipped bitstream.
8. **`CONTRIBUTING.md` sends documentation changes to a separate repo** that will no longer
   exist as a separate thing after this migration.
9. **Four qmake `.pro` files** in `gui-app/tools/` duplicate the CMake build definition and
   exist only for Qt Creator, with `BUILD.md` steering contributors to them. Also: no
   `compile_commands.json` is emitted anywhere, so no editor gets working clangd.
10. **`fx3-firmware/firmware/version.c` is dead code.** It is absent from `C_SOURCES` and
    `#include`s a `version.h` that exists nowhere in the tree, so it has never compiled.
11. **The version string is double-stringified.** `usb-descriptor.c:233` applies
    `TOSTRING()` to `FIRMWARE_GIT_COMMIT`, which CMake already defines as a string literal,
    so `firmware_version_string` reads `Domesday Duplicator ("abc12345")` — with literal
    quote characters. The USB descriptor the host sees comes from a different path
    (`USB_DESC_PRODUCT_BYTES`) and is correct.
12. **The docs site fetches its theme over the network at build time**
    (`remote_theme: just-the-docs/just-the-docs`), which cannot work in a Nix sandbox, and
    **`baseurl` hard-codes the old repo's Pages path** (`/DomesdayDuplicator-docs`), which
    breaks as soon as the site moves into the monorepo. Both are dissolved by the MkDocs
    conversion in [docs-theme-migration.md](docs-theme-migration.md).
13. **`cyfxflashprog.img` is missing.** The FX3 programmer needs this Cypress secondary
    loader for EEPROM and SPI flash programming, but it is not in the repository, is located
    only via CWD-relative paths or an undocumented `$FX3_FLASH_PROG`, and is named in no
    README.
14. **The licence names are transposed.** `LICENSE` is GPLv3 and the hardware licence is
    CC BY-SA 4.0, but the README labels software as CC BY-SA (linking to the GPLv3 file) and
    hardware as GPLv3 (linking to the CC BY-SA URL).
15. **There are no tests.** No `enable_testing()`, `add_test()`, GoogleTest, Catch2 or QTest
    anywhere — see [agents-and-testing.md](agents-and-testing.md).

Numbering note: these are referenced as D1–D18 in
[implementation-plan.md](implementation-plan.md), which assigns each one to a phase. The
IDE-coupling items (D14–D16) are broken out in
[ide-independence.md](ide-independence.md).

## 3. Target layout

```
domesdayduplicator/
├── flake.nix                     # the ONLY flake: packages, dev shells, `nix flake check`
├── flake.lock                    # the ONLY lock — no component carries either file
├── README.md
├── CONTRIBUTING.md
├── LICENSE
├── docs-tech/                    # this directory — engineering process docs
├── nix/
│   ├── lib.nix                   # eachSystem helper, shared pkgs config
│   ├── overlays/                 # e.g. pinned quartus, cyfx3sdk
│   └── modules/
│       └── udev.nix              # NixOS module for the FX3/DdD udev rules
├── graphics/                     # project logos/screenshots used by READMEs
├── hardware/
│   ├── shell.nix                 # kicad dev shell (root flake: `.#hardware`)
│   ├── pcb/                      # was: hardware/KiCAD/Domesday Duplicator/
│   └── doc/                      # was: hardware/Documentation/
├── fpga/
│   ├── shell.nix                 # free Verilog tools (root flake: `.#fpga`)
│   ├── package.nix               # bitstream; unfree quartus, x86_64-linux only
│   ├── src/                      # was: firmware/DE0-NANO/DomesdayDuplicator/
│   └── README.md
├── fx3/
│   ├── firmware/
│   │   ├── package.nix           # arm-none-eabi cross build → .elf/.img
│   │   ├── src/                  # was: firmware/fx3/fx3-firmware/firmware/
│   │   └── gpif/                 # was: GPIF_II/
│   ├── programmer/
│   │   ├── package.nix
│   │   └── src/
│   └── sdk/                      # vendored CyFX3 SDK, pruned (see §5.3)
├── gui/
│   ├── package.nix               # Qt6 app + dddconv + dddutil
│   ├── shell.nix                 # dev shell (root flake: `.#gui`)
│   ├── CMakeLists.txt            # single front-end (dedup of the current two)
│   ├── cmake/FindLibUSB.cmake
│   └── src/{DomesdayDuplicator,dddconv,dddutil}/
├── docs/
│   ├── package.nix               # site build (root flake: `.#docs-site`)
│   ├── shell.nix                 # mkdocs dev shell (root flake: `.#docs`)
│   ├── mkdocs.yml                # Material theme, matching decode-orc
│   └── content/                  # was: wiki-default/ — NOT "site/", which would
│                                 # collide with mkdocs' default site_dir
└── .github/workflows/
    ├── build.yml                 # path-filtered matrix over the components
    └── deploy-pages.yml          # docs/site → GitHub Pages (unchanged behaviour)
```

Renames chosen deliberately:

- **`firmware/` is dissolved.** It currently means "FPGA gateware *and* USB controller
  firmware *and* a host-side programmer tool", which is three unrelated toolchains. `fpga/`
  and `fx3/` are honest about that. The `fx3/programmer/` binary is a host tool, but it
  belongs with the thing it programs.
- **`gui-app/tools/` → `gui/src/`.** The `tools/` level exists only for historical
  qmake reasons and adds a directory for nothing.
- **`hardware/KiCAD/Domesday Duplicator/` → `hardware/pcb/`.** Removes the space in the
  path, which currently forces quoting in every script and CI step that touches it.
- **`docs/wiki-default/` → `docs/content/`.** "wiki-default" describes where the content came
  from in 2019, not what it is. `content/` rather than `site/` because MkDocs' `site_dir`
  defaults to `site` and the two cannot share a name.

Directory renames with spaces removed are the one part of this plan that will annoy anyone
with an in-flight branch, so they should all land in a single commit, announced.

## 4. Flake strategy

Full design and sketches: [nix-flake-design.md](nix-flake-design.md). The shape:

- **One flake, one lock.** Exactly one `flake.nix`, at the repository root, and exactly one
  `flake.lock`. No component carries either file.
- **Logic lives in plain `.nix` files, not in `flake.nix`.** Each component has a
  `package.nix` / `shell.nix` taking `{ pkgs, ... }`, and the root flake `callPackage`s or
  `import`s them. This is the key decision: the root flake does not duplicate the component
  definitions, and there is no need for cross-flake `inputs` — with their extra lock files
  and `follows` boilerplate — between parts of the same repo.
- **Nothing unfree is reachable from the root flake's default outputs.** The bitstream is
  exposed as `packages.x86_64-linux.bitstream` and a `x86_64-linux`-only Quartus dev shell,
  built from a second `import nixpkgs` of the same locked input with `allowUnfree` set, and
  `nix flake check` skips both. A contributor fixing a GUI typo must never be asked to
  download Quartus.

### Corrected: this section used to say "seven flakes"

Earlier revisions specified a thin `flake.nix` per component — root, `hardware/`, `fpga/`,
`fx3/firmware/`, `fx3/programmer/`, `gui/`, `docs/` — so that `cd gui && nix develop` worked.
It was implemented in Phase 3 and removed once its cost became visible.

The justification given here was: *"One `flake.lock` at the root is authoritative. Component
flakes each get their own lock, but CI resolves everything through the root flake so versions
cannot skew."* The first half is fine; the conclusion does not follow. CI is not the only
consumer. Every component flake declared unpinned `nixos-unstable`, so a **developer**
entering through a component got whatever that resolved to on the day, silently diverging
from the root pin — and Nix creates and `git add`s those component locks without being asked.
Reproducibility that only holds on one entry path is not reproducibility.

Nothing was lost in removing them: Nix walks up to the enclosing flake, so `nix develop .#gui`
works from any subdirectory. Only bare `nix develop` changed meaning — it always gives the
all-components shell now, not the component you are standing in. See
[nix-flake-design.md](nix-flake-design.md) §1.

The unfree/`x86_64-linux`-only nature of Quartus was the strongest argument for a separate
`fpga/` flake, and it does not survive either: the FPGA build does pull a multi-gigabyte,
`redistributable = false` download that no binary cache can serve and that cannot run in
GitHub's hosted CI. But containing that needs a separate **`pkgs`**, not a separate **flake**
— a second `import nixpkgs { config.allowUnfree = true; }` of the already-locked input, with
the outputs guarded by system and kept out of `checks`. Same containment, same lock file.

## 5. Known-hard cases

### 5.1 Quartus (FPGA)

nixpkgs has `quartus-prime-lite` 25.1 (`pkgs/by-name/qu/quartus-prime-lite/quartus.nix`).
Verified from the package source:

- `platforms = [ "x86_64-linux" ]`, `license = unfree`, `redistributable = false` — so it
  is fetched from Altera at build time and never from a binary cache.
- It takes a `supportedDevices` argument, default
  `[ "Arria II" "Cyclone V" "Cyclone IV" "Cyclone 10 LP" "MAX II/V" "MAX 10 FPGA" ]`.
  This project needs **only `"Cyclone IV"`** (EP4CE22F17C6), so override it — that removes
  five device component downloads.
- `withQuesta ? true` — set to `false`, there is no simulation in this project.

**The GUI is not required.** The nixpkgs wrapper puts every binary in `quartus/bin/*` on
`PATH` — `quartus_sh`, `quartus_map`, `quartus_fit`, `quartus_asm`, `quartus_sta`,
`quartus_cpf`, `quartus_pgm`, `jtagd` — with the `quartus` GUI merely one of them, and Lite
needs no licence file. Compile, convert and device programming all run from a shell, using
the `.cof` and `.cdf` files already committed. See [ide-independence.md](ide-independence.md)
§2.

Two open decisions, both for the maintainer:

1. **Which Quartus version is canonical?** The project was last saved with 18.0 Lite;
   nixpkgs gives 25.1. Options: (a) accept 25.1 and record the version in-tree; (b) pin a
   second nixpkgs input at a revision carrying an older Quartus. **(a) is recommended** —
   pinning old unfree installers is fragile because Altera removes old downloads. Either way
   the compile must be verified on real hardware before the old bitstream is retired.

   *This is a smaller risk than it first appeared.* The two "megawizard IP cores" are
   committed plain Verilog — `IPfifo.v` instantiates `dcfifo`, `IPpllGenerator.v`
   instantiates `altpll`, both with explicit `defparam` values, pulled in by `.qip` files
   that do nothing but add the `.v` to the project. Nothing invokes MegaWizard at build time,
   so the question is only whether 25.1 accepts the 2017-era parameters, not whether the IP
   can be regenerated headlessly.
2. **Headless build.** There is currently no build script. The flake should add a
   `quartus_sh --flow compile DomesdayDuplicator` wrapper plus `quartus_cpf` for the `.jic`.
   Quartus fitting *is* deterministic for a fixed seed and toolchain; what is not guaranteed
   is byte-identity of the output file, since a compile timestamp is embedded in the bitstream
   header. Pin the seed explicitly and measure the difference (P6-9) rather than assuming
   either way. The dev shell remains the primary deliverable.

### 5.2 KiCad (PCB)

`kicad` 10.0.4 is in nixpkgs, free, all platforms — the easy one. The catch is that the
project files are KiCad 5 format (`.sch` + `Domesday Duplicator.pro` + `-cache.lib`).
KiCad 10 will offer to migrate them, which is a one-way conversion of the *design* and
should be a separate, deliberate commit — not a side effect of the reorganisation.

Plan: the flake ships a dev shell with `kicad` immediately. The packaged
`kicad-cli`-driven gerber/PDF/BOM export is gated behind the format migration, since
`kicad-cli` cannot read the legacy format. Track that as a follow-up issue.

### 5.3 Cypress/Infineon FX3 SDK

**Not in nixpkgs** — verified against `cyfx3`, `fx3`, `cypress`, `cyusb`, `ez-usb` and
`infineon`. The only adjacent packages are `fxload` (a host uploader that handles the RAM
path only) and `libfx2`/`python3Packages.fx2` (the FX**2** chip family, unrelated). Nothing
packages the ARM libraries, headers, linker script or `elf2img` that the build needs, and
that will not change: Infineon's download is login-walled, so there is no URL nixpkgs could
fetch unattended.

So the SDK stays vendored (71 MB), which is also the pragmatic choice. But:

- **Prune the three unused library profiles** (`fx3_debug`, `fx3_profile_debug`,
  `fx3_profile_release`) — roughly 45 MB of the 71 MB, none of it referenced by
  `CMakeLists.txt`. Note this only shrinks the *checkout*; the blobs stay in history unless
  a filter-repo pass removes them (see [submodule-migration.md](submodule-migration.md) §5).
- **Redistribution: decided (P0-2).** The SDK stays vendored regardless of its licence
  notices, on the maintainer's call that it is already widely mirrored. It is refreshed from
  the official vendor download, with the vendor's `license.txt` shipped alongside so the
  headers' reference to it resolves. The `requireFile` alternative is documented but not
  taken.
- **Split the ELF-to-image tool into its own derivation** rather than `ExternalProject_Add`
  (Phase 5 went further and replaced the vendor tool with `fx3/mkimage`), and pass
  it in via `nativeBuildInputs`.
- **Deal with the second Cypress artefact.** `fx3-programmer` needs a secondary loader,
  `cyfxflashprog.img`, for EEPROM and SPI flash programming — and it is not in this
  repository, is located only via CWD-relative paths or an undocumented `$FX3_FLASH_PROG`,
  and is named in no README. It is publicly downloadable from `cyusb_linux` on GitHub, so
  unlike the SDK it *can* be fetched, subject to the same licence check. Tracked as D13.

### 5.4 The documentation site

Originally this section proposed packaging the existing Jekyll site with `bundlerEnv`. That
is superseded: the site **converts to MkDocs + Material + `mkdocs-awesome-nav`**, matching
the toolchain decode-orc already uses. Rationale, task list and risks:
[docs-theme-migration.md](docs-theme-migration.md).

Briefly — converting is less work than packaging what is there now. All 24 markdown files
have no front matter and no Liquid, so the Jekyll-specific machinery is deleted rather than
ported. All three MkDocs packages are in nixpkgs, which removes the build-time theme fetch
(no `remote_theme`) and the need to hand-maintain a Ruby gem set that matches GitHub's
`github-pages` bundle. Following decode-orc, CI builds the site *with Nix* and uploads the
result, so the deployed site and the locally built one are the same derivation output.

The one cost is that page URLs change shape (`…/Page.html` → `…/page/`). Since the site is
already moving off `/DomesdayDuplicator-docs` when docs folds into the monorepo, both changes
should land together so inbound links break once.

## 6. Phasing

Each phase is independently revertible and has a gate that must pass before the next one
starts. All of it runs on the long-lived branch `20260812-002`; `master` is untouched until
every phase is complete, so there is no per-phase merge and no CI signal until Phase 7 (see
[implementation-plan.md](implementation-plan.md) → *Branching*).

Phase numbers here match [implementation-plan.md](implementation-plan.md), which is the
authoritative task list.

| Phase | Work | Done when | State |
| --- | --- | --- | --- |
| **0. Decisions** | Decisions ([decisions.md](decisions.md)); tag each submodule repo `pre-monorepo`; confirm the superproject pins the tip of each default branch | All seven decisions recorded | **Done** |
| **1. Merge histories** | Fold all four submodules in at their *current* paths, history preserved; delete `.gitmodules`. The four upstream repos are **left alone** (P0-6) — the monorepo README carries the "work here" notice instead | `git clone` (no `--recursive`, no SSH key) yields a complete tree; `git log --follow` works on a file from each former submodule | **Done** |
| **2. Re-layout** | Apply the §3 renames in one commit; fix the stale CI paths; dedup the GUI CMake front-ends; fix the absolute `/etc/udev/rules.d` install; author `AGENTS.md`; update every README | Existing non-Nix build instructions still work verbatim | **Done** |
| **3. Nix foundation** | `nix/lib.nix` and the root aggregator; `gui/`, `fx3/programmer/`, `hardware/` and `fpga/` (free tools) shells; test scaffolding and the first unit tests; `TESTING.md` | `nix build .#gui .#fx3-programmer` succeed; `nix flake check` runs the suite | **Done** |
| **4. Documentation** | Jekyll → MkDocs Material; content reorganised to match the navigation; `docs/` flake; the site URL move | `nix build .#docs-site` succeeds under `--strict`; every page renders with working images | **Done** |
| **5. FX3 firmware flake** | Cross-compile derivation; the ELF-to-image tool as its own derivation — and then **replaced outright** by project-authored GPLv3 code (`fx3/mkimage`), deleting the SDK's proprietary `elf2img`; stamp the GUI with its commit (D21) | `nix build .#fx3-firmware` produces an `.img` that flashes and enumerates on real hardware | **Built; hardware gate outstanding** |
| **6. FPGA flake** | Quartus decision from §5.1; headless compile flow; gateware lint and testbenches; bitstream provenance record | Bitstream built from the flake captures correctly on real hardware | — |
| **7. CI and releases** | One path-filtered `build.yml` producing the GUI, FX3 firmware and programmer **on every commit**; a tag-triggered `release.yml` publishing them with checksums and provenance; keep the native Windows/macOS matrix, which Nix cannot replace | A `v*` tag yields a release whose every asset reports the tagged commit, and none reports `unknown` | — |
| **8. Cleanup and release** | Per-component READMEs in place of `BUILD.md` duplication; SPDX header convention; tag the first monorepo release | — | — |

Project-convention documents (`AGENTS.md`, `TESTING.md`) and the first test suites landed in
phases 2 and 3 — see [agents-and-testing.md](agents-and-testing.md). `AGENTS.md` leads with
two absolute rules: no git operations that change repository state without an explicit
request, and no AI attribution in commits, PRs, code or documentation.

Phases 5 and 6 both require hardware-in-the-loop verification and must not be signed off on
the strength of a successful compile alone.

## 7. Risks

| Risk | Severity | Mitigation |
| --- | --- | --- |
| ~~Cypress SDK redistribution terms disallow vendoring~~ | Closed | P0-2: vendored regardless, by maintainer decision. The firmware build stays in CI and no history rewrite is needed |
| Quartus 25.1 changes the gateware's timing/behaviour | Medium (was High) | Hardware capture test against a known-good disc before retiring the 18.0-built bitstream; keep the released `.jic` in the repo until then. Downgraded because the IP is committed Verilog, not regenerated at build time (§5.1) |
| Repo becomes unwieldy (~400 MB clone) | Medium | Prune the three unused SDK profiles; optionally `--strip-blobs-bigger-than` on the docs image history; consider LFS. All are one-way — decide before phase 1, not after |
| Open PRs/branches on the four upstream repos are stranded | Medium | Phase 0 tags + a freeze window; land the merge quickly rather than over weeks |
| Renames break contributors' in-flight work | Low | Single announced commit; `git log --follow` still works |
| Altera pulls the 25.1 installer, breaking the FPGA flake | Low but unfixable-in-a-hurry | Document the manual install fallback in `fpga/README.md`; the dev shell matters more than the packaged build |

## 8. Open questions for the maintainer

**All of these are now answered — see [decisions.md](decisions.md).** Retained below for the
reasoning; the log carries the outcomes. The only outstanding work is hardware verification
of the Quartus 25.1 gateware (P0-3), which is a Phase 6 gate.

1. ~~Should the four upstream repos be archived or deleted?~~ **Answered:** neither — they
   are left alone and cleaned up separately (P0-6).
2. Is anyone consuming `DomesdayDuplicator-docs` or `-gui-app` as a submodule *elsewhere*?
   Still worth knowing, though less urgent now that the repos stay in place and keep working.
3. ~~Is a ~400 MB clone acceptable, or should phase 1 prune history?~~ **Answered:** accept
   ~400 MB; no pruning, no LFS (P0-5). Caveat: an unfavourable P0-2 outcome would force a
   pruning pass after all, and only during Phase 1.
4. ~~Quartus: accept 25.1 or pin an older nixpkgs?~~ **Answered:** accept 25.1 (P0-3). The
   upgrade already exists on `fpgaupdate-202512`, which also replaced the Intel `dcfifo` IP
   with portable Verilog. Hardware verification is outstanding.
5. ~~macOS: `nix flake check` gate or best-effort?~~ **Answered:** best-effort (P0-7). The
   existing Homebrew CI jobs remain the authoritative macOS coverage.
6. ~~The Cypress licence review (P0-2).~~ **Answered:** vendor the SDK regardless, refreshed
   from the official download, with the vendor's `license.txt` shipped alongside it.

All decisions are now taken; only P0-3's hardware verification remains, as a Phase 6 gate.
