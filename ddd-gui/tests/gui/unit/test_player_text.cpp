/************************************************************************

    test_player_text.cpp

    T1 tests for what the interface says about the player
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QSet>
#include <QString>

#include "player_connection.h"
#include "player_state.h"
#include "player_status.h"
#include "player_text.h"

namespace ddd::gui {
namespace {

PlayerConnection Connected() {
  PlayerConnection connection;
  connection.state = PlayerConnectionState::kConnected;
  connection.port_path = QStringLiteral("/dev/ttyUSB0");
  connection.baud_rate = 9600;
  connection.model_name = QStringLiteral("Pioneer LD-V4300D");
  connection.model_id_code = QStringLiteral("15");
  connection.firmware_version = QStringLiteral("A1");
  connection.model_code = QStringLiteral("P1515A1");
  connection.recognised_model = true;
  return connection;
}

PlayerConnection Failed(PlayerConnectionProblem problem) {
  PlayerConnection connection;
  connection.state = PlayerConnectionState::kDisconnected;
  connection.problem = problem;
  return connection;
}

TEST(PlayerTextTest, EveryStateSaysSomethingDifferent) {
  // A panel that said "not connected" to four quite different situations would
  // send a user looking for the fault in the wrong place, so no two of these
  // may be worded the same.
  QSet<QString> summaries;

  const PlayerConnection states[] = {
      PlayerConnection{},
      [] {
        PlayerConnection searching;
        searching.state = PlayerConnectionState::kSearching;
        return searching;
      }(),
      Connected(),
      Failed(PlayerConnectionProblem::kNoPlayerFound),
      Failed(PlayerConnectionProblem::kPortUnavailable),
      Failed(PlayerConnectionProblem::kLinkLost),
  };

  for (const PlayerConnection& connection : states) {
    const QString summary = PlayerConnectionSummary(connection);
    EXPECT_FALSE(summary.isEmpty());
    EXPECT_FALSE(summaries.contains(summary)) << summary.toStdString();
    summaries.insert(summary);
  }
}

TEST(PlayerTextTest, EveryFailureNamesSomethingToDoAboutIt) {
  const PlayerConnectionProblem problems[] = {
      PlayerConnectionProblem::kNoPlayerFound,
      PlayerConnectionProblem::kPortUnavailable,
      PlayerConnectionProblem::kNotAPlayer,
      PlayerConnectionProblem::kLinkLost,
  };

  for (const PlayerConnectionProblem problem : problems) {
    const QString detail = PlayerConnectionDetail(Failed(problem));
    EXPECT_FALSE(detail.isEmpty()) << static_cast<int>(problem);
  }
}

TEST(PlayerTextTest, ThePermissionCaseIsNamedBecauseItIsTheCommonOne) {
  // On Linux, not being in the group that owns the serial devices is the most
  // likely first-run experience there is, and "could not open the port" without
  // that sentence sends people to the wrong place.
  PlayerConnection connection =
      Failed(PlayerConnectionProblem::kPortUnavailable);
  connection.detail = QStringLiteral("/dev/ttyUSB0");

  const QString detail = PlayerConnectionDetail(connection);
  EXPECT_TRUE(detail.contains(QStringLiteral("/dev/ttyUSB0")));
  EXPECT_TRUE(detail.contains(QStringLiteral("dialout")));
}

TEST(PlayerTextTest, SomethingThatIsNotAPlayerSaysHowToLeaveItAlone) {
  PlayerConnection connection = Failed(PlayerConnectionProblem::kNotAPlayer);
  connection.detail = QStringLiteral("/dev/ttyS0 answered with \"GARBAGE\"");

  const QString detail = PlayerConnectionDetail(connection);
  EXPECT_TRUE(detail.contains(QStringLiteral("GARBAGE")));
  EXPECT_TRUE(detail.contains(QStringLiteral("excluded")));
}

TEST(PlayerTextTest, AWorkingConnectionNeedsNoExplanation) {
  EXPECT_TRUE(PlayerConnectionDetail(Connected()).isEmpty());

  EXPECT_EQ(
      PlayerConnectionSource(Connected()),
      QStringLiteral(
          "Pioneer LD-V4300D (firmware A1) on /dev/ttyUSB0 at 9600 baud"));
}

TEST(PlayerTextTest, AMismatchNamesBothModelsAndThePort) {
  PlayerConnection connection = Connected();
  connection.state = PlayerConnectionState::kModelMismatch;
  connection.selected_model_name = QStringLiteral("Pioneer LD-V8000");

  const QString detail = PlayerConnectionDetail(connection);
  EXPECT_TRUE(detail.contains(QStringLiteral("Pioneer LD-V8000")));
  EXPECT_TRUE(detail.contains(QStringLiteral("Pioneer LD-V4300D")));
  EXPECT_TRUE(detail.contains(QStringLiteral("/dev/ttyUSB0")));
}

TEST(PlayerTextTest, AnUnrecognisedPlayerIsToldItWillMostlyWork) {
  PlayerConnection connection = Connected();
  connection.recognised_model = false;
  connection.model_id_code = QStringLiteral("44");
  connection.model_code = QStringLiteral("P1544ZZ");

  const QString detail = PlayerConnectionDetail(connection);
  EXPECT_TRUE(detail.contains(QStringLiteral("P1544ZZ")));
  EXPECT_FALSE(PlayerConnectionSummary(connection).isEmpty());
}

TEST(PlayerTextTest, AnUnverifiedDefinitionSaysSoAndAVerifiedOneDoesNot) {
  // The interface's half of the promise players/README.md makes: a command set
  // that has never met its hardware is a plausible inheritance, and the user is
  // told which of the two they have.
  EXPECT_FALSE(PlayerVerificationNote(Connected()).isEmpty());

  PlayerConnection verified = Connected();
  verified.bench_verified = true;
  EXPECT_TRUE(PlayerVerificationNote(verified).isEmpty());

  // Nothing to say when there is no player.
  EXPECT_TRUE(PlayerVerificationNote(PlayerConnection{}).isEmpty());
}

TEST(PlayerTextTest, ATimeCodeIsShownAsAClock) {
  EXPECT_EQ(FormatTimeCode(1234500), QStringLiteral("1:23:45"));
  EXPECT_EQ(FormatTimeCode(0), QStringLiteral("0:00:00"));

  // Seven digits with leading zeros is how the player sends a time under an
  // hour, and it arrives here as a shorter number.
  EXPECT_EQ(FormatTimeCode(123400), QStringLiteral("0:12:34"));
}

TEST(PlayerTextTest, TheAddressIsReadInTheDiscsOwnTerms) {
  player::PlayerStatus status;
  status.valid = true;
  status.address.valid = true;
  status.address.value = 12345;
  status.disc_type = player::DiscType::kCav;
  EXPECT_EQ(PlayerAddressText(status), QStringLiteral("Frame 12345"));

  status.disc_type = player::DiscType::kClv;
  status.address.value = 1234500;
  EXPECT_EQ(PlayerAddressText(status), QStringLiteral("1:23:45"));
}

TEST(PlayerTextTest, TheLeadInAndLeadOutAreSaidRatherThanNumbered) {
  // The number means nothing there — the lead-in has its own frame numbering —
  // so showing it would be showing a figure with no meaning.
  player::PlayerStatus status;
  status.valid = true;
  status.address.valid = true;
  status.address.value = 100;
  status.address.in_lead_in = true;
  EXPECT_EQ(PlayerAddressText(status), QStringLiteral("Lead-in"));

  status.address.in_lead_in = false;
  status.address.in_lead_out = true;
  EXPECT_EQ(PlayerAddressText(status), QStringLiteral("Lead-out"));
}

TEST(PlayerTextTest, APositionNoModelCanReportIsNoRowAtAll) {
  // Empty rather than "unknown", so the panel can hide the row: a row that is
  // blank for almost every user forever is worse than no row.
  player::PlayerStatus status;
  status.valid = true;
  EXPECT_TRUE(PhysicalPositionText(status).isEmpty());

  status.physical_position_mm = 133.30F;
  EXPECT_TRUE(PhysicalPositionText(status).contains(QStringLiteral("133.30")));
}

TEST(PlayerTextTest, TheStatusBarCarriesTheStateWhateverElseIsHidden) {
  EXPECT_EQ(PlayerStatusBarText(PlayerConnection{}, player::PlayerStatus{}),
            PlayerConnectionSummary(PlayerConnection{}));

  player::PlayerStatus status;
  status.valid = true;
  status.state = player::PlayerState::kPlaying;
  status.disc_type = player::DiscType::kCav;
  status.address.valid = true;
  status.address.value = 100;

  const QString line = PlayerStatusBarText(Connected(), status);
  EXPECT_TRUE(line.contains(QStringLiteral("Pioneer LD-V4300D")));
  EXPECT_TRUE(line.contains(QStringLiteral("Playing")));
  EXPECT_TRUE(line.contains(QStringLiteral("Frame 100")));
}

TEST(PlayerTextTest, EveryPlayerStateHasItsOwnName) {
  const player::PlayerState states[] = {
      player::PlayerState::kUnknown,    player::PlayerState::kDoorOpen,
      player::PlayerState::kParked,     player::PlayerState::kSettingUp,
      player::PlayerState::kUnloading,  player::PlayerState::kPlaying,
      player::PlayerState::kStillFrame, player::PlayerState::kPaused,
      player::PlayerState::kSearching,  player::PlayerState::kScanning,
      player::PlayerState::kMultiSpeed,
  };

  // All of them distinct. The states differ in whether the disc is turning and
  // whether the picture is advancing, so two of them reading the same on screen
  // would be two different situations a user could not tell apart.
  QSet<QString> names;
  for (const player::PlayerState state : states) {
    const QString name = PlayerStateName(state);
    EXPECT_FALSE(name.isEmpty());
    EXPECT_FALSE(names.contains(name)) << name.toStdString();
    names.insert(name);
  }
}

}  // namespace
}  // namespace ddd::gui
