# Domesday Duplicator FPGA Gateware

Verilog for the Terasic DE0-NANO board (Altera/Intel Cyclone IV, `EP4CE22F17C6`). It samples
the ADC, buffers the data and feeds it to the FX3 USB 3.0 controller over the GPIF II bus.

## Two images, and why

The gateware is **two** bitstreams that live in one flash, and which one a unit is running
decides what it can do:

| | [factory/](factory/) | [application/](application/) |
| --- | --- | --- |
| What it is | a boot loader, and a recovery state | the capture gateware |
| How it reaches a unit | JTAG, once, when the unit is provisioned | a device update over the USB cable |
| Captures | **no** | yes |
| Changes | essentially never after its first release | freely |
| Reports `IMAGE_ROLE` | `0x00` | `0x01` |

The factory image exists so that a gateware update has something to fall back to. There is
no way to avoid writing the flash — the FPGA has no other configuration source, and the
host cannot reach its configuration pins at all — so instead a unit always has a resident
image that can identify itself, give the FX3 access to the flash, and decide at power-on
whether the application image is intact enough to enter.

It is deliberately not a copy of the capture gateware. An image containing the capture
logic would have to change whenever the capture logic changed, which is the opposite of
resident. The whole model, the flash layout and the boot decision are on the
[EPCS layout and boot flow](../docs/content/development/epcs-layout-and-boot-flow.md)
documentation page; **[factory/README.md](factory/README.md) carries the freeze policy and
must be read before anything in that directory is edited.**

## Contents

| Path | Contents |
| --- | --- |
| [application/](application/) | The capture gateware: its Quartus project and the modules only it needs |
| [factory/](factory/) | The resident boot loader. **Read its README before editing** |
| [common/](common/) | What both images contain, and the models the free tools simulate the device with |
| [provisioning/](provisioning/) | The conversion that puts both images in one flash file |
| [tests/](tests/) | Testbenches and the lint, style, simulation and constraint runners |
| [configs/](configs/) | udev rules for the USB-Blaster JTAG cable |
| `package.nix` | The bitstream build. Its own CI workflow — see [How the bitstream is built](#how-the-bitstream-is-built) |
| `checks.nix` | Lint and simulation checks, which are in the per-commit tier |
| `build-local.sh` | Out-of-tree local build of both images |
| `make-boot-block.py` | The boot block that tells the factory image where the application image is |
| `make-halfband-coefficients.py` | The decimation filter's coefficients, and `--response` to measure them. The table is committed into `halfBandDecimator.v`, because gateware cannot open a file; `tests/test_halfband_coefficients.py` regenerates it and fails if the two have parted company |
| `bitstream-provenance.py` | Provenance record and digests for a built bitstream |
| `verilator-waivers.vlt` | Lint waivers, each with the reason it is waived |

Inside `application/`:

| File | Role |
| --- | --- |
| `DomesdayDuplicator.v` | Top level: pin mapping and module wiring |
| `DomesdayDuplicator.qsf` | Quartus settings — device, pin assignments, source list |
| `DomesdayDuplicator.qpf` | Quartus project file |
| `DomesdayDuplicator.SDC` | Timing constraints. Checked by `tests/run-sdc.sh`; the I/O delay values in its header are pessimistic placeholders pending the datasheets |
| `DomesdayDuplicator.cof` | Conversion to the raw image bytes a device update writes. Its `rpd_little_endian` setting decides the bit orientation of those bytes and is load-bearing — read the comment beside it before changing anything here |
| `halfBandDecimator.v` | The 10 MHz anti-alias filter and 2:1 decimation, for tape capture at 20 Msps. In front of `dataGenerator.v`, so the sequence counter and the test ramp are attached to the samples that survive. The design, the measured response and the phase are on [The decimation filter](../docs/content/development/fpga-decimation-filter.md) |
| `dataGenerator.v` | ADC sampling and the built-in test-data generator |
| `buffer.v` | Sample buffering between the sampling side and the FX3 |
| `fifo.v` | The single-clock FIFO `buffer.v` is built from |
| `bufferMonitor.v` | What the buffer did, reported at registers `0x40`–`0x56`. An observer: every port but the sampling pulse is an output, and that pulse reaches nothing outside this module |
| `fx3StateMachine.v` | GPIF II handshake with the FX3 |

Inside `common/`:

| File | Role |
| --- | --- |
| `spiRegisters.v` | The register bank the FX3 reads and writes over SPI, register map version 2. The capture buffer window at `0x40`–`0x56` and the decimation register at `0x12` are both parameterised off in the factory image, which has neither a buffer to report on nor a sample stream to decimate |
| `flashBridge.v` | The explicitly-unlocked pass-through to the EPCS, registers `0x20`–`0x22` |
| `asmiBlock.v` | Access to the configuration flash pins, which are not user I/O |
| `remoteUpdate.v` | Reconfiguration and the configuration watchdog, register `0x23`, and the block's read-back of its own setup at `0x30`–`0x37` |
| `version.vh` | Generated build stamp the register bank reports; regenerated into the build directory by `generate-version.sh` |
| `IPpllGenerator.v` | Instantiation of the Altera `altpll` primitive |
| `sim/` | Behavioural models of the two device primitives and of the EPCS64 itself, so the free tools can simulate what the real parts do. Test fixtures; never compiled into a bitstream |

Inside `factory/`: the top level, the boot decision (`bootLoader.v`), the checksum it
validates the boot block with (`crc32.v`), and its own Quartus project. See
[factory/README.md](factory/README.md).

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

Every runner takes the whole of `fpga/` rather than one directory, because the two images
share half their source and a check that saw only one of them would miss the half that can
break both.

```bash
./tests/run-lint.sh       # T4: verilator --lint-only over the hand-written modules
./tests/run-style.sh      # T4: formatting and style, via verible
./tests/run-sim.sh        # T3: the module testbenches, under Icarus Verilog
./tests/run-sdc.sh        # T4: both images' constraints parse and cover every pin
./tests/run-version.sh    # T2: the commit-to-register version stamp generator
./tests/test_boot_block.py  # T1: the boot block encoder, offset by offset
./tests/test_halfband_coefficients.py  # T1: the decimation filter's coefficients
./tests/run-format.sh     # not a check — the formatter, run it to fix run-style.sh
```

All of them run unchanged as `nix flake check` checks (`fpga-lint`, `fpga-style`,
`fpga-sim`, `fpga-sdc`, `fpga-version`, `fpga-provenance`, `fpga-boot-block`,
`fpga-halfband-coefficients`), and they are
the automated coverage the gateware gets on every commit; the bitstream itself is compiled by
a separate workflow ([How the bitstream is built](#how-the-bitstream-is-built)).

The simulation that matters most is `tb_bootLoader`, which builds the factory image's boot
path exactly as its top level wires it — boot logic, flash bridge, active serial block,
reconfiguration control — against a model of the EPCS64, and drives the four cases the boot
flow documents. It is the only logic in this repository that a field update can never
repair.

## Style

The style guide is the [lowRISC Verilog Coding Style Guide](https://github.com/lowRISC/style-guides/blob/master/VerilogCodingStyle.md),
with four recorded deviations — four-space indent, Verilog-2001 rather than SystemVerilog,
existing module and file names kept, and no `_i`/`_o`/`_d`/`_q` suffixes. The convention
itself is summarised in [AGENTS.md](../AGENTS.md) §5.3.

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
./build-local.sh               # copies the sources to build/, compiles both images,
                               # converts, records provenance
```

or hermetically, which is the route for anything that gets released:

```bash
nix build .#bitstream
```

Either produces, in a directory laid out like the sources:

| Output | What it is |
| --- | --- |
| `application/DomesdayDuplicator.sof` | Volatile JTAG configuration of the capture gateware |
| `application/DomesdayDuplicator_auto.rpd` | The application image as bytes in the flash — what a device update writes, verbatim and bit for bit |
| `factory/DomesdayDuplicatorFactory.sof` | Volatile JTAG configuration of the factory image |
| `provisioning/DomesdayDuplicatorProvisioning.jic` | **Both images**, at their EPCS64 addresses. This is what provisions a board |
| `provisioning/DomesdayDuplicatorProvisioning.svf` | The same content as JTAG vectors, for provisioning a board without Quartus |
| `provisioning/DomesdayDuplicatorProvisioning.map` | Where the converter actually put them — the check on the layout |
| `provisioning/boot-block.bin` | The twenty-four bytes that tell the factory image about that application image |
| `reports/` | The compilation and timing reports for both images |
| `bitstream-provenance.txt` | Digests for everything above |

**There is deliberately no `.jic` containing the application image alone.** Programming one
would write the capture gateware over the factory image at address zero, leaving a unit with
nothing to fall back to. The converter cannot emit the raw image bytes without also
producing such a file, so both build routes delete it as soon as the `.rpd` is out.

**Do not run `quartus_sh` in the source directories.** It rewrites the tracked `.qsf` in
place and scatters about thirty build products beside the sources. Both routes above copy to
a build directory first, which is the only reason this is not a recurring annoyance.

The underlying commands, if you would rather drive them yourself — the GUI is never required
for any of them:

```bash
cd factory     && quartus_sh --flow compile DomesdayDuplicatorFactory
cd application && quartus_sh --flow compile DomesdayDuplicator
cd provisioning && quartus_cpf -c DomesdayDuplicatorProvisioning.cof   # both .sof -> one .jic
cd provisioning && quartus_cpf -c -q 4.5MHz -g 3.3 -n p \
    DomesdayDuplicatorProvisioning_write_jic.cdf \
    DomesdayDuplicatorProvisioning.svf                                # the same, as vectors
```

The declared frequency in that second conversion is not decoration: the converter turns
every wait in the sequence into a count of TCK cycles at that rate, so the same content
emitted at 6 MHz carries a third more cycles and exactly the same hundred-second erase.

### If Quartus is installed by hand

The Nix package is convenient, not required. Install Quartus Prime Lite from Altera, put its
`bin` directory on `PATH`, and `build-local.sh` works unchanged — it checks for `quartus_sh`
and `quartus_cpf` and says what to do if they are missing. This is the fallback if Altera
ever withdraws the 25.1 installer and the nixpkgs fetch hash stops resolving.

## Programming the board

The configuration description files are committed and name their own inputs, so from a
build directory:

```bash
cd provisioning && quartus_pgm DomesdayDuplicatorProvisioning_write_jic.cdf   # both images, permanent
cd application  && quartus_pgm DomesdayDuplicator_write_sof.cdf               # volatile, lost on power cycle
cd factory      && quartus_pgm DomesdayDuplicatorFactory_write_sof.cdf        # volatile
```

The permanent one can also be done without Quartus at all, over the same on-board cable:

```bash
cd provisioning && ddd-jtag DomesdayDuplicatorProvisioning.svf                # both images, permanent
```

`ddd-jtag` is built with the capture application and drives the USB-Blaster over libusb —
see [USB-Blaster and SVF programming](../docs/content/development/usb-blaster-and-svf.md).
It writes the same flash content by the same sequence, so everything below applies to it
unchanged, the power cycle most of all. Quartus's `jtagd` holds the cable open whenever it
is running.

**Provisioning is the last time a cable is needed.** Once a unit carries both images, the
application half is updated over the USB cable it already has — see the
[device update mechanism](../docs/content/development/device-update-mechanism.md).

**Power cycle after any of the three**, and after the `.jic` in particular: `quartus_pgm`
reaches the flash by loading a serial flash loader into the FPGA and leaves it running, so
until the board is power cycled the FPGA is running Altera's loader rather than either of
these images. It answers nothing on the register link, and an update attempted in that
state is refused for exactly that reason.

One thing the provisioning file does *not* carry is the boot block: it is written to
`0x100000` by the update path, on a device, and until it is there a freshly provisioned
unit comes up in the factory image and reports itself in recovery. The erase is
page-selective, so *re*-provisioning a working unit leaves its old boot block in place, and
a unit whose new application image still matches that block's CRC boots straight into it.
`boot-block.bin` beside
the `.jic` is exactly the twenty-four bytes for the application image built alongside it.
Quartus' converter has no way to place arbitrary data in a `.jic` for this device family —
tried, and it refuses the conversion — so the two artefacts stay separate.

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

**Measured, not assumed.** It was once asserted that Quartus
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
`.svf` they differ by one line: the converter's header names the input file and the time
that file was last written, and nothing else varies between two conversions of one `.jic`.
For the `.sof` they differ, and the canonical one is what a rebuild should be compared against. The
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

## How the bitstream is built

**Decided 2026-08-14, superseding "leave the bitstream out of CI for the time being": Quartus
runs in CI, in workflows of its own.** The facts that made the earlier decision have not
changed — `quartus-prime-lite` is `x86_64-linux` only, unfree, and marked
**`redistributable = false`** in nixpkgs, so it can never be served from `cache.nixos.org` and
every cold run must fetch it from Altera. What changed is where that cost is paid.

| Tier | Quartus? | What builds the gateware |
| --- | --- | --- |
| `nix flake check`, and the per-commit `build.yml` | **no** | lint, style, simulation, constraints, version and boot-block checks — the free tools only |
| `bitstream.yml` — gateware changes, manual dispatch, and called from a release tag | yes | `nix build .#bitstream`, both images, with digests and provenance |
| `release-firmware.yml` — `fw-v*` tags | yes, via the above | every released artefact, built from the tagged commit |
| `reproducibility-audit.yml` — weekly | yes, via the above | rebuilds the latest release and compares digests |

The per-commit tier is the one every contributor runs, and it stays free of unfree downloads:
a one-line documentation fix must never require a multi-gigabyte toolchain to validate. The
dedicated workflows pay for Quartus only when the gateware or a release actually needs it.

What makes the CI build viable, all of it encoded in the workflow rather than in folklore:

- the closure is **cached**, privately to this repository, keyed on `flake.lock`; a cold miss
  falls back to the hash-pinned fetch from Altera's CDN, which is slow but never wrong;
- the runner's disk is prepared first — the stock image's preinstalled toolchains have to go
  to fit the closure plus two compiles into ~14 GB;
- `supportedDevices = [ "Cyclone IV" ]` in `flake.nix` keeps the download to one device family
  instead of six.

**Licence position.** Prime Lite needs no licence file, and installing it in CI from Altera's
own installer is ordinary use. Caching the closure so that *our own* CI need not re-download
it is also ordinary use; publishing that cache would be redistribution, which is why the cache
is private and why third-party Docker images of Quartus — the MiSTer community's route — are
rejected here.

The consequence for releases: **every released bitstream is CI-built from the tagged commit**,
attached automatically with its provenance record. Nothing is built on a maintainer's machine
and nothing is attached by hand. The full model, including key custody and the reproducibility
audit, is on the *Release pipeline* page of the documentation site.

Local builds remain first class for development and bench work — `./build-local.sh` and
`nix build .#bitstream` are unchanged, and are what you should use while iterating. It is
only the *release* artefacts that are defined as CI-built.

If Quartus-in-CI ever proves unsustainable (cache eviction economics, runner disk limits,
Intel CDN rot), the fallback is preserved in git history: maintainer-built bitstreams
committed to a tracked `fpga/prebuilt/` directory behind a digest-and-source-tree-hash gate,
auditable because of the same byte-identical reproducibility measured above.

## Documentation

For detailed documentation, please see the
[main project documentation](https://simoninns.github.io/DomesdayDuplicator).
