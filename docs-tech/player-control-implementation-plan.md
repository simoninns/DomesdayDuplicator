# LaserDisc Player Control (`ddd-gui/`) — Implementation Plan

## Purpose

The capture application can drive the Duplicator but not the player. Everything about
getting a disc spinning, finding the start of a side and stopping at the right place is
still done by hand, or by keeping the old application ([gui/](../gui/)) open beside the new
one. This plan closes that gap, and closes the largest remaining block of **Future** rows
in the [capture application plan](ddd-gui-implementation-plan.md) — the ledger that plan
keeps precisely so nothing from the old application is quietly lost.

It builds four things:

1. **Player control over a serial link.** When player control is enabled and a model is
   selected, the application finds the player itself: it works out which serial port the
   player is on and at what baud rate, confirms that the player which answered is the one
   the user said, and reconnects on its own when the link comes back.
2. **A player definition per model, as data in a header.** Every generic control — play,
   still, step, seek to a frame, read the disc status — maps to that model's command and
   parameter sequence in one file. Adding a player is adding a header and a registry line,
   not editing a switch statement in the middle of a transport layer.
3. **A pop-up remote control**, so the user can drive the player directly from the
   application, with the buttons the connected model actually supports.
4. **A different automatic-capture flow from the old application.** Instead of presenting
   every option and asking the user to know which apply, the application **examines the
   disc** first — type, addressing, length, video standard, what the player reports about
   the side — and then offers a guided setup showing only what is applicable to what it
   found.

Point 4 is the one behavioural departure from the old application, and it is deliberate.
The old **Automatic Capture** dialog asked for the disc type before it had looked at the
disc, offered CAV frame fields and CLV time fields side by side with both live, and failed
several seconds later with *"The disc in the player does not match the selected capture
option"* if the answer was wrong. Every fact that dialog asked for is one the player can be
asked. Asking the player first turns a form into a report.

## Authoritative references (in-tree)

| Reference | What it settles |
| --- | --- |
| [AGENTS.md](../AGENTS.md) §2, §5, §6, §8 | Component boundaries, C++ style, naming, testing obligations |
| [TESTING.md](../TESTING.md) §2, §5, §8 | Tier labels, the hardware-in-the-loop procedure, conventions for new tests |
| [ddd-gui-implementation-plan.md](ddd-gui-implementation-plan.md) | The architecture this plugs into, and the **Future** ledger this plan discharges |
| [gui/src/DomesdayDuplicator/playercommunication.cpp](../gui/src/DomesdayDuplicator/playercommunication.cpp) | The Pioneer command sequences, response formats and timeouts that are known to work on real hardware |
| [gui/src/DomesdayDuplicator/playercontrol.cpp](../gui/src/DomesdayDuplicator/playercontrol.cpp) | The automatic-capture state machine, including the spin-down/spin-up ordering that makes lead-in capture work |
| [ddd-gui/src/capture/device_monitor.h](../ddd-gui/src/capture/device_monitor.h) | The polling-rather-than-callback discovery pattern this plan copies for serial ports |
| [ddd-gui/src/gui/capture_controller.h](../ddd-gui/src/gui/capture_controller.h) | The Qt-bridge shape a `PlayerController` must match |
| [docs/content/general/laserdisc-player.md](../docs/content/general/laserdisc-player.md), [docs/content/ldv4300d/](../docs/content/ldv4300d/) | What the project already tells users about players |

The old application's player code is **the specification for the wire protocol** and is not
the specification for anything else. Its command strings, response prefixes, timeout
classes and the order of operations in automatic capture are field-proven across years of
captures and are carried over as-is. Its structure — a `QThread` subclass with a command
queue, `std::atomic` caches and the protocol interleaved with the transport — is not.

One porting hazard, recorded because it will otherwise be reproduced by eye: in
`playercommunication.cpp` the tray comments are transposed (`CO` is commented *"Open door"*
and `OP` *"Close door"*, while the behaviour is the other way round). **Trust the command
strings and the observed behaviour, not the comments.** Every command carried across is to
be checked against the Pioneer *Level III User's Manual* command set and confirmed on a
bench player before its definition header is called finished.

## Hard constraints

These are decided, and every task below is subject to them.

- **The protocol layer is Qt-free and transport-free.** Command encoding, response parsing,
  the player definitions, the connection state machine, the examine sequence and the
  automatic-capture sequence all live in a new `ddd_player` library that links no Qt and
  opens no port. It talks to an `ISerialPort` interface. This is the same rule that makes
  `ddd_capture` testable without a Duplicator, applied to the player: the entire protocol
  becomes unit-testable against a scripted fake port, on a machine with no player, no
  serial adapter and no display.
- **Qt owns the port and the thread, in exactly one place.** `QSerialPort` (Qt 6
  `SerialPort` module, already a dependency of [gui/](../gui/)) implements `ISerialPort`
  behind the interface, and one worker thread owns the session. Nothing in the GUI thread
  ever waits on a serial read.
- **Serial is blocking and slow, and the design must say so.** At 1200 baud a status poll
  is tens of milliseconds and a tray-open is tens of *seconds*. Every command carries a
  timeout class from its definition, and every user-visible operation is cancellable.
- **Probing writes bytes to devices that may not be players.** Auto-detection is opt-in,
  remembers the port that worked, tries that port first, and never touches a port the user
  has excluded. See *Risks and safety* — this is the one part of this plan that can affect
  equipment outside the project.
- **Player control never becomes a precondition for capture.** Every existing capture path
  keeps working with player control disabled, absent, or broken mid-session. A player that
  stops answering degrades the application to what it does today; it does not fail a
  capture that is running.
- **Detection is reported honestly.** Anything the examine step could not determine is shown
  as *unknown* and the guided setup asks for it. A field the application guessed is
  labelled as a guess. This matters most for the video standard, where the player is not
  always able to say (*Open decisions*, below).
- **Nothing here touches the FPGA ↔ FX3 ↔ host protocol** and nothing here writes
  non-volatile memory on any device. Player control is a second, independent link.

## Architecture overview

```
                    ┌──────────────────────────────────────────────┐
   Qt / GUI thread  │ PlayerPanel   PlayerRemoteDialog             │
                    │ ExamineDialog GuidedCaptureDialog            │
                    │                     │                        │
                    │              PlayerController ───────────────┼──► CaptureController
                    │              AutoCaptureController           │    (existing)
                    └─────────────────────┬────────────────────────┘
                                          │ queued signals/slots
                    ┌─────────────────────┴────────────────────────┐
   Player thread    │ PlayerWorker — owns the session, polls status │
                    │        QtSerialPort : ISerialPort            │
                    └─────────────────────┬────────────────────────┘
                                          │
                    ┌─────────────────────┴────────────────────────┐
   ddd_player       │ PlayerSession  DiscExaminer  AutoCaptureSeq  │
   (no Qt, no I/O)  │ CommandEncoder ResponseParser                │
                    │ PlayerDefinition + players/*.h registry      │
                    └──────────────────────────────────────────────┘
```

Three properties of that split are the point of it:

**The protocol has no clock and no port.** `PlayerSession` is driven by a caller that hands
it bytes and takes bytes away. A test can therefore drive a whole connect-probe-examine
sequence in microseconds, inject a garbled reply at any step, and assert on what was sent.

**The sequences are values, not control flow.** `DiscExaminer` and `AutoCaptureSequence`
are step machines that return *the next thing to do* rather than doing it. The old
application's automatic capture is 400 lines of blocking calls inside a `QThread::run()`
and is untestable by construction; the same logic as a step machine is testable line for
line, including every failure branch, with no player attached.

**One bridge object, like the capture side.** `PlayerController` is to the player what
`CaptureController` is to the engine: the only place where a QObject and the worker thread
are in scope together. `AutoCaptureController` is the only object that knows about both
controllers, which keeps the coupling between "capture is running" and "disc is playing" in
a single, testable place.

### File layout

```
ddd-gui/src/player/                 ← new library: ddd_player, links no Qt
  CMakeLists.txt
  player_definition.h               the schema every model is described in
  player_command.h                  the generic command and query enums
  player_capabilities.h             what a model can do, as flags
  player_registry.{h,cpp}           every definition, and lookup by ID code
  command_encoder.{h,cpp}           definition + argument → bytes on the wire
  response_parser.{h,cpp}           bytes on the wire → typed results
  serial_port.h                     ISerialPort, and the line settings a probe needs
  player_session.{h,cpp}            probe, identify, connect, command, disconnect
  player_status.h                   the polled status value type
  disc_profile.h                    what an examination found
  disc_examiner.{h,cpp}             the examine step machine
  auto_capture_plan.{h,cpp}         what the guided setup produces, and its validation
  auto_capture_sequence.{h,cpp}     the automatic-capture step machine
  players/
    README.md                       how to add a player — the contract, step by step
    pioneer_level_iii.h             the shared Pioneer Level III command set
    pioneer_ld_v4300d.h             per model: identity, deltas, capabilities
    pioneer_ld_v8000.h
    pioneer_ld_v4400.h
    pioneer_ld_v4200.h
    pioneer_ld_v2200.h
    pioneer_cld_v5000.h
    pioneer_cld_v2800.h
    pioneer_cld_v2600.h
    pioneer_cld_v2400.h
    pioneer_vc_v330.h

ddd-gui/src/gui/                    ← additions to the existing Qt layer
  qt_serial_port.{h,cpp}            ISerialPort over QSerialPort
  serial_port_scanner.{h,cpp}       candidate ports, from QSerialPortInfo
  player_worker.{h,cpp}             the thread that owns the session
  player_controller.{h,cpp}         the Qt bridge
  player_settings.{h,cpp}           persisted player configuration
  player_text.{h,cpp}               every user-visible string, as pure functions
  player_panel.{h,cpp}              the dock: connection state and live status
  player_remote_dialog.{h,cpp}      the pop-up remote
  examine_dialog.{h,cpp}            "Examine disc", its progress and its report
  guided_capture_dialog.{h,cpp}     the setup built from the examination
  auto_capture_controller.{h,cpp}   player ↔ capture coupling
```

`player_text.{h,cpp}` follows the established pattern of
[firmware_text.h](../ddd-gui/src/gui/firmware_text.h) and
[update_text.h](../ddd-gui/src/gui/update_text.h): the wording a user meets is a pure
function of a state value, so it can be tested without a widget and cannot drift between
the panel, the dialog and the log.

## The player definition schema

This is the centre of the plan, so it is specified before the phases that build it.

A `PlayerDefinition` is **plain `constexpr` data**. No virtuals, no per-model subclasses, no
code in a definition header beyond composing one of these:

```cpp
struct PlayerDefinition {
  // --- Identity -----------------------------------------------------------
  std::string_view name;          // "Pioneer LD-V4300D", as shown to the user
  std::string_view id_code;       // "15" — what the model request replies with
  std::string_view manufacturer;  // "Pioneer"

  // --- How to find it -----------------------------------------------------
  ProbeSpec probe;                // request, expected reply shape, baud rates to try

  // --- What it can do -----------------------------------------------------
  PlayerCapabilities capabilities;

  // --- What to send -------------------------------------------------------
  // Indexed by PlayerCommand. A default-constructed entry means "this model
  // does not have this command", which is checked against `capabilities` at
  // compile time by a static_assert in the registry.
  std::array<CommandSpec, kPlayerCommandCount> commands;

  // --- How to read what comes back ---------------------------------------
  DiscStatusDecode disc_status;   // how this model's ?D reply is laid out
  StateDecode player_state;       // how its active-mode reply maps to states
};
```

with the argument encoding described as data rather than as a format string:

```cpp
struct CommandSpec {
  std::string_view prefix;        // "FR"
  std::string_view suffix;        // "SE"
  ArgumentEncoding argument;      // kNone | kDecimal | kDecimalPadded
  uint8_t argument_width;         // 5 for a CAV frame, 7 for a CLV time code
  ResponseKind response;          // kAcknowledge | kNumeric | kText | kNone
  TimeoutClass timeout;           // kNormal (5 s) | kLong (30 s)
};
```

Four consequences, each of which is why the schema is shaped this way:

**A printf-style format string would be a hole in the type system.** `"FR%uSE"` cannot be
checked, cannot be swept by a test, and gives a definition author a way to write a command
that formats a frame number into a tray command. Prefix/suffix/width/encoding can be
validated for every registered model in one table-driven test, and the encoder is one
function with one set of edge cases (width overflow, negative, absent argument).

**Capabilities and the command table cannot disagree.** A model that declares
`supports_chapter_search` and leaves `kSeekChapter` empty fails a `static_assert`. This is
the failure mode that matters in practice: a remote button that is enabled and does
nothing.

**Composition, not inheritance.** `pioneer_level_iii.h` defines the shared command set as a
`constexpr` function returning a `PlayerDefinition`; a model header calls it and overrides
what differs:

```cpp
inline constexpr PlayerDefinition kPioneerLdV8000 = [] {
  PlayerDefinition definition = PioneerLevelIII();
  definition.name = "Pioneer LD-V8000";
  definition.id_code = "06";
  // Only this model, and only from firmware A9, can report the physical
  // position of the optical assembly — see PhysicalPositionSpec.
  definition.capabilities.physical_position = PhysicalPosition::kFirmwareGated;
  definition.commands[kReadPhysicalPosition] = {"2962MQ", "", kNone, 0, kText, kNormal};
  return definition;
}();
```

Nine of the ten launch models are the Level III set unchanged; the deltas are the
interesting part, and this makes them the only part a reader has to read.

**An unrecognised player still works, and says so.** A model request that answers with the
expected shape but an ID code no definition claims resolves to a **generic Pioneer Level
III** definition carrying `is_generic = true`. The application then reports *"Unrecognised
player (ID 44) — using the generic Pioneer command set; some controls may not work"*, which
is both true and useful. The old application did the same thing silently; saying it is the
improvement.

### `players/README.md` — the contract for adding a player

A step list, not prose: copy the nearest model's header; set `name`, `id_code`,
`manufacturer`; adjust capabilities; override only the commands that differ; add one line
to `player_registry.cpp`; add one row to the registry test's expectation table; run
`ctest -L unit`; then the bench checklist from TESTING.md — the manual sequence that must
be walked with the real player before the model is listed as supported. The README states
plainly that a definition which has never met its player is **untested**, and how the
application labels it as such.

## Phase 1 — The protocol, without a serial port

Everything in this phase is host-native and testable with nothing plugged in. Nothing here
opens a port, and no file in it includes a Qt header.

**Status: built.** `ddd_player` exists with the ten definitions, the encoder, the parser,
the session and 66 T1 tests. Three details differ from the sketch above, each for a reason
recorded in the code:

- **No padded argument encoding.** The Pioneer wire format is unpadded decimal, so
  `kDecimalPadded` would have been a code path nothing exercised. `ArgumentEncoding` has
  one member; adding a second is a line in the enum and a branch in the encoder.
- **A definition points at its probe rather than carrying one.** Pointer equality is then
  what makes "the distinct probes across the registry" a list the session can iterate
  without deduplicating anything.
- **No stop-register commands.** The old application declared `setStopFrame()` and
  `setStopTimeCode()` and both were stubs that sent nothing, so there is no known-good
  sequence to carry over and none was invented. Phase 5 watches the address instead, which
  is what the old application actually did.

One improvement over the old application worth naming, because it is a class of bug rather
than a tidy-up: a command that comes out longer than a player accepts is now a **refusal**.
The old application truncated to twenty characters on the way out, which turns an over-long
command into a different, perfectly valid one — a seek to the wrong frame rather than an
error, with nothing afterwards to show it happened.

### Task 1.1 — Library skeleton and build wiring

Create `ddd-gui/src/player/` as the `ddd_player` target: C++20, `-Wall -Wextra`, under the
component's existing clang-format and clang-tidy gates, SPDX headers on every file from the
first commit. Link `ddd_gui_lib` against it. Add `Qt6::SerialPort` to the component's
`find_package` line and to `ddd_gui_lib` only — `ddd_capture` and `ddd_player` must both
still link with no Qt at all, which is what the existing `ddd_capture_tests` binary proves
by continuing to link. Add `qt6.qtserialport` to
[ddd-gui/package.nix](../ddd-gui/package.nix)'s `buildInputs` (as
[gui/package.nix](../gui/package.nix) already does) and confirm `nix build .#ddd-gui`.

**Acceptance criteria**
- `cmake -B ddd-gui/build -S ddd-gui && cmake --build ddd-gui/build` succeeds with only
  distribution packages, Qt ≥ 6.5 including the SerialPort module.
- `nix build .#ddd-gui` and `nix flake check` pass.
- A new `ddd_player_tests` binary exists, links **no Qt**, and runs under `ctest -L unit`.
- `./tools/check-licence-headers.sh` passes with the new tree present.

### Task 1.2 — The definition schema, the registry, and ten players

Implement `player_definition.h`, `player_command.h`, `player_capabilities.h`,
`pioneer_level_iii.h` and the ten model headers, with `player_registry.{h,cpp}` exposing
lookup by ID code, iteration over every registered model, and the generic fallback.

The generic command enum is derived from what the old application's `PlayerCommunication`
already exposes, which is the set the remote and the automatic capture need: tray open and
close; play, pause, still, stop, and *play with stop codes disabled*; step and scan forward
and reverse; multi-speed forward and reverse and the speed setting; seek by frame, by time
code and by chapter; the stop-register writes for frame and time code; on-screen display;
audio channel selection; key lock; and the queries — active mode, current address, disc
status, standard user code, Pioneer user code, model request, and physical position.

Two of those deserve their own note in the header, because they are not obvious and are
load-bearing:

- **Play with stop codes disabled** is `PL` followed by `64RB` (enable audio during
  multi-speed) and `MF` (multi-speed forward) as one sequence. It exists because a CAV disc
  with stop codes will pause part way through a whole-disc capture, and this is how the old
  application got past that. It is a compound command, which is why `CommandSpec` allows a
  suffix as well as a prefix.
- **Physical position** (`2962MQ`) reads a V25 memory location on the LD-V8000 and is valid
  on that model at firmware A9 only. The reply is a little-endian hexadecimal count of
  10 μm units, and turning that into millimetres includes a byte swap. It is capability-
  gated on *model and firmware version*, which is why `PlayerCapabilities` carries a
  tri-state rather than a bool for it.

**Acceptance criteria**
- T1 sweep over the whole registry: every definition has a unique ID code and name, every
  capability flag has a non-empty command, every command has a valid encoding/width pair,
  and no two definitions share an ID code.
- Looking up each of the ten known ID codes returns the expected model name; an unknown
  code returns the generic definition with `is_generic` set.
- The registry is usable in a constant expression — the sweep test asserts what it can at
  compile time, so a malformed definition fails the build rather than a test.

### Task 1.3 — Encoding and parsing

`CommandEncoder` turns a `(definition, command, argument)` into the bytes to write,
including the `\r` terminator. `ResponseParser` turns a reply into a typed result: an
acknowledgement, an error (any reply containing `E`, per the Pioneer convention the old
application relies on), a number, or text — plus the address parsing, which is where the
subtleties are:

- The current-address reply may be prefixed `<` for lead-in or `>` for lead-out. Those
  prefixes are not decoration: knowing the player is in the lead-in is what makes
  capture-from-lead-in work, and the flags are carried in the parsed result rather than
  stripped and forgotten.
- A frame address is the first five digits; a time code is the first seven. The same reply
  is read either way depending on the disc type, which is why the parser takes the
  addressing mode as an argument rather than guessing.
- An empty reply is *no answer*, and is distinct from a zero address. The old application
  returned `-1` for both a timeout and a failure to parse; the typed result separates
  "timed out", "player refused" and "answered with something unparseable", because those
  three want three different messages and, in the automatic capture, three different
  responses.

**Acceptance criteria**
- T1: every command in every registered definition encodes to the exact byte sequence the
  old application sends, checked against a committed table — this is the test that proves
  the port did not change the wire protocol.
- T1: argument width overflow, negative arguments and arguments supplied to a
  no-argument command are refused rather than truncated.
- T1: address parsing covers lead-in, lead-out, both widths, empty, short, non-numeric and
  error replies.

### Task 1.4 — `ISerialPort`, the session, and the fake port

`ISerialPort` is deliberately small: open with line settings, close, write bytes, read with
a deadline, and report whether it is open. `PlayerSession` sits on top and owns the
lifecycle:

**Probing.** For a candidate port, for each baud rate in the definition's `ProbeSpec` (9600,
4800, 2400, 1200 — 8 data bits, no parity, one stop bit, no flow control), send the model
request and look for a reply of the expected shape. Auto-detect uses a short per-attempt
timeout with a small number of retries; a user-fixed baud rate uses a long timeout and one
attempt, because at a known rate a slow answer is still an answer. Both come from the
definition rather than from constants in the transport, so a future non-Pioneer player with
a different probe is a header change.

**Identification.** The reply carries the ID code and a firmware version. The session
resolves those to a definition and reports *what answered*, which the layer above compares
against *what the user selected*. A mismatch is a first-class outcome, not an error: the
application says "you selected an LD-V4300D; an LD-V8000 answered on `/dev/ttyUSB0`" and
offers to switch.

**Command execution** is one path — encode, write, await reply within the command's timeout
class, parse — so every command gets identical timeout and error handling, which the old
application's per-command duplication did not guarantee.

The test harness is a `FakeSerialPort` in `ddd-gui/tests/support/`: scripted request/reply
pairs, with the ability to answer late, answer wrongly, answer at the wrong baud rate, go
silent part way through a sequence, or vanish entirely.

**Acceptance criteria**
- T1: auto-detect finds a fake player at each of the four rates, and reports failure after
  exhausting them when nothing answers.
- T1: a port that answers with a different model's ID code produces the mismatch outcome,
  not a connection.
- T1: a port that answers only after the deadline produces a timeout, and the session is
  left closed and reusable rather than half-open.
- T1: a link that dies mid-command is reported as a disconnection, once.

## Phase 2 — The link: discovery, the worker, and the panel

The protocol meets a real port. From here on, some acceptance criteria need a player on the
bench (T5).

### Task 2.1 — `QtSerialPort` and the port scanner

`QtSerialPort` implements `ISerialPort` over `QSerialPort` — the only file in the tree that
includes a `QtSerialPort` header. `SerialPortScanner` enumerates candidates via
`QSerialPortInfo` and returns, for each, the system path, a human description, whether it is
currently busy, and enough identity (vendor/product where the platform supplies it) to sort
likely adapters above unlikely ones.

**Enumeration is polled, for the same reason device discovery is.** The rationale in
[device_monitor.h](../ddd-gui/src/capture/device_monitor.h) applies unchanged: no
cross-platform port-arrival notification exists that behaves the same way on Linux, macOS
and Windows. A USB serial adapter appearing is not a time-critical event, so this polls
considerably slower than the USB monitor — a couple of seconds — and only while player
control is enabled and disconnected.

**Acceptance criteria**
- T1: the scanner's filtering, ordering and busy-port exclusion are pure functions over a
  supplied port list and are tested as such, with no ports opened.
- Manual, per platform: with a USB serial adapter attached, the port appears in the list
  within one poll interval and disappears when unplugged.

### Task 2.2 — Discovery: finding the player by itself

The auto-detection loop, in the order it tries things — the order is the design:

1. **The remembered port.** If the last successful connection is still present, probe it
   first, at its remembered baud rate. On a machine that has connected before, detection is
   one probe and takes well under a second.
2. **The user's chosen port,** if they have fixed one. A fixed port is never departed from:
   if the player is not there, the application says so rather than searching elsewhere.
3. **A scan of remaining candidates,** newest-looking first, skipping busy ports and any
   port on the exclusion list. Each is probed at each baud rate until one answers.

Failure to find anything is a *state*, not a modal — reported in the panel and in the
status bar, with a retry that is also automatic on a slow timer. Reconnection after a link
loss uses the same path, remembered port first, with backoff so that a player that is
switched off does not produce a probe every second for the rest of the session.

**Acceptance criteria**
- T1, against fakes: remembered-port-first ordering; a fixed port never falls back to a
  scan; excluded ports are never opened; backoff grows and resets on success.
- T5: with a player attached and no configuration at all, enabling player control connects
  and identifies the model within a few seconds; power-cycling the player reconnects
  without user action; unplugging the adapter reports a disconnection and does not spin.

### Task 2.3 — The worker, the controller and the settings

`PlayerWorker` owns the session on its own thread and does three things: executes queued
commands, polls status, and reports. `PlayerController` is the Qt bridge — enable/disable,
selected model, connection state, latest status, command submission, and signals for
everything the interface shows.

**Status polling.** Active mode and current address, at a rate the user can perceive but
which cannot saturate a 1200-baud link — around 4 Hz at 9600 baud, backed off automatically
at lower rates by measuring how long the previous poll took. Polling pauses while a
multi-step sequence (examine, automatic capture) owns the session, because interleaving a
status query into the middle of a seek is how a reply gets attributed to the wrong command.

**Settings**, persisted alongside the existing capture settings and following
[capture_settings.h](../ddd-gui/src/gui/capture_settings.h)'s value-struct pattern: player
control enabled; selected model (or "detect automatically"); serial port (or "find it");
baud rate (or "detect"); excluded ports; and the two coupling preferences introduced in
Phase 5. Loaded with clamping rather than rejection, as the capture settings are.

**Acceptance criteria**
- T1: the controller reaches every connection state — disabled, searching, connected,
  model-mismatched, disconnected — against a fake port, driven through a `QCoreApplication`
  event loop as the existing `ddd_gui_tests` binary does.
- T1: settings round-trip, including the "detect" sentinels; a hand-edited settings file
  with a nonsensical baud rate loads as "detect" rather than failing.
- T1: no command is ever issued on the GUI thread; the controller's public API returns
  immediately in every state.

### Task 2.4 — The player panel and the menu

A dock panel, taking its place beside the existing panels with a `View ▸ Panels` entry from
its own toggle action: connection state and how it was reached (port, baud rate, model,
firmware version), live status (player state, disc type, current address, and physical
position where the model supports it), and buttons for **Remote…** and **Examine disc…**.

A `Player` menu carries the same two entries plus **Connect / Disconnect** and
**Settings…**, so the feature is reachable when the panel is closed. The status bar gains
the player's state alongside the capture state, since the status bar is the one thing that
cannot be hidden.

Every string comes from `player_text.cpp`, and the log gets the connection lifecycle at
info level and the command traffic at debug level — a serial trace in the existing Log
panel is what makes a misbehaving player diagnosable remotely, and it is why the old
application's `qDebug()` calls were the first thing anyone was asked for.

**Acceptance criteria**
- Widget test: the panel builds and lays out with a null controller, exactly as the
  existing panels do, so the whole interface remains testable with no player.
- Widget test: each connection state produces its own wording, and the controls that
  require a connection are disabled without one.
- T1: `player_text` is a pure function of state and is tested without a widget.

## Phase 3 — The remote control

A **non-modal** pop-up dialog, so the user can drive the player while watching the spectrum
and waveform panels — which is exactly what setting up a capture consists of, and which the
old application's modal-feeling dialog made awkward.

Contents, carried from the old remote and then trimmed: transport (reject, play, pause,
still, step forward/reverse, scan forward/reverse, multi-speed forward/reverse with a speed
selector); the numeric entry with chapter/frame/time addressing and search; display and
audio; key lock; user-code reads (standard and Pioneer); and a manual command field with
the raw reply shown verbatim.

Three departures from the old dialog:

- **Buttons come from capabilities.** A control the connected model does not support is
  disabled with a tooltip saying which model it needs, rather than present and inert. The
  old dialog offered every button to every player.
- **The addressing selector follows the disc.** With a CLV disc loaded, frame entry is not
  offered. This is the same principle the examine flow is built on, applied to the remote.
- **`Repeat` is gone.** It was never functional in the old application and is already
  recorded as *Retired* in the capture application plan's ledger.

The manual command field is retained deliberately, including for generic/unrecognised
players: it is the tool that lets a user work out what an undocumented player does, and
therefore the tool that lets a new definition header be written.

**Acceptance criteria**
- Widget test: every button emits the command the definition maps it to, against a fake
  player; the capability gating disables the right controls for a model that lacks them;
  the addressing selector follows the disc type.
- Widget test: a manual command's reply is displayed verbatim, including an error reply.
- T5: on a real player, every enabled control does what its label says. This is a checklist
  in TESTING.md, walked per newly supported model.

## Phase 4 — Examining the disc

The first half of the new automatic-capture flow, and the part with no equivalent in the
old application.

### Task 4.1 — `DiscProfile` and the examine sequence

`DiscProfile` is a value type holding what an examination found, every field carrying its
provenance — *measured*, *reported by the player*, *inferred*, or *unknown*:

| Field | How it is obtained |
| --- | --- |
| Disc present, tray state | Active-mode query |
| Disc type (CAV / CLV) | Disc status query, decoded per the model's `DiscStatusDecode` |
| Addressing (frame / time code) | Follows from disc type |
| Disc size, side, CX where reported | Disc status decode — model-dependent, `unknown` where the model does not report it |
| Length: lead-out frame or time code | Seek to an impossible address, then read the current address — the technique the old application already used to find the disc end |
| Lead-in reachable | Whether the lead-in flag is seen when seeking to the start |
| Chapters present | Chapter search response, plus the Pioneer user code |
| Standard and Pioneer user codes | Their queries |
| Video standard | See *Open decisions* — from the player where the model reports it, otherwise inferred or declared, and always labelled with which |
| Estimated capture size and duration | Length × the current capture settings' `EstimatedBytesPerSecond()` |

`DiscExaminer` is a step machine over that list: each step names the command to send, what a
good reply looks like, and what an unusable reply means for the profile. **Failure is
partial, not total.** A player that cannot report the disc size still yields a profile with
the type and the length in it, and the guided setup asks only about what is missing. The
old application's equivalent gave up on the first refusal.

The sequence is cancellable between every step, and it puts the player back roughly where
it found it: examination spins the disc up and seeks, which is unavoidable, but it does not
leave a disc playing.

**Acceptance criteria**
- T1: a complete examination against a scripted fake player yields the expected profile for
  a CAV disc, a CLV disc, an empty tray, and an open tray.
- T1: each individual step failing leaves its field `unknown` and the rest of the profile
  intact; the sequence never aborts the whole examination for one refused query.
- T1: cancellation between any two steps leaves the examiner in a state that can be started
  again.
- T5: on real CAV and CLV discs, the reported length matches the disc's actual lead-out to
  the frame/second, and the whole examination completes in a time worth waiting for
  (target: under 30 s, dominated by spin-up and the two long seeks).

### Task 4.2 — The examine dialog

**Examine disc** from the panel, the `Player` menu, or the guided-capture entry point.
Progress is per step and named in plain language ("spinning up", "reading disc status",
"finding the end of the side"), with cancel live throughout. What it leaves behind is a
report the user can read: what was found, how each fact was obtained, and what could not be
determined — followed by **Set up capture…**, which carries the profile into Phase 5.

The report is also copyable as text, because it is the thing a user will paste into an
issue when a disc behaves strangely.

**Acceptance criteria**
- Widget test: driven by a fake player, the dialog shows each step, completes, and produces
  the report; cancelling mid-sequence returns to the idle state with no capture set up.
- Widget test: a profile with unknown fields renders them as unknown rather than as blanks
  or zeroes.

## Phase 5 — Guided capture setup and automatic capture

### Task 5.1 — The guided setup

Built **from** the profile, showing only what applies:

- **CAV** offers frame addressing: whole side, a frame range, or from the lead-in for a
  given number of frames. The maximum comes from the measured lead-out, so a range that
  cannot exist cannot be typed.
- **CLV** offers time-code addressing on the same three shapes, bounded the same way.
- Anything the examination could not determine is asked for here, once, with what the
  application believes prefilled and marked as a guess.
- The estimated file size and duration are shown against the destination volume's free
  space, using the capture settings' existing estimator. A setup that will not fit is
  flagged before the disc starts spinning rather than by the low-space warning forty
  minutes in.
- The capture name is offered prefilled from the disc facts where they are known, without
  imposing a scheme — full advanced naming remains **Future**.
- Key lock, and the two coupling preferences, are here as checkboxes with the current
  setting.

`AutoCapturePlan` is the value this dialog produces and `ValidateAutoCapturePlan()` is a
pure function over it, so the interface's enablement rules and the sequence's preconditions
are the same rules rather than two copies of them.

**Acceptance criteria**
- T1: validation covers every invalid plan — end before start, range beyond the measured
  length, zero-length lead-in capture, a plan whose addressing does not match the profile.
- Widget test: a CAV profile offers no time-code controls and a CLV profile offers no frame
  controls; bounds come from the profile; the size estimate tracks the range and the
  current output format.

### Task 5.2 — The automatic-capture sequence

`AutoCaptureSequence` is the old application's state machine, re-expressed as a step
machine and with its ordering preserved exactly, because that ordering is what makes the
result usable:

1. Key lock on, if asked, so the front panel cannot interfere.
2. Confirm the disc still matches the plan (it may have been swapped since the examination).
3. Spin down, when the capture starts from the lead-in or covers the whole side. This is
   the non-obvious step: the lead-in is only readable on the way up from a stop, so capture
   must start *before* the player does.
4. Start the capture, then spin up — in that order, with play-with-stop-codes-disabled for
   CAV so a stop code cannot pause the disc mid-side.
5. Or, for a partial capture, seek to the start address, then start the capture, then play.
6. Watch the current address until the end address is reached, with a stall detector: an
   address that has not advanced for several polls while the player claims to be playing
   ends the capture with a specific message rather than running until the disk fills.
7. Stop the capture, stop the player, release the key lock.

Cancellation is honoured between every step and, unlike the old application, during the
watch phase — the capture is finalised properly rather than abandoned.

**Acceptance criteria**
- T1: each of the three capture shapes (whole side, range, from lead-in) produces the
  expected step order for both CAV and CLV, asserted step by step against fakes.
- T1: every failure branch — disc swapped, spin-up refused, seek refused, link lost
  mid-capture, address stalled, user cancelled — ends with the capture stopped, the player
  stopped and its own message.
- T1: the sequence never issues a start-capture without a matching stop, in any branch.
  This is the property test that matters most: a leaked capture writes until the volume
  fills.

### Task 5.3 — Coupling to the capture engine

`AutoCaptureController` is the only object holding both a `PlayerController` and a
`CaptureController`, and it implements the two preferences from the old application's
ledger:

- **Stop the player when the capture stops.** Safe, and on by default.
- **Stop the capture when the player stops.** Off by default, and this is a considered
  default rather than timidity: a player that briefly reports a stopped state mid-side —
  which happens on a disc with a defect — would truncate a good capture. When enabled, it
  waits for the state to persist across several polls before acting.

The profile's disc facts are written into the capture's existing provenance so the file
records what disc it came from — the reader for which already exists in
[capture_provenance.h](../ddd-gui/src/capture/capture_provenance.h). The full metadata
sidecar remains **Future**.

Failure of the player mid-capture never destroys the capture: the link dropping stops the
automation, reports it, and leaves the capture running under manual control.

**Acceptance criteria**
- T1: an automatic capture end to end against a fake player and the existing fake USB
  backend — no hardware, no player — producing a file with the expected provenance.
- T1: link loss mid-capture leaves the capture running and reports the automation as ended.
- T5: whole-side CAV and CLV captures on real discs, with the file's first frames confirmed
  to contain the lead-in for a lead-in capture.

## Phase 6 — Hardware validation, documentation and support

### Task 6.1 — The bench procedure

A new TESTING.md section for player control, in the shape of the existing update
procedures: what to have ready, the per-model checklist (connect and identify at each baud
rate the model supports; every remote control; examine a CAV disc and a CLV disc; the three
automatic capture shapes; a mid-capture cancel; a mid-capture unplug), and what a pass
looks like.

The automated T5 tests get their own label. Existing `hil` tests need a Duplicator; these
need a **player**, and a machine with one and not the other must not fail tests it cannot
run. TESTING.md §2's T5 row gains the `hil-player` label beside `hil`, keeping one label per
test, and the player tests are run with `ctest -L hil-player`.

### Task 6.2 — Documentation

- `ddd-gui/src/player/players/README.md` — the contract for adding a player (Task 1.2).
- A `docs/content/capture-gui/` page on player control: enabling it, what auto-detection
  does, the examine flow, the guided capture, and what to do when the player is not found.
  It states plainly which models have been tested on real hardware and which have
  definitions that have not yet met their player.
- `ddd-gui/README.md` gains the serial dependency and the new library in its layout section.
- The **Future** rows this plan discharges are struck from
  [ddd-gui-implementation-plan.md](ddd-gui-implementation-plan.md)'s ledger with a pointer
  here — the ledger is only useful if it is kept.

### Task 6.3 — Packaging and permissions

Serial port access is a permission problem on every platform and it will be the single most
common support question:

- **Linux:** membership of `dialout` (or `uucp`). The project already ships a udev module
  ([nix/modules/udev.nix](../nix/modules/udev.nix)) for the Duplicator; a serial adapter is
  a third-party device and is *not* something this project should be writing rules for, so
  this is documentation, and the application detects a permission failure and says exactly
  what it means rather than reporting a generic open failure.
- **Flatpak:** the manifest needs `--device=all` or an explicit serial device permission;
  confirm and record which.
- **macOS and Windows:** driver notes for the common USB serial adapters, and confirmation
  that the port naming the scanner shows matches what users see elsewhere.

**Acceptance criteria**
- A permission-denied port open produces the platform-specific explanation, tested by
  injection (T1) and confirmed once per platform by hand.
- The Flatpak build connects to a player, or the manifest is corrected until it does.

## Test plan summary

| Tier | Where | What it covers |
| --- | --- | --- |
| T1 `unit` | `ddd_player_tests` (links no Qt) | The registry sweep, encoding against the committed byte table, response parsing, the session's probe/identify/timeout paths, the examiner, plan validation, the automatic-capture sequence step by step and failure by failure |
| T1 `unit` | `ddd_gui_tests` | The controller's state machine, settings round-trip, `player_text`, the coupling controller against fake player and fake USB backends |
| T1 `unit` | `ddd_gui_widget_tests` | Player panel, remote dialog, examine dialog, guided setup — all with a null controller and with fakes |
| T5 `hil-player` | `ddd_player_hardware_tests` | Connect and identify against a real player; a real examination against a known disc |
| T5 manual | TESTING.md | The per-model checklist, and the automatic capture procedures on real discs |

The property worth stating separately, because it is the reason for the whole layering:
**every line of player logic in this plan is exercised with no player attached.** Hardware
proves the wire protocol and the timing; it is not where the logic is tested.

## Risks and safety

**Probing writes to serial devices that are not players.** A scan sends a few bytes to every
candidate port. On a machine where a serial port is a UPS, a scientific instrument or a
device that reprograms itself, that is not free. Mitigations, all required rather than
optional: auto-detection only runs when player control is explicitly enabled; the remembered
port is tried first so a configured machine scans nothing; ports can be excluded and
exclusions persist; a busy port is skipped rather than contended for; and the
documentation states what probing does. If the user fixes a port, no other port is ever
opened.

**The player is a mechanism.** Commands spin discs, move optical assemblies and open trays.
Tray control stays a deliberate user action and is never part of an automatic sequence.

**Truncating a good capture** is the failure mode of "stop capture when the player stops",
which is why it is off by default and debounced when on.

**A leaked capture** — a sequence branch that starts a capture and never stops it — fills
the volume. It is covered by a property test rather than by example tests.

**Model definitions written without the hardware** will be wrong in places. They are
labelled untested in the interface and in the documentation until somebody walks the bench
checklist with that model, and the generic fallback means an unrecognised or misbehaving
player still gets a usable manual-command path.

Nothing in this plan touches the FPGA ↔ FX3 ↔ host protocol, the capture data path, or any
non-volatile memory on any device. AGENTS.md §4's programming prohibition is not engaged.

## Open decisions

Two things this plan cannot settle from the source available, recorded so they are decided
deliberately rather than by whoever writes the code first:

**How the video standard is determined.** The old application never established it. Three
sources exist, in descending order of trustworthiness: the model's disc-status reply where
that model documents the field (per-model, and captured in `DiscStatusDecode`); the model
itself where it is region-locked to one standard (an LD-V4300D is NTSC; a CLD-V2800 is
not); and the user, asked once and remembered. This plan assumes that priority order and
labels the answer with its source in the profile. A fourth possibility — measuring the line
rate from the RF, which this application uniquely could do since it already computes a live
spectrum — is genuinely attractive and genuinely a separate piece of work; it is noted here
and not planned.

**Whether the disc-status decode is worth doing per model.** Disc size, side and CX come
from fields whose layout varies by model and whose documentation is uneven. The schema
supports it and the profile reports `unknown` where the decode is absent, so a definition
can start with type-only decoding and gain the rest when somebody with the manual and the
player fills it in. Nothing downstream requires those fields.

Neither blocks Phase 1, which is why they are recorded rather than resolved.

## Feature ledger — the Future rows this plan discharges

From [ddd-gui-implementation-plan.md](ddd-gui-implementation-plan.md)'s *Future* table:

| Row | Disposition here |
| --- | --- |
| LaserDisc player serial control (Pioneer protocol, model detection, auto-reconnect) | Phases 1–2 |
| Player information display (model, status, position, physical mm) | Task 2.4 |
| Player remote dialog (full transport, manual serial commands, user-code reads) | Phase 3 |
| Automatic capture state machine (whole disc / partial / lead-in, CAV+CLV, key-lock) | Phase 5, re-shaped around the examine flow |
| "Stop player when capture stops" / "Stop capture when player stops" | Task 5.3 |
| Metadata sidecar — player-derived fields | Partially: disc facts reach the existing capture provenance (Task 5.3). The sidecar itself stays Future with advanced naming |

Remaining Future after this plan: advanced naming and its metadata sidecar; reset
notes/mint marks on side change; migration of the old application's INI settings; and
packaging work already tracked elsewhere.

## Out of scope

Non-Pioneer players (the schema is built to accept them; no definition is written for one
here), LD-ROM or CD-V specific handling, disc-side automation across a disc flip, any
attempt to decode video from the RF in order to identify the disc, and the RF-based video
standard detection noted above. Advanced naming and the metadata sidecar are a separate
plan. This plan is complete when Phase 6's checklist has been walked on real hardware with
at least two distinct player models and the ledger above contains no unaccounted row.
