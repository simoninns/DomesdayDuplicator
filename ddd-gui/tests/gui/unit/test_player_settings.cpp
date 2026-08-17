/************************************************************************

    test_player_settings.cpp

    T1 tests for what is remembered about the player, and what is not
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSettings>
#include <QStringList>

#include "player_settings.h"

namespace ddd::gui {
namespace {

class PlayerSettingsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-player-settings-%1")
            .arg(QLatin1String(info->name())));
    QSettings().clear();
  }

  void TearDown() override { QSettings().clear(); }
};

TEST_F(PlayerSettingsTest, PlayerControlIsOffUntilSomebodyTurnsItOn) {
  // The default that matters most in this feature. Searching for a player means
  // writing bytes to every serial port on the machine, and an application that
  // did that on first launch would be doing something nobody asked for.
  const PlayerSettings loaded = LoadPlayerSettings();
  EXPECT_FALSE(loaded.enabled);
  EXPECT_TRUE(loaded.model_id_code.isEmpty());
  EXPECT_TRUE(loaded.port_path.isEmpty());
  EXPECT_EQ(loaded.baud_rate, 0U);
  EXPECT_TRUE(loaded.excluded_ports.isEmpty());
}

TEST_F(PlayerSettingsTest, WhatWasSavedComesBack) {
  PlayerSettings saved;
  saved.enabled = true;
  saved.model_id_code = QStringLiteral("15");
  saved.port_path = QStringLiteral("/dev/ttyUSB0");
  saved.baud_rate = 4800;
  saved.excluded_ports = QStringList{QStringLiteral("/dev/ttyS0")};
  saved.remembered_port = QStringLiteral("/dev/ttyUSB1");
  saved.remembered_baud = 1200;

  SavePlayerSettings(saved);

  EXPECT_EQ(LoadPlayerSettings(), saved);
}

TEST_F(PlayerSettingsTest, ABaudRateNoPlayerUsesReadsAsWorkItOut) {
  // Clamped rather than refused, as the capture settings are: a hand-edited
  // file should give a working search rather than an error.
  QSettings store;
  store.setValue(QStringLiteral("player/baud_rate"), 115200);
  store.sync();

  EXPECT_EQ(LoadPlayerSettings().baud_rate, 0U);

  // And the rates a player really can be set to survive.
  EXPECT_TRUE(IsSupportedPlayerBaudRate(9600));
  EXPECT_TRUE(IsSupportedPlayerBaudRate(1200));
  EXPECT_FALSE(IsSupportedPlayerBaudRate(115200));
  EXPECT_FALSE(IsSupportedPlayerBaudRate(0));
}

TEST_F(PlayerSettingsTest, AModelThisBuildDoesNotKnowReadsAsWhicheverAnswers) {
  // Keeping the string and never matching it would put the application
  // permanently in the model-mismatch state against a player that is working
  // perfectly well.
  QSettings store;
  store.setValue(QStringLiteral("player/model_id_code"), QStringLiteral("XX"));
  store.sync();

  EXPECT_TRUE(LoadPlayerSettings().model_id_code.isEmpty());
}

TEST_F(PlayerSettingsTest, AHalfRememberedPortIsForgotten) {
  // A port with no rate would be probed by trying every rate, which is what the
  // scan does anyway; a rate with no port names nothing. Either way it is not
  // the one cheap probe the pair exists to provide.
  QSettings store;
  store.setValue(QStringLiteral("player/remembered_port"),
                 QStringLiteral("/dev/ttyUSB0"));
  store.sync();

  const PlayerSettings loaded = LoadPlayerSettings();
  EXPECT_TRUE(loaded.remembered_port.isEmpty());
  EXPECT_EQ(loaded.remembered_baud, 0U);
}

TEST_F(PlayerSettingsTest, AnEmptyExcludedEntryIsNotAPort) {
  QSettings store;
  store.setValue(QStringLiteral("player/excluded_ports"),
                 QStringList{QString(), QStringLiteral("/dev/ttyS0")});
  store.sync();

  EXPECT_EQ(LoadPlayerSettings().excluded_ports,
            QStringList{QStringLiteral("/dev/ttyS0")});
}

TEST_F(PlayerSettingsTest, TheCouplingPreferenceHasTheDefaultItArguesFor) {
  const PlayerSettings loaded = LoadPlayerSettings();

  // Off, and a considered default rather than timidity: a player that briefly
  // reports a stopped state partway through a side — which a disc with a defect
  // will make it do — would truncate a capture that was going perfectly well.
  EXPECT_FALSE(loaded.stop_capture_with_player);
}

TEST_F(PlayerSettingsTest, TheCouplingPreferenceRoundTrips) {
  PlayerSettings settings = LoadPlayerSettings();
  settings.stop_capture_with_player = true;
  SavePlayerSettings(settings);

  const PlayerSettings loaded = LoadPlayerSettings();
  EXPECT_TRUE(loaded.stop_capture_with_player);
  EXPECT_EQ(loaded, settings);
}

// The preference that is gone, and the check that it stays gone from a settings
// file written by a build that had it. It is removed on save rather than
// ignored on load, so a setting nothing reads cannot linger where somebody
// might later find it and believe it does something.
TEST_F(PlayerSettingsTest, TheRetiredStopThePlayerPreferenceIsCleanedUp) {
  QSettings().setValue(QStringLiteral("player/stop_player_with_capture"), true);

  SavePlayerSettings(LoadPlayerSettings());

  EXPECT_FALSE(
      QSettings().contains(QStringLiteral("player/stop_player_with_capture")));
}

}  // namespace
}  // namespace ddd::gui
