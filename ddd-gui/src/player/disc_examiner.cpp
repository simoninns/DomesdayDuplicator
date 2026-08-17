/************************************************************************

    disc_examiner.cpp

    Working out what is in the player, one step at a time
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "disc_examiner.h"

#include <utility>

namespace ddd::player {

DiscExaminer::DiscExaminer(const PlayerDefinition& definition,
                           std::string_view firmware, ExamineScope scope)
    : definition_(&definition),
      controls_(ControlsFor(definition, firmware)),
      scope_(scope) {
  BuildPlan();
}

void DiscExaminer::BuildPlan() {
  // Always: there is no examination without knowing what the player is doing,
  // and the three queries below it are the ones every definition is required to
  // have.
  plan_.push_back(ExamineStage::kCheckingPlayer);

  if (controls_.Has(PlayerCommand::kPlay)) {
    plan_.push_back(ExamineStage::kSpinningUp);
  }

  plan_.push_back(ExamineStage::kReadingDiscStatus);

  // Two queries that read the disc rather than drive it, so they cost about
  // twenty milliseconds each and are worth asking before anything expensive.
  if (controls_.Has(PlayerCommand::kQueryTvSystem)) {
    plan_.push_back(ExamineStage::kReadingTvSystem);
  }

  // Everything above this line is a question the player answers off the disc it
  // has already read. Everything below moves the disc — the Pioneer user code
  // searches to the lead-in, the chapter probe is a search, and the length is
  // measured by seeking to both ends of the side. That is the whole difference
  // between the two scopes, and it is why the line is here.
  if (scope_ == ExamineScope::kIdentify) {
    AppendRestoreSteps();
    return;
  }

  // Here, or not at all.
  //
  // Pioneer's manual recommends issuing the user-code request immediately after
  // spin-up and before any other control command, and the reason is mechanical:
  // the player searches to the lead-in to answer it. Anywhere later in this
  // sequence it would undo a measurement that had just been taken.
  //
  // Always asked for, never offered as a choice. It is the disc's own
  // identifying record and the examination is the one moment in a session when
  // reading it is free of consequence: the player is about to be sent to the
  // lead-in anyway, and everything that depends on position happens after.
  if (controls_.Has(PlayerCommand::kQueryPioneerUserCode)) {
    plan_.push_back(ExamineStage::kReadingPioneerUserCode);
  }

  if (controls_.Has(PlayerCommand::kQueryStandardUserCode)) {
    plan_.push_back(ExamineStage::kReadingStandardUserCode);
  }

  // Only where the disc cannot be asked instead. The disc-status reply carries
  // a chapter field, and a model that reports it has already answered this
  // question without the disc moving — so probing for chapter one would be a
  // search command sent to find out something already known.
  if (!definition_->disc_status.chapters.present() &&
      controls_.Has(PlayerCommand::kSeekChapter)) {
    plan_.push_back(ExamineStage::kCheckingChapters);
  }

  // Which of the two seeks is used depends on the disc, which is not known
  // yet — so the pair goes in if either is available and Applies() decides on
  // the day.
  if (controls_.Has(PlayerCommand::kSeekFrame) ||
      controls_.Has(PlayerCommand::kSeekTimeCode)) {
    plan_.push_back(ExamineStage::kFindingEnd);
    plan_.push_back(ExamineStage::kReadingEnd);
    plan_.push_back(ExamineStage::kFindingStart);
    plan_.push_back(ExamineStage::kReadingStart);
  }

  AppendRestoreSteps();
}

void DiscExaminer::AppendRestoreSteps() {
  if (controls_.Has(PlayerCommand::kPause)) {
    plan_.push_back(ExamineStage::kSettling);
  }

  // Whether this one runs is decided on the day, in Applies(): the plan is
  // built in the constructor and what the player was doing is not known until
  // the first query has been answered.
  //
  // After the settle rather than instead of it, and the order matters on a
  // Pioneer player: stop is Reject, and a Reject arriving at a transport that
  // is already spinning down opens the tray. From a held-still disc it is an
  // ordinary spin-down, and it is the only stop this examination ever sends.
  if (controls_.Has(PlayerCommand::kStop)) {
    if (controls_.Has(PlayerCommand::kQueryActiveMode)) {
      plan_.push_back(ExamineStage::kCheckingTransport);
    }
    plan_.push_back(ExamineStage::kSpinningDown);
  }
}

AddressMode DiscExaminer::addressing() const {
  return profile_.addressing.known() ? profile_.addressing.value
                                     : AddressMode::kFrame;
}

bool DiscExaminer::Applies(ExamineStage stage) const {
  switch (stage) {
    case ExamineStage::kSpinningUp:
      // A disc that is already turning does not need starting, and starting it
      // again would cost the ten seconds of a spin-up for nothing.
      return !spinning_;

    case ExamineStage::kCheckingTransport:
      // No point asking where no stop could follow.
      return spun_up_here_;

    case ExamineStage::kSpinningDown:
      // Two conditions, and both are load-bearing. The first: only where this
      // examination is why the disc is turning — a player that was already
      // running is left running, because put back means put back, not stopped.
      // The second: only where the player has just said the disc is moving. A
      // stop is Reject on a Pioneer player, and a Reject sent to a disc that is
      // not turning opens the tray.
      return spun_up_here_ && disc_turning_;

    case ExamineStage::kFindingEnd:
    case ExamineStage::kFindingStart:
      // A disc whose type was never established cannot be seeked on: the frame
      // and time-code seeks are different commands with differently sized
      // addresses, and guessing which is the one mistake this whole library is
      // arranged to avoid.
      return profile_.disc_type.known() &&
             controls_.Has(profile_.disc_type.value == DiscType::kClv
                               ? PlayerCommand::kSeekTimeCode
                               : PlayerCommand::kSeekFrame);

    case ExamineStage::kReadingEnd:
      return end_seek_sent_;

    case ExamineStage::kReadingStart:
      // Only when the seek to the start was accepted.
      //
      // Not symmetrical with the end, and deliberately: a seek *past* the end
      // is expected to be refused — the player runs to the end of the side and
      // then says no, which is the entire technique — while a seek to the first
      // address of the disc has no such excuse. A refusal there means the
      // player did not move, so reading the address afterwards would report
      // wherever it still was as the start of the programme.
      return start_seek_ok_;

    default:
      return true;
  }
}

ExamineStep DiscExaminer::StepFor(ExamineStage stage) const {
  ExamineStep step;
  step.stage = stage;

  const bool clv =
      profile_.disc_type.known() && profile_.disc_type.value == DiscType::kClv;

  switch (stage) {
    case ExamineStage::kCheckingPlayer:
      step.command = PlayerCommand::kQueryActiveMode;
      break;
    case ExamineStage::kSpinningUp:
      step.command = PlayerCommand::kPlay;
      break;
    case ExamineStage::kReadingDiscStatus:
      step.command = PlayerCommand::kQueryDiscStatus;
      break;
    case ExamineStage::kReadingTvSystem:
      step.command = PlayerCommand::kQueryTvSystem;
      break;
    case ExamineStage::kReadingPioneerUserCode:
      step.command = PlayerCommand::kQueryPioneerUserCode;
      break;
    case ExamineStage::kReadingStandardUserCode:
      step.command = PlayerCommand::kQueryStandardUserCode;
      break;
    case ExamineStage::kCheckingChapters:
      step.command = PlayerCommand::kSeekChapter;
      step.argument = 1;
      break;
    case ExamineStage::kFindingEnd:
      step.command =
          clv ? PlayerCommand::kSeekTimeCode : PlayerCommand::kSeekFrame;
      step.argument = clv ? kImpossibleTimeCode : kImpossibleFrame;
      break;
    case ExamineStage::kFindingStart:
      step.command =
          clv ? PlayerCommand::kSeekTimeCode : PlayerCommand::kSeekFrame;
      step.argument = kFirstAddress;
      break;
    case ExamineStage::kReadingEnd:
    case ExamineStage::kReadingStart:
      step.command = PlayerCommand::kQueryAddress;
      break;
    case ExamineStage::kSettling:
      step.command = PlayerCommand::kPause;
      break;
    case ExamineStage::kCheckingTransport:
      step.command = PlayerCommand::kQueryActiveMode;
      break;
    case ExamineStage::kSpinningDown:
      step.command = PlayerCommand::kStop;
      break;
    case ExamineStage::kIdle:
    case ExamineStage::kFinished:
      break;
  }

  return step;
}

void DiscExaminer::Advance() {
  while (index_ < plan_.size() && !Applies(plan_[index_])) {
    ++index_;
    ++completed_;
  }

  if (index_ >= plan_.size() && !finished()) {
    outcome_ = ExamineOutcome::kCompleted;
  }
}

std::optional<ExamineStep> DiscExaminer::Next() {
  if (finished()) {
    return std::nullopt;
  }

  if (pending_.has_value()) {
    return pending_;
  }

  Advance();
  if (finished()) {
    return std::nullopt;
  }

  pending_ = StepFor(plan_[index_]);
  return pending_;
}

void DiscExaminer::Apply(const Reply& reply) {
  if (finished() || !pending_.has_value()) {
    return;
  }

  const ExamineStage stage = pending_->stage;
  pending_.reset();
  ++index_;
  ++completed_;

  // The two statuses that are about the link rather than about the disc. Every
  // other outcome — refused, unanswered, unreadable, unsupported — leaves this
  // step's field unknown and the examination carries on.
  if (reply.status == ReplyStatus::kLinkFailed ||
      reply.status == ReplyStatus::kNotConnected) {
    outcome_ = ExamineOutcome::kLinkFailed;
    return;
  }

  switch (stage) {
    case ExamineStage::kCheckingPlayer:
      ApplyCheckingPlayer(reply);
      break;
    case ExamineStage::kSpinningUp:
      ApplySpinningUp(reply);
      break;
    case ExamineStage::kReadingDiscStatus:
      ApplyDiscStatus(reply);
      break;
    case ExamineStage::kReadingTvSystem:
      ApplyTvSystem(reply);
      break;
    case ExamineStage::kReadingPioneerUserCode:
      ApplyUserCode(reply, profile_.pioneer_user_code);
      break;
    case ExamineStage::kReadingStandardUserCode:
      ApplyUserCode(reply, profile_.standard_user_code);
      break;
    case ExamineStage::kCheckingChapters:
      ApplyChapters(reply);
      break;
    case ExamineStage::kFindingEnd:
      // Nothing to read from this one. A refusal is the expected answer — the
      // address asked for is past the end of any disc — and where the player
      // stopped is what the next step asks.
      end_seek_sent_ = true;
      break;
    case ExamineStage::kReadingEnd:
      ApplyEndAddress(reply);
      break;
    case ExamineStage::kFindingStart:
      start_seek_ok_ = reply.ok();
      break;
    case ExamineStage::kReadingStart:
      ApplyStartAddress(reply);
      break;
    case ExamineStage::kCheckingTransport:
      // A refusal or an unreadable answer leaves this false, and that is the
      // answer to act on: not knowing whether the disc is moving is not a
      // licence to send the command that opens the tray if it is not.
      if (reply.ok()) {
        disc_turning_ =
            IsSpinning(ParsePlayerState(reply.text, definition_->state_decode));
      }
      break;
    case ExamineStage::kSettling:
    case ExamineStage::kSpinningDown:
    case ExamineStage::kIdle:
    case ExamineStage::kFinished:
      // Nothing to read from any of these, and for the two tidy-up steps a
      // refusal is not a failure: the examination's findings are already in
      // hand by then, and a player that will not be tidied up after is a fact
      // about the player rather than about the disc. The last two are named
      // only to keep the switch exhaustive — no step is ever built for them, so
      // no reply can arrive carrying one.
      break;
  }

  if (!finished()) {
    Advance();
  }
}

void DiscExaminer::ApplyCheckingPlayer(const Reply& reply) {
  if (!reply.ok()) {
    return;
  }

  const PlayerState state =
      ParsePlayerState(reply.text, definition_->state_decode);
  if (state == PlayerState::kUnknown) {
    return;
  }

  profile_.tray.Record(TrayStateFor(state), Provenance::kReported);

  if (state == PlayerState::kDoorOpen) {
    // Nothing below this can be answered with the tray open, and the answer the
    // user needs is one sentence long. Reported as a finding rather than as a
    // failure: the examination did what it could and the disc is not in the
    // player.
    outcome_ = ExamineOutcome::kTrayOpen;
    return;
  }

  if (IsSpinning(state)) {
    spinning_ = true;
    profile_.disc_present.Record(true, Provenance::kReported);
  }
}

void DiscExaminer::ApplySpinningUp(const Reply& reply) {
  if (reply.status == ReplyStatus::kRefused) {
    // The player was asked to play and said no with the tray shut. There is no
    // disc, or none it can read, and every remaining step would be a refusal
    // taking its own timeout to arrive at.
    profile_.disc_present.Record(false, Provenance::kMeasured);
    outcome_ = ExamineOutcome::kNoDisc;
    return;
  }

  if (reply.ok()) {
    profile_.disc_present.Record(true, Provenance::kMeasured);
    profile_.tray.Record(TrayState::kClosed, Provenance::kInferred);

    // This examination is now the reason the disc is turning, so it is this
    // examination's job to stop it again — see kSpinningDown.
    spun_up_here_ = true;
  }
}

void DiscExaminer::ApplyDiscStatus(const Reply& reply) {
  if (!reply.ok()) {
    return;
  }

  // Kept whole as well as decoded: a report that says "side 2" and shows the
  // reply it read that from is one somebody can check.
  profile_.disc_status_reply = reply.text;

  const DiscStatus status =
      ParseDiscStatus(reply.text, definition_->disc_status);
  if (!status.valid) {
    return;
  }

  // Every field independently, because the player answers each of them
  // independently — including with "I could not tell", which is not "no".
  //
  // The exception is C1, which is only allowed to add a disc and never to take
  // one away. By this point the player may already have accepted a play
  // command, which is stronger evidence than a status digit; a player
  // contradicting itself should not be able to turn that into "no disc" and
  // abandon a measurement that was going to work.
  if (status.loaded.value_or(false) || !profile_.disc_present.known()) {
    profile_.disc_present.Record(status.loaded.value_or(false),
                                 Provenance::kReported);
  }

  if (status.size != DiscSize::kUnknown) {
    profile_.disc_size.Record(status.size, Provenance::kReported);
  }

  if (status.side.has_value()) {
    profile_.disc_side.Record(*status.side, Provenance::kReported);
  }

  if (status.chapters.has_value()) {
    profile_.chapters.Record(*status.chapters, Provenance::kReported);
  }

  if (status.type == DiscType::kUnknown) {
    return;
  }

  profile_.disc_type.Record(status.type, Provenance::kReported);
  profile_.addressing.Record(AddressModeFor(status.type),
                             Provenance::kInferred);
}

void DiscExaminer::ApplyTvSystem(const Reply& reply) {
  if (!reply.ok()) {
    return;
  }

  const TvSystem system = ParseTvSystem(reply.text, definition_->tv_system);
  if (!system.valid || system.disc == VideoStandard::kUnknown) {
    return;
  }

  // The disc's field, not the output's. On a player that converts they differ,
  // and a capture is of what is on the disc.
  profile_.video_standard.Record(system.disc, Provenance::kReported);
}

void DiscExaminer::ApplyUserCode(const Reply& reply, UserCodeReading& into) {
  if (reply.ok() && IsErrorCode(reply.text)) {
    // "E04" and the like. The LD-V4400 manual documents it as no user code
    // being encoded on the disc, which is a fact about the disc and not a fault
    // — so it is recorded rather than discarded, and the report says which.
    into.outcome = UserCodeReading::Outcome::kNotEncoded;
    into.text = reply.text;
    return;
  }

  if (reply.ok() && !reply.text.empty()) {
    into.outcome = UserCodeReading::Outcome::kRead;
    into.text = reply.text;
    return;
  }

  into.outcome = UserCodeReading::Outcome::kRefused;
  into.text = reply.text;
}

void DiscExaminer::ApplyChapters(const Reply& reply) {
  if (reply.ok()) {
    profile_.chapters.Record(true, Provenance::kMeasured);
    return;
  }

  if (reply.status == ReplyStatus::kRefused) {
    profile_.chapters.Record(false, Provenance::kMeasured);
  }

  // Anything else — silence, an unreadable answer — leaves it unknown. "The
  // player did not answer" is not "the disc has no chapters".
}

void DiscExaminer::ApplyEndAddress(const Reply& reply) {
  if (!reply.ok()) {
    return;
  }

  const DiscAddress address = ParseAddress(reply.text, addressing());
  if (address.valid) {
    profile_.programme_end.Record(address.value, Provenance::kMeasured);
  }
}

void DiscExaminer::ApplyStartAddress(const Reply& reply) {
  if (!reply.ok()) {
    return;
  }

  const DiscAddress address = ParseAddress(reply.text, addressing());

  // Only where the reply said something about where the player is. A reply that
  // carried neither an address nor a lead-in marker says nothing, and recording
  // "the lead-in is not reachable" from it would be inventing a finding.
  if (address.valid || address.in_lead_in) {
    profile_.lead_in_reachable.Record(address.in_lead_in,
                                      Provenance::kMeasured);
  }

  if (address.valid) {
    profile_.programme_start.Record(address.value, Provenance::kMeasured);
  }
}

void DiscExaminer::Cancel() {
  if (finished()) {
    return;
  }

  pending_.reset();
  outcome_ = ExamineOutcome::kCancelled;
}

void DiscExaminer::Restart() {
  index_ = 0;
  completed_ = 0;
  pending_.reset();
  outcome_ = ExamineOutcome::kInProgress;
  profile_ = DiscProfile{};
  spinning_ = false;
  end_seek_sent_ = false;
  start_seek_ok_ = false;
}

ExamineStage DiscExaminer::stage() const {
  if (pending_.has_value()) {
    return pending_->stage;
  }
  return finished() ? ExamineStage::kFinished : ExamineStage::kIdle;
}

}  // namespace ddd::player
