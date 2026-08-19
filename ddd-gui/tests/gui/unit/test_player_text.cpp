/************************************************************************

    test_player_text.cpp

    T1 tests for what the interface says about the player
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QByteArray>
#include <QSet>
#include <QString>
#include <QStringList>
#include <cstddef>
#include <string>

#include "disc_examiner.h"
#include "disc_profile.h"
#include "player_command.h"
#include "player_connection.h"
#include "player_request.h"
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
      Failed(PlayerConnectionProblem::kPortNotPermitted),
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
      PlayerConnectionProblem::kPortNotPermitted,
      PlayerConnectionProblem::kNotAPlayer,
      PlayerConnectionProblem::kLinkLost,
  };

  for (const PlayerConnectionProblem problem : problems) {
    const QString detail = PlayerConnectionDetail(Failed(problem));
    EXPECT_FALSE(detail.isEmpty()) << static_cast<int>(problem);
  }
}

// Every platform's advice, checked on whichever platform this happens to be.
//
// The point of taking the platform as an argument rather than reading it from
// the build inside the wording: two of these three sentences can never be seen
// by the person who wrote them, and a permission message that is wrong is worse
// than none — it sends somebody to spend an afternoon on the wrong remedy.
TEST(PlayerTextTest, EachPlatformIsToldItsOwnRemedy) {
  const QString linux_advice = SerialPermissionAdvice(HostPlatform::kLinux);
  const QString macos_advice = SerialPermissionAdvice(HostPlatform::kMacOs);
  const QString windows_advice = SerialPermissionAdvice(HostPlatform::kWindows);

  // The group is the whole of the Linux answer, and it is not the same group on
  // every distribution.
  EXPECT_TRUE(linux_advice.contains(QStringLiteral("dialout")));
  EXPECT_TRUE(linux_advice.contains(QStringLiteral("uucp")));

  // macOS has no group to join — /dev/cu.* is open to everybody — so the Linux
  // sentence would be actively misleading there. The driver is the answer.
  EXPECT_TRUE(macos_advice.contains(QStringLiteral("driver")));
  EXPECT_FALSE(macos_advice.contains(QStringLiteral("dialout")));

  // Nor does Windows: a COM port that will not open is one another program has.
  EXPECT_TRUE(windows_advice.contains(QStringLiteral("COM")));
  EXPECT_FALSE(windows_advice.contains(QStringLiteral("dialout")));

  EXPECT_NE(linux_advice, macos_advice);
  EXPECT_NE(macos_advice, windows_advice);
}

TEST(PlayerTextTest, ARefusedPortNamesTheRemedyForThisPlatform) {
  // Not being allowed the port is the most likely first-run experience there
  // is, and "could not open the port" without the sentence that follows sends
  // people to look at their cable.
  PlayerConnection connection =
      Failed(PlayerConnectionProblem::kPortNotPermitted);
  connection.detail = QStringLiteral("/dev/ttyUSB0");

  const QString detail = PlayerConnectionDetail(connection);
  EXPECT_TRUE(detail.contains(QStringLiteral("/dev/ttyUSB0")));
  EXPECT_TRUE(detail.contains(SerialPermissionAdvice(ThisPlatform())));

  // Without a port to name it is still the advice, rather than nothing.
  EXPECT_EQ(PlayerConnectionDetail(
                Failed(PlayerConnectionProblem::kPortNotPermitted)),
            SerialPermissionAdvice(ThisPlatform()));
}

TEST(PlayerTextTest, APortThatWouldNotOpenCarriesTheAdviceToo) {
  // The generic case cannot tell a busy port from a refused one — some backends
  // genuinely cannot say — so it offers both possibilities and the same remedy.
  PlayerConnection connection =
      Failed(PlayerConnectionProblem::kPortUnavailable);
  connection.detail = QStringLiteral("/dev/ttyUSB0");

  const QString detail = PlayerConnectionDetail(connection);
  EXPECT_TRUE(detail.contains(QStringLiteral("/dev/ttyUSB0")));
  EXPECT_TRUE(detail.contains(SerialPermissionAdvice(ThisPlatform())));
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

TEST(PlayerTextTest, ATimeCodeTypedAsAClockIsReadAsOne) {
  // The inverse of FormatTimeCode, and the form the disc sleeve uses.
  EXPECT_EQ(ParseTimeCodeEntry(QStringLiteral("1:23:45")), 1234500);

  // Right-aligned, the way a clock is read: two fields are minutes and seconds
  // on a disc under an hour, not hours and minutes.
  EXPECT_EQ(ParseTimeCodeEntry(QStringLiteral("23:45")), 234500);
  EXPECT_EQ(ParseTimeCodeEntry(QStringLiteral("45")), 45);

  // And the bare form, for somebody reading the player's own display.
  EXPECT_EQ(ParseTimeCodeEntry(QStringLiteral("1234500")), 1234500);

  // Round trips against the formatter, which is the property that matters.
  // value_or rather than a dereference: a refusal here would otherwise fail as
  // a crash rather than as a message saying what was refused.
  EXPECT_EQ(FormatTimeCode(
                ParseTimeCodeEntry(QStringLiteral("2:03:04")).value_or(-1)),
            QStringLiteral("2:03:04"));
}

TEST(PlayerTextTest, SomethingThatIsNotATimeIsRefusedRatherThanGuessedAt) {
  // A seek to a number invented out of "1:99" would move the disc somewhere
  // nobody asked for, which is worse than being told the entry is wrong.
  EXPECT_FALSE(ParseTimeCodeEntry(QStringLiteral("1:99")).has_value());
  EXPECT_FALSE(ParseTimeCodeEntry(QStringLiteral("1:23:60")).has_value());
  EXPECT_FALSE(ParseTimeCodeEntry(QStringLiteral("10:00:00")).has_value());
  EXPECT_FALSE(ParseTimeCodeEntry(QStringLiteral("1:2:3:4")).has_value());
  EXPECT_FALSE(ParseTimeCodeEntry(QStringLiteral("1::3")).has_value());
  EXPECT_FALSE(ParseTimeCodeEntry(QStringLiteral("abc")).has_value());
  EXPECT_FALSE(ParseTimeCodeEntry(QString()).has_value());
  EXPECT_FALSE(ParseTimeCodeEntry(QStringLiteral("12345678")).has_value());
}

TEST(PlayerTextTest, EveryControlHasItsOwnName) {
  // Named for the log and for the sentence explaining why a control is
  // unavailable, so two controls reading the same would make both illegible.
  QSet<QString> names;
  for (size_t index = 0; index < player::kPlayerCommandCount; ++index) {
    const QString name =
        PlayerCommandName(static_cast<player::PlayerCommand>(index));
    EXPECT_FALSE(name.isEmpty());
    EXPECT_FALSE(names.contains(name)) << name.toStdString();
    names.insert(name);
  }

  for (size_t index = 0; index < player::kAudioModeCount; ++index) {
    const QString name = AudioModeName(static_cast<player::AudioMode>(index));
    EXPECT_FALSE(name.isEmpty());
  }

  for (size_t index = 0; index < player::kPlaybackSpeedCount; ++index) {
    const QString name =
        PlaybackSpeedName(static_cast<player::PlaybackSpeed>(index));
    EXPECT_FALSE(name.isEmpty());
  }
}

TEST(PlayerTextTest, AnUnavailableControlSaysWhichPlayersDoHaveIt) {
  PlayerConnection connection = Connected();

  const QString note = UnsupportedControlNote(
      connection, player::PlayerCommand::kQueryPhysicalPosition);

  // The LD-V4300D cannot report the optical assembly's position and the
  // LD-V8000 can, so the note names both sides of that. The old application
  // said nothing at all, which made a capability the player lacked look like a
  // fault in the cable.
  EXPECT_TRUE(note.contains(QStringLiteral("Pioneer LD-V4300D")));
  EXPECT_TRUE(note.contains(QStringLiteral("Pioneer LD-V8000")));
}

TEST(PlayerTextTest, AControlNothingOffersSaysThatRatherThanNamingNobody) {
  PlayerConnection connection = Connected();

  // The branch that fires when a control is added to the generic set before any
  // definition has a sequence for it. kCount stands in for that here because it
  // is the one value of the enumeration guaranteed to have no sequence — every
  // real control today is offered by every registered model, so there is no
  // other way to reach this wording, and leaving it unreached would mean
  // shipping a sentence nobody has read.
  const QString note =
      UnsupportedControlNote(connection, player::PlayerCommand::kCount);

  EXPECT_TRUE(note.contains(QStringLiteral("Pioneer LD-V4300D")));
  EXPECT_TRUE(note.contains(QStringLiteral("nor does any other")));
}

TEST(PlayerTextTest, AnExchangeIsDescribedAsWhatWentAndWhatCameBack) {
  PlayerReply reply;
  reply.request = CommandRequest(player::PlayerCommand::kQueryActiveMode);
  reply.sent = QStringLiteral("?P");
  reply.text = QStringLiteral("P04");
  reply.status = player::ReplyStatus::kOk;

  const QString line = PlayerReplyText(reply);
  EXPECT_TRUE(line.contains(QStringLiteral("?P")));
  EXPECT_TRUE(line.contains(QStringLiteral("P04")));
}

TEST(PlayerTextTest, AManualReplyIsShownVerbatimIncludingARefusal) {
  // The whole reason the manual command field exists: what the player actually
  // answered, not this library's opinion of it.
  PlayerReply reply;
  reply.request = RawRequest(QStringLiteral("?U"));
  reply.sent = QStringLiteral("?U");
  reply.text = QStringLiteral("E04");
  reply.status = player::ReplyStatus::kOk;

  const QString line = PlayerReplyText(reply);
  EXPECT_TRUE(line.contains(QStringLiteral("E04")));

  // And marked as a refusal, because a reply that is exactly 'E' and digits is
  // not a user code — an LD-V4300D answers the Pioneer user-code query that way
  // on a parked player.
  EXPECT_TRUE(line.contains(QStringLiteral("refused")));

  // A user code that merely contains an 'E' is not.
  reply.text = QStringLiteral("ENCODED123");
  EXPECT_FALSE(PlayerReplyText(reply).contains(QStringLiteral("refused")));
}

TEST(PlayerTextTest, BytesAreDumpedInTheShapeSomebodyCanCountColumnsIn) {
  // A full line, pinned exactly: the offset, sixteen bytes of hex with a gap at
  // the halfway mark so a column can be counted off by eye, then the ASCII
  // gutter. Decimal offsets rather than the conventional hex, because what is
  // being counted here is fields in a fixed-width record.
  //
  // These are the first sixteen bytes of a real Pioneer user code, taken off
  // the project's own bench.
  const QString full = FormatByteDump(QByteArray("#59-014    *MCA ", 16));
  EXPECT_EQ(full, QStringLiteral("0000  23 35 39 2D 30 31 34 20  "
                                 "20 20 20 2A 4D 43 41 20 |#59-014    *MCA |"));

  // A short line pads its hex out so the gutter still starts in the same
  // column, which is the whole point of a fixed-width dump.
  const QString partial = FormatByteDump(QByteArray("#59-014", 7));
  EXPECT_TRUE(
      partial.startsWith(QStringLiteral("0000  23 35 39 2D 30 31 34 ")));
  EXPECT_TRUE(partial.endsWith(QStringLiteral("|#59-014|")));
  EXPECT_EQ(partial.indexOf(QLatin1Char('|')), full.indexOf(QLatin1Char('|')));

  EXPECT_TRUE(FormatByteDump(QByteArray()).isEmpty());
}

TEST(PlayerTextTest, ADumpBreaksEverySixteenBytesAndMarksTheUnprintable) {
  QByteArray bytes;
  for (int value = 0; value < 20; ++value) {
    bytes.append(static_cast<char>(value));
  }

  const QStringList lines = FormatByteDump(bytes).split(QLatin1Char('\n'));
  ASSERT_EQ(lines.size(), 2);
  EXPECT_TRUE(lines.at(0).startsWith(QStringLiteral("0000  00 01 02")));
  EXPECT_TRUE(lines.at(1).startsWith(QStringLiteral("0016  10 11 12 13")));

  // A control byte is a dot in the gutter and its own value in the hex, which
  // is the whole reason for having both columns.
  EXPECT_TRUE(lines.at(0).endsWith(QStringLiteral("|................|")));
}

TEST(PlayerTextTest, AUserCodeIsReportedRegionByRegion) {
  // The reply the whole exercise is about, off the project's own bench: an MCA
  // *Casper* disc whose Disc Control Data reads perfectly and whose Key Data —
  // the customer's own disc-identifying information — is entirely unreadable.
  const QString record = QStringLiteral(
      "#59-014    *MCA / CASPER THX LTBX         !2 %0510803@@@@@@@");
  ASSERT_EQ(record.size(), 60);

  PlayerReply reply;
  reply.request = CommandRequest(player::PlayerCommand::kQueryPioneerUserCode);
  reply.sent = QStringLiteral("?U");
  reply.status = player::ReplyStatus::kOk;
  reply.text = record + record + QString(60, QLatin1Char('`')) +
               QString(20, QLatin1Char('0'));
  ASSERT_EQ(reply.text.size(), 200);

  const QString report = PlayerReplyReport(reply);

  EXPECT_TRUE(report.contains(QStringLiteral("200 characters")));
  EXPECT_TRUE(report.contains(QStringLiteral("Disc Control Data — 120")));
  EXPECT_TRUE(report.contains(QStringLiteral("Key Data — 60")));
  EXPECT_TRUE(report.contains(QStringLiteral("Control Data — 20")));

  // Each region dumped at its own place in the whole, so an offset read off the
  // screen is an offset into the user code.
  EXPECT_TRUE(report.contains(QStringLiteral("0120  60 60 60")));
  EXPECT_TRUE(report.contains(QStringLiteral("0180  30 30 30")));

  // The finding, said as a finding rather than left to be counted.
  EXPECT_TRUE(report.contains(QStringLiteral("none of it could be read")));

  // And the whole-reply count up front, so nobody has to add three headings
  // together to see how much of a disc did not read. A PAL CLV disc on this
  // bench returns 180 unreadable out of 200.
  EXPECT_TRUE(
      report.contains(QStringLiteral("60 of the 200 could not be read")));

  // And the region that read cleanly does not claim otherwise.
  const qsizetype disc_control = report.indexOf(QStringLiteral("Disc Control"));
  const qsizetype key_data = report.indexOf(QStringLiteral("Key Data"));
  ASSERT_GT(key_data, disc_control);
  EXPECT_FALSE(report.mid(disc_control, key_data - disc_control)
                   .contains(QStringLiteral("could not be read")));
}

TEST(PlayerTextTest, AUserCodeOfTheWrongLengthSaysSoRatherThanPretending) {
  PlayerReply reply;
  reply.request = CommandRequest(player::PlayerCommand::kQueryPioneerUserCode);
  reply.sent = QStringLiteral("?U");
  reply.status = player::ReplyStatus::kOk;
  reply.text = QString(130, QLatin1Char('A'));

  const QString report = PlayerReplyReport(reply);

  // The region boundaries shown are the format's, and somebody reading a dump
  // of a short reply needs to know that rather than assume they are the
  // player's.
  EXPECT_TRUE(report.contains(QStringLiteral("130 characters")));
  EXPECT_TRUE(report.contains(QStringLiteral("The format says 200")));
  EXPECT_TRUE(report.contains(QStringLiteral("stopped part way through")));
  EXPECT_TRUE(report.contains(QStringLiteral("Not in the reply")));
}

TEST(PlayerTextTest, ARefusedUserCodeIsNotDressedUpAsAReading) {
  // E04 is not a very short user code, so there are no regions to report on and
  // reporting three empty ones would be inventing a reading of it.
  PlayerReply reply;
  reply.request = CommandRequest(player::PlayerCommand::kQueryPioneerUserCode);
  reply.sent = QStringLiteral("?U");
  reply.status = player::ReplyStatus::kOk;
  reply.text = QStringLiteral("E04");

  const QString report = PlayerReplyReport(reply);
  EXPECT_TRUE(report.contains(QStringLiteral("refused")));
  EXPECT_FALSE(report.contains(QStringLiteral("Key Data")));

  // And it says what it means, in both the readings that have been seen on the
  // bench: a disc that has no user code, and a player that cannot reach the
  // lead-in from where it is. Told only "refused", somebody would have no way
  // to tell those apart or to know that trying again might work.
  EXPECT_TRUE(
      report.contains(QStringLiteral("carries no Pioneer User's Code")));
  EXPECT_TRUE(report.contains(QStringLiteral("with the disc playing")));
  EXPECT_TRUE(
      report.contains(QStringLiteral("disc must be playing for this to work")));
}

TEST(PlayerTextTest, ARefusedStandardUserCodeSaysTheDiscMustBePlaying) {
  // The same finding as above, on the other code: a stopped player refuses this
  // one too, and the reading somebody takes from a bare "refused" is that the
  // disc has no code — which on this project's bench was wrong, because both
  // codes read once the disc was playing.
  PlayerReply reply;
  reply.request = CommandRequest(player::PlayerCommand::kQueryStandardUserCode);
  reply.sent = QStringLiteral("$Y");
  reply.status = player::ReplyStatus::kOk;
  reply.text = QStringLiteral("E04");

  const QString report = PlayerReplyReport(reply);
  EXPECT_TRUE(report.contains(QStringLiteral("refused")));
  EXPECT_TRUE(
      report.contains(QStringLiteral("disc must be playing for this to work")));
  EXPECT_TRUE(report.contains(QStringLiteral("with the disc playing")));
}

TEST(PlayerTextTest, AUserCodeRefusedOutrightIsNotedTheSameWay) {
  // A refusal that arrives as a refusal rather than as an error code in a text
  // reply. Both shapes have been seen, and the reading to correct is the same
  // either way.
  PlayerReply pioneer;
  pioneer.request =
      CommandRequest(player::PlayerCommand::kQueryPioneerUserCode);
  pioneer.sent = QStringLiteral("?U");
  pioneer.status = player::ReplyStatus::kRefused;
  pioneer.error_code = QStringLiteral("E04");

  EXPECT_TRUE(PlayerReplyReport(pioneer).contains(
      QStringLiteral("disc must be playing for this to work")));

  PlayerReply standard;
  standard.request =
      CommandRequest(player::PlayerCommand::kQueryStandardUserCode);
  standard.sent = QStringLiteral("$Y");
  standard.status = player::ReplyStatus::kRefused;
  standard.error_code = QStringLiteral("E04");

  EXPECT_TRUE(PlayerReplyReport(standard).contains(
      QStringLiteral("disc must be playing for this to work")));
}

TEST(PlayerTextTest, TheUnreadableCharacterIsSaidToBeThePlayersAndNotOurs) {
  // The distinction that sent somebody looking at this in the first place: a
  // wall of backticks is not a decode this application got wrong. Per the
  // LD-V4400 manual, the player sends 0x60 for each character it could not read
  // off the disc — so it is a fact about the disc, and it is said in words.
  // The standard user code rather than the Pioneer one, so this exercises the
  // generic wording every other reply gets; the Pioneer code has a structure of
  // its own and is covered above.
  PlayerReply reply;
  reply.request = CommandRequest(player::PlayerCommand::kQueryStandardUserCode);
  reply.sent = QStringLiteral("$Y");
  reply.status = player::ReplyStatus::kOk;
  reply.text = QString(60, QLatin1Char('`')) + QString(20, QLatin1Char('0'));

  const QString report = PlayerReplyReport(reply);
  EXPECT_TRUE(report.contains(QStringLiteral("80 bytes")));
  EXPECT_TRUE(report.contains(QStringLiteral("60 of them")));
  EXPECT_TRUE(report.contains(QStringLiteral("could not read off the disc")));
  EXPECT_TRUE(report.contains(QStringLiteral("0000  60 60 60")));
}

TEST(PlayerTextTest, OnlyARelpyWorthDumpingIsDumped) {
  // One rule, and it has to leave the common case alone: an active-mode reply
  // is three legible characters and burying it under a hex dump would make
  // every ordinary command worse to serve a rare one.
  PlayerReply legible;
  legible.request = CommandRequest(player::PlayerCommand::kQueryActiveMode);
  legible.sent = QStringLiteral("?P");
  legible.status = player::ReplyStatus::kOk;
  legible.text = QStringLiteral("P04");

  const QString short_report = PlayerReplyReport(legible);
  EXPECT_TRUE(short_report.contains(QStringLiteral("P04")));
  EXPECT_FALSE(short_report.contains(QStringLiteral("bytes.")));

  // Longer than a dump line gets one.
  PlayerReply long_reply = legible;
  long_reply.text = QString(17, QLatin1Char('A'));
  EXPECT_TRUE(PlayerReplyReport(long_reply).contains(QStringLiteral("0000  ")));

  // And so does a short reply carrying a byte that is not printable, which is
  // the case a length rule alone would miss.
  PlayerReply binary = legible;
  binary.text = QStringLiteral("AB") + QChar(QLatin1Char('\x01'));
  EXPECT_TRUE(PlayerReplyReport(binary).contains(QStringLiteral("0000  ")));
}

TEST(PlayerTextTest, EveryWayAnExchangeCanFailReadsDifferently) {
  const player::ReplyStatus failures[] = {
      player::ReplyStatus::kRefused,         player::ReplyStatus::kNoAnswer,
      player::ReplyStatus::kUnparseable,     player::ReplyStatus::kLinkFailed,
      player::ReplyStatus::kNotConnected,    player::ReplyStatus::kUnsupported,
      player::ReplyStatus::kInvalidArgument,
  };

  QSet<QString> lines;
  for (const player::ReplyStatus status : failures) {
    PlayerReply reply;
    reply.request = CommandRequest(player::PlayerCommand::kPlay);
    reply.sent = QStringLiteral("PL");
    reply.status = status;

    const QString line = PlayerReplyText(reply);
    EXPECT_FALSE(line.isEmpty());
    EXPECT_FALSE(lines.contains(line)) << line.toStdString();
    lines.insert(line);
  }
}

// --- Examining the disc ----------------------------------------------------

player::DiscProfile ExaminedCavDisc() {
  player::DiscProfile disc;
  disc.disc_present.Record(true, player::Provenance::kMeasured);
  disc.tray.Record(player::TrayState::kClosed, player::Provenance::kReported);
  disc.disc_type.Record(player::DiscType::kCav, player::Provenance::kReported);
  disc.addressing.Record(player::AddressMode::kFrame,
                         player::Provenance::kInferred);
  disc.programme_start.Record(1, player::Provenance::kMeasured);
  disc.programme_end.Record(54000, player::Provenance::kMeasured);
  disc.lead_in_reachable.Record(true, player::Provenance::kMeasured);
  disc.chapters.Record(true, player::Provenance::kMeasured);
  disc.disc_status_reply = "10001";
  return disc;
}

TEST(PlayerTextTest, EveryExamineStageSaysSomethingDifferentAndInPlainWords) {
  const player::ExamineStage stages[] = {
      player::ExamineStage::kIdle,
      player::ExamineStage::kCheckingPlayer,
      player::ExamineStage::kSpinningUp,
      player::ExamineStage::kReadingDiscStatus,
      player::ExamineStage::kReadingPioneerUserCode,
      player::ExamineStage::kReadingStandardUserCode,
      player::ExamineStage::kCheckingChapters,
      player::ExamineStage::kFindingEnd,
      player::ExamineStage::kReadingEnd,
      player::ExamineStage::kFindingStart,
      player::ExamineStage::kReadingStart,
      player::ExamineStage::kSettling,
      player::ExamineStage::kFinished,
  };

  QSet<QString> seen;
  for (const player::ExamineStage stage : stages) {
    const QString name = ExamineStageName(stage);
    EXPECT_FALSE(name.isEmpty());
    EXPECT_FALSE(seen.contains(name)) << name.toStdString();
    seen.insert(name);
  }

  // The slow one explains itself, because a progress line that has not moved
  // for eleven seconds is the one a user is staring at.
  EXPECT_TRUE(ExamineStageName(player::ExamineStage::kReadingPioneerUserCode)
                  .contains(QStringLiteral("lead-in")));
}

TEST(PlayerTextTest, EveryProvenanceReadsDifferently) {
  const player::Provenance sources[] = {
      player::Provenance::kUnknown,  player::Provenance::kReported,
      player::Provenance::kMeasured, player::Provenance::kInferred,
      player::Provenance::kDeclared,
  };

  QSet<QString> seen;
  for (const player::Provenance source : sources) {
    const QString note = ProvenanceNote(source);
    EXPECT_FALSE(note.isEmpty());
    EXPECT_FALSE(seen.contains(note)) << note.toStdString();
    seen.insert(note);
  }
}

TEST(PlayerTextTest, AnAddressIsWrittenTheWayItsDiscIsAddressed) {
  EXPECT_EQ(FormatDiscAddress(54000, player::AddressMode::kFrame),
            QStringLiteral("Frame 54000"));

  // The same number read as a time code is a completely different thing, which
  // is why the mode is passed in rather than guessed.
  EXPECT_EQ(FormatDiscAddress(504500, player::AddressMode::kTimeCode),
            QStringLiteral("0:50:45"));
}

TEST(PlayerTextTest, TheHeadlineSaysWhatTheDiscIsAndHowLongItRuns) {
  const QString summary =
      ExamineSummary(ExaminedCavDisc(), player::ExamineOutcome::kCompleted);

  EXPECT_TRUE(summary.contains(QStringLiteral("CAV")));
  EXPECT_TRUE(summary.contains(QStringLiteral("Frame 54000")));
}

TEST(PlayerTextTest, EachWayAnExaminationCanEndReadsDifferently) {
  const player::ExamineOutcome outcomes[] = {
      player::ExamineOutcome::kInProgress, player::ExamineOutcome::kCompleted,
      player::ExamineOutcome::kTrayOpen,   player::ExamineOutcome::kNoDisc,
      player::ExamineOutcome::kLinkFailed, player::ExamineOutcome::kCancelled,
  };

  QSet<QString> outcome_text;
  QSet<QString> summaries;
  for (const player::ExamineOutcome outcome : outcomes) {
    const QString text = ExamineOutcomeText(outcome);
    EXPECT_FALSE(text.isEmpty());
    EXPECT_FALSE(outcome_text.contains(text)) << text.toStdString();
    outcome_text.insert(text);

    const QString summary = ExamineSummary(ExaminedCavDisc(), outcome);
    EXPECT_FALSE(summary.isEmpty());
    EXPECT_FALSE(summaries.contains(summary)) << summary.toStdString();
    summaries.insert(summary);
  }
}

TEST(PlayerTextTest, TheReportLabelsEveryFactWithHowItWasArrivedAt) {
  const QString report = DiscProfileReport(
      ExaminedCavDisc(), player::ExamineOutcome::kCompleted, 0.0);

  // The measurement and the inference are not presented alike, which is the
  // whole reason provenance is carried at all.
  EXPECT_TRUE(
      report.contains(QStringLiteral("Last address: Frame 54000  "
                                     "(measured)")));
  EXPECT_TRUE(
      report.contains(QStringLiteral("Addressing: frame number  (inferred)")));
  EXPECT_TRUE(
      report.contains(QStringLiteral("Type: CAV  (reported by the "
                                     "player)")));
}

TEST(PlayerTextTest, AFieldNobodyEstablishedIsSaidToBeUnknownRatherThanBlank) {
  player::DiscProfile disc = ExaminedCavDisc();
  disc.chapters = player::Fact<bool>{};

  const QString report =
      DiscProfileReport(disc, player::ExamineOutcome::kCompleted, 0.0);

  EXPECT_TRUE(
      report.contains(QStringLiteral("Chapters: not known  (not "
                                     "established)")));
  EXPECT_FALSE(report.contains(QStringLiteral("Chapters: none")));
}

TEST(PlayerTextTest, ACavDiscWithNoStandardHasNoPlayingTimeAndSaysWhy) {
  const QString report = DiscProfileReport(
      ExaminedCavDisc(), player::ExamineOutcome::kCompleted, 1.0e6);

  // The same "not known" every unestablished field gets, with the sentence
  // underneath naming the one command that would have answered it.
  EXPECT_TRUE(report.contains(
      QStringLiteral("Video standard: not known  (not established)")));
  EXPECT_TRUE(report.contains(QStringLiteral("TV system request")));
  EXPECT_TRUE(report.contains(QStringLiteral("video standard is known")));

  // And nothing was estimated from a length that is only a frame count.
  EXPECT_FALSE(report.contains(QStringLiteral("Capture size")));
}

TEST(PlayerTextTest, AClvDiscsPlayingTimeAndCaptureSizeAreBothStated) {
  player::DiscProfile disc;
  disc.disc_type.Record(player::DiscType::kClv, player::Provenance::kReported);
  disc.addressing.Record(player::AddressMode::kTimeCode,
                         player::Provenance::kInferred);
  disc.programme_start.Record(0, player::Provenance::kMeasured);
  disc.programme_end.Record(504500, player::Provenance::kMeasured);
  disc.video_standard.Record(player::VideoStandard::kPal,
                             player::Provenance::kDeclared);

  const QString report =
      DiscProfileReport(disc, player::ExamineOutcome::kCompleted, 1.0e6);

  // Labelled with where it came from, so a standard the player reported and a
  // standard the user typed do not read alike.
  EXPECT_TRUE(
      report.contains(QStringLiteral("Video standard: PAL  (declared)")));
  EXPECT_TRUE(report.contains(QStringLiteral("Playing time: 0:50:45")));
  EXPECT_TRUE(report.contains(QStringLiteral("Capture size")));
}

TEST(PlayerTextTest, TheFourUserCodeOutcomesAreFourDifferentSentences) {
  QSet<QString> lines;

  const player::UserCodeReading::Outcome outcomes[] = {
      player::UserCodeReading::Outcome::kNotRead,
      player::UserCodeReading::Outcome::kNotEncoded,
      player::UserCodeReading::Outcome::kRefused,
      player::UserCodeReading::Outcome::kRead,
  };

  for (const player::UserCodeReading::Outcome outcome : outcomes) {
    player::DiscProfile disc = ExaminedCavDisc();
    disc.pioneer_user_code.outcome = outcome;
    disc.pioneer_user_code.text =
        outcome == player::UserCodeReading::Outcome::kNotEncoded ? "E04"
                                                                 : "#59-014";

    const QString report =
        DiscProfileReport(disc, player::ExamineOutcome::kCompleted, 0.0);

    const QStringList found =
        report.split(QLatin1Char('\n')).filter(QStringLiteral("Pioneer:"));
    ASSERT_EQ(found.size(), 1);
    EXPECT_FALSE(lines.contains(found.front())) << found.front().toStdString();
    lines.insert(found.front());
  }
}

TEST(PlayerTextTest, AUserCodeTheDiscHasIsNotConfusedWithOneItLacks) {
  player::DiscProfile disc = ExaminedCavDisc();
  disc.pioneer_user_code.outcome = player::UserCodeReading::Outcome::kRead;
  disc.pioneer_user_code.text = std::string("#59-014") + std::string(60, '`');

  const QString report =
      DiscProfileReport(disc, player::ExamineOutcome::kCompleted, 0.0);

  // The player could not read sixty of them off the disc, which is a fact
  // about the disc and not about this application.
  EXPECT_TRUE(
      report.contains(QStringLiteral("60 of 67 characters could not be "
                                     "read off the disc")));
}

TEST(PlayerTextTest, TheReportSaysTheUserCodesAreInformationalOnly) {
  // A hard constraint of the design, stated where a user reading the report
  // will see it — because the Disc Control Data looks very much like it
  // carries the disc's length, and it is not to be used that way.
  const QString report = DiscProfileReport(
      ExaminedCavDisc(), player::ExamineOutcome::kCompleted, 0.0);

  EXPECT_TRUE(report.contains(QStringLiteral("informational only")));
}

TEST(PlayerTextTest, TheDiscStatusReplyIsShownBesideWhatWasDecodedFromIt) {
  // The working, not just the answer. A report that says "side 2" and shows
  // the reply it read that from is one somebody can check.
  const QString report = DiscProfileReport(
      ExaminedCavDisc(), player::ExamineOutcome::kCompleted, 0.0);

  EXPECT_TRUE(report.contains(QStringLiteral("Disc status reply: \"10001\"")));
  EXPECT_TRUE(report.contains(QStringLiteral("loaded, CAV/CLV, size, side")));
}

TEST(PlayerTextTest, TheSizeAndTheSideAreReportedAsADiscIsDescribed) {
  player::DiscProfile disc = ExaminedCavDisc();
  disc.disc_size.Record(player::DiscSize::k30cm, player::Provenance::kReported);
  disc.disc_side.Record(2, player::Provenance::kReported);

  const QString report =
      DiscProfileReport(disc, player::ExamineOutcome::kCompleted, 0.0);

  // Inches, because that is what a disc is sold as, however the format is
  // specified.
  EXPECT_TRUE(report.contains(
      QStringLiteral("Size: 12 inch  (reported by the player)")));
  EXPECT_TRUE(report.contains(
      QStringLiteral("Side: side 2  (reported by the player)")));
}

TEST(PlayerTextTest, ASideThePlayerCouldNotDetermineIsNotReportedAsSideOne) {
  const QString report = DiscProfileReport(
      ExaminedCavDisc(), player::ExamineOutcome::kCompleted, 0.0);

  EXPECT_TRUE(
      report.contains(QStringLiteral("Side: not known  (not established)")));
  EXPECT_FALSE(report.contains(QStringLiteral("Side: side 1")));
}

TEST(PlayerTextTest, BothDiscDiametersAreNamed) {
  EXPECT_EQ(DiscSizeName(player::DiscSize::k30cm), QStringLiteral("12 inch"));
  EXPECT_EQ(DiscSizeName(player::DiscSize::k20cm), QStringLiteral("8 inch"));
  EXPECT_NE(DiscSizeName(player::DiscSize::kUnknown),
            DiscSizeName(player::DiscSize::k30cm));
}

TEST(PlayerTextTest, AProfileWithNothingInItStillProducesAReadableReport) {
  const player::DiscProfile nothing;

  for (const player::ExamineOutcome outcome :
       {player::ExamineOutcome::kTrayOpen, player::ExamineOutcome::kNoDisc,
        player::ExamineOutcome::kLinkFailed,
        player::ExamineOutcome::kCancelled}) {
    const QString report = DiscProfileReport(nothing, outcome, 1.0e6);
    EXPECT_FALSE(report.isEmpty());
    EXPECT_FALSE(report.contains(QStringLiteral(": \n")))
        << "a blank field would read as a bug in the report";
  }
}

TEST(PlayerTextTest, AnExamineTraceLineNamesTheStepAsWellAsTheBytes) {
  // Two address queries in one trace, and without the step name they would be
  // indistinguishable.
  const QString line =
      ExamineStepText(player::ExamineStage::kReadingEnd, QStringLiteral("?F"),
                      QStringLiteral("054000"));

  EXPECT_TRUE(line.contains(QStringLiteral("?F")));
  EXPECT_TRUE(line.contains(QStringLiteral("054000")));
  EXPECT_TRUE(
      line.contains(ExamineStageName(player::ExamineStage::kReadingEnd)));

  const QString silent = ExamineStepText(player::ExamineStage::kReadingEnd,
                                         QStringLiteral("?F"), QString());
  EXPECT_TRUE(silent.contains(QStringLiteral("no answer")));
}

// --- The automatic capture -------------------------------------------------

TEST(PlayerTextTest, EveryAutoCaptureStageHasItsOwnWording) {
  QStringList seen;
  for (int value = 0;
       value <= static_cast<int>(player::AutoCaptureStage::kFinished);
       ++value) {
    const QString name =
        AutoCaptureStageName(static_cast<player::AutoCaptureStage>(value));
    EXPECT_FALSE(name.isEmpty());
    EXPECT_FALSE(seen.contains(name)) << name.toStdString();
    seen << name;
  }

  // The one nobody would arrive at by reading the code that does it, so the
  // progress line explains itself: a user watching their disc stop when they
  // asked for a capture is entitled to know why.
  EXPECT_TRUE(AutoCaptureStageName(player::AutoCaptureStage::kSpinningDown)
                  .contains(QStringLiteral("spin-up")));
}

TEST(PlayerTextTest, EveryAutoCaptureOutcomeHasItsOwnWordingAndItsOwnSentence) {
  QStringList clauses;
  QStringList sentences;

  for (int value = 0;
       value <= static_cast<int>(player::AutoCaptureOutcome::kCancelled);
       ++value) {
    const auto outcome = static_cast<player::AutoCaptureOutcome>(value);

    const QString clause = AutoCaptureOutcomeText(outcome);
    EXPECT_FALSE(clause.isEmpty());
    EXPECT_FALSE(clauses.contains(clause)) << clause.toStdString();
    clauses << clause;

    const QString sentence = AutoCaptureSummary(outcome);
    EXPECT_FALSE(sentence.isEmpty());
    EXPECT_FALSE(sentences.contains(sentence)) << sentence.toStdString();
    sentences << sentence;
  }

  // The one branch that ends with a file still growing has to say so — the
  // whole reason that branch is allowed to exist is that the user is told.
  EXPECT_TRUE(AutoCaptureSummary(player::AutoCaptureOutcome::kLinkFailed)
                  .contains(QStringLiteral("still running")));
}

TEST(PlayerTextTest, EveryPlanProblemGetsItsOwnSentenceAndNoneIsSilent) {
  EXPECT_TRUE(PlanProblemText(player::PlanProblem::kNone).isEmpty());

  QStringList seen;
  for (int value = 1;
       value <= static_cast<int>(player::PlanProblem::kEndBeyondProgramme);
       ++value) {
    const QString text =
        PlanProblemText(static_cast<player::PlanProblem>(value));

    // The old application had one message for all of them, arriving several
    // seconds after the disc had started spinning.
    EXPECT_FALSE(text.isEmpty());
    EXPECT_FALSE(seen.contains(text)) << text.toStdString();
    seen << text;
  }
}

TEST(PlayerTextTest, TheEstimateNeedsTheStandardOnACavDisc) {
  player::DiscProfile disc;
  disc.disc_type.Record(player::DiscType::kCav, player::Provenance::kReported);
  disc.programme_start.Record(1, player::Provenance::kMeasured);
  disc.programme_end.Record(54000, player::Provenance::kMeasured);

  const player::AutoCapturePlan plan = player::DefaultPlanFor(disc);

  // A frame count is only a duration once the frame rate is known, and
  // assuming thirty a second on a PAL disc is twenty per cent out — in the
  // direction that fills the volume a capture was estimated to fit on.
  EXPECT_TRUE(AutoCaptureEstimate(plan, disc, 40.0e6).isEmpty());

  disc.video_standard.Record(player::VideoStandard::kNtsc,
                             player::Provenance::kReported);
  const QString estimate = AutoCaptureEstimate(plan, disc, 40.0e6);
  EXPECT_FALSE(estimate.isEmpty());
  EXPECT_TRUE(estimate.contains(QStringLiteral("disk")));
}

TEST(PlayerTextTest, TheTimeLeftIsThePlayingTimeLeftAndNeedsNoClock) {
  player::DiscProfile disc;
  disc.disc_type.Record(player::DiscType::kCav, player::Provenance::kReported);
  disc.programme_start.Record(1, player::Provenance::kMeasured);
  disc.programme_end.Record(54000, player::Provenance::kMeasured);
  disc.video_standard.Record(player::VideoStandard::kNtsc,
                             player::Provenance::kReported);

  const player::AutoCapturePlan plan = player::DefaultPlanFor(disc);

  // 1800 frames from the end at 30000/1001 is a minute of disc left, which is a
  // minute of waiting left — the disc plays in real time, so there is nothing
  // to measure and no rate to settle down.
  const QString remaining = AutoCaptureRemainingText(plan, disc, 54000 - 1800);
  EXPECT_FALSE(remaining.isEmpty());
  EXPECT_TRUE(remaining.contains(QStringLiteral("1:00")));

  // Nothing once the end has been reached, rather than a countdown that sits
  // at zero through the spin-down and the file being closed.
  EXPECT_TRUE(AutoCaptureRemainingText(plan, disc, 54000).isEmpty());
  EXPECT_TRUE(AutoCaptureRemainingText(plan, disc, -1).isEmpty());
}

TEST(PlayerTextTest, TheTimeLeftOnACavDiscNeedsTheVideoStandardToo) {
  player::DiscProfile disc;
  disc.disc_type.Record(player::DiscType::kCav, player::Provenance::kReported);
  disc.programme_end.Record(54000, player::Provenance::kMeasured);

  const player::AutoCapturePlan plan = player::DefaultPlanFor(disc);

  // A frame count is only a duration once the frame rate is known. Saying
  // nothing beats a figure that would be a fifth out on a PAL disc.
  EXPECT_TRUE(AutoCaptureRemainingText(plan, disc, 27000).isEmpty());

  disc.video_standard.Record(player::VideoStandard::kPal,
                             player::Provenance::kDeclared);
  EXPECT_FALSE(AutoCaptureRemainingText(plan, disc, 27000).isEmpty());
}

TEST(PlayerTextTest, TheTimeLeftOnAClvDiscIsReadStraightOffTheTimeCodes) {
  player::DiscProfile disc;
  disc.disc_type.Record(player::DiscType::kClv, player::Provenance::kReported);
  disc.programme_start.Record(0, player::Provenance::kMeasured);
  disc.programme_end.Record(504500, player::Provenance::kMeasured);

  const player::AutoCapturePlan plan = player::DefaultPlanFor(disc);

  // 0:20:45 into a side that runs to 0:50:45: half an hour to go, and no video
  // standard needed for it because the addresses are already times.
  const QString remaining = AutoCaptureRemainingText(plan, disc, 204500);
  EXPECT_TRUE(remaining.contains(QStringLiteral("30:00")));
}

TEST(PlayerTextTest, ASuggestedNameCarriesTheSideAndNothingIsInventedForIt) {
  player::DiscProfile disc;
  EXPECT_TRUE(SuggestedCaptureName(disc).isEmpty());

  disc.disc_type.Record(player::DiscType::kClv, player::Provenance::kReported);
  disc.disc_side.Record(2, player::Provenance::kReported);

  const QString name = SuggestedCaptureName(disc);
  EXPECT_TRUE(name.contains(QStringLiteral("CLV")));
  EXPECT_TRUE(name.contains(QStringLiteral("Side2")));
}

}  // namespace
}  // namespace ddd::gui
