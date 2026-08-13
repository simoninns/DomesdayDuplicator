# TESTING.md

How the Domesday Duplicator is tested, what that covers today, and what it does not.

This document is deliberately honest about scope. Before Phase 3 of the repository
reorganisation there were **no automated tests at all**. There are now 78 across four
components, plus three gateware testbenches, a lint pass over five Verilog modules, and a
static check on the documentation site. That is a start, not a suite, and this document says
so where it applies rather than describing an aspiration as though it were a fact.

---

## 1. Why testing here is unusual

Most projects test one program on one machine. This one spans a PCB, an FPGA, a bare-metal
USB controller and a desktop application, connected by a wire protocol none of them can
verify alone. Three consequences shape everything below.

**A green build proves very little.** The firmware compiles for a processor the build machine
does not have. The gateware compiles for silicon that is not present. Neither has ever been
executed at the point CI goes green.

**The failure mode that matters is silent.** A capture device that crashes is a nuisance. A
capture device that *drops one sample in ten thousand* produces files that look fine, decode
fine, and are subtly wrong forever. Nothing in a unit test catches that.

**Some things can only be tested on hardware, and that is not a gap to be closed.** Mocking
libusb, DMA and GPIF to the fidelity needed to prove a capture path would produce a model
elaborate enough to have its own bugs. The honest answer is a documented manual procedure,
which is §5.

## 2. Test tiers

Every automated test carries exactly one tier as a CTest label.

| Tier | Label | Meaning | In CI |
| --- | --- | --- | --- |
| T1 | `unit` | Host-native, no I/O, no hardware, mocked dependencies. Milliseconds | Yes |
| T2 | `golden` | Host-native, output compared against committed reference data | Yes |
| T3 | `sim` | Gateware simulation (Verilator / Icarus Verilog) | Yes |
| T4 | `static` | Lint, format, ERC/DRC, link checks, licence-header checks | Yes |
| T5 | `hil` | Hardware-in-the-loop. Needs a real Domesday Duplicator, usually a player too | **No** — manual, gated on release |

T5 never runs in CI and never runs unattended. Several of its steps write to non-volatile
memory on the device.

## 3. Running the tests

**From the repository root** — `nix flake check` itself works from any subdirectory, since
Nix walks up to the single root `flake.nix`, but the `cmake` paths below are relative to the
root:

```bash
nix flake check                      # everything, on a clean machine
```

Or per component, from a configured build tree:

```bash
cmake -B gui/build -S gui
cmake --build gui/build
ctest --test-dir gui/build                    # all tests
ctest --test-dir gui/build -L unit            # one tier
ctest --test-dir gui/build -LE hil            # everything except hardware
ctest --test-dir gui/build --output-on-failure
```

Tests are built by default. Pass `-DBUILD_TESTING=OFF` to skip them — the Nix packages do
this when `doCheck` is false.

The gateware has no CMake and so no `ctest`. Its checks are scripts, run either through the
flake or directly:

```bash
nix develop .#fpga -c ./fpga/tests/run-lint.sh     # T4
nix develop .#fpga -c ./fpga/tests/run-sim.sh      # T3
```

## 4. What exists today

### 4.1 `gui/` — 21 tests

| File | Covers | Tiers |
| --- | --- | --- |
| `tests/test_stringutilities.cpp` | UTF-8 ↔ wide-string conversion: round trips, all four UTF-8 sequence lengths, surrogate pairs, truncated input, embedded NUL | T1 |
| `tests/test_samplecodec.cpp` | The 10-bit/16-bit sample codec: exhaustive round trip over all 1024 values in all 4 slot positions, bit-position isolation, golden byte vectors, the test-pattern ramp | T1, T2 |

The codec tests are the most valuable thing in the suite. A defect there does not crash and
does not print an error — it silently corrupts every capture that is ever converted, and the
corruption is only detectable by comparing against an original that may no longer exist.

One test is skipped on Linux: `LoneHighSurrogateIsDropped` only applies where `wchar_t` is
two bytes, which is Windows.

### 4.2 `fx3/programmer/` — 24 tests

| File | Covers | Tiers |
| --- | --- | --- |
| `tests/test_paging.cpp` | EEPROM and SPI flash paging arithmetic: page padding, I2C slave rollover at 64 KiB, transfer chunking, sector counts, and that the programming loop terminates and covers the image exactly | T1 |
| `tests/test_flashprog.cpp` | Locating `cyfxflashprog.img`: search order, the compiled-in install path, empty and missing `$FX3_FLASH_PROG`, a directory masquerading as the image, and that the returned string is owned by the caller | T1 |
| `tests/cli-contract.sh` | The command-line contract: that the help text does not promise SPI flash support the tool lacks, that it names the memory it actually writes, that removed options are neither advertised nor silently ignored, and that a bad device index is rejected rather than treated as device 0 | T2 |

`cli-contract.sh` is the odd one out: it runs the built binary and asserts on its *promises*
rather than its computations. It exists because D24 and D25 both survived for a long time —
help text claiming SPI flash programming the tool has never implemented, and a `-r` that
printed a message and slept. Nothing could fail while the words and the code disagreed, so
now something does.

These look like tests of trivial code, and are not. An off-by-one in the paging arithmetic
rolls the I2C slave address at the wrong offset and writes firmware bytes over the wrong
device — which bricks the FX3, recoverable only via the PMODE jumper. The path-resolution
tests guard the D13 fix, where every candidate path used to be relative to the working
directory, so an installed binary could not find the secondary loader at all.

### 4.3 `fx3/firmware/` — one golden test

| File | Covers | Tiers |
| --- | --- | --- |
| `tests/descriptor-golden.sh` | The generated USB product descriptor: two fixed commit strings in, byte-for-byte comparison against `tests/descriptor-{0123abcd,unknown}.h`, including the computed length byte | T2 |

The generated header is the *only* path by which a version reaches the device — the FX3
serves `USB_DESC_PRODUCT_BYTES` verbatim as its product string descriptor, so a wrong length
byte or a wrong encoding is a defect the host sees and that nothing else in the build would
catch. Two commit strings rather than one, of different lengths, because the interesting byte
is computed rather than fixed.

To change the descriptor deliberately, regenerate the references:

```bash
cd fx3/firmware
for c in 0123abcd unknown; do
    bash generate-descriptor.sh /tmp "$c" > "tests/descriptor-$c.h"
done
```

This does **not** cover D8. That defect lives on `firmware_version_string`, a separate,
unreferenced symbol that `--gc-sections` discards before it ever reaches the device; nothing
host-side can observe it. See the Phase 2 notes in
[docs-tech/implementation-plan.md](docs-tech/implementation-plan.md).

There is no unit tier here and there cannot usefully be one: every source file in the
component is freestanding ARM926EJ-S code calling into the Cypress SDK, so the build host
cannot execute any of it. What *is* testable is the host-side tooling that decides what ends
up in the image.

### 4.4 `fx3/mkimage/` — 32 tests

| File | Covers | Tiers |
| --- | --- | --- |
| `tests/test_bootimage.cpp` | FX3 boot image construction: header fields, checksum range and wrapping, vector-area trimming, 64 KiB section splitting, `.bss` zero-fill, word alignment, ELF validation and rejection, and golden byte vectors | T1, T2 |

`fx3-mkimage` replaced the Cypress SDK's `elf2img` in Phase 5, and its acceptance check was a
byte comparison against that tool on the project's own firmware — identical, 111,316 bytes.
**That check cannot be re-run**, because the vendor tool has been deleted from the tree. This
suite is what guards the format now, so it is deliberately more thorough than the size of the
tool suggests.

Two tests are worth knowing about individually:

- `Checksum.MatchesTheWorkedExampleFromAN76405` reproduces the worked example in the public
  specification, `0x6AF37AF2`. It checks the implementation against Infineon's own arithmetic
  rather than against itself.
- `Golden.*` pin complete images byte for byte, so a refactor that keeps the structure but
  changes the encoding is caught.

A wrong image here does not fail loudly — the bootloader either refuses it or runs something
subtly wrong on a device that is expensive to recover.

### 4.5 `docs/` — one static check

`nix build .#docs-site` runs `mkdocs build --strict`, which fails on broken internal links,
`.nav.yml` entries pointing at missing files, and orphaned pages. That replaces the three
hand-written shell scripts the Jekyll site used.

**It has one blind spot worth knowing about.** MkDocs only validates links it parses, and it
does not parse raw HTML. A raw `<img src="assets/...">` is passed through untouched, and
because pages are served from directory URLs the path resolves one level too shallow and
404s — with the build still green. Eighteen such images were silently broken when the site
was migrated. **Use markdown image syntax**, `![](path){ width="600" }`, which MkDocs does
rewrite. If you need to check the built output directly, resolve every `href` and `src` in
`result/` against the output tree.

### 4.6 `fpga/` — three testbenches, a lint pass and a digest test

Unlike every other component, the gateware has no `ctest` suite: there is no CMake here, and
the tools are a linter and a simulator rather than a compiler. The checks are Nix derivations
running the same scripts a developer runs, so the two cannot drift.

| File | Covers | Tiers |
| --- | --- | --- |
| `tests/tb_dataGenerator.v` | The test-pattern generator: the 0…1020 ramp over three periods, ADC passthrough and its one-cycle registration, that test mode ignores the ADC bus, and the sequence number — including the wrap after exactly 63 sequences of 65536 samples | T3 |
| `tests/tb_fx3StateMachine.v` | The GPIF II handshake: idle until asked, a packet of exactly 8192 clock cycles, that a single-cycle request is enough, the gap between back-to-back packets, and that a mid-packet reset abandons rather than resumes | T3 |
| `tests/tb_statusLED.v` | The LED pattern: reset state, two full periods including both direction reversals, that the LEDs hold between steps, and that a reset restarts the walk upwards | T3 |
| `tests/run-lint.sh` | `verilator --lint-only -Wall` over the five hand-written modules | T4 |
| `tests/test_provenance.py` | The byte offsets the canonical bitstream digest masks, that a payload change is *not* masked, and that a moved field raises rather than digesting unmasked data | T1, T2 |

Run them with `./fpga/tests/run-lint.sh` and `./fpga/tests/run-sim.sh` from
`nix develop .#fpga`, or as the `fpga-lint`, `fpga-sim` and `fpga-provenance` flake checks.

`tb_dataGenerator.v` is the simulation counterpart of §5: the ramp and sequence number it
asserts are exactly what the capture-integrity procedure counts breaks in, so a defect
introduced into the generator is caught before a bitstream is built rather than after a
60-second capture.

`tb_fx3StateMachine.v` is the one that earns its keep. The state machine has no visible
failure mode — a packet of 8191 words instead of 8192 still completes a capture, and every
sample after it is wrong. It also uses blocking assignments inside a clocked block, which can
make simulation and synthesis disagree; the packet-length assertion is what turns "that is
probably fine" into something checked.

**What is not covered, and cannot be for free:** `buffer.v`, and therefore the design as a
whole. See the caveat in §6.

Lint runs with `-Wall`, and everything it reports is either a failure or a waiver carrying
its reason in `fpga/verilator-waivers.vlt`. The waived findings — a blocking assignment in
sequential logic, two incomplete `case` statements, an implicit width promotion, unused
control-bus bits fixed by the PCB — are each pinned by one of the testbenches above rather
than merely declared benign.

### 4.7 Everything else — nothing yet

| Component | Automated coverage | Why |
| --- | --- | --- |
| `hardware/` | **None**, and blocked | `kicad-cli` cannot read KiCad 5 legacy `.sch`, so ERC/DRC cannot be automated until the files are migrated. Manual for now |

## 5. The capture-integrity procedure (T5)

**This is the most important test in the project, and it is manual.**

The parts have existed for years without being written down. `dataGenerator.v` contains a
built-in test-pattern generator: when `testModeFlag` is asserted — `fx3_testMode` is
`fx3_control[05]`, settable by the host over the FX3 control interface — the FPGA substitutes
a counter ramp for real ADC data:

```verilog
assign dataOut[9:0] = testModeFlag ? testData : adcData;
…
if (testData == 10'd1021 - 1) testData <= 10'd0;
else                          testData <= testData + 10'd1;
```

And the application's test-data analysis walks a captured file checking that ramp is unbroken.

Together they form a complete end-to-end integrity oracle. Any discontinuity in the sequence
proves a sample was dropped somewhere across **FPGA → FIFO → FX3 → USB 3.0 → host → disk**.
For a data-acquisition device that is worth more than any amount of unit coverage, because
dropped samples are the failure mode that matters and the one that is invisible in normal use.

### Procedure

1. Build or install the capture application:
   `nix build .#gui` — or `nix develop .#gui` and build from source.
2. Connect the Domesday Duplicator. Confirm it enumerates as `1d50:603b`
   (`lsusb | grep 1d50`). If it appears as `04b4:...` it is still in bootloader mode and has
   no firmware loaded.
3. Launch the capture application and **enable test mode**.
4. Capture for **at least 60 seconds**. This matters: the buffer must wrap several times, and
   a short capture can pass while a real one drops samples.
5. Analyse the capture: **Edit → Analyse test data...**, or from a shell,
   `DomesdayDuplicator --analyse-test-data <file>` — which exits 0 for an intact ramp, 1 for
   a break and 2 for a file it could not read, so the gate can be scripted.
6. **Pass = zero sequence breaks.** Any break at all is a release blocker, not a flake — the
   ramp is deterministic, so there is no such thing as an intermittent false positive here.

### When to run it

- Before any release.
- After **any** change to gateware, FX3 firmware, or the host capture path — including
  changes that look purely cosmetic. A build that compiles proves nothing about the data path.
- After a Quartus version change, which alters synthesis and therefore timing.

### What it does not cover

Analogue performance. The test pattern is generated *after* the ADC, so it proves the digital
path is lossless and says nothing about gain, filtering or noise. Those remain manual bench
measurements against the calculations in `hardware/doc/`.

## 6. Planned work

Listed so this document can be read as a status report rather than a wish list. Each item is
tied to a phase of the reorganisation plan in [docs-tech/](docs-tech/).

| What | Tier | Phase | Notes |
| --- | --- | --- | --- |
| CI test lanes | — | 7 | Run T1–T4 in the consolidated workflow. T5 never runs in CI |
| Licence-header check | T4 | 8 | Nine of 69 source files carry SPDX identifiers today |
| `buffer.v` testbench | T3 | — | Needs a free `dcfifo` model, or a hand-written stand-in for it. See the caveat below; not scheduled |

Phase 6 delivered the four gateware items that used to be on this list: the `-Wall` lint
pass and the `dataGenerator`, `fx3StateMachine` and `statusLED` testbenches, all in §4.6.

Further GUI targets worth having, not yet scheduled: `amplitudemeasurement` (pure computation
over a sample buffer), the `analysetestdata` logic itself (it is the host half of the §5
oracle, so it must be trustworthy), `configuration` with injected settings rather than the
real `QSettings` backing store, and the capture state machine — `UsbDeviceBase` is already an
abstraction, so it can be mocked and the orchestration tested without hardware.

### A caveat on whole-design gateware simulation

`DomesdayDuplicator.v` instantiates the Altera `dcfifo` and `altpll` primitives through
`IPfifo.v` and `IPpllGenerator.v`. Full elaboration therefore needs vendor simulation models.
Either stub them or restrict simulation to the surrounding logic — but say which in the
testbench. Do not claim whole-design simulation is free, because it is not.

Phase 6 took the second route, and the cost is worth stating plainly: **`buffer.v` is
untested.** It is the ping-pong FIFO pair between the ADC and FX3 clock domains — two
`dcfifo` instances, the overflow detection, and the switch between them — which makes it one
of the two modules where a defect would show up as dropped samples rather than as a device
that does not work. It is linted (against black-box declarations, so the instantiations are
checked for arity and width) and it is covered on hardware by §5, and that is all.

Writing a stand-in `dcfifo` would make it simulable, but a hand-written model of a vendor
primitive is a second implementation that can itself be wrong in the direction that makes the
test pass. That is a real piece of work, not a gap to close in passing.

## 7. Conventions for new tests

- **One tier label per test.** If a test seems to need two, it is usually two tests. The
  exception is a file whose cases genuinely span T1 and T2, like the sample codec.
- **Test the failure mode, not the function.** `fx3_pad_to_i2c_page` is four lines; the test
  file is 200, because what is being pinned is "this never writes past a page boundary", not
  "this returns 64".
- **Prefer properties over examples where the input space is small enough to sweep.** The
  codec has 1024 representable values in 4 positions — exhaust them rather than picking three.
- **Golden data is committed and reviewed.** A golden test whose reference file is regenerated
  whenever it fails is not a test.
- **Never write a test that programs the device's non-volatile memory.** Bricking a
  contributor's hardware because CI ran is not an acceptable outcome.
- **Do not describe tests that do not exist.** If a change needs manual verification, say what
  you did and on what.
