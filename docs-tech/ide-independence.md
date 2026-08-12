# IDE independence

**Requirement:** no part of the project may depend on a specific IDE. Quartus ships a
proprietary GUI; the Cypress FX3 flow is traditionally Eclipse-based (EZ-USB Suite). Neither
may be needed to build, test, program or contribute. Development happens in VS Code today,
but the repository must not know or care.

Three tests for "IDE-free". A component passes only if all three hold:

1. **Every build, verify and program step runs from a shell** in `nix develop`, with no GUI
   in the path.
2. **No committed file is only editable by one tool** — or if it is (vendor design data),
   its *output* is committed and reviewable, and that output is what the build consumes.
3. **Language support works in any editor**, via LSP, with no per-editor project files.

Nothing here privileges VS Code. `compile_commands.json` and `.clangd` are LSP artefacts
consumed by clangd — VS Code, Neovim, Emacs, Helix, Zed and Qt Creator all read them.

## 1. Where the project stands

| Component | IDE coupling today | Verdict |
| --- | --- | --- |
| FX3 firmware | **None.** No `.project`, `.cproject`, `.settings` or `.launch` anywhere in the tree | Eclipse dependency already removed — protect it |
| FX3 GPIF | `GPIF_II/*.cydsn` needs GPIF II Designer (Windows-only) | Design-time only; the generated header is committed and is what builds |
| FPGA | No build script — GUI by convention, not by necessity (§2) | Fixable with a script; no blocker |
| GUI app | Four qmake `.pro` files for Qt Creator, and `BUILD.md` steers users to them (**D14**) | Delete; CMake is already primary |
| All C/C++ | No `compile_commands.json` anywhere (**D15**) | The single highest-value fix |
| Repo-wide | One `.editorconfig`, buried in `gui-app/tools/DomesdayDuplicator/`; `.vscode`/`.idea` ignore rules only in `gui-app/.gitignore` (**D16**) | Promote to root |

The FX3 firmware is the good news: the CMake + `arm-none-eabi-toolchain.cmake` build already
replaced EZ-USB Suite. That was done before this plan and just needs guarding against
regression.

## 2. FPGA without the Quartus GUI

### 2.1 The CLI tools are all there

nixpkgs' `quartus-prime-lite` wrapper wraps **everything** in `${unwrapped}/quartus/bin/*`
plus `qsys-{generate,edit,script}` and symlinks it all into `$out/bin` (verified in
`pkgs/by-name/qu/quartus-prime-lite/package.nix`). So `nix develop ./fpga` puts
`quartus_sh`, `quartus_map`, `quartus_fit`, `quartus_asm`, `quartus_sta`, `quartus_cpf`,
`quartus_pgm` and `jtagd` on `PATH`. The `quartus` GUI is one binary among many — available
if wanted, never required. Quartus Prime **Lite** needs no licence file, so there is no
GUI-only licensing step either.

```bash
# full compile, reading the committed .qsf
quartus_sh --flow compile DomesdayDuplicator

# or staged, when you want the intermediate reports
quartus_map DomesdayDuplicator && quartus_fit DomesdayDuplicator \
  && quartus_asm DomesdayDuplicator && quartus_sta DomesdayDuplicator

# programming files — the .cof is already committed
quartus_cpf -c DomesdayDuplicator.cof

# program the device — the .cdf files are already committed too
quartus_pgm DomesdayDuplicator_write_sof.cdf     # volatile, to SRAM
quartus_pgm DomesdayDuplicator_write_jic.cdf     # permanent, to EPCS
```

Anything more elaborate goes in a Tcl script run with `quartus_sh -t build.tcl`.

Two practical notes:

- `quartus_pgm` needs `jtagd` running and a udev rule for the USB-Blaster. **The nixpkgs
  package ships no udev rules** (checked). Put the USB-Blaster rule in the project's own
  NixOS module alongside the FX3 rule — one `hardware.domesdayDuplicator.enable` covering
  both devices is the right shape.
- The nixpkgs build disables the `quartus_help` component, so built-in help is reduced. Use
  Intel's online command reference.

### 2.2 The IP does not need the wizard — this was the real risk

Earlier drafts of this plan flagged "may need `IPfifo`/`IPpllGenerator` regenerated under a
newer Quartus" as a **large** task, on the assumption that MegaWizard-generated IP implies a
GUI dependency. Having read the files, that is not the case:

- `IPfifo.v` directly instantiates `dcfifo` from `altera_mf`, with every parameter as an
  explicit `defparam` (`lpm_numwords = 8192`, `lpm_width = 16`, `rdsync_delaypipe = 5`, …).
- `IPpllGenerator.v` likewise instantiates `altpll`.
- Both are **committed plain Verilog**, pulled into the project by `.qip` files that do
  nothing but `set_global_assignment -name VERILOG_FILE`.

Nothing invokes MegaWizard at build time. `quartus_map` compiles those `.v` files like any
other source. `IPpllGenerator.ppf` is a wizard *parameter* file, listed as a `MISC_FILE` —
irrelevant to compilation.

So the wizard is needed only to **re-parameterise** the IP, and even that is avoidable: FIFO
depth or PLL multiply/divide can be changed by editing the `defparam` block directly, because
`dcfifo` and `altpll` are ordinary instantiable library megafunctions.

Consequences for the plan:

- **P0-3 shrinks.** The spike is "does Quartus 25.1 compile the committed sources for
  Cyclone IV E", not "can we regenerate IP headlessly". Risk drops from High to Medium.
- **P6-3 (IP regeneration) is probably unnecessary.** Keep it as a contingency for the case
  where 25.1 rejects the 2017-era `intended_device_family` strings or deprecated parameters.
- Treat the generated `.v` files as **source of truth** from here on, and say so in
  `fpga/README.md`. They already are in practice.
- If a genuine regeneration is ever needed headlessly, `qsys-script`, `qsys-generate` and
  `quartus_ipgenerate` are wrapped too — but that means converting the old megafunction IP to
  modern `.qsys`/`.ip` form, which is a design change, not a build change.

### 2.3 Verifying gateware without Quartus at all

`verilator` 5.048 and `iverilog` 13.0 are in nixpkgs. The project-authored modules
(`DomesdayDuplicator.v`, `buffer.v`, `dataGenerator.v`, `fx3StateMachine.v`, `statusLED.v`)
are ordinary Verilog and can be linted and simulated with no Quartus, no licence, no GUI, and
on any platform — including CI, where the unfree multi-gigabyte Quartus build cannot run.

The `dcfifo`/`altpll` instantiations need vendor simulation models to elaborate, so
whole-design simulation is not free. But `verilator --lint-only` over the hand-written
modules is, and that alone catches a useful class of mistakes on every push. Worth adding as
a `nix flake check` even though full bitstream builds stay out of CI.

## 3. FX3 without Eclipse

Already achieved, and the checks confirm it: no Eclipse project files anywhere. The build is
CMake plus a toolchain file, driven from a shell.

To protect it:

- Add `.cproject`, `.project`, `.settings/`, `*.launch` to the root `.gitignore`, so they
  cannot drift back in with a casual `git add`.
- Keep `arm-none-eabi-toolchain.cmake` the single source of cross-compilation settings.

Two honest caveats:

- **GPIF II Designer** is still needed to change the GPIF state machine. That is vendor
  design data, and test 2 above is what applies: the generated `domesday-duplicator-gpif.h`
  is committed and is what the build consumes, so no contributor needs the tool to build,
  flash or modify the firmware. Only altering the bus protocol itself requires it. Record in
  `fx3/firmware/gpif/README.md` which `.cydsn` revision produced the committed header.
- **On-chip debugging** (JTAG halt/step of the FX3) is the one workflow that remains
  vendor-tool-shaped. The project does not use it today — debugging is via USB-level tracing
  and the version string in the USB descriptor. Not a gap worth closing speculatively.

## 4. Drop qmake from the GUI app (D14)

`gui-app/tools/` carries four qmake project files — `ddd-tools.pro`,
`DomesdayDuplicator/DomesdayDuplicator.pro`, `dddconv/dddconv.pro`, `dddutil/dddutil.pro` —
alongside the CMake build, and `BUILD.md` tells contributors to open them in Qt Creator.

Delete them:

- They are a **second build definition** that will drift from `CMakeLists.txt` — the same
  failure mode as D2's duplicated CMake front-ends, one level up.
- They are the only *editor-specific* build files in the repo.
- Qt Creator opens CMake projects natively, so deleting them costs Qt Creator users nothing.
  VS Code (CMake Tools), CLion, Zed, Neovim and Emacs all consume CMake or its
  `compile_commands.json` output.

(`hardware/pcb/Domesday Duplicator.pro` is a KiCad project file that happens to share the
extension. Leave it alone.)

## 5. LSP for every editor (D15)

The highest-value change in this document, and the cheapest. Without
`compile_commands.json`, every editor needs hand-maintained include paths; with it, clangd
gives completion, diagnostics, go-to-definition and refactoring in all of them.

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

in each component's top-level `CMakeLists.txt` (or `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` from
the dev shell). Symlink `build/compile_commands.json` to the component root, and gitignore
both it and `.cache/clangd/`.

The FX3 firmware needs one extra step, because clangd defaults to the host compiler and will
flood a bare-metal ARM translation unit with false errors. Commit
`fx3/firmware/.clangd`:

```yaml
CompileFlags:
  Compiler: arm-none-eabi-gcc
  Add: [--target=arm-none-eabi, -mcpu=arm926ej-s]
```

`.clangd` is an LSP configuration file, not an IDE one — every clangd client honours it, so
this is consistent with the requirement rather than an exception to it.

For Verilog, `verible` provides `verible-verilog-ls` (LSP), `verible-verilog-format` and
`verible-verilog-lint`; `svls` is an alternative LSP. Both are in nixpkgs and both are
editor-agnostic.

## 6. Editor configuration without editor lock-in (D16)

| File | Committed? | Why |
| --- | --- | --- |
| `.editorconfig` (root) | **Yes** | Universally supported; encodes indentation and line endings, not tooling. Promote the buried `gui-app/tools/DomesdayDuplicator/.editorconfig` and extend it to Verilog, CMake, Python and YAML |
| `.clangd`, `compile_commands.json` config | **Yes** (`.clangd` only) | LSP, not IDE |
| `.envrc` containing `use flake` | **Yes** | direnv drops the dev shell into whatever terminal or editor you use. Inert for anyone without direnv |
| `.vscode/` | **No** | Committing it would privilege one editor. Gitignore it |
| `.idea/`, `*.user`, `*.creator.user*`, `.qtc*` | **No** | Same, and these already appear in `gui-app/.gitignore` — promote to root |

Editor setup instructions belong in `docs-tech/editor-setup.md`, with sections for VS Code
**and** at least Neovim/Emacs/Helix and Qt Creator, so the documentation does not quietly
impose what the repository is careful not to. Each section should be short: point at
`nix develop` (or direnv), then at clangd and `verible-verilog-ls`.

## 7. Dev shell contents

Each component's `shell.nix` carries its language servers and formatters, so tooling arrives
with the shell rather than from a per-developer install:

| Shell | Additions beyond the build toolchain |
| --- | --- |
| `gui` | `clang-tools` (clangd, clang-format) |
| `fx3` | `clang-tools`, `python3` (for `generate-descriptor.sh`) |
| `fpga` | `verible`, `verilator`, `iverilog` — none of which need Quartus |
| `docs` | the MkDocs Python env (already covered) |
| root `default` | `clang-tools`, `verible`, plus the free build toolchains |

Note `fpga`'s LSP and simulation tools sit in the **free** part of the dependency graph.
Someone editing Verilog gets linting and simulation from `nix develop` without pulling
Quartus at all; only an actual bitstream build needs the unfree download.

## 8. Tasks

Folded into [implementation-plan.md](implementation-plan.md) as:

| Task | Phase | Detail |
| --- | --- | --- |
| **P2-11** Delete the four qmake `.pro` files, update `BUILD.md` | 2 | §4, D14 |
| **P2-12** `CMAKE_EXPORT_COMPILE_COMMANDS` + `fx3/firmware/.clangd` | 2 | §5, D15 |
| **P2-13** Root `.editorconfig`, `.gitignore` and `.envrc` | 2 | §6, D16 |
| **P3-5** LSP/lint/sim tools in the dev shells + `docs-tech/editor-setup.md` | 3 | §7 |
| **P6-2** CLI compile/convert/program flow, using the committed `.cof`/`.cdf` | 6 | §2.1 |
| **P6-1** USB-Blaster udev rule in the NixOS module | 6 | §2.1 |
| **P6-6** `verilator --lint-only` as a `nix flake check` | 6 | §2.3 |

**Acceptance for the whole document:** a contributor with only `nix`, `git` and a text editor
of their choosing can build the GUI, build and flash the FX3 firmware, compile and program
the bitstream, and build the docs — with no GUI IDE launched at any point.
