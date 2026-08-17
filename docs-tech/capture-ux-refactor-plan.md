# Capture UX Refactor (`ddd-gui/`) — Plan

## Purpose

The application now has everything a capture needs — naming, player control, disc
examination, guided automatic capture — but the pieces were added one dialog at a time, and
it shows. There are two genuinely different ways of taking a capture, and the interface
does not present them as two ways:

- **A manual capture** is: set the naming if you want it, press Start, press Stop. That
  path is nearly right already. What it lacks is a nudge — nothing tells a user that
  naming has not been set until the file lands as `RF-Sample_<timestamp>` — and a shortcut:
  the naming dialog asks eight questions about the disc, several of which a connected
  player can simply be asked.
- **An automatic capture** is a workflow: find out what the disc is, name the capture from
  it, choose what to capture and how, run it, and see what happened. Today that workflow is
  spread across four windows reached from three places: the Player panel or Player menu →
  **Examine disc** ([examine_dialog.cpp](../ddd-gui/src/gui/examine_dialog.cpp)) → **Set up
  capture** ([guided_capture_dialog.cpp](../ddd-gui/src/gui/guided_capture_dialog.cpp)),
  with the destination, format and rate on the Capture panel, and the naming fields in a
  dialog none of those open. A user doing the normal thing — capture both sides of one
  disc — navigates all of it twice.

Alongside those two paths, two pieces of standing clutter:

- **The Player dock panel** ([player_panel.cpp](../ddd-gui/src/gui/player_panel.cpp))
  duplicates what the status bar already says, holds four buttons that are also in the
  Player menu, and earns permanent screen space for something most users configure once
  and never look at again.
- **The remote control dialog**
  ([player_remote_dialog.cpp](../ddd-gui/src/gui/player_remote_dialog.cpp)) stacks five
  group boxes — transport, go to, display and audio, user codes, manual command — into one
  tall column. The first two are used constantly; the last two are for adding support for
  new player models, and they cost every user a window half as tall again as it needs to
  be.

This plan restructures the interface around those observations. **It is a UI
recomposition, not an engine change**: `CaptureController`, `PlayerController`,
`AutoCaptureController`, `DiscExaminer`, `AutoCaptureSequence` and the validation,
estimate and text helpers all keep their interfaces, with one small, additive exception
(the examination scope in Phase 3).

## Authoritative references (in-tree)

| Reference | What it settles |
| --- | --- |
| [AGENTS.md](../AGENTS.md) §2, §5, §6, §8 | Component boundaries, C++ style, naming, testing obligations |
| [TESTING.md](../TESTING.md) | Tier labels and conventions for the tests each phase owes |
| [player-control-implementation-plan.md](player-control-implementation-plan.md) | The architecture the player UI sits on, and why examine-then-offer replaced the old ask-first dialog |
| [ddd-gui/src/gui/main_window.cpp](../ddd-gui/src/gui/main_window.cpp) | The dock framework, menu construction, and the single-instance dialog pattern (`QPointer` + `WA_DeleteOnClose`) |
| [ddd-gui/src/gui/auto_capture_controller.h](../ddd-gui/src/gui/auto_capture_controller.h) | The one object that couples player and capture; the wizard drives it exactly as the guided dialog does |
| [ddd-gui/src/player/disc_examiner.h](../ddd-gui/src/player/disc_examiner.h) | The examination step plan the quick look-up in Phase 3 subsets |
| [ddd-gui/src/gui/capture_naming_dialog.cpp](../ddd-gui/src/gui/capture_naming_dialog.cpp) | The naming fields, their apply-as-you-type model, and the per-side memory the shared form must carry over intact |

## The shape of the change

Five decisions, then the phases that deliver them.

### 1. The manual path stays exactly as it is, plus an affordance

Manual capture is set-and-press; a wizard would be in its way. Two additions only:

- **The Naming… button on the Capture panel advertises itself** when a capture is about to
  go out unnamed: name field empty, no naming field ticked, not in test mode. An accent
  colour on the button (through the same pinned-height stylesheet mechanism
  `ActiveButtonStyle` already uses — see the comment in
  [capture_panel.cpp](../ddd-gui/src/gui/capture_panel.cpp) for why the height must be
  pinned), plus a tooltip saying what will happen instead. It is a nudge, not a warning:
  the timestamped name is a legitimate choice, so the colour is the theme's accent, never
  the error red, and it never blocks the Start button.
- **The naming dialog can ask the player.** See decision 2.

### 2. The naming dialog gains "Ask the player", backed by a quick examination

Three of the naming dialog's fields — disc type, video standard, side — are facts a
connected player can report in seconds, without seeking and without measuring anything.
The full examination is the wrong tool for this: it seeks to both ends of the side, takes
the better part of a minute, and moves the disc.

So the examiner gains a **scope**: `DiscExaminer` takes an enum (`kFull`, the whole plan
as today; `kIdentify`, the plan cut after `kReadingTvSystem` plus the settling step). The
identify scope runs `?P`, spin-up if needed, `?D`, `?S`, and leaves the player still — no
seeks, no user codes, nothing measured. `PlayerController::Examine()` takes the scope as a
defaulted parameter, so every existing caller compiles unchanged.

In the naming dialog, an **Ask the player** button (enabled only while the connection is
live) runs an identify-scope examination and, from the resulting profile, fills and ticks
exactly the fields the profile knows: `disc_type` → disc type, `video_standard` → video
standard, `disc_side` → side. Fields the profile does not know are left untouched, as is
everything the user has typed (title, notes, mint marks). The button shows the stage line
while it runs, the same wording the examine window uses
([player_text.h](../ddd-gui/src/gui/player_text.h) `ExamineStageName`).

Wiring: `CaptureNamingDialog` is currently built inside `CapturePanel`, which has no
player controller. Rather than threading a `PlayerController*` through the panel, the
panel's Naming… press becomes a signal (`NamingRequested`), and `MainWindow` — which owns
both controllers — builds the dialog, exactly as it already does for
`RemoteRequested`/`ExamineRequested` from the player panel. With no player controller the
button is absent, and the dialog is unchanged from today.

### 3. Automatic capture becomes one dialog with Next and Previous

A new **`AutoCaptureWizard`** replaces the Examine → Set up capture chain as the way an
automatic capture is taken. One non-modal window (the spectrum and waveform panels are what
the user watches during a run, exactly as the guided dialog's comment says), one instance
(`QPointer` in `MainWindow`), four pages behind a `QStackedWidget` with Previous / Next /
page-specific action buttons.

Hand-built `QDialog` + `QStackedWidget`, **not `QWizard`**: every dialog in this
application is hand-built against the widget-test conventions (object names on every
control, buildable with null controllers), the flow needs non-modality and a page that
refuses to advance while a run is live, and `QWizard`'s frame, watermark and button
machinery would be fought at every step for no gain.

The pages:

1. **Disc** — what is in the player. The full examination runs on this page: auto-started
   on first entry when the connection is live and the examiner is idle, with the stage
   line, address-free progress bar and cancel semantics carried over from
   `ExamineDialog::SetProgress`/`Cancel` (including "said as soon as it is asked for").
   Below the result: the **shared naming form** (Phase 1), prefilled from the profile by
   the same rules as decision 2 — plus the suggested capture name
   (`SuggestedCaptureName`, resolved against the destination exactly as the guided dialog
   does today, so the prefill is never a name the file will not get). Re-examine is a
   button on the page; Next is enabled once a profile with `disc_type` and
   `programme_end` exists, which is the same gate `ExamineDialog::ApplyState` applies to
   its Set up capture button today.
2. **Capture settings** — what to take and where to put it. The shape radios, range
   controls, video-standard question (only when the profile lacks it), key lock and
   stop-with-player checkboxes move here from the guided dialog, unchanged in behaviour —
   the same `AutoCapturePlan`, the same `ValidateAutoCapturePlan` gate, the same estimate
   and free-space sentence. Joined by the settings that today force a detour to the
   Capture panel: destination folder, format, sample rate. All three read and write
   `CaptureController::SetSettings()` so the panel, the wizard and the settings file
   cannot disagree (the `loading_`-guard pattern from `CapturePanel::ShowSettings` copied
   as-is). Next is enabled while the plan validates; the problem sentence
   (`PlanProblemText`) says why when it is not.
3. **Capture** — the run. Start is the page's action button; progress is the guided
   dialog's address-based bar and stage-plus-remaining line, moved. While
   `AutoCaptureController::running()`, Previous and Next are disabled and closing the
   window is refused with the same "watching the run" rationale
   `MainWindow::ShowGuidedCaptureDialog` applies today. Stop cancels through
   `AutoCaptureController::Cancel()` with the finalise-properly wording kept.
4. **Summary** — what happened. The outcome sentence (`AutoCaptureSummary`), the file
   path and size (from `CaptureController::CaptureFinished`), the duration. Two buttons:
   **Capture another side** — returns to page 1, increments the side in the naming fields,
   and re-examines, which is the both-sides-of-one-disc loop done in two clicks — and
   Close.

What this replaces and what stays:

- `GuidedCaptureDialog` is retired; its widgets and logic move into pages 2–3.
- `ExamineDialog` **stays**, as the diagnostic it is good at: the provenance-annotated
  report and Copy report have a purpose (bug reports, adding player definitions) that a
  wizard page should not carry. Its "Set up capture…" button now opens the wizard with the
  profile it already measured, landing on page 2 — no second examination.
- `AutoCaptureController` is untouched. The wizard is one more caller.

Entry points converge: **Capture ▸ Automatic capture…** appears as a button on the Capture
panel (enabled while the player connection is live) and as a menu action beside the other
player entries. Both open the same single instance.

### 4. The Player dock is removed and its contents go where they were already going

Everything on the panel exists somewhere else or moves somewhere better:

| Panel element | Where it lives afterwards |
| --- | --- |
| Player control checkbox | The menu action (already exists); also on the remote's Connection tab, with the serial-port explanation as its tooltip |
| Search now, Remote…, Examine… buttons | Menu actions (already exist) |
| Connection summary / detail / source / verification | The remote dialog's **Connection** tab |
| "Use this model" button | The Connection tab, beside the mismatch text it belongs to |
| State / tray / disc / position readouts | The remote dialog's headline already carries state and position (`PlayerStatusBarText`); the full readout block joins the Connection tab |
| Status-at-a-glance | The status bar's permanent player label, unchanged |

The top-level **Player menu folds into Tools** as a section: Player control (toggle),
Search now, Remote control…, Examine disc…, Automatic capture…, Player settings…, above a
separator from the instrument entries. One player, set up once, is a tool — it does not
need a top-level menu any more than it needs a dock. (If the flat Tools menu reads too
long in practice, promoting the section back to a top-level menu is a one-line change;
the panel's removal does not depend on the choice.)

Removal mechanics: `BuildPlayerDock` goes, the dock's line leaves the Panels menu, and
`restoreState()` simply ignores the `player_dock` entry in saved layouts — no migration
needed. `player_panel.{h,cpp}` and its test retire once their assertions have moved (see
Phase 2 tests).

One consequence to handle deliberately: today the Remote menu action is **disabled unless
the connection is live**. Once the remote hosts the Connection tab, it is the place a user
goes to find out *why* nothing is connected — so the action becomes always enabled, and
the dialog opens on the Connection tab when the connection is not live, with the Control
tab's contents greyed exactly as `ApplyControls` already greys them.

### 5. The remote control dialog gets tabs

The headline (status line) stays above the tab bar — it is the one thing every tab wants
in view. Below it, a `QTabWidget`:

| Tab | Contents | Why |
| --- | --- | --- |
| **Control** | Transport, Go to, Display and audio (including key lock) | The remote a user actually uses, now at half the height |
| **Connection** | Enable checkbox, summary/detail/source/verification, Use this model, Search now, the status readout block | Decision 4's landing place; the diagnostics a disconnected user needs |
| **Disc codes** | The two user-code buttons and the byte view | Read rarely, and the 15-line byte view is most of the old dialog's height |
| **Manual command** | The raw command line and reply view | For adding player models — a developer surface, deliberately last |

Nothing behavioural changes: `GatedButton` gating, `ApplyAddressModes`, the
activated-not-currentIndexChanged rule for the two combos, and the request-id matching for
manual replies all move as they are. Every control keeps its object name, which is what
lets most of `test_player_remote_dialog.cpp` survive the move untouched.

## Phases

Each phase leaves the application working and shippable; none depends on a later one.

### Phase 1 — Extract the shared forms — **done**

Pure refactor, no behavioural change.

- **`CaptureNamingForm`** (new widget): the whole body of `CaptureNamingDialog` — the
  checkbox-gated fields, the per-side notes/mint memory, the apply-as-you-type model, the
  preview — moves into a reusable widget. The dialog becomes a thin shell around it. The
  wizard's page 1 embeds the same widget, which is what makes "the naming dialog and the
  wizard agree about naming" true by construction. `ClearAllFields()` is a public slot:
  what it clears belongs to the form, where to put the button that calls it does not, so
  the dialog keeps the button in its own button box.
- **`CapturePlanForm`** (new widget): the shape radios, CAV/CLV address controls,
  standard question, key lock and stop-with-player checkboxes, estimate and problem lines
  from `GuidedCaptureDialog`, exposed as `Plan()`, `problem()`, `disc()`, a
  `SetEditable()` for locking during a run, and a `Changed()` signal. The guided dialog
  becomes a shell around it (and stays that way until Phase 4 retires it), keeping only
  the headline, the capture name, the run's status and progress, and the buttons.
- Object names are kept verbatim, and both dialogs re-export the form constants under
  their old names, so the existing widget tests keep finding the controls through
  `findChild` without a line changed.

One placement decision made here because it settles a Phase 4 question: **the capture-name
field stays out of `CapturePlanForm`** — a name is not part of the plan, and Phase 4 puts
it on page 1 with the rest of the naming. In the guided dialog it therefore moves from the
middle of the second form block to just under the headline, above the plan. That reads in
the order the wizard will use (what it is called, then what to take), and is the only
visible difference this phase makes.

*Tests*: `test_capture_naming_dialog.cpp` and `test_guided_capture_dialog.cpp` pass
unmodified — that is the proof the extraction changed nothing.

### Phase 2 — Remote tabs, Connection tab, dock removal

- Restructure `PlayerRemoteDialog` per decision 5; build the Connection tab from the
  player panel's widgets and connections (the settings-checkbox sync, the
  `UseConnectedModel` hookup, the search-enabled-only-while-disconnected rule).
- Make the Remote menu action always enabled; open on the Connection tab when not live.
- Remove the Player dock and `player_panel.{h,cpp}`; fold the Player menu into Tools.

*Tests*: assertions from `test_player_panel.cpp` migrate into
`test_player_remote_dialog.cpp` (connection-tab section) before the panel file is
deleted; a main-window test covers the menu fold and the absence of the dock from the
Panels menu.

### Phase 3 — The manual path

- The examiner scope (`kFull`/`kIdentify`) in `DiscExaminer` and through
  `PlayerController::Examine()`.
- `NamingRequested` signal from `CapturePanel`; `MainWindow` builds the naming dialog
  with both controllers; the **Ask the player** button and profile→fields fill.
- The Naming… button's attention state on the Capture panel.

*Tests*: unit tests for the identify plan (no seek steps in the plan, settling present,
partial refusal still yields a profile) beside the existing examiner tests in
`tests/player/`; widget tests for the fill rules (known facts fill and tick, unknown facts
leave fields alone, typed text is never overwritten) and for the attention state's
condition.

### Phase 4 — The wizard

- `AutoCaptureWizard` with the four pages, embedding the Phase 1 forms; single-instance
  and close-refusal-while-running handling in `MainWindow`.
- Entry points: Capture panel button, Tools menu action; `ExamineDialog`'s Set up
  capture… retargets to the wizard (profile handed over, page 2).
- `GuidedCaptureDialog` retired.

*Tests*: a widget test per page gate (Next disabled until examined; Next disabled while
the plan does not validate; navigation locked while running; another-side loop increments
the side and restarts the examination), plus the null-controller build the window tests
require. The run itself is already covered by the `AutoCaptureController` tests and does
not need re-proving through the wizard.

### Phase 5 — Sweep

- Wording pass over every entry point so the two paths are named consistently (“capture” /
  “automatic capture”).
- User documentation in [docs/](../docs/) updated to describe the two paths.
- The **Future** ledger in
  [ddd-gui-implementation-plan.md](ddd-gui-implementation-plan.md) updated for anything
  this plan discharged or moved.

## What deliberately does not change

- The engine layer: controllers, sequences, examiner (beyond the additive scope),
  validation, estimates, text helpers, settings storage.
- The one-direction coupling rule (player may stop capture, never the reverse).
- The manual capture controls and their lockout rules on the Capture panel.
- The single-source-of-truth settings pattern — every new surface reads and writes
  `SetSettings()` and listens to `SettingsChanged`, never keeping a copy.
- The non-modality of every player-facing window, and the single-instance rule for each.
