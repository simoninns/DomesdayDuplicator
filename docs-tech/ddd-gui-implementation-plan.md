# Capture Application Rebuild (`ddd-gui/`) — Implementation Plan

## Purpose

The existing capture application ([gui/](../gui/)) is functionally complete and its USB
transfer engine is proven, but its user feedback is thin: during a capture the operator sees
an RMS number, a coarse waveform refreshed once a second, and counters. There is no way to
run the device *without* writing to disk, so checking cabling, RF level or spectral content
means starting a throw-away capture. The interface is a fixed single window of group boxes,
with no theme support and visual conventions that predate the rest of the project's tooling.

This plan rebuilds the capture application from scratch in a new `ddd-gui/` directory, leaving
`gui/` untouched and shippable throughout. The new application is centred on real-time
signal monitoring: a **monitor mode** streams from the device through the full validation
pipeline with no disk writer attached, feeding live views of throughput, amplitude over
time, and frequency content; **capture** is then the same pipeline with a writer attached,
started and stopped without interrupting the stream. The GUI is panel-based (dockable,
pop-outable, selectable from a View menu), themed light/dark, and follows the architecture
patterns already established in the author's videosynth and decode-orc projects.

The new component lives in `ddd-gui/`; executable and CMake target are named `ddd-gui`,
and code lives in `ddd::capture` (engine) and `ddd::gui` (Qt layer). Once it reaches
feature parity it replaces `gui/`; until then the two applications coexist and `gui/`
remains shippable.

## Authoritative references (in-tree)

- Repository conventions, licence headers, naming: [AGENTS.md](../AGENTS.md) (§2, §5, §6)
- Test tiers and the hardware-in-the-loop integrity gate: [TESTING.md](../TESTING.md) (§2, §5)
- Wire protocol and device behaviour:
  [docs/content/development/software-guide.md](../docs/content/development/software-guide.md),
  firmware constants in [fx3/firmware/src/domesday-duplicator.h](../fx3/firmware/src/domesday-duplicator.h)
- Proven transfer-engine mechanisms to carry forward:
  [gui/src/DomesdayDuplicator/UsbDeviceBase.cpp](../gui/src/DomesdayDuplicator/UsbDeviceBase.cpp),
  [UsbDeviceLibUsb.cpp](../gui/src/DomesdayDuplicator/UsbDeviceLibUsb.cpp),
  [UsbDeviceWinUsb.cpp](../gui/src/DomesdayDuplicator/UsbDeviceWinUsb.cpp)
- Format and codec ground truth: [gui/src/common/](../gui/src/common/)
  (`captureformat.h`, `samplecodec.h`, `flacwriter.*`, `capturereader.*`, `testdataanalyser.*`)

## The real-time problem, stated once

The device delivers 40,000,000 samples/s, each a 10-bit value in a 16-bit little-endian
word — **80 MB/s, continuously, for hours**. Bits 15:10 of every word carry a 6-bit
sequence counter (incrementing every 65,536 samples, wrapping 0..62) that is the only
evidence a capture is bit-perfect. The device streams continuously once opened; the host
must never stall reading, because any gap is either detected as a sequence mismatch (good)
or silently corrupts an archival capture (unacceptable). The FX3 has 64 KB of buffer — the
host-side queue is the only real slack in the system.

The old engine solved this well, and this plan ports its mechanisms rather than reinventing
them: a ring of ~2 MB memory-locked disk buffers (default 256 MB queue), C++20 atomic
wait/notify handoff with no mutexes on the hot path, 128 KB bulk transfers in
small-transfer mode, elevated thread priorities (`SCHED_RR` / `REALTIME_PRIORITY_CLASS`),
startup discard of the first ~4 buffers, short-packets-fatal, and the Linux
`usbfs_memory_mb` workaround. What is *new* is the strict rule that monitoring must be a
read-only, bounded-cost tap on this path — the GUI can starve without consequence; the
pipeline can never wait on the GUI.

## Hard constraints

- `gui/` is not modified by any phase of this plan. The two applications coexist.
- **No cross-component includes** (AGENTS.md §2). `ddd-gui/` gets its own copies of the
  Qt-free core (`samplecodec`, `flacwriter`, `capturereader`, `testdataanalyser`,
  `captureformat`) together with their tests, and its own `cmake/Find*.cmake` modules.
  Wire-protocol constants (VID:PID `0x1209:0x2347`, vendor request `0xB6`, endpoint
  `0x81`) are re-declared in `ddd-gui/` deliberately — they are protocol, not shared code.
- Any change to the capture data path requires the T5 hardware-in-the-loop procedure
  before it can be called working (TESTING.md §5; AGENTS.md §4). A green build proves
  nothing here.
- Every source file carries the SPDX header of AGENTS.md §5.4;
  `tools/check-licence-headers.sh` covers `ddd-gui/` automatically and must stay green.
- Code style is the Google C++ Style Guide, enforced in the build: `.clang-format`
  (`BasedOnStyle: Google`) as a `--dry-run --Werror` gate every binary depends on, and
  `.clang-tidy` (`google-*`, `bugprone-*`, `WarningsAsErrors: '*'`) run by the build.
  This is the videosynth convention; the old `gui/` style is not carried over.
- C++20 (repository standard; the handoff design requires `std::atomic<T>::wait`).
- Qt ≥ 6.5 for `QStyleHints::colorScheme` (the old app requires 6.2; raising the floor is
  acceptable for a new application and must be stated in `ddd-gui/README.md`).
- Tests are gtest under CTest with tier labels exactly as in
  [gui/tests/CMakeLists.txt](../gui/tests/CMakeLists.txt) (`unit`, `golden`, `functional`
  additions use the same labelling function pattern).
- The build works with ordinary distribution tools; Nix packaging is added alongside, never
  instead (AGENTS.md §7). Version is injected (`-DDDD_VERSION=`), git lookup is a local
  fallback only (AGENTS.md §9).
- **Every dependency must carry a GPLv3-compatible licence.** This is a hard requirement
  on all of them, direct or vendored, at every phase — checked before adoption, not after
  (AGENTS.md §10). The baseline set qualifies: Qt 6 (LGPL-3.0), libusb (LGPL-2.1),
  libFLAC (BSD-3-Clause), GoogleTest (BSD-3-Clause). The FFT proposed in Phase 4 is
  vendored KissFFT or pocketfft (both BSD-3-Clause) — confirm at that phase, do not
  assume. A dependency whose licence position is unclear is raised before use, per
  AGENTS.md §13.
- No file, target, branch or symbol named after a plan phase (AGENTS.md §6).

## Architecture overview

Two layers, split for testability exactly as videosynth splits them:

```
ddd-gui/
├── CMakeLists.txt          single build definition; format/tidy gates; version injection
├── cmake/                  FindLibUSB.cmake, FindFLAC.cmake (component-local copies)
├── src/
│   ├── capture/            ddd::capture — Qt-free engine library (static)
│   │                       device sources, ring buffer, sequence validation, codecs,
│   │                       writers, stats, monitor taps
│   └── gui/                ddd::gui — Qt6 application `ddd-gui`
│                           main window, theme system, dock panels, presenters
└── tests/
    ├── unit/               T1 — engine and presenter tests, no I/O
    ├── gui/                T1 — Qt layer: models, controllers (unit/) and widgets (widget/)
    ├── golden/             T2 — capture-format checks against the bytes on disk
    ├── functional/         T1-labelled soak/pipeline tests using the synthetic source
    ├── hardware/           T5 — labelled `hil`, needs a device attached, never run in CI
    └── support/            fixtures shared between test binaries
```

**Threads.** Identical roles to the proven engine, plus one addition:

1. *GUI thread* — Qt only. Polls published stats at ≤ 30 Hz. Never touches the pipeline.
2. *Orchestrator thread* — owns buffer allocation, locking, priority elevation, worker
   lifecycle, error latching, graceful/forced shutdown.
3. *USB transfer thread* — backend-specific (libusb / WinUSB); submits and reaps bulk
   transfers, marks ring slots full.
4. *Processing thread* — sequence validation, metrics, conversion/encode, sink write.
   In monitor mode the sink is a null writer; everything else is identical.
5. *Analysis worker (GUI side, new)* — computes FFTs and scope geometry from published
   snapshots, so even that cost stays off both the GUI thread and the pipeline.

**Monitor tap (new).** The processing thread publishes two things at bounded cost:
an atomically-versioned stats block (throughput, transfer counts, sequence state, buffer
fill level, min/max/clipped, RMS accumulator), and a triple-buffered raw-sample snapshot
(a fixed-size copy, ≤ 64 KiB, taken at most every Nth disk buffer). Consumers read the
latest complete version and never block the writer; a slow GUI drops snapshots, never
samples.

**Sources and sinks are seams.** `ISampleSource` has three implementations — libusb,
WinUSB, and a synthetic source that generates the FPGA test ramp with sequence markers at
a configurable rate (including full 80 MB/s). `ISampleSink` has two implementations:
null (monitor) and native FLAC. The synthetic source is what makes the pipeline,
backpressure and failure paths testable in CI with no hardware, which the old application
could never do.

The `ddd::capture` library stays strictly Qt-free for a second reason beyond testability:
a forward requirement (see *Forward requirements* below) is a headless command-line
capture tool, and everything it would need — device discovery, monitor, capture, error
reporting, progress — must live in the engine, with the GUI as one thin front end over
it. Any task in this plan that puts such logic in `ddd::gui` instead of `ddd::capture` is
doing it in the wrong place.

**Monitor → capture is a sink swap.** Starting a capture attaches the writer at the next
disk-buffer boundary without stopping the USB stream; stopping detaches it and finalises
the file. The device's dormant start/stop vendor request (`0xB5`) stays unused, matching
current gateware behaviour (it samples continuously) and matching the old application,
which never sends it either.

---

## Phase 1 — Component scaffold and application shell

Establishes `ddd-gui/` as a self-contained component with its quality gates in place from the
first file, and produces a runnable themed, panelled application shell with no device code.
Getting the gates in first matters: retrofitting clang-tidy onto a component is a sweep;
starting under it is free.

### Task 1.1 — Component skeleton and build gates

Create `ddd-gui/` with the layout above: top-level `CMakeLists.txt` (C++20,
`CMAKE_EXPORT_COMPILE_COMMANDS ON`, `DDD_VERSION` injection with git fallback), component
`.clangd`, `.clang-format` (`BasedOnStyle: Google`) and `.clang-tidy` scoped to `ddd-gui/`,
per-file clang-format stamp targets gating every binary, gtest harness with the
tier-labelling helper function modelled on `gui/tests/CMakeLists.txt`, and a `README.md`
stating build requirements. SPDX headers on every file from the outset.

**Acceptance criteria**
- `cmake -B ddd-gui/build -S ddd-gui && cmake --build ddd-gui/build` succeeds on Linux with only
  distribution packages (Qt ≥ 6.5, gtest).
- A deliberately misformatted file fails the build; restoring formatting fixes it.
- `ctest --test-dir ddd-gui/build -L unit` runs a placeholder engine test and passes.
- `./tools/check-licence-headers.sh` passes over the tree with `ddd-gui/` present.

### Task 1.2 — Theme system

Port the videosynth two-class pattern: a pure `ThemeManager` (parses `auto|light|dark`,
resolves against `QStyleHints::colorScheme`, palette-darkness fallback when the platform
reports Unknown) and a `ThemeController` (applies the palette — full role coverage
including `Light/Midlight/Mid/Dark/Shadow`, per the decode-orc Windows lesson — persists
the mode in QSettings, follows OS changes in auto mode, emits `ThemeChanged(bool)`).
Include the `theme_tokens` palette-derived colour helpers for custom-painted widgets.
View → Theme menu with Auto/Light/Dark radio actions.

**Acceptance criteria**
- `ThemeManager` resolution logic covered by T1 unit tests (all mode × scheme × fallback
  combinations), no QApplication required.
- Switching theme at runtime restyles the whole shell without restart; choice persists
  across runs; auto mode follows a live OS scheme change.

### Task 1.3 — Panel framework and application shell

`QMainWindow` with `QDockWidget` panels built by dedicated `Build*Dock()` methods, each
dock given an `objectName` for state persistence. View → Panels submenu populated from the
docks' `toggleViewAction()`s; docks float (pop out) natively. Initial panels are
placeholders: Capture, Statistics, Waveform, Spectrum, Amplitude History, Log (Log starts
hidden). Menus: File → Exit, View (Panels, Theme), Help → About. Window geometry and dock
layout saved in `closeEvent` via `saveGeometry`/`saveState` and restored on start;
QSettings identity set once in `main`. A `--debug` CLI switch and a Log panel fed through
a small logger seam (`ILogger`-style, so engine code never links Qt for logging).

**The About dialog must display the build's commit hash** — the injected `DDD_VERSION`,
the same string `--version` reports. This is a requirement rather than a courtesy, and it
is why there are deliberately two routes to it:

- A user reporting a capture problem has to be able to say which build produced it, and
  the population that runs this application from a terminal is not the population most
  likely to hit a hardware problem. "Help → About, read me the version" works over a
  forum post; "run it with `--version`" often does not.
- The command line is not always available. On Windows the application is linked as a GUI
  subsystem executable, so `--version` writes to a console that is not attached and the
  user sees nothing at all. Making the About dialog carry the hash is what stops the
  most-installed platform being the one where a binary cannot be identified.
- The same string reaches a capture's own metadata (the `DDD_VERSION` Vorbis comment,
  Task 5.1), so a file, a dialog and a shell all name the same commit and can be checked
  against each other.

The text is built by a pure function so the requirement can be tested rather than
eyeballed; a dialog is otherwise modal and untestable.

**Acceptance criteria**
- Any panel can be hidden, shown, docked, floated and re-docked from the View menu; the
  arrangement survives restart.
- The About text contains the build version, asserted by a T1 test, and that text is what
  the dialog shows. `--version` and About report the same string.
- A build with `-DDDD_VERSION=` set shows that value in About; a build that could not
  determine one shows `unknown` rather than an empty or absent version line.
- Shell runs and quits cleanly headless-CI-safe tests aside on Linux, and compiles for
  Windows and macOS (CI proof arrives with Task 1.4).

### Task 1.4 — Build wiring: flake, checks, CI matrix

Add `ddd-gui/package.nix` and `ddd-gui/shell.nix`, wire them into the root `flake.nix` (packages,
dev shell, checks) without adding any component flake (AGENTS.md §7 one-flake rule). Add
`ddd-gui` to the native CI build matrix beside `gui`, on the same runners the existing
`gui-native` job uses — Linux x64, macOS ARM64 and Windows x64. That is the project's
whole runner set; there is no ARM64 Linux or x64 macOS job anywhere in the tree today, and
inventing one here would be a change to the project's CI strategy rather than a step in
this plan. The release workflow's `unknown`-version guard applies to `ddd-gui` too.

The two quality gates run in this native job and *not* in the Nix build, so this is the
only place a formatting or lint regression is caught.

**Acceptance criteria**
- `nix build .#ddd-gui` and `nix flake check` pass; `gui` outputs are unchanged.
- CI produces `ddd-gui` artefacts for all three targets on every commit.
- A build without `.git` and with `-DDDD_VERSION=` set reports the injected version, and
  CI fails if it does not.

---

## Phase 2 — Capture engine, no hardware

The Qt-free `ddd::capture` library: everything that can be proven without a device, proven
without a device. This phase ends with a CI-runnable soak test pushing synthetic data
through the full pipeline at full rate — the strongest statement short of T5 that the
real-time design holds.

### Task 2.1 — Formats, protocol constants and ported core

Define the engine's sample-format module (40 MSPS, 10-bit in 16-bit words, sequence-marker
layout, `(v − 512) × 64` scaling) and the component's own wire-protocol constants. Port
the 10→16-bit scaling codec, `captureformat`, `flacwriter`, `capturereader` and
`testdataanalyser` from `gui/src/common/` into `src/capture/`, renamed and restyled to the
component's conventions, together with their unit and golden tests (including the FLAC
round-trip and its golden vectors). The ported writer changes container: it writes
**native FLAC only** (`FLAC__stream_encoder_init_file` in place of `init_ogg_file`;
everything else — encoding, Vorbis-comment metadata block, threading — is identical). The
Ogg encapsulation of `.ldf` was a workaround for long-fixed FLAC limitations and is not
carried forward: native FLAC is read by ld-decode through the same libavcodec loader
(`lddecode/utils.py` routes `.flac` and `.ldf` identically) *and* imports into general
audio editors (Tenacity/Audacity), which cannot open Ogg FLAC. Legacy container support
ends here: the reader handles **native FLAC and 16-bit `.raw` only** — `.ldf` and
10-bit-packed `.lds` are neither written nor read by the new application (the old
application remains the tool for legacy files), so the packed-sample codec is not ported.
`.raw` reading is retained because it is trivial and gives `--analyse-test-data` a common
format with the old application for verdict-parity checks (Task 5.3).

**Acceptance criteria**
- Ported tests pass under the new tier labels; golden vectors for the retained formats
  unchanged from `gui/`.
- A native `.flac` written by the ported writer round-trips to sample-identical data
  through the ported reader (T1/T2) and carries the Vorbis-comment metadata block.
- No `#include` reaches outside `ddd-gui/`.

### Task 2.2 — Ring buffer and handoff

Port the disk-buffer ring: ~2 MB slots sized to a multiple of the endpoint max packet,
count derived from a configurable queue size (64–512 MB, default 256 MB), C++20 atomic
wait/notify handoff with no mutex on the hot path, memory locking (mlock/VirtualLock) with
graceful degradation, the startup-discard mechanism, and a forced-abort protocol that
releases any waiter. Expressed as its own class so it is testable with plain threads.

**Departure from the old engine, deliberately.** The old ring gives each slot a pair of
`atomic_flag`s and releases blocked threads at shutdown by *double-toggling* the full flag
— clear to wake the consumer, then set to wake the producer. That is racy: a waiter not
scheduled between the two toggles re-reads the flag, finds the value it was already
waiting on, and blocks permanently. Writing the test the acceptance criteria below ask for
("abort releases blocked waiters") reproduced it on the second run. The ring here uses one
`std::atomic<uint32_t>` per slot with four states — empty, full, dumped, aborted — so
shutdown moves a slot to a value nobody is waiting for and *leaves it there*. No wake can
be missed because there is no path back. The old engine is not being fixed as part of this
work (AGENTS.md §4 puts its capture path behind the T5 procedure); this records why the new
one differs.

**Acceptance criteria**
- T1 tests: producer/consumer correctness under contention, overflow detection (slot
  still full at completion → fatal), abort releases blocked waiters *on both sides*,
  startup discard count honoured.
- No mutex or condition variable appears anywhere on the producer→consumer path.

### Task 2.3 — Sequence validation and metrics

Port the sequence-marker state machine (Sync → Running / Disabled / Failed; phase lock
within ≤ 65,537 samples; wrap at 62; marker-stripping before downstream use) and the
test-ramp verifier (auto-detected 1021/1024 wrap). Add the metrics accumulator: samples,
min/max, per-extreme clip counts, recent-window extremes, RMS.

**Acceptance criteria**
- T1 tests cover: clean lock, mid-stream mismatch, markerless legacy stream → Disabled
  with capture continuing, wrap boundaries, ramp break detection at exact offsets.
- Validation and metrics for a 2 MB buffer complete comfortably inside the real-time
  budget (measured; a 2 MB buffer arrives every 26 ms at 80 MB/s).

### Task 2.4 — Sources, sinks and the pipeline orchestrator

Define `ISampleSource` / `ISampleSink`, implement the synthetic ramp source (configurable
rate, sequence markers, optional injected faults: sequence break, short delivery, stall)
and the null and FLAC sinks. Implement the orchestrator: thread lifecycle, priority
elevation (failure non-fatal, logged), error latching with the full `TransferResult`-style
error taxonomy of the old engine, graceful stop at buffer boundaries, forced abort, and
the monitor→capture sink attach/detach at disk-buffer boundaries.

**One addition to the taxonomy: `kSourceStalled`, with a watchdog behind it.** A device
that stops delivering *without failing a transfer* is invisible to every other check in
the old engine — no error is raised, no thread returns, and the application simply waits
for data that is not coming. The user sees a frozen progress figure and cannot tell that
from a slow disc. The control thread here watches the completed-transfer count and, if it
has not moved for a configurable interval (default 5 s, against a 26 ms buffer period at
full rate, so ~200 buffers of slack and no false positives), latches `kSourceStalled` and
aborts. It costs one comparison every 50 ms on a thread with nothing else to do, and it is
the difference between an error message and an apparent hang.

**Acceptance criteria**
- T1 tests: start/stop/abort state machine, error latching precedence, sink swap occurs
  exactly at a buffer boundary with no sample lost or duplicated (synthetic source,
  counted stream).
- Injected faults surface as their specific error codes, never as hangs — including an
  injected stall, which must surface as `kSourceStalled` within the watchdog interval
  rather than as a test timeout.

### Task 2.5 — Monitor tap and full-rate soak test

Implement the versioned stats block and the triple-buffered snapshot publisher, then the
functional soak: synthetic source at 80 MB/s through validation + null sink, and through
the FLAC sink, for ≥ 60 s in CI (longer locally via an environment knob), with a consumer
hammering the tap concurrently.

**Measuring "costs the pipeline nothing" honestly.** The obvious test — compare pipeline
throughput with and without a tap consumer — is confounded, and measurably so. A consumer
spinning with no pause copies the stats block millions of times a second and saturates
memory bandwidth the pipeline itself needs: measured here, that took an unpaced pipeline
from 600 MB/s to 140 MB/s *with publish latency unchanged*. That is a memory controller
being shared, not a lock being contended, and asserting on the throughput ratio would be
asserting something the measurement cannot distinguish. Two tests replace it. A T1 test
measures the writer's own publish time with four threads hammering the tap and with none —
if publication took a lock, a reader holding it would show there immediately. The soak then
compares throughput against a consumer reading at 1 kHz, which is thirty times faster than
any display and slow enough not to compete for bandwidth.

**Acceptance criteria**
- Soak passes with zero sequence errors and zero overflow at 80 MB/s on CI runners; the
  FLAC-sink variant may be rate-reduced on constrained runners, and if it is, the cap is
  logged in the test output, not hidden.
- Tap readers never observe a torn stats block or torn snapshot (checked by embedded
  generation counters).
- Mean publish cost stays in the hundreds of nanoseconds with four threads hammering the
  tap (T1, measured directly — a lock would cost microseconds, and a descheduled
  lock-holder milliseconds), and pipeline throughput with a 1 kHz monitoring consumer is
  within noise of throughput without one.
- TESTING.md gains a paragraph describing the new functional tier usage in `ddd-gui/`.

---

## Phase 3 — USB acquisition and monitor mode

The engine meets the device. Backends are ported, not redesigned: the old engine's
transfer geometry survives hours-long bit-perfect captures in the field, and this plan
treats that as evidence to preserve. From this phase on, changes are in T5 territory.

### Task 3.1 — Device discovery and control (libusb)

Device enumeration by VID:PID with preferred-device path selection, hot-plug detection
surfaced to the GUI, the `0xB6` vendor configuration request (test-mode bit), and
**SuperSpeed enforcement** — the old backends accepted High-speed devices that cannot
carry 80 MB/s; the new one refuses to open anything below SuperSpeed with a specific
error. Fix carried into the WinUSB backend in Task 3.3.

Also new here: an **FX3 firmware version check** on connect. The firmware embeds its git
hash in the USB product string descriptor (`"Domesday Duplicator (<hash>)"`,
[fx3/firmware/src/usb-descriptor.c](../fx3/firmware/src/usb-descriptor.c)), and releases
build every artefact from the same commit (AGENTS.md §9), so the application compares
that hash against its own injected version. A difference raises a **warning modal — not
an error**: it is shown once per connection, explains that the firmware build differs
from the one this application was released with, and never blocks monitoring or capture.
An unparseable product string gets the same warning, not a failure. (The matching check
of the FPGA gateware version is a forward requirement — see *Forward requirements* — as
it needs a protocol addition the current gateware does not have.)

**Detection is polled, not hot-plug, and that is a choice.** libusb has
`libusb_hotplug_register_callback`, but it is unsupported on Windows and its macOS
behaviour has depended on the libusb build; WinUSB has no equivalent short of a window
handle and a device-notification message pump. Polling is the one mechanism that behaves
identically on all three platforms, and at 200 ms it costs an enumeration five times a
second — microseconds — to meet a 500 ms requirement with room to spare. It runs on its
own thread, because reading a product string means opening the device and doing a control
transfer, and on the GUI thread that is a visible stutter five times a second. It is
suspended while streaming: enumeration opens devices, and doing that to one that is
delivering data is avoidable bus traffic for an answer that is already obvious.

**The version comparison is on a prefix, and the reason is not cosmetic.** The firmware
asks git for eight characters; the application's stamp comes from whichever build system
produced it, and Nix supplies seven. Comparing the strings whole would report a mismatch
between two artefacts of the *same* commit — a warning that fires when nothing is wrong,
which is worse than no warning at all because it teaches the user to dismiss the dialog
unread. Both sides also go through the same `-dirty` stripping: the FX3's CMake asks git
exactly as the application's does, so a development device reports
`Domesday Duplicator (bb65470-dirty)` and rejecting that as unparseable would warn on
every connection. An application that cannot name its own commit says nothing at all,
rather than accusing the firmware of being unknown when it is equally unknown itself.

**Acceptance criteria**
- Device attach/detach reflected in the GUI within 500 ms; T5: a device on a USB 2 port
  is reported as "connected at insufficient speed", not opened.
- Test-mode toggle verified end-to-end on hardware (ramp visible in captured data).
- Version-check comparison logic is a pure, T1-tested unit; T5: a firmware built from a
  different commit triggers the warning modal exactly once and capture still works; a
  matching build shows nothing.

### Task 3.2 — libusb streaming source and monitor mode in the GUI

Port the libusb transfer engine (128 KB small transfers by default, queue span capped by
the limited-queue option at 12 MB for the usbfs limit, `SHORT_NOT_OK`, infinite timeout,
`NO_MEM` mapped to the usbfs guidance error) as an `ISampleSource`. Wire the GUI: Capture
panel gains Monitor / Stop; Statistics panel shows live throughput (MB/s and effective
MSPS), transfer count, sequence status, ring-buffer fill, min/max/clipping from the tap.
Settings panel/dialog for queue size, transfer mode, preferred device.

**The transfer geometry becomes a pure function.** The old engine worked out transfer
size, count, stride and starting slot inline in each backend, and the two copies had
subtly different starting-index arithmetic. Both now call one `PlanTransferLayout()` that
takes a slot size, a slot count and an endpoint packet size and returns the whole layout.
This is the most intricate arithmetic in either backend and the least observable on
hardware: a stride wrong by one produces a capture that is subtly *interleaved* rather
than one that fails, and the only way to notice that with a device attached is for a disc
to sound wrong. As arithmetic over a struct it is simply checked — including a simulation
that walks the transfers through several laps of the ring and asserts the buffers come out
in the order the consumer reads them.

**The completion callback may block, and must.** When a transfer completes and the slot it
would be resubmitted into is still full, the callback waits for the consumer, and libusb's
event handling waits with it. That is intentional and inherited: dropping the transfer
instead would silently lose samples, which is the one thing the sequence markers exist to
make impossible. If the consumer genuinely cannot keep up, the transfers already submitted
keep being filled by the kernel, the device's 64 KB FIFO overruns, and the validator
reports exactly that — a stall becomes a reported error rather than a quiet corruption.

**Acceptance criteria**
- T5 on Linux: monitor mode sustains ≥ 1 hour with zero sequence errors, GUI live
  throughout; ring-fill indicator behaves sanely under induced CPU load.
- Pulling the cable mid-monitor produces a clean, specific error and a recoverable
  application state (re-attach and monitor again without restart).
- T5: aborting mid-stream returns the transfer thread within two seconds. See the
  hardware tier below — this is the one that caught a real hang.

### Task 3.4 — A hardware tier that runs

Every acceptance criterion above is a T5 procedure, and a procedure nobody runs is a
comment. `tests/hardware/` is a gtest binary labelled `hil`, excluded from every ordinary
run and from CI, that turns them into something a maintainer executes in one command with
a device plugged in:

    ctest --test-dir build -L hil

It refuses rather than skips when nothing is attached — a hardware test that "passes"
because there was no hardware is worse than no test. Nothing in it reprogrammes anything:
it streams and sends the `0xB6` configuration request, which is what the application does
in normal use, and writing the FX3 EEPROM or the FPGA flash stays a manual procedure
(AGENTS.md §4).

One of its checks is on the *rate*, not the data, and it earns its place: the device's
output is clocked by a 40 MHz converter, so a working device delivers 80 MB/s and
physically cannot deliver more. A higher figure means the samples are not coming from the
ADC — an unprogrammed FPGA, or gateware that is not the sampler — and that is a diagnosis
no amount of staring at sample values would produce. It earned it immediately: the first
device it ran against was delivering a 16-bit counter at 117 MB/s, and the rate check is
what named the cause where the sequence-mismatch error only reported a symptom.

**Measured on hardware**, once that device was running gateware built from this tree
(Quartus 25.1, programmed to EPCS64 and cold-booted from it):

| | |
| --- | --- |
| Transfer rate, 800 MB | 79.2 MB/s — 99% of the wire rate |
| Sequence markers | 6,103 counter periods, every one exactly 65,536 samples |
| Test pattern | 121,634,816 samples checked against the ramp, unbroken |
| Peak ring depth | 1 buffer of 128 |
| Abort mid-stream | 10 ms |

The hour-long soak in TESTING.md §5 is still a manual procedure and still the release
gate; this is what a maintainer can get in twenty seconds before starting one.

### Task 3.3 — WinUSB source and macOS validation

Port the WinUSB backend (RAW_IO, `MAXIMUM_TRANSFER_SIZE` query, overlapped reap loop, the
explicit underflow probe) with the SuperSpeed check added. Validate the libusb backend on
macOS (Intel and Apple Silicon), where the old app has no platform-specific capture code —
confirm that remains true or document what was needed.

**Two departures from the old WinUSB backend, both deliberate.**

The wait for a completion is bounded rather than indefinite. The old code called
`WinUsb_GetOverlappedResult` with the wait flag set, which returns when the transfer
completes and never otherwise — so a device that stopped delivering without failing left
the transfer thread blocked in the kernel with no way back, and the application hung. This
waits on the event with a timeout and looks around between waits, which is what lets the
stall watchdog from Task 2.4 actually stop the thread rather than merely report it.

The SuperSpeed check has to be made a different way, because WinUSB cannot answer the
question directly. Its `DEVICE_SPEED` query uses the driver's own three-value enumeration,
which stops at `HighSpeed` — there is no SuperSpeed constant, so a USB 3 device reports
`HighSpeed` and the query cannot distinguish the two. The bulk endpoint's maximum packet
size can: 512 bytes at High-speed and 1024 at SuperSpeed, fixed by the specification. That
is what the check reads.

**Acceptance criteria**
- T5 monitor soak (≥ 1 hour, zero sequence errors) on Windows and macOS.
- Platform quirks discovered are recorded in `ddd-gui/README.md`, not just fixed.

---

## Phase 4 — Real-time signal visualisation

The reason this application exists. All widgets are QPainter-painted custom `QWidget`s
using the theme tokens (the videosynth pattern — no QCustomPlot, no OpenGL), with their
mapping mathematics split into Qt-free, unit-tested modules. All analysis runs on the
GUI-side worker fed by tap snapshots; panel refresh is throttled (target 15–30 Hz,
degrading by dropping frames, never by backpressuring).

### Task 4.1 — Waveform scope panel

Time-domain scope over the latest snapshot: 10-bit sample space with grid lines at zero,
mid, and clip levels; selectable time span; persistence/brightness-accumulation mode
(decode-orc's `WaveformMonitorWidget` approach) for eyeballing FM carrier envelope; cursor
readout (sample value, time offset). Mapping math in a `waveform_mapping`-style pure
module.

**Acceptance criteria**
- T1 tests for mapping (sample→pixel, span/zoom, cursor inverse mapping).
- Scope stays fluid during a full-rate monitor with no measurable effect on pipeline
  stats (compare soak metrics with panel hidden vs. visible).

### Task 4.2 — Amplitude history panel

Rolling RMS-over-time strip (minutes of history at ~10 Hz resolution) with min/max
envelope and clip-event ticks, replacing the old 1 Hz RMS number/chart. History ring and
statistics in a pure module; clipping events also mirrored as a count in the Statistics
panel.

**Acceptance criteria**
- T1 tests: RMS/envelope aggregation, ring wraparound, clip-event edge cases (exact 0 and
  1023 samples).
- An amplitude step injected by the synthetic source appears in the history at the correct
  time offset.

### Task 4.3 — Spectrum panel

FFT magnitude display, DC to 20 MHz: windowed (Hann) power spectrum from snapshots,
averaging with adjustable decay, log magnitude scale, peak-hold trace, frequency cursor
readout — enough to see LaserDisc FM carriers and interference at a glance. Requires the
vendored FFT (KissFFT or pocketfft, BSD-3-Clause): confirm licence position per
AGENTS.md §10 *before* vendoring, record the decision in `ddd-gui/README.md`.

**Acceptance criteria**
- T1 tests: a synthetic single-tone snapshot yields a peak in the correct bin at the
  correct level; window normalisation verified against an analytically known input.
- FFT cost measured and documented; runs entirely on the analysis worker.

### Task 4.4 — Statistics panel completion

Full live statistics: elapsed time, throughput, transfer count, sequence-protection state
(protected / legacy-unprotected / failed), ring-buffer fill high-water mark, samples
processed, min/max, per-extreme clipped counts with recent window, device link speed, and
— during capture — bytes written, encoder backlog, and free-space time remaining.

**Acceptance criteria**
- Every displayed figure comes from the versioned stats block; nothing on any panel reads
  pipeline state directly.
- Values verified against synthetic-source ground truth in a functional test at the
  presenter level (T1, headless `QCoreApplication` gtest main, the videosynth pattern).

---

## Phase 5 — Capture to disk

Capture becomes real: the writer attaches to a running monitor, and the result must meet
the same bar as the old application — bit-perfect for hours — before this component can be
proposed as a replacement.

### Task 5.1 — Capture controls and file management

Capture panel: destination directory, filename (default `RF-Sample_<timestamp>`, free-text
override), FLAC compression level 0–8 defaulting 1, duration limit, free-space-as-time
readout (estimated ~40 MB/s for FLAC) refreshed continuously, and a low-space warning
threshold. **Native FLAC is the only capture format** — `.raw` is not carried forward,
since ld-decode and Tenacity both read `.flac` directly and FLAC level 0–1 with
multithreaded libFLAC covers the low-CPU case `.raw` served. Capture is always the full
40 MSPS stream; the old 4:1 CD decimation option is not carried forward — rate reduction
is a downstream processing job, not a capture-time one. Captures default to the compound
extension **`.ddd.flac`** (e.g. `RF-Sample_2026-08-13_12-00-00.ddd.flac`): the `.ddd`
marker makes the sample's origin visible in the filename while the file remains a plain
`.flac` to every importer, and ld-decode's extension dispatch (`endswith(".flac")`) still
matches. Captures carry the provenance Vorbis comments (TITLE, ENCODER, DDD_VERSION,
DDD_SAMPLE_RATE_HZ, DDD_TEST_MODE, DATE).

**Acceptance criteria**
- Start from idle (opens device, monitors, records) and start from monitor (sink attach)
  both work; stop returns to monitor, not idle.
- Duration limit stops at the boundary-aligned point; a captured `.ddd.flac` passes
  `flac -t`, decodes in ld-decode tooling, and imports into Tenacity (manual check,
  recorded); Vorbis comments verified in a T2 test.

### Task 5.2 — Error surfacing

Map every engine error to specific, actionable GUI guidance, carrying over the old app's
hard-won messages: usbfs memory limit (with the `usbfs_memory_mb` instruction), buffer
overflow/underflow, USB transfer failure, file-write failure (including disk-full), 
sequence mismatch, test-ramp verification failure, device disconnect. Errors during
capture preserve what was written and finalise the FLAC container so the partial file is
readable.

**Acceptance criteria**
- Each error path triggered via synthetic-source fault injection in functional tests;
  each shows its specific message, never a generic one.
- A capture killed mid-write (process SIGKILL, T5 manual) leaves a file the ported
  `capturereader` can still read to its truncation point — matching the old app's
  unpatched-STREAMINFO handling.

### Task 5.3 — Test mode and integrity verification

Test-mode toggle (vendor request `0xB6` bit 0) with forced `TestData_<timestamp>` naming,
inline ramp verification during test captures, and the ported analyser exposed both as a
GUI action with progress/cancel and as the headless `--analyse-test-data <file>` CLI with
exit codes 0/1/2 — so the T5 procedure of TESTING.md §5 can be run against `ddd-gui`
exactly as against the old application.

**Acceptance criteria**
- `--analyse-test-data` agrees with the old application's verdict on the same files
  (pass, fail, and too-short-for-wrap cases).
- Full T5 hardware-in-the-loop pass: test-mode capture of ≥ 4 hours, zero sequence
  breaks, analysed clean — the gate for calling `ddd-gui/` a working capture application.

---

## Existing-application feature inventory and disposition

Every user-facing feature of [gui/](../gui/), with where it lands. Four dispositions:

- **Phase N** — built by this plan, in that phase.
- **Superseded** — replaced in this plan by something strictly better; nothing to revisit.
- **Retired** — deliberately dropped, with the reason recorded; a conscious decision, not
  an omission.
- **Future** — deferred beyond this plan. The rename-and-replace decision requires every
  Future row to be either implemented or consciously retired in a follow-up plan; until
  then this table is the ledger that nothing has been lost.

### Carried into this plan

| Feature (old application) | Disposition |
| --- | --- |
| Capture start/stop, elapsed/transfers/size statistics | Phase 5 (statistics much richer — Phase 4) |
| Duration limit with auto-stop | Phase 5 |
| Free-space-as-recording-time display | Phase 5 |
| FLAC output, compression 0–8, Vorbis provenance comments | Phase 5 — as native FLAC `.ddd.flac` (container superseded, below) |
| Sequence-marker verification, error taxonomy and guidance messages | Phases 2–3, 5 |
| Test mode, inline ramp verification, `TestData_` naming | Phase 5 |
| Analyse-test-data GUI + `--analyse-test-data` CLI | Phase 5 (reads `.flac` and legacy `.raw`) |
| Advanced statistics (min/max, clipped, recent window) | Phase 4 |
| USB tuning settings: queue size, small transfers, limited queue, preferred device | Phases 2–3 |
| WinUSB backend | Phase 3 |
| Window/dialog geometry persistence | Phase 1 (docks + geometry) |
| Debug logging switch | Phase 1 |
| About dialog with injected version | Phase 1 — and now a stated requirement: it is the second route to the build's commit hash, and the only one that works on Windows (Task 1.3) |
| Settings persistence with versioned migration | Phase 1 onward (new settings file; migration *from* the old app's INI is **Future**, if ever wanted) |

New in this plan, with no old-application equivalent: **monitor mode** (Phase 3), the
real-time waveform/spectrum/amplitude panels (Phase 4), the **FX3 firmware version check
with warning modal** (Task 3.1), SuperSpeed enforcement (Task 3.1), and the panel/theming
shell (Phase 1).

### Superseded

| Feature (old application) | Replaced by |
| --- | --- |
| Ogg FLAC `.ldf` container | Native FLAC `.ddd.flac` (Tasks 2.1, 5.1). Compression levels and Vorbis comments carry over. `.ldf` is neither written nor read by the new application |
| RMS amplitude number / 1 Hz chart | Phase 4 amplitude-history panel |

### Retired

| Feature (old application) | Reason |
| --- | --- |
| 16-bit `.raw` output | FLAC is the sole capture format; FLAC level 0–1 (multithreaded libFLAC ≥ 1.5) covers the low-CPU case `.raw` served. `.raw` remains *readable* for test-data analysis |
| 4:1 CD decimation | Capture is always the full 40 MSPS stream; rate reduction is a downstream processing job |
| Legacy 10-bit packed `.lds` read support | Not read, now or in future; the packed codec is not ported. The old application remains the tool for legacy files |
| VID/PID override setting | The IDs are fixed protocol constants (`0x1209:0x2347`), not user configuration |
| Windows async file I/O (overlapped `.raw` writes) | Fell with `.raw` output — libFLAC owns file output on all platforms |
| Remote button "Repeat" | Was never functional in the old application |

### Future

| Feature (old application) | Notes |
| --- | --- |
| Advanced naming: auto/manual filename modes, disc metadata fields, mint marks, per-side holdings, append-duration rename | |
| Metadata sidecar file (`serialInfo`/`namingInfo`/`captureInfo`/`timeSampledData`) | Written as **YAML** in the new application (the old app wrote JSON); the captureInfo portion may land with Phase 5; player-derived fields need player support. The YAML dependency (e.g. yaml-cpp, MIT) is subject to the GPLv3-compatibility rule |
| LaserDisc player serial control (Pioneer protocol, model detection, auto-reconnect) | |
| Player information display (model, status, position, physical mm) | |
| Player remote dialog (full transport, manual serial commands, user-code reads) | |
| Automatic capture state machine (whole disc / partial / lead-in, CAV+CLV, key-lock) | |
| "Stop player when capture stops" / "Stop capture when player stops" | With player support |
| Reset notes/mint marks on side change preference | With advanced naming |
| Flatpak / WiX / macOS packaging, desktop + metainfo files | At rename-and-replace time |

## Forward requirements — design for now, build later

Two capabilities are explicitly *not* built by any phase of this plan, but the
architecture must not obstruct them. They are recorded here so design decisions are made
with them in view.

**Command-line capture.** A future headless tool (or `ddd-gui` CLI mode) performing basic
monitor and capture from a shell: select device, choose format/destination/duration,
capture, report the same error taxonomy through exit codes and stderr, scriptable for
unattended and remote use. The design consequence is already stated in the architecture
section: all capture capability lives in the Qt-free `ddd::capture` library, and the
existing `--analyse-test-data` CLI (Phase 5) establishes the pattern of GUI-optional
entry points. No task in this plan may make device open, capture control or error
reporting depend on a running GUI.

**FPGA gateware version check.** The counterpart to Task 3.1's FX3 firmware check: query
the gateware's version over USB3 via the FX3 and warn (again a modal warning, never an
error) when it differs from the expected build. The current gateware has no version
register and the FX3 no vendor request to read one, so this needs an FPGA ↔ FX3 ↔ host
protocol addition — a change that touches all three components and must be flagged as
such (AGENTS.md §2 and §13). The design consequence now is only that the version-check
presentation in the GUI (Task 3.1) should not assume the firmware is the sole versioned
part of the device.

**Device firmware and gateware update.** A future panel for updating the FX3 firmware and
the FPGA bitstream from the application, so a user can bring a device up to date without
the developer tooling in [fx3/programmer/](../fx3/programmer/). The design consequences
now: the engine's device-discovery layer should be structured so a second USB personality
(the FX3 bootloader / programming mode) can be recognised later, and nothing in the
application may assume the capture personality is the only one the hardware presents.
The hardware-safety rule of AGENTS.md §4 applies with full force: writing the FX3 EEPROM
or the FPGA EPCS flash is permanent programming, is never part of any automated test, and
when this feature is eventually built it must be a deliberate, confirmed, user-initiated
act with no unattended path to it.

## Out of scope for this plan

Renaming `ddd-gui/` over `gui/`, packaging/installers, the player-control feature family,
the command-line capture tool and the firmware/gateware updater are all follow-up plans.
This plan is complete when Phase 5's T5 gate passes and the inventory table above
contains no row whose disposition is unaccounted for.
