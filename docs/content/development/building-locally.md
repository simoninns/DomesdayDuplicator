# Building Locally

How to build each component from a checkout, out of tree, without installing anything into
the source directories.

Everything here runs from the **repository root**. Each component keeps its own build
directory, so building one never disturbs another and `git status` stays clean.

```bash
git clone https://github.com/simoninns/DomesdayDuplicator
cd DomesdayDuplicator
```

There are no submodules — a plain clone gives you the whole project.

## Getting a toolchain

**Nix on Linux is the only supported development and build environment**, and nothing needs
installing to use it: each component has a development shell carrying exactly what it needs,
pinned by the repository's single `flake.lock`. If you develop on macOS or Windows, use a
Linux virtual machine.

| Shell | Carries |
| --- | --- |
| `nix develop .#ddd-gui` | Qt 6, libusb, libFLAC, GoogleTest, CMake, Ninja, clangd, clang-format and clang-tidy |
| `nix develop .#fx3` | `arm-none-eabi-gcc`, CMake, Ninja, libusb, GoogleTest |
| `nix develop .#fpga` | verible, verilator, iverilog, gtkwave — free tools, no Quartus |
| `nix develop .#fpga-quartus` | the above plus Quartus Prime Lite (`x86_64-linux` only, several GB) |
| `nix develop .#docs` | MkDocs Material and the site plugins |

These work from any directory in the tree; Nix walks up to find the flake at the root. A bare
`nix develop` gives the all-components shell rather than the one for the directory you happen
to be standing in.

## Why out of tree

Two of the three components have a specific reason beyond tidiness:

- **Quartus rewrites the project file it is given.** `quartus_sh` records
  `LAST_QUARTUS_VERSION` in the `.qsf` as it compiles, so building in `fpga/application` or
  `fpga/factory` dirties a tracked file every single time — and then scatters thirty-odd build products among the
  Verilog sources.
- **The FX3 build is a cross build.** Its CMake cache holds an ARM toolchain, and reusing a
  directory that once configured a host build produces confusing failures rather than clear
  ones.

The GUI has no such trap, but the same habit applies for consistency.

---

## The capture application

```bash
nix develop .#ddd-gui

cmake -B build/ddd-gui -S ddd-gui -G Ninja
cmake --build build/ddd-gui
```

Building it also runs its two quality gates: `clang-format` as a build target and
`clang-tidy` through `CXX_CLANG_TIDY`, so compiling is what runs them. Both tools change their
check sets between releases, and CI runs them from this shell — a clang-tidy of a different
version will disagree with CI in both directions, passing what CI fails and occasionally the
reverse, which is why the shell is pinned rather than assumed.

One consequence worth knowing: changing `.clang-tidy` does not invalidate object files, so
an incremental build re-analyses only what you edited. After a config change, build from a
clean tree before believing a green result.

`QT_QPA_PLATFORM=offscreen` is only needed where there is no display, such as over SSH; drop
it to run the application itself.

### As a Nix package

```bash
nix build .#ddd-gui
./result/bin/ddd-gui --version
```

This builds hermetically, runs the test suite as part of the build, and checks the installed
binary reports its commit. Note that it reads **git-tracked files only** — a new file that has
not been `git add`ed does not exist as far as the flake is concerned, which produces a
confusing "path that does not exist" error. CMake reads the working tree and has no such
restriction, which is why it is the better loop while you are editing.

---

## The FX3 firmware

A cross build for the ARM926EJ-S core in the FX3, so it needs a toolchain file. Pass it as an
**absolute** path — CMake resolves a relative one against the build directory, not the source
directory.

```bash
nix develop .#fx3

cmake -B build/fx3-firmware -S fx3/firmware -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/fx3/firmware/arm-none-eabi-toolchain.cmake"
cmake --build build/fx3-firmware
```

That produces three files in `build/fx3-firmware/`:

| File | What it is |
| --- | --- |
| `firmware.img` | The boot-loadable image — this is what gets programmed |
| `firmware.elf` | The linked ELF it was made from |
| `firmware.map` | The link map, for working out where the space went |

```bash
ctest --test-dir build/fx3-firmware --output-on-failure
```

The build needs `fx3-mkimage` to turn the ELF into the image. If it is not on `PATH` — it is
in the `.#fx3` shell — CMake compiles it from `fx3/mkimage` automatically with the host
compiler, so this works either way.

### The host tools

`fx3-mkimage` and `fx3-programmer` are ordinary host builds and need no toolchain file:

```bash
cmake -B build/fx3-mkimage -S fx3/mkimage -G Ninja
cmake --build build/fx3-mkimage
ctest --test-dir build/fx3-mkimage --output-on-failure

cmake -B build/fx3-programmer -S fx3/programmer -G Ninja
cmake --build build/fx3-programmer
ctest --test-dir build/fx3-programmer --output-on-failure
```

### Loading it onto a device

```bash
./build/fx3-programmer/fx3-programmer -l                                  # list devices
./build/fx3-programmer/fx3-programmer -d 0 -u build/fx3-firmware/firmware.img
```

Reaching the device needs udev rules — see
[Linux device access](hardware-programming/linux-device-access.md) — and the boot-mode
jumper has to be set correctly. [FX3 firmware](hardware-programming/fx3-firmware.md) covers
the jumper, RAM versus EEPROM programming and what each mode looks like on the host.

### As Nix packages

```bash
nix build .#fx3-firmware      # result/ holds firmware.img, .elf and .map
nix build .#fx3-programmer
```

---

## The FPGA gateware

Everything except producing a bitstream is free software and needs no Quartus.

### Lint, simulation and constraints

```bash
nix develop .#fpga

./fpga/tests/run-lint.sh      # T4: verilator --lint-only over the hand-written modules
./fpga/tests/run-sim.sh       # T3: the module testbenches, under Icarus Verilog
./fpga/tests/run-style.sh     # T4: formatting and style, via verible
./fpga/tests/run-sdc.sh       # T4: the timing constraints parse and cover every pin
./fpga/tests/run-version.sh   # T2: the commit-to-register version stamp generator
```

They all work out of tree — they build in a temporary directory and leave nothing behind.
They are the same checks CI runs, as `nix flake check`'s `fpga-lint`, `fpga-sim`,
`fpga-style`, `fpga-sdc` and `fpga-version`, so a clean run locally means a clean run there.

`run-sdc.sh` is worth knowing about, because the timing constraints are the one gateware
source file only Quartus consumes, and Quartus never runs in CI. It checks the two things
that can be checked for free: that the file is valid Tcl, and that it names every pin the
top level maps — a constraint covering fifteen of sixteen databus pins leaves the sixteenth
unanalysed, and nothing else in the tree would notice. Whether the numbers in it are right,
and whether the design meets them, still needs a Quartus run.

### Building the bitstream

This is the one that needs Quartus Prime Lite, which is unfree, `x86_64-linux` only, and a
multi-gigabyte download.

```bash
nix develop .#fpga-quartus -c ./fpga/build-local.sh
```

The script copies `fpga/src` to `fpga/build` and compiles there, for the reason given above —
`quartus_sh` would otherwise rewrite a tracked `.qsf` on every run. Pass a different
destination as the first argument if you want the build somewhere else entirely:

```bash
nix develop .#fpga-quartus -c ./fpga/build-local.sh /tmp/ddd-bitstream
```

It produces `DomesdayDuplicator.sof` (volatile, for JTAG) and `DomesdayDuplicator.jic` (for
the EPCS64 flash), plus a `bitstream-provenance.txt` recording the Quartus version, the source
commit and the digests that make the result verifiable.

Program the board from the build directory:

```bash
quartus_pgm DomesdayDuplicator_write_sof.cdf     # volatile, lost on power cycle
quartus_pgm DomesdayDuplicator_write_jic.cdf     # permanent, writes EPCS64 flash
```

The USB-Blaster needs its own udev rules; [FPGA bitstream](hardware-programming/fpga-bitstream.md)
covers those and the programming procedure in full.

For a release build rather than an iteration, use the derivation instead — it pins the
toolchain and builds in a sandbox:

```bash
nix build .#bitstream
```

The bitstream is deliberately **not** built by CI. Quartus is `redistributable = false`, so it
can never come from a binary cache and every run would fetch gigabytes from Altera; the
bitstream is built locally and attached to firmware releases by hand, with its provenance
record and digests standing in for a CI build.

---

## The documentation site

```bash
nix develop .#docs
mkdocs build -f docs/mkdocs.yml --strict
mkdocs serve -f docs/mkdocs.yml     # live preview on http://127.0.0.1:8000
```

`--strict` is what CI uses: it fails on a broken internal link or a page missing from the
navigation, so a clean local build means the deploy will not surprise you.

---

## Everything at once

```bash
nix flake check
```

Builds every component and runs every test suite — the GUI, the FX3 firmware and host tools,
the documentation site, and the gateware lint and simulation. It skips only the bitstream, for
the Quartus reasons above.

This is what CI runs on every push, so it is also the answer to "will this pass?".

## Version stamping

Each component stamps the commit it was built from into its output: the capture application's
`--version` and About dialog, the FX3 firmware's USB product descriptor, and the bitstream's
provenance record.

A commit and nothing else — no release version anywhere, so all three parts of a Duplicator
report the same kind of thing and a bug report can quote three hashes side by side.

In a normal checkout this comes from `git` automatically, and a modified working tree is
marked — `af2511a5-dirty` — because a bare commit hash would name source that is not what
was built. Building from a tarball or inside a sandbox has no `git` to ask, so the value is
passed in:

```bash
cmake -B build/ddd-gui -S ddd-gui -DDDD_COMMIT=af2511a5
cmake -B build/fx3-firmware -S fx3/firmware -DFIRMWARE_VERSION=af2511a5 ...
```

Left unset with no `git` available, both fall back to `unknown` — which the release workflow
rejects, since a released binary that cannot be traced to a commit defeats the point of
building it in CI at all.
