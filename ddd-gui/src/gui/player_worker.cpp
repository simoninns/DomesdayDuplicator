/************************************************************************

    player_worker.cpp

    The thread that owns the serial link
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "player_worker.h"

#include <QElapsedTimer>
#include <QTimer>
#include <algorithm>
#include <utility>

#include "logger.h"
#include "player_command.h"
#include "player_controls.h"
#include "player_definition.h"
#include "player_discovery.h"
#include "player_registry.h"
#include "player_text.h"
#include "qt_serial_port.h"
#include "response_parser.h"

namespace ddd::gui {
namespace {

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// A reply's payload, byte for byte.
//
// Latin-1 rather than UTF-8, and it matters: a reply is arbitrary bytes, not
// text. The Pioneer user code is a 200-byte record that may carry anything, and
// fromUtf8 would replace every byte above 0x7F that did not happen to form a
// valid sequence — so the hex dump the remote shows would be a dump of what
// this decode had already destroyed. Latin-1 maps all 256 values one-to-one and
// back again, which is the property wanted here.
QString ToPayload(std::string_view bytes) {
  return QString::fromLatin1(bytes.data(),
                             static_cast<qsizetype>(bytes.size()));
}

// Bytes as they should appear in a log line or a label: the trailing terminator
// taken off, since a carriage return in either helps nobody.
QString ToDisplayBytes(std::string_view bytes) {
  if (!bytes.empty() && bytes.back() == player::kCommandTerminator) {
    bytes.remove_suffix(1);
  }
  return ToPayload(bytes);
}

// The probe outcome that says most about what is wrong.
//
// A scan of six ports produces six answers, and the one worth showing is not
// the last: "something answered and was not a player" is a finding, while "that
// port could not be opened" is the least informative of them and is what most
// ports on a machine will say.
//
// A port that was *refused* outranks silence, though, and deliberately: it may
// well be the port the player is on, and unlike every other outcome here it
// names something the user can go and do. A machine where one port is refused
// and another simply has nothing on it is better served by the first sentence
// than the second.
PlayerConnectionProblem WorseProblem(PlayerConnectionProblem current,
                                     PlayerConnectionProblem candidate) {
  const auto rank = [](PlayerConnectionProblem problem) {
    switch (problem) {
      case PlayerConnectionProblem::kNotAPlayer:
        return 4;
      case PlayerConnectionProblem::kPortNotPermitted:
        return 3;
      case PlayerConnectionProblem::kNoPlayerFound:
        return 2;
      case PlayerConnectionProblem::kPortUnavailable:
        return 1;
      case PlayerConnectionProblem::kNone:
      case PlayerConnectionProblem::kLinkLost:
        return 0;
    }
    return 0;
  };

  return rank(candidate) > rank(current) ? candidate : current;
}

PlayerConnectionProblem ProblemFor(const player::ProbeResult& result) {
  switch (result.status) {
    case player::ProbeResult::Status::kPortUnavailable:
      return result.open_error == player::PortOpenError::kNotPermitted
                 ? PlayerConnectionProblem::kPortNotPermitted
                 : PlayerConnectionProblem::kPortUnavailable;
    case player::ProbeResult::Status::kUnusableAnswer:
      return PlayerConnectionProblem::kNotAPlayer;
    case player::ProbeResult::Status::kNoAnswer:
      return PlayerConnectionProblem::kNoPlayerFound;
    case player::ProbeResult::Status::kConnected:
      return PlayerConnectionProblem::kNone;
  }
  return PlayerConnectionProblem::kNoPlayerFound;
}

}  // namespace

PlayerWorker::PlayerWorker(PlayerBackend backend, capture::ILogger* logger,
                           QObject* parent)
    : QObject(parent), backend_(std::move(backend)), logger_(logger) {
  if (!backend_.make_port) {
    backend_.make_port = [] { return std::make_unique<QtSerialPort>(); };
  }
  if (!backend_.list_ports) {
    backend_.list_ports = [] { return EnumerateSerialPorts(); };
  }

  // Parented, so it moves to the worker's thread with this object. Started only
  // from Start(), which runs there — a timer may only be started from the
  // thread it lives on.
  timer_ = new QTimer(this);
  timer_->setSingleShot(true);
  connect(timer_, &QTimer::timeout, this, &PlayerWorker::Tick);
}

PlayerWorker::~PlayerWorker() = default;

void PlayerWorker::RequestStop() {
  stopping_.store(true);

  // The port may be waiting out a thirty-second seek. This is the only thing
  // any other thread may touch about it.
  if (port_ != nullptr) {
    port_->RequestAbort();
  }
}

void PlayerWorker::Start(const ddd::gui::PlayerSettings& settings) {
  const bool port_changed = settings.port_path != settings_.port_path ||
                            settings.baud_rate != settings_.baud_rate ||
                            settings.excluded_ports != settings_.excluded_ports;

  settings_ = settings;

  if (!settings_.enabled) {
    Stop();
    return;
  }

  stopping_.store(false);

  if (port_ == nullptr) {
    port_ = backend_.make_port();
    session_ =
        std::make_unique<player::PlayerSession>(port_.get(), backend_.clock);
  }

  // A live link whose port the user has just changed is a link to the wrong
  // port. One whose model selection changed is not — the same player is still
  // on the end of it, and only what to say about it has changed.
  if (session_->connected() && port_changed) {
    session_->Disconnect();
  }

  running_ = true;
  failures_ = 0;

  if (session_->connected()) {
    Publish(DescribeConnection());
    SchedulePoll(std::chrono::milliseconds{0});
    return;
  }

  PlayerConnection searching;
  searching.state = PlayerConnectionState::kSearching;
  Publish(searching);

  // Immediately rather than after the first interval: the user has just
  // switched this on, and the first thing they should see is it looking.
  timer_->start(0);
}

void PlayerWorker::Stop() {
  running_ = false;
  timer_->stop();

  if (session_ != nullptr) {
    session_->Disconnect();
  }

  // Released rather than kept closed. A port this application is holding open
  // is a port nothing else can use, and "player control is off" has to mean the
  // machine is exactly as it would be if the feature did not exist.
  session_.reset();
  port_.reset();

  PlayerConnection disabled;
  disabled.state = PlayerConnectionState::kDisabled;
  Publish(disabled);
}

void PlayerWorker::SearchNow() {
  if (!running_ || session_ == nullptr || session_->connected()) {
    return;
  }

  failures_ = 0;
  timer_->start(0);
}

void PlayerWorker::SetPaused(bool paused) {
  paused_ = paused;

  if (!paused_ && running_ && session_ != nullptr && session_->connected()) {
    SchedulePoll(std::chrono::milliseconds{0});
  }
}

void PlayerWorker::Send(const ddd::gui::PlayerRequest& request) {
  PlayerReply reply;
  reply.request = request;

  if (!running_ || session_ == nullptr || !session_->connected()) {
    reply.status = player::ReplyStatus::kNotConnected;
    emit RequestCompleted(reply);
    return;
  }

  const player::Reply answer = [&]() -> player::Reply {
    switch (request.kind) {
      case PlayerRequest::Kind::kCommand:
        return session_->Execute(request.command, request.argument);
      case PlayerRequest::Kind::kAudio:
        return session_->SetAudio(request.audio);
      case PlayerRequest::Kind::kSpeed:
        return session_->SetSpeed(request.speed);
      case PlayerRequest::Kind::kRaw:
        return session_->SendRaw(request.raw.toStdString(), request.response,
                                 request.timeout);
    }
    return {};
  }();

  reply.status = answer.status;
  reply.text = ToPayload(answer.text);
  reply.error_code = ToQString(answer.error_code);
  reply.sent = ToDisplayBytes(answer.sent);

  // The serial trace. At debug level because it is one line per command and
  // there may be several a second — and it is the thing that makes a
  // misbehaving player diagnosable by somebody who does not have it in front of
  // them, which is why the old application's qDebug() output was the first
  // thing anyone was ever asked for.
  Log(capture::LogLevel::kDebug,
      QStringLiteral("Player: %1").arg(PlayerReplyText(reply)));

  emit RequestCompleted(reply);

  if (answer.status == player::ReplyStatus::kLinkFailed) {
    ReportLinkLost();
    return;
  }

  // The player has just been told to do something, so what it is doing is about
  // to change. Read it now rather than at the end of an interval that was
  // scheduled when nothing was happening.
  if (!paused_) {
    SchedulePoll(std::chrono::milliseconds{0});
  }
}

void PlayerWorker::RequestExamineCancel() { examine_cancelled_.store(true); }

void PlayerWorker::Examine(player::ExamineScope scope) {
  if (examining_) {
    // Two examinations would be two sequences seeking one player. The second is
    // dropped rather than queued: by the time the first finished, whatever the
    // second was asked about would be stale anyway.
    //
    // Dropped silently, and that is safe only because it is unreachable from
    // the interface — there is one examine window, and it will not start a
    // second while one is running. Answering it instead would be worse: the
    // result signal is a broadcast, so a spurious one would tell the window
    // that the examination it *is* waiting for had ended.
    return;
  }

  if (!running_ || session_ == nullptr || !session_->connected()) {
    emit ExamineFinished(player::DiscProfile{},
                         player::ExamineOutcome::kLinkFailed);
    return;
  }

  examining_ = true;
  examine_cancelled_.store(false);

  // The sequence owns the session for its duration. Without this the status
  // poll would interleave a query into the middle of a seek, and a reply
  // attributed to the wrong command is how a seek comes to report the tray
  // state.
  const bool was_paused = paused_;
  paused_ = true;

  player::DiscExaminer examiner(*session_->identity().definition,
                                session_->identity().firmware_version, scope);

  Log(capture::LogLevel::kInfo, QStringLiteral("Examining the disc"));

  while (const std::optional<player::ExamineStep> step = examiner.Next()) {
    if (examine_cancelled_.load() || stopping_.load()) {
      examiner.Cancel();
      break;
    }

    emit ExamineProgress(step->stage,
                         static_cast<int>(examiner.steps_completed()),
                         static_cast<int>(examiner.steps_planned()));

    const player::Reply reply =
        session_->Execute(step->command, step->argument);

    Log(capture::LogLevel::kDebug,
        QStringLiteral("Examine: %1")
            .arg(ExamineStepText(step->stage, ToDisplayBytes(reply.sent),
                                 ToPayload(reply.text))));

    examiner.Apply(reply);
  }

  const player::ExamineOutcome outcome = examiner.outcome();

  Log(capture::LogLevel::kInfo,
      QStringLiteral("Examination %1").arg(ExamineOutcomeText(outcome)));

  examining_ = false;
  paused_ = was_paused;

  emit ExamineFinished(examiner.profile(), outcome);

  if (outcome == player::ExamineOutcome::kLinkFailed) {
    ReportLinkLost();
    return;
  }

  // The disc has been spun up and moved about, so nothing the panel is showing
  // is still true. Read it now rather than at the end of an interval scheduled
  // before any of this happened.
  if (!paused_) {
    SchedulePoll(std::chrono::milliseconds{0});
  }
}

void PlayerWorker::Tick() {
  if (!running_ || stopping_.load() || session_ == nullptr) {
    return;
  }

  if (session_->connected()) {
    if (paused_) {
      // Still armed, so polling resumes on its own when the sequence that
      // owns the session lets go.
      SchedulePoll(kMinimumPollInterval);
      return;
    }
    Poll();
    return;
  }

  AttemptDiscovery();
}

void PlayerWorker::AttemptDiscovery() {
  PlayerConnection searching;
  searching.state = PlayerConnectionState::kSearching;
  Publish(searching);

  const std::vector<DiscoveryAttempt> attempts =
      PlanDiscovery(settings_, backend_.list_ports());

  // Starts at "nothing to report" so that the first real outcome replaces it.
  // Seeding it with kNoPlayerFound instead would outrank every port that could
  // not be opened, and a machine whose only serial port is not permitted would
  // be told nothing answered — sending the user to look at their player rather
  // than at their group membership.
  PlayerConnectionProblem problem = PlayerConnectionProblem::kNone;
  QString detail;
  QString refused_port;

  if (attempts.empty()) {
    // No ports at all, or every one of them excluded. Not a failure of the
    // search — there was nothing to search.
    problem = PlayerConnectionProblem::kPortUnavailable;
  }

  for (const DiscoveryAttempt& attempt : attempts) {
    if (stopping_.load()) {
      return;
    }

    const std::optional<uint32_t> rate =
        attempt.baud_rate == 0 ? std::nullopt
                               : std::optional<uint32_t>(attempt.baud_rate);

    const player::ProbeResult result =
        session_->Probe(attempt.port_path.toStdString(), rate);

    if (result.connected()) {
      failures_ = 0;
      polls_since_disc_status_ = 0;
      last_disc_type_ = player::DiscType::kUnknown;

      const PlayerConnection found = DescribeConnection();
      Log(capture::LogLevel::kInfo,
          QStringLiteral("Player connected: %1 (%2) on %3 at %4 baud")
              .arg(found.model_name, found.model_code, found.port_path)
              .arg(found.baud_rate));

      emit PortRemembered(found.port_path, found.baud_rate);
      Publish(found);
      SchedulePoll(std::chrono::milliseconds{0});
      return;
    }

    const PlayerConnectionProblem found = ProblemFor(result);
    problem = WorseProblem(problem, found);

    if (result.status == player::ProbeResult::Status::kUnusableAnswer &&
        detail.isEmpty()) {
      detail = QStringLiteral("%1 answered with \"%2\"")
                   .arg(attempt.port_path, ToQString(result.unexpected_reply));
    } else if (result.status == player::ProbeResult::Status::kPortUnavailable &&
               detail.isEmpty()) {
      detail = attempt.port_path;
    }

    // Kept separately from `detail`, which is first-come: the port worth naming
    // in a permission message is the one that was refused, and it may well not
    // be the first port that failed to open.
    if (found == PlayerConnectionProblem::kPortNotPermitted &&
        refused_port.isEmpty()) {
      refused_port = attempt.port_path;
    }
  }

  ++failures_;

  PlayerConnection none;
  none.state = PlayerConnectionState::kDisconnected;
  none.problem = problem == PlayerConnectionProblem::kNone
                     ? PlayerConnectionProblem::kNoPlayerFound
                     : problem;
  none.detail = none.problem == PlayerConnectionProblem::kPortNotPermitted
                    ? refused_port
                    : detail;
  Publish(none);

  ScheduleSearch();
}

void PlayerWorker::Poll() {
  const player::PlayerDefinition& definition = *session_->identity().definition;

  QElapsedTimer elapsed;
  elapsed.start();

  player::PlayerStatus status;

  const player::Reply mode =
      session_->Execute(player::PlayerCommand::kQueryActiveMode);
  if (mode.status == player::ReplyStatus::kLinkFailed) {
    ReportLinkLost();
    return;
  }

  if (mode.ok()) {
    status.valid = true;
    status.state = player::ParsePlayerState(mode.text, definition.state_decode);
    status.tray = player::TrayStateFor(status.state);
  }

  // The disc does not change while it is spinning, so this is asked for on
  // connection and then rarely — and never at all while the tray is open,
  // where the answer is already known.
  const bool disc_worth_asking =
      status.state != player::PlayerState::kDoorOpen &&
      (last_disc_type_ == player::DiscType::kUnknown ||
       polls_since_disc_status_ >= kDiscStatusEveryPolls);

  if (disc_worth_asking) {
    const player::Reply disc =
        session_->Execute(player::PlayerCommand::kQueryDiscStatus);
    if (disc.status == player::ReplyStatus::kLinkFailed) {
      ReportLinkLost();
      return;
    }
    if (disc.ok()) {
      last_disc_type_ =
          player::ParseDiscType(disc.text, definition.disc_status);
    }
    polls_since_disc_status_ = 0;
  } else {
    ++polls_since_disc_status_;
  }

  status.disc_type = last_disc_type_;

  // The address is read in the mode the disc implies. A disc whose type is not
  // known yet is read as frames, which is what a CAV disc uses and what the
  // parser will refuse outright if the reply turns out to be a time code — a
  // refusal being much better than a number that is wrong by a factor of a
  // hundred.
  const player::Reply address =
      session_->Execute(player::PlayerCommand::kQueryAddress);
  if (address.status == player::ReplyStatus::kLinkFailed) {
    ReportLinkLost();
    return;
  }
  if (address.ok()) {
    status.address = player::ParseAddress(
        address.text, player::AddressModeFor(status.disc_type));
  }

  if (session_->SupportsPhysicalPosition()) {
    const player::Reply position =
        session_->Execute(player::PlayerCommand::kQueryPhysicalPosition);
    if (position.status == player::ReplyStatus::kLinkFailed) {
      ReportLinkLost();
      return;
    }
    if (position.ok()) {
      status.physical_position_mm =
          player::ParsePhysicalPositionMillimetres(position.text);
    }
  }

  emit StatusUpdated(status);
  SchedulePoll(std::chrono::milliseconds{elapsed.elapsed()});
}

void PlayerWorker::ReportLinkLost() {
  Log(capture::LogLevel::kWarning,
      QStringLiteral("Player link lost on %1").arg(connection_.port_path));

  // The remembered port is deliberately left alone. A cable pulled out is the
  // commonest reason to be here, and the port it was pulled from is exactly the
  // one to try first when it goes back in.
  failures_ = 1;

  PlayerConnection lost;
  lost.state = PlayerConnectionState::kDisconnected;
  lost.problem = PlayerConnectionProblem::kLinkLost;
  Publish(lost);

  ScheduleSearch();
}

void PlayerWorker::Publish(const PlayerConnection& connection) {
  if (connection == connection_) {
    return;
  }

  connection_ = connection;
  emit ConnectionChanged(connection_);
}

void PlayerWorker::ScheduleSearch() {
  if (!running_ || stopping_.load()) {
    return;
  }

  const std::chrono::milliseconds delay = SearchRetryDelay(failures_);
  timer_->start(static_cast<int>(delay.count()));
}

void PlayerWorker::SchedulePoll(std::chrono::milliseconds last_poll) {
  if (!running_ || stopping_.load()) {
    return;
  }

  const std::chrono::milliseconds interval =
      std::clamp(last_poll, kMinimumPollInterval, kMaximumPollInterval);
  timer_->start(static_cast<int>(interval.count()));
}

PlayerConnection PlayerWorker::DescribeConnection() const {
  const player::PlayerIdentity& identity = session_->identity();
  const player::PlayerDefinition& definition = *identity.definition;

  PlayerConnection connection;
  connection.state = PlayerConnectionState::kConnected;
  connection.port_path = QString::fromStdString(session_->port_path());
  connection.baud_rate = session_->baud_rate();
  connection.model_name = ToQString(definition.name);
  connection.model_id_code = ToQString(identity.id_code);
  connection.firmware_version = ToQString(identity.firmware_version);
  connection.model_code = ToQString(identity.model_code);
  connection.recognised_model = identity.recognised;
  connection.bench_verified = definition.bench_verified;

  // Resolved here, once, from the definition and the firmware the player
  // reported — so the remote gates its buttons on a value rather than on a
  // pointer into something this thread owns.
  connection.controls =
      player::ControlsFor(definition, identity.firmware_version);

  // A model the user selected and did not get. The connection is live either
  // way — it is a real player and every control works — but saying so is the
  // difference between a user who knows their setup is not what they think and
  // one who spends an afternoon on it.
  if (!settings_.model_id_code.isEmpty() &&
      settings_.model_id_code != connection.model_id_code) {
    const player::PlayerDefinition* const selected =
        player::FindPlayerByIdCode(settings_.model_id_code.toStdString());

    connection.state = PlayerConnectionState::kModelMismatch;
    connection.selected_model_name = selected != nullptr
                                         ? ToQString(selected->name)
                                         : settings_.model_id_code;
  }

  return connection;
}

void PlayerWorker::Log(capture::LogLevel level, const QString& message) const {
  if (logger_ != nullptr) {
    logger_->Log(level, message.toStdString());
  }
}

}  // namespace ddd::gui
