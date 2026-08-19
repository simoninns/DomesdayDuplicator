# TESTING.md

How the Domesday Duplicator is tested, what that covers today, and what it does not.

This document is deliberately honest about scope. This repository once had **no automated
tests at all**. There are now 1,421 across five
components, plus nine gateware testbenches, a lint pass over five Verilog modules, a static
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
| T5 | `hil` | Hardware-in-the-loop. Needs a real Domesday Duplicator | **No** — manual, gated on release |
| T5 | `hil-player` | Hardware-in-the-loop against a real **LaserDisc player** | **No** — manual, gated on release |

T5 never runs in CI and never runs unattended. Several of its steps write to non-volatile
memory on the device.

**Why the player has a label of its own.** They are two different pieces of hardware and
not every bench has both: a machine with a Duplicator and no player must not fail tests it
has no way to run. Each test still carries exactly one tier label, which keeps the rule
simple — the two are separate rows here rather than a single test wearing both.

One wrinkle worth knowing, because it is silent otherwise: CTest matches labels as an
unanchored regular expression, so `-L hil` selects `hil-player` too. The device tier alone
is `-L "^hil$"`. Exclusion needs no such care — `-LE hil` excludes both, which is what CI
and the Nix build do.

## 3. Running the tests

**From the repository root** — `nix flake check` itself works from any subdirectory, since
Nix walks up to the single root `flake.nix`, but the `cmake` paths below are relative to the
root:

```bash
nix flake check                      # everything, on a clean machine
```

Or per component, from a configured build tree:

```bash
cmake -B ddd-gui/build -S ddd-gui
cmake --build ddd-gui/build
ctest --test-dir ddd-gui/build                    # all tests
ctest --test-dir ddd-gui/build -L unit            # one tier
ctest --test-dir ddd-gui/build -LE hil            # everything except hardware
ctest --test-dir ddd-gui/build --output-on-failure
```

It also uses the `functional` label, which no other component does. Those are whole-pipeline
soak tests: a synthetic source generates the device's stream in software at its real
80 MB/s and pushes it through validation, metrics, the monitor tap and a sink for a minute,
with a consumer reading the tap as fast as it can. They are the strongest statement short of
T5 that the real-time design holds, and they are the reason `ctest -L unit` exists as the
everyday loop — the functional tier takes minutes where the rest takes seconds.

```bash
ctest --test-dir ddd-gui/build -L unit          # the everyday loop, ~5 seconds
ctest --test-dir ddd-gui/build -LE hil          # everything but the hardware tiers
ctest --test-dir ddd-gui/build -L "^hil$"       # the device tier, Duplicator attached
ctest --test-dir ddd-gui/build -L hil-player    # the player tier, player attached
DDD_SOAK_SECONDS=10 ctest --test-dir ddd-gui/build -L functional   # a shorter soak
```

The player tier takes its bench from the environment, so it does not have to scan every
serial port on the machine — which is a thing to avoid where the bench has other equipment
on it (see *Risks and safety* in the player control plan):

```bash
DDD_PLAYER_PORT=/dev/ttyUSB0 DDD_PLAYER_BAUD=9600 \
DDD_PLAYER_DISC_TYPE=cav DDD_PLAYER_LAST_FRAME=54321 \
  ctest --test-dir ddd-gui/build -L hil-player --output-on-failure
```

All four are optional and each buys something: the first two skip the scan, and the last
two turn the examination from a measurement into a comparison — without them it must
still complete and establish the disc's type and length, with them it must also get them
right. `DDD_PLAYER_MODEL_ID` checks the identity against the badge on the front panel.
**These tests spin the disc**, for about a minute, exactly as the application's own
**Examine** does.

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

The licence-header check (§4.7) belongs to no component and needs no toolchain at all:

```bash
./tools/check-licence-headers.sh          # T4; -v also lists the unconverted files
```

## 4. What exists today

### 4.1 `ddd-gui/` — 1,648 tests (1,638 without hardware)

The capture application. Split by what a test needs rather than by what it
covers: `ddd_capture_tests` links no Qt at all, which is what makes the engine's Qt-free
rule enforceable — if the engine ever grows a Qt dependency, that binary stops linking.
`ddd_player_tests` links none either, for the same reason and with the same effect on the
LaserDisc player protocol.

| File | Covers | Tiers |
| --- | --- | --- |
| `tests/unit/test_logger.cpp` | The engine's logging seam: level filtering, the callback boundary | T1 |
| `tests/unit/test_sample_format.cpp` | The device's wire layout: sample/counter packing, that the two agree with the byte-level constants the hot loop uses, the `(v−512)×64` scaling ld-decode expects, capture file naming | T1 |
| `tests/unit/test_test_pattern_verifier.cpp` | The ramp check: intact ramps, both gateware ramp lengths discovered rather than assumed, breaks reported at their exact offset, a dropped sample caught, state carried across buffers | T1 |
| `tests/unit/test_sequence_validator.cpp` | Sequence-marker validation and the metrics that share its pass: lock-on within one counter period, mid-stream mismatch at the exact sample, a markerless legacy stream disabling checking rather than failing, the wrap at 62, marker stripping, clip counts, RMS — and a measurement that the whole pass fits inside the 26 ms real-time budget | T1 |
| `tests/unit/test_disk_buffer_ring.cpp` | The producer-to-consumer handoff: geometry rounding, overflow detection, fill-level accounting, a contended run of 4,000 slots checked serial-by-serial, and that an abort releases waiters on **both** sides | T1 |
| `tests/unit/test_monitor_tap.cpp` | The wait-free publishers: 200,000 stats publications against a hammering reader with no torn read, triple-buffered snapshots never seen half-written, a slow reader dropping snapshots rather than delaying the writer, and the writer's own publish cost measured with four readers hammering and with none | T1 |
| `tests/unit/test_capture_pipeline.cpp` | The orchestrator: start/stop/abort, error latching precedence, injected faults surfacing as their own codes, a stalled source declared stalled rather than waited for, a sink attached mid-stream receiving whole buffers with no sample lost or repeated, the device's buffer readings reaching the statistics — counted once per reading however many times the same one is seen, and accumulated across the run as the device's own counters clear when they are read — and the published throughput: measured across a window rather than averaged over the run, so a paced source reads its true rate while the same snapshot's lifetime average is still a third below it, no figure published at all until a window has passed, and the last rate held once the capture stops rather than divided by a stopping time in which no buffer can arrive | T1 |
| `tests/unit/test_usb_device.cpp` | The SuperSpeed rule, device personalities — a device with no firmware never selected for capture even when it is the remembered preference, found when a caller asks for any personality, and a change of personality counting as a change of device — preferred-device selection, and the USB transfer layout: transfers a whole number of packets, dividing a buffer exactly, the queue capped at the usbfs limit — and a simulation walking the transfers through several laps of the ring to prove buffers are handed over in the order the consumer reads them | T1 |
| `tests/unit/test_firmware_version.cpp` | The firmware version comparison: commits parsed out of the USB product string, dirty builds on either side, stamps of differing length from one commit still matching, and an application that cannot name its own commit staying quiet | T1 |
| `tests/unit/test_fpga_telemetry.cpp` | The gateware's account of its capture buffer: a well-formed block read field by field, the all-zero reading of gateware without the instrument and the all-ones reading of a floating link both refused, a layout version this build does not know refused rather than misread, geometry that cannot be true refused before anything divides by it — and the scale itself, where a peak at the packet threshold is no back pressure at all, half the room above it is half the scale, and an interval that lost samples reads 100 whatever its peak was | T1 |
| `tests/unit/test_digest.cpp` | SHA-256 against the published FIPS 180-2 vectors and the million-character case, the streaming interface agreeing with the one-shot function at every chunk boundary, and hex parsing refusing anything but 64 hex characters | T1 |
| `tests/unit/test_json_value.cpp` | The manifest parser's strictness stated as tests: duplicate keys, trailing content, comments, trailing commas, leading zeros, unescaped control characters, lone surrogates and runaway nesting each refused by name — plus numbers surviving a round trip as the text they arrived as | T1 |
| `tests/unit/test_minisign_verify.cpp` | Signature verification against signatures **minisign 0.12 produced**, in both its modes: a manifest with one byte changed refused, an edited trusted comment refused because the second signature covers it, a signature from another key refused, and malformed key and signature files refused | T1 |
| `tests/unit/test_update_manifest.cpp` | The manifest schema: the fixture read field by field and written back byte-identically, a one-component bundle accepted and an empty one refused, an unknown schema version stopping the parse rather than producing a list, every problem reported rather than only the first, dotted versions ordered while commit hashes and `unknown` are refused an ordering at all — and the provisioning component: a set read and written back under its own name, a set carrying only vectors accepted as a manifest, and a component kind this build does not know refused by name rather than silently skipped | T1 |
| `tests/unit/test_update_bundle.cpp` | The archive: entries round-tripped through the writer and reader including the empty, exactly-one-block and one-byte-over cases; directories, paths, bad checksums, truncation and duplicate names refused; and, at bundle level, a tampered manifest, a tampered payload, a wrong length, a missing payload, a missing signature and a manifest that is not the first entry each refused with their own message — plus a signed bundle carrying all four payloads opened and its vectors checked against the manifest's digest like any other payload, and refused when one byte of them changes | T1 |
| `tests/golden/test_stock_tar_bundle.cpp` | A bundle **as `tools/make-update-bundle.sh` really produced it** — GNU tar's bytes, minisign's signature — opened, verified and compared against what this project's own writer produces. The one test that says the reader reads what the release tooling writes rather than only what this code writes | T1, T2 |
| `tests/unit/test_update_key.cpp` | Which signatures a build accepts: a development bundle accepted with the explicit opt-in and refused without it, a bundle whose claimed channel and signing key disagree refused, the compiled-in development key checked against the one in `tools/keys/`, and the default policy proved able to open something at all | T1 |
| `tests/unit/test_boot_image.cpp` | The FX3 boot image, read from the host's side: a well-formed image parsed into its sections with the offsets landing on the right bytes, and every malformed case refused with a sentence — a missing signature, an image that is not executable code, a type the boot ROM would not run, a checksum that does not match, a truncated file, bytes after the checksum, an image with nothing in it, and a section length that would wrap when multiplied into a byte count | T1 |
| `tests/unit/test_device_recovery.cpp` | Programming a device that has no firmware to be programmed with: the prelude downloading each section and starting it, the downloaded bytes proved to be the image's own, the updater opened at the path the device *came back* at rather than the one it left, and then the ordinary update running unchanged — plus every way the prelude can fail, all of which are things that happen to a device nobody is holding: a bundle with no firmware in it, a payload that is not an image, a download stopping part way, a device that will not start what it was given, one that never comes back, and a cancellation, each proved to leave nothing written | T1 |
| `tests/unit/test_device_updater.cpp` | The status packet: every field decoded at its offset, the three counters proved not interchangeable, the wrong length refused, a phase or error code this build does not know refused rather than narrated — and that every error code has its own sentence, none of them repeating another's | T1 |
| `tests/unit/test_update_gate.cpp` | The install-time gate: a bundle needing a newer application refused with that verdict rather than a generic one, an unknown manifest schema refused, firmware or gateware speaking a version outside this build's range refused in both directions, a downgrade inside the range allowed, a build that cannot order its own version saying so rather than assuming, the gateware floor not applied to a device whose FPGA never answered, a device with no firmware passing the checks that need an identity while being refused a bundle that carries no firmware to give it, and a file carrying only the bring-up payloads refused by the update window with a sentence naming the window that does want it | T1 |
| `tests/unit/test_update_orchestrator.cpp` | The whole flow against a fake device: an install proved by reading the identity back, every chunk but the last page-aligned, the chunk size taken from the device and rounded down to whole pages, the stages reported in order, transfer progress monotonic and reaching its total — and each failure branch by name: no update agent, a capture running, a payload that is not firmware, a stream digest mismatch, a readback mismatch, a device that stops answering, one that never returns, one that comes back running the wrong build, and a cancellation proved to leave nothing committed — plus the deferred restart the bring-up flow uses, where the write and the readback happen and the reset, the FPGA reload and the confirmation deliberately do not, and the ordinary path proved to still do all three | T1 |
| `tests/unit/test_update_cli.cpp` | `ddd-update`'s command line and its exit codes: each option parsed, `--device` with nothing after it refused, two bundles refused, and a missing file reported as a bundle error before any device is touched | T1 |
| `tests/unit/test_usb_blaster_cable.cpp` | The USB-Blaster's wire protocol, against a fake byte pipe: a TCK cycle as the same pin state twice with the clock raised, TDO asked for on the half of the cycle before the edge, eight TMS-low cycles collapsing into one byte-shift command, the last bit of a scan dropping back to bit-bang because it raises TMS, a long run split at the largest command, a wait clocked as whole bytes with the remainder bit-banged and nothing ever asked back, commands held until something needs the cable to have caught up, the two status bytes on every packet dropped rather than read as data, and a cable that only ever answers status given up on rather than waited for | T1 |
| `tests/unit/test_svf_player.cpp` | The programming file and the TAP state machine it walks: the run forced to a known state, a scan's whole cycle stream — the walk there, TMS raised on the last bit and nowhere else, the walk to the state the file says scans end in — answers compared under their mask and a mismatch naming the line and both values, what a statement remembers and what it deliberately does not, waits counted and left where their end state says, a wait taking at least as long as the count stands for at the rate the file declares, and the files this player refuses rather than half-understands: a chain with more than one device on it, a drive of a reset line the cable does not have, a value wider than its scan, a statement it does not know. Fixtures include a real Quartus-emitted file played against a device that agrees with it and one that does not | T1 |
| `tests/unit/test_bringup_orchestrator.cpp` | Bringing a board up, and the one property here that protects hardware rather than data: **the FPGA is refused until the FX3 has been programmed** — before the cable is so much as opened — whatever calls it and in whatever order, including after an FX3 step that failed. Plus both halves run in order against fakes, the deferred restart the fitted jumper requires, a set with no firmware and a set with no vectors each refused, the cable driver's own sentence carried through rather than replaced, a stopped play reported as stopped rather than failed, and progress reported in the shape the update page already consumes | T1 |
| `tests/unit/test_jtag_cli.cpp` | `ddd-jtag`'s command line and its exit codes: each option parsed, two files refused, a missing file reported before any cable is opened, and a dry run reading a whole programming file and reporting what it would have clocked out, with nothing attached and nothing written | T1 |
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
| `tests/golden/test_flac_round_trip.cpp` | The capture format: lossless round trip, that the file is native FLAC (`fLaC`) and not Ogg (`OggS`), the sample-rate label ld-decode requires, provenance tags surviving into the file, and the uncompressed `.s16` reader | T1, T2 |
| `tests/golden/test_test_data_analysis.cpp` | The offline ramp check, on files written by this application's own encoder: pass, fail with the break at its exact offset, and too-short-to-wrap reported as weak evidence — plus progress against the file's own length, and a cancelled analysis reported as no verdict rather than as a pass | T1, T2 |
| `tests/functional/test_pipeline_soak.cpp` | The whole pipeline at 80 MB/s for a minute, with null and FLAC sinks, and a tap consumer reading flat out | T1 (`functional`) |
| `tests/player/test_player_registry.cpp` | Every registered player model swept at once: unique model IDs and names, every claimed capability having a command to send for it, every definition reachable by a probe the session actually iterates, an unclaimed model ID resolving to nothing rather than to the generic definition, physical position gated on the firmware revision and not on the model — and definitions deliberately built wrong, because the consistency check that fails the build cannot be tested by compiling | T1 |
| `tests/player/test_player_controls.cpp` | What a connected player can actually be asked to do, resolved from its definition and the firmware it reported: nothing at all before there is a player, a missing command sequence and a declared lack each withholding a control on its own — the second being the case that matters, since a model inheriting the shared set and then saying it has no scan must not be offered one — a mode with no wire parameter withheld though the command exists, physical position following the firmware rather than the model, and a sweep proving every registered model can be driven at all | T1 |
| `tests/player/test_command_encoder.cpp` | The bytes on the wire, pinned against a committed table taken from the previous application — the test that says this port did not change a protocol known to work. Every model encoding the shared set identically, unpadded decimal addresses, an argument that comes before the mnemonic as well as between two, the audio parameter table's deliberate gap, and every refusal: a negative or over-wide address, a missing or spurious argument, a mode the model has no parameter for, and a command too long for a player to accept — refused rather than truncated, because a truncated command is a different valid one | T1 |
| `tests/player/test_response_parser.cpp` | Reading replies: an acknowledgement as leniently as the players require, a refusal with its error code, silence told apart from refusal, a text reply not put through the error convention because a user code may contain an 'E' — and, because a real LD-V4300D answers both user-code queries with "E04" while parked, a reply that is *exactly* 'E' and digits told apart from one that merely contains an 'E' — and a text reply keeping every byte but its terminator, since the Pioneer user code is a fixed-width record whose space-padded fields whitespace trimming would eat, lead-in and lead-out markers kept, a time code read as a frame refused rather than silently truncated, every documented active mode plus the two undocumented ones, the physical position byte-swapped and scaled — with an unreadable one reported as no position rather than as zero, which is a real position — and the disc's own programme status decoded in full against both manuals' worked examples: loaded, CAV/CLV, size, side and chapters, the project's own `11011` bench reading decoding as the second side of a 12-inch CLV disc, an 'X' left absent rather than read as a zero — because "I could not tell which side this is" is not "side 1" — a reply too short for the fields the model claims not read at all, and a model that reports no such field never asked to invent one. Plus the TV system request, which is the only thing that carries the video standard: the manual's own two NTSC examples, this project's `220` bench reading from a PAL disc, the disc's standard read separately from the one being output — because a converting player makes those different answers, and a capture is of what is on the disc — and an unknown digit left unknown rather than guessed | T1 |
| `tests/player/test_user_code.cpp` | The Pioneer User's Code, which is not one blob but three regions of documented size — Disc Control Data, Key Data, Control Data — totalling 200 characters in disc order. A real disc off this project's bench splitting at those boundaries, which is what turns "sixty of the two hundred failed" into the far more specific "the customer's own identifying data could not be read"; unreadable characters counted per region; a character that was never encoded told apart from one the player could not read, because Pioneer's own worked example has an empty Key Data where this bench's disc has an unreadable one; and a short reply reported as short rather than padded over | T1 |
| `tests/player/test_disc_profile.cpp` | What an examination found, and what follows from it: a field nobody filled in saying so rather than reading as zero — which matters because zero is a real frame number — every value recorded together with where it came from, a CLV disc's length being a time already and needing no video standard, a CAV disc's frame count producing no playing time at all until a standard is declared and the right one for each once it is, and the three user-code outcomes — not asked, none encoded, could not be read — proved not to compare equal, so nothing downstream can treat "we did not ask" as "the disc has none" | T1 |
| `tests/player/test_disc_examiner.cpp` | The whole examine sequence, driven with no player attached: a parked CAV disc examined in dependency order, a CLV disc seeked by time code where a CAV one is seeked by frame — with the old application's own impossible addresses — a player already playing not spun up again, and every failure branch, which is the point of the sequence being a value rather than control flow. One refused query leaving one field unknown and the rest of the examination finishing; a disc whose type never arrived not seeked on a guess; a seek back to the start that was refused meaning the address after it is not the start, unlike the seek past the end where a refusal is the technique working; an open tray and a disc that will not spin reported as findings rather than faults; a link failure keeping what was found; a cancel possible at every one of the ten steps and restartable from each; a reply arriving after a cancel ignored; both user codes read without being asked for, with the Pioneer one read before anything has positioned the disc — which is what makes reading it unconditionally affordable — an error code recorded as no user code rather than as one; the disc's size, side and chapters taken from its own programme status rather than by driving the transport, so the chapter probe is not sent at all and survives only as the fallback for a model that cannot report the field; a field the player could not determine left unknown rather than read as a no, and a "not loaded" digit never allowed to undo a disc that demonstrably played; the video standard asked for rather than declared, taken from the disc's field and not the output's, left unknown where the player will not say and not asked for at all on a model without the command; and a model with no user-code queries or no seek at all having a shorter plan rather than a plan full of refusals | T1 |
| `tests/player/test_auto_capture_plan.cpp` | What a guided setup may produce, and what makes a capture impossible: the three shapes a player can actually be asked for, a plan checked against the disc it was built for rather than against itself — an address in the wrong scheme for the disc refused, a range that ends before it starts refused, a range that runs off the end of the measured programme refused, and a disc whose length nobody established refusing every shape rather than accepting a range nothing can bound — a disc positively found to be absent told apart from a disc nobody asked about, the estimate covering the programme span alone and stating that it does, and the default plan for a profile with holes in it being one that can be shown rather than one that cannot | T1 |
| `tests/player/test_auto_capture_sequence.cpp` | The automatic capture, step by step and failure by failure, with nothing plugged in: each shape's own ordering — a whole-side capture stopping the player *before* the capture so the spin-down reaches the file, every other shape stopping the capture first — the front panel locked and released, a disc swapped between the setup and the capture noticed rather than captured anyway, a refused spin-up ending the run rather than watching an address that will never move, an address that stops advancing distinguished from a player that has merely stopped by asking the player which it is, a cancel between any two steps still running the tail down so the file is finalised, and the property the whole design exists for: **every branch that started a capture stops it**, with the single exception stated rather than accidental — a link that dies leaves the capture running and says so, because truncating a good capture over a cable is the worse of the two failures | T1 |
| `tests/player/test_player_session.cpp` | Finding and driving a player against a scripted fake port: found at each of the four baud rates without being told, the model and firmware read out of the reply, an unrecognised model still connecting with the generic set, a terse player still identified, something that is not a player named as such, a fixed rate never departed from, an answer that arrives too late leaving a session that is closed and reusable, a reply arriving a byte at a time still being one reply, every reply carrying the bytes that provoked it — including the failures, since "the link died sending FR100SE" is more use than "the link died" — and every command path: acknowledged, refused, ignored, unsupported, unbuildable, and a link that dies mid-command reported once and then as "not connected" | T1 |
| `tests/gui/unit/test_player_discovery.cpp` | Which serial ports get written to, and when — the part of this feature that reaches outside the application. USB adapters ranked above built-in ports, excluded and busy ports never offered, the remembered port tried first and only once, a remembered port that has gone not probed at all, a fixed port never departed from and tried even when nothing lists it, and a retry delay that grows and stays bounded | T1 |
| `tests/gui/unit/test_player_settings.cpp` | Player control off until somebody turns it on, a round trip of everything else, and the clamping: a baud rate no player uses and a model this build does not know both read as "work it out", and half a remembered port is forgotten rather than half-used | T1 |
| `tests/gui/unit/test_player_text.cpp` | Every wording the player produces, without a widget: no two states reading the same, every failure naming something to do about it, the Linux permission case named because it is the commonest one, a mismatch naming both models and the port, an unverified definition saying so, time codes as a clock and a clock read back as a time code — with "1:99" refused rather than guessed at, because a seek to a number the application invented would move the disc somewhere nobody asked for — the lead-in and lead-out said rather than numbered, a position no model can report shown as no row at all, an unavailable control naming the models that do have it, an exchange described as what went out beside what came back, and the hex dump a reply that is data rather than a word gets — pinned to the column, marking the unprintable, left off entirely for a reply like "P04" that is already legible, and for a Pioneer user code split into its three documented regions, each numbered from its own place in the whole, each saying what it could not read as against what was never encoded, and a reply of the wrong length saying so rather than pretending the format's offsets are the player's. Plus the examine report: every stage named in words a user can act on — with the eleven-second one explaining itself — every fact labelled with how it was arrived at, so a measurement and an inference are not presented alike, a field nobody established reading "not known" rather than as a blank, a CAV disc's playing time withheld with the reason given while a CLV disc's is stated along with what a capture of it would cost, the size and side reported as a disc is described — inches, because that is what a disc is sold as — a side the player could not determine never reported as side 1, the disc-status reply shown beside what was decoded from it so the decode can be checked rather than trusted, the four user-code outcomes reading as four different sentences, the sixty characters a player could not read said in words, the user codes labelled informational only where somebody reading the report will see it, and the undecoded disc-status reply carried through verbatim | T1 |
| `tests/gui/unit/test_auto_capture_controller.cpp` | The automatic capture where the player meets the capture engine, against fake player and fake USB backends: a whole run driven end to end with the writer attached before the disc starts and detached after it stops, the disc's own facts — model, type, size, side, standard, programme bounds — reaching the capture's provenance so a file says which side of which disc it is, the two coupling preferences each proved in both directions with the debounce that keeps a player's momentary stop from truncating a good capture, and a link that dies leaving the capture running with the interface told so rather than a capture quietly outliving the thing that started it | T1 |
| `tests/gui/unit/test_player_controller.cpp` | The whole connection state machine against a scripted fake port and a fake clock: nothing opened or written until player control is turned on, a player found and identified with no configuration, the port that worked remembered and written through to the settings file, silence told apart from a port that will not open and from something that is not a player, an excluded port never opened even with a player on it, the wrong model reported as a live connection that says so and resolved by accepting what answered, the status polled and read in the disc's own terms for CAV and CLV, a link that dies reported and searched for again, switching off releasing the port, a command going out and its answer coming back with the request attached — so a caller with more than one thing outstanding can tell the answers apart — a request with no player answered rather than dropped, what the player can do arriving with the connection and leaving with it, an examination driving the whole sequence on the worker's thread and coming back as a profile — with the bytes proved to have gone out in the old application's own form — both user codes read every time, the disc's own programme status reaching the profile as its size, side and chapters with no chapter search sent at all, the video standard reported rather than declared, an examination with no player answered rather than lost so that a window waiting on it never waits forever, an open tray ending it after one question without spinning anything up, the status poll proved not to interleave with it — a query landing between a seek and its answer being how a reply gets attributed to the wrong command — and every method proved to return immediately | T1 |
| `tests/gui/unit/*.cpp` | Theme resolution across every mode/scheme/fallback combination, the bounded log model, the engine-to-GUI logging bridge, and the About text's build provenance, author, copyright and the notices the GPL asks an interactive program to show | T1 |
| `tests/gui/unit/test_capture_settings.cpp` | Settings persistence: what was saved comes back, out-of-range values clamped rather than refused, test mode deliberately not remembered, the front-end gain declaration remembered because a switch stays where it is put, an impossible switch pattern read as no declaration, and the gain never reaching the engine's options | T1 |
| `tests/gui/unit/test_statistics_presenter.cpp` | Every figure the Statistics panel shows, produced without a widget: both throughput units, elapsed time as seconds or as a clock, that no field carries a voltage until the gain is declared and that the levels carry one afterwards, that clipping is byte-identical whether the declaration is absent, right or deliberately wrong, the whole view checked against the statistics a synthetic pipeline run actually published — and the device buffer: a working capture shown as half the buffer in use with the moving figure leading the caption, a stretched one described in words as well as on the bar, lost samples replacing the percentages with the damage, and idle told apart from a gateware that cannot report | T1 |
| `tests/gui/unit/test_analysis_worker.cpp` | Snapshot analysis and the thread it happens on: sequence counters stripped from every sample, a poll with nothing new staying silent, a snapshot too short for a transform still drawing a waveform, a tone reaching the right spectrum bin, and the worker stopped safely while snapshots are still being published — the race that would otherwise read a publisher the pipeline had already replaced | T1 |
| `tests/gui/unit/test_capture_controller.cpp` | The whole monitor-mode path against a fake USB backend: devices reaching the GUI, the firmware warning raised once per connection, statistics published, nothing written, enumeration pausing while streaming, and a cable pulled mid-monitor leaving an application that can monitor again | T1 |
| `tests/gui/unit/test_capture_to_disk.cpp` | Capture against a fake USB backend: starting from idle and from an existing monitor session, a stop that returns to monitoring rather than to idle, two captures in one session giving two files, the forced `TestData_` name on the file that is actually created, a written capture read back as FLAC with its provenance tags, a test-mode capture analysing clean, and a duration limit that stops the file on a buffer boundary without stopping the stream | T1 |
| `tests/gui/unit/test_capture_faults.cpp` | Fault injection through the controller: each failure reaching the user as its own message and carrying nobody else's remedy, a capture that fails mid-write leaving a finalised and readable partial file, and the message naming where that file is | T1 |
| `tests/gui/unit/test_capture_failure_presenter.cpp` | The error taxonomy as a user meets it: no two failures sharing a summary or a remedy, every failure naming something to do, the title carrying the code, and the usbfs remedy carrying the exact command to paste | T1 |
| `tests/gui/unit/test_analysis_cli.cpp` | `--analyse-test-data`'s exit codes: 0 for an intact ramp, 1 for a break, 2 for a file that could not be analysed — with the verdict on stdout and "I could not read this" on stderr | T1 |
| `tests/gui/unit/test_bringup_text.cpp` | What the bring-up wizard says: every page numbered and titled, the overview naming every physical act in advance, **every power-cycle instruction asking for *both* cables**, the timeout leading with the partial power cycle rather than mentioning it third, one vocabulary for the jumper (fitted and removed, never open and closed), the charge-only cable named ahead of the not-connected case, an attached-but-unopenable cable given the remedy that fits it, the kit's debug port separating an unpowered board from an unanswering one, each firmware a board can be running named as itself — current firmware with its commit and a pointer at the ordinary update path, a protocol this build does not know, and the original `1d50:603b` — the legend explaining the three marks in the colours the rows actually use, an ordinary update file refused with the reason, the closing checks — four for a finished board, one for a device that is not there, and none at all for a claim the set did not make — and the two lines every page ends in: a finished step leading with **All done** and naming the button to press rather than burying success mid-paragraph, an unfinished one saying what it is waiting for, the working pages opening by naming the button rather than closing with it, and the two physical pages given as numbered instructions | T1 |
| `tests/gui/unit/test_bundled_update.cpp` | Where an installed build looks for the update bundle it was packaged with — three layouts, no two of which exist on the same computer: beside the executable for an MSI, `Contents/Resources` for a `.app`, and the XDG data path under the application ID for a Flatpak or a prefix install. One name everywhere, the data directories kept in QStandardPaths' order so a user's own copy wins, nothing offered at all when the application does not know where it is, and a build that bundles nothing finding nothing | T1 |
| `tests/gui/widget/test_about_dialog.cpp` | That the logo and the application icon are compiled into the binary and load — the failure a static library's dropped resource initialiser causes, which appears only in the real application because the test binaries link it differently — and that the dialog is wider than the text it has to lay out, cuts no line off at the right-hand edge, can still be scrolled to text that does not fit, carries the logo and the notices, and has a link that can be followed | T1 |
| `tests/gui/widget/test_main_window_panels.cpp` | The dock panel framework: every panel present, floatable, toggled from the View menu, a layout that survives a restart, that no panel demands so much height that the column it shares stops being resizable, and that the separator above the bottom panel can actually be dragged in both directions — the failure a zero-height central widget causes, which resizes fine when asked in code and not at all with the mouse | T1 |
| `tests/gui/widget/test_settings_dialog.cpp` | The settings dialog and its tabs: the two halves grouped rather than run together, the dialog opening on the tab the menu entry was about, each half round-tripping without touching the other — two controllers apply them, so a page that overwrote the other's values would be a way to lose a setting by opening a window — a chosen port that is not there staying chosen, an exclusion surviving its adapter being unplugged, and a remembered port forgotten when it is excluded or overridden | T1 |
| `tests/gui/widget/test_player_panel.cpp` | The player dock: built and laid out with no controller at all, every reading blank until there is a player, the checkbox being the setting rather than a copy of it, a connected player named with the port and speed it was reached on, an unverified definition saying so on screen, the wrong model offering to be accepted, **Search now** offered only when there is nothing to talk to, **Remote…** and **Examine…** each offered only once there is something to drive and each asking the window rather than opening one — two remotes would be two things sending commands down one cable, and two examinations would be two sequences seeking one disc — and losing the player blanking the readings rather than leaving a stale position on screen | T1 |
| `tests/gui/widget/test_player_remote_dialog.cpp` | The remote: built and inert with no controller at all, non-modal because driving the player while watching the spectrum is the whole point of it, every button asserted against the bytes it puts on a scripted port rather than against a signal, the speed and audio selectors carrying their per-model parameter, a seek sending the address that was typed and an entry that is not one never sent at all, the addressing following the disc — no frame entry on a CLV disc — a control the model lacks disabled with a tooltip naming the models that have it, losing the player greying the window out rather than closing it, a manual command shown exactly as it was answered, refusal included, and a whole 200-byte Pioneer user code — the real one, off this project's bench — reaching the box intact and dumped for reading, with the sixty characters the player could not read said in words rather than left as a wall of backticks | T1 |
| `tests/gui/widget/test_examine_dialog.cpp` | The examine window: built and inert with no controller at all, **Examine** offered only when there is a player and a cable pulled out greying it rather than closing the window, the window saying what examining will do to the disc before it is asked for, a whole examination run against a scripted player end to end — stop offered while it runs, start not, the progress bar ending full and the window ready to do the second side — the last disc's report cleared before the next one starts rather than left to be read as the new side's, a profile with holes in it rendered as "not known" rather than as blanks or zeroes, an open tray explained rather than shown as an empty disc, the report copyable and copy offered only once there is something to copy, and stopping saying so before it has taken effect — because a request that is made and not yet granted otherwise looks like a button that was ignored | T1 |
| `tests/gui/widget/test_guided_capture_dialog.cpp` | The guided capture setup: built from a profile rather than from nothing, so a CAV disc gets frame entry and a CLV disc gets time entry and neither is offered the other's — absent rather than merely disabled — the three shapes offered with the entry fields each of them needs, a plan that cannot be made saying which of the reasons it is and leaving **Start** unavailable, the estimate following what is typed, a suggested name that is already taken said so before anything is written, and a run's progress and its estimated time remaining shown while it goes | T1 |
| `tests/gui/widget/test_capture_panel.cpp` | The capture controls: the device list, a USB 2 device named as such and refused, each button reading as the next thing that will happen and turning green while monitoring and red while capturing without changing size — the layout shift a stylesheet on a button causes, because the size the stylesheet path computes is not the one the platform style chose — device and test mode locked while streaming, the destination fixed once the file is open while the duration and low-space settings stay live, test mode taking the name field away, and free space shown as how much capture it holds rather than as a size | T1 |
| `tests/gui/widget/test_update_page.cpp` | The whole update flow as a widget, driven against fakes with nothing plugged in — including branches a bench cannot be asked for. A verified bundle enabling the install and saying so, a development bundle bannered, a file that is not a bundle and one that is not there each refused with a reason, a bundle needing a newer application disabling the button, a successful install reporting what the device now runs, and each failure by name: a capture in progress, a corrupted transfer caught before anything is committed, a device that never comes back, and the wrong build coming back not being called a success. Plus a device with no firmware: named as being in recovery mode with both ways it gets there stated, offered **Program this device** rather than a repair, its version rows reading "None installed" and "Cannot be read", and a payload that is not firmware proved never to reach the device's memory | T1 |
| `tests/gui/widget/test_board_bringup_wizard.cpp` | The bring-up flow driven end to end with nothing plugged in: the step order asserted as data — the jumper page before the configure page, the configure page before anything is written, no power cycle in the middle of the writing — and the programming button refused until the FPGA is configured; both branches, where a board in its boot ROM skips two pages and a legacy board is held at the jumper until it comes back; the connectivity page's three failures; a release bundle that predates the bring-up payloads refused by name and a complete one accepted with its development banner; the configure writing nothing, then all three images written in order with the restart deferred; a stopped play; a power cycle nobody performed; and a programming step that failed offered again with the sentence saying nothing is broken; the status marks asserted as characters rather than as words, because a UTF-8 tick read a byte at a time is mojibake that every wording test passes over; the four states a packaged build's own bundle produces — preselected and named as such, refused when it does not verify, replaced by a chosen file and put back again, and a build carrying none saying which file to download; a finished step disabling and relabelling its own button while the line under it says **All done**, and a failed run's sentence surviving the polls and navigation that would otherwise replace it with an invitation to start; and **the power-cycle page refusing to report a cycle that has not happened** — the step before it leaves the FX3 running the bundle's firmware out of RAM, so a working Duplicator is already enumerating, and the page requires the device to go away and come back on the *application* image, with a partial cycle caught by the image role and a board back in its boot ROM told that J4 is still fitted | T1 |
| `tests/gui/widget/test_analysis_dialog.cpp` | The analysis dialog: pass and fail reported with the break's offset, pass and fail coloured differently through the theme tokens, an unreadable file distinguished from a failed one, the cancel button becoming the close button, and a dialog destroyed mid-analysis joining its worker rather than leaving a thread running into a destroyed object | T1 |
| `tests/gui/widget/test_statistics_panel.cpp` | That the figures reach the right labels: the four integrity states reading differently, a new run clearing the last one's numbers, a finished run leaving them up, the three capture-only rows blank while monitoring and filled in once a writer is attached, and the back-pressure bar — showing a working capture's buffer as half used rather than as nothing, carrying the reading's figures in its tooltip, and saying nothing at all rather than a confident zero when the gateware cannot report | T1 |
| `tests/gui/widget/test_waveform_panel.cpp` | The scope panel: the span choices reaching the plot, persistence off until asked for, the cursor reading in codes alone until a gain is declared, the plot painting empty, full and in persistence mode, and — counted in pixels a person could actually see — persistence leaving earlier sweeps on screen while its absence leaves only the latest | T1 |
| `tests/gui/widget/test_spectrum_panel.cpp` | The spectrum panel: averaging and peak-hold controls, an empty bin described rather than reported as -120 dBFS, the plot painting with and without a spectrum, both views offered with peak hold disabled where it would mean nothing, the frequency range defaulting past the filter's corner — and, in pixels, that the spectrogram draws a carrier as a visible band, that widening the range moves it down the frequency axis, and that it grows from the right over a fixed window of time rather than stretching to fill | T1 |
| `tests/gui/widget/test_amplitude_panel.cpp` | The amplitude panel: statistics becoming history points at the intended rate, the window's extremes, a new run clearing the history and a finished one keeping it — the property the gain design rests on — that correcting the declaration re-labels history already recorded instead of discarding it — the nominal-level marks and the RMS trace each drawn on both sides of 0 V — measured against the panel's own gridlines so the checks survive a change of margins — a Clear that resets the sampler along with the history, and a time span that can be set to everything held or narrowed to match the spectrogram — checked by measuring what is drawn at the oldest edge, because counting the whole plot passes against a panel that ignores the setting | T1 |
| `tests/hardware/test_device_capture.cpp` | An attached device: found at a usable speed, reporting a readable firmware commit, delivering at the ADC's rate and no faster, aborting within two seconds, streaming with no samples lost, and its test ramp arriving intact | T5 (`hil`) |
| `tests/hardware/test_player_hardware.cpp` | An attached LaserDisc player: found and identified on a real port at a real baud rate, the model ID checked against the badge on the front panel where the operator says what it is, the read-only queries a model's definition claims actually answered by the hardware that has them, and a whole examination of a real disc — completing, measuring the end of the side by seeking past it, and matching the type and the last address the operator read off the player's own display | T5 (`hil-player`) |

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

### 4.2 `fx3/programmer/` — 24 tests

| File | Covers | Tiers |
| --- | --- | --- |
| `tests/test_paging.cpp` | EEPROM and SPI flash paging arithmetic: page padding, I2C slave rollover at 64 KiB, transfer chunking, sector counts, and that the programming loop terminates and covers the image exactly | T1 |
| `tests/test_flashprog.cpp` | Locating `cyfxflashprog.img`: search order, the compiled-in install path, empty and missing `$FX3_FLASH_PROG`, a directory masquerading as the image, and that the returned string is owned by the caller | T1 |
| `tests/cli-contract.sh` | The command-line contract: that the help text does not promise SPI flash support the tool lacks, that it names the memory it actually writes, that removed options are neither advertised nor silently ignored, and that a bad device index is rejected rather than treated as device 0 | T2 |

`cli-contract.sh` is the odd one out: it runs the built binary and asserts on its *promises*
rather than its computations. It exists because two defects both survived for a long time —
help text claiming SPI flash programming the tool has never implemented, and a `-r` that
printed a message and slept. Nothing could fail while the words and the code disagreed, so
now something does.

These look like tests of trivial code, and are not. An off-by-one in the paging arithmetic
rolls the I2C slave address at the wrong offset and writes firmware bytes over the wrong
device — which bricks the FX3, recoverable only via the PMODE jumper. The path-resolution
tests guard a fix for every candidate path having been relative to the working
directory, so an installed binary could not find the secondary loader at all.

### 4.3 `fx3/firmware/` — three tests

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

### 4.4 `fx3/mkimage/` — 32 tests

| File | Covers | Tiers |
| --- | --- | --- |
| `tests/test_bootimage.cpp` | FX3 boot image construction: header fields, checksum range and wrapping, vector-area trimming, 64 KiB section splitting, `.bss` zero-fill, word alignment, ELF validation and rejection, and golden byte vectors | T1, T2 |

`fx3-mkimage` replaced the Cypress SDK's `elf2img`, and its acceptance check was a
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

### 4.6 `fpga/` — nine testbenches, a lint pass and two digest tests

Unlike every other component, the gateware has no `ctest` suite: there is no CMake here, and
the tools are a linter and a simulator rather than a compiler. The checks are Nix derivations
running the same scripts a developer runs, so the two cannot drift.

| File | Covers | Tiers |
| --- | --- | --- |
| `tests/tb_dataGenerator.v` | The test-pattern generator: the 0…1020 ramp over three periods, ADC passthrough and its one-cycle registration, that test mode ignores the ADC bus, and the sequence number — including the wrap after exactly 63 sequences of 65536 samples | T3 |
| `tests/tb_fx3StateMachine.v` | The GPIF II handshake: idle until asked, a packet of exactly 8192 clock cycles, that a single-cycle request is enough, the gap between back-to-back packets, and that a mid-packet reset abandons rather than resumes | T3 |
| `tests/tb_fifo.v` | The single-clock FIFO: order across a wrap, the show-ahead contract the GPIF read path depends on, the write-to-read bypass, occupancy at every boundary, and that a write to a full FIFO is discarded rather than corrupting the queue | T3 |
| `tests/tb_buffer.v` | The capture buffer: that `dataAvailable` is never a lie — including one word short of a packet — that it is held for exactly one packet, sample order across packet boundaries and across an overflow, the error flag's hold on the *second* overflow as well as the first, and that the instrument it carries sees the occupancy of the FIFO it is reporting on | T3 |
| `tests/tb_bufferMonitor.v` | The capture buffer instrument: that a peak belongs to its own interval and the lifetime peak and sticky overflow bit survive a read, that a stall is one event however long it lasts while the samples it cost are counted separately, that counters saturate rather than wrap and say so, that an event on the sampling edge is counted in the interval that is starting rather than lost or double-counted, and that the packet boundary and the near-full prescale are phases a reading does not shift | T3 |
| `tests/tb_spiRegisters.v` | The SPI register bank, driven at the fastest clock the specification allows: reset values, the identity block and the image role in one transfer, test mode and the LEDs written and read back, address auto-increment and wrap, unmapped reads returning zero, writes to read-only registers discarded, a byte cut short by chip select leaving nothing behind, the `0x20`–`0x23` window read and written, that `BRIDGE_DATA` alone does *not* auto-increment, the decoded-byte pulse the watchdog tickle is built on, and the capture buffer window at `0x40`–`0x56` — that a read of `TELEM_ID` samples the instrument exactly once, that a write there and a read that reaches it by auto-increment do not, that the geometry can be read without sampling, and that a second bank compiled with the window off reads it as unmapped | T3 |
| `tests/tb_flashBridge.v` | The EPCS pass-through: inert while locked, an interrupted unlock sequence that cannot be completed by a later stray write, the sequence that does unlock it, a real read answered by a model of the EPCS64 — so the mode-0 edges have to be the right way round — and relocking by write and by reset | T3 |
| `tests/tb_bootLoader.v` | The factory image's boot decision, built as its top level wires it and read through the bridge from a model of the EPCS64: a valid boot block arms the watchdog with the right address and *then* reconfigures, and the wrong magic, a bad block checksum, a damaged image and an unknown layout version each leave the unit in the factory image | T3 |
| `tests/tb_crc32.v` | The boot block's checksum against the published CRC-32 check value, so what the gateware computes is what a host's library computes, plus the restart the boot logic depends on between its two runs of bytes | T3 |
| `tests/run-lint.sh` | `verilator --lint-only -Wall` over the thirteen hand-written modules, across both images and the half they share | T4 |
| `tests/run-sdc.sh` | Both images' timing constraints: that they parse as Tcl, and that they name every pin the top level maps | T4 |
| `tests/run-version.sh` | The commit-to-identity-register stamp: an eight-character hash, the seven-character one a Nix build passes, a dirty tree, a build with no commit, a full-length hash, and a string that is not a hash at all | T2 |
| `tests/test_provenance.py` | The byte offsets the canonical bitstream digest masks, that a payload change is *not* masked, and that a moved field raises rather than digesting unmasked data — and the same for the `.svf`, whose header names the time its input file was last written and is therefore the one line in it that a rebuild changes | T1, T2 |
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
checks that asking for it works, which is §8's remaining gateware item.

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
whole. See the caveat in §8.

Lint runs with `-Wall`, and everything it reports is either a failure or a waiver carrying
its reason in `fpga/verilator-waivers.vlt`. The waived findings — a blocking assignment in
sequential logic, two incomplete `case` statements, an implicit width promotion, unused
control-bus bits fixed by the PCB — are each pinned by one of the testbenches above rather
than merely declared benign.

### 4.7 Repository-wide — the licence-header and update-bundle checks

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
same files. Without that, a local run would header-check every `moc_*.cpp` in `ddd-gui/build/`.

#### `update-bundle`

Every commit assembles a real update bundle with `tools/make-update-bundle.sh` — a
firmware-only, development-signed one over a synthetic payload — and takes it apart again.
It checks that the entries come out in the order the format fixes, that the signature
verifies, that the manifest's digest matches the payload, and that assembling the same
inputs twice gives byte-identical files.

Everything after the bundle exists is done with **stock tools**: GNU `tar` lists it,
`minisign` verifies it, `sha256sum` checks the digest. That is the point of the check.
The application's own reader is covered by the tests in §4.1, and a check that used this
project's reader to validate this project's writer could only ever say that the two agree
with each other.

Nothing here writes to a device. The payload is a text file, and installing a bundle stays
a deliberate human act (AGENTS.md §4).

### 4.8 Everything else — nothing yet

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
   `nix build .#ddd-gui` — or `nix develop .#ddd-gui` and build from source.
2. Connect the Domesday Duplicator. Confirm it enumerates as `1209:2347`
   (`lsusb | grep 1209`). If it appears as `04b4:...` it is still in bootloader mode and has
   no firmware loaded.
3. Launch the capture application and **enable test mode**.
4. Capture for **at least 60 seconds**. This matters: the buffer must wrap several times, and
   a short capture can pass while a real one drops samples.
5. Analyse the capture: **Edit → Analyse test data...**, or from a shell,
   `DomesdayDuplicator --analyse-test-data <file>` — which exits 0 for an intact ramp, 1 for
   a break and 2 for a file it could not read, so the gate can be scripted.

   `ddd-gui` offers the same check under **Tools → Analyse test data...** and as
   `ddd-gui --analyse-test-data <file>`, with the same three exit codes and the same
   wording. The two applications have been checked side by side on the same files for all
   three verdicts — pass, break, and too short for the ramp to have wrapped — and agree on
   the code, the sample offset and the expected and actual values.
6. **Pass = zero sequence breaks.** Any break at all is a release blocker, not a flake — the
   ramp is deterministic, so there is no such thing as an intermittent false positive here.
7. **Cross-check the device's own account against the host's.** The end of every run is
   logged as, for example, `Device buffer: peak back pressure 0%, peak 8194 words, 0
   overflows, 0 samples dropped`. Two things are being checked here, and they are the two
   halves of one claim:

    - On a clean run the peak is **8194 of 16384 words** and the drop count is **zero**. A
      higher peak means the host was late even though nothing was lost, which is worth
      knowing about a machine before it is trusted with a disc. A peak reported as `0`, or
      the row reading *Not reported by this gateware*, means the instrument is not being
      read at all — a gateware that predates it, or a device that never answered.
    - On a run that *did* drop samples, the gateware's `samples dropped` and the ramp
      analysis's gap must agree. They are independent instruments watching the same event
      from opposite ends of the cable, and a disagreement means one of them is wrong.
      Producing such a run deliberately — a slow destination volume, or the highest FLAC
      compression level on a loaded machine — is the only way to check this half, and it is
      worth doing once per gateware change rather than once per release.

### The decimated path

The gateware can halve the sampling rate (`DECIMATION`, register `0x12`), and the decimator
sits in front of the test-data generator — so a test capture at 20 Msps is an unbroken ramp
exactly as one at 40 Msps is, and **step 4 must be run at both rates**. It is the same
procedure with **Sample rate** set to *20 MSPS for VHS* for the second pass, and the same pass
condition.

That covers the plumbing: that the decimated stream is contiguous, that the sequence counter
is attached to the samples that survive, and that the buffer and the FX3 handle the halved
rate. It does **not** cover the filter, which the ramp cannot exercise — a ramp is not a
spectrum, and every sample of it is in the passband. The filter's response is checked by
`fpga/tests/tb_halfBandDecimator.v` in simulation and by
`fpga/make-halfband-coefficients.py --response` in arithmetic; measuring it on hardware means
a signal generator and a spectrum analyser on the RF input, which is a bench measurement
under §5's *What it does not cover* rather than part of this procedure. What those two checks
between them pin down — the response, the ripple, the stopband and the group delay — is set
out on [The decimation filter](docs/content/development/fpga-decimation-filter.md).

### When to run it

- Before any release.
- After **any** change to gateware, FX3 firmware, or the host capture path — including
  changes that look purely cosmetic. A build that compiles proves nothing about the data path.
- After a Quartus version change, which alters synthesis and therefore timing.

### What it does not cover

Analogue performance. The test pattern is generated *after* the ADC, so it proves the digital
path is lossless and says nothing about gain, filtering or noise. Those remain manual bench
measurements against the calculations in `hardware/doc/`.

## 6. The device update procedures (T5)

Everything below writes the FX3's boot EEPROM. **Nothing automated does this** (AGENTS.md
§4): each step is a deliberate human act, and this section exists so that it is the same
deliberate human act every time.

You need a Duplicator and a USB 3 port. One step — provisioning a unit running firmware
that has no update agent to receive one (U0) — needs the J4 jumper and
`fx3-programmer`.
Everything else is done from the application, which is the whole point of the mechanism,
and that now includes recovering a unit whose update was interrupted (U5) and bringing up
a kit that has never been programmed at all (U6).

The B-series items go further and write the **FPGA's** configuration flash as well, using
the DE0-Nano's own USB-Blaster to configure it first. Those need both cables and the case
off. The order is enforced in the engine rather than left to the procedure: the FX3 reaches
its boot ROM before the FPGA is touched and only ever runs the new firmware afterwards, so
the pairing that would put two drivers on one net is unreachable rather than merely
avoided.

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

### U0 — programming a device that cannot yet update itself

A unit with no update agent has no `0xD0` to answer, so the application says so rather
than pretending. It needs the jumper once, and only once.

In the field that means the **original** firmware, which is what `main` still carries and
what every unit is running today: no version of the current firmware has been released, so
there is no intermediate generation to meet. This is the bench route; the application does
the same job from **Tools ▸ Firmware ▸ Bring up a new or legacy board…** (B1), and that is
what a user is told to use.

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

1. Attach the device. Confirm **Tools → Firmware → Update firmware…** reports the commit
   you expect — the one U0 flashed, not the one in the bundle.
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
5. Re-open **Tools → Firmware → Update firmware…**. The versions page must agree with the
   confirmation.

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
2. Open **Tools → Firmware → Update firmware…**. The page says the device is in recovery
   mode, that its firmware is missing, and that it is not damaged. The firmware row reads
   **None installed** and the gateware row **Cannot be read**.
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

Use a SuperSpeed Explorer Kit that has never been programmed, or erase one deliberately
with `./tools/blank-board.sh --fx3` — see *Getting a blank board* below.

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
- G1 after any change to `epcs-flash.c`, `remoteUpdate.v`, `bootLoader.v` or
  `flashBridge.v` — and after any change to how the `.rpd` is generated, because the byte
  orientation it is emitted in is load-bearing (§6, defect 5).
- B-V1 after any change to `svf_player.cpp` or `usb_blaster_cable.cpp`, and after any
  change to the `quartus_cpf` invocation that emits the `.svf` — the declared frequency in
  particular, which is what every wait in the file is counted in.
- B0 and B1 before any release that changes `bringup_orchestrator.cpp` or the bring-up
  wizard, and once per release that changes what the bundle contains.
- B2 whenever `ddd-gui/packaging/bundled-update.env` changes, and after any change to
  the packaging workflows or to `bundled_update.cpp` — every part of that path is a
  claim about a machine other than the one it was built on.

### Getting a blank board

U6, B0, B1 and B2 all need a board that has never been programmed, and B2 needs one per
binary package the release produces. Keeping a virgin board for each is not a plan; erasing
one is:

```bash
nix develop .#fpga-quartus --command ./tools/blank-board.sh
```

It writes `0xFF` over the whole FX3 EEPROM and erases the EPCS64, leaving the pair as an
unprogrammed one: the FX3 falling back to its boot ROM at `04b4:00f3`, the FPGA
unconfigured. `--fx3` and `--fpga` do one half each; the FX3 half needs no Quartus and so
no shell. Both halves are verified by readback, and the script says what to build if it
cannot find `fx3-programmer` or a `.jic`.

**Power cycle before testing against it**, and check J4 is removed. The script leaves the
FPGA running Altera's serial flash loader and the FX3 in the Cypress flash-programmer mode,
and a board with J4 fitted never reads its EEPROM at all — which looks identical over USB
and is not the same test.

The one thing it cannot prove is that the boot block at `0x100000` is gone: the JTAG erase
is page-selective by default and steps over it, so the script passes `--erase_all` and
prints how long the erase took, a whole-device one taking tens of seconds where the
page-selective one takes about four. Nothing can read that sector back once the firmware
is erased. **If a board provisioned after a blanking comes up in the application image
rather than recovery, the boot block survived** — that is the symptom, and it is the one
thing about these items a stale board would quietly change.

### G0 — provisioning a unit with the dual-image flash

Performed first on 2026-08-15. Needs the USB-Blaster and `nix build .#bitstream`.

1. Check the memory map beside the `.jic` before programming anything:
   `provisioning/DomesdayDuplicatorProvisioning.map` must place the factory image at
   `0x000000` and the application image at `0x200000`, with the gap at `0x100000` for the
   boot block. Those addresses are the layout; a `.map` that disagrees means the `.cof`
   has drifted and nothing downstream is meaningful.
2. `jtagconfig` must name the USB-Blaster and the EP4CE22.
3. From a writable copy of the provisioning directory:
   `quartus_pgm DomesdayDuplicatorProvisioning_write_jic.cdf`. Eleven seconds: erase,
   blank-check, program, CRC verify. The programmer also prints the EPCS silicon ID —
   `0x16`, which must match `UPDATE_EPCS_ID_EPCS64` in the firmware.
4. Power-cycle. **The erase is page-selective**, so what the unit boots into depends on
   what the boot block sector held before: a unit whose flash never carried a boot block
   comes up in the factory image with `IMAGE_ROLE` reading `0x00` and the application
   showing *recovery gateware*; a unit reprovisioned over a working installation keeps
   its boot block, and if the page content still matches the block's CRC it boots
   straight to the application. Both are correct.
5. The programmer leaves the FPGA running its flash loader, not this project's gateware,
   so the first thing after any JTAG programming is a power-cycle. An update attempted
   before it is refused with *"the FPGA is not answering"*, which is the gate doing its
   job.

### B-V1 — programming the flash with no Quartus (JTAG vectors over the on-board cable)

**Partly performed, 2026-08-17 — and it found a defect the whole tier exists to find.**
The gating item for the board bring-up work: it is what decides whether the application
can provision a board itself, and it is the only thing about that path a bench can settle.
Everything else is covered at T1 with nothing attached (`ctest -R jtag`, 61 tests), and no
test could have answered the question below.

**What the first cable session established.** The cable was pointed at a DE0-Nano for the
first time as part of a full bring-up run, and the play stopped at the first `TDO`
comparison in the file — `SDR 4 TDI (0) TDO (0) MASK (F)`, answered `F`. Bisected with an
IDCODE probe, a seven-statement file that shifts 42 bits and writes nothing:

| Read in | Answer |
| --- | --- |
| bit-bang mode, one cycle at a time | correct |
| byte-shift mode | `FF` for every byte — no information at all |

Reading a Cyclone IV's IDCODE with the first 24 bits byte-shifted and the last 8
bit-banged returned `02FFFFFF` against the expected `020F30DD`: the top byte, which came
from the bit-bang cycles, is right. So **byte-shift *shifting* is proved correct** — the
shift had advanced exactly 24 places — and only its read-back is missing.

`usb_blaster_cable.cpp` now never byte-shifts a scan whose TDO is captured, and the
mechanism behind the cable's silence is unexplained rather than understood. What makes
that an acceptable place to stop: of this project's 73,297,811 shifted bits, **103 are
read** — one ten-thousandth of one per cent — so the fast path still carries everything
that takes time. An IDCODE probe is the cheapest proof a JTAG cable works at all and
should be step 1 of this item from now on.

**And a second finding, larger than the first: the provisioning `.svf` is not
self-contained.** After the cable was fixed the play stopped again, at the first
identification read, answering `F` to everything. The file speaks **Virtual JTAG** —
`SIR 00E` is USER1, `SIR 00C` is USER0 — to Altera's **Serial Flash Loader**, a soft
design that has to be configured into the FPGA before any of it means anything. The file
carries no configuration of its own: its largest scan is 2,108 bits, and an EP4CE22
configuration is 5,748,760. `quartus_pgm` loads the loader from its own installation
before playing this protocol; `quartus_cpf` emits only the second half of the job.

Proved by supplying the missing half by hand:

```
quartus_cpf -c -q 4.5MHz -g 3.3 -n p \
    $QUARTUS_ROOTDIR/common/devinfo/programmer/sfl_enhanced_01_020f30dd.sof \
    provisioning-loader.svf
ddd-jtag provisioning-loader.svf     # 5,749,532 bits, 2.6 s, volatile
ddd-jtag <the identification block>  # now plays clean
```

The loader image is chosen **by IDCODE** — `020f30dd` is the EP4CE22 — and the variant
matters: `sfl_ep4ce22.sof` configures a loader reporting node `00206E00`, and this file
expects `18206E00`, which is `sfl_enhanced_01_020f30dd.sof`. `jtagconfig -n` names the
node it finds, which is the quickest way to tell the two apart.

**Decided 2026-08-17: the loader is not used, and this project's own factory image is
configured instead.** The two options were —

- **The loader is Altera's.** Putting it in the update bundle means redistributing an
  Altera artefact, which is the same question the Quartus caching position answers for the
  toolchain (see the licence position in *Release pipeline*). Rejected: the second option
  works and needs no answer to it.
- **There may be no need for it at all.** The alternative is to JTAG-configure the FPGA
  with *this project's own* factory image — volatile, one 5.7 Mbit scan, our own artefact —
  and then write both flash images through the **existing** firmware update path over USB,
  which G1 has already bench-proved. That would replace the SVF flash-writing half
  entirely, and the JTAG side would only ever configure.

  **Tried on the bench, 2026-08-17, and it works.** `DomesdayDuplicatorFactory.sof`
  converted the same way gives a 1.4 MB `.svf` that plays in **2.6 seconds**, and what
  followed is the designed boot flow arriving by a different road: the factory image came
  up, validated the application image in the flash, and handed over to it — `jtagconfig`
  shows the Virtual JTAG node gone, and the register block answers over the FX3's SPI link
  with `IMAGE_ROLE 0x01` and the application's commit. On a board with nothing valid in
  flash there is nothing to hand over to, so the factory image stays resident with its
  bridge, which is the bring-up case.

  What it still needs is the one thing the firmware refuses: writing the factory region at
  `0x000000` (`update-protocol.h`, *"never written from here by any path"*). The host
  cannot do it instead — the bridge takes one byte per USB control transfer, so 8 MB is
  millions of round trips, which is exactly why the firmware shifts the bytes itself.

  **The freeze is lifted for that one path**, decided on the ground that a DE0-Nano
  carries its USB-Blaster on the board, so the cable that recovers a half-written factory
  region is present on every unit and already connected during bring-up. The firmware
  gains a third update target guarded by a magic in the begin flags.

**Still to perform**: the flash write itself, its duration, and the comparison against
`quartus_pgm` — steps 4 to 6 below, which cannot run until the firmware carries that third
target.
Nothing has yet written a byte to an EPCS by this route. When they do run, steps 2 and 4
change with the design: the file played is the factory *configure* `.svf`, and the write
that follows is an ordinary update transfer, so what is being timed is a ~212 KB flash
write of the kind G1 already measured at 17 seconds rather than a 105-second vector play.

What is already known without a board, measured on the CI-built provisioning content
(Quartus 25.1, 2026-08-17):

| | |
| --- | --- |
| `.svf` emitted beside the `.jic` | 18.4 MB (the `.jic` is 8.4 MB) |
| the same file gzipped | 251 KB — it is verbose hexadecimal text |
| the same content as `.jbc`, the recorded fallback | 538 KB |
| what one run asks for | 37,140 statements, 73.3 Mbit shifted, 471.9 M idle clocks |
| what the idle clocks stand for | ~105 seconds of waiting, at the 4.5 MHz the file declares |
| traffic that implies in byte-shift mode | about 68 MB over a full-speed USB link |

The procedure, on a DE0-Nano whose flash content does not matter:

1. **Read the IDCODE first**, before any file that writes anything. The probe is seven
   statements and shifts 42 bits:

   ```
   TRST ABSENT;
   ENDIR IDLE;
   ENDDR IDLE;
   STATE RESET;
   STATE IDLE;
   SIR 10 TDI (006);
   SDR 32 TDI (00000000) TDO (020F30DD) MASK (0FFFFFFF);
   ```

   `ddd-jtag <probe>.svf` must report success. A failure here is the cable or the driver
   and nothing else; the top nibble is masked because it carries the silicon revision.
   This step exists because the first cable session skipped it and spent a bring-up run
   discovering what these 42 bits say in a second.
2. `nix build .#bitstream`, and take `provisioning/DomesdayDuplicatorProvisioning.svf`
   from the result. Confirm the application can read it at all, with nothing plugged in:
   `ddd-jtag --dry-run <file>.svf` must play every statement and report the counts above.
3. Connect **only** the DE0-Nano's mini-USB. Stop Quartus's `jtagd` if it is running — it
   holds the cable open, and the failure to claim it is the first thing this can go wrong
   on.
4. `time ddd-jtag <file>.svf`. **Record the wall-clock duration**; it is the number the
   provisioning flow's progress estimate and its page wording will be built on, and it
   cannot be obtained any other way.
5. Power-cycle the board, then confirm the flash content the way G0 does: the unit comes
   up in the factory image with `IMAGE_ROLE` reading `0x00` (or in the application image
   if a boot block survived — both are correct, and for the same reason as in G0).
6. Re-run G0 with `quartus_pgm` on the same board and confirm the two routes leave it in
   the same state.

**Pass** = the run completes with no mismatch reported, and the board afterwards is
indistinguishable from one provisioned by `quartus_pgm`.

The three things only this can settle:

- **Which half of the cycle TDO belongs to.** ✅ **Settled 2026-08-17.** The cable is told
  to sample TDO with TCK low, on the reasoning that a TAP updates it on the falling edge;
  reading after the edge instead would shift every answer along by one bit. The IDCODE
  probe above returns the right value, so the choice is right — and the failure it did
  surface behaved exactly as predicted here, loudly and at the first `TDO` comparison,
  long before anything was written.
- **How fast the cable actually clocks.** Every wait in the file is a cycle count worked
  out at the declared 4.5 MHz, so a cable clocking faster would shorten them all. The
  player holds each wait open for the time its count stands for, which makes the run
  correct whatever the cable does — but the *duration* in step 3 is what says how much
  that costs, and whether the declared rate should be raised to reduce the cycles.
- **Whether the FT245 framing is right in the direction that matters.** Two status bytes
  are dropped from the front of every packet the chip sends, checked against fixtures at
  T1; only a real cable proves the packets arrive as the fixtures say.

If the run is absurdly slow, the recorded fallback is the `.jbc` interpreter — more code,
the same seams, and a file a third of the size. Verify first, build second.

### The interconnect direction, and why it is not a bench item

**Recorded, not performed.** This was once a gating oscilloscope measurement (B-V0),
protecting against contention on `CTL_07`. It is not one any more, and the reason is worth
keeping: the bad pairing became unreachable rather than merely avoided.

**The FX3 is in its boot ROM while the FPGA changes.** Bring-up reaches the boot ROM first,
by jumper, and in the boot ROM every GPIF and GPIO pin is unconfigured and undriven — so
the gateware can become current underneath an FX3 that is driving nothing at all.
`BringUpOrchestrator::ProgramDevice` refuses until `ConfigureFpga` has succeeded, before
the boot ROM is so much as opened, and a unit test and a widget test hold it.

**And the original firmware never runs again afterwards.** By the time any firmware
executes it is the firmware out of the bundle, and there is no flow that puts the original
back — so no sequence of pages, stops or reruns can produce the pairing:

| | What the FX3 is running | What the FPGA is running |
| --- | --- | --- |
| Before the jumper page | whatever it was | whatever it was — a shipped pairing |
| While the FPGA is configured | boot ROM, J4 fitted — **drives nothing** | becoming current |
| While the three images are written | firmware from the bundle | current gateware |
| After the one power cycle | current firmware | current gateware |

The one mixture the flow ever *runs* is **current firmware over current gateware**, which
is what a working unit is. The directions the reading rests on:

| | `CTL_07` / `GPIO_24` |
| --- | --- |
| Original gateware (`97f7dec^:fpga/src/DomesdayDuplicator.v:96`) | FPGA **input**, unused |
| Original firmware (`97f7dec^:fx3/firmware/src/domesday-duplicator.c:268-279`) | FX3 **output**, actively driven |
| Current gateware (`fpga/application/DomesdayDuplicator.v:90,177`) | FPGA **output** — this is `spi_miso` |
| Current firmware | FX3 **input** — it reads MISO |

Original firmware over current gateware is the pairing that must not happen: two output
stages on one net, bounded by the Explorer Kit's 22 Ω series resistors to roughly forty
milliamps — past the per-pin DC maximum of both dies, and sustained rather than transient,
because the original firmware claims the pin and holds it. Current firmware over original
gateware is the safe mixture: the net is driven by nobody, which reads as noise, which is
exactly the diagnosis the application wants ("this gateware has no register interface").

**If anyone ever puts a probe on it**, three things are worth measuring while it is there,
and none of them gates anything:

1. that `CTL_07` floats on a unit running current firmware over original gateware — any
   board part way through a bring-up — and does not on a finished one, where the FPGA
   drives MISO;
2. the original GPIF II configuration as well as the GPIO overrides above. The GPIF state
   machine drives `CTL` lines of its own, outside the overrides, and that is the half of
   this a source reading is most likely to have missed;
3. the ringing on `FPGA_SCLK` during an ordinary register read. The *FPGA register
   interface* page used to list series termination as a remedy needing a hardware change;
   the Explorer Kit already provides 22 Ω on every `CTL` line, so what is left to try if
   ringing is ever a problem is a shorter list than that page said.

Should the reading turn out wrong, the ordering does not change — it is the right way round
regardless, and it costs nothing — but the wizard's diagnosis of an unresponsive register
interface would need re-reading.

### B0 — bringing up a bare pair from the application

**Not yet performed.** Needs B-V1 settled first, and a never-programmed FX3 kit with a
DE0-Nano carrying Terasic's demo bitstream.

1. Build a bundle: `nix build .#fx3-firmware .#bitstream`, then `./tools/dev-bundle.sh`,
   which collects all four payloads and signs them with the development key. It prints
   which ones it found; a run that names a missing factory pair cannot do this item.
2. Open **Tools ▸ Firmware ▸ Bring up a new or legacy board…** with both cables connected
   and the unit out of its case.
3. Work the wizard through. On this board the connectivity page should report the FX3
   *waiting in its boot ROM* and skip both jumper pages — **7 of 9 steps, not 9**.
4. Record the wall-clock duration of the configure step and of the programming step
   separately, and compare each against the estimate its page printed. Both estimates are
   deliberately pessimistic; a page that promised five minutes and took twelve is a defect
   in the estimate, not in the run.
5. On the **power cycle** page, confirm it does *not* report success before you touch
   anything. The step before it hands the firmware to the FX3's boot ROM and runs it out
   of memory, so a fully working Duplicator is enumerating at that moment — a page that
   ticked here would be reporting a power cycle that had not happened, which is exactly
   what it used to do.
6. At the end, the verification page must show all four ticks, with the gateware line
   reading the **application** image — the factory image validated it at power-on and
   handed over. A board that comes back on the factory image is a failed run, not a
   partial success.
7. Then T5, with no update in between: there is nothing left to install.

**Pass** = the wizard reaches its last page with every check ticked, and the unit passes
the capture-integrity procedure without anything else being installed on it.

### B1 — bringing up a legacy unit

**Not yet performed.** The same, on a unit running the original firmware (`1d50:603b`).

The differences from B0, and they are the point of running it separately:

1. The connectivity page must name the board *running the original Duplicator firmware* —
   not report it as absent, and not call it broken.
2. The flow is **9 of 9 steps**: this board is running firmware, so it has to be sent to the
   jumper.
3. After fitting J4 and pulling **both** cables, the jumper page must notice the boot ROM
   appear by itself.
4. On the **power cycle** page, deliberately pull only the USB 3.0 cable first. The board
   stays lit from the mini-USB and comes straight back as a working Duplicator — and the
   page must **refuse it**, saying *the board did not lose power*, because the FPGA is
   still holding the image JTAG put there rather than one loaded from flash. Then pull
   both and confirm it completes. This is the failure the whole flow is worded around, and
   it is the one worth provoking.

**Pass** = as B0, plus the wording checks above.

### B2 — the offline bring-up, from an installed release build

**Not yet performed.** What the bundled update file exists for, and the only way to find
out whether it works: every part of it — the packaging pin, the fetch by digest, the
install layout, the search — is a claim about a machine that is not the one it was built
on.

Needs a release build of the application (a Flatpak, an MSI or a DMG from a `gui-v*` run
whose pin was not empty), a machine it has never been installed on, and the same bare pair
B0 uses.

1. Install the packaged application on a machine with **no network at all** — unplugged or
   with the interface down, not merely "not used". A path that quietly fetches something
   would otherwise pass here and fail in the field.
2. Open **Tools ▸ Firmware ▸ Bring up a new or legacy board…** and go to the image page.
   It must arrive with a file **already chosen**, naming the version and commit the pin
   points at, and saying it was checked.
3. Confirm the page names the same version the pin does. A stale pin is the likeliest
   failure here and it is invisible from inside the application.
4. Work the wizard through as in B0. **The network stays down for all of it** — that is
   the whole item, and it is stronger than it used to be: bring-up now finishes the job,
   so there is no ordinary update afterwards to bring the network back for.

**Pass** = the wizard preselects the bundled file on a machine that has never downloaded
anything, and the board is fully current at the end of it, offline throughout.

**Also worth doing once**: rename or delete the installed file and reopen the wizard. It
must say this build carries none and name the file to download — not offer a file it cannot
read, and not fail silently.

### G1 — the gateware update, and the handover it ends in

The same wizard/`ddd-update` flow as U1/U2 with a bundle carrying gateware. What is
different from the FX3 target: the erase pauses the transfer message warns about, the
multi-minute estimate, and the reboot at the end being a *reconfiguration* — the factory
image validates what was written and hands the FPGA over to it.

**Pass** = after the update's restart the gateware row shows the bundle's commit,
`IMAGE_ROLE` reads `0x01`, and a capture runs. A power-cycle then proves the cold path:
factory validates the image over the bridge (about a quarter of a second for a compressed
image), arms the reconfiguration block, and hands over — the unit must come up in the
application image with no cable and no host.

Measured on first performance (V6): a 212 KB compressed image sends in **17 s**
(~12.5 KB/s against the estimate's deliberately pessimistic 2 KB/s) and the device-side
readback verify takes **59 s** (~3.6 KB/s). The readback dominates, and its cost lives in
the frozen factory image's bridge — known before the freeze, which is what V6 was for. An
uncompressed image (719 KB) scales linearly: 57 s send, 200 s verify.

### What the first bench session established, and how

The handover did not work when first performed, and the defects it surfaced are worth
recording because every one of them passed every host-side test. All five are now fixed
and pinned — the first four by testbenches, the fifth by the `.cof` comment:

1. **All three reconfiguration parameter numbers were wrong** (`remoteUpdate.v`): the
   boot address was written to the read-only state register and the watchdog timeout to
   the early-CONF_DONE bit. Corrected against Table 17 of the Remote Update IP User
   Guide; the input register was then read back on hardware to prove each write lands.
2. **The `Osc_int` and `Cd_early` option bits were never written.** The handbook requires
   the factory configuration to set both; the simulation model now records them and
   `tb_bootLoader` fails if they are not set.
3. **The boot loader never relocked the flash bridge**, leaving the fabric driving the
   AS pins into the handover. The firmware always had this discipline (`epcsLock()`);
   the gateware gained a `StateRelock`, and the testbench asserts the pins are released
   before the reconfiguration request.
4. **The reconfigure and tickle strobes were 200 ns** against the handbook's 250 ns
   minimum. Now 800 ns.
5. **The `.rpd` bit orientation — V7.** The AS engine consumes configuration bytes
   LSB-first while SPI delivers MSB-first, so the flash must hold each byte
   bit-reversed. `quartus_pgm` performs that reversal when programming a `.jic`; the
   update path writes the `.rpd` verbatim. With `rpd_little_endian=1` every image the
   updater wrote was bit-backwards on the wire — the fabric's own CRC verify passed,
   because it read back exactly the bytes it wrote, and only the AS engine ever saw the
   garbage. Found with a logic analyser on the EPCS pins: the page that boots delivers
   `56 EF EF …` and the updater's page delivered `6A F7 F7 …` — the same header,
   reversed. The application `.cof` now sets `rpd_little_endian=0`, making the `.rpd`
   wire-true, and the comment beside it says why that value is load-bearing.

The trace that settled it also settled **V7's other half and V4**: the engine's preamble
reads the flash's electronic signature (`0xAB` → `0x16`) before every load, and the
attempt itself was a FAST_READ (`0x0B`) at exactly `0x200000` — the staged address,
committed and used. The **watchdog tickle** is proven by the application image surviving
past the ~54 s timeout with the watchdog enabled: the FX3's register traffic resets it.

Diagnostics added during the session and kept: registers `0x30`–`0x37` expose the
reconfiguration block's own account of the boot — MSM mode, the previous attempt's
trigger condition, and every field of the staged update register, read back after the
writes. Signature `0xDD` in the top byte distinguishes "gateware without the instrument"
from "no answer". They cost microseconds at boot and they are how the parameter defects
were measured rather than guessed.

### What it does not cover yet

The interruption cases for the gateware target: power pulled mid-write (which must leave
the previous image bootable, since sectors are erased as the write enters them), power
pulled during the readback verify, and the boot block sector erased by JTAG to force the
fall-back from the other direction. Those come after the clean path they interrupt, and
the clean path has now been shown once.

The watchdog period is still the 12-bit maximum (~54 s at the internal oscillator's
nominal rate) rather than a measured margin over the worst-case FX3 boot — it must be
narrowed before the factory image is frozen, and V5's double-configuration timing should
be measured properly at the same sitting. And the boot logic still has no guard against
an application image that validates, configures and is nonetheless dead: such an image
would cycle factory→application forever at about three seconds a lap. `Cd_early` now
probes the image before every attempt, which narrows the window to exactly that case,
but the deliberate second-attempt refusal the plan calls for remains open.

## 7. The player control procedure (T5)

Driving a LaserDisc player over its serial port, and capturing a side without a hand on the
front panel. The automated half of this is `ctest -L hil-player` (§3), which connects,
identifies and examines a disc. This is the other half: the per-model checklist that no
amount of scripting can stand in for, because most of it is *watching the player* rather
than reading what the application believes about it.

**Why so much of it is manual.** A control that is enabled and does nothing, a control that
does something other than its label, and a control that works perfectly all look identical
from inside the application: in every case a command went out and an acknowledgement came
back. The only instrument that can tell them apart is a person looking at the player. That
is the whole content of P1 below, and it is why a model stays `bench_verified = false`
until somebody has walked it.

Everything about the *logic* is already covered at T1 with nothing plugged in — every
refusal, every open tray, every link that dies halfway through, every branch of the
automatic capture. What remains for hardware is whether the bytes this project believes it
is sending mean, to a real player, what its manual says they do.

### What to have ready

- **The player**, and its manual. The manual matters for P1: a control the application
  leaves greyed out is only a fault if the model actually has it, and that is the one
  direction the application cannot check itself.
- **A serial cable that suits the player.** Pioneer industrial players take a straight-
  through cable to a PC; several of the consumer decks want a null-modem. If nothing at all
  answers at any rate, this is the first thing to doubt.
- **A CAV disc and a CLV disc**, both with the last address of the side known — read it off
  the player's own display by seeking to the end, rather than off the sleeve, since the
  sleeve is the programme and the measurement is the disc.
- **A disc with two recorded sides**, for the side check in P2.
- **A Domesday Duplicator** as well, for P3 onwards. The automatic capture writes a file, so
  those steps need both pieces of hardware and enough free space for a whole side.
- **A machine whose serial permissions are settled** — see immediately below, because
  nothing else here can start until they are.

### Serial port permissions, which is where this usually stops first

Not a step so much as the thing that blocks step one, on every platform and for a different
reason on each. The application detects a refused port and says which of these applies; this
is the same information in the place somebody looks when the application will not start.

- **Linux.** The serial devices belong to a group — `dialout` on Debian, Ubuntu and Fedora,
  `uucp` on Arch and its derivatives. `sudo usermod -a -G dialout $USER`, then **log out and
  back in**: a group granted to an account does not apply to a session that already exists,
  which is why "I added myself and it still does not work" is the commonest follow-up.
  A USB serial adapter is third-party hardware and this project does not ship a udev rule
  for one — [nix/modules/udev.nix](nix/modules/udev.nix) covers the Duplicator and nothing
  else. That is deliberate: writing rules for other people's devices is not this project's
  business.
- **Flatpak.** The manifest carries `--device=all`, which is what grants `/dev/ttyUSB*` and
  `/dev/ttyACM*` inside the sandbox; there is no narrower static permission that reaches a
  serial port and no portal for one. It is the same permission the USB capture path already
  needs. The sandbox keeps the user's supplementary groups, so the group membership above is
  still required — a Flatpak install does not route around it.
- **macOS.** Not a permission problem in the Unix sense at all: `/dev/cu.*` is open to
  everybody. What goes wrong instead is the driver. An adapter using the built-in FTDI or
  CH34x support appears as `/dev/cu.usbserial-…` and needs nothing installed; one needing a
  vendor driver needs that driver *allowed* in **System Settings → Privacy & Security**
  after installation, and until it is, its port either does not appear or will not open.
- **Windows.** No group to join, and a COM port can only be held by one program at a time —
  a terminal left open on it is the usual cause. Check **Device Manager → Ports (COM & LPT)**
  for the adapter's driver and its port number, and confirm the number the application's
  scan shows is the number Device Manager shows.

### P0 — connect and identify, at every rate the player offers

From a configuration that has never seen this player: player control off, no remembered
port, nothing excluded.

1. Set the player's rate to the first position its switches offer, switch it on, enable
   player control and let auto-detection run.
2. Confirm the model name and firmware revision the panel shows are the player's own. An
   unrecognised player is not a failure — it connects on the generic Pioneer set and reports
   the model code it answered with, which is exactly what a new definition needs — but it
   *is* the finding that this bench session should end in a new header
   ([`ddd-gui/src/player/players/README.md`](ddd-gui/src/player/players/README.md)).
3. Repeat for **each rate the player's own switches support**, fixing the rate in the
   settings each time and then, separately, letting auto-detection find it unaided. Both
   paths matter: the fixed one is a single attempt with a long timeout and the search is
   several short ones, and a player can answer one and not the other.
4. With a rate that works, confirm the port is remembered: restart the application and watch
   that it reconnects without scanning. On a configured machine no other port is ever
   opened, which is the promise the risks section of the plan makes and this is where it is
   checked.
5. Power-cycle the player mid-session. It must reconnect on its own.
6. Unplug the adapter mid-session. The application must report it **once** and settle — not
   spin, and not report it repeatedly.

### P1 — the remote, control by control

The long one, and the one that has to be done watching the player rather than the screen.
In this order, because each control leaves the player somewhere the next one needs it:

tray open, tray close, play, pause, still, step forward, step back, scan forward, scan back,
multi-speed forward and back at **each rate the selector offers**, a frame search, a time
search, a chapter search, on-screen display on and off, **each audio mode**, key lock on and
off, and reject.

- A control that is enabled and does nothing is a wrong capability flag.
- A control that does something other than its label is a wrong command sequence.
- A control the application greys out **that the model's manual says it has** is a wrong flag
  in the other direction, and it is the one this checklist would otherwise never reach —
  which is why the manual is on the list of things to have ready.

Then the two reads, with a disc **spinning**:

- The standard user code (`$Y`), which is cheap and moves nothing.
- The Pioneer user code (`?U`) **last, or expect to lose your place** — it is not a query,
  it searches to the lead-in to answer, and on this project's LD-V4300D that took 11.1
  seconds and left the player parked at frame 1. A model materially slower than the thirty
  seconds allowed needs its own timeout in its definition.
- `E04` from either is an error code, not a code to record: the LD-V4400 manual documents it
  as no user code being encoded, and this project's bench sees it from a parked disc that
  reads perfectly while spinning. The dialog must say the player refused rather than show
  `E04` as the disc's code.
- In the dump, a run of `` ` `` is the *player* saying it could not read those characters off
  the disc and a run of NULs is data that was never encoded. Both are facts about the disc,
  they are not the same fact, and the dump must not present them alike.

Finally the manual command field with `?X`, whose answer is the model code that belongs in
the definition — so it is also the check that the header claims the right ID.

### P2 — examine a CAV disc and a CLV disc

Once for each, with the disc parked rather than playing.

1. **The type and the addressing** must match the disc in your hand: frames on the CAV disc,
   time codes on the CLV one.
2. **The last address** must match what the player's own display shows at the end of the
   side. It is measured by seeking past the end and reading where that landed, so this is a
   comparison of two measurements rather than of a measurement against a claim.
3. **Watch what the sequence skips.** A step the model has no command for is not in the plan
   at all — the progress runs shorter, and that is correct. A step that runs and comes back
   *not known* is either a wrong capability flag or a reply this definition cannot decode.
4. **Check the disc status against the disc.** The report prints the five characters it
   decoded from — loaded, CAV/CLV, size, side, chapters — beside what it made of them. Turn
   the disc over and examine again: **the side must change and nothing else about it should**.
   A model whose reply is laid out differently shows up exactly here, as a size or a side
   that is confidently wrong, and the fix is `DiscStatusDecode` in that model's header.
5. **Check the video standard**, on a disc of each standard if you have both. It comes from
   the TV system request (`?S`) and is reported rather than guessed, so a model that answers
   it *wrongly* is the one failure in this whole procedure that nothing else can catch. Send
   `?S` from the manual command field to see the raw three characters — output, disc,
   external sync — and check the middle one. A model that does not answer at all should have
   `tv_system = false` in its header rather than a report that says the standard could not be
   established on every disc it ever meets.
6. The examination should take about a minute and **leave the disc held still at the start of
   the side**, not playing.
7. **Examine with the tray open, and with the tray shut and empty.** Two different findings —
   "the tray is open" and "the player would not start a disc" — and a model that reports its
   states differently is the one that gets them the wrong way round.

### P3 — the three automatic capture shapes

Needs the Duplicator as well. Each is run from **Set up capture** on the examine report, so
the plan is built from a profile that was just measured.

1. **A range**, from one address to another well inside the side. The shortest of the three:
   pick a couple of minutes. The player seeks to the start, the capture attaches, the disc
   plays, and both stop at the end address. Check the capture's length against the estimate
   the dialog showed.
2. **From spin-up to an address.** The capture attaches *first* and the disc is started from
   a stop, so the file holds the spin-up. Confirm by looking at the beginning of the capture:
   there is signal before there is a picture.
3. **The whole side.** The same beginning, and the ordering that matters at the other end —
   the player is stopped and **the capture keeps running through the spin-down**, because the
   run-out is not an address and this is the only way it reaches a file. This one takes as
   long as the side does.

For each: the front panel is locked while it runs if the plan asked for it and released
afterwards, the progress and the estimated time remaining move sensibly, and the finished
file carries the disc's own facts — model, type, size, side, standard, programme bounds — in
its metadata rather than an empty set of them.

Then the two coupling preferences, one each way: **stop the player when the capture stops**
(on by default) and **stop the capture when the player stops** (off by default, and
debounced — a momentary stop must not truncate a good capture).

### P4 — a capture cancelled halfway

Start the whole-side shape and cancel it a minute in.

The run must not be abandoned where it stands: the writer is detached, **the file is
finalised and plays**, the player is stopped and the front panel is released. That is the
difference from the old application, which walked away from the run and left the rest to
whoever noticed. Open the resulting capture and confirm it is a valid file of about the
length it ran for.

### P5 — a cable pulled halfway

Start the whole-side shape and **unplug the serial adapter** a minute in.

The deliberate behaviour, and the one thing here that looks like a bug if you have not read
this paragraph: **the capture keeps running.** The automation stops, the application says
the automation stopped and the capture is still going, and the user stops it by hand. The
player carries on to the end of the side regardless — it does not need the cable — and
truncating a good capture because an adapter came loose is the worse of the two failures.

A pass is: one message, saying both halves of that; a capture still writing; and no attempt
to send anything else down a dead port.

### What a pass looks like

- The player is found and identified at **every rate its own switches offer**, both by search
  and when told.
- Every control does what its label says, and every control that is greyed out is one the
  model does not have.
- Both discs examine to the type, addressing and last address they really are, and turning a
  disc over changes the side and nothing else.
- All three capture shapes produce a file of the expected length, with the disc's facts in
  it, and the whole-side one holds the spin-up and the spin-down.
- A cancel finalises; a pulled cable does not.
- Anything that fails is a **definition** change rather than an application change. If it
  cannot be fixed by editing that model's header, the schema has a gap and that is worth
  saying out loud rather than working around.

Only then is `bench_verified = true` for that model — and it is a claim about evidence, not
about capability. The tests passing proves a definition is internally consistent and encodes
the bytes it says it does. They cannot prove those are the right bytes for a player none of
them has ever met.

### When to run it

Per model, once, and again whenever that model's definition changes. The shared Pioneer
Level III base is the exception: a change there is a change to every model at once, so it
wants at least one verified model re-walked before release.

### What it does not cover

The capture path. A capture started by this procedure is still only proved intact by §5 —
the automatic sequence decides *when* to start and stop a capture and has nothing to do with
whether the samples in it are all there.

## 8. Planned work

Listed so this document can be read as a status report rather than a wish list.

| What | Tier | Notes |
| --- | --- | --- |
| CI test lanes | — | Run T1–T4 in the consolidated workflow. T5 never runs in CI |
| SPDX conversion of the remaining long-form headers | T4 | 25 files. Opportunistic by design (AGENTS.md §5.4) — not a scheduled task, and the check prints the count each run |
| Finish validating the single-clock gateware | T5 | See below. The board is programmed and a 16-minute capture came back clean; four checks remain |
| Gateware-target interruption tests | T5 | The clean path ran on 2026-08-15 and is §6 (G0/G1): provisioning, the update, the factory→application handover, V4, V6 and V7 all performed or measured, and the five defects the session surfaced are recorded there. What remains is the interruption half: power pulled mid-write and during the readback verify, the boot block sector erased by JTAG, and the *recovery gateware* → **Reinstall gateware** repair flow driven from each of those states |
| The player control bench walk | T5 | §7, per model, and the gate on `bench_verified`. **Nothing is walked yet**: every definition in `ddd-gui/src/player/players/` is inherited from the documented Pioneer Level III set, and the LD-V4300D — this project's own bench player — has measured readings for connection, disc status, TV system and both user codes but has not been taken through P1 to P5. The automated half (`ctest -L hil-player`) runs today; the checklist is what turns an inheritance into an observation |
| A Flatpak build talking to a player | T5 | The manifest's `--device=all` is what grants `/dev/ttyUSB*` inside the sandbox, and there is no narrower static permission or portal for a serial device — so the permission is right by construction and by the same reasoning that covers the USB capture path. It has not been *confirmed* from inside a built Flatpak, which is a ten-minute check and is the acceptance criterion the player control plan's Task 6.3 leaves open |
| First release through the CI pipeline | T4/T5 | The bitstream, release and reproducibility-audit workflows exist and every part of them that can be exercised without a signing key and a tag has been: the bundle assembles and verifies against a pinned public key, an application built with that key pinned accepts it and one built without refuses it, and a release build refuses a development-signed bundle unless the opt-in is given. What remains needs the maintainer: generate the release keypair, set `UPDATE_SIGNING_KEY`, commit `tools/keys/release.pub`, then tag. The rehearsal is then: the tag publishes a release whose every asset was CI-built from it, the `.dddfw` installs onto bench hardware through the file-picker path with the device reporting the identities the manifest names, and the audit job runs green against that release at least once |
| Watchdog period and handover timing, before the freeze | T5 | The handover works (§6 G1, 2026-08-15) and the watchdog tickle is proven by the application surviving past the timeout. Still open before the factory image is frozen: narrow `WatchdogTimeout` from the 12-bit maximum to a measured margin over the worst-case FX3 boot, measure the double-configuration time against the FX3's "FPGA ready" assumption (**V5**), and give the boot logic its deliberate second-attempt refusal so a validating-but-dead application image parks in recovery instead of cycling |

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
`dataGenerator`, `fx3StateMachine` and `spiRegisters` testbenches are all in §4.6, and the
licence-header check is in §4.7. So is the `buffer.v` testbench, which is here because it
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

## 9. Conventions for new tests

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
