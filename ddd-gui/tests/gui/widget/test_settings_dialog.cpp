/************************************************************************

    test_settings_dialog.cpp

    T1 tests for the settings dialog and its tabs
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <vector>

#include "player_settings.h"
#include "serial_port_scanner.h"
#include "settings_dialog.h"

namespace ddd::gui {
namespace {

SerialPortCandidate Port(const char* path) {
  SerialPortCandidate candidate;
  candidate.path = QLatin1String(path);
  candidate.usb_adapter = true;
  return candidate;
}

std::vector<SerialPortCandidate> TwoPorts() {
  return {Port("/dev/ttyUSB0"), Port("/dev/ttyUSB1")};
}

template <typename T>
T* Find(const SettingsDialog& dialog, const char* name) {
  return dialog.findChild<T*>(QLatin1String(name));
}

TEST(SettingsDialogTest, TheSettingsAreGroupedByWhatTheyAreAbout) {
  // The point of the tabs: these are settings about two different pieces of
  // equipment, and a single form would put "which serial port" directly beneath
  // "USB transfer size" under one OK.
  const SettingsDialog dialog(CaptureSettings{}, {}, PlayerSettings{},
                              TwoPorts());

  auto* tabs = Find<QTabWidget>(dialog, SettingsDialog::kTabsName);
  ASSERT_NE(tabs, nullptr);
  ASSERT_EQ(tabs->count(), 2);
  EXPECT_TRUE(tabs->tabText(0).contains(QStringLiteral("Capture")));
  EXPECT_TRUE(tabs->tabText(1).contains(QStringLiteral("Player")));
}

TEST(SettingsDialogTest, ItOpensOnTheTabTheMenuEntryWasAbout) {
  // Reaching the player's settings from the Player menu must not be a hunt
  // through a dialog named after something else.
  const SettingsDialog capture_first(CaptureSettings{}, {}, PlayerSettings{},
                                     TwoPorts());
  EXPECT_EQ(Find<QTabWidget>(capture_first, SettingsDialog::kTabsName)
                ->currentIndex(),
            0);

  const SettingsDialog player_first(CaptureSettings{}, {}, PlayerSettings{},
                                    TwoPorts(), SettingsDialog::Tab::kPlayer);
  EXPECT_EQ(
      Find<QTabWidget>(player_first, SettingsDialog::kTabsName)->currentIndex(),
      1);
}

TEST(SettingsDialogTest, EachHalfComesBackAsItWentIn) {
  CaptureSettings capture;
  capture.queue_size_bytes = size_t{512} << 20;
  capture.small_transfers = false;

  PlayerSettings player;
  player.enabled = true;
  player.model_id_code = QStringLiteral("15");
  player.port_path = QStringLiteral("/dev/ttyUSB1");
  player.baud_rate = 4800;

  const SettingsDialog dialog(capture, {}, player, TwoPorts());

  EXPECT_EQ(dialog.Settings().queue_size_bytes, capture.queue_size_bytes);
  EXPECT_FALSE(dialog.Settings().small_transfers);

  EXPECT_TRUE(dialog.Player().enabled);
  EXPECT_EQ(dialog.Player().model_id_code, QStringLiteral("15"));
  EXPECT_EQ(dialog.Player().port_path, QStringLiteral("/dev/ttyUSB1"));
  EXPECT_EQ(dialog.Player().baud_rate, 4800U);
}

TEST(SettingsDialogTest, TheCaptureHalfIsUntouchedByThePlayerHalf) {
  // Two controllers apply these, so a dialog that let one page overwrite the
  // other's values would be a way to lose a setting by opening a window.
  CaptureSettings capture;
  capture.front_end_gain_switches = 0;
  capture.capture_name = QStringLiteral("kept");

  SettingsDialog dialog(capture, {}, PlayerSettings{}, TwoPorts());
  Find<QCheckBox>(dialog, SettingsDialog::kPlayerEnabledCheckName)
      ->setChecked(true);

  EXPECT_EQ(dialog.Settings().capture_name, QStringLiteral("kept"));
  EXPECT_TRUE(dialog.Player().enabled);
}

TEST(SettingsDialogTest, AChosenPortThatIsNotThereStaysChosen) {
  // Silently resetting it because the adapter is unplugged would change the
  // user's configuration behind their back, and they would find out by watching
  // the application probe every other port on the machine.
  PlayerSettings player;
  player.port_path = QStringLiteral("/dev/ttyGONE");

  const SettingsDialog dialog(CaptureSettings{}, {}, player, TwoPorts());

  EXPECT_EQ(dialog.Player().port_path, QStringLiteral("/dev/ttyGONE"));
}

TEST(SettingsDialogTest, AnExclusionSurvivesTheAdapterBeingUnplugged) {
  PlayerSettings player;
  player.excluded_ports = QStringList{QStringLiteral("/dev/ttyGONE")};

  const SettingsDialog dialog(CaptureSettings{}, {}, player, TwoPorts());

  auto* excluded =
      Find<QListWidget>(dialog, SettingsDialog::kPlayerExcludedListName);
  ASSERT_NE(excluded, nullptr);

  // Both the ports that exist and the one that does not, so the exclusion can
  // be seen and kept.
  EXPECT_EQ(excluded->count(), 3);
  EXPECT_EQ(dialog.Player().excluded_ports,
            QStringList{QStringLiteral("/dev/ttyGONE")});
}

TEST(SettingsDialogTest, ExcludingTheRememberedPortForgetsIt) {
  // Otherwise it would be tried first and rejected on every search from now on.
  PlayerSettings player;
  player.remembered_port = QStringLiteral("/dev/ttyUSB0");
  player.remembered_baud = 9600;

  SettingsDialog dialog(CaptureSettings{}, {}, player, TwoPorts());

  auto* excluded =
      Find<QListWidget>(dialog, SettingsDialog::kPlayerExcludedListName);
  ASSERT_NE(excluded, nullptr);
  excluded->item(0)->setCheckState(Qt::Checked);

  EXPECT_TRUE(dialog.Player().remembered_port.isEmpty());
  EXPECT_EQ(dialog.Player().remembered_baud, 0U);
}

TEST(SettingsDialogTest, FixingADifferentPortForgetsTheRememberedOne) {
  PlayerSettings player;
  player.remembered_port = QStringLiteral("/dev/ttyUSB0");
  player.remembered_baud = 9600;
  player.port_path = QStringLiteral("/dev/ttyUSB1");

  const SettingsDialog dialog(CaptureSettings{}, {}, player, TwoPorts());

  EXPECT_TRUE(dialog.Player().remembered_port.isEmpty());
}

TEST(SettingsDialogTest, EveryModelAndEverySpeedCanBeChosen) {
  const SettingsDialog dialog(CaptureSettings{}, {}, PlayerSettings{},
                              TwoPorts());

  // "Whichever answers" plus every registered model; "work it out" plus every
  // rate a player can be set to. Both lists come from the registry, so a player
  // family added later appears here without this file changing.
  auto* model = Find<QComboBox>(dialog, SettingsDialog::kPlayerModelComboName);
  ASSERT_NE(model, nullptr);
  EXPECT_EQ(model->count(), 11);
  EXPECT_TRUE(model->itemData(0).toString().isEmpty());

  auto* baud = Find<QComboBox>(dialog, SettingsDialog::kPlayerBaudComboName);
  ASSERT_NE(baud, nullptr);
  EXPECT_EQ(baud->count(), 5);
  EXPECT_EQ(baud->itemData(0).toUInt(), 0U);
}

TEST(SettingsDialogTest, TheCouplingPreferenceIsOnThePlayerTabAndRoundTrips) {
  PlayerSettings player;
  player.stop_capture_with_player = true;

  const SettingsDialog dialog(CaptureSettings{}, {}, player, TwoPorts());

  auto* stop_capture =
      Find<QCheckBox>(dialog, SettingsDialog::kPlayerStopCaptureCheckName);
  ASSERT_NE(stop_capture, nullptr);

  EXPECT_TRUE(stop_capture->isChecked());

  stop_capture->setChecked(false);
  EXPECT_FALSE(dialog.Player().stop_capture_with_player);
}

// The coupling runs one way, and the dialog is where somebody would look for
// the other way. There is no control for stopping the player with a capture,
// because outside an automatic capture this application does not drive the
// player at all.
TEST(SettingsDialogTest, ThereIsNoControlForStoppingThePlayerWithACapture) {
  const SettingsDialog dialog(CaptureSettings{}, {}, PlayerSettings{},
                              TwoPorts());

  EXPECT_EQ(dialog.findChild<QCheckBox*>(
                QStringLiteral("settings_player_stop_player")),
            nullptr);
}

}  // namespace
}  // namespace ddd::gui
