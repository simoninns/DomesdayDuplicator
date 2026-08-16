/************************************************************************

    test_player_panel.cpp

    T1 tests for the player panel
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "fake_serial_port.h"
#include "player_controller.h"
#include "player_panel.h"
#include "player_text.h"
#include "serial_port_scanner.h"

namespace ddd::gui {
namespace {

using namespace std::chrono_literals;

constexpr const char* kPortPath = "/dev/ttyFAKE0";
constexpr const char* kLdV4300DReply = "P1515A1";

template <typename Predicate>
bool PumpUntil(Predicate predicate, std::chrono::milliseconds limit = 5000ms) {
  const auto deadline = std::chrono::steady_clock::now() + limit;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    QApplication::processEvents();
    std::this_thread::sleep_for(1ms);
  }
  return predicate();
}

class PlayerPanelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-player-panel-%1")
            .arg(QLatin1String(info->name())));
    QSettings().clear();

    SerialPortCandidate candidate;
    candidate.path = QLatin1String(kPortPath);
    candidate.usb_adapter = true;
    ports_.push_back(candidate);
  }

  void TearDown() override {
    panel_.reset();
    controller_.reset();
    QSettings().clear();
  }

  void BuildWithController() {
    PlayerBackend backend;
    backend.make_port = [this] {
      return std::make_unique<player::BorrowedSerialPort>(&port_);
    };
    backend.list_ports = [this] { return ports_; };
    backend.clock = port_.clock();

    controller_ = std::make_unique<PlayerController>(std::move(backend));
    panel_ = std::make_unique<PlayerPanel>(controller_.get());
  }

  template <typename T>
  T* Find(const char* name) const {
    return panel_->findChild<T*>(QLatin1String(name));
  }

  QString TextOf(const char* name) const { return Find<QLabel>(name)->text(); }

  player::FakeSerialPort port_;
  std::vector<SerialPortCandidate> ports_;
  std::unique_ptr<PlayerController> controller_;
  std::unique_ptr<PlayerPanel> panel_;
};

TEST_F(PlayerPanelTest, ItBuildsAndSaysWhatItIsDoingWithNoControllerAtAll) {
  // The property every panel in this application has: it lays out exactly as
  // it does in the real window and drives nothing, so the interface stays
  // testable on a machine with no player and no serial adapter.
  PlayerPanel panel(nullptr);

  auto* summary =
      panel.findChild<QLabel*>(QLatin1String(PlayerPanel::kSummaryLabelName));
  ASSERT_NE(summary, nullptr);
  EXPECT_EQ(summary->text(), PlayerConnectionSummary(PlayerConnection{}));

  // Present but inert, rather than absent — the panel has the same shape in
  // every build of the window.
  auto* enabled = panel.findChild<QCheckBox*>(
      QLatin1String(PlayerPanel::kEnabledCheckName));
  ASSERT_NE(enabled, nullptr);
  EXPECT_FALSE(enabled->isEnabled());

  auto* search = panel.findChild<QPushButton*>(
      QLatin1String(PlayerPanel::kSearchButtonName));
  ASSERT_NE(search, nullptr);
  EXPECT_FALSE(search->isEnabled());
}

TEST_F(PlayerPanelTest, EveryReadingIsBlankUntilThereIsAPlayer) {
  // A stale position left on screen beside "no player" reads as a live one.
  BuildWithController();

  EXPECT_EQ(TextOf(PlayerPanel::kStateLabelName), QStringLiteral("—"));
  EXPECT_EQ(TextOf(PlayerPanel::kAddressLabelName), QStringLiteral("—"));
  EXPECT_FALSE(Find<QLabel>(PlayerPanel::kSourceLabelName)->isVisible());
}

TEST_F(PlayerPanelTest, TheCheckboxIsThePlayerControlSetting) {
  BuildWithController();
  controller_->Start();

  auto* enabled = Find<QCheckBox>(PlayerPanel::kEnabledCheckName);
  ASSERT_NE(enabled, nullptr);
  EXPECT_FALSE(enabled->isChecked());

  enabled->setChecked(true);
  EXPECT_TRUE(controller_->settings().enabled);

  // And a change made anywhere else is reflected here, rather than leaving the
  // box and the application disagreeing.
  controller_->SetEnabled(false);
  EXPECT_FALSE(enabled->isChecked());
}

TEST_F(PlayerPanelTest, AConnectedPlayerIsNamedWithHowItWasReached) {
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  port_.AddStatusResponses(9600, "P04", "10011", "0012345");
  BuildWithController();
  controller_->Start();
  controller_->SetEnabled(true);

  ASSERT_TRUE(PumpUntil([this] { return controller_->connected(); }));
  ASSERT_TRUE(PumpUntil([this] { return controller_->status().valid; }));

  EXPECT_EQ(TextOf(PlayerPanel::kSummaryLabelName),
            QStringLiteral("Pioneer LD-V4300D"));
  EXPECT_TRUE(
      TextOf(PlayerPanel::kSourceLabelName).contains(QLatin1String(kPortPath)));
  EXPECT_TRUE(
      TextOf(PlayerPanel::kSourceLabelName).contains(QStringLiteral("9600")));

  EXPECT_EQ(TextOf(PlayerPanel::kStateLabelName), QStringLiteral("Playing"));
  EXPECT_EQ(TextOf(PlayerPanel::kDiscLabelName), QStringLiteral("CAV"));
  EXPECT_EQ(TextOf(PlayerPanel::kAddressLabelName),
            QStringLiteral("Frame 12345"));

  // The row for a reading this model cannot produce stays hidden rather than
  // showing a blank forever.
  EXPECT_FALSE(Find<QLabel>(PlayerPanel::kPositionLabelName)->isVisible());
}

TEST_F(PlayerPanelTest, AnUnverifiedDefinitionIsSaidSoOnScreen) {
  // The interface's half of players/README.md's promise.
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  BuildWithController();
  controller_->Start();
  controller_->SetEnabled(true);

  ASSERT_TRUE(PumpUntil([this] { return controller_->connected(); }));

  auto* note = Find<QLabel>(PlayerPanel::kVerificationLabelName);
  ASSERT_NE(note, nullptr);
  EXPECT_FALSE(note->text().isEmpty());
}

TEST_F(PlayerPanelTest, TheWrongModelOffersToBeAccepted) {
  port_.AddPioneerPlayer(9600, "P1506A9");  // an LD-V8000
  BuildWithController();

  PlayerSettings settings = controller_->settings();
  settings.enabled = true;
  settings.model_id_code = QStringLiteral("15");  // LD-V4300D
  controller_->SetSettings(settings);

  ASSERT_TRUE(PumpUntil([this] {
    return controller_->connection().state ==
           PlayerConnectionState::kModelMismatch;
  }));

  auto* use_model = Find<QPushButton>(PlayerPanel::kUseModelButtonName);
  ASSERT_NE(use_model, nullptr);
  EXPECT_TRUE(use_model->isVisibleTo(panel_.get()));

  use_model->click();

  ASSERT_TRUE(PumpUntil([this] {
    return controller_->connection().state == PlayerConnectionState::kConnected;
  }));
  EXPECT_FALSE(use_model->isVisibleTo(panel_.get()));
}

TEST_F(PlayerPanelTest, SearchingNowIsOfferedOnlyWhenThereIsNothingToTalkTo) {
  BuildWithController();
  controller_->Start();
  controller_->SetEnabled(true);

  auto* search = Find<QPushButton>(PlayerPanel::kSearchButtonName);
  ASSERT_NE(search, nullptr);

  ASSERT_TRUE(PumpUntil([this] {
    return controller_->connection().state ==
           PlayerConnectionState::kDisconnected;
  }));
  EXPECT_TRUE(search->isEnabled());

  // Nothing to search for once there is a player.
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  search->click();

  ASSERT_TRUE(PumpUntil([this] { return controller_->connected(); }));
  EXPECT_FALSE(search->isEnabled());
}

TEST_F(PlayerPanelTest, LosingThePlayerBlanksTheReadings) {
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  port_.AddStatusResponses(9600, "P04", "10011", "0012345");
  BuildWithController();
  controller_->Start();
  controller_->SetEnabled(true);

  ASSERT_TRUE(PumpUntil([this] { return controller_->status().valid; }));
  ASSERT_EQ(TextOf(PlayerPanel::kStateLabelName), QStringLiteral("Playing"));

  port_.set_link_broken(true);

  ASSERT_TRUE(PumpUntil([this] { return !controller_->connected(); }));
  EXPECT_EQ(TextOf(PlayerPanel::kStateLabelName), QStringLiteral("—"));
  EXPECT_EQ(TextOf(PlayerPanel::kAddressLabelName), QStringLiteral("—"));
}

}  // namespace
}  // namespace ddd::gui
