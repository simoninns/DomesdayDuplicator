/************************************************************************

    auto_capture_sequence.cpp

    Capturing a side by itself, one step at a time
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "auto_capture_sequence.h"

#include <utility>

namespace ddd::player {

AutoCaptureSequence::AutoCaptureSequence(const PlayerDefinition& definition,
                                         std::string_view firmware,
                                         AutoCapturePlan plan, DiscProfile disc)
    : plan_(std::move(plan)),
      disc_(std::move(disc)),
      definition_(&definition),
      controls_(ControlsFor(definition, firmware)) {
  // Checked before a single byte is sent, and checked against the same function
  // the dialog enables its button with. A plan that got this far invalid is a
  // programming fault rather than a user one, and the run stops here with
  // nothing spun up and nothing written.
  if (ValidateAutoCapturePlan(plan_, disc_) != PlanProblem::kNone) {
    outcome_ = AutoCaptureOutcome::kInvalidPlan;
    stage_ = AutoCaptureStage::kFinished;
    return;
  }

  // A player that cannot be told to play cannot be captured from
  // automatically — and one that cannot seek cannot be sent to the start of a
  // range. Said now, so the user is not told about it by a capture that starts
  // and immediately stops.
  const bool needs_seek = !BeginsWithSpinUp(plan_.shape);
  if (!controls_.Has(PlayerCommand::kPlay) ||
      (needs_seek && !controls_.Has(SeekCommand()))) {
    outcome_ = AutoCaptureOutcome::kUnsupportedPlayer;
    stage_ = AutoCaptureStage::kFinished;
    return;
  }

  stage_ = AutoCaptureStage::kLockingFrontPanel;
}

PlayerCommand AutoCaptureSequence::SeekCommand() const {
  return plan_.addressing == AddressMode::kTimeCode
             ? PlayerCommand::kSeekTimeCode
             : PlayerCommand::kSeekFrame;
}

PlayerCommand AutoCaptureSequence::PlayCommand() const {
  // A CAV disc with stop codes pauses partway through a side, which ends a
  // whole-side capture with no error and no explanation — the old application
  // met this and this is how it got past it. CLV discs carry no stop codes, so
  // the plain play command is what they get.
  if (plan_.addressing == AddressMode::kFrame &&
      controls_.Has(PlayerCommand::kPlayWithoutStopCodes)) {
    return PlayerCommand::kPlayWithoutStopCodes;
  }
  return PlayerCommand::kPlay;
}

AutoCaptureStep AutoCaptureSequence::StepFor(AutoCaptureStage stage) const {
  AutoCaptureStep step;
  step.stage = stage;

  switch (stage) {
    case AutoCaptureStage::kLockingFrontPanel:
      step.command = PlayerCommand::kKeyLockOn;
      break;
    case AutoCaptureStage::kConfirmingDisc:
      step.command = PlayerCommand::kQueryDiscStatus;
      break;
    case AutoCaptureStage::kSpinningDown:
      step.command = PlayerCommand::kStop;
      break;
    case AutoCaptureStage::kSeekingStart:
      step.command = SeekCommand();
      step.argument = plan_.start_address;
      break;
    case AutoCaptureStage::kStartingCapture:
      step.action = AutoCaptureAction::kStartCapture;
      break;
    case AutoCaptureStage::kSpinningUp:
      step.command = PlayCommand();
      break;
    case AutoCaptureStage::kWatching:
      step.command = PlayerCommand::kQueryAddress;
      step.delay = kWatchInterval;
      break;
    case AutoCaptureStage::kCheckingStall:
      step.command = PlayerCommand::kQueryActiveMode;
      break;
    case AutoCaptureStage::kStoppingCapture:
      step.action = AutoCaptureAction::kStopCapture;
      break;
    case AutoCaptureStage::kStoppingPlayer:
      step.command = PlayerCommand::kStop;
      break;
    case AutoCaptureStage::kUnlockingFrontPanel:
      step.command = PlayerCommand::kKeyLockOff;
      break;
    case AutoCaptureStage::kIdle:
    case AutoCaptureStage::kFinished:
      break;
  }

  return step;
}

void AutoCaptureSequence::Advance() {
  switch (stage_) {
    case AutoCaptureStage::kLockingFrontPanel:
      stage_ = AutoCaptureStage::kConfirmingDisc;
      return;
    case AutoCaptureStage::kConfirmingDisc:
      stage_ = BeginsWithSpinUp(plan_.shape) ? AutoCaptureStage::kSpinningDown
                                             : AutoCaptureStage::kSeekingStart;
      return;
    case AutoCaptureStage::kSpinningDown:
    case AutoCaptureStage::kSeekingStart:
      stage_ = AutoCaptureStage::kStartingCapture;
      return;
    case AutoCaptureStage::kStartingCapture:
      stage_ = AutoCaptureStage::kSpinningUp;
      return;
    case AutoCaptureStage::kSpinningUp:
      stage_ = AutoCaptureStage::kWatching;
      return;
    default:
      // The watch decides for itself, and the tail is AdvanceTeardown's.
      return;
  }
}

void AutoCaptureSequence::Finish(AutoCaptureOutcome outcome) {
  if (finishing_) {
    return;
  }

  finishing_ = true;
  pending_outcome_ = outcome;
  pending_.reset();

  if (outcome == AutoCaptureOutcome::kLinkFailed) {
    link_failed_ = true;
  }

  stage_ = FirstTeardownStage();
  AdvanceTeardown();
}

AutoCaptureStage AutoCaptureSequence::FirstTeardownStage() const {
  // A whole-side capture stops the player first and goes on writing through the
  // spin-down. That is not tidiness: the run-out is not an address, so it can
  // only reach a file by being recorded while the disc is being stopped, and a
  // capture that detached its writer first would end a few seconds short of
  // exactly the part nothing else can reach.
  return EndsWithSpinDown(plan_.shape) ? AutoCaptureStage::kStoppingPlayer
                                       : AutoCaptureStage::kStoppingCapture;
}

AutoCaptureStage AutoCaptureSequence::TeardownSuccessor(
    AutoCaptureStage stage) const {
  const bool player_first = EndsWithSpinDown(plan_.shape);

  switch (stage) {
    case AutoCaptureStage::kStoppingPlayer:
      return player_first ? AutoCaptureStage::kStoppingCapture
                          : AutoCaptureStage::kUnlockingFrontPanel;
    case AutoCaptureStage::kStoppingCapture:
      return player_first ? AutoCaptureStage::kUnlockingFrontPanel
                          : AutoCaptureStage::kStoppingPlayer;
    default:
      return AutoCaptureStage::kFinished;
  }
}

bool AutoCaptureSequence::TeardownStepApplies(AutoCaptureStage stage) const {
  switch (stage) {
    case AutoCaptureStage::kStoppingCapture:
      // The one exception, and it is the plan's rule rather than an oversight:
      // a player fault never destroys a capture. With the link gone there is no
      // way to know where the disc is, and the player will carry on to the end
      // of the side by itself — so the capture is left running and the caller
      // is told, through capture_left_running().
      //
      // **Only where the link is what ended the run**, though, and not merely
      // where it has since died. A whole-side capture stops the player before
      // the writer; a link that fails during that stop has not cost the run
      // anything — the side was captured — and detaching the writer needs no
      // serial link at all. Guarding on link_failed_ here instead would leave
      // that capture writing until the volume filled, which is the one failure
      // this whole tail exists to prevent.
      return capture_running() &&
             pending_outcome_ != AutoCaptureOutcome::kLinkFailed;

    case AutoCaptureStage::kStoppingPlayer:
      // Only a player that was actually started, and only over a link that
      // still exists.
      //
      // Not tidiness: on a Pioneer player the stop command is Reject, and a
      // Reject sent to a disc that is already spinning down opens the tray —
      // see PlayerCommand::kStop. `transport_started_` is what keeps this to at
      // most one stop per run, sent to a disc this sequence knows it set
      // turning. A run cancelled between the opening spin-down and the play
      // that follows it has not started the transport, so nothing is sent and
      // the tray stays shut.
      return transport_started_ && !link_failed_ &&
             controls_.Has(PlayerCommand::kStop);

    case AutoCaptureStage::kUnlockingFrontPanel:
      // A lock this sequence took is a lock this sequence releases. A link that
      // died with it on leaves the player's front panel locked and there is
      // nothing to be done about that from here — which is one of the reasons
      // the plan does not default it on.
      return key_lock_taken_ && !link_failed_ &&
             controls_.Has(PlayerCommand::kKeyLockOff);

    default:
      return false;
  }
}

void AutoCaptureSequence::CompleteTeardownStep(AutoCaptureStage stage) {
  stage_ = TeardownSuccessor(stage);
  AdvanceTeardown();
}

void AutoCaptureSequence::AdvanceTeardown() {
  // Walked in the order the shape decided, skipping what does not apply. One
  // loop and one predicate rather than conditions at each call site, because
  // the property this has to hold — nothing that was started is left running —
  // is only checkable if there is one place that decides it.
  while (stage_ != AutoCaptureStage::kFinished) {
    if (TeardownStepApplies(stage_)) {
      return;
    }
    stage_ = TeardownSuccessor(stage_);
  }

  outcome_ = pending_outcome_;
}

std::optional<AutoCaptureStep> AutoCaptureSequence::Next() {
  if (finished()) {
    return std::nullopt;
  }

  if (pending_.has_value()) {
    return pending_;
  }

  // A cancel that arrived while nothing was outstanding. Honoured here rather
  // than in Cancel() itself so that there is one place the tail is entered from
  // and one order in which it runs.
  if (cancel_requested_ && !finishing_) {
    Finish(AutoCaptureOutcome::kCancelled);
    if (finished()) {
      return std::nullopt;
    }
  }

  // The lock is the only step of the run proper that can be skipped outright.
  if (stage_ == AutoCaptureStage::kLockingFrontPanel &&
      (!plan_.key_lock || !controls_.Has(PlayerCommand::kKeyLockOn))) {
    stage_ = AutoCaptureStage::kConfirmingDisc;
  }

  // A model with no disc-status query is one this cannot ask about the disc.
  // The rest of the run is unaffected, so the check is skipped rather than the
  // capture refused.
  if (stage_ == AutoCaptureStage::kConfirmingDisc &&
      !controls_.Has(PlayerCommand::kQueryDiscStatus)) {
    Advance();
  }

  // Likewise a player with no stop command: the disc cannot be spun down, so a
  // lead-in capture gets whatever the lead-in of a disc that is already turning
  // amounts to, which is nothing. Not a reason to refuse the capture — the
  // programme is still captured in full.
  if (stage_ == AutoCaptureStage::kSpinningDown &&
      !controls_.Has(PlayerCommand::kStop)) {
    Advance();
  }

  pending_ = StepFor(stage_);

  // Recorded when the command goes out rather than when it is answered, and
  // deliberately: a refused seek or a refused play may still have spun the disc
  // up. "PL64RBMF" is three commands in one, and a player that took the first
  // and rejected the third is playing a disc the sequence would otherwise
  // believe it had never started. The tail then leaves it running.
  if (stage_ == AutoCaptureStage::kSeekingStart ||
      stage_ == AutoCaptureStage::kSpinningUp) {
    transport_started_ = true;
  }

  return pending_;
}

void AutoCaptureSequence::Apply(const StepResult& result) {
  if (finished() || !pending_.has_value()) {
    return;
  }

  const AutoCaptureStage stage = pending_->stage;
  const AutoCaptureAction action = pending_->action;
  pending_.reset();

  if (action == AutoCaptureAction::kStartCapture) {
    if (!result.capture_ok) {
      // Nothing has been written and the disc is not running. The player is put
      // back — the lock released, and stopped if the seek spun it up — and the
      // engine's own report is what tells the user why.
      Finish(AutoCaptureOutcome::kCaptureFailed);
      return;
    }
    capture_started_ = true;
    Advance();
    return;
  }

  if (action == AutoCaptureAction::kStopCapture) {
    // Recorded as stopped whatever the engine said. A detach that reports a
    // failure has still detached — the sink is gone — and asking again would be
    // a second stop rather than a remedy.
    capture_stopped_ = true;

    // It does change what the run is called, though. A capture whose file would
    // not finalise is not a capture that completed, and a run reported as
    // completed is one nobody goes back and checks.
    if (!result.capture_ok &&
        pending_outcome_ == AutoCaptureOutcome::kCompleted) {
      pending_outcome_ = AutoCaptureOutcome::kCaptureFailed;
    }

    CompleteTeardownStep(AutoCaptureStage::kStoppingCapture);
    return;
  }

  const Reply& reply = result.reply;

  // The link, rather than the disc. Every other unhappy answer — refused,
  // unanswered, unreadable, unsupported — is dealt with by the step that asked.
  if (reply.status == ReplyStatus::kLinkFailed ||
      reply.status == ReplyStatus::kNotConnected) {
    if (finishing_) {
      // The tail cannot be run over a link that is not there. Whatever ended
      // the run is still what is reported; the capture, if one is running, is
      // left running.
      link_failed_ = true;
      CompleteTeardownStep(stage);
      return;
    }
    Finish(AutoCaptureOutcome::kLinkFailed);
    return;
  }

  switch (stage) {
    case AutoCaptureStage::kLockingFrontPanel:
      ApplyLockingFrontPanel(reply);
      break;
    case AutoCaptureStage::kConfirmingDisc:
      ApplyConfirmingDisc(reply);
      break;
    case AutoCaptureStage::kSpinningDown:
      ApplySpinningDown(reply);
      break;
    case AutoCaptureStage::kSeekingStart:
      ApplySeekingStart(reply);
      break;
    case AutoCaptureStage::kSpinningUp:
      ApplySpinningUp(reply);
      break;
    case AutoCaptureStage::kWatching:
      ApplyWatching(reply);
      break;
    case AutoCaptureStage::kCheckingStall:
      ApplyCheckingStall(reply);
      break;

    case AutoCaptureStage::kStoppingPlayer:
    case AutoCaptureStage::kUnlockingFrontPanel:
      // Nothing to read. A player that refuses to stop is a player that is
      // already stopped as far as anything here can tell, and turning that into
      // a failure would overwrite the reason the run actually ended.
      CompleteTeardownStep(stage);
      return;

    case AutoCaptureStage::kStartingCapture:
    case AutoCaptureStage::kStoppingCapture:
    case AutoCaptureStage::kIdle:
    case AutoCaptureStage::kFinished:
      break;
  }

  // A cancel that arrived while this step was in flight. Applied after the
  // reply so that what was learnt from it is not thrown away — and before the
  // next step is built, so nothing further is sent.
  if (cancel_requested_ && !finishing_) {
    Finish(AutoCaptureOutcome::kCancelled);
  }
}

void AutoCaptureSequence::ApplyLockingFrontPanel(const Reply& reply) {
  // A refusal leaves the front panel live, which is a smaller thing than the
  // capture the user asked for. Recorded either way so that the tail does not
  // release a lock that was never taken.
  key_lock_taken_ = reply.ok();
  Advance();
}

void AutoCaptureSequence::ApplyConfirmingDisc(const Reply& reply) {
  if (!reply.ok()) {
    // A player that will not answer this is not evidence that the disc changed.
    // Partial failure is this library's rule: the check is skipped and the
    // capture goes ahead, because the alternative refuses discs on players that
    // seek and play perfectly well.
    Advance();
    return;
  }

  const DiscStatus status =
      ParseDiscStatus(reply.text, definition_->disc_status);
  if (!status.valid) {
    Advance();
    return;
  }

  const bool gone = status.loaded.has_value() && !*status.loaded;

  // The type is what the plan's addresses are written in, so a disagreement
  // here means every figure in the plan means something else.
  const bool wrong_type = status.type != DiscType::kUnknown &&
                          disc_.disc_type.known() &&
                          status.type != disc_.disc_type.value;

  // The side and the size are the disc's own programme status and cost nothing
  // to compare. A side that has been flipped is the commonest way for the disc
  // to change between an examination and a capture, and its length is
  // different.
  const bool wrong_side = status.side.has_value() && disc_.disc_side.known() &&
                          *status.side != disc_.disc_side.value;

  const bool wrong_size = status.size != DiscSize::kUnknown &&
                          disc_.disc_size.known() &&
                          status.size != disc_.disc_size.value;

  if (gone || wrong_type || wrong_side || wrong_size) {
    Finish(AutoCaptureOutcome::kDiscChanged);
    return;
  }

  Advance();
}

void AutoCaptureSequence::ApplySpinningDown(const Reply& reply) {
  // Deliberately lenient. The aim of this step is that the disc is not turning,
  // and a player that refuses a stop it has no use for has met the aim. Several
  // models answer an already-stopped stop with an error.
  static_cast<void>(reply);
  Advance();
}

void AutoCaptureSequence::ApplySeekingStart(const Reply& reply) {
  if (!reply.ok()) {
    // Fatal, and unlike the spin-down deliberately so: a seek that did not
    // happen leaves the player wherever it was, and starting a capture there
    // would produce a file of the wrong part of the disc with nothing in it to
    // say so.
    Finish(AutoCaptureOutcome::kPlayerRefused);
    return;
  }

  Advance();
}

void AutoCaptureSequence::ApplySpinningUp(const Reply& reply) {
  if (!reply.ok()) {
    // The capture is running by now, so this is the branch that most needs the
    // tail: the writer is detached and the file finalised rather than left open
    // on a disc that never started.
    Finish(AutoCaptureOutcome::kPlayerRefused);
    return;
  }

  Advance();
}

void AutoCaptureSequence::ApplyWatching(const Reply& reply) {
  if (!reply.ok()) {
    // Not readable, so not evidence of movement. It counts towards the stall
    // rather than being ignored: a player answering nothing useful while a file
    // grows is exactly the state this watch exists to notice.
    ++readings_without_advance_;
    if (readings_without_advance_ >= kStallReadings) {
      stage_ = AutoCaptureStage::kCheckingStall;
    }
    return;
  }

  const DiscAddress address = ParseAddress(reply.text, plan_.addressing);

  if (address.in_lead_out) {
    // Past the end of the programme. The measured end may be a frame or two
    // short of where the player thinks the lead-out begins, and running into it
    // is the same finding as reaching the end address.
    Finish(AutoCaptureOutcome::kCompleted);
    return;
  }

  if (address.in_lead_in) {
    // The number in a lead-in reply is not a programme address, so it is not
    // compared with anything and does not count as a stall. This is where a
    // whole-side capture spends its first seconds, and the disc is moving.
    readings_without_advance_ = 0;
    return;
  }

  if (!address.valid) {
    ++readings_without_advance_;
    if (readings_without_advance_ >= kStallReadings) {
      stage_ = AutoCaptureStage::kCheckingStall;
    }
    return;
  }

  if (address.value >= plan_.end_address) {
    last_address_ = address.value;
    Finish(AutoCaptureOutcome::kCompleted);
    return;
  }

  if (!last_address_.has_value() || address.value > *last_address_) {
    last_address_ = address.value;
    readings_without_advance_ = 0;
    return;
  }

  ++readings_without_advance_;
  if (readings_without_advance_ >= kStallReadings) {
    stage_ = AutoCaptureStage::kCheckingStall;
  }
}

void AutoCaptureSequence::ApplyCheckingStall(const Reply& reply) {
  const PlayerState state =
      reply.ok() ? ParsePlayerState(reply.text, definition_->state_decode)
                 : PlayerState::kUnknown;

  switch (state) {
    case PlayerState::kSettingUp:
    case PlayerState::kSearching:
      // On its way. A spin-up takes seconds and a search can take longer, and
      // neither is a stall — the address is not moving because the player has
      // not started reading the programme yet.
      readings_without_advance_ = 0;
      stage_ = AutoCaptureStage::kWatching;
      return;

    case PlayerState::kPlaying:
    case PlayerState::kScanning:
    case PlayerState::kMultiSpeed:
      // The player says it is playing and the address says it is not. Something
      // is wrong with the disc or the player, and the file would otherwise grow
      // until the volume filled.
      Finish(AutoCaptureOutcome::kStalled);
      return;

    case PlayerState::kStillFrame:
    case PlayerState::kPaused:
    case PlayerState::kParked:
    case PlayerState::kUnloading:
    case PlayerState::kDoorOpen:
      // The player stopped of its own accord, or somebody stopped it. A
      // finding, not a fault: the capture up to that point is good and is
      // finalised properly.
      Finish(AutoCaptureOutcome::kPlayerStopped);
      return;

    case PlayerState::kUnknown:
      // Asked twice now — once for an address and once for a state — and
      // neither answered. Ended rather than watched indefinitely, for the same
      // reason as a stall.
      Finish(AutoCaptureOutcome::kStalled);
      return;
  }

  Finish(AutoCaptureOutcome::kStalled);
}

void AutoCaptureSequence::Cancel() {
  if (finished()) {
    return;
  }

  // Recorded rather than acted on, so that a cancel arriving in the middle of a
  // step does not lose that step's reply — and so that the tail is entered from
  // the one place that knows what has to be undone.
  cancel_requested_ = true;

  if (!pending_.has_value() && !finishing_) {
    Finish(AutoCaptureOutcome::kCancelled);
  }
}

}  // namespace ddd::player
