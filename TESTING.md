# TESTING.md

How the Domesday Duplicator is tested, what that covers today, and what it does not.

This document is deliberately honest about scope. Before Phase 3 of the repository
reorganisation there were **no automated tests at all**. There are now 44 across two
components, plus a static check on the documentation site. That is a start, not a suite, and
this document says so where it applies rather than describing an aspiration as though it were
a fact.

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

### 4.2 `fx3/programmer/` — 23 tests

| File | Covers | Tiers |
| --- | --- | --- |
| `tests/test_paging.cpp` | EEPROM and SPI flash paging arithmetic: page padding, I2C slave rollover at 64 KiB, transfer chunking, sector counts, and that the programming loop terminates and covers the image exactly | T1 |
| `tests/test_flashprog.cpp` | Locating `cyfxflashprog.img`: search order, the compiled-in install path, empty and missing `$FX3_FLASH_PROG`, a directory masquerading as the image, and that the returned string is owned by the caller | T1 |

These look like tests of trivial code, and are not. An off-by-one in the paging arithmetic
rolls the I2C slave address at the wrong offset and writes firmware bytes over the wrong
device — which bricks the FX3, recoverable only via the PMODE jumper. The path-resolution
tests guard the D13 fix, where every candidate path used to be relative to the working
directory, so an installed binary could not find the secondary loader at all.

### 4.3 `docs/` — one static check

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

### 4.4 Everything else — nothing yet

| Component | Automated coverage | Why |
| --- | --- | --- |
| `fx3/firmware/` | **None** | Bare-metal ARM. The descriptor golden test (§6) is planned for Phase 5 |
| `fpga/` | **None** | Testbenches are planned for Phase 6 |
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

And `dddutil`'s test-data analysis walks a captured file checking that ramp is unbroken.

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
5. Open the captured file in `dddutil` and run the test-data analysis.
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
| FX3 descriptor golden test | T2 | 5 | Feed `generate-descriptor.sh` a fixed commit string, compare the generated header byte-for-byte against a committed reference. Would have caught D8, and covers the length byte the host actually reads |
| `verilator --lint-only` over all hand-written modules | T4 | 6 | Immediate value, near-zero effort |
| `dataGenerator.v` testbench | T3 | 6 | Assert the ramp is exactly 0…1020 then wraps — the simulation counterpart of §5 |
| `fx3StateMachine.v` testbench | T3 | 6 | The highest-risk module: the GPIF II handshake |
| `statusLED.v` testbench | T3 | 6 | Simple timing logic, easy win |
| CI test lanes | — | 7 | Run T1–T4 in the consolidated workflow. T5 never runs in CI |
| Licence-header check | T4 | 8 | Nine of 69 source files carry SPDX identifiers today |

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
