# TESTING.md

How the Domesday Duplicator is tested, what that covers today, and what it does not.

This document is deliberately honest about scope. Before Phase 3 of the repository
reorganisation there were **no automated tests at all**. There are now 844 across five
components, plus three gateware testbenches, a lint pass over five Verilog modules, a static
check on the documentation site, and a licence-header check and an update-bundle check over
the whole tree. That is a
start, not a suite, and this document says so where it applies rather than describing an
aspiration as though it were a fact.

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

`ddd-gui/` works the same way, with the same tier labels:

```bash
cmake -B ddd-gui/build -S ddd-gui
cmake --build ddd-gui/build
ctest --test-dir ddd-gui/build -L unit
```

It also uses the `functional` label, which no other component does. Those are whole-pipeline
soak tests: a synthetic source generates the device's stream in software at its real
80 MB/s and pushes it through validation, metrics, the monitor tap and a sink for a minute,
with a consumer reading the tap as fast as it can. They are the strongest statement short of
T5 that the real-time design holds, and they are the reason `ctest -L unit` exists as the
everyday loop — the functional tier takes minutes where the rest takes seconds.

```bash
ctest --test-dir ddd-gui/build -L unit          # the everyday loop, ~5 seconds
ctest --test-dir ddd-gui/build -LE hil          # everything but the hardware tier
ctest --test-dir ddd-gui/build -L hil           # the hardware tier, device attached
DDD_SOAK_SECONDS=10 ctest --test-dir ddd-gui/build -L functional   # a shorter soak
```

Note the `-LE hil`: unlike every other component, `ddd-gui/` has a T5 tier that is a gtest
binary rather than a manual procedure, so a bare `ctest` here would try to talk to a device.
CI and the Nix build both exclude it explicitly.

`DDD_SOAK_SECONDS` sets each soak's duration, defaulting to 60. Whatever it ends up as is
printed in the test output, so a shortened run cannot be mistaken for a full one. The
FLAC-sink soak backs off and retries at half rate if the machine cannot sustain the
encoder, and says so in its output rather than passing quietly at a quarter speed.

**What the soak does not cover.** Everything past the host's memory: the USB stack, the
cable, the FX3 and the gateware. A pipeline that passes here can still lose samples to a
bad cable, which is why §5 exists and why passing this is not a substitute for it. The
`hil` tier reaches some of that — enough to tell a working device from a misbehaving one in
one command — but not the hour-long soak with a player attached that §5 describes.

Its build additionally runs clang-format and clang-tidy as gates, so a formatting or lint
regression fails the build before any test runs. That is deliberate — it is a new component
and can afford to be held to it from the first file — but it means the build needs those
tools present. `-DDDD_ENABLE_CLANG_FORMAT=OFF -DDDD_ENABLE_CLANG_TIDY=OFF` turns them off
where they are unavailable, which is what the Nix package does.

Tests are built by default. Pass `-DBUILD_TESTING=OFF` to skip them — the Nix packages do
this when `doCheck` is false.

The gateware has no CMake and so no `ctest`. Its checks are scripts, run either through the
flake or directly:

```bash
nix develop .#fpga -c ./fpga/tests/run-lint.sh     # T4
nix develop .#fpga -c ./fpga/tests/run-sim.sh      # T3
```

The licence-header check (§4.8) belongs to no component and needs no toolchain at all:

```bash
./tools/check-licence-headers.sh          # T4; -v also lists the unconverted files
```

## 4. What exists today

### 4.1 `gui/` — 37 tests

| File | Covers | Tiers |
| --- | --- | --- |
| `tests/test_stringutilities.cpp` | UTF-8 ↔ wide-string conversion: round trips, all four UTF-8 sequence lengths, surrogate pairs, truncated input, embedded NUL | T1 |
| `tests/test_samplecodec.cpp` | The 10-bit/16-bit sample codec: exhaustive round trip over all 1024 values in all 4 slot positions, bit-position isolation, golden byte vectors, the test-pattern ramp | T1, T2 |
| `tests/test_flacroundtrip.cpp` | The FLAC writer and reader against each other, and the container's bytes at fixed offsets | T1, T2 |
| `tests/test_testdataanalyser.cpp` | The offline test-pattern analyser: an unbroken ramp, an injected discontinuity, and the exit codes a script depends on | T1 |

The codec tests are the most valuable thing in the suite. A defect there does not crash and
does not print an error — it silently corrupts every capture that is ever converted, and the
corruption is only detectable by comparing against an original that may no longer exist.

One test is skipped on Linux: `LoneHighSurrogateIsDropped` only applies where `wchar_t` is
two bytes, which is Windows.

### 4.2 `ddd-gui/` — 748 tests (742 without hardware)

The replacement capture application. Split by what a test needs rather than by what it
covers: `ddd_capture_tests` links no Qt at all, which is what makes the engine's Qt-free
rule enforceable — if the engine ever grows a Qt dependency, that binary stops linking.

| File | Covers | Tiers |
| --- | --- | --- |
| `tests/unit/test_logger.cpp` | The engine's logging seam: level filtering, the callback boundary | T1 |
| `tests/unit/test_sample_format.cpp` | The device's wire layout: sample/counter packing, that the two agree with the byte-level constants the hot loop uses, the `(v−512)×64` scaling ld-decode expects, capture file naming | T1 |
| `tests/unit/test_test_pattern_verifier.cpp` | The ramp check: intact ramps, both gateware ramp lengths discovered rather than assumed, breaks reported at their exact offset, a dropped sample caught, state carried across buffers | T1 |
| `tests/unit/test_sequence_validator.cpp` | Sequence-marker validation and the metrics that share its pass: lock-on within one counter period, mid-stream mismatch at the exact sample, a markerless legacy stream disabling checking rather than failing, the wrap at 62, marker stripping, clip counts, RMS — and a measurement that the whole pass fits inside the 26 ms real-time budget | T1 |
| `tests/unit/test_disk_buffer_ring.cpp` | The producer-to-consumer handoff: geometry rounding, overflow detection, fill-level accounting, a contended run of 4,000 slots checked serial-by-serial, and that an abort releases waiters on **both** sides | T1 |
| `tests/unit/test_monitor_tap.cpp` | The wait-free publishers: 200,000 stats publications against a hammering reader with no torn read, triple-buffered snapshots never seen half-written, a slow reader dropping snapshots rather than delaying the writer, and the writer's own publish cost measured with four readers hammering and with none | T1 |
| `tests/unit/test_capture_pipeline.cpp` | The orchestrator: start/stop/abort, error latching precedence, injected faults surfacing as their own codes, a stalled source declared stalled rather than waited for, and a sink attached mid-stream receiving whole buffers with no sample lost or repeated | T1 |
| `tests/unit/test_usb_device.cpp` | The SuperSpeed rule, device personalities — a device with no firmware never selected for capture even when it is the remembered preference, found when a caller asks for any personality, and a change of personality counting as a change of device — preferred-device selection, and the USB transfer layout: transfers a whole number of packets, dividing a buffer exactly, the queue capped at the usbfs limit — and a simulation walking the transfers through several laps of the ring to prove buffers are handed over in the order the consumer reads them | T1 |
| `tests/unit/test_firmware_version.cpp` | The firmware version comparison: commits parsed out of the USB product string, dirty builds on either side, stamps of differing length from one commit still matching, and an application that cannot name its own commit staying quiet | T1 |
| `tests/unit/test_digest.cpp` | SHA-256 against the published FIPS 180-2 vectors and the million-character case, the streaming interface agreeing with the one-shot function at every chunk boundary, and hex parsing refusing anything but 64 hex characters | T1 |
| `tests/unit/test_json_value.cpp` | The manifest parser's strictness stated as tests: duplicate keys, trailing content, comments, trailing commas, leading zeros, unescaped control characters, lone surrogates and runaway nesting each refused by name — plus numbers surviving a round trip as the text they arrived as | T1 |
| `tests/unit/test_minisign_verify.cpp` | Signature verification against signatures **minisign 0.12 produced**, in both its modes: a manifest with one byte changed refused, an edited trusted comment refused because the second signature covers it, a signature from another key refused, and malformed key and signature files refused | T1 |
| `tests/unit/test_update_manifest.cpp` | The manifest schema: the fixture read field by field and written back byte-identically, a one-component bundle accepted and an empty one refused, an unknown schema version stopping the parse rather than producing a list, every problem reported rather than only the first, and dotted versions ordered while commit hashes and `unknown` are refused an ordering at all | T1 |
| `tests/unit/test_update_bundle.cpp` | The archive: entries round-tripped through the writer and reader including the empty, exactly-one-block and one-byte-over cases; directories, paths, bad checksums, truncation and duplicate names refused; and, at bundle level, a tampered manifest, a tampered payload, a wrong length, a missing payload, a missing signature and a manifest that is not the first entry each refused with their own message | T1 |
| `tests/golden/test_stock_tar_bundle.cpp` | A bundle **as `tools/make-update-bundle.sh` really produced it** — GNU tar's bytes, minisign's signature — opened, verified and compared against what this project's own writer produces. The one test that says the reader reads what the release tooling writes rather than only what this code writes | T1, T2 |
| `tests/unit/test_update_key.cpp` | Which signatures a build accepts: a development bundle accepted with the explicit opt-in and refused without it, a bundle whose claimed channel and signing key disagree refused, the compiled-in development key checked against the one in `tools/keys/`, and the default policy proved able to open something at all | T1 |
| `tests/unit/test_boot_image.cpp` | The FX3 boot image, read from the host's side: a well-formed image parsed into its sections with the offsets landing on the right bytes, and every malformed case refused with a sentence — a missing signature, an image that is not executable code, a type the boot ROM would not run, a checksum that does not match, a truncated file, bytes after the checksum, an image with nothing in it, and a section length that would wrap when multiplied into a byte count | T1 |
| `tests/unit/test_device_recovery.cpp` | Programming a device that has no firmware to be programmed with: the prelude downloading each section and starting it, the downloaded bytes proved to be the image's own, the updater opened at the path the device *came back* at rather than the one it left, and then the ordinary update running unchanged — plus every way the prelude can fail, all of which are things that happen to a device nobody is holding: a bundle with no firmware in it, a payload that is not an image, a download stopping part way, a device that will not start what it was given, one that never comes back, and a cancellation, each proved to leave nothing written | T1 |
| `tests/unit/test_device_updater.cpp` | The status packet: every field decoded at its offset, the three counters proved not interchangeable, the wrong length refused, a phase or error code this build does not know refused rather than narrated — and that every error code has its own sentence, none of them repeating another's | T1 |
| `tests/unit/test_update_gate.cpp` | The install-time gate: a bundle needing a newer application refused with that verdict rather than a generic one, an unknown manifest schema refused, firmware or gateware speaking a version outside this build's range refused in both directions, a downgrade inside the range allowed, a build that cannot order its own version saying so rather than assuming, the gateware floor not applied to a device whose FPGA never answered, and a device with no firmware passing the checks that need an identity while being refused a bundle that carries no firmware to give it | T1 |
| `tests/unit/test_update_orchestrator.cpp` | The whole flow against a fake device: an install proved by reading the identity back, every chunk but the last page-aligned, the chunk size taken from the device and rounded down to whole pages, the stages reported in order, transfer progress monotonic and reaching its total — and each failure branch by name: no update agent, a capture running, a payload that is not firmware, a stream digest mismatch, a readback mismatch, a device that stops answering, one that never returns, one that comes back running the wrong build, and a cancellation proved to leave nothing committed | T1 |
| `tests/unit/test_update_cli.cpp` | `ddd-update`'s command line and its exit codes: each option parsed, `--device` with nothing after it refused, two bundles refused, and a missing file reported as a bundle error before any device is touched | T1 |
| `tests/analysis/test_front_end_gain.cpp` | The board's SW401 gain switch: all fifteen switch patterns against the gain and full-scale input on the hardware calculations sheet, that closing a second switch *lowers* the gain because the resistors are in parallel, all-switches-open treated as no declaration rather than as unity, and an undeclared gain converting nothing at all | T1 |
| `tests/analysis/test_waveform_mapping.cpp` | The scope's arithmetic: sample and code to pixel and back, span and offset, a cursor clamped to the window, column decimation keeping the extremes of what it covers while leaving genuinely empty columns empty, and that every span the panel offers fits inside a snapshot rather than being silently clamped to less time than its label claims | T1 |
| `tests/analysis/test_signal_levels.cpp` | The nominal capture level: the 75% bounds landing on codes 128 and 896, symmetrical about mid-scale because the signal swings both ways about 0 V, leaving headroom before the converter clips, and a range failing nominal if either end does | T1 |
| `tests/analysis/test_amplitude_history.cpp` | The history ring and the sampler that fills it: wraparound dropping the oldest, extremes falling off the back with the points that carried them, per-interval clip counts derived from running totals, and a gap producing one point rather than a burst | T1 |
| `tests/analysis/test_fourier_transform.cpp` | The FFT, against a directly evaluated DFT sharing no code with it, plus an impulse, a tone on a bin centre, and Parseval at the 4,096 points the application runs | T1 |
| `tests/analysis/test_spectrogram_history.cpp` | The spectrum-over-time ring: rows oldest-first, the oldest dropped rather than the ring growing, a column keeping the highest bin it covers, every bin reaching some column so a one-bin carrier cannot be lost at some frequencies and not others, history kept across the whole span so narrowing the display re-draws it rather than discarding it, and a frame rate measured from the frames themselves so the time axis can be labelled in seconds rather than in a direction | T1 |
| `tests/analysis/test_spectrum_analyser.cpp` | The spectrum scaling: a tone reading its own level in its own bin, a full-scale tone at 0 dB, the Hann window keeping it out of distant bins, DC in the DC bin, a short snapshot refused rather than zero-padded, and peak hold and averaging behaving as described | T1 |
| `tests/unit/test_device_monitor.cpp` | Hot-plug detection: attach and detach reported, an attach noticed inside 500 ms, nothing reported while nothing changes, a failed enumeration not mistaken for an empty one, and enumeration suspended while streaming | T1 |
| `tests/unit/test_capture_naming.cpp` | What a capture is called: a timestamp that sorts as text whatever the machine's locale, a typed name that cannot escape into a path, the characters and reserved device names Windows refuses, test captures forced to `TestData_` whatever was typed, and an existing capture never overwritten | T1 |
| `tests/unit/test_capture_provenance.cpp` | What a capture says about itself: the real 40 MHz sample rate recorded because the FLAC header cannot hold it, test mode recorded either way, and the front-end gain written only when a declaration was actually made — never a default that would read as calibration data | T1 |
| `tests/unit/test_free_space.cpp` | Free space as a length of time rather than a size, the FLAC estimate bracketed against the wire rate, and a volume that cannot be read reported as unknown rather than as full | T1 |
| `tests/golden/test_flac_round_trip.cpp` | The capture format: lossless round trip, that the file is native FLAC (`fLaC`) and not Ogg (`OggS`), the sample-rate label ld-decode requires, provenance tags surviving into the file, and the `.raw` reader | T1, T2 |
| `tests/golden/test_test_data_analysis.cpp` | The offline ramp check, on files written by this application's own encoder: pass, fail with the break at its exact offset, and too-short-to-wrap reported as weak evidence — plus progress against the file's own length, and a cancelled analysis reported as no verdict rather than as a pass | T1, T2 |
| `tests/functional/test_pipeline_soak.cpp` | The whole pipeline at 80 MB/s for a minute, with null and FLAC sinks, and a tap consumer reading flat out | T1 (`functional`) |
| `tests/gui/unit/*.cpp` | Theme resolution across every mode/scheme/fallback combination, the bounded log model, the engine-to-GUI logging bridge, and the About text's build provenance, author, copyright and the notices the GPL asks an interactive program to show | T1 |
| `tests/gui/unit/test_capture_settings.cpp` | Settings persistence: what was saved comes back, out-of-range values clamped rather than refused, test mode deliberately not remembered, the front-end gain declaration remembered because a switch stays where it is put, an impossible switch pattern read as no declaration, and the gain never reaching the engine's options | T1 |
| `tests/gui/unit/test_statistics_presenter.cpp` | Every figure the Statistics panel shows, produced without a widget: both throughput units, elapsed time as seconds or as a clock, that no field carries a voltage until the gain is declared and that the levels carry one afterwards, that clipping is byte-identical whether the declaration is absent, right or deliberately wrong — and the whole view checked against the statistics a synthetic pipeline run actually published | T1 |
| `tests/gui/unit/test_analysis_worker.cpp` | Snapshot analysis and the thread it happens on: sequence counters stripped from every sample, a poll with nothing new staying silent, a snapshot too short for a transform still drawing a waveform, a tone reaching the right spectrum bin, and the worker stopped safely while snapshots are still being published — the race that would otherwise read a publisher the pipeline had already replaced | T1 |
| `tests/gui/unit/test_capture_controller.cpp` | The whole monitor-mode path against a fake USB backend: devices reaching the GUI, the firmware warning raised once per connection, statistics published, nothing written, enumeration pausing while streaming, and a cable pulled mid-monitor leaving an application that can monitor again | T1 |
| `tests/gui/unit/test_capture_to_disk.cpp` | Capture against a fake USB backend: starting from idle and from an existing monitor session, a stop that returns to monitoring rather than to idle, two captures in one session giving two files, the forced `TestData_` name on the file that is actually created, a written capture read back as FLAC with its provenance tags, a test-mode capture analysing clean, and a duration limit that stops the file on a buffer boundary without stopping the stream | T1 |
| `tests/gui/unit/test_capture_faults.cpp` | Fault injection through the controller: each failure reaching the user as its own message and carrying nobody else's remedy, a capture that fails mid-write leaving a finalised and readable partial file, and the message naming where that file is | T1 |
| `tests/gui/unit/test_capture_failure_presenter.cpp` | The error taxonomy as a user meets it: no two failures sharing a summary or a remedy, every failure naming something to do, the title carrying the code, and the usbfs remedy carrying the exact command to paste | T1 |
| `tests/gui/unit/test_analysis_cli.cpp` | `--analyse-test-data`'s exit codes: 0 for an intact ramp, 1 for a break, 2 for a file that could not be analysed — with the verdict on stdout and "I could not read this" on stderr | T1 |
| `tests/gui/widget/test_about_dialog.cpp` | That the logo and the application icon are compiled into the binary and load — the failure a static library's dropped resource initialiser causes, which appears only in the real application because the test binaries link it differently — and that the dialog is wider than the text it has to lay out, cuts no line off at the right-hand edge, can still be scrolled to text that does not fit, carries the logo and the notices, and has a link that can be followed | T1 |
| `tests/gui/widget/test_main_window_panels.cpp` | The dock panel framework: every panel present, floatable, toggled from the View menu, a layout that survives a restart, that no panel demands so much height that the column it shares stops being resizable, and that the separator above the bottom panel can actually be dragged in both directions — the failure a zero-height central widget causes, which resizes fine when asked in code and not at all with the mouse | T1 |
| `tests/gui/widget/test_capture_panel.cpp` | The capture controls: the device list, a USB 2 device named as such and refused, each button reading as the next thing that will happen and turning green while monitoring and red while capturing without changing size — the layout shift a stylesheet on a button causes, because the size the stylesheet path computes is not the one the platform style chose — device and test mode locked while streaming, the destination fixed once the file is open while the duration and low-space settings stay live, test mode taking the name field away, and free space shown as how much capture it holds rather than as a size | T1 |
| `tests/gui/widget/test_update_page.cpp` | The whole update flow as a widget, driven against fakes with nothing plugged in — including branches a bench cannot be asked for. A verified bundle enabling the install and saying so, a development bundle bannered, a file that is not a bundle and one that is not there each refused with a reason, a bundle needing a newer application disabling the button, a successful install reporting what the device now runs, and each failure by name: a capture in progress, a corrupted transfer caught before anything is committed, a device that never comes back, and the wrong build coming back not being called a success. Plus a device with no firmware: named as being in recovery mode with both ways it gets there stated, offered **Program this device** rather than a repair, its version rows reading "None installed" and "Cannot be read", and a payload that is not firmware proved never to reach the device's memory | T1 |
| `tests/gui/widget/test_analysis_dialog.cpp` | The analysis dialog: pass and fail reported with the break's offset, pass and fail coloured differently through the theme tokens, an unreadable file distinguished from a failed one, the cancel button becoming the close button, and a dialog destroyed mid-analysis joining its worker rather than leaving a thread running into a destroyed object | T1 |
| `tests/gui/widget/test_statistics_panel.cpp` | That the figures reach the right labels: the four integrity states reading differently, a new run clearing the last one's numbers, a finished run leaving them up, and the three capture-only rows blank while monitoring and filled in once a writer is attached | T1 |
| `tests/gui/widget/test_waveform_panel.cpp` | The scope panel: the span choices reaching the plot, persistence off until asked for, the cursor reading in codes alone until a gain is declared, the plot painting empty, full and in persistence mode, and — counted in pixels a person could actually see — persistence leaving earlier sweeps on screen while its absence leaves only the latest | T1 |
| `tests/gui/widget/test_spectrum_panel.cpp` | The spectrum panel: averaging and peak-hold controls, an empty bin described rather than reported as -120 dBFS, the plot painting with and without a spectrum, both views offered with peak hold disabled where it would mean nothing, the frequency range defaulting past the filter's corner — and, in pixels, that the spectrogram draws a carrier as a visible band, that widening the range moves it down the frequency axis, and that it grows from the right over a fixed window of time rather than stretching to fill | T1 |
| `tests/gui/widget/test_amplitude_panel.cpp` | The amplitude panel: statistics becoming history points at the intended rate, the window's extremes, a new run clearing the history and a finished one keeping it — the property the gain design rests on — that correcting the declaration re-labels history already recorded instead of discarding it — the nominal-level marks and the RMS trace each drawn on both sides of 0 V — measured against the panel's own gridlines so the checks survive a change of margins — a Clear that resets the sampler along with the history, and a time span that can be set to everything held or narrowed to match the spectrogram — checked by measuring what is drawn at the oldest edge, because counting the whole plot passes against a panel that ignores the setting | T1 |
| `tests/hardware/test_device_capture.cpp` | An attached device: found at a usable speed, reporting a readable firmware commit, delivering at the ADC's rate and no faster, aborting within two seconds, streaming with no samples lost, and its test ramp arriving intact | T5 (`hil`) |

Three of these are worth singling out.

**The sink-swap test** is the one the synthetic source exists for. It runs the pipeline in
monitor mode, attaches a recording sink mid-stream, and then checks the recorded values are
an unbroken run of the device's ramp. Counting bytes would pass even if the pipeline had
written one buffer twice and dropped another; only the values can show that "start
recording" lost nothing.

**The stall test** covers a failure the old engine cannot see at all. A device that stops
delivering without failing a transfer raises no error and returns from no call, so the
application simply waits — and a frozen progress figure looks exactly like a slow disc. The
new engine has a watchdog on the transfer count, and this is what proves it fires.

**The hardware tier** is the only one that needs a device, and it is run deliberately:
`ctest --test-dir build -L hil`. It refuses rather than skips when nothing is attached,
because a hardware test that "passes" because there was no hardware is worse than no test.
Nothing in it reprogrammes anything — it streams and sends the `0xB6` configuration
request, which is what the application does in normal use (AGENTS.md §4). Its
rate check is diagnostic in its own right: the device's output is clocked by a 40 MHz
converter, so a working one delivers 80 MB/s and cannot deliver more, and a higher figure
means the samples are not coming from the ADC at all. That is not hypothetical — the first
device this tier ran against was delivering a 16-bit counter at 117 MB/s from an FPGA that
was not running the sampler, and the rate check is what named the cause where the
sequence-mismatch error only reported a symptom.

Measured against a device running gateware built from this tree: 79.2 MB/s over 800 MB
(99% of the wire rate), 6,103 sequence-counter periods every one exactly 65,536 samples,
121,634,816 test-pattern samples unbroken, peak ring depth 1 buffer of 128, and an abort
returning in 10 ms. This tier is twenty seconds and does not replace §5 — it is what makes
starting §5 worthwhile.

### 4.3 `fx3/programmer/` — 24 tests

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

### 4.4 `fx3/firmware/` — three tests

| File | Covers | Tiers |
| --- | --- | --- |
| `tests/descriptor-golden.sh` | The generated USB product descriptor: two fixed commit strings in, byte-for-byte comparison against `tests/descriptor-{0123abcd,unknown}.h`, including the computed length byte | T2 |
| `tests/register-map` | The host-testable half of the FPGA register map: which addresses exist, which are writable, and what the firmware refuses to relay | T1 |
| `tests/update-protocol` | The host-testable half of the device update protocol: the `UPDATE_BEGIN` packet decoded and a reserved flag refused, the status packet's fields at their offsets, which requests are admitted in which phase, out-of-order and oversized chunks refused, the capture/update exclusion, a failure staying stuck at its first cause, an image without the `'CY'` signature refused before anything is written, and the paging arithmetic for **both** media — the EEPROM's slave addressing, write spans capped at a page, read spans capped at a bank and page padding, the EPCS's program spans and sector boundaries, and a walk over a whole image of each proving the writes cover it exactly once with no page, slave or sector boundary crossed and nothing programmed into a sector that has not been erased. Plus what the two media's readers have to agree about: the flash's silicon identifiers and whether an image fits the device that answered, the CRC-32 pinned to its published check value, and the boot block encoded byte for byte against a golden block `fpga/make-boot-block.py` produced | T1 |

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

This does **not** cover the old `firmware_version_string` defect. That one lived on a
separate, unreferenced symbol which `--gc-sections` discards before it ever reaches the
device, so nothing host-side can observe it — which is exactly why the version now travels
through the product descriptor above instead.

The unit tier here covers exactly the two source files written to be SDK-free —
`fpga-register-map.c` and `update-protocol.c` — and it exists because of what those two
files decide. Everything else in the component is freestanding ARM926EJ-S code calling into
the Cypress SDK, so the build host cannot execute any of it; a change to `update-agent.c`'s
I2C sequencing is a bench change and nothing here will catch it.

Keeping those two SDK-free is worth the effort it costs. The decisions in them — which
registers a host may write, which requests are refused, and where each byte of a firmware
image lands in the boot EEPROM — are exactly the sort that fail quietly on hardware, by
allowing something rather than by crashing. An off-by-one in the paging arithmetic writes
past a page or a slave boundary and leaves a device that will not enumerate, and a bench is
a poor place to discover that.

### 4.5 `fx3/mkimage/` — 32 tests

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

### 4.6 `docs/` — one static check

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

### 4.7 `fpga/` — eight testbenches, a lint pass and two digest tests

Unlike every other component, the gateware has no `ctest` suite: there is no CMake here, and
the tools are a linter and a simulator rather than a compiler. The checks are Nix derivations
running the same scripts a developer runs, so the two cannot drift.

| File | Covers | Tiers |
| --- | --- | --- |
| `tests/tb_dataGenerator.v` | The test-pattern generator: the 0…1020 ramp over three periods, ADC passthrough and its one-cycle registration, that test mode ignores the ADC bus, and the sequence number — including the wrap after exactly 63 sequences of 65536 samples | T3 |
| `tests/tb_fx3StateMachine.v` | The GPIF II handshake: idle until asked, a packet of exactly 8192 clock cycles, that a single-cycle request is enough, the gap between back-to-back packets, and that a mid-packet reset abandons rather than resumes | T3 |
| `tests/tb_spiRegisters.v` | The SPI register bank, driven at the fastest clock the specification allows: reset values, the identity block and the image role in one transfer, test mode and the LEDs written and read back, address auto-increment and wrap, unmapped reads returning zero, writes to read-only registers discarded, a byte cut short by chip select leaving nothing behind, the `0x20`–`0x23` window read and written, that `BRIDGE_DATA` alone does *not* auto-increment, and the decoded-byte pulse the watchdog tickle is built on | T3 |
| `tests/tb_flashBridge.v` | The EPCS pass-through: inert while locked, an interrupted unlock sequence that cannot be completed by a later stray write, the sequence that does unlock it, a real read answered by a model of the EPCS64 — so the mode-0 edges have to be the right way round — and relocking by write and by reset | T3 |
| `tests/tb_bootLoader.v` | The factory image's boot decision, built as its top level wires it and read through the bridge from a model of the EPCS64: a valid boot block arms the watchdog with the right address and *then* reconfigures, and the wrong magic, a bad block checksum, a damaged image and an unknown layout version each leave the unit in the factory image | T3 |
| `tests/tb_crc32.v` | The boot block's checksum against the published CRC-32 check value, so what the gateware computes is what a host's library computes, plus the restart the boot logic depends on between its two runs of bytes | T3 |
| `tests/run-lint.sh` | `verilator --lint-only -Wall` over the twelve hand-written modules, across both images and the half they share | T4 |
| `tests/run-sdc.sh` | Both images' timing constraints: that they parse as Tcl, and that they name every pin the top level maps | T4 |
| `tests/run-version.sh` | The commit-to-identity-register stamp: an eight-character hash, the seven-character one a Nix build passes, a dirty tree, a build with no commit, a full-length hash, and a string that is not a hash at all | T2 |
| `tests/test_provenance.py` | The byte offsets the canonical bitstream digest masks, that a payload change is *not* masked, and that a moved field raises rather than digesting unmasked data | T1, T2 |
| `tests/test_boot_block.py` | The boot block encoder, field by field and by offset, including the exact bytes `tb_bootLoader.v` is written against, and the four descriptions it refuses to encode | T1 |

Run them with `./fpga/tests/run-lint.sh` and `./fpga/tests/run-sim.sh` from
`nix develop .#fpga`, or as the `fpga-lint`, `fpga-style`, `fpga-sim`, `fpga-sdc`,
`fpga-provenance`, `fpga-version` and `fpga-boot-block` flake checks.

`tb_bootLoader.v` is the one that matters most, and for a reason none of the others share:
the logic it covers is the only logic in this repository that a field update can never
repair. A factory image that refuses a good boot block strands every unit in recovery; one
that accepts a bad block hands the device to an image that may not come back. It is
therefore built the way the device is — boot logic, flash bridge, active serial block and
reconfiguration control, with a model of the EPCS64 behind them — rather than by stubbing
out the flash. The expected checksums come from an independent implementation (Python's
`zlib`), so a fault shared by the gateware and the testbench cannot pass.

What no simulation can cover is the handover itself: a simulated device cannot reconfigure.
The testbench checks that the right thing was asked for at the right moment; the bench
checks that asking for it works, which is §7's remaining gateware item.

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
whole. See the caveat in §7.

Lint runs with `-Wall`, and everything it reports is either a failure or a waiver carrying
its reason in `fpga/verilator-waivers.vlt`. The waived findings — a blocking assignment in
sequential logic, two incomplete `case` statements, an implicit width promotion, unused
control-bus bits fixed by the PCB — are each pinned by one of the testbenches above rather
than merely declared benign.

### 4.8 Repository-wide — the licence-header and update-bundle checks

Two checks have no component, because their subject is the whole tree rather than any part
of it. Both are T4 and both live in `nix/checks.nix`.

#### `licence-headers`

`tools/check-licence-headers.sh`, the `licence-headers` flake check (T4). Every project-authored `.c .h .cpp .inl .v .py .sh .nix .S` file must carry both
a copyright statement and a licence statement in its first 40 lines; a file missing either
fails the build.

It accepts SPDX and the long-form GPL notice alike, and prints how many files still carry the
long form on every run. That is deliberate. The convention is SPDX (AGENTS.md §5.4) and files
convert as they are touched, so a check that failed every unconverted file would force
exactly the sweeping rewrite the convention exists to avoid — while a check that said nothing
would let the conversion stall unnoticed.

Vendored and generated files are exempt **by name**, each with its reason in the script.
There is no wildcard: adding a third-party file means writing an exemption for it, which is
the point. An exemption nobody had to write is an exemption nobody reviewed.

The check reads only tracked files. It asks git when git is there, and walks the tree when it
is not — inside the Nix sandbox the flake source *is* the tracked set, so both routes see the
same files. Without that, a local run would header-check every `moc_*.cpp` in `gui/build/`.

#### `update-bundle`

Every commit assembles a real update bundle with `tools/make-update-bundle.sh` — a
firmware-only, development-signed one over a synthetic payload — and takes it apart again.
It checks that the entries come out in the order the format fixes, that the signature
verifies, that the manifest's digest matches the payload, and that assembling the same
inputs twice gives byte-identical files.

Everything after the bundle exists is done with **stock tools**: GNU `tar` lists it,
`minisign` verifies it, `sha256sum` checks the digest. That is the point of the check.
The application's own reader is covered by the tests in §4.2, and a check that used this
project's reader to validate this project's writer could only ever say that the two agree
with each other.

Nothing here writes to a device. The payload is a text file, and installing a bundle stays
a deliberate human act (AGENTS.md §4).

### 4.9 Everything else — nothing yet

| Component | Automated coverage | Why |
| --- | --- | --- |
| `hardware/` | **None**, and blocked | `kicad-cli` cannot read KiCad 5 legacy `.sch`, so ERC/DRC cannot be automated until the files are migrated. Manual for now |

## 5. The capture-integrity procedure (T5)

**This is the most important test in the project, and it is manual.**

The parts have existed for years without being written down. `dataGenerator.v` contains a
built-in test-pattern generator: when `test_mode_flag` is asserted — `fx3_test_mode` comes
from the SPI register bank at register `0x10`, settable by the host over the FX3 control
interface — the FPGA substitutes a counter ramp for real ADC data:

```verilog
assign data_out[9:0] = test_mode_flag ? test_data : adc_data;
…
if (test_data == 10'd1021 - 1) begin
    test_data <= 10'd0;
end else begin
    test_data <= test_data + 10'd1;
end
```

And the application's test-data analysis walks a captured file checking that ramp is unbroken.

Together they form a complete end-to-end integrity oracle. Any discontinuity in the sequence
proves a sample was dropped somewhere across **FPGA → FIFO → FX3 → USB 3.0 → host → disk**.
For a data-acquisition device that is worth more than any amount of unit coverage, because
dropped samples are the failure mode that matters and the one that is invisible in normal use.

### Procedure

1. Build or install the capture application:
   `nix build .#gui` — or `nix develop .#gui` and build from source.
2. Connect the Domesday Duplicator. Confirm it enumerates as `1209:2347`
   (`lsusb | grep 1209`). If it appears as `04b4:...` it is still in bootloader mode and has
   no firmware loaded.
3. Launch the capture application and **enable test mode**.
4. Capture for **at least 60 seconds**. This matters: the buffer must wrap several times, and
   a short capture can pass while a real one drops samples.
5. Analyse the capture: **Edit → Analyse test data...**, or from a shell,
   `DomesdayDuplicator --analyse-test-data <file>` — which exits 0 for an intact ramp, 1 for
   a break and 2 for a file it could not read, so the gate can be scripted.

   `ddd-gui` offers the same check under **File → Analyse test data...** and as
   `ddd-gui --analyse-test-data <file>`, with the same three exit codes and the same
   wording. The two applications have been checked side by side on the same files for all
   three verdicts — pass, break, and too short for the ramp to have wrapped — and agree on
   the code, the sample offset and the expected and actual values.
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

## 6. The firmware update procedure (T5)

Everything below writes the FX3's boot EEPROM. **Nothing automated does this** (AGENTS.md
§4): each step is a deliberate human act, and this section exists so that it is the same
deliberate human act every time.

You need a Duplicator and a USB 3 port. One step — provisioning a unit running firmware
from before the update agent existed (U0) — needs the J4 jumper and `fx3-programmer`.
Everything else is done from the application, which is the whole point of the mechanism,
and that now includes recovering a unit whose update was interrupted (U5) and bringing up
a kit that has never been programmed at all (U6).

### What to have ready before you start

- A development bundle of the firmware under test: `./tools/dev-bundle.sh` after building
  the firmware, which writes `build/domesday-duplicator-update-0.0.0-dev.dddfw`.
- A **known-good** bundle of the firmware currently on the device, so that any state this
  procedure leaves the unit in can be undone without a jumper.
- The commit each of them carries, so the identity check at the end means something. The
  bundle's manifest states it; `tar -xOf <bundle> manifest.json` prints it.

### The jumper command, written out once

Two of the steps below need `fx3-programmer`, and both of them need two things that are
easy to get wrong from a cold start:

- the device must **already be in bootloader mode** — J4 fitted and power-cycled, showing
  `04b4:00f3`. `-p` and `-u` both refuse anything else;
- the Cypress secondary loader has to be findable. Its in-tree candidate paths are relative
  to the working directory, so from the repository root it must be named explicitly.

From the repository root, that is:

```bash
FX3_FLASH_PROG=fx3/programmer/cyfxflashprog.img \
  ./fx3/programmer/build/fx3-programmer -p fx3/firmware/build/firmware.img -v
```

Then remove J4 and power-cycle again. `$FX3_FLASH_PROG` is first in the resolution order,
ahead of both the relative candidates and the installed copy under `/usr/local/share`.

### U0 — provisioning a device that cannot yet update itself

A unit running firmware from before the update agent has no `0xD0` to answer, so the
application will say so rather than pretending. It needs the jumper once, and only once.

1. Fit J4, power-cycle, confirm `04b4:00f3`.
2. Run the command above.
3. Remove J4, power-cycle, confirm `1209:2347`.
4. `lsusb -d 1209:2347 -v | grep bcdDevice` — **expect `1.00`**. That is the protocol
   version field, and it is the cheapest possible proof that the firmware now running is
   one that can update itself.

### U1 — the ordinary update, from the application

The bundle to install is the one `./tools/dev-bundle.sh` wrote:

```
<repository root>/build/domesday-duplicator-update-0.0.0-dev.dddfw
```

**Its commit must differ from the one now on the device**, or step 4 proves nothing: an
update that installs the image already installed cannot demonstrate that anything was
installed. If U0 flashed the same build the bundle carries — which it will have, if both
came from one `cmake --build` — reconfigure and rebuild the firmware, then re-run
`dev-bundle.sh`, before starting:

```bash
cmake -B fx3/firmware/build -S fx3/firmware \
      -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake   # re-stamps the version
cmake --build fx3/firmware/build
./tools/dev-bundle.sh
tar -xOf build/domesday-duplicator-update-0.0.0-dev.dddfw manifest.json | grep commit
```

The reconfigure is not optional. `FIRMWARE_VERSION` is worked out at configure time, so
`cmake --build` alone rebuilds the image with the *previous* stamp and the bundle would
carry a commit the device already reports.

1. Attach the device. Confirm **Help → Firmware…** reports the commit you expect — the one
   U0 flashed, not the one in the bundle.
2. On the **Update** tab, press **Choose update file…** and pick the bundle above. Confirm
   it reports *verified*, names the version, shows the development banner, states a time
   estimate and "leave the device plugged in", and enables **Update**.
3. Press **Update**. The stages, quoted as the window titles them:

   | Stage title | What is happening |
   | --- | --- |
   | *Checking the update* | Host-side; over before it is read |
   | *Sending the update to the device* | **The EEPROM is written here**, page by page as each chunk arrives |
   | *The device is checking what it wrote* | The whole region read back and hashed |
   | *Restarting the device* | It disconnects and reconnects by itself |
   | *Confirming the new version* | The identity read back off the live device |

   There is no long "writing" stage on this target and there is not meant to be: the
   firmware writes each chunk to the EEPROM as it arrives, so the sending bar is the
   writing bar. The `UPDATE_PHASE_WRITING` the protocol defines is one page here — the
   signature page — and the EPCS target is where it becomes a stage of its own.

   Record roughly how long *sending* and *checking what it wrote* each took. The
   application's estimate is derived from a nominal EEPROM rate and this is the only
   measurement of the real one.
4. **Pass** = the confirmation names the bundle's commit, and it is not the commit the
   device started with.
5. Re-open **Help → Firmware…**. The versions page must agree with the confirmation.

### U2 — the same update, headlessly

```bash
ddd-update --dry-run build/domesday-duplicator-update-0.0.0-dev.dddfw   # expect 0
ddd-update build/domesday-duplicator-update-0.0.0-dev.dddfw             # expect 0
```

**Pass** = both exit zero, every stage is named in order, and the second prints the
device's commit read back after the restart. This is the same engine code the application
drives, so a disagreement between U1 and U2 is a finding in itself.

Re-installing the bundle U1 just installed is fine here, and is the ordinary way to run
this: the run still exercises the transfer, the write, the readback verification, a real
disconnect and reconnect, and the identity read. What it cannot do is *distinguish*
installed from not-installed, because both answers are the same string — so if you want
that assertion too, build a second image with a stamp of its own:

```bash
cmake -B /tmp/fwbuild -S fx3/firmware \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/fx3/firmware/arm-none-eabi-toolchain.cmake \
      -DFIRMWARE_VERSION=deadbe01
cmake --build /tmp/fwbuild

./tools/make-update-bundle.sh \
  --output build/domesday-duplicator-update-0.0.0-u2.dddfw \
  --version 0.0.0 --commit deadbe01 --channel development \
  --secret-key tools/keys/development.key --public-key tools/keys/development.pub \
  --notes 'U2 bench image — distinguishable stamp, not a real commit.' \
  --firmware /tmp/fwbuild/firmware.img --firmware-identity deadbe01 \
  --firmware-interface-version 1
```

Passing the stamp in is the mechanism Nix and CI already use, because a build outside a
checkout has no `.git` to ask. Two things about it are worth knowing before you try:
`-dirty` is stripped before commits are compared, so no amount of editing or rebuilding
changes the identity without a new commit; and the stamp must be hex and at least seven
characters or it is not read as a commit at all. Use `make-update-bundle.sh` directly
rather than `dev-bundle.sh`, which takes the commit from git. Install an honest bundle
afterwards so the unit is not left reporting a commit that does not exist.

### U3 — interrupted update, and the fallback it depends on (verification item V1)

**This is the load-bearing one.** The whole safety story rests on a kit whose EEPROM holds a
*corrupt* image falling back to the USB bootloader, and that has to be demonstrated rather
than assumed.

1. Start an update as in U1.
2. **Pull the USB cable while *Sending the update to the device* is in progress**, with the
   bar somewhere past half. That is the stage in which the EEPROM is actually being
   written, so at that moment it holds a partial image — and the signature page is held
   back until everything else has been written *and* read back, so there is no valid image
   for the boot ROM to find.
3. Plug the device back in.
4. **Pass** = the device enumerates as `04b4:00f3`, the Cypress bootloader. Anything else —
   a device that does not enumerate, or one that enumerates as `1209:2347` and does not
   work — falsifies V1 and is a finding that changes the design, not a test failure to
   retry.
5. Recover with **U5 below**, which is the point of this whole procedure: a device that
   has fallen back is directly programmable from the application, with no jumper and no
   shell. J4 is how a *working* device is forced into bootloader mode; a device that has
   fallen back is already in it.

   If U5 is what is under test and has not yet been shown to work, the shell route still
   exists as a fallback — `fx3-programmer -l` lists a fallen-back device as
   `Mode=Bootloader`:

   ```bash
   FX3_FLASH_PROG=fx3/programmer/cyfxflashprog.img \
     ./fx3/programmer/build/fx3-programmer -p fx3/firmware/build/firmware.img -v
   ```

   Power-cycle afterwards. Record which route you used.

Repeat step 2 at two other points — very early in *Sending*, and during *The device is
checking what it wrote* — and confirm the same fallback each time. The second of those is
the interesting one: the image is complete on the medium by then and only the signature
page is missing, which is the narrowest the window ever gets.

### U4 — the refusals

Each of these must be refused, and refused with a sentence rather than a code:

| Do this | Expect |
| --- | --- |
| Start a capture, then try to update | "The device is capturing. Stop the capture and try again." |
| Start an update, then try to start a capture | The capture does not start |
| Choose a file that is not a bundle | A reason, and **Update** stays disabled |
| Choose a bundle whose `minimum_application_version` is above this build | "Update the application first", and **Update** stays disabled |
| Close the Firmware window mid-update | It explains why not, and says when it will be safe |
| With a device in recovery mode, choose a bundle carrying only gateware | "This device has no working firmware, and this update file does not contain any", and **Program this device** stays disabled |

### U5 — recovering an interrupted update, from the application alone

The other half of U3, and the reason U3's recovery step no longer needs a shell. Start
from a device left in bootloader mode by U3.

1. Confirm the state without opening anything: the application's status bar reads **Device
   attached with no firmware**, and the Capture panel's device list names the port with
   *recovery mode, no firmware installed*. Monitoring and capture are both unavailable.
2. Open **Help → Firmware…**. The page says the device is in recovery mode, that its
   firmware is missing, and that it is not damaged. The firmware row reads **None
   installed** and the gateware row **Cannot be read**.
3. The button reads **Program this device**, not "Update" and not "Repair".
4. Choose the bundle from U1 and press it. The stages must run:

   | Stage | What is happening |
   | --- | --- |
   | *Starting the device up* | The firmware is going into the FX3's **RAM**. Nothing permanent is written in this stage |
   | *Sending the update to the device* | **The EEPROM is written here**, by the firmware that has just been loaded |
   | *The device is checking what it wrote* | Readback and digest, exactly as in U1 |
   | *Restarting the device* | It disconnects and reconnects by itself |
   | *Confirming the new version* | The commit is read back off the device |

5. **Pass** = the confirmation quotes the bundle's commit, the device enumerates as
   `1209:2347`, and a capture runs. No jumper was fitted and no shell command was used at
   any point.
6. Repeat once with the cable pulled during *Starting the device up*. Expect the device to
   come back in recovery mode again, unchanged, and step 4 to succeed on a second attempt
   — nothing permanent is written in that stage, so an interruption there costs a retry
   and nothing else.

Also run it headlessly, which drives the identical engine path:

```bash
ddd-update build/domesday-duplicator-update-0.0.0-dev.dddfw    # expect 0
```

with the device in recovery mode. It must report the device as being in recovery mode and
programme it without any extra option.

**On Windows**, expect the device *not to appear at all* until WinUSB has been bound to
`04b4:00f3` with Zadig. Confirm both halves of that: that it is missing before the
binding, and that steps 1 to 5 then run identically to Linux afterwards. That binding
step is a documented user procedure, on the *If an update fails* page.

### U6 — a kit that has never been programmed

The same procedure as U5, on a device that has never held this firmware at all. A blank
EEPROM and one corrupted by an interrupted update are indistinguishable on the wire, so
this is a test of the *wording* as much as of the mechanism — and of the assumption that
they really are indistinguishable.

Use a SuperSpeed Explorer Kit that has never been programmed, or erase one deliberately.

1. Plug it in with **no jumper fitted**. It must enumerate as `04b4:00f3`.
2. Follow U5 from step 1. Every screen must read the same as it did there.
3. **Pass** = the kit reaches working firmware from the application alone, and at no point
   is the user told that anything is broken, damaged or needs repairing. A person who has
   just soldered a board has not broken anything, and being told they have is a failure of
   this step even when the programming succeeds.

### When to run it

- Before any release that changes the FX3 firmware.
- After any change to `update-agent.c`, which is the half of the update path no host test
  reaches.
- U3 in particular after any change to the order in which pages are written.
- U5 and U6 after any change to `device_programmer.cpp` or `boot_image.cpp`, which are the
  only code in the application that hands bytes to a device's boot ROM.

### What it does not cover

The FPGA target. The firmware now answers for it — `0xD1`–`0xD3` write the EPCS through the
gateware's flash bridge and `0xD5` reconfigures the FPGA — but **none of it has been run on a
board**, because running it needs a unit provisioned with a dual-image flash and no unit has
been. The gateware procedures are written up in the session that first performs them, and
§7 lists what that session has to establish.

Everything about target 1 that can be checked without hardware has been: the paging and
sector arithmetic, the boot block's encoding checked byte for byte against the encoder the
build uses, the CRC-32 pinned to its published check value, the compatibility gate that
refuses a gateware update to a device that cannot take one, and the whole flow driven
through the application against a fake device. None of that is evidence that a real flash
was written correctly, and this section exists to say so.

## 7. Planned work

Listed so this document can be read as a status report rather than a wish list.

| What | Tier | Notes |
| --- | --- | --- |
| CI test lanes | — | Run T1–T4 in the consolidated workflow. T5 never runs in CI |
| SPDX conversion of the remaining long-form headers | T4 | 25 files. Opportunistic by design (AGENTS.md §5.4) — not a scheduled task, and the check prints the count each run |
| Finish validating the single-clock gateware | T5 | See below. The board is programmed and a 16-minute capture came back clean; four checks remain |
| Device-update bench procedures for the FPGA target | T5 | §6 covers the FX3 target. The firmware, the gateware and the application halves of the EPCS path are all built; nothing has run on a board. The first session confirms, in this order: the flash identifies itself through the bridge and names a device the firmware recognises (**V7**, and the silicon identifier is currently taken from the datasheet rather than from a part); a gateware update from the running application image completes and the unit comes back reporting the new gateware commit; the throughput is measured against the estimate the application shows before it starts (**V6** — and only if the bit-bang dominates is the bridge's read path worth changing, which is a change to the frozen image); power is pulled mid-write and the unit boots the factory image with `IMAGE_ROLE` reading `0x00`, the application names it *recovery gateware*, and **Reinstall gateware** repairs it; the same from a freshly provisioned unit, which starts in that state by construction; and the boot block sector is erased by JTAG to confirm the same fall-back from the other direction. Written up in the session that performs them, never ahead of it |
| First release through the CI pipeline | T4/T5 | The bitstream, release and reproducibility-audit workflows exist and every part of them that can be exercised without a signing key and a tag has been: the bundle assembles and verifies against a pinned public key, an application built with that key pinned accepts it and one built without refuses it, and a release build refuses a development-signed bundle unless the opt-in is given. What remains needs the maintainer: generate the release keypair, set `UPDATE_SIGNING_KEY`, commit `tools/keys/release.pub`, then tag. The rehearsal is then: the tag publishes a release whose every asset was CI-built from it, the `.dddfw` installs onto bench hardware through the file-picker path with the device reporting the identities the manifest names, and the audit job runs green against that release at least once |
| Dual-image provisioning and the factory-to-application handover | T5 | Both gateware images build and the boot decision is simulated, but no unit has been provisioned with a dual-image flash. First bench session confirms: the provisioning `.jic` programs, the unit comes up in the factory image with `IMAGE_ROLE` reading `0x00`, the reconfiguration block's parameter encoding is the one `remoteUpdate.v` assumes, the handover to the application image works and `IMAGE_ROLE` then reads `0x01`, the watchdog period is measured against a worst-case FX3 boot before it is frozen, and the FX3's "FPGA ready" timing assumption still holds across two configurations rather than one. Not written up ahead of being run |

### Validating the single-clock gateware

The gateware moved from two clock domains joined by an Altera `dcfifo` to one 80 MHz clock
and the project's own `fifo.v`, with the 40 MSPS sampling rate decimated from that clock. It
is verified in simulation (five testbenches), by lint and style, by a constraint check, and by
a Quartus build whose timing closes with every slack positive and whose memory infers as 32
M9K blocks.

**Programmed and partly validated on 2026-08-14.** A 16-minute capture of 38,363,201,536
samples completed with no sequence break. That result carries weight because
`capture_pipeline.cpp` validates the sequence counter over *every* buffer, unconditionally and
before the test-mode branch, and stops the capture the moment one is wrong — so a capture that
ran to completion is a positive statement that nothing was dropped between the ADC and the
host, not merely an absence of complaints.

What is left:

1. **A test-mode capture analysed offline** with `--analyse-test-data`. Not redundant with the
   live check above: it reads the file back off the disk, so it also covers the FLAC encoder,
   the filesystem and the drive (§5, and the caveat in `test_data_analysis.h`).
2. **A capture of 30 minutes or more**, to catch timing marginality too rare for 16.
3. **A noise-floor comparison against a pre-change capture.** The one analogue change is that
   the ADC's clock now comes from a fabric flip-flop rather than a PLL output. Jitter at
   40 MSPS is expected to be negligible, but "expected" is not measured.
4. **A deliberate stall test**: pause the host reader mid-capture and confirm `bufferError` and
   the resulting sequence-number gap behave as documented.

Note for step 1: `--analyse-test-data` checks the 0…1020 **test-mode ramp**, so running it on
an ordinary RF capture reports a break within the first few samples. That is the tool being
asked the wrong question, not a fault. The sequence numbers are not available as a fallback,
because the capture application converts each sample back to the 10-bit domain and writes only
that — the sequence field exists during a capture and nowhere afterwards.

The gateware items that used to be on this list are done: the `-Wall` lint pass and the
`dataGenerator`, `fx3StateMachine` and `spiRegisters` testbenches are all in §4.7, and the
licence-header check is in §4.8. So is the `buffer.v` testbench, which is here because it
needed a free `dcfifo` model — replacing the IP with `fifo.v` removed the requirement
rather than meeting it.

Further GUI targets worth having, not yet scheduled: `amplitudemeasurement` (pure computation
over a sample buffer), the `analysetestdata` logic itself (it is the host half of the §5
oracle, so it must be trustworthy), `configuration` with injected settings rather than the
real `QSettings` backing store, and the capture state machine — `UsbDeviceBase` is already an
abstraction, so it can be mocked and the orchestration tested without hardware.

### A caveat on whole-design gateware simulation

`DomesdayDuplicator.v` instantiates the Altera `altpll` primitive through
`IPpllGenerator.v`. Full elaboration therefore needs a vendor simulation model. Either stub
it or restrict simulation to the surrounding logic — but say which in the testbench. Do not
claim whole-design simulation is free, because it is not.

Everything below the top level *is* simulated. The buffer used to be exempt for the same
reason — it was two `dcfifo` instances — and replacing that IP with `fifo.v` is what brought
the capture path into the testbench suite.

This used to cost more than the top level. `buffer.v` was the ping-pong `dcfifo` pair
between the ADC and FX3 clock domains, so it was untested too — and it is one of the two
modules where a defect shows up as dropped samples rather than as a device that does not
work. The option considered at the time was to write a stand-in `dcfifo`, and it was
rejected because a hand-written model of a vendor primitive is a second implementation that
can itself be wrong in the direction that makes the test pass.

What closed it was removing the primitive instead of modelling it: `fifo.v` is the
project's own single-clock FIFO, so `buffer.v` and everything under it are now ordinary
Verilog with ordinary testbenches. Only the top level is left needing `altpll`, and the
only thing that reaches is the pin mapping and the clock generation, both of which §5
covers on hardware.

## 8. Conventions for new tests

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
