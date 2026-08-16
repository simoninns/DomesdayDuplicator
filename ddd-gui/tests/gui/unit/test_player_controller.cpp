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
#include <chrono>
#include <memory>
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
  controller_->SetEnabled(false);

  const auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_LT(elapsed, 100ms);
}

}  // namespace
}  // namespace ddd::gui
