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
| [fx3/firmware/](fx3/firmware/) | C, bare metal | `arm-none-eabi-gcc` + CMake | Cypress FX3 (ARM926EJ-S) |
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
├── flake.nix                  # aggregator: packages, dev shells, checks, NixOS module
├── nix/
│   ├── lib.nix                # supported systems, shared pkgs config
│   ├── shell.nix              # the default dev shell
│   └── modules/udev.nix       # NixOS device permissions
├── .github/workflows/         # deploy-docs.yml — builds the site with Nix
├── docs/
│   ├── mkdocs.yml             # site config; docs_dir is "content", never "site"
│   └── content/               # the site's markdown, one directory per nav section
├── docs-tech/                 # engineering-process docs for the repository itself
├── fpga/
│   ├── README.md
│   └── src/                   # Quartus project and Verilog
├── fx3/
│   ├── README.md
│   ├── .clangd                # per-component, see §7
│   ├── firmware/
│   │   ├── src/               # firmware C sources
│   │   ├── gpif/              # GPIF II Designer project (not built)
│   │   ├── CMakeLists.txt
│   │   └── arm-none-eabi-toolchain.cmake
│   ├── programmer/
│   │   └── src/
│   └── sdk/                   # vendored Cypress FX3 SDK 1.3.5
├── graphics/                  # logos and screenshots used by READMEs
├── gui/
│   ├── CMakeLists.txt         # the single build definition
│   ├── cmake/                 # FindLibUSB.cmake
│   └── src/{DomesdayDuplicator,dddconv,dddutil}/
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
| `fx3/sdk/**` | Vendored Infineon/Cypress SDK. Never reformat, never "fix" warnings, never re-indent. See [fx3/sdk/README.md](fx3/sdk/README.md) |
| `fx3/firmware/src/domesday-duplicator-gpif.h` | Generated by GPIF II Designer. Regenerate it, do not edit it — [fx3/firmware/gpif/README.md](fx3/firmware/gpif/README.md) |
| `fpga/src/IPfifo.v`, `fpga/src/IPpllGenerator.v` | Originally MegaWizard output, but **treated as source of truth**. Change `defparam` values deliberately; do not reformat |
| `gui/src/DomesdayDuplicator/qcustomplot.{cpp,h}` | Vendored third-party library |
| `hardware/pcb/Gerber/`, `hardware/pcb/PDF/` | Plotted from the KiCad project. Regenerate, do not edit |

The root `.editorconfig` marks these paths as `unset` so a "format on save" cannot quietly
rewrite them, but that is a safety net rather than the rule.

## 4. Hardware safety

Unique to this project, and non-negotiable:

- Changes to gateware, FX3 firmware or the capture path can brick a device or — worse —
  silently corrupt captures. **A green build is not sufficient evidence.**
- Any such change requires the hardware-in-the-loop procedure before merge. The project has a
  complete end-to-end integrity oracle: `dataGenerator.v` emits a known 0…1020 counter ramp
  in test mode, and `dddutil`'s test-data analysis walks a captured file checking that ramp is
  unbroken. Any discontinuity proves a dropped sample somewhere across
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

### 5.4 Licence headers

The project is **GPLv3 for software** and **CC BY-SA 4.0 for hardware** (§9).

Nine of the 69 source files currently carry SPDX identifiers; the rest use the long-form GPL
notice. The convention is to **adopt SPDX**, converting files opportunistically as they are
touched rather than in one sweeping commit. Do not add a long-form header to a new file.

## 6. Naming

- Never name a file, directory, branch, symbol or CMake target after a plan step, phase or
  iteration. `phase0`, `v2`, `new`, `final`, `refactored` are all forbidden — they are
  meaningless within weeks and permanent in git history.
- Name things after what they do.

## 7. Development environment

Every component builds with ordinary, distribution-packaged tools. There are also per-component
Nix flakes — but **Nix is not required**, and no build may be made Nix-only.

```bash
nix develop                  # all free components in one shell
nix develop .#gui            # or .#fx3, .#fpga, .#hardware
nix build .#gui .#fx3-programmer
nix flake check              # build everything and run the T1-T4 tests
```

Each component has a `flake.nix` that is a thin wrapper over a shared `package.nix` and
`shell.nix`; the root flake `callPackage`s the same files. There is therefore exactly one
definition of each component and no cross-flake inputs. Design notes:
[docs-tech/nix-flake-design.md](docs-tech/nix-flake-design.md).

`fpga/` is the exception: Quartus is unfree, x86_64-linux only and not redistributable, so it
stays behind its own flake and is never aggregated into the root one. `nix develop .#fpga`
gives the *free* tools — Verilog lint, simulation and a language server — with no Quartus
download at all.

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

# FX3 firmware (cross-compiled)
cmake -B fx3/firmware/build -S fx3/firmware \
      -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake
cmake --build fx3/firmware/build

# FX3 programmer (host)
cmake -B fx3/programmer/build -S fx3/programmer
cmake --build fx3/programmer/build

# FPGA bitstream (needs Quartus Prime Lite)
cd fpga/src && quartus_sh --flow compile DomesdayDuplicator
```

Build directories are `build/` under each component and are gitignored. Never build in-tree.

## 8. Testing

Read [TESTING.md](TESTING.md). It defines five tiers — `unit`, `golden`, `sim`, `static`,
`hil` — attached to every test as a CTest label, and documents the hardware-in-the-loop
capture-integrity procedure that is the most important test in the project.

```bash
nix flake check                    # everything, on a clean machine
ctest --test-dir gui/build         # one component
```

**What exists today: 44 tests across two components** — 21 in `gui/` (UTF-8 conversion, the
10-bit/16-bit sample codec) and 23 in `fx3/programmer/` (EEPROM/flash paging arithmetic,
secondary-loader path resolution). `fx3/firmware/`, `fpga/`, `hardware/` and `docs/` have
**no automated coverage yet**; TESTING.md §4.3 says what is planned and in which phase.

Do not write documentation, comments or PR descriptions implying coverage that does not
exist. Where a change needs manual verification, state plainly what you did — which component
you built, on what, and what you observed. "Should work" is not verification.

Two things that look like tests are not:

- `gui/src/dddutil/analysetestdata.cpp` is a *product feature* that analyses captured
  test-pattern data. It is the host half of the §4 integrity oracle, not a test of the code.
- `docs/TESTING.md` is a manual site-preview guide, superseded at the repository root.

When adding logic, put it somewhere it can be tested. The pure parts of this codebase live in
`gui/src/dddconv/samplecodec.h`, `fx3/programmer/src/fx3-paging.h` and
`fx3/programmer/src/fx3-flashprog.c` precisely so they can be exercised without Qt, libusb or
hardware — extend that pattern rather than adding logic inside an I/O loop.

## 9. Licensing

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

## 10. Documentation

| Where | What |
| --- | --- |
| [docs/content/](docs/content/) | The published project website — user-facing. Markdown images only; see [docs/README.md](docs/README.md) |
| [docs-tech/](docs-tech/) | Engineering-process documentation for this repository: the reorganisation plan, decision log, flake design, defect register |
| Component `README.md` | How to build and use that one component |

Documentation changes belong in the same repository, and usually the same pull request, as
the change they describe.

## 11. Contribution hygiene

- Keep changes focused. A re-layout and a behaviour change in one commit cannot be reviewed
  or reverted independently.
- Never mix a rename with an edit to the same file — `git log --follow` stops working, and
  the diff becomes unreadable.
- Discuss significant changes in an issue first.
- Describe how you verified the change: which component, which toolchain, what you observed.

## 12. When to stop and ask

Stop and ask rather than guessing when:

- A change would touch the FPGA ↔ FX3 ↔ host protocol.
- A change would alter the capture data path in any way.
- A vendored or generated file appears to need editing.
- The licence position of a new dependency is unclear.
- A build fails in a way that suggests the *plan* is wrong rather than the code.

Report what you found and what you did not do. An honest "this part is blocked, here is why"
is worth more than a confident guess.
