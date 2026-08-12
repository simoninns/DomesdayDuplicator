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
| `DomesdayDuplicator.SDC` | Timing constraints |
| `dataGenerator.v` | ADC sampling and the built-in test-data generator |
| `buffer.v` | Sample buffering between the ADC and FX3 clock domains |
| `fx3StateMachine.v` | GPIF II handshake with the FX3 |
| `statusLED.v` | Front-panel status LED behaviour |
| `IPfifo.v`, `IPpllGenerator.v` | Instantiations of the Altera `dcfifo` and `altpll` primitives |

## The generated IP is source, not wizard output

`IPfifo.v` and `IPpllGenerator.v` are committed as plain Verilog with explicit `defparam`
values. They instantiate Altera primitives, but they are ordinary source files — nothing in
the build regenerates them, so **MegaWizard is not needed to build the project**, and the
GPIF II Designer equivalent for the FX3 side is likewise a design-time tool only.

Change FIFO depth or PLL multiply/divide by editing the `defparam` block directly. `dcfifo`
and `altpll` are ordinary instantiable megafunctions; the wizard only ever wrote these files
out.

`IPpllGenerator.ppf` is a wizard *parameter* file, listed in the project as a `MISC_FILE`. It
plays no part in compilation.

## Working on the gateware without Quartus

Everything except producing a bitstream is free software and cross-platform:

```bash
nix develop .#fpga        # verible, verilator, iverilog, gtkwave — no Quartus
```

```bash
./tests/run-lint.sh       # T4: verilator --lint-only over the five hand-written modules
./tests/run-sim.sh        # T3: the three module testbenches, under Icarus Verilog
```

Both run unchanged as `nix flake check` checks (`fpga-lint`, `fpga-sim`), and they are the
only automated coverage the gateware gets in CI — bitstream builds cannot run there.

`verible-verilog-ls` is a language server, so any editor with an LSP client gets completion,
navigation and diagnostics in the Verilog sources.

**Lint runs with `-Wall`, and every warning it reports is either a failure or a waiver with a
written reason** in [verilator-waivers.vlt](verilator-waivers.vlt). The waived items are real
properties of source that has been in the field since 2018 — a blocking assignment in a
clocked block, two incomplete `case` statements, an implicit width promotion — and each is
pinned by a testbench rather than merely asserted to be harmless. They are candidates for a
gateware cleanup, but that belongs in a change whose gate is a capture-integrity run on
hardware.

**What is not covered:** `buffer.v`, and therefore the design as a whole. It is two `dcfifo`
instances and the logic that switches between them, and simulating a `dcfifo` needs Altera's
`altera_mf` library, which has no free model. `IPfifo.v` and `IPpllGenerator.v` are not even
linted for the same reason; the black-box declarations beside them
(`IPfifo_bb.v`, `IPpllGenerator_bb.v`) are what let the modules that instantiate the IP
elaborate. The buffering path is covered on hardware instead, by the capture-integrity
procedure in [TESTING.md](../TESTING.md) §5.

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
for years, so the tag was set and never acted on — see defect D23 in
[docs-tech/implementation-plan.md](../docs-tech/implementation-plan.md).

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

Quartus Prime Lite is unfree, `x86_64-linux` only, and marked `redistributable = false` in
nixpkgs — so it can never be served from `cache.nixos.org`, and every cold CI run would fetch
gigabytes from Altera into a runner with roughly 14 GB of disk. The decision, and the three
ways it could reach CI later, are in
[docs-tech/implementation-plan.md](../docs-tech/implementation-plan.md).

The consequence for releases: **the bitstream is built locally and attached by hand**, with
its provenance record. Publishing the canonical digest is what makes it independently
verifiable anyway — anyone with the same pinned Quartus can rebuild and compare, with no CI
involvement at all.

## Documentation

For detailed documentation, please see the
[main project documentation](https://simoninns.github.io/domesdayduplicator).
