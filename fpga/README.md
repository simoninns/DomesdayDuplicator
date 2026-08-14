# Domesday Duplicator FPGA Gateware

Verilog for the Terasic DE0-NANO board (Altera/Intel Cyclone IV, `EP4CE22F17C6`). It samples
the ADC, buffers the data and feeds it to the FX3 USB 3.0 controller over the GPIF II bus.

## Contents

| Path | Contents |
| --- | --- |
| [src/](src/) | Quartus project and all Verilog sources |
| [tests/](tests/) | Testbenches and the lint and simulation runners |
| [configs/](configs/) | udev rules for the USB-Blaster JTAG cable |
| `package.nix` | The bitstream build. Not in CI — see [Reproducibility](#reproducibility) |
| `checks.nix` | Lint and simulation checks, which *are* in CI |
| `build-local.sh` | Out-of-tree local build |
| `bitstream-provenance.py` | Provenance record and digests for a built bitstream |
| `verilator-waivers.vlt` | Lint waivers, each with the reason it is waived |

Inside `src/`:

| File | Role |
| --- | --- |
| `DomesdayDuplicator.v` | Top level: pin mapping and module wiring |
| `DomesdayDuplicator.qsf` | Quartus settings — device, pin assignments, source list |
| `DomesdayDuplicator.qpf` | Quartus project file |
| `DomesdayDuplicator.SDC` | Timing constraints. Checked by `tests/run-sdc.sh`; the I/O delay values in its header are pessimistic placeholders pending the datasheets |
| `dataGenerator.v` | ADC sampling and the built-in test-data generator |
| `buffer.v` | Sample buffering between the sampling side and the FX3 |
| `fifo.v` | The single-clock FIFO `buffer.v` is built from |
| `fx3StateMachine.v` | GPIF II handshake with the FX3 |
| `spiRegisters.v` | The register bank the FX3 reads and writes over SPI |
| `version.vh` | Generated build stamp the register bank reports; regenerated into the build directory by `generate-version.sh` |
| `IPpllGenerator.v` | Instantiation of the Altera `altpll` primitive |

## The generated IP is source, not wizard output

`IPpllGenerator.v` is committed as plain Verilog with explicit `defparam` values. It
instantiates an Altera primitive, but it is an ordinary source file — nothing in the build
regenerates it, so **MegaWizard is not needed to build the project**, and the GPIF II
Designer equivalent for the FX3 side is likewise a design-time tool only.

Change the PLL multiply/divide by editing the `defparam` block directly. `altpll` is an
ordinary instantiable megafunction; the wizard only ever wrote the file out. The 80 MHz
system clock was retuned that way, without the wizard.

There used to be a second one, `IPfifo.v`, wrapping `dcfifo`, from when the design had two
clock domains and needed a FIFO that spanned them. `fifo.v` replaced it when the gateware
moved to a single 80 MHz clock, which is also what made the buffering path simulable.

`IPpllGenerator.ppf` is a wizard *parameter* file, listed in the project as a `MISC_FILE`. It
plays no part in compilation.

## Working on the gateware without Quartus

Everything except producing a bitstream is free software and cross-platform:

```bash
nix develop .#fpga        # verible, verilator, iverilog, gtkwave — no Quartus
```

```bash
./tests/run-lint.sh       # T4: verilator --lint-only over the six hand-written modules
./tests/run-style.sh      # T4: formatting and style, via verible
./tests/run-sim.sh        # T3: the five module testbenches, under Icarus Verilog
./tests/run-sdc.sh        # T4: the timing constraints parse and cover every pin
./tests/run-version.sh    # T2: the commit-to-register version stamp generator
./tests/run-format.sh     # not a check — the formatter, run it to fix run-style.sh
```

All five checks run unchanged as `nix flake check` checks (`fpga-lint`, `fpga-style`,
`fpga-sim`, `fpga-sdc`, `fpga-version`), and they are the only automated coverage the
gateware gets in CI — bitstream builds cannot run there.

## Style

The style guide is the [lowRISC Verilog Coding Style Guide](https://github.com/lowRISC/style-guides/blob/master/VerilogCodingStyle.md),
with four recorded deviations — four-space indent, Verilog-2001 rather than SystemVerilog,
existing module and file names kept, and no `_i`/`_o`/`_d`/`_q` suffixes. The reasoning for
each is in [docs-tech/fpga-verilog-style-plan.md](../docs-tech/fpga-verilog-style-plan.md);
the convention itself is summarised in [AGENTS.md](../AGENTS.md) §5.3.

**Do not hand-format Verilog.** `./tests/run-format.sh` is the formatter, and three files
beside the sources are its only configuration:

| File | What it is |
| --- | --- |
| [.verible-format](.verible-format) | Formatter settings. Four-space indent, 100 columns, every alignment mode set explicitly rather than left at Verible's `infer` |
| [.rules.verible_lint](.rules.verible_lint) | Style rules — Verible's defaults, plus `signal-name-style` and `explicit-begin`, minus the SystemVerilog-only rules. Every departure carries its reason |
| [verible-waivers](verible-waivers) | Narrow per-case exceptions. One entry: the four DE0-Nano port names |

`verible-verilog-ls` is a language server that finds `.rules.verible_lint` by searching
upward, so any editor with an LSP client shows the same diagnostics the CI check enforces,
with no editor configuration at all.

One rule the tooling does *not* enforce: every `parameter` and `localparam` carries an
explicit width or `integer`. Verible's `explicit-parameter-storage-type` wants a SystemVerilog
storage type and no Verilog-2001 form satisfies it, so it is disabled with the evidence in
`.rules.verible_lint` and this one is held up by review instead.

**Lint runs with `-Wall`, and every warning it reports is either a failure or a waiver with a
written reason** in [verilator-waivers.vlt](verilator-waivers.vlt). The waived items are real
properties of source that has been in the field since 2018 — a blocking assignment in a
clocked block, two incomplete `case` statements, an implicit width promotion — and each is
pinned by a testbench rather than merely asserted to be harmless. They are candidates for a
gateware cleanup, but that belongs in a change whose gate is a capture-integrity run on
hardware.

**What is not covered:** the top level, because it instantiates `altpll` through
`IPpllGenerator` and simulating that needs Altera's `altera_mf` library, which has no free
model. `IPpllGenerator.v` is not even linted for the same reason; the black-box declaration
beside it (`IPpllGenerator_bb.v`) is what lets the top level elaborate.

`buffer.v` used to be exempt for the same reason — it was two `dcfifo` instances — and
replacing that IP with `fifo.v` is what brought the capture path into the testbench suite.
The pin-level behaviour of the whole design is still covered on hardware, by the
capture-integrity procedure in [TESTING.md](../TESTING.md) §5.

## Building a bitstream

Quartus Prime Lite is required — the design targets a Cyclone IV, which Lite supports, and
Lite needs no licence file.

**Canonical version: 25.1 (`25.1std.0 Build 1129`).** That is what
`nix develop .#fpga-quartus` provides and what the reproducibility measurements below were
taken with. The project files were last *written* by Quartus 16.0.2/18.0.0; 25.1 compiles
them unchanged, with no upgrade prompt and no source edits, and updates
`LAST_QUARTUS_VERSION` in the `.qsf` as a side effect of every compile.

```bash
nix develop .#fpga-quartus     # x86_64-linux only; multi-GB first download
./build-local.sh               # copies src/ to build/, compiles, converts, records provenance
```

or hermetically, which is the route for anything that gets released:

```bash
nix build .#bitstream
```

Either produces `DomesdayDuplicator.sof` (volatile JTAG configuration),
`DomesdayDuplicator.jic` (permanent EPCS64 configuration), the compilation reports, and
`bitstream-provenance.txt`.

**Do not run `quartus_sh` in `src/`.** It rewrites the tracked `.qsf` in place and scatters
about thirty build products beside the sources. Both routes above copy to a build directory
first, which is the only reason this is not a recurring annoyance.

The underlying commands, if you would rather drive them yourself — the GUI is never required
for any of them:

```bash
quartus_sh --flow compile DomesdayDuplicator     # or quartus_map/_fit/_asm/_sta separately
quartus_cpf -c DomesdayDuplicator.cof            # .sof -> .jic; the .cof is committed
```

### If Quartus is installed by hand

The Nix package is convenient, not required. Install Quartus Prime Lite from Altera, put its
`bin` directory on `PATH`, and `build-local.sh` works unchanged — it checks for `quartus_sh`
and `quartus_cpf` and says what to do if they are missing. This is the fallback if Altera
ever withdraws the 25.1 installer and the nixpkgs fetch hash stops resolving.

## Programming the board

Both configuration description files are committed and name their own inputs, so from a
build directory:

```bash
quartus_pgm DomesdayDuplicator_write_sof.cdf   # volatile, lost on power cycle
quartus_pgm DomesdayDuplicator_write_jic.cdf   # permanent, into the EPCS64 flash
```

`quartus_pgm` needs `jtagd` (it starts one) and a udev rule for the USB-Blaster — the DE0-Nano
has one on board. **nixpkgs' Quartus package ships no udev rules**, so this repository
supplies them: [configs/70-altera-usb-blaster.rules](configs/70-altera-usb-blaster.rules),
installed on NixOS by

```nix
imports = [ domesdayduplicator.nixosModules.udev ];
hardware.domesdayDuplicator.enable = true;   # FX3 and USB-Blaster together
```

On other distributions, copy the file to `/etc/udev/rules.d/` and
`udevadm control --reload`. Keep the `70-` prefix: it sorts before `73-seat-late.rules`,
which is what consumes the `uaccess` tag. This project shipped a rule that sorted *after* it
for years, so the tag was set and never acted on.

Without the rule, `quartus_pgm` and `jtagconfig` report "No JTAG hardware available" to
everyone but root.

## Reproducibility

**Measured, not assumed.** Earlier drafts of the reorganisation plan asserted that Quartus
output varies between runs because "place-and-route seeds and timestamps vary". The seed part
was wrong: the Fitter seed is a fixed project setting, and fitting is deterministic for a
given seed and toolchain. What that leaves is a question about *files*, not about *fitting*,
and it was settled by compiling the same commit four times — twice locally, once with the
`.qsf` settings below pinned, and once inside a Nix build sandbox:

| Artefact | Result |
| --- | --- |
| `DomesdayDuplicator.jic` | **Byte-identical** across all four builds |
| `DomesdayDuplicator.sof` | Differs in 32–34 bytes of 704,015, all of it header metadata |

The `.sof` differences are, in full: a 10-byte per-run design hash, the same hash again as
ASCII inside the `<sld_project_info>` block, two copies of a 32-bit Unix compile timestamp,
and the two-byte file checksum that covers them. No byte of configuration data differs — and
the identical `.jic`, which `quartus_cpf` derives from the `.sof`'s configuration payload,
is the independent proof of that.

So there are two digests, and `bitstream-provenance.txt` carries both for each artefact:

| Digest | Over | Answers |
| --- | --- | --- |
| **Release** | The file as shipped | "Did I download the file that was released, intact?" |
| **Canonical** | The configuration content, with the four variable fields masked | "Does a rebuild of this commit agree with it?" |

For the `.jic` the two are the same number, because it carries no per-run fields. For the
`.sof` they differ, and the canonical one is what a rebuild should be compared against. The
masking is fail-loud — if a future Quartus moves a field, the tool refuses to produce a
digest rather than producing one that silently matches nothing — and the byte offsets are
fixed by [tests/test_provenance.py](tests/test_provenance.py), which runs as the
`fpga-provenance` check.

To verify a released bitstream:

```bash
nix build .#bitstream                       # or ./build-local.sh, same Quartus version
./bitstream-provenance.py --build-dir result
# compare the canonical lines against the released bitstream-provenance.txt
```

### What reproducibility does *not* extend to

Identical results require the same Quartus version, the same 32/64-bit build and the same CPU
architecture — floating-point differences across architectures can perturb a fit. That is why
`bitstream-provenance.txt` records all three. Changing any of them gives a different but
equivalent-quality fit, not a wrong one.

The `.qsf` pins the two settings this depends on:

```tcl
set_global_assignment -name SEED 1
set_global_assignment -name NUM_PARALLEL_PROCESSORS 4
```

Both are Quartus' own defaults. They are written out so the guarantee rests on the project
file rather than on a default a future version could change — and the pinning was verified to
be inert: adding them produced a byte-identical `.jic` on a 16-core machine, which also
demonstrates that the processor count does not affect the fit.

`ROUTER_TIMING_OPTIMIZATION_LEVEL` was considered and left alone: it is only worth pinning if
routing varies between runs, and it does not.

## Why this is not built by CI

**Decided 2026-08-12: leave the bitstream out of CI for the time being.** The blocker is not
technical difficulty, it is cost and a licence judgement:

- `quartus-prime-lite` is `x86_64-linux` only, unfree, and marked **`redistributable = false`**
  in nixpkgs — so it can never be served from `cache.nixos.org`. Every cold CI run must fetch
  it afresh.
- The fetch itself is unattended-friendly: a plain `fetchurl` from
  `downloads.intel.com/akdlm/software/acdsinst/…`, with no login and no click-through. So CI
  *can* do it.
- Even restricted to `supportedDevices = [ "Cyclone IV" ]`, the download is GB-scale and the
  unpacked store path larger again — tight against a GitHub-hosted runner's ~14 GB of disk.

The three ways it could reach CI, when the time comes:

| Option | Speed | Cost | Licence |
| --- | --- | --- | --- |
| **Self-hosted runner** with a warm Nix store | Minutes | A machine to run and maintain | Clean — nothing is redistributed |
| GitHub-hosted, fetch from Altera each run | 20–40 min | None | Clean — fetched from source |
| GitHub-hosted + private binary cache | 5–10 min | Cachix/S3 and credentials | **Judgement call** — `redistributable = false` is precisely about not redistributing these binaries |

**Intended shape when adopted:** the GUI and FX3 build per commit; the bitstream on `fw-v*`
tags and manual dispatch only, so a firmware release still gets a bitstream built from the
release commit without paying for Quartus on every push. The two-stream release split makes
this cheaper than it would be under a single tag — Quartus would run only on firmware
releases, not on every GUI release.

The consequence for releases today: **the bitstream is built locally and attached by hand**,
with its provenance record. Publishing the canonical digest is what makes it independently
verifiable anyway — anyone with the same pinned Quartus can rebuild and compare, with no CI
involvement at all.

`verilator --lint-only` ([tests/run-lint.sh](tests/run-lint.sh)) *does* run in CI, so the
gateware is not entirely uncovered there.

## Documentation

For detailed documentation, please see the
[main project documentation](https://simoninns.github.io/domesdayduplicator).
