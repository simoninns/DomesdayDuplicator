/************************************************************************

    test_player_controller.cpp

    T1 tests for the bridge between the GUI and the player link
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSettings>
#include <QString>
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "fake_serial_port.h"
#include "player_controller.h"
#include "player_settings.h"
#include "serial_port_scanner.h"

namespace ddd::gui {
namespace {

using namespace std::chrono_literals;

constexpr const char* kPortPath = "/dev/ttyFAKE0";

// An LD-V4300D — the model on the project's own bench — and an LD-V8000, for
// the cases that need two different players.
constexpr const char* kLdV4300DReply = "P1515A1";
constexpr const char* kLdV8000Reply = "P1506A9";

// Pump the event loop until a condition holds, so a test fails with a message
// rather than hanging. Everything here is asynchronous by design: the worker
// runs on its own thread and reports through queued signals.
template <typename Predicate>
bool PumpUntil(Predicate predicate, std::chrono::milliseconds limit = 5000ms) {
  const auto deadline = std::chrono::steady_clock::now() + limit;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents();
    std::this_thread::sleep_for(1ms);
  }
  return predicate();
}

class PlayerControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-player-%1").arg(QLatin1String(info->name())));
    QSettings().clear();

    // One port, and a fake clock, so a search that finds nothing takes
    // microseconds rather than the three seconds four baud rates and three
    // attempts really cost.
    SerialPortCandidate candidate;
    candidate.path = QLatin1String(kPortPath);
    candidate.usb_adapter = true;
    ports_.push_back(candidate);

    // The fake is one port with one path, so pointing the application at any
    // other fails to open — which is what a port that is not there does.
    port_.set_only_path(kPortPath);
  }

  void TearDown() override {
    controller_.reset();
    QSettings().clear();
  }

  // Build the controller over the test's fake port. Deferred so a test can
  // script the player before anything starts looking for it.
  void BuildController() {
    PlayerBackend backend;
    backend.make_port = [this] {
      return std::make_unique<player::BorrowedSerialPort>(&port_);
    };
    backend.list_ports = [this] { return ports_; };
    backend.clock = port_.clock();

    controller_ = std::make_unique<PlayerController>(std::move(backend));
  }

  // Turn player control on and wait for whatever it settles into.
  void Enable() {
    controller_->Start();
    controller_->SetEnabled(true);
  }

  bool WaitForState(PlayerConnectionState state) {
    return PumpUntil(
        [this, state] { return controller_->connection().state == state; });
  }

  player::FakeSerialPort port_;
  std::vector<SerialPortCandidate> ports_;
  std::unique_ptr<PlayerController> controller_;
};

TEST_F(PlayerControllerTest, NothingIsTouchedUntilPlayerControlIsTurnedOn) {
  // The default that matters: no port opened, nothing written, no enumeration.
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  BuildController();
  controller_->Start();

  // Given time to do the wrong thing, and asserted afterwards rather than
  // immediately — an application that opened the port a second later would
  // pass an instant check.
  PumpUntil([] { return false; }, 200ms);

  EXPECT_EQ(controller_->connection().state, PlayerConnectionState::kDisabled);
  EXPECT_EQ(port_.open_count(), 0);
  EXPECT_TRUE(port_.writes().empty());
}

TEST_F(PlayerControllerTest, APlayerIsFoundAndIdentifiedWithNoConfiguration) {
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));

  const PlayerConnection& connection = controller_->connection();
  EXPECT_EQ(connection.model_name, QStringLiteral("Pioneer LD-V4300D"));
  EXPECT_EQ(connection.model_id_code, QStringLiteral("15"));
  EXPECT_EQ(connection.firmware_version, QStringLiteral("A1"));
  EXPECT_EQ(connection.port_path, QLatin1String(kPortPath));
  EXPECT_EQ(connection.baud_rate, 9600U);
  EXPECT_TRUE(connection.recognised_model);
  EXPECT_TRUE(controller_->connected());
}

TEST_F(PlayerControllerTest, ThePortThatWorkedIsRememberedForNextTime) {
  // The whole point of remembering: the second run costs one probe rather than
  // a scan of every port at every rate.
  port_.AddPioneerPlayer(1200, kLdV4300DReply);
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));
  ASSERT_TRUE(PumpUntil(
      [this] { return !controller_->settings().remembered_port.isEmpty(); }));

  EXPECT_EQ(controller_->settings().remembered_port, QLatin1String(kPortPath));
  EXPECT_EQ(controller_->settings().remembered_baud, 1200U);

  // Written through to the settings file, not just held in memory.
  EXPECT_EQ(LoadPlayerSettings().remembered_baud, 1200U);
}

TEST_F(PlayerControllerTest, AnEmptyPortReportsThatNothingAnswered) {
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kDisconnected));
  EXPECT_EQ(controller_->connection().problem,
            PlayerConnectionProblem::kNoPlayerFound);
}

TEST_F(PlayerControllerTest, APortThatWillNotOpenIsReportedAsSuch) {
  // Not the same as "no player": on Linux this is usually a permission
  // problem, and it is the most likely first-run experience there is.
  port_.set_open_fails(true);
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kDisconnected));
  EXPECT_EQ(controller_->connection().problem,
            PlayerConnectionProblem::kPortUnavailable);
}

TEST_F(PlayerControllerTest, SomethingThatIsNotAPlayerIsNamedAsSuch) {
  port_.AddResponse(9600, "?X\r", "GARBAGE\r");
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kDisconnected));
  EXPECT_EQ(controller_->connection().problem,
            PlayerConnectionProblem::kNotAPlayer);
  EXPECT_TRUE(
      controller_->connection().detail.contains(QStringLiteral("GARBAGE")));
}

TEST_F(PlayerControllerTest, ThereIsNothingToSearchWhenEveryPortIsExcluded) {
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  BuildController();

  PlayerSettings settings = controller_->settings();
  settings.enabled = true;
  settings.excluded_ports = QStringList{QLatin1String(kPortPath)};
  controller_->SetSettings(settings);

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kDisconnected));

  // The port on the exclusion list is never opened, even though there is a
  // player on it. That is the promise the exclusion list makes.
  EXPECT_EQ(port_.open_count(), 0);
}

TEST_F(PlayerControllerTest, TheWrongModelIsALiveConnectionThatSaysSo) {
  port_.AddPioneerPlayer(9600, kLdV8000Reply);
  BuildController();

  PlayerSettings settings = controller_->settings();
  settings.enabled = true;
  settings.model_id_code = QStringLiteral("15");  // LD-V4300D
  controller_->SetSettings(settings);

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kModelMismatch));

  const PlayerConnection& connection = controller_->connection();
  EXPECT_EQ(connection.model_name, QStringLiteral("Pioneer LD-V8000"));
  EXPECT_EQ(connection.selected_model_name,
            QStringLiteral("Pioneer LD-V4300D"));

  // Live, not broken: it is a real player and everything works.
  EXPECT_TRUE(controller_->connected());
}

TEST_F(PlayerControllerTest, AMismatchCanBeResolvedByAcceptingWhatAnswered) {
  port_.AddPioneerPlayer(9600, kLdV8000Reply);
  BuildController();

  PlayerSettings settings = controller_->settings();
  settings.enabled = true;
  settings.model_id_code = QStringLiteral("15");
  controller_->SetSettings(settings);

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kModelMismatch));

  controller_->UseConnectedModel();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));
  EXPECT_EQ(controller_->settings().model_id_code, QStringLiteral("06"));
}

TEST_F(PlayerControllerTest, TheStatusIsPolledAndReadInTheDiscsOwnTerms) {
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  port_.AddStatusResponses(9600, "P04", "10011", "0012345");
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));
  ASSERT_TRUE(PumpUntil([this] { return controller_->status().valid; }));

  const player::PlayerStatus& status = controller_->status();
  EXPECT_EQ(status.state, player::PlayerState::kPlaying);
  EXPECT_EQ(status.tray, player::TrayState::kClosed);
  EXPECT_EQ(status.disc_type, player::DiscType::kCav);
  EXPECT_TRUE(status.address.valid);
  EXPECT_EQ(status.address.value, 12345);
}

TEST_F(PlayerControllerTest, ACavAddressAndAClvAddressAreBothRead) {
  // A CLV time code is seven digits, and reading it as a frame number would be
  // wrong by orders of magnitude — so the mode has to follow the disc.
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  port_.AddStatusResponses(9600, "P04", "11011", "1234500");
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));
  ASSERT_TRUE(PumpUntil([this] {
    return controller_->status().disc_type == player::DiscType::kClv &&
           controller_->status().address.valid;
  }));

  EXPECT_EQ(controller_->status().address.value, 1234500);
}

TEST_F(PlayerControllerTest, ALinkThatDiesIsReportedAndSearchedForAgain) {
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  port_.AddStatusResponses(9600, "P04", "10011", "0012345");
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));
  ASSERT_TRUE(PumpUntil([this] { return controller_->status().valid; }));

  // The cable comes out.
  port_.set_link_broken(true);

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kDisconnected));
  EXPECT_EQ(controller_->connection().problem,
            PlayerConnectionProblem::kLinkLost);
  EXPECT_FALSE(controller_->connected());

  // The remembered port is deliberately left alone: a cable pulled out is the
  // commonest reason to be here, and that port is exactly the one to try first
  // when it goes back in.
  EXPECT_EQ(controller_->settings().remembered_port, QLatin1String(kPortPath));
}

TEST_F(PlayerControllerTest, TurningPlayerControlOffReleasesThePort) {
  // "Off" has to mean the machine is exactly as it would be if the feature did
  // not exist — a port held open is a port nothing else can use.
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));

  controller_->SetEnabled(false);

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kDisabled));
  EXPECT_TRUE(PumpUntil([this] { return !port_.IsOpen(); }));
}

TEST_F(PlayerControllerTest, ChangingTheFixedPortDropsTheLinkToTheOldOne) {
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));

  PlayerSettings settings = controller_->settings();
  settings.port_path = QStringLiteral("/dev/ttyNOWHERE");
  controller_->SetSettings(settings);

  // A live link to a port the user has just said is the wrong one is a link to
  // the wrong port.
  ASSERT_TRUE(WaitForState(PlayerConnectionState::kDisconnected));
}

TEST_F(PlayerControllerTest, ACommandGoesOutAndItsAnswerComesBack) {
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  port_.AddResponse(9600, "FR100SE\r", "R\r");
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));

  std::vector<PlayerReply> replies;
  QObject::connect(
      controller_.get(), &PlayerController::RequestCompleted, controller_.get(),
      [&replies](const PlayerReply& reply) { replies.push_back(reply); });

  const uint64_t id =
      controller_->Send(CommandRequest(player::PlayerCommand::kSeekFrame, 100));
  EXPECT_NE(id, 0U);

  ASSERT_TRUE(PumpUntil([&replies] { return !replies.empty(); }));

  // The request comes back with its answer, which is what lets a caller with
  // more than one thing outstanding tell the answers apart.
  EXPECT_EQ(replies.front().request.id, id);
  EXPECT_EQ(replies.front().status, player::ReplyStatus::kOk);
  EXPECT_EQ(replies.front().sent, QStringLiteral("FR100SE"));

  const std::vector<std::string> writes = port_.writes();
  EXPECT_NE(std::find(writes.begin(), writes.end(), "FR100SE\r"), writes.end());
}

TEST_F(PlayerControllerTest, ARawCommandIsSentExactlyAsTypedAndAnsweredAsText) {
  // The manual command field. Read as text rather than as an acknowledgement,
  // so a refusal arrives as the bytes the player sent rather than as this
  // library's opinion of them — which is the entire point of the field.
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  port_.AddResponse(9600, "?U\r", "E04\r");
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));

  std::vector<PlayerReply> answers;
  QObject::connect(controller_.get(), &PlayerController::RequestCompleted,
                   controller_.get(), [&answers](const PlayerReply& reply) {
                     if (reply.request.kind == PlayerRequest::Kind::kRaw) {
                       answers.push_back(reply);
                     }
                   });

  controller_->Send(RawRequest(QStringLiteral("?U")));

  ASSERT_TRUE(PumpUntil([&answers] { return !answers.empty(); }));
  EXPECT_EQ(answers.front().status, player::ReplyStatus::kOk);
  EXPECT_EQ(answers.front().text, QStringLiteral("E04"));
}

TEST_F(PlayerControllerTest, ACommandWithNoPlayerIsAnsweredRatherThanDropped) {
  // A reply for every request, in every state. A control that pressed and got
  // nothing back at all could not tell "waiting" from "ignored".
  BuildController();
  controller_->Start();

  std::vector<PlayerReply> answers;
  QObject::connect(
      controller_.get(), &PlayerController::RequestCompleted, controller_.get(),
      [&answers](const PlayerReply& reply) { answers.push_back(reply); });

  controller_->Send(CommandRequest(player::PlayerCommand::kPlay));

  ASSERT_TRUE(PumpUntil([&answers] { return !answers.empty(); }));
  EXPECT_EQ(answers.front().status, player::ReplyStatus::kNotConnected);
  EXPECT_EQ(port_.open_count(), 0);
}

TEST_F(PlayerControllerTest, WhatAPlayerCanDoArrivesWithTheConnection) {
  // The remote gates its buttons on this, and it is resolved on the worker's
  // thread from the definition and the firmware the player reported — so that
  // the interface never has to reach into the protocol to ask.
  port_.AddPioneerPlayer(9600, kLdV8000Reply);  // firmware A9
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));

  EXPECT_TRUE(controller_->controls().Has(player::PlayerCommand::kPlay));
  EXPECT_TRUE(controller_->controls().Has(
      player::PlayerCommand::kQueryPhysicalPosition));

  // And goes away with it, so nothing stays clickable for a player that is no
  // longer there.
  controller_->SetEnabled(false);
  ASSERT_TRUE(WaitForState(PlayerConnectionState::kDisabled));
  EXPECT_FALSE(controller_->controls().any());
}

// --- Examining the disc ----------------------------------------------------

// A CAV disc of 54,000 frames, scripted end to end. The two "?F" answers are
// the two halves of the length measurement: where the player stopped when it
// was sent past the end, and where it stopped when it was sent back to the
// start.
void ScriptCavDisc(player::FakeSerialPort& port) {
  port.AddResponse(9600, "?P\r", "P01\r");
  port.AddResponse(9600, "?D\r", "10001\r");
  port.AddResponse(9600, "PL\r", "R\r");
  port.AddResponse(9600, "$Y\r", "Y1000\r");
  port.AddResponse(9600, "?U\r", "E04\r");
  port.AddResponse(9600, "?S\r", "220\r");
  port.AddResponse(9600, "FR60000SE\r", "E04\r");
  port.AddResponse(9600, "FR1SE\r", "R\r");
  port.AddResponse(9600, "PA\r", "R\r");
  port.AddResponseSequence(9600, "?F\r", {"054000\r", "<00001\r"});
}

TEST_F(PlayerControllerTest, AnExaminationDrivesTheWholeSequenceAndReportsIt) {
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  ScriptCavDisc(port_);
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));

  std::vector<player::ExamineStage> stages;
  std::vector<player::DiscProfile> results;
  std::vector<player::ExamineOutcome> outcomes;

  QObject::connect(controller_.get(), &PlayerController::ExamineProgress,
                   controller_.get(),
                   [&stages](player::ExamineStage stage, int, int) {
                     stages.push_back(stage);
                   });
  QObject::connect(controller_.get(), &PlayerController::ExamineFinished,
                   controller_.get(),
                   [&results, &outcomes](const player::DiscProfile& disc,
                                         player::ExamineOutcome outcome) {
                     results.push_back(disc);
                     outcomes.push_back(outcome);
                   });

  controller_->Examine();

  ASSERT_TRUE(PumpUntil([&results] { return !results.empty(); }));

  EXPECT_EQ(outcomes.front(), player::ExamineOutcome::kCompleted);

  const player::DiscProfile& disc = results.front();
  EXPECT_EQ(disc.disc_type.value, player::DiscType::kCav);
  EXPECT_EQ(disc.programme_end.value, 54000);
  EXPECT_EQ(disc.programme_end.provenance, player::Provenance::kMeasured);
  EXPECT_TRUE(disc.lead_in_reachable.value);
  EXPECT_EQ(disc.disc_status_reply, "10001");

  // Read off the disc rather than asked of the user, which is the whole of
  // what "220" bought.
  EXPECT_EQ(disc.video_standard.value, player::VideoStandard::kPal);
  EXPECT_EQ(disc.video_standard.provenance, player::Provenance::kReported);

  // Every step said what it was for, so the progress line had something to
  // show throughout.
  EXPECT_EQ(stages.size(), size_t{11});
  EXPECT_EQ(stages.front(), player::ExamineStage::kCheckingPlayer);

  // And the bytes really went out, in the old application's own form.
  const std::vector<std::string> writes = port_.writes();
  EXPECT_NE(std::find(writes.begin(), writes.end(), "FR60000SE\r"),
            writes.end());
  EXPECT_NE(std::find(writes.begin(), writes.end(), "PA\r"), writes.end());
}

TEST_F(PlayerControllerTest, BothUserCodesAreReadEveryTime) {
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  ScriptCavDisc(port_);
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));

  std::vector<player::DiscProfile> results;
  QObject::connect(
      controller_.get(), &PlayerController::ExamineFinished, controller_.get(),
      [&results](const player::DiscProfile& disc, player::ExamineOutcome) {
        results.push_back(disc);
      });

  controller_->Examine();
  ASSERT_TRUE(PumpUntil([&results] { return !results.empty(); }));

  const std::vector<std::string> writes = port_.writes();
  EXPECT_NE(std::find(writes.begin(), writes.end(), "?U\r"), writes.end());
  EXPECT_NE(std::find(writes.begin(), writes.end(), "$Y\r"), writes.end());

  // This disc carries no Pioneer code, which is a finding about the disc and
  // not a failure of the examination.
  EXPECT_EQ(results.front().pioneer_user_code.outcome,
            player::UserCodeReading::Outcome::kNotEncoded);
  EXPECT_TRUE(results.front().standard_user_code.read());
}

TEST_F(PlayerControllerTest, TheDiscsOwnProgrammeStatusReachesTheProfile) {
  // Size, side and chapters, from one reply that costs nothing and moves
  // nothing. The side is the field a capture most needs and the one the old
  // application had no way of knowing at all.
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  ScriptCavDisc(port_);
  port_.AddResponse(9600, "?D\r", "11011\r");
  port_.AddResponseSequence(9600, "?F\r", {"0504500\r", "<0000000\r"});
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));

  std::vector<player::DiscProfile> results;
  QObject::connect(
      controller_.get(), &PlayerController::ExamineFinished, controller_.get(),
      [&results](const player::DiscProfile& disc, player::ExamineOutcome) {
        results.push_back(disc);
      });

  controller_->Examine();
  ASSERT_TRUE(PumpUntil([&results] { return !results.empty(); }));

  const player::DiscProfile& disc = results.front();
  EXPECT_EQ(disc.disc_type.value, player::DiscType::kClv);
  EXPECT_EQ(disc.disc_size.value, player::DiscSize::k30cm);
  EXPECT_EQ(disc.disc_side.value, 2);
  EXPECT_TRUE(disc.chapters.value);
  EXPECT_EQ(disc.chapters.provenance, player::Provenance::kReported);

  // And nothing was sent to find out about chapters, because the disc had
  // already said.
  const std::vector<std::string> writes = port_.writes();
  EXPECT_EQ(std::find(writes.begin(), writes.end(), "CH1SE\r"), writes.end());
}

TEST_F(PlayerControllerTest,
       AnExaminationWithNoPlayerIsAnsweredRatherThanLost) {
  // A result for every examination, in every state. A dialog waiting on this
  // signal has to be told even when there was nothing to examine.
  BuildController();
  controller_->Start();

  std::vector<player::ExamineOutcome> outcomes;
  QObject::connect(
      controller_.get(), &PlayerController::ExamineFinished, controller_.get(),
      [&outcomes](const player::DiscProfile&, player::ExamineOutcome outcome) {
        outcomes.push_back(outcome);
      });

  controller_->Examine();

  ASSERT_TRUE(PumpUntil([&outcomes] { return !outcomes.empty(); }));
  EXPECT_EQ(outcomes.front(), player::ExamineOutcome::kLinkFailed);
}

TEST_F(PlayerControllerTest, AnOpenTrayEndsTheExaminationAfterOneQuestion) {
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  port_.AddResponse(9600, "?P\r", "P00\r");
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));

  std::vector<player::ExamineOutcome> outcomes;
  QObject::connect(
      controller_.get(), &PlayerController::ExamineFinished, controller_.get(),
      [&outcomes](const player::DiscProfile&, player::ExamineOutcome outcome) {
        outcomes.push_back(outcome);
      });

  controller_->Examine();

  ASSERT_TRUE(PumpUntil([&outcomes] { return !outcomes.empty(); }));
  EXPECT_EQ(outcomes.front(), player::ExamineOutcome::kTrayOpen);

  // No play command, so nothing spun up a tray that is open.
  const std::vector<std::string> writes = port_.writes();
  EXPECT_EQ(std::find(writes.begin(), writes.end(), "PL\r"), writes.end());
}

TEST_F(PlayerControllerTest, TheStatusPollDoesNotInterleaveWithAnExamination) {
  // The reason the sequence pauses polling: a status query landing between a
  // seek and its answer is how a reply gets attributed to the wrong command.
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  ScriptCavDisc(port_);
  BuildController();
  Enable();

  ASSERT_TRUE(WaitForState(PlayerConnectionState::kConnected));

  std::vector<player::ExamineStage> stages;
  QObject::connect(controller_.get(), &PlayerController::ExamineProgress,
                   controller_.get(),
                   [&stages](player::ExamineStage stage, int, int) {
                     stages.push_back(stage);
                   });

  bool finished = false;
  QObject::connect(controller_.get(), &PlayerController::ExamineFinished,
                   controller_.get(),
                   [&finished](const player::DiscProfile&,
                               player::ExamineOutcome) { finished = true; });

  const size_t before = port_.writes().size();
  controller_->Examine();
  ASSERT_TRUE(PumpUntil([&finished] { return finished; }));

  const std::vector<std::string> writes = port_.writes();
  const std::vector<std::string> during(writes.begin() + before, writes.end());

  // Exactly the ten commands the sequence sends, and nothing else. A poll that
  // had slipped in would show up here as an extra "?P" or "?D".
  EXPECT_EQ(during.size(), stages.size());
}

TEST_F(PlayerControllerTest, EverythingReturnsImmediatelyInEveryState) {
  // The property the whole design rests on: no method here may wait for a
  // player. A search of every port at every rate takes seconds, and a window
  // that stopped repainting for that long would be one a user force-quits.
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  BuildController();

  const auto start = std::chrono::steady_clock::now();

  controller_->Start();
  controller_->SetEnabled(true);
  controller_->SearchNow();
  controller_->SetPaused(true);
  controller_->SetPaused(false);
  controller_->UseConnectedModel();
  controller_->Send(CommandRequest(player::PlayerCommand::kSeekFrame, 40000));
  controller_->Examine();
  controller_->CancelExamine();
  controller_->SetEnabled(false);

  const auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_LT(elapsed, 100ms);
}

}  // namespace
}  // namespace ddd::gui
