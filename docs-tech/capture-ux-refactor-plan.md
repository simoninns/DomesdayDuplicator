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
  capture** (`guided_capture_dialog.cpp`, retired in Phase 4),
  with the destination, format and rate on the Capture panel, and the naming fields in a
  dialog none of those open. A user doing the normal thing — capture both sides of one
  disc — navigates all of it twice.

Alongside those two paths, two pieces of standing clutter:

- **The Player dock panel** (`player_panel.cpp`, removed in Phase 2)
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
Search now, Remote control…, Examine disc…, Automatic capture…, above a separator from the
instrument entries. (Player settings… was in this list and was later dropped — see *After
the plan* below.) One player, set up once, is a tool — it does not
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

### Phase 2 — Remote tabs, Connection tab, dock removal — **done**

- `PlayerRemoteDialog` restructured per decision 5: the headline stays above a
  `QTabWidget` carrying Control, Connection, Disc codes and Manual command, in that
  order — how often each is wanted. The window is now titled "Player" rather than
  "Remote control", since it is no longer only a remote. `ShowTab()` chooses the opening
  page.
- The Connection tab is built from the player panel's widgets and connections whole: the
  settings-checkbox sync, the `UseConnectedModel` hookup, the
  search-enabled-only-while-disconnected rule, and the status readout block with its
  hidden optical-assembly row.
- The Remote menu action is always enabled — see decision 4 — and the main window opens
  the dialog on Connection when nothing is connected and on Control when something is.
  The tab is chosen only when the window is created, so somebody returning to a remote
  they left on the manual page is not moved.
- The Player dock and `player_panel.{h,cpp}` are gone, and the Player menu is now a
  section at the top of Tools, separated from the instrument entries.

*Tests*: every assertion from `test_player_panel.cpp` migrated into
`test_player_remote_dialog.cpp` as a Connection-tab section before the panel was deleted,
joined by tests for the tab structure and the opening page. `test_main_window_panels.cpp`
covers the menu fold, the absence of the dock and its Panels entry, and the
remote-is-always-reachable rule.

Two things worth recording:

**A pre-existing bug, found by writing the menu tests.** `Examine disc…` had no initial
enabled state — it was only ever set from the `ConnectionChanged` handler. So it sat
enabled from the moment the window opened until the first connection report arrived, and
with player control switched off no report ever comes, leaving it enabled for the whole
session with nothing behind it. Fixed by setting it alongside the search action's initial
state.

**The upgrade path is tested rather than assumed.** Anybody running an earlier build has a
saved layout naming a `player_dock` that no longer exists. `ALayoutSavedWhenThereWasAPlayerDockStillRestores`
writes exactly such a state through `QMainWindow::saveState` and asserts that the
remaining docks come back and a hidden one stays hidden — that the saved arrangement is
carried over rather than discarded. Removing a dock is the one change here that could
otherwise reach a user as a fault.

### Phase 3 — The manual path — **done**

- `ExamineScope` (`kFull`/`kIdentify`) on `DiscExaminer`, defaulted to `kFull` so no
  existing caller changes, and carried through `PlayerWorker::Examine()` and
  `PlayerController::Examine()`. The identify plan is cut at the line where steps stop
  reading and start moving the disc — the Pioneer user code is on the moving side of that
  line, which is the easy one to get wrong, since it is a query that seeks to the lead-in
  to answer.
- `CapturePanel::NamingRequested` replaces the panel opening the dialog itself, and
  `MainWindow::ShowNamingDialog` builds it with both controllers. The panel has no
  business knowing a player exists; only the main window holds both.
- `CaptureNamingForm` takes an optional `PlayerController` and grows an **Ask the player**
  button — absent, not disabled, where there is no player layer at all — plus a public
  `FillFromProfile()` that Phase 4's wizard page 1 will call with its own examination's
  result.
- `CaptureNamingFields::DescribesDisc()` in the engine answers "has anything been said
  about this disc", and the Capture panel's Naming… button colours itself through
  `ActiveButtonStyle` with a new muted-amber `kAttention` token when the answer is no.

Three decisions worth recording:

**The fill rule has two halves, not one.** Nothing typed is ever overwritten — title,
notes, mint marks and metadata notes are things only a person knows, and a button that
cleared them because a disc was spun up would be unusable. But the three fields the player
*can* answer are overwritten even when they were set by hand: somebody who ticked CAV and
then asked the disc, which said CLV, asked because they wanted the disc's answer. Both
halves are asserted.

**A side number the form cannot hold is not followed.** The spin box tops out at
`kMaximumDiscSide`, and setting it beyond that would silently clamp — recording a wrong
side as an established fact. Out-of-range readings leave the field untouched instead.

**The nudge never blocks.** An unnamed capture is a legitimate way to work, so the Start
button is untouched by it and test mode suppresses it entirely (the name is forced to
`TestData_` there, so the button has nothing to offer). It clears on the keystroke rather
than when the field loses focus, since a button that stayed coloured while somebody typed
into the very field it was pointing at would be arguing with them.

*Tests*: seven examiner unit tests for the identify plan — no moving steps, the exact step
sequence, the disc left still, a shorter honest step count, partial refusal, an open tray,
and the default scope still being everything. Widget tests for each fill rule, for the
attention state's four conditions, and for the button emitting rather than opening. Plus
`CaptureNamingAskTest`, which drives the whole path against a scripted player and asserts
on the wire that no seek and no `?U` went out for a naming field — the property the scope
exists for, checked where it can actually be observed.

### Phase 4 — The wizard — **done**

- `AutoCaptureWizard`: four pages behind a `QStackedWidget`, embedding
  `CaptureNamingForm` on page 1 and `CapturePlanForm` on page 2. Page 1 auto-starts the
  examination when the connection is live; page 4 is reached automatically when the run
  finishes, because somebody who has left a forty-minute side running wants the answer on
  screen rather than a button to press for it.
- `MainWindow` holds one instance (`QPointer` + `WA_DeleteOnClose`), and the wizard's own
  `closeEvent` refuses while a capture is running — it is the only thing reporting that
  run, and closing it would leave a disc spinning with no way back to Stop.
- Entry points: **Tools ▸ Automatic capture…**, a button on the Capture panel, and
  `ExamineDialog`'s "Set up capture…", which hands its profile over and lands on page 2
  without examining again.
- `GuidedCaptureDialog` retired.

Three things worth recording:

**A real bug, found by the page tests.** `RebuildPlanForm` first used `deleteLater()` on
the outgoing form. A form awaiting deletion is still a child of the window: it still
answers `findChild`, and it is still connected to `Refresh()`. So after a second disc
arrived, anything looking a control up by name got the *previous* disc's copy — the CAV
frame boxes were still findable on a CLV disc, and the two forms disagreed about what was
being captured until the event loop next ran. Now deleted outright, with the reason and
the re-entrancy argument recorded at the call site.

**The Capture panel still knows nothing about players.** Its automatic-capture button is
gated by `SetAutomaticCaptureAvailable(bool)`, which the main window drives from the
connection. The panel learns only that one of its buttons is or is not available, which
keeps Phase 3's boundary intact.

**The guided dialog's tests did not simply move.** They were split by what they were
actually about: the shape, address, standard, key-lock and estimate assertions became
`test_capture_plan_form.cpp`, testing the form directly rather than through a window;
the naming, run-progress and outcome assertions became `test_auto_capture_wizard.cpp`.
The plan form also gained a lock-and-release test that the guided dialog never had.

*Tests*: 15 on the plan form and 23 on the wizard — every page gate, the hand-over from
the Examine report, the rebuild on a second disc, navigation locked during a run, the
auto-advance to the summary, and the other-side loop both when a side is being recorded
and when it is not.

### Phase 5 — Sweep — **done**

- **Wording.** The Examine window's button is **Automatic capture…**, the same words as the
  Tools entry and the Capture panel's button, because all three open the same window and
  start the same thing — "Set up capture…" read as a third feature. Every comment naming
  the retired guided setup now names what actually does the job, and `CapturePlanForm`'s
  object names lost the `guided_` prefix they inherited from a class that no longer exists.
- **User documentation.** [player-control.md](../docs/content/capture-gui/player-control.md)
  gains a *Two ways to take a capture* section, an *Ask the player* section, a page-by-page
  account of the wizard and a description of the player window's four tabs; the section
  index and the quick start carry the same two-path framing;
  [capture-control.md](../docs/content/capture-gui/capture-control.md) documents the
  Automatic capture… button and the Naming… nudge;
  [capture-naming.md](../docs/content/capture-gui/capture-naming.md) documents Ask the
  player and both halves of its fill rule; and every **Player →** menu path in the docs is
  now **Tools ▸**, because that menu no longer exists.
- **The ledger.** The three player rows in
  [ddd-gui-implementation-plan.md](ddd-gui-implementation-plan.md) that named a dock, a
  one-column remote or a guided dialog now say where each landed, with a note above the
  table that a row names the surface this plan left behind rather than the one that first
  discharged it. Nothing was discharged or retired by this plan — it moved things — and the
  table says so rather than leaving a reader to infer it from silence.
- [player-control-implementation-plan.md](player-control-implementation-plan.md) gains a
  note at the top mapping each UI surface it describes to what replaced it. Its engine
  reasoning is still the reason to read it; its UI passages are now history, and a document
  that read as current while describing a deleted dock would be worse than one that says so.

Two things worth recording:

**A real wording bug, found by the sweep.** Two strings — the status bar's and the Capture
panel's, both for a device with no firmware — sent the user to **Help ▸ Firmware…**. That
entry is on **Tools**, and has been since it was built. Somebody following either message
would have opened the About box.

**The menu entry and the window it opens still differ, deliberately.** **Remote control…**
opens a window titled "Player". Renaming the entry to match was tried on paper and made the
menu worse: a section that already begins "Player control" and ends "Player settings…" does
not need a third entry starting with the same word, and "Remote control…" is the only one of
the three that says what pressing it does. The window keeps the broader title because it is
no longer only a remote.

## What deliberately does not change

- The engine layer: controllers, sequences, examiner (beyond the additive scope),
  validation, estimates, text helpers, settings storage.
- The one-direction coupling rule (player may stop capture, never the reverse).
- The manual capture controls and their lockout rules on the Capture panel.
- The single-source-of-truth settings pattern — every new surface reads and writes
  `SetSettings()` and listens to `SettingsChanged`, never keeping a copy.
- The non-modality of every player-facing window, and the single-instance rule for each.

## After the plan

Changes made on review, once the whole of it was being used rather than read. Recorded here
rather than folded into the phases above, because the phases are what was designed and this
is what using it taught.

**The wizard opened too small.** Each page is a `QScrollArea`, and a scroll area reports a
small fixed size hint whatever it contains — so the window came up a couple of hundred pixels
square regardless of the four pages inside it, with page 1 scrolling almost immediately. It
now opens at 760×820, bounded to 90% of the available screen so it cannot come up with its
own Next button below the desktop edge. The pages stay scroll areas: page 1 genuinely is
long, and on a short screen no-scroll would be the worse failure.

**The naming form's opening sentence was incoherent**, and worse, it sat flush under the
wizard's Capture name field where it read as a description of *that*. Rewritten, and the
capture name moved into a **What it will be called** group box of its own — two blocks each
with a heading, because the name is one thing and what the disc is another, even though the
second can build the first.

**A name already taken now gets " (1)", " (2)"** rather than "_2", and the note beside it
says the resulting name and nothing else. What happens is what every desktop does with a name
already in use; the paragraph explaining it was a paragraph nobody needed to read twice. The
first collision is (1) and not (2), because the number counts the copies rather than the
files.

**The device and the destination folder left the Capture panel** for the Capture tab of
File ▸ Settings…. Neither changes once it is set — a Duplicator does not move between USB
ports and captures do not move between drives — and a control that is set once does not earn
a row on the panel somebody works from. The panel's device combo was already writing
`preferred_device_path`, the same value the Settings dialog shows, so this removed a second
view of one setting rather than moving a setting.

Two consequences handled deliberately. The panel's **status line** is now the only thing it
says about the device, so it carries the diagnosis as well as the state — a device in recovery
or on a USB 2 port says so there. And **Free space** names its folder in a tooltip, because
the number is worth nothing if you cannot tell which volume it is about and the folder is no
longer on the panel to read off.

*A real regression, caught by an existing test.* The first version asked
`SelectDevice(…) != nullptr` for "is there a device worth enabling the buttons for", on the
strength of a comment claiming that was the engine's own question. It is not:
`DeviceSelection::kCaptureCapable` means "running capture firmware" and says nothing about the
link, so a device on a USB 2 port passed it and the Start buttons came back to life on a
device that cannot carry 80 MB/s. Both properties are checked now, which is what the panel's
own code did before the combo was removed.

**Tools ▸ Player settings… is gone.** It opened File ▸ Settings… on its Player tab — the same
dialog one menu along, so it bought a saved click and cost a menu entry that read as a second
place to configure a player. `SettingsDialog::Tab` stays: a caller may still know which half
somebody is after.

**One wording bug found on the way.** Two strings — the status bar's and the Capture panel's,
both for a device with no firmware — sent the user to **Help ▸ Firmware…**. That entry is on
**Tools**, and always has been.
