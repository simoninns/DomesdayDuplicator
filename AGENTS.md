# AGENTS.md

Conventions for anyone — human or automated assistant — working in this repository.

Everything here applies to agents *and* to people. Where a rule exists specifically because
assistants get it wrong by default, that is called out.

---

## Rule 1 — No automatic git operations

**Never run a command that changes repository state unless the user has explicitly asked for
that specific action in that specific message.**

> **Forbidden without an explicit request:** `git add`, `git commit`, `git push`,
> `git stash`, `git rebase`, `git reset`, `git revert`, `git merge`, `git cherry-pick`,
> `git tag`, `git branch -d`/`-D`, any `git checkout`/`git switch` that discards changes,
> `git clean`, and any `gh pr create` / `gh pr merge`.
>
> **Always permitted:** `git status`, `git log`, `git diff`, `git show`, `git blame`,
> `git ls-files`, `git grep` — anything read-only.

The loopholes that need closing explicitly:

- **"Make this change" means edit the files.** It does not imply staging, and it does not
  imply committing.
- **Permission for one commit does not carry to the next.** Each one is asked for separately.
- **Finishing a task is not a trigger to commit.** Leave the work in the working tree and
  report what changed.
- **Do not commit and then offer to undo it.** If a commit seems useful, say so and wait.
- A dirty working tree at the end of a task is the expected outcome, not a loose end.

This matters more here than in a typical repository. The history in this monorepo was
assembled by rewriting and merging four former submodule repositories with
`--allow-unrelated-histories` (see [docs-tech/submodule-migration.md](docs-tech/submodule-migration.md)).
An unrequested `git commit` or `git push` in the middle of that kind of work is expensive to
unpick.

## Rule 2 — No AI attribution anywhere

**Nothing this project produces may advertise the tools used to produce it.**

> **Forbidden strings** — in commit messages, PR titles and bodies, code comments,
> documentation, changelogs and release notes:
>
> - `Co-Authored-By: Claude …`, or any `Co-authored-by:` naming an AI tool or service
> - `Generated with …`, `Created by …`, `Written with the help of …`
> - `🤖`, "AI-assisted", or any tool or vendor name used as an attribution
> - Links to AI products in generated content

Commit messages describe **the change**, not how it was written.

This is stated explicitly because several coding assistants append attribution trailers by
default unless told not to, and silence is read as consent.

The same restriction covers advertisements, promotions and commercial references generally.
This is a preservation tool, not a shop window.

---

## 1. What this project is

The Domesday Duplicator is a LaserDisc-focused RF capture device: 40 million samples per
second, 10-bit resolution, over USB 3.0. The repository holds the whole thing — board,
gateware, firmware, host software and documentation.

### 1.1 Components and their toolchains

| Component | Language / format | Toolchain | Runs on |
| --- | --- | --- | --- |
| [hardware/](hardware/) | KiCad 5 schematics and PCB | KiCad | — |
| [fpga/](fpga/) | Verilog | Intel Quartus Prime Lite | Cyclone IV `EP4CE22F17C6` on a Terasic DE0-NANO |
| [fx3/firmware/](fx3/firmware/) | C, bare metal | `arm-none-eabi-gcc` + CMake | Cypress FX3 (ARM926EJ-S) on a **SuperSpeed Explorer Kit CYUSB3KIT-003**, which plugs into the main board's GPIF II headers — see [fx3/README.md](fx3/README.md) |
| [fx3/programmer/](fx3/programmer/) | C | host compiler + CMake, libusb-1.0 | Developer machine |
| [gui/](gui/) | C++20 | Qt 6.2+ + CMake | Developer / user machine |
| [docs/](docs/) | Markdown | MkDocs + Material | GitHub Pages |

Five toolchains, four target architectures. Assume nothing transfers between them.

### 1.2 Repository layout

```
├── AGENTS.md                  # this file
├── TESTING.md                 # test tiers, and the hardware-in-the-loop procedure
├── CONTRIBUTING.md
├── LICENSE                    # GPLv3 — software
├── README.md
├── .editorconfig              # repository-wide formatting
├── .envrc                     # direnv: `use flake` (opt-in)
├── flake.nix                  # the ONLY flake: packages, dev shells, checks, NixOS module
├── flake.lock                 # the ONLY lock — components never carry either file
├── nix/
│   ├── lib.nix                # supported systems, shared pkgs config
│   ├── shell.nix              # the default dev shell
│   ├── checks.nix             # whole-tree checks that belong to no component
│   └── modules/udev.nix       # NixOS device permissions
├── tools/                     # repository-wide scripts, run by hand and by the checks
│   └── check-licence-headers.sh
├── .github/workflows/         # deploy-docs.yml — builds the site with Nix
├── docs/
│   ├── mkdocs.yml             # site config; docs_dir is "content", never "site"
│   └── content/               # the site's markdown, one directory per nav section
├── docs-tech/                 # engineering-process docs for the repository itself
├── fpga/
│   ├── README.md
│   ├── src/                   # Quartus project and Verilog
│   ├── tests/                 # testbenches, lint and simulation runners
│   ├── configs/               # USB-Blaster udev rules
│   ├── verilator-waivers.vlt  # lint waivers, each with its reason
│   ├── bitstream-provenance.py
│   ├── build-local.sh         # out-of-tree local build
│   ├── checks.nix             # fpga-lint, fpga-sim, fpga-provenance
│   ├── package.nix            # the bitstream — unfree, x86_64-linux, not in CI
│   └── quartus-shell.nix      # nix develop .#fpga-quartus
├── fx3/
│   ├── README.md
│   ├── .clangd                # per-component, see §7
│   ├── firmware/
│   │   ├── src/               # firmware C sources
│   │   ├── tests/             # descriptor golden test and its reference headers
│   │   ├── gpif/              # GPIF II Designer project (not built)
│   │   ├── CMakeLists.txt
│   │   ├── arm-none-eabi-toolchain.cmake
│   │   └── package.nix        # the cross build
│   ├── mkimage/               # fx3-mkimage: ELF -> FX3 boot image (host tool)
│   │   ├── src/
│   │   └── tests/
│   ├── programmer/
│   │   └── src/
│   └── sdk/                   # vendored Cypress FX3 SDK 1.3.5
├── graphics/                  # logos and screenshots used by READMEs
├── gui/
│   ├── CMakeLists.txt         # the single build definition
│   ├── cmake/                 # FindLibUSB.cmake, FindFLAC.cmake
│   ├── packaging/             # flatpak/, windows/, macos/, assets/ — the installers
│   └── src/
│       ├── DomesdayDuplicator/  # the capture application
│       └── common/              # Qt-free core: sample codec, FLAC writer, reader, analyser
└── hardware/
    ├── pcb/                   # KiCad project
    └── doc/
```

There are **no git submodules**. A plain `git clone` gives the complete project.

## 2. Component boundaries

Each component builds independently, and that must stay true. The monorepo makes it easy to
violate for the first time.

- **No cross-component source includes.** `gui/` must not `#include` from `fx3/`, and so on.
- **Shared constants are duplicated deliberately.** The USB VID/PID (`0x1D50`/`0x603B`) and
  the control-bit assignments appear separately in the gateware, the firmware and the host
  software. This is not accidental duplication to be refactored away — it is a *wire
  protocol*, and the three definitions live in three different languages on three different
  processors. Document the protocol in one place and note each definition site.
- **Any change to the FPGA ↔ FX3 ↔ host control protocol touches three components** and must
  say so explicitly in the pull request. A change that updates only two of the three produces
  a device that enumerates and captures nothing but garbage.

## 3. Do not hand-edit generated or vendored files

| Path | Rule |
| --- | --- |
| `fx3/sdk/**` | Vendored Infineon/Cypress SDK. Never reformat, never "fix" warnings, never re-indent. Do not restore `fx3/sdk/util/` — see [fx3/sdk/README.md](fx3/sdk/README.md) |
| `fx3/firmware/src/domesday-duplicator-gpif.h` | Generated by GPIF II Designer. Regenerate it, do not edit it — [fx3/firmware/gpif/README.md](fx3/firmware/gpif/README.md) |
| `fpga/src/IPfifo.v`, `fpga/src/IPpllGenerator.v` | Originally MegaWizard output, but **treated as source of truth**. Change `defparam` values deliberately; do not reformat |
| `gui/src/DomesdayDuplicator/qcustomplot.{cpp,h}` | Vendored third-party library |
| `hardware/pcb/Gerber/`, `hardware/pcb/PDF/` | Plotted from the KiCad project. Regenerate, do not edit |

The root `.editorconfig` marks these paths as `unset` so a "format on save" cannot quietly
rewrite them, but that is a safety net rather than the rule.

**`fx3/mkimage/` is not on this list and is not vendored**, despite sitting next to
`fx3/sdk/`. It is the project's own GPLv3 code, written against a public specification to
replace the SDK's proprietary `elf2img`. Treat it as ordinary project source — but see
[fx3/mkimage/README.md](fx3/mkimage/README.md) before changing the image format, because its
output has to stay something the FX3 bootloader accepts.

## 4. Hardware safety

Unique to this project, and non-negotiable:

- Changes to gateware, FX3 firmware or the capture path can brick a device or — worse —
  silently corrupt captures. **A green build is not sufficient evidence.**
- Any such change requires the hardware-in-the-loop procedure before merge. The project has a
  complete end-to-end integrity oracle: `dataGenerator.v` emits a known 0…1020 counter ramp
  in test mode, and the capture application's test-data analysis walks a captured file checking
  that ramp is unbroken — **Edit → Analyse test data...**, or `--analyse-test-data <file>` from
  a shell, which exits non-zero on a break so the check can be scripted. Any discontinuity proves a dropped sample somewhere across
  FPGA → FIFO → FX3 → USB 3.0 → host → disk. Zero sequence breaks is the pass condition.
- **Never propose a change that writes to the FX3 EEPROM or the FPGA EPCS flash as part of an
  automated test.** Permanent programming is a deliberate manual act.

## 5. Coding standards

### 5.1 C++ (`gui/`)

- C++20. `set(CMAKE_CXX_STANDARD 20)` is already in `gui/CMakeLists.txt`.
- Follow the style of the file you are editing. The GUI code is Qt-idiomatic: `QString` over
  `std::string` at API boundaries, signals and slots for cross-object communication.
- `ILogger.h`, `UsbDeviceBase` with its `UsbDeviceLibUsb`/`UsbDeviceWinUsb` subclasses, and
  the header-only `StringUtilities` are deliberately abstract shapes. Prefer extending that
  pattern to adding concrete cross-dependencies.
- Do not reformat code you are not otherwise changing. Whitespace-only diffs bury the change.

### 5.2 C (`fx3/`)

- The firmware is C, `gnu11`, freestanding. There is no libc beyond what the SDK provides,
  no heap beyond the CyU3P allocator, and no `printf` on the device.
- The programmer is C99 and host-native.
- Keep new firmware logic in pure helpers where possible, so it can be host-compiled and
  tested. Everything that touches CyU3P, DMA or GPIF can only be tested on hardware.

### 5.3 Verilog (`fpga/`)

- Follow the existing style: `lowerCamelCase` signal names, `always @(posedge clock)`.
- Explicit widths on all literals.
- `./fpga/tests/run-lint.sh` must pass. It runs `verilator --lint-only -Wall`, so new code is
  held to the whole warning set.
- **Do not silence a lint finding without a reason.** `fpga/verilator-waivers.vlt` waives
  nine pre-existing findings, each with a written justification and each pinned by a
  testbench. A waiver with no reason is indistinguishable from a bug someone hid.
- Changing gateware means the bitstream has to be rebuilt *and* re-verified on hardware
  (TESTING.md §5). A change that is claimed to be behaviour-neutral can be shown to be: build
  a `.jic` before and after and compare — it is byte-identical across rebuilds.

### 5.4 Licence headers

The project is **GPLv3 for software** and **CC BY-SA 4.0 for hardware** (§10).

**Every project-authored source file carries a copyright statement and a licence
statement.** That is checked, not merely asked for: `./tools/check-licence-headers.sh` runs
as the T4 `licence-headers` check in `nix flake check`, and a file missing either half fails
the build. It covers `.c .h .cpp .inl .v .py .sh .nix .S`; vendored and generated files are
exempt by name in the script, each with its reason.

The convention is **SPDX**. New files use it. In a `/* */` or `//` language:

```c
/************************************************************************

    flacwriter.cpp

    Ogg FLAC capture output
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/
```

and in a `#` language, immediately after the file's opening description paragraph (and after
the shebang, where there is one):

```bash
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
```

Rules that go with it:

- **Name every substantive author**, one `SPDX-FileCopyrightText` line each, oldest first,
  with the years that author actually worked on the file. `gui/src/DomesdayDuplicator/`'s
  USB and logging files carry two lines for this reason. A one-line patch — a typo, a missing
  `#include` — is not a new copyright line.
- **Do not add a long-form GPL notice to a new file.** 25 files still carry one; they are
  valid and the check accepts them. Convert one to SPDX when you are already editing it, not
  in a sweep of its own. The check prints the remaining count on every run so the number
  stays visible.
- **Do not touch the header of anything in the exempt list.** A vendor refresh or a tool
  regeneration overwrites it, so the change is lost and the diff is noise.

## 6. Naming

- Never name a file, directory, branch, symbol or CMake target after a plan step, phase or
  iteration. `phase0`, `v2`, `new`, `final`, `refactored` are all forbidden — they are
  meaningless within weeks and permanent in git history.
- Name things after what they do.

## 7. Development environment

Every component builds with ordinary, distribution-packaged tools. There is also a Nix flake
— but **Nix is not required**, and no build may be made Nix-only.

**Run every one of these from anywhere in the working tree.** Nix walks up to find the root
flake, so the directory you are in makes no difference; the `.#name` selects the component.

```bash
nix develop                  # all free components in one shell
nix develop .#gui            # or .#fx3, .#fpga, .#hardware, .#docs
nix develop .#fpga-quartus   # adds Quartus; x86_64-linux only, multi-GB first download
nix build .#gui .#fx3-firmware .#fx3-programmer .#fx3-mkimage
nix flake check              # build everything and run the T1-T4 tests
```

A bare `nix develop` always gives the all-components default shell, in any directory. It
does *not* pick up the component you happen to be standing in.

### One flake, one lock

**There is exactly one `flake.nix`, at the repository root, and exactly one `flake.lock`.**
Components carry `package.nix` and `shell.nix`; they must not carry a `flake.nix`.

This is a reproducibility rule, and it is load-bearing. An earlier layout gave each component
a thin flake so `cd gui && nix develop` would work. Every one of those flakes declared
`nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable"` and grew a lock file of its own, so
entering the tree through a component resolved whatever `nixos-unstable` pointed at that day
— a different nixpkgs from the root pin, with no warning that it had happened. **Do not
reintroduce component flakes.** Design notes:
[docs-tech/nix-flake-design.md](docs-tech/nix-flake-design.md).

Quartus is unfree, x86_64-linux only and not redistributable, so the bitstream build is
guarded by system and never runs in CI — but it still comes from the root flake, fed by a
second import of the *same locked* nixpkgs with `allowUnfree` set. Containing an unfree
dependency does not require a second lock file. `nix develop .#fpga` gives the *free* tools —
Verilog lint, simulation and a language server — with no Quartus download at all.

NixOS users get device permissions from `nixosModules.udev`:
`hardware.domesdayDuplicator.enable = true;`

### 7.1 Editor independence

The project must never require a specific IDE. This is a hard constraint, not a preference:
the FX3 sources came from Eclipse CDT, the GUI from Qt Creator, and the gateware from the
Quartus GUI, and each brought a build definition that drifted from the real one.

- `CMAKE_EXPORT_COMPILE_COMMANDS` is on in every CMake component, so configuring a build
  writes `build/compile_commands.json`.
- Each component has a `.clangd` pointing at its build directory. `fx3/firmware/.clangd`
  additionally sets `Compiler: arm-none-eabi-gcc`, without which clangd analyses bare-metal
  ARM sources against the host libc and reports hundreds of false errors.
- Any editor with a language-server client works with no further setup.
- `compile_commands.json` is gitignored — it embeds absolute paths from whoever configured
  the build.
- Do not add IDE project files. `.vscode/`, `.idea/`, `.project`, `.cproject`, `*.pro.user`
  and friends are gitignored at the root specifically so they cannot drift back in.

Per-editor instructions — VS Code, Neovim, Emacs, Helix, Qt Creator, CLion, KDevelop — are in
[docs-tech/editor-setup.md](docs-tech/editor-setup.md).

### 7.2 Common commands

```bash
# GUI (Qt 6 + libusb)
cmake -B gui/build -S gui && cmake --build gui/build

# FX3 firmware (cross-compiled). Add -DFIRMWARE_VERSION=<hash> outside a git checkout.
cmake -B fx3/firmware/build -S fx3/firmware \
      -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake
cmake --build fx3/firmware/build

# FX3 programmer (host)
cmake -B fx3/programmer/build -S fx3/programmer
cmake --build fx3/programmer/build

# FPGA bitstream (needs Quartus Prime Lite: nix develop .#fpga-quartus)
./fpga/build-local.sh          # or, hermetically: nix build .#bitstream
```

Build directories are `build/` under each component and are gitignored. Never build in-tree.

That applies with particular force to `fpga/src/`: `quartus_sh` **rewrites the `.qsf` in
place** to record `LAST_QUARTUS_VERSION`, so compiling there dirties a tracked file on every
build and scatters thirty-odd products beside the sources. `build-local.sh` and
`nix build .#bitstream` both copy to a build directory first.

## 8. Testing

Read [TESTING.md](TESTING.md). It defines five tiers — `unit`, `golden`, `sim`, `static`,
`hil` — attached to every test as a CTest label, and documents the hardware-in-the-loop
capture-integrity procedure that is the most important test in the project.

```bash
nix flake check                    # everything, on a clean machine
ctest --test-dir gui/build         # one component
```

**What exists today: 78 tests across four components** — 21 in `gui/` (UTF-8 conversion, the
10-bit/16-bit sample codec), 24 in `fx3/programmer/` (EEPROM paging arithmetic,
secondary-loader path resolution, the CLI contract), 32 in `fx3/mkimage/` (boot image construction) and one
golden test in `fx3/firmware/` (the generated USB product descriptor). `fpga/` adds three
Verilog testbenches, a `-Wall` lint pass over five modules and a bitstream-digest test, none
of them under CTest — there is no CMake there. `hardware/` has **no automated coverage yet**;
`docs/` has a static check only. Across the whole tree, `licence-headers` is a T4 check with
no component of its own (§5.4). TESTING.md §6 says what is planned and in which phase.

`fpga/buffer.v` is deliberately uncovered: it is two Altera `dcfifo` instances, and `dcfifo`
has no free simulation model. Do not describe the gateware as tested without that caveat.

Do not write documentation, comments or PR descriptions implying coverage that does not
exist. Where a change needs manual verification, state plainly what you did — which component
you built, on what, and what you observed. "Should work" is not verification.

Two things that look like tests are not:

- `gui/src/common/testdataanalyser.cpp` is a *product feature* that analyses captured
  test-pattern data. It is the host half of the §4 integrity oracle, not a test of the code —
  though it does have its own unit tests, because a gate that cannot fail proves nothing.
- `docs/TESTING.md` is a manual site-preview guide, superseded at the repository root.

When adding logic, put it somewhere it can be tested. The pure parts of this codebase live in
`gui/src/common/`, `fx3/programmer/src/fx3-paging.h` and
`fx3/programmer/src/fx3-flashprog.c` precisely so they can be exercised without Qt, libusb or
hardware — extend that pattern rather than adding logic inside an I/O loop.

## 9. Releases and artefact provenance

Every artefact a user installs is built by CI, and **a release contains exactly the artefacts
built from the release commit** — not a rebuild of roughly that source.

| Artefact | Built by | When |
| --- | --- | --- |
| GUI (Linux x64/ARM64, Windows x64, macOS x64/ARM64) | CI | Every commit |
| `firmware.img` / `.elf` / `.map` | CI (`nix build .#fx3-firmware`) | Every commit |
| `fx3-programmer` | CI (`nix build .#fx3-programmer`) | Every commit |
| FPGA `.sof` / `.jic` | **Local build, attached by hand** | Per release |

Two rules follow from this, and they constrain how you change build files:

1. **Never make a version discoverable only at build time.** A Nix build from a tag has no
   `.git`, so `git rev-parse` yields `unknown` and produces an untraceable release binary.
   Versions are *injected* — `-DFIRMWARE_VERSION=`, `-DDDD_VERSION=` — with the git lookup as
   a fallback for local developer builds only. The release workflow fails if any artefact
   reports `unknown`.
2. **Do not remove the native build matrix in favour of Nix.** Nix cannot produce the Windows
   binary. The two paths coexist deliberately; deleting the native jobs would silently drop a
   supported platform from every future release.

The FPGA is the exception because Quartus is unfree, GB-scale and `redistributable = false`,
so it can never come from a binary cache and every cold CI run would re-fetch it. It is
therefore excluded from CI for now, built locally, and attached to releases with a provenance
record and published digests.

A note on reproducibility, since it is easy to get wrong in both directions: Quartus *fitting*
is deterministic — same source, same seed, same toolchain gives the same placement and
routing, regardless of the build machine. What is **not** guaranteed is byte-identity of the
`.sof`, because a compile timestamp is embedded in the bitstream header. So do not assume a
rebuild will hash-match the released file, and do not assume the design differs just because
it does not.

Full model: [docs-tech/implementation-plan.md](docs-tech/implementation-plan.md) →
*Release artefacts and provenance*.

## 10. Licensing

| | Licence | File |
| --- | --- | --- |
| Software | **GNU GPL v3** | [LICENSE](LICENSE) |
| Hardware | **CC BY-SA 4.0** | [hardware/pcb/LICENSE.txt](hardware/pcb/LICENSE.txt) |

These two were transposed in the README until Phase 2 of the reorganisation — the labels each
pointed at the other one's file. If you find them stated the other way round anywhere, that is
the old error, not a second opinion.

Third-party components keep their own licences:

- `fx3/sdk/` — Cypress/Infineon proprietary; see [fx3/sdk/README.md](fx3/sdk/README.md)
- `fx3/programmer/cyfxflashprog.img` and the code derived from `cyusb_linux` — LGPL-2.1; see
  [fx3/programmer/VENDOR.md](fx3/programmer/VENDOR.md)
- `gui/src/DomesdayDuplicator/qcustomplot.*` — GPLv3, upstream

Do not add a dependency whose licence is incompatible with GPLv3 without raising it first.

## 11. Documentation

| Where | What |
| --- | --- |
| [docs/content/](docs/content/) | The published project website — user-facing. Markdown images only; see [docs/README.md](docs/README.md) |
| [docs-tech/](docs-tech/) | Engineering-process documentation for this repository: the reorganisation plan, decision log, flake design, defect register |
| Component `README.md` | How to build and use that one component |

Documentation changes belong in the same repository, and usually the same pull request, as
the change they describe.

## 12. Contribution hygiene

- Keep changes focused. A re-layout and a behaviour change in one commit cannot be reviewed
  or reverted independently.
- Never mix a rename with an edit to the same file — `git log --follow` stops working, and
  the diff becomes unreadable.
- Discuss significant changes in an issue first.
- Describe how you verified the change: which component, which toolchain, what you observed.

## 13. When to stop and ask

Stop and ask rather than guessing when:

- A change would touch the FPGA ↔ FX3 ↔ host protocol.
- A change would alter the capture data path in any way.
- A vendored or generated file appears to need editing.
- The licence position of a new dependency is unclear.
- A build fails in a way that suggests the *plan* is wrong rather than the code.

Report what you found and what you did not do. An honest "this part is blocked, here is why"
is worth more than a confident guess.
