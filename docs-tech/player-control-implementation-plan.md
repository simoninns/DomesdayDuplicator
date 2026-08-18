# LaserDisc Player Control (`ddd-gui/`) — Implementation Plan

!!! note "The UI in this plan was later restructured"

    Everything below was built. The *engine* — `PlayerController`, `PlayerSession`,
    `DiscExaminer`, `AutoCaptureSequence`, `AutoCaptureController`, the player definitions
    and the text helpers — is current, and is the reason to read this document: it records
    why each of those is shaped the way it is.

    The **user interface it describes is not**. The
    [capture UX plan](capture-ux-refactor-plan.md) recomposed it without changing the engine:

    | This plan says | Now |
    | --- | --- |
    | A **Player dock panel** (Task 2.4) | Gone. Its readouts are the player window's **Connection** tab; whether a player is connected is the status bar's permanent label |
    | A top-level **Player menu** | A section at the top of **Tools** |
    | **Remote control…**, one tall column of group boxes (Phase 3) | Titled "Player", tabbed: Control, Connection, Disc codes, Manual command. Opens whether or not a player is connected |
    | **GuidedCaptureDialog**, reached from the examine report (Phase 5) | Retired. `AutoCaptureWizard` — four pages from examination to summary — reached from Tools, the Capture panel, or an examine report |
    | The naming fields in a dialog of their own | `CaptureNamingForm`, shared by that dialog and the wizard's first page, and able to ask the player |

    Read a UI passage below as the reasoning that produced the current surface rather than a
    description of it. `DiscExaminer` also gained an `ExamineScope` after this plan, so a
    quick identify can run without moving the disc.

## Purpose

The capture application can drive the Duplicator but not the player. Everything about
getting a disc spinning, finding the start of a side and stopping at the right place is
still done by hand, or by keeping the old application (`gui/`) open beside the new
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
| `gui/src/DomesdayDuplicator/playercommunication.cpp` (removed — see below) | The Pioneer command sequences, response formats and timeouts that are known to work on real hardware |
| `gui/src/DomesdayDuplicator/playercontrol.cpp` (removed — see below) | The automatic-capture state machine, including the spin-down/spin-up ordering that makes lead-in capture work |
| [ddd-gui/src/capture/device_monitor.h](../ddd-gui/src/capture/device_monitor.h) | The polling-rather-than-callback discovery pattern this plan copies for serial ports |
| [ddd-gui/src/gui/capture_controller.h](../ddd-gui/src/gui/capture_controller.h) | The Qt-bridge shape a `PlayerController` must match |
| [docs/content/general/laserdisc-player.md](../docs/content/general/laserdisc-player.md), [docs/content/ldv4300d/](../docs/content/ldv4300d/) | What the project already tells users about players |

!!! note

    The `gui/` rows above name the capture application this project used to carry. It was
    removed from the repository on 2026-08-18, so those paths no longer resolve in a
    checkout; read them from git history when the original wire behaviour needs checking.

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
  `SerialPort` module) implements `ISerialPort`
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
- **The Pioneer user code is informational only, and the automatic capture never depends on
  it.** Everything the capture needs in order to decide what to do — disc type, addressing,
  where the disc ends — comes from the techniques the old application used and proved across
  years of captures: the disc-status query for the type, and a seek to a deliberately
  impossible address followed by an address read for the end. The user code may be shown
  beside those, and may be recorded, but it never supplies a figure the capture acts on.
  Three discs on this project's bench give three different answers — two with a readable
  record and an unreadable Key Data, one with no user code at all — so a flow that leaned on
  it would work on some discs and not others, and would fail in a way the user could do
  nothing about. Reading it also costs eleven seconds and throws away the player's position,
  which is a poor thing to have on the critical path of a capture. See Phase 3's findings
  and Task 4.1.
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
                                    — REMOVED; see the note at the top
  player_remote_dialog.{h,cpp}      the pop-up remote — now the tabbed player window
  examine_dialog.{h,cpp}            "Examine disc", its progress and its report
  guided_capture_dialog.{h,cpp}     the setup built from the examination
                                    — REPLACED by auto_capture_wizard.{h,cpp}
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
[ddd-gui/package.nix](../ddd-gui/package.nix)'s `buildInputs` (as `gui/package.nix`
already did) and confirm `nix build .#ddd-gui`.

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

**Status: built, and exercised against a real LD-V4300D.** The Qt-side port, the scanner,
the discovery planner, the worker thread, the controller, the settings and their tab of the
settings dialog, the Player dock and the Player menu all exist, with 52 further T1 tests —
including the first tests the settings dialog has ever had. On the project's
own bench the application enumerates five USB serial adapters, finds the player on the
first one it tries, identifies it as `P151502` → **Pioneer LD-V4300D, firmware 02**, and
polls it — the whole search taking 163 ms and a two-query poll 44 ms, which settles the
poll interval at its 250 ms floor exactly as this plan predicted.

Four deviations from the sketch above, and two things the bench taught that no fake could
have:

- **"Newest-looking first" is not knowable, so it is not claimed.** `QSerialPortInfo` does
  not report when a port appeared. The ordering is USB adapters before built-in ports —
  which is the real signal, since a player is on an adapter on every machine this will run
  on and a built-in port is far more likely to be something a user would rather was not
  interrupted — and the system's own order within each group.
- **Busy ports are found out about by failing to open.** Qt 6 removed
  `QSerialPortInfo::isBusy()`. The field and the rule stay, because the rule is right and a
  test can supply it; enumeration simply cannot fill it in, and a busy port is reported
  with the same words a permission problem gets, which is true of both.
- **One switch, not a switch and a pair of verbs.** The menu has **Player control**
  (checkable) and **Search now** rather than Connect/Disconnect: connecting and enabling
  are the same decision, and "enabled but deliberately disconnected" is a state nothing
  else in the design can represent. Switching player control off releases the port
  entirely, so "off" means the machine is exactly as it would be if the feature did not
  exist — which is the stronger promise.
- **No dead buttons for the next two phases.** **Remote…** and **Examine disc…** arrive
  with the remote and the examine sequence. Adding them now, disabled, would be precisely
  the failure this plan criticises the old application for.

What the bench found:

- **Zero padding is padding, not width.** The LD-V4300D answers the address query with
  seven zero-padded digits — `0002103` — whatever the disc is. The width check that refuses
  a time code read as a frame number was counting those, so it refused every reading from a
  real player. It now measures significant digits. This is a defect that only hardware
  could have surfaced: every plausible fixture had been written unpadded.
- **The disc-status reply is five digits, not a letter and a digit.** `11011`, with the
  type at index 1 as the old application always read it. The fixtures now use the real
  shape.

One thing left open, and it belongs to Phase 4: the Pioneer user-code query answered `E04`
on a parked player. A text reply is deliberately not put through the error convention —
a user code may contain an `E` — so a reply that is *exactly* `E` followed by digits wants
treating as a refusal by the examine sequence rather than as a user code.

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

**The player's settings are a tab of the one settings dialog, not a dialog of their own.**
They are a different kind of setting from the capture ones — what is on the end of a cable
rather than how this machine moves data off the Duplicator — and a single flat form would
put "which serial port" directly beneath "USB transfer size" under one **OK**. Two separate
dialogs would be worse still: two places for the same settings to disagree. So `File ▸
Settings…` opens on **Capture** and `Player ▸ Player settings…` opens the same dialog on
**Player**, and each half is applied to its own controller.

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

**Status: built, and exercised against the real LD-V4300D.** The remote exists, reachable
from **Player ▸ Remote control…** and from a **Remote…** button on the Player dock, both of
which are offered only once there is a player to drive. Underneath it, the worker gained a
command path — requests in, replies out, executed between polls on the thread that already
owns the session — and the protocol gained `PlayerControls`, which flattens a definition and
the firmware a player reported into one answer per control. 39 further tests, of which 15
are the remote's own.

On the bench the read-only half of the command set was walked end to end: `?P` → `P01`,
`?D` → `11011`, `?F` → `0001429`, `?X` → `P151502` through the manual field, and both
refusals — a frame address wider than the format allows, and a physical-position query on a
model that has no such command — stopped before anything was written to the port.

Four things worth recording:

- **`PlayerControls` is a value, not a pointer to a definition.** The remote gates its
  buttons on the interface thread while the definition belongs to the session on the
  worker's. A `PlayerDefinition*` would be perfectly valid to send across — they are
  compile-time constants — and would still be the wrong thing to put in a signal, because it
  invites the interface to start reasoning about the protocol. Resolving it once at
  connection also puts the firmware gate in one place instead of at every caller.
- **Both halves of the schema decide what is offered.** The command table says whether there
  is anything to send; the capability flags say whether the model claims the control at all.
  A definition that inherits the shared Pioneer set and then declares, say, no digital audio
  has a sequence to send and no business offering it — and that is precisely the case that
  would otherwise reach a user as a button that appears to work.
- **Key lock and the display are pairs of buttons, not tickboxes.** Nothing asks the player
  whether its front panel is locked, so a tick that cannot be read back would be a claim
  this application is in no position to make. They are commands; a checkbox would look like
  a readout.
- **The tooltip names the models that do have the control**, by sweeping the registry rather
  than by a table written here — so a player family added later is named without this
  wording being touched. Where nothing in the build offers it, it says that instead of
  sending the user hunting for a model that does not exist.

And one thing the bench settled, which was left open at the end of Phase 2: **both**
user-code queries answer `E04`, not just the Pioneer one. A text reply is still not put
through the error convention — a user code may legitimately contain an 'E' — but a reply
that is *exactly* `E` followed by digits is now recognised as a refusal, in `IsErrorCode`.
That closes the Phase 2 open item, and Phase 4's examine sequence inherits it rather than
having to rediscover it.

**A correction to what was written here first.** That `E04` was recorded as "the player
refuses while parked". The
[LD-V4400 Level I & III manual](https://www.manualslib.com/manual/837576/Pioneer-Ld-V4400-Level-I.html)
(pp. 93–94) documents it as *no data encoded in the Pioneer User's Code*. Both readings fit
the bench — the same disc gives `E04` parked and a full 200-byte record spinning — but the
cause claimed here had not been established, and the manual's is the one to quote.

### What the Pioneer user code actually is

Documented in the LD-V4400 Level I & III manual (TP 116 v1.1, §36), and worth restating
here because it is the shape Phase 4 will have to read. It is **200 characters**, encoded in
the last 100 frames (200 fields) of the lead-in per IEC specifications, one character per
field, in three regions that are always in this order:

| Region | Frames | Characters | Offsets |
| --- | --- | --- | --- |
| Disc Control Data | 60 | 120 | 0–119 |
| Key Data | 30 | 60 | 120–179 |
| Control Data | 10 | 20 | 180–199 |

The Key Data holds up to 60 characters of disc-identifying information specified by the
customer, encoded during mastering.

The regions are in `user_code.h`, with `static_assert`s that they total 200 and run end to
end — the order is part of the format, so a table that had drifted would read at the wrong
offsets rather than failing visibly.

Off this project's bench, an MCA *Casper* disc maps onto that exactly:

```
Disc Control Data   0–119    the same 60-character record twice
Key Data          120–179    ` × 60  — none of it could be read
Control Data      180–199    0 × 20
```

**An earlier draft of this section guessed at that split and got it wrong**, offering
"three copies with the third unreadable" or "two copies and 80 bytes of unencoded area".
It is neither: the sixty unreadable characters are precisely the Key Data region, which is
a much more specific finding — the customer's own identifying data is the part this disc
would not give up.

Within a 60-character record, each field carries a single-character tag and is padded to a
fixed width, with `@` filling the record out. Pioneer's worked example and the Casper disc
agree on the layout:

| Offset | Tag | Casper | Pioneer's example |
| --- | --- | --- | --- |
| 0 | `#` | `59-014` | `PTJ-01` |
| 11 | `*` | `MCA / CASPER THX LTBX` | `LASER JUKEBOX INFORMATION VOL1` |
| 42 | `!` | `1` / `2` | `1` |
| 45 | `%` | `0493804` / `0510803` | `0192429` |
| 53 | — | `@@@@@@@` | `@@@@@@@` |

**Inference, not documentation:** `!` is the side number and `%` the side's playing time as
a 7-digit time code, in the same format the address query returns. The evidence is good —
the Casper disc reads `!1` on one side and `!2` on the other; all four `%` values seen
(Pioneer's example, both Casper sides, Hudsucker side 1) parse as valid time codes with
minutes and seconds under 60 and frames under 30; Casper's two sides sum to 100:46 against a
runtime of about 100 minutes; and a seek past the end of side 1 parked the player at address
`0493804`, which is exactly that side's `%` field.

**And none of it is used for anything.** Nothing in the automatic capture reads a figure out
of the user code — the disc's length is measured, by the technique the old application
proved. See the hard constraint above and Task 4.1 for why: it is not reliably present, the
field meanings are inferred rather than documented, and reading it costs eleven seconds and
the player's position. It is shown, and it may be recorded; it is not acted on.

Two characters have meanings of their own, and Pioneer's example is what makes the
distinction visible: **`` ` `` (0x60) is a character the player could not read**, and
**NUL is a character that was never encoded**. The example's Key Data is sixty NULs — that
disc carries none; Casper's is sixty backticks — that disc carries some and the player could
not get at it. Reported the same way those two discs would look identical, and neither
reading would be true.

`$Y` is not a second view of the same thing. It answers `Y1000` — five characters, unrelated.

### And `?U` is not a query

The single most important line in the manual, because it changes what the command *is*:
"When the disc is spinning and the player receives the `?U` command it automatically
searches to lead-in." It moves the optical assembly, and **it does so whether or not there
is anything to read**. Measured on an LD-V4300D:

| From | Outcome | Cost | Player left at |
| --- | --- | --- | --- |
| Middle of a side, disc with a user code | 200 characters | 11.1 s | Frame 1, paused |
| Frame 20000, disc with none | `E04` | 1.8 s | Frame 1, paused |

The second row corrects an earlier reading recorded here. A CAV disc that answered `E04`
promptly, with the player still at frame 1, was taken as evidence that the player had
refused *without* seeking. It had not — it had simply been at frame 1 already. Seeking
first and refusing second is what it always does; the 1.8 seconds is the seek, and the
remaining nine of the eleven are the read.

So there is no cheap `?U`. Even the one that yields nothing costs the position.

That is a constraint on Phases 4 and 5 rather than a detail — anything that has positioned
the disc must not then read the user code, and the manual accordingly recommends reading it
immediately after spin-up and before any other control command. The remote's Pioneer button
says so in its tooltip and in the box while it waits.

### What five discs on the bench actually did

Recorded because it is the evidence behind the hard constraint above, and because no
plausible amount of reading would have predicted it:

| Disc | `?D` | Which now reads as | User code |
| --- | --- | --- | --- |
| *Casper*, NTSC CLV, side 1 and side 2 | `11011` | loaded, CLV, 12-inch, **side 2**, chapters | Record readable twice; **Key Data wholly unreadable** |
| *The Hudsucker Proxy*, NTSC CLV | — | — | Same shape |
| NTSC CAV | `10001` | loaded, CAV, 12-inch, side 1, chapters | **`E04`** — none encoded |
| PAL CLV | `11001` | loaded, CLV, 12-inch, side 1, chapters | 200 characters of which **180 unreadable** — everything but the Control Data |
| PAL CAV | `10001` | loaded, CAV, 12-inch, side 1, chapters | **`E04`** — none encoded |

Two of five yielded a usable user code, and no two failed alike. A flow that took its
figures from there would work on half a shelf.

The third column is retrospective: when these readings were taken this plan decoded one
digit of the reply and had the other four down as an open question. They are all documented,
in two manuals — see *Open decisions*, where that is now settled. The one reading singled
out here as unexplained turns out to say Casper was on its second side, which the `!2` in
that side's own user code had already said.

One negative finding survives it, because it is the sort of thing that gets assumed: **the
disc-status reply does not carry the video standard.** No field for it in either manual, and
the PAL CAV disc and the NTSC CAV disc answer `?D` identically (`10001`). That is a fact
about `?D` and not about the players — `?S` answers it in twenty milliseconds. See *Open
decisions*.

The standard user code differs between them too — `Y1000` on both NTSC discs, `Y0000` on the
PAL CLV — and nothing here explains why. One sample per standard is not a finding.

Four defects came out of looking at all this, all now fixed:

- **`?U` had the wrong timeout class, and not marginally.** Eleven seconds against a
  five-second normal class. The only reason it ever appeared to work was a player already
  sitting on the lead-in, left there by the previous `?U` — the 4.6-second figure recorded
  in an earlier draft here was measured in exactly that state and is not the real cost. It
  takes the long class now.
- **`ParseText` was trimming whitespace.** Right for a reply about to be read as a number,
  wrong for a fixed-width record whose fields are space-padded — it was deleting payload.
  It now takes off the terminator and nothing else, which is safe because every parser that
  reads a reply strips for itself.
- **The payload was being decoded as UTF-8.** A reply is arbitrary bytes — Pioneer's own
  example contains NULs — and `fromUtf8` would have replaced anything above 0x7F that did
  not form a valid sequence, so a hex dump would have been a dump of what the decode had
  already destroyed. It is Latin-1 now, which maps all 256 values one-to-one and back.
- **The reply was shown as one undifferentiated wall.** It is now split at the documented
  boundaries, each region dumped in hex with an ASCII gutter at its own offsets within the
  whole, and each labelled with what it could not read as against what was never there.

Two things deferred rather than done, and both deliberately. The T5 walk of *every* control
has not happened: the bench run above was read-only, because the rest of the remote moves
the disc and the tray, and the document it is walked against is Task 6.1's. What is
concrete now is the per-model list in
[players/README.md](../ddd-gui/src/player/players/README.md), which says what to watch for
and how to tell a wrong capability flag from a wrong command sequence. And **Examine disc…**
still arrives with Phase 4 rather than appearing here disabled.

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

**Status: built.** `DiscProfile` and `DiscExaminer` are in `ddd_player` and link no Qt;
`ExamineDialog` is the window, reachable from **Player ▸ Examine disc…** and from an
**Examine…** button on the Player dock, both offered only once there is a player. The worker
gained an `Examine()` slot that runs the whole sequence on the thread that already owns the
session, pausing the status poll for its duration. 79 further tests, of which 34 are the
step machine's and 12 the dialog's.

Six things worth recording, four of which are departures from what was planned above:

- **The sequence hands out steps; it does not send them.** `DiscExaminer::Next()` returns
  the next command and `Apply()` takes what the player said. Every branch of it — an open
  tray, a disc that will not spin, a refused disc-status query, a link that dies halfway
  through, a cancel between any two steps — is therefore a test that runs in microseconds
  with nothing plugged in. That was the point of the shape, and it paid: the cascade where
  an unknown disc type means no seek can be sent is pinned by a test rather than discovered
  by a user with an unusual player.
- **It runs on the worker thread, not the interface's.** Handing steps back across the
  thread boundary one at a time would have been a round trip per step for no gain — every
  step is a blocking exchange the session already knows how to make. What crosses the
  boundary is progress and a profile.
- **Cancelling waits out the step in flight**, rather than aborting the read. `ISerialPort`
  has `RequestAbort` and it is deliberately not used here: an abandoned read is reported as
  a port failure, so cancelling an examination would drop the link and cost a rediscovery.
  Waiting out one command — five seconds usually, thirty at the very worst — is much the
  smaller price. The window says "stopping" as soon as the button is pressed, because a
  request that is made and not yet granted otherwise looks like a button that was ignored.
- **The profile gained a first address, and the sequence gained the step that measures it.**
  The plan listed only the length. Seeking back to the start is needed anyway — to answer
  whether the lead-in is reachable, and to leave the player somewhere useful — so the start
  address falls out of a step that was already there. Phase 5's "whole side" is
  start-to-end, and now both ends are measured.
- **The video standard is asked for, not declared.** `?S`, the TV system request — twenty
  milliseconds, moves nothing, and answers the question this plan had spent two rounds of
  bench work concluding could not be answered. The field taken is the disc's, not the
  output's, because a converting player makes those different answers. A CAV disc
  consequently gets a playing time and a capture-size estimate for the first time.
- **The disc-status reply is decoded in full, and the chapter probe went away with it.**
  This plan had the reply down as one usable digit and an open question; it is five
  documented fields — loaded, CAV/CLV, size, side, chapters — in both manuals this project
  can reach. See *Open decisions*, where that question is now closed. The examination reads
  the disc's own programme status instead of driving the transport to rediscover part of it.
- **Both user codes are always read.** No option, no checkbox. The Pioneer read is placed
  where the seek it costs was going to happen anyway, which is what makes it affordable
  unconditionally; and a disc's own identifying records are not something a user should have
  to know to ask for.
- **It does not put the player back where it found it.** It leaves the disc held still at
  the start of the side. That is a deliberate departure: the examination is a precursor to a
  capture, and the start of the side is where a capture wants to begin. Restoring the
  original position would mean a third seek to undo the second.
- **The seek past the end and the seek back to the start are not treated alike.** A refusal
  from the first is the technique working — the player runs to the end of the side and then
  says no. A refusal from the second means the player did not move, so the address read that
  would follow it is skipped rather than recorded as the start of the programme.

The raw `?D` reply is kept and reported all the same, as the working beside the answer
rather than as evidence for a decode that has since been written. Every field of the profile
except one is now established by an examination; the exception is the video standard on a
model with no `?S`, which the guided setup will ask for.

Deferred, as Phase 3's remote was: the T5 walk against real CAV and CLV discs belongs to
Task 6.1's document. **Set up capture…** is not on the window either — it arrives with
Phase 5 rather than appearing here disabled, which is the same rule the remote's capability
gating exists to enforce.

### Task 4.1 — `DiscProfile` and the examine sequence

`DiscProfile` is a value type holding what an examination found, every field carrying its
provenance — *measured*, *reported by the player*, *inferred*, or *unknown*:

| Field | How it is obtained |
| --- | --- |
| Disc present, tray state | Active-mode query, then confirmed by C1 of the disc status |
| Disc type (CAV / CLV) | Disc status query, C2 |
| Addressing (frame / time code) | Follows from disc type |
| Disc size, disc side | Disc status query, C3 and C4 — the disc's own programme status, so it costs nothing and moves nothing |
| Length: lead-out frame or time code | Seek to an impossible address, then read the current address — the technique the old application already used to find the disc end. `FR60000SE` for CAV and `FR1595900SE` for CLV, exactly as `playercommunication.cpp`'s `getMaximumFrameNumber()` and `getMaximumTimeCode()` send them |
| Lead-in reachable | Whether the lead-in flag is seen when seeking to the start |
| Chapters present | Disc status query, C5. A chapter search only where the model does not report the field |
| Standard and Pioneer user codes | Their queries, always both — **informational only**, see below |
| Video standard | TV system request (`?S`), C2 — the disc's own standard rather than the one being output. Declared by the user only on a model that cannot be asked |
| Estimated capture size and duration | Length × the current capture settings' `EstimatedBytesPerSecond()` |

`DiscExaminer` is a step machine over that list: each step names the command to send, what a
good reply looks like, and what an unusable reply means for the profile. **Failure is
partial, not total.** A player that cannot report the disc size still yields a profile with
the type and the length in it, and the guided setup asks only about what is missing. The
old application's equivalent gave up on the first refusal.

The sequence is cancellable between every step, and it puts the player back roughly where
it found it: examination spins the disc up and seeks, which is unavoidable, but it does not
leave a disc playing.

**Both user codes are always read.** They are the disc's own identifying records, and the
examination is the one moment in a session when reading the Pioneer one costs nothing extra:
the player is about to be sent to the lead-in anyway, and everything that depends on
position happens afterwards. An examination that left them for the user to fetch by hand
would be one followed by two more trips to the remote, the second of which moves the disc.

**But the user code is informational and nothing acts on it.** This is a hard constraint,
and Phase 3's bench work is why. It is tempting to use: the Disc Control Data carries what
looks very much like the side number and the side's playing time, so a disc's length appears
to be readable straight out of the lead-in. It is not to be used that way.

- **It is not reliably there.** Three discs on this bench, three different outcomes: two
  with a readable record whose Key Data would not read at all, and one — a CAV disc,
  spinning and perfectly healthy — with no user code whatsoever. A length that arrives for
  some discs and not others is worse than one that always costs a seek, because the flow
  around it has to handle both anyway.
- **The field meanings are inferred, not documented.** The `!` and `%` reading is supported
  by four samples and an accidental corroboration, and it is still a guess — even now that
  the disc status has independently confirmed the side number the `!` field carried. The
  seek-to-the-end technique measures the thing itself; the user code describes it.
- **Reading it is expensive and destructive of position.** Eleven seconds, and the player
  ends up at the lead-in — which is exactly the wrong place to be if the next thing to
  happen is a capture from a chosen start point.

So the profile's *length* is always measured, by the old application's technique, and any
user code that was read sits beside it as something to show and to record. Where the two
disagree, the measurement wins and neither is quietly dropped.

It is read **first** in the sequence, and that placement is what makes reading it
unconditionally affordable — Pioneer's own manual recommends issuing `?U` immediately after
spin-up and before any other control command, and at that point the position it destroys has
not been established yet.

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

**Status: built.** `AutoCapturePlan`, `ValidateAutoCapturePlan()` and `AutoCaptureSequence`
are in `ddd_player` and link no Qt; `GuidedCaptureDialog` is the window, reached from **Set
up capture…** on the examine report; `AutoCaptureController` is the one object holding both
a `PlayerController` and a `CaptureController`. 98 further tests, of which 48 are the plan's
and the sequence's, 19 the dialog's and 10 the coupling's — and one of those ten is a whole
automatic capture, end to end, against a fake serial port and the fake USB backend,
producing a FLAC file with the disc's own facts in its tags. No player, no Duplicator.

Six things worth recording, three of which are departures from what was planned above:

- **The one branch where a started capture is not stopped is the link failing *the run*,
  and it is stated rather than accidental.** This plan says both things: Task 5.2's acceptance list
  has "link lost mid-capture" ending with the capture stopped, and Task 5.3's prose says
  "the link dropping stops the automation, reports it, and leaves the capture running under
  manual control". They cannot both hold. The second is what is built, because it is the
  one with an argument attached and because the first is unachievable as written anyway —
  with the link gone the player cannot be stopped either. A player that loses its cable
  goes on playing to the end of the side, so stopping the capture would truncate a good one
  to avoid a file that the user is now watching. The sequence says so through
  `capture_left_running()`, the controller logs it, and the window says **"the capture is
  still running"** — so the property test is "every branch but this one stops what it
  started, and this one announces it", which is checkable rather than aspirational.
- **The sequence is driven from the interface thread, not the worker's.** The opposite of
  the examine sequence, and for a reason: an examination is a run of blocking exchanges the
  session already knows how to make, while this one interleaves player commands with
  attaching and detaching a writer — which is the interface thread's to do. A step per
  queued round trip costs nothing when the watch polls twice a second. Cancellation is then
  answered between one step and the next rather than at the end of a thirty-second seek.
- **The transport is recorded as started when the command goes out, not when it is
  answered.** `PL64RBMF` is three commands in one, and a player that took the first and
  rejected the third is playing a disc the sequence would otherwise believe it never
  started — and would then leave running.
- **The stall detector asks rather than assumes.** An address that has not advanced for
  five readings produces one active-mode query, and that query separates the three things
  which look identical from the address alone: a player still getting up to speed (carry
  on), a player that has stopped (a finding — the file is finalised and the capture is
  good as far as it goes), and a player insisting it is playing a disc that is not moving
  (a stall — ended, because the alternative is a file that grows until the volume fills).
- **A refused disc-status check does not refuse the capture.** Partial failure is this
  library's rule and it applies here too: a player that will not answer `?D` is not evidence
  the disc changed, and refusing on it would refuse discs on players that seek and play
  perfectly well. What *is* fatal is a positive disagreement — a CLV reply where a CAV disc
  was examined, the other side, or a tray the player says is empty.
- **The video standard is the only thing the guided setup ever has to ask for, and on a
  model with `?S` it asks for nothing at all.** That is what Phase 4 bought: the old
  Automatic Capture dialog asked for the disc type before it had looked at the disc, and
  this one is built from a profile. A CAV disc gets frame controls and no time-code ones; a
  CLV disc the reverse — absent rather than greyed out, because a disabled field for a thing
  this disc does not have invites somebody to look for the setting that would enable it.

- **A name that is already taken is said so as it is typed, in the guided setup and in the
  Capture panel alike.** No capture has ever overwritten another — the engine resolves the
  path before it opens anything, and has since long before this phase — but the rename it
  does instead was silent, and a typed name carries no timestamp, so the second capture of
  "Casper side 1" quietly becoming "Casper side 1_2" is the *ordinary* case rather than an
  edge one. Two files nobody can tell apart afterwards is a slower way to lose a capture
  than overwriting one, but not a much slower way. `ResolveCaptureDestination()` is now the
  single call that answers "where will this really be written, and is that the name that was
  asked for" — the panel and the guided setup show it live, the controller opens it, and
  `CaptureRenamed` reports it if it ever happens anyway.
- **The window says how much longer the capture has to run, and it needs no clock to do
  it.** The disc plays in real time, so the programme left to play *is* the time left to
  wait: the figure is a pure function of where the player is and where the plan ends, right
  from the first reading rather than settling down over the first minute the way an
  observed-rate estimate would. It is shown only while the disc is actually being watched —
  a countdown beside "spinning the disc down" would be counting towards something that has
  already happened — and not at all on a CAV disc whose video standard nobody established,
  which is the one case a frame count is not a duration.

**A correction to what was written here first, and to the shapes this plan set out.** The
three shapes were first built as "the whole side", "a range" and "from the lead-in for a
given number of frames", with the last two words doing damage: they read as though the
lead-in were somewhere a player could be sent, and the plan's own text had a
`lead_in_reachable` check refusing both lead-in shapes on a disc whose start the
examination could not seek to. **There is no command that puts a player on the lead-in.** A
player takes an address or it starts from a stop; the lead-in is not an address. So it is
never asked for and never refused — it is what a capture gets by being running while the
disc spins up, which works on any player that can be stopped and started, and which the
examination's `lead_in_reachable` (a fact about a *seek* to the first frame) says nothing
about either way. The check is gone and all three shapes are always on offer.

The same mistake was made at the other end and cost more. The run-out is not an address
either, and the tail as first built stopped the capture and then the player — so a
whole-side capture ended a few seconds short of exactly the part of the disc nothing else
can reach, with no way for a later capture to go back for it. A whole-side capture now
stops the player **first** and keeps writing through the spin-down; every other shape stops
the writer first, because there is nothing in the middle of a side worth the extra seconds.
The order of those two steps is the shape's decision, and `EndsWithSpinDown()` is where it
is made.

Reordering the tail found a real defect in the branch beneath it, which is the useful half
of the lesson. The stop-capture step had been guarded on `link_failed_`, so that a link
lost mid-side left the capture running; with the player stopped first, a link that died
during a *whole-side spin-down* hit that same guard and left the writer attached for ever.
The guard is now on `pending_outcome_ == kLinkFailed` — the capture is handed back to the
user only when the link is what ended the run, not merely when it has since died. Detaching
a writer needs no serial link, and the side had already been captured.

Deferred, as Phases 3 and 4 were: the T5 walk on real discs belongs to Task 6.1's document.
Nothing on the bench has yet run an automatic capture end to end.

### Task 5.1 — The guided setup

Built **from** the profile, showing only what applies:

- Three shapes, and **the list is decided by there being no command that puts a player on
  the lead-in.** A player can be sent to an address and it can be started from a stop; the
  lead-in is not an address, so the only way it reaches a file is for the capture to be
  running while the disc spins up. It is therefore never asked for — it is what two of the
  three shapes get by construction. The same holds at the other end: the run-out is not an
  address either, so it reaches a file only if the capture is still running when the player
  is spun down.
  1. **The whole side.** Start the capture, spin the disc up, run to the end, and spin it
     down again *before* the capture is stopped. The only shape whose file holds the whole
     of the side.
  2. **From one address to another.** Seek, start the capture, play to the second address.
     The disc turns throughout, so neither the spin-up nor the spin-down is in the file.
  3. **From spin-up to an address.** The front of a whole-side capture.
- **CAV** offers frame addressing and **CLV** time-code addressing, on the same three
  shapes, bounded by the measured length either way — so a range that cannot exist cannot
  be typed.
- Anything the examination could not determine is asked for here, once, with what the
  application believes prefilled and marked as a guess. *In the event there is exactly one
  such field — the video standard on a model with no `?S` — and nothing to prefill it with:
  the disc status reads identically for a PAL and an NTSC disc and the model does not imply
  it either, which Phase 4's open decision settled at length. So it opens on "not known"
  and says why, and what the user chooses is recorded as `kDeclared`. A prefilled guess
  there would be a guess with no evidence behind it, which is worse than a question.*
- The estimated file size and duration are shown against the destination volume's free
  space, using the capture settings' existing estimator. A setup that will not fit is
  flagged before the disc starts spinning rather than by the low-space warning forty
  minutes in.
- The capture name is offered prefilled from the disc facts where they are known, without
  imposing a scheme — full advanced naming remains **Future**. *The prefill is resolved
  against the destination folder before it is offered, and this is not optional: it is built
  from what the disc **is** — "CLV_PAL_Side2" — so it is the same every time that side is
  captured, where the generated `RF-Sample_<timestamp>` is free by construction. A
  suggestion that was already taken would put a name in the field that is not the name of
  the file.*
- Key lock, and the two coupling preferences, are here as checkboxes with the current
  setting.

`AutoCapturePlan` is the value this dialog produces and `ValidateAutoCapturePlan()` is a
pure function over it, so the interface's enablement rules and the sequence's preconditions
are the same rules rather than two copies of them.

**Acceptance criteria**
- T1: validation covers every invalid plan — end before start, range beyond the measured
  length, a zero-length capture, a plan whose addressing does not match the profile. It does
  *not* refuse a shape for a lead-in the examination could not seek to: no command puts a
  player there, and the shapes that hold it need only a player that can be stopped.
- Widget test: a CAV profile offers no time-code controls and a CLV profile offers no frame
  controls; bounds come from the profile; the size estimate tracks the range and the
  current output format.

### Task 5.2 — The automatic-capture sequence

`AutoCaptureSequence` is the old application's state machine, re-expressed as a step
machine and with its ordering preserved exactly, because that ordering is what makes the
result usable:

1. Key lock on, if asked, so the front panel cannot interfere.
2. Confirm the disc still matches the plan (it may have been swapped since the examination).
   From the disc type and the measured length, not from the user code — a disc that has none
   would fail a check built on it, and reading one here would cost eleven seconds and send
   the player to the lead-in immediately before a capture that needs it somewhere else.
3. Spin down, for either shape that is to hold the spin-up. This is the non-obvious step:
   there is no command that puts a player on the lead-in, so the only way it reaches a file
   is for the capture to be running while the disc comes up from a stop — which means
   stopping a disc the user has very likely just started.
4. Start the capture, then spin up — in that order, with play-with-stop-codes-disabled for
   CAV so a stop code cannot pause the disc mid-side.
5. Or, for a partial capture, seek to the start address, then start the capture, then play.
6. Watch the current address until the end address is reached, with a stall detector: an
   address that has not advanced for several polls while the player claims to be playing
   ends the capture with a specific message rather than running until the disk fills.
7. Stop the capture, stop the player, release the key lock — **except on a whole-side
   capture, where the player is stopped first and the writer stays attached through the
   spin-down.** The run-out is not an address, so nothing can seek to it and no later
   capture can go back for it; recording it while the disc is being stopped is the only way
   it is ever in a file. The order of those two steps is therefore the shape's decision.

Cancellation is honoured between every step and, unlike the old application, during the
watch phase — the capture is finalised properly rather than abandoned.

**Acceptance criteria**
- T1: each of the three capture shapes (whole side, range, from spin-up) produces the
  expected step order for both CAV and CLV, asserted step by step against fakes — including
  which of the writer and the player is stopped first.
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
  to contain the spin-up and its last to contain the spin-down.

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

Recorded so they are decided deliberately rather than by whoever writes the code first. One
of the two has since been settled — by reading the manual that had the answer in it all
along, which is the more useful half of the lesson.

**~~How the video standard is determined.~~ Settled — there is a command for it.** The old
application never established it, and this plan proposed three sources in descending order
of trustworthiness: the disc-status reply, the model itself, and the user. Bench work
knocked out the first two, and the third — asking the user — became the plan of record. It
was the wrong answer, arrived at honestly: **the player will simply say.**

`?S`, the *TV System Request*, is documented in the LD-V4400 Level I & III manual (§38,
p. 95) as "returns information describing the TV System and connection to an external sync
generator". Three characters, printed as C3 C2 C1 and arriving in that order: the standard
being **output**, the standard **of the disc**, and the standard of the external sync (0
where none is connected). Each takes the same values — 0 unknown, 1 NTSC, 2 PAL.

That manual is for an NTSC-only player, so its table has no PAL row and its worked examples
are `110` and `111`. The PAL value is this project's own bench reading, taken with a PAL CAV
disc playing on the LD-V4300D:

```
/dev/ttyUSB4 at 9600 baud — ?X -> 'P151502'
  ?D   -> '10001'    (20 ms)   loaded, CAV, 12-inch, side 1, chapters
  ?S   -> '220'      (20 ms)   PAL out, PAL disc, no external sync
```

**Twenty milliseconds, and it moves nothing** — so it sits beside the disc-status query in
the examine sequence, and the standard is *reported* rather than *declared*. C2 is the field
taken, not C3: on a player that converts they disagree, and a capture is of what is on the
disc rather than of what is on the cable.

Two consequences beyond the label:

- **A CAV disc now has a playing time and a size estimate.** A frame count is only a
  duration once the frame rate is known, and the frame rate needs the standard. Until this,
  every CAV examination reported "not known" for both.
- **The user is not asked.** The guided setup keeps `kDeclared` for a model that cannot
  answer `?S`, which is the honest fallback rather than the plan of record.

What the three original sources were wrong about is still worth keeping, because it is why
this one is trusted:

- **The disc-status reply does not carry it.** Measured, on this project's LD-V4300D: a PAL
  CAV disc and an NTSC CAV disc both answer `?D` with `10001`. There is nothing in that
  reply to tell them apart, so `DiscStatusDecode` cannot be the source however carefully it
  is filled in for a model.
- **The model does not imply it either.** This plan asserted that an LD-V4300D is an NTSC
  player. **It is not — the LD-V4300D is dual-format, NTSC and PAL**, and the bench has
  since driven it with a PAL CLV disc and a PAL CAV disc. So the one worked example this
  plan offered for "the model is region-locked, so the model tells you" was itself a
  counter-example. Some models genuinely are single-standard, but a rule of that shape needs
  per-model evidence before it can be relied on, and where it is wrong it is wrong silently
  — the worst way for a guess to fail.

Both of those remain true, and both are now beside the point: neither source has to carry
the standard when a command answers it directly. The fourth possibility — measuring the
line rate from the RF, which this application uniquely could do since it already computes a
live spectrum — goes back to being an attractive extra rather than the only route, and is
still a separate piece of work not planned here. It would be worth having as a cross-check
against a player that answers `?S` wrongly, which is a thing no amount of reading can rule
out.

**The lesson, twice over.** Both open decisions in this section were closed by reading the
manual rather than by measuring, designing round the gap, or asking the user. The
disc-status fields were documented; the TV system request was documented. In both cases this
plan had recorded a careful, well-evidenced argument for why the information could not be
had — and in both cases the argument was sound and the premise was false.

**~~Whether the disc-status decode is worth doing per model.~~ Settled — it was documented
all along.** This plan recorded the disc-status reply as carrying "disc size, side and CX on
at least some models, but the layout varies and this project does not have a manual for
every one of them", decoded one digit of it, and left the rest as an open question with four
bench readings attached. That was wrong, and it was wrong in the most avoidable way: the
reply is documented identically in the LD-V4400 manual (§34, p. 92) and the LD-V8000 manual
(p. 107), it is the same five fields on both, and nobody had looked.

| | Field | 0 | 1 | X |
| --- | --- | --- | --- | --- |
| C1 | Disc loading | not loaded | loaded | — |
| C2 | CAV/CLV | CAV | CLV | — |
| C3 | Disc size | 12 inch | 8 inch | unknown |
| C4 | Disc side | side 1 | side 2 | unknown |
| C5 | Chapter code | no chapters | chapters | unknown |

with `0XXXX` given as the reply from a player with nothing loaded and `10001` as the worked
example of "12-inch CAV disc loaded with chapter code".

That decodes every reading this project has taken, and the one that had been singled out as
unexplained explains itself: `11011` is Casper's **second side**. The Pioneer user code on
that same side carries `!2`, which this plan had already guessed was a side number from four
samples. Two independent readings off the same disc agreeing is what turns that guess into
a fact — and it is the only corroboration of the `!` field that did not come from the user
code itself.

Three consequences, all now built:

- **The chapter probe is gone.** C5 answers it, so the examination no longer sends a search
  command to find out something the disc had already said. One fewer step, and one less
  movement of the disc. The probe survives only as a fallback for a model whose decode has
  no chapter field.
- **The side is in the profile.** Two sides of one disc are two files, and until now the
  application had no way of knowing which it was making.
- **`X` is a third value, not a zero.** Both manuals document it, and reading it as a digit
  would turn "I could not tell which side this is" into "side 1".

The raw reply is still kept and still printed in the report — no longer because it is
undecoded, but as the working beside the answer. A report that says "side 2" and shows the
`11011` it read that from is one somebody can check.

**What is still not in it: the video standard.** Neither manual lists a field for it, which
settles the matter from the documentation as well as from the bench, where a PAL CAV disc
and an NTSC CAV disc both answer `10001`. See the decision above.

The one remaining open item is therefore the first: how the video standard is determined.
It does not block Phase 1, which is why it is recorded rather than resolved.

## Feature ledger — the Future rows this plan discharges

From [ddd-gui-implementation-plan.md](ddd-gui-implementation-plan.md)'s *Future* table:

| Row | Disposition here |
| --- | --- |
| LaserDisc player serial control (Pioneer protocol, model detection, auto-reconnect) | Phases 1–2 |
| Player information display (model, status, position, physical mm) | Task 2.4 |
| Player remote dialog (full transport, manual serial commands, user-code reads) | Phase 3 |
| Disc examination (type, addressing, measured length, chapters, user codes) | Phase 4 — no equivalent in the old application |
| Automatic capture state machine (whole disc / partial / lead-in, CAV+CLV, key-lock) | Phase 5, re-shaped around the examine flow |
| "Stop player when capture stops" / "Stop capture when player stops" | Task 5.3 — and the first of the two has since been **removed**. The automatic capture spins the disc down as a step of its own sequence, so that preference only ever acted on manual captures, where the disc belongs to whoever is operating it; it is also the unsafe direction, since a Reject arriving while the disc is already spinning down opens the tray. The coupling now runs one way only: the player may stop the capture, never the reverse |
| Metadata sidecar — player-derived fields | **Built.** Disc facts reach the capture provenance (Task 5.3), and the sidecar itself now exists as `<capture>.ddd.yaml` — carrying the player identity for every capture taken with a live link, and the whole examination with each fact's provenance for an automatic one |

Remaining Future after this plan: migration of the old application's INI settings, and
packaging work already tracked elsewhere. Advanced naming, its metadata sidecar and the
per-side notes preference have since been built — see the
[ddd-gui plan](ddd-gui-implementation-plan.md)'s ledger.

## Out of scope

Non-Pioneer players (the schema is built to accept them; no definition is written for one
here), LD-ROM or CD-V specific handling, disc-side automation across a disc flip, any
attempt to decode video from the RF in order to identify the disc, and RF-based video
standard detection — which, now that the player answers `?S`, would be a cross-check rather
than the primary source. Advanced naming and the metadata sidecar are a separate
plan. This plan is complete when Phase 6's checklist has been walked on real hardware with
at least two distinct player models and the ledger above contains no unaccounted row.
