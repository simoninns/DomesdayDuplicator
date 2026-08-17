/************************************************************************

    auto_capture_controller.cpp

    Where the player and the capture engine meet
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "auto_capture_controller.h"

#include <QTimer>

#include "capture_controller.h"
#include "logger.h"
#include "player_controller.h"
#include "player_registry.h"
#include "player_text.h"

namespace ddd::gui {

AutoCaptureController::AutoCaptureController(PlayerController* player,
                                             CaptureController* capture,
                                             capture::ILogger* logger,
                                             QObject* parent)
    : QObject(parent), player_(player), capture_(capture), logger_(logger) {
  timer_ = new QTimer(this);
  timer_->setSingleShot(true);
  connect(timer_, &QTimer::timeout, this, &AutoCaptureController::Step);

  if (player_ != nullptr) {
    settings_ = player_->settings();

    connect(player_, &PlayerController::RequestCompleted, this,
            &AutoCaptureController::OnReply);
    connect(player_, &PlayerController::StatusUpdated, this,
            &AutoCaptureController::OnStatusUpdated);
    connect(player_, &PlayerController::SettingsChanged, this,
            &AutoCaptureController::SetSettings);
  }

  if (capture_ != nullptr) {
    connect(capture_, &CaptureController::CapturingChanged, this,
            &AutoCaptureController::OnCapturingChanged);
  }
}

AutoCaptureController::~AutoCaptureController() = default;

void AutoCaptureController::SetSettings(const PlayerSettings& settings) {
  settings_ = settings;
}

void AutoCaptureController::Start(const player::AutoCapturePlan& plan,
                                  const player::DiscProfile& disc) {
  if (running() || player_ == nullptr || capture_ == nullptr) {
    return;
  }

  if (!player_->connected()) {
    emit Finished(player::AutoCaptureOutcome::kLinkFailed);
    return;
  }

  plan_ = plan;

  // The definition the player identified itself as, or the generic Pioneer set
  // for one whose ID no definition claims — which is the same fallback the
  // session made when it connected, and produces a sequence that works.
  const PlayerConnection& connection = player_->connection();
  const player::PlayerDefinition* definition =
      player::FindPlayerByIdCode(connection.model_id_code.toStdString());
  if (definition == nullptr) {
    definition = &player::GenericPlayer();
  }

  sequence_ = std::make_unique<player::AutoCaptureSequence>(
      *definition, connection.firmware_version.toStdString(), plan, disc);

  if (sequence_->finished()) {
    // Refused before anything was sent — an invalid plan, or a player with no
    // transport. Reported through the same signal as every other ending, so a
    // window waiting for one never waits forever.
    const player::AutoCaptureOutcome outcome = sequence_->outcome();
    sequence_.reset();
    emit Finished(outcome);
    return;
  }

  // What the file will say about the disc it came from. Set before the capture
  // is opened, because a file's tags are written into its header when it is
  // created and a fact that arrived afterwards has nowhere to go.
  capture_->SetDiscProvenance(DescribeDisc(disc));

  // The sequence owns the session for its duration. Without this the status
  // poll would interleave a query into the middle of a seek, and a reply
  // attributed to the wrong command is how a seek comes to report the tray
  // state.
  player_->SetPaused(true);

  stopped_readings_ = 0;
  delayed_ = false;

  Log(tr("Automatic capture started."));
  emit RunningChanged(true);

  Step();
}

void AutoCaptureController::Step() {
  if (sequence_ == nullptr) {
    return;
  }

  const std::optional<player::AutoCaptureStep> step = sequence_->Next();
  if (!step.has_value()) {
    FinishRun();
    return;
  }

  const int address =
      sequence_->last_address().has_value() ? *sequence_->last_address() : -1;
  emit Progress(step->stage, address);

  // The watch's pacing, and the only place a step's delay is used. The flag is
  // what stops this waiting again for the same step when the timer brings it
  // back round — Next() hands out the same step until it is answered, so
  // without one the watch would wait for ever and never poll.
  if (step->delay.count() > 0 && !delayed_) {
    delayed_ = true;
    timer_->start(static_cast<int>(step->delay.count()));
    return;
  }
  delayed_ = false;

  switch (step->action) {
    case player::AutoCaptureAction::kStartCapture:
      capture_->StartCapture();

      // Whether the engine actually attached a writer, rather than whether it
      // was asked to. StartCapture reports its own failure through Failed(),
      // which the window shows; what the sequence needs is one bit.
      Apply(player::CaptureDone(capture_->capturing()));
      return;

    case player::AutoCaptureAction::kStopCapture:
      capture_->StopCapture();
      Apply(player::CaptureDone(!capture_->capturing()));
      return;

    case player::AutoCaptureAction::kSendCommand:
      break;
  }

  pending_request_ =
      player_->Send(CommandRequest(step->command, step->argument));
}

void AutoCaptureController::Apply(const player::StepResult& result) {
  if (sequence_ == nullptr) {
    return;
  }

  sequence_->Apply(result);
  delayed_ = false;

  // Round again through the event loop rather than by recursion. A whole-side
  // capture is thousands of steps and a recursive drive would be thousands of
  // frames of stack; it also gives the interface a chance to paint between
  // steps, which is what keeps Stop answerable.
  QTimer::singleShot(0, this, &AutoCaptureController::Step);
}

void AutoCaptureController::OnReply(const PlayerReply& reply) {
  if (sequence_ == nullptr || reply.request.id != pending_request_ ||
      pending_request_ == 0) {
    // Somebody else's answer. The remote is still usable during a capture, so
    // this is an ordinary occurrence rather than a fault.
    return;
  }

  pending_request_ = 0;

  player::Reply parsed;
  parsed.status = reply.status;
  parsed.text = reply.text.toStdString();
  parsed.error_code = reply.error_code.toStdString();
  parsed.sent = reply.sent.toStdString();

  Apply(player::PlayerReplied(parsed));
}

void AutoCaptureController::FinishRun() {
  if (sequence_ == nullptr) {
    return;
  }

  const player::AutoCaptureOutcome outcome = sequence_->outcome();
  const bool left_running = sequence_->capture_left_running();

  sequence_.reset();
  pending_request_ = 0;
  delayed_ = false;
  timer_->stop();

  if (player_ != nullptr) {
    player_->SetPaused(false);
  }

  // Cleared rather than left behind. It describes the disc this run was set up
  // for, and a capture somebody takes by hand afterwards may well be of a
  // different one — a file carrying the previous disc's side number would be
  // worse than one carrying none.
  if (capture_ != nullptr) {
    capture_->SetDiscProvenance({});
  }

  Log(tr("Automatic capture %1.").arg(AutoCaptureOutcomeText(outcome)));

  if (left_running) {
    // The one branch that ends with a capture still writing. Said plainly,
    // because the alternative is a file that outlives the thing that started it
    // with nobody told.
    Log(tr("The capture is still running and is now yours to stop."));
  }

  emit RunningChanged(false);
  emit Finished(outcome);
}

void AutoCaptureController::Cancel() {
  if (sequence_ == nullptr) {
    return;
  }

  sequence_->Cancel();

  // If nothing is outstanding, the cancel has already taken the sequence into
  // its tail and there is nobody to wake it up.
  if (pending_request_ == 0 && !timer_->isActive()) {
    QTimer::singleShot(0, this, &AutoCaptureController::Step);
  }
}

void AutoCaptureController::OnCapturingChanged(bool capturing,
                                               const QString& path) {
  static_cast<void>(path);

  if (capturing) {
    stopped_readings_ = 0;
    return;
  }

  // Not while a sequence is running: it stops the player itself, in its own
  // order and after the file has been finalised.
  if (running() || !settings_.stop_player_with_capture) {
    return;
  }

  if (player_ == nullptr || !player_->connected()) {
    return;
  }

  Log(tr("Stopping the player, because the capture has stopped."));
  player_->Send(CommandRequest(player::PlayerCommand::kStop));
}

void AutoCaptureController::OnStatusUpdated(
    const player::PlayerStatus& status) {
  if (!settings_.stop_capture_with_player || running() || capture_ == nullptr ||
      !capture_->capturing()) {
    stopped_readings_ = 0;
    return;
  }

  if (player::IsSpinning(status.state) ||
      status.state == player::PlayerState::kUnknown) {
    // Anything but a player that has definitely stopped resets the count. A
    // reading nobody could parse is not evidence of a stop, and treating it as
    // one is how a link with a loose connector truncates a capture.
    stopped_readings_ = 0;
    return;
  }

  ++stopped_readings_;
  if (stopped_readings_ < kStoppedReadings) {
    return;
  }

  stopped_readings_ = 0;
  Log(tr("Stopping the capture, because the player has stopped."));
  capture_->StopCapture();
}

capture::DiscProvenance AutoCaptureController::DescribeDisc(
    const player::DiscProfile& disc) const {
  capture::DiscProvenance facts;

  // Every field only where it is known. A file claiming a side number nothing
  // established would send somebody looking for a disc that does not exist.
  if (player_ != nullptr && !player_->connection().model_name.isEmpty()) {
    const PlayerConnection& connection = player_->connection();
    facts.player =
        connection.firmware_version.isEmpty()
            ? connection.model_name.toStdString()
            : QStringLiteral("%1 (firmware %2)")
                  .arg(connection.model_name, connection.firmware_version)
                  .toStdString();
  }

  if (disc.disc_type.known()) {
    facts.disc_type = DiscTypeName(disc.disc_type.value).toStdString();
  }
  if (disc.disc_size.known()) {
    facts.disc_size = DiscSizeName(disc.disc_size.value).toStdString();
  }
  if (disc.disc_side.known()) {
    facts.disc_side = QString::number(disc.disc_side.value).toStdString();
  }
  if (disc.video_standard.known()) {
    facts.video_standard =
        VideoStandardName(disc.video_standard.value).toStdString();
  }

  const player::AddressMode mode = disc.addressing.known()
                                       ? disc.addressing.value
                                       : player::AddressMode::kFrame;

  if (disc.programme_start.known()) {
    facts.programme_start =
        FormatDiscAddress(disc.programme_start.value, mode).toStdString();
  }
  if (disc.programme_end.known()) {
    facts.programme_end =
        FormatDiscAddress(disc.programme_end.value, mode).toStdString();
  }

  return facts;
}

void AutoCaptureController::Log(const QString& message) const {
  if (logger_ != nullptr) {
    logger_->Info(message.toStdString());
  }
}

}  // namespace ddd::gui
