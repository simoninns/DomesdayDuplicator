# ddd-gui — capture application

The capture and signal-monitoring application for the Domesday Duplicator.

This is the project's capture application, and what CI builds, tests, packages and
releases. The application it replaced was removed from the repository on 2026-08-18; its
history remains in git. The hardware capture-integrity procedure that this application is
held to is [TESTING.md](../TESTING.md) §5.

## What works today

**The application shell**: a themed, panelled window with light and dark themes, dockable
panels that can be floated into windows of their own, a View menu built from the panels
themselves, persistent window and layout state, and a log panel fed through the engine's
logging seam.

**The capture engine**. The ring buffer, the sequence validator, the metrics, the monitor
tap, the FLAC writer and reader, and the orchestrator that runs them all on their own
threads — driven either by a real device or by a synthetic source that generates the
device's stream in software at its real 80 MB/s.

**Device updates**. Tools → Firmware → Update firmware… gains an Update page: choose a
`.dddfw` bundle and the application verifies its signature and every payload digest,
compares the installed and available versions of all three parts, runs the compatibility gate, and — after one
confirmation — streams the firmware to the device, watches it write and verify it, waits
for it to restart, and reports the version it reads back off the live device. Every stage
says what is happening and how long it will take, and every failure says whether the
device is safe (it always is) and what to do next.

The same page programs a device that has *no* firmware, and that is not a separate
mechanism. A Duplicator whose EEPROM its boot ROM will not accept — one whose update was
interrupted, or one that has never been programmed at all — enumerates as the boot ROM
instead, and the application recognises it, names it *recovery mode*, and offers **Program
this device**. It hands the boot ROM the bundle's own firmware to run out of RAM, waits
for the device to come back as a Duplicator, and then installs normally: so a first-time
programming of a bare Explorer Kit runs through the same protocol and the same digest
checks as a routine update, with no jumper and no shell.

**Programming an FPGA that no update can reach.** A board whose flash has never been
written cannot be updated over USB — the route to it runs through gateware that is not
there yet — so the engine also drives the DE0-Nano's on-board USB-Blaster directly: a
libusb cable driver and a player for the JTAG vectors the bitstream build emits beside its
`.jic`. `ddd-jtag` is the shell half of it, and `ddd-jtag --dry-run` checks a programming
file with nothing attached. The knowledge of what a Cyclone IV and an EPCS64 want stays in
Quartus, at build time; see *USB-Blaster and SVF programming*.

**Bringing a board up.** Tools → Firmware → Bring up a new or legacy board… puts both
halves of that together: a nine-page flow that takes a board from any state at all to
fully up to date, ending on the application gateware with nothing left to install. Its
order is not a presentation choice — the original firmware and the current gateware drive
the same interconnect line, so the FX3 reaches its boot ROM (where it drives nothing)
before the FPGA is touched, and `BringUpOrchestrator` refuses to write anything until the
FPGA has been configured. See *Bringing up a new or legacy board*.

The engine half of all that is Qt-free, so `ddd-update` drives the identical code path
from a shell — `dev-bundle.sh && ddd-update` is the whole edit-to-running-device loop, and
it handles a device in recovery mode with no extra option. The FX3 target is complete; the
FPGA's EPCS target is refused with a clear reason until the gateware's flash bridge
exists. The protocol is on the *Device update mechanism* documentation page.

**Monitor mode**. Attach a device, press *Start monitoring*, and the signal is validated,
measured and displayed while nothing is written anywhere. The Capture panel finds devices
as they are plugged in and unplugged, refuses one attached at a speed that cannot carry
80 MB/s, and offers the gateware's test-pattern mode; the Statistics panel shows
throughput in both MB/s and Msps — the rate over the last second rather than the average
since the run began, so it reads the device's 40 Msps throughout instead of creeping up
towards it — sequence-marker integrity, buffer-queue depth with its high-water mark,
signal level, whole-run extremes, clipping counts, samples processed, link speed and the
declared front-end gain, all read from the wait-free tap.

**The signal displays**. A time-domain scope with selectable span, persistence and a
cursor readout; a spectrum with adjustable averaging, peak hold and a frequency cursor, in
either a live trace or a spectrogram — level as colour, frequency up the side
and time running left to right over a fixed window labelled in seconds, which is what makes a
drifting carrier or an intermittent interferer visible at all — over a range topping out at 14, 16, 18 or 20 MHz
(14 by default, which puts the board's 13.2 MHz filter corner just inside the
edge); and an amplitude history strip holding five minutes of min/max envelope,
RMS drawn either side of 0 V, clip events, the 75% nominal-level bounds marked
on both sides, a Clear for starting the history again after a change, and a span that
shows everything held or narrows to match the spectrogram so the two scroll at
the same pace.
All of them are QPainter-painted in the window's own colours, and all
of the analysis behind them runs on a worker thread that the pipeline never waits for.

**The front-end gain declaration**. SW401 on the Domesday Duplicator board is a mechanical
switch with no path to the FPGA, so the application cannot read it. The switch block is written as the board shows it —
`1010` is switches 1 and 3 closed, the rest open. Declare it in
*File → Settings…* and every level reads in millivolts at the BNC; leave it undeclared and
they read in converter codes, because a default would produce authoritative-looking
figures wrong by up to a factor of four. Clipping detection never depends on it.

**Capture to disk**. *Start capture* attaches a FLAC writer to the running stream — from
idle it starts the stream too, so the common case is one press. Stopping returns to
monitoring rather than to idle, which is what makes taking both sides of a disc possible
without reopening the device. Captures are written as native FLAC with the compound
extension `.ddd.flac`: a plain `.flac` to every importer, with the `.ddd` saying where the
samples came from. Choose the folder, the name (or leave it to be named after the time it
was taken), the compression level and an optional duration limit; the panel shows the
destination volume's free space as *how much capture it holds*, because that is the
question — not "is there 400 GB free" but "will this last the side I am about to play" —
and warns once when it drops below a threshold you set.

Every capture carries its own provenance in Vorbis comments: the build that produced it,
the real 40 MHz sample rate that FLAC's header cannot express, whether it was taken in test
mode, and — only when one has actually been declared — the front-end gain, so the
calibration needed to read the samples as volts travels with them.

**When a capture goes wrong**, the message names the failure, says what to do about it, and
says where the partial file is. Every failure code has its own remedy: a full disk, a bad
cable, the kernel's usbfs limit and a machine that cannot keep up are four different
answers, not one. Whatever the failure, the FLAC stream is closed properly on the way out,
so what was written is readable and reports its own length.

**Test-mode integrity checking**. With the gateware's test pattern running, every sample is
the previous one plus one, and any break is a sample the capture path lost. Test captures
are always named `TestData_` — forced, not defaulted, because a file called "Blade Runner
side 1" that turns out to be ramps is a trap. The ramp is checked live as it arrives and
again offline off the disk: *Tools → Analyse test data…* with progress and a cancel, or
`ddd-gui --analyse-test-data <file>` from a shell, exiting 0 for an intact ramp, 1 for a
break and 2 for a file it could not read — so the integrity gate of
[TESTING.md](../TESTING.md) §5 can be scripted. It agrees with the old application's verdict
on the same files.

**Both USB backends**: libusb on Linux and macOS, WinUSB on Windows, chosen at configure
time. Both refuse a device below SuperSpeed with a specific error rather than opening it
and failing later, which the old application did not.

**LaserDisc player control**, over the player's serial port and off until it is turned on —
while it is off, no serial port on the machine is opened, written to or listed. It finds the
player without being told which port or which speed, drives it from a remote window, and
**examines a disc**: type, addressing, size, side, chapters, television standard and the two
ends of the programme *measured* by seeking past each of them, with every field labelled
with how it was arrived at so a measurement and an inference do not look alike. From that
report it will capture a side by itself — the whole side including the spin-up and the
spin-down, a range between two addresses, or from the spin-up to an address — writing the
disc's own facts into the capture's provenance as it goes. None of that existed in the old
application beyond the state machine it drove blind. See
[Player control](../docs/content/capture-gui/player-control.md).

**Not yet**: the four-hour hardware-in-the-loop pass that is the gate for calling this a
working capture application, and the advanced naming and metadata sidecar of the old
application. See the plan's inventory table for the full ledger.

That the engine can be tested at all without hardware is the point of the split. The old
application could only be proven by attaching a device and hoping a fault reproduced;
here, a sequence break, a short transfer, a stalled device and a buffer overflow can each
be *asked for* and the response checked on every push. It does not replace the hardware
procedure in [TESTING.md](../TESTING.md) §5 — everything past the host's memory is still
out of reach — but it means a hardware session is spent on hardware questions.

## Building

Requires **Qt 6.5 or later** — including the **SerialPort** module, which several
distributions package separately from qtbase — CMake 3.21+, and GoogleTest. Qt 6.5 is the
floor because `QStyleHints::colorScheme()` — how the application follows the desktop's
light/dark setting — arrived in that release. SerialPort is for the LaserDisc player link,
and only the Qt layer uses it: the player protocol itself is Qt-free.

```bash
cmake -B build -S .
cmake --build build
ctest --test-dir build
```

**libFLAC** is needed too — 1.4.0 or later, and 1.5.0 or later to encode on more than one
core. It is BSD-3-Clause, so linking it into a GPLv3 application is fine.

**libusb 1.0** is needed on Linux and macOS, and not on Windows: the WinUSB backend uses
`winusb` and `cfgmgr32`, which ship with the toolchain. It is LGPL-2.1-or-later, so
linking it into a GPLv3 application is fine.

On Linux the device needs a udev rule before a non-root user can open it — without one,
enumeration finds the device but opening it fails. See
the rule shipped with the FX3 programmer,
[`fx3/programmer/configs/70-domesday-duplicator.rules`](../fx3/programmer/configs/70-domesday-duplicator.rules).

The **serial port** for player control is a separate permission and has no rule shipped with
it: a USB serial adapter is third-party hardware, so on Linux this is group membership
(`dialout`, or `uucp` on Arch) rather than anything this project installs. The application
tells a refused port apart from a busy or absent one and gives the remedy for the platform
it is running on; [TESTING.md](../TESTING.md) §7 has all three platforms, and the user-facing
version is in [Player control](../docs/content/capture-gui/player-control.md).

With Nix, from anywhere in the working tree:

```bash
nix develop .#ddd-gui     # dev shell, including the clang tooling the gates need
nix build .#ddd-gui       # the package, tests included
```

Two build inputs a packaged build passes and an ordinary one does not need:

| | |
| --- | --- |
| `-DDDD_RELEASE_UPDATE_KEY_FILE` | which minisign public key this build accepts release bundles from. Defaults to `tools/keys/release.pub` when the source tree carries it; a build with none can verify no release bundle and says so |
| `-DDDD_BUNDLED_UPDATE_FILE` | an update bundle to install beside the application, so a board can be brought up with no network. Placed under one fixed name in the platform's data location and preselected by the bring-up wizard — and verified there exactly as a downloaded file is. A build with none opens that page with its file picker. `./tools/dev-bundle.sh` produces one locally |

### Quality gates

Both run as part of an ordinary build, and both fail it:

- **clang-format** (`--dry-run --Werror`, `BasedOnStyle: Google`) over every source file,
  one stamp per file so a failure names the file.
- **clang-tidy** (`google-*`, `bugprone-*`, warnings as errors) on every target.

They are turned off in the Nix package build, and only there: that sandbox has an
unrelated clang-format version whose output differs, and an unwrapped clang-tidy that
cannot resolve the standard library headers. Neither failure would say anything about the
code. The native CI build is where the gates run.

To disable them locally — for a bisect, or on a machine without the tools:

```bash
cmake -B build -S . -DDDD_ENABLE_CLANG_FORMAT=OFF -DDDD_ENABLE_CLANG_TIDY=OFF
```

## Layout

```
src/capture/      ddd::capture — the engine. Qt-free, by rule.
src/analysis/     ddd::analysis — the display mathematics. Qt-free, for the same reason.
src/player/       ddd::player — the LaserDisc player protocol. Qt-free, and portless.
src/player/players/ one header per supported player model. See its README to add one.
src/gui/          ddd::gui — the Qt layer, built as a static library, plus main().
src/gui/resources/ the application's graphics, compiled in (a local copy, AGENTS.md §2)
src/update-cli/   ddd-update — a main() over the engine. Links no Qt, deliberately.
src/jtag-cli/     ddd-jtag — the same, for the JTAG programming path.
src/vendor/       the only third-party sources here: SHA-256 and Ed25519. See VENDOR.md.
cmake/            FindFLAC.cmake, a component-local copy (AGENTS.md §2)
tests/unit/       T1, engine. Links no Qt at all.
tests/analysis/   T1, display mathematics. Links no Qt either.
tests/player/     T1, the player protocol, against a scripted fake serial port.
                  The Qt half of the player — discovery, the controller, the
                  panel and the remote — is under tests/gui/, since it needs Qt.
tests/golden/     T2, the capture file format checked against what it must be on disk.
tests/functional/ T1, the whole pipeline at the device's rate. Minutes, not seconds.
tests/gui/unit/   T1, Qt layer. Runs under a QCoreApplication; no display needed.
tests/gui/widget/ T1, widgets. Needs a QApplication and the offscreen platform plugin.
tests/hardware/   T5, needs hardware attached, and two kinds of it: `hil` needs a
                  Duplicator, `hil-player` needs a LaserDisc player. Separate labels
                  so a bench with one and not the other runs what it can. Neither
                  runs in CI. Note that `-L hil` matches both — CTest's label
                  selection is an unanchored regex — so the device tier alone is
                  `-L "^hil$"`, while `-LE hil` correctly excludes both.
tests/support/    Fixtures shared between test binaries.
```

`src/vendor/` is a target of its own so that the quality gates that apply to this
project's code cannot apply to code this project must not edit: no `-Wall -Wextra`, no
clang-tidy, and excluded from the clang-format glob. Everything the engine sees of it is
two wrappers, `digest.h` and `minisign_verify.h`, so replacing an implementation later is
a change to two files. Its provenance, licences and refresh procedure are in
[src/vendor/VENDOR.md](src/vendor/VENDOR.md).

`src/update-cli/` is one file, and its value is negative space: it links `ddd_capture` and
nothing else, so it stops linking the moment a Qt dependency reaches the update path. That
is the same enforcement `ddd_capture_tests` provides for the capture engine.

`src/analysis/` is separate from the engine because none of it is needed to make a
capture, and separate from the GUI because a QPainter cannot be unit tested while the
arithmetic behind it can. `ddd_analysis_tests` links no Qt, which is what keeps that
boundary enforceable rather than aspirational.

`src/player/` is the same idea applied to the LaserDisc player link: it links no Qt and it
opens no port, talking to an `ISerialPort` interface instead. So the whole protocol — the
per-model definitions, the probe that finds a player and works out its baud rate, every
command's bytes and every reply's meaning — is exercised on a machine with no player and
no serial adapter attached, with the player answering late, answering wrongly, answering
at the wrong rate or going silent mid-command. The multi-step sequences are values rather
than control flow for the same reason: the examine sequence hands out the next command and
takes back what the player said, so an open tray, a refused query, a link that dies halfway
through and a cancel between any two steps are all things a test can simply state.
`QSerialPort` implements that interface in
`src/gui/`, where Qt already lives.

The functional tier is not part of the ordinary edit-build-test loop. `ctest -L unit`
skips it; a bare `ctest` runs it, and so does CI. Each soak runs for a minute by default —
`DDD_SOAK_SECONDS` shortens that for a quick local check or lengthens it for an overnight
run, and whichever it ends up as is printed, so a shortened run cannot be mistaken for a
full one.

The hardware tier is the one that needs a device, and it is run deliberately:

```bash
ctest --test-dir build -L hil
```

It refuses rather than skips when nothing is attached — a hardware test that "passes"
because there was no hardware is worse than no test. Nothing in it reprogrammes anything:
it streams and sends the `0xB6` configuration request, which is what the application does
in normal use, and writing the FX3 EEPROM or the FPGA flash stays a manual procedure
([AGENTS.md](../AGENTS.md) §4).

One of its checks is on the *rate* rather than the data. The device's output is clocked by
a 40 MHz converter, so a working one delivers 80 MB/s and physically cannot deliver more;
a higher figure means the samples are not coming from the ADC at all — an unprogrammed
FPGA, or gateware that is not the sampler. That is a diagnosis no amount of looking at
sample values would produce.

### The FFT

The spectrum panel needs a Fourier transform, and the plan proposed vendoring KissFFT or
pocketfft. Both are BSD-3-Clause and both would be fine in a GPLv3 application, so the
licence position (AGENTS.md §10) was not what decided it. The cost on the other side was:
the format and lint gates below run over every file under `src/` as errors, and vendored
code fails both immediately, so carrying it would mean building an exemption mechanism
into the gates and then maintaining it.

`src/analysis/fourier_transform.cpp` is a radix-2 Cooley-Tukey transform written for this
project instead — about eighty lines, checked in the tests against a directly evaluated
DFT that shares no code with it. The performance argument for a vendored one does not
arise here: the panel asks for at most thirty 4,096-point transforms a second, which is a
few milliseconds of one core, on a thread nothing waits for.

The Qt-free rule on `src/capture/` is load-bearing rather than tidy. Everything a capture
needs — device discovery, the transfer pipeline, validation, writers — belongs there, so
that it can be tested without a display and so that the planned headless command-line
capture tool can reuse it unchanged. `ddd_capture_tests` links no Qt, which is what makes
the rule enforceable: if the engine ever grows a Qt dependency, that binary stops linking.

## Versioning

`--version` reports the commit the binary was built from. Builds outside a git checkout
must pass it, since there is no `.git` for CMake's fallback to consult:

```bash
cmake -B build -S . -DDDD_VERSION=abcd1234
```

A release artefact reporting `unknown` fails the release gate ([AGENTS.md](../AGENTS.md)
§9).

**Help → About shows the same string**, and that is deliberate rather than duplication.
The Windows build is linked as a GUI subsystem executable, so `--version` writes to a
console that is not attached and the user sees nothing at all; on the platform with the
most installations, the dialog is the only way to identify a binary. A user reporting a
bad capture has to be able to say which build produced it. The About text is built by a
pure function (`about_text.h`) so a test can assert it carries the version — a modal
dialog cannot be checked any other way. The CI `--version` check therefore runs on Linux
and macOS only; the About test covers all three.

## Conventions

Google C++ style, enforced by the gates above; SPDX headers on every file
([AGENTS.md](../AGENTS.md) §5.4); `snake_case` filenames, `PascalCase` types and methods,
`trailing_underscore_` members, `kConstants`. Every public class documents its
thread-safety contract in its header comment — with a real-time capture pipeline arriving
in later phases, "which thread may call this" is not something to leave to inference.
