/************************************************************************

    auto_capture_sequence.h

    Capturing a side by itself, one step at a time
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>

#include "auto_capture_plan.h"
#include "disc_profile.h"
#include "player_command.h"
#include "player_controls.h"
#include "player_definition.h"
#include "response_parser.h"

namespace ddd::player {

// What the automatic capture is doing, in the order it does it.
//
// The ordering is the old application's, preserved exactly, because it is what
// makes the result usable rather than merely correct — see kSpinningDown, which
// is the step nobody would arrive at by reasoning about it.
enum class AutoCaptureStage : uint8_t {
  kIdle,

  // Lock the player's front panel, if the plan asked for it.
  kLockingFrontPanel,

  // Is this still the disc the plan was built for? It may have been swapped
  // since the examination, and everything below is addressed in units that only
  // mean anything on the disc that was measured.
  kConfirmingDisc,

  // Is the disc turning? Asked before the step below, and only before that
  // step.
  //
  // **This exists because the stop below is dangerous.** On a Pioneer player
  // stop is Reject, and a Reject arriving at a transport that is already
  // stopped or spinning down opens the tray. So the sequence has to know before
  // it can safely send one — and it cannot assume, because the disc examination
  // that usually precedes a capture now leaves the player as it found it, which
  // for a parked disc means parked.
  kCheckingTransport,

  // Stop the disc, for a capture that is to hold the spin-up.
  //
  // **The non-obvious step.** There is no command that puts a player on the
  // lead-in, so the only way it reaches a file is for the capture to be running
  // while the disc comes up from a stop — which means stopping a disc the user
  // has very likely just started. Nothing about the rest of the sequence hints
  // at this, and the old application's ordering is the only evidence it
  // matters.
  //
  // Skipped when the disc is already stopped, which is not an optimisation: it
  // is the difference between a capture and an ejected disc.
  kSpinningDown,

  // Or, for a capture of part of the side, go to where it starts.
  kSeekingStart,

  // Attach the writer. Before the player is started, always: a capture that
  // began after the disc did would be missing its own beginning.
  kStartingCapture,

  // Start the disc — with the disc's stop codes ignored on a CAV disc, because
  // a stop code partway through a side would otherwise pause the player and end
  // a whole-side capture early.
  kSpinningUp,

  // Watch the address until it reaches the end of the plan.
  kWatching,

  // Ask the player what it thinks it is doing, because the address has not
  // moved for several readings. One query, and it distinguishes the three
  // things that look identical from the address alone: a player still getting
  // up to speed, a player that has stopped, and a player that claims to be
  // playing a disc that is not moving.
  kCheckingStall,

  // Is the disc still turning? Asked immediately before the stop below, every
  // time, and this is the rule rather than a special case: **nothing in this
  // application sends a stop to a player it has not just asked about.**
  //
  // "This sequence started the transport" is not the same claim as "the
  // transport is moving now". Somebody can stop the disc from the player's own
  // front panel mid-capture; the run can end *because* the player stopped,
  // which is an ordinary outcome and one of the two the stop-with-player
  // coupling exists to produce. In every one of those the disc is already
  // stopped, and a Reject sent to a stopped Pioneer transport opens the tray —
  // so the tidy-up would eject the disc somebody had just finished capturing.
  kCheckingBeforeStopping,

  // The tail, run down whatever ended the run.
  //
  // **The order of the first two depends on the shape.** A whole-side capture
  // stops the player first and keeps writing through the spin-down, because the
  // run-out is not an address and this is the only way it reaches a file. Every
  // other shape stops the capture first, since there is nothing at its end
  // worth the extra seconds.
  kStoppingCapture,
  kStoppingPlayer,
  kUnlockingFrontPanel,

  kFinished,
};

// How the run ended.
enum class AutoCaptureOutcome : uint8_t {
  kInProgress,

  // The end of the plan was reached, or the player ran into the lead-out.
  kCompleted,

  // The plan does not describe a capture that could be made of this disc.
  // Checked before anything is sent — see ValidateAutoCapturePlan.
  kInvalidPlan,

  // The connected player has no command for something the plan needs: no way to
  // play, or no way to seek for a plan that has to seek.
  kUnsupportedPlayer,

  // The disc in the player is not the one the plan was built for.
  kDiscChanged,

  // The player said no to something the capture depends on.
  kPlayerRefused,

  // The address stopped advancing while the player went on claiming to play.
  // Ended here rather than left running, because the alternative is a file that
  // grows until the volume fills.
  kStalled,

  // The player stopped, paused or parked before the end of the plan. A finding
  // rather than a fault: a hand on the front panel does this, and so does a
  // disc defect the player cannot read past.
  kPlayerStopped,

  // The capture engine would not start, or would not stop.
  kCaptureFailed,

  // The serial link failed. **The capture is deliberately left running** — see
  // capture_left_running() below.
  kLinkFailed,

  kCancelled,
};

// What the caller is being asked to do.
enum class AutoCaptureAction : uint8_t {
  // Send `command` to the player and hand back its reply.
  kSendCommand,

  // Attach the writer, and say whether it attached.
  kStartCapture,

  // Detach it, and say whether it detached.
  kStopCapture,
};

// One thing to do next.
struct AutoCaptureStep {
  AutoCaptureStage stage = AutoCaptureStage::kIdle;
  AutoCaptureAction action = AutoCaptureAction::kSendCommand;

  PlayerCommand command = PlayerCommand::kQueryActiveMode;
  std::optional<int32_t> argument;

  // Wait this long before carrying the step out.
  //
  // Carried on the step rather than left to the caller's own timer, because it
  // is a property of the step: the watch is a poll and its rate belongs with
  // the logic that decides what a poll means. There is no clock in this
  // library, so this is a request rather than an enforcement — the test simply
  // asserts on the figure and does not wait.
  std::chrono::milliseconds delay{0};

  bool operator==(const AutoCaptureStep&) const = default;
};

// What happened when the caller carried a step out.
struct StepResult {
  // For a step that sent a command. Ignored for the two capture actions.
  Reply reply;

  // For kStartCapture and kStopCapture. Ignored for a command step.
  bool capture_ok = true;
};

inline StepResult PlayerReplied(Reply reply) {
  StepResult result;
  result.reply = std::move(reply);
  return result;
}

inline StepResult CaptureDone(bool ok) {
  StepResult result;
  result.capture_ok = ok;
  return result;
}

// The automatic capture, as a value rather than as control flow.
//
// The old application's state machine, re-expressed with its ordering preserved
// exactly and its structure discarded. That structure was four hundred lines of
// blocking calls inside a QThread::run(), and its failure branches — a disc
// swapped between the setup and the capture, a refused spin-up, a link that
// died mid-side — could only be reached by arranging for real hardware to
// misbehave. In practice that meant they were never reached until a user found
// one, with a capture in progress.
//
// Here every one of them is a test that runs in microseconds with nothing
// plugged in, including the property that matters most: **a capture that was
// started is stopped on every branch but one**, and the exception is stated
// rather than accidental.
//
// That exception is the link failing. The plan's rule is that a player fault
// never destroys a capture, so a link that drops mid-side ends the *automation*
// and leaves the capture running for the user to stop by hand — the player
// carries on to the end of the side regardless, and truncating a good capture
// because the cable came out would be the worse of the two failures. The
// sequence says so through capture_left_running(), so the caller reports it
// rather than the capture quietly outliving the thing that started it.
//
// Thread-safety: none, deliberately. One caller drives one sequence.
class AutoCaptureSequence {
 public:
  // The disc profile is the examination's, and is what the plan's addresses are
  // checked against. Both are copied: a sequence outlives the dialog that built
  // it, and a capture that read its bounds out of a window somebody had since
  // closed would be a capture with no bounds.
  AutoCaptureSequence(const PlayerDefinition& definition,
                      std::string_view firmware, AutoCapturePlan plan,
                      DiscProfile disc);

  // The step to carry out now, or nothing when the run is over.
  //
  // Stable until Apply(): asking twice yields the same step. The watch asks for
  // the same step many times over, which is what a poll is.
  std::optional<AutoCaptureStep> Next();

  // What happened when the caller carried that step out.
  void Apply(const StepResult& result);

  // Stop, finishing the capture properly rather than abandoning it.
  //
  // Takes effect between steps, and does not skip the tail: the writer is
  // detached, the file is finalised, the player is stopped and the front panel
  // is released. That is the difference from the old application, which
  // abandoned the run where it stood.
  void Cancel();

  bool finished() const { return outcome_ != AutoCaptureOutcome::kInProgress; }
  AutoCaptureOutcome outcome() const { return outcome_; }
  AutoCaptureStage stage() const { return stage_; }

  const AutoCapturePlan& plan() const { return plan_; }

  // Where the player was when it was last asked. Absent until the watch has
  // read an address — which is what a progress bar needs, since the two ends of
  // the plan are already known to whoever built it.
  std::optional<int32_t> last_address() const { return last_address_; }

  // Is there a writer attached, as far as this sequence knows?
  bool capture_running() const { return capture_started_ && !capture_stopped_; }

  // Did the run end leaving a capture running?
  //
  // True only on the link-failure branch, and it is the whole reason that
  // branch is allowed to exist: the caller reports "the automation stopped and
  // your capture is still going", which is a sentence somebody can act on. A
  // capture that outlived its sequence silently would not be.
  bool capture_left_running() const { return finished() && capture_running(); }

  // How often the address is read during the watch.
  //
  // Twice a second: fast enough that a capture stops within a frame or two of
  // where it was asked to, and slow enough to leave the link alone. At 1200
  // baud an address query is the better part of a second all by itself, which
  // this deliberately does not try to correct for — a poll that overran would
  // simply be answered late, and being late by a frame at the end of a
  // forty-minute side costs nothing.
  static constexpr std::chrono::milliseconds kWatchInterval{500};

  // Readings with no advance before the player is asked what it is doing.
  //
  // Five, so a genuine stall is noticed inside three seconds while a disc that
  // is merely between chapters is not accused of one.
  static constexpr int kStallReadings = 5;

 private:
  // Build the step for the current stage.
  AutoCaptureStep StepFor(AutoCaptureStage stage) const;

  // Move to the next stage of the run proper.
  void Advance();

  // Enter the tail, skipping the parts of it that do not apply, and remember
  // what the run is going to be reported as. Everything that ends a run goes
  // through here, which is what makes "a capture that was started is stopped"
  // a property of one function rather than of nine call sites.
  void Finish(AutoCaptureOutcome outcome);

  // Move to the next applicable step of the tail, ending the run when there are
  // none left. Called with stage_ already set to the step being considered.
  void AdvanceTeardown();

  // Where the tail starts, which is the shape's decision — see
  // kStoppingCapture.
  AutoCaptureStage FirstTeardownStage() const;

  // The tail's order, as a function rather than as a fall-through, so that a
  // step which has just been carried out cannot be considered again — which is
  // exactly the loop a tail written as "re-examine the current stage" produces.
  AutoCaptureStage TeardownSuccessor(AutoCaptureStage stage) const;

  // Is this step of the tail one this run actually has to take?
  bool TeardownStepApplies(AutoCaptureStage stage) const;

  // Whether a stop is on the cards at all in this teardown, leaving aside
  // whether the disc is actually moving. The condition the ask-first step and
  // the stop itself share, so the two cannot drift apart.
  bool WouldStopPlayer() const;

  // Record what the player said it was doing. Does not move the sequence on:
  // the two callers are in different halves of it and move on differently.
  void ApplyCheckingTransport(const Reply& reply);
  void ApplyCheckingTransportAndAdvance(const Reply& reply);

  // Mark a step of the tail done and move past it.
  void CompleteTeardownStep(AutoCaptureStage stage);

  void ApplyLockingFrontPanel(const Reply& reply);
  void ApplyConfirmingDisc(const Reply& reply);
  void ApplySpinningDown(const Reply& reply);
  void ApplySeekingStart(const Reply& reply);
  void ApplySpinningUp(const Reply& reply);
  void ApplyWatching(const Reply& reply);
  void ApplyCheckingStall(const Reply& reply);

  // Which play command this disc wants, and which seek.
  PlayerCommand PlayCommand() const;
  PlayerCommand SeekCommand() const;

  AutoCapturePlan plan_;
  DiscProfile disc_;
  const PlayerDefinition* definition_ = nullptr;
  PlayerControls controls_;

  AutoCaptureStage stage_ = AutoCaptureStage::kIdle;
  AutoCaptureOutcome outcome_ = AutoCaptureOutcome::kInProgress;

  // What the run will be reported as once the tail has been run down. Held
  // separately from outcome_ so that the tail's own steps cannot overwrite the
  // reason the run ended — a stop command that is refused during teardown does
  // not turn a stall into a refusal.
  AutoCaptureOutcome pending_outcome_ = AutoCaptureOutcome::kCompleted;

  // The step handed out and not yet answered.
  std::optional<AutoCaptureStep> pending_;

  bool finishing_ = false;
  bool cancel_requested_ = false;

  bool key_lock_taken_ = false;
  bool transport_started_ = false;

  // Whether the player said it was moving when kCheckingTransport asked.
  //
  // False until something says otherwise, and that default is the safe one: a
  // model that cannot be asked gets no stop, so a whole-side capture of it
  // misses the spin-up rather than risking the tray.
  bool disc_turning_ = false;
  bool capture_started_ = false;
  bool capture_stopped_ = false;

  // Set when the link has gone, so the tail does not send commands into a dead
  // port and wait out a timeout apiece for them.
  bool link_failed_ = false;

  std::optional<int32_t> last_address_;
  int readings_without_advance_ = 0;
};

}  // namespace ddd::player
