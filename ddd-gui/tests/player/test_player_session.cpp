/************************************************************************

    test_player_session.cpp

    T1 tests for finding, identifying and driving a player
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "fake_serial_port.h"
#include "player_registry.h"
#include "player_session.h"
#include "players/pioneer_ld_v4300d.h"
#include "players/pioneer_ld_v8000.h"

namespace ddd::player {
namespace {

// An LD-V8000 running firmware A9 — the one model with a capability gated on
// its firmware, so it is the useful default to answer as.
constexpr const char* kLdV8000Reply = "P1506A9";

// An LD-V4300D, for the cases that want a player without that capability.
constexpr const char* kLdV4300DReply = "P1515A1";

class PlayerSessionTest : public testing::Test {
 protected:
  // The session is driven by the fake port's clock, so a command that times out
  // does so in microseconds rather than in five seconds — and does so at
  // exactly the moment the timeout says it should.
  FakeSerialPort port_;
  PlayerSession session_{&port_, port_.clock()};

  void ConnectAt(uint32_t baud_rate, const char* reply = kLdV8000Reply) {
    port_.AddPioneerPlayer(baud_rate, reply);
    const ProbeResult result = session_.Probe("/dev/ttyUSB0", baud_rate);
    ASSERT_TRUE(result.connected());
  }
};

TEST_F(PlayerSessionTest, APlayerIsFoundAtEveryRateItMightBeSetTo) {
  // A player's baud rate is a DIP switch on its back panel, and a user who has
  // never opened the application has no idea which way it is set. Each of the
  // four is found without being told.
  for (const uint32_t rate : {9600U, 4800U, 2400U, 1200U}) {
    FakeSerialPort port;
    PlayerSession session(&port, port.clock());
    port.AddPioneerPlayer(rate, kLdV8000Reply);

    const ProbeResult result = session.Probe("/dev/ttyUSB0");

    ASSERT_TRUE(result.connected()) << "not found at " << rate;
    EXPECT_EQ(result.baud_rate, rate);
    EXPECT_EQ(session.baud_rate(), rate);
    EXPECT_EQ(session.port_path(), "/dev/ttyUSB0");
    EXPECT_TRUE(session.connected());
  }
}

TEST_F(PlayerSessionTest, TheModelAndItsFirmwareAreReadOutOfTheReply) {
  ConnectAt(9600);

  const PlayerIdentity& identity = session_.identity();
  EXPECT_EQ(identity.model_code, "P1506A9");
  EXPECT_EQ(identity.id_code, "06");
  EXPECT_EQ(identity.firmware_version, "A9");
  EXPECT_TRUE(identity.recognised);
  EXPECT_EQ(identity.definition, &pioneer::kLdV8000);

  // The capability that is gated on that firmware rather than on the model.
  EXPECT_TRUE(session_.SupportsPhysicalPosition());
}

TEST_F(PlayerSessionTest, AnUnrecognisedModelStillConnects) {
  // A real player of a model no definition claims. It gets the generic command
  // set and is reported as unrecognised — which is both true and useful, and is
  // what lets somebody work out what it does and write its definition.
  port_.AddPioneerPlayer(9600, "P1544ZZ");

  const ProbeResult result = session_.Probe("/dev/ttyUSB0", 9600);

  ASSERT_TRUE(result.connected());
  EXPECT_FALSE(result.identity.recognised);
  EXPECT_EQ(result.identity.id_code, "44");
  EXPECT_EQ(result.identity.definition, &GenericPlayer());
  EXPECT_TRUE(result.identity.definition->is_generic);
}

TEST_F(PlayerSessionTest, APlayerThatWillNotSayItsFirmwareIsStillIdentified) {
  // Terse but usable: the prefix and the model ID are enough to know which
  // player it is. Only the firmware-gated capabilities go away, which is the
  // right answer for a player that would not say.
  port_.AddPioneerPlayer(9600, "P1506");

  const ProbeResult result = session_.Probe("/dev/ttyUSB0", 9600);

  ASSERT_TRUE(result.connected());
  EXPECT_EQ(result.identity.id_code, "06");
  EXPECT_TRUE(result.identity.firmware_version.empty());
  EXPECT_FALSE(session_.SupportsPhysicalPosition());
}

TEST_F(PlayerSessionTest, SomethingThatIsNotAPlayerIsSaidToBeSomethingElse) {
  // Worth separating from silence: it usually means a serial device that is not
  // a player, and telling a user that is more use than "not found".
  port_.AddResponse(9600, "?X\r", "GARBAGE\r");

  const ProbeResult result = session_.Probe("/dev/ttyUSB0");

  EXPECT_EQ(result.status, ProbeResult::Status::kUnusableAnswer);
  EXPECT_EQ(result.unexpected_reply, "GARBAGE");
  EXPECT_EQ(result.baud_rate, 9600U);
  EXPECT_FALSE(session_.connected());
}

TEST_F(PlayerSessionTest, AReplyTooShortToIdentifyIsNotAPlayer) {
  // The prefix alone. There is no model ID in it, so there is nothing to
  // resolve — and resolving it to the generic definition would report a player
  // that has not actually been identified.
  port_.AddPioneerPlayer(9600, "P15");

  const ProbeResult result = session_.Probe("/dev/ttyUSB0", 9600);

  EXPECT_EQ(result.status, ProbeResult::Status::kUnusableAnswer);
  EXPECT_FALSE(session_.connected());
}

TEST_F(PlayerSessionTest, AnEmptyPortIsSilentRatherThanBroken) {
  const ProbeResult result = session_.Probe("/dev/ttyUSB0");

  EXPECT_EQ(result.status, ProbeResult::Status::kNoAnswer);
  EXPECT_FALSE(session_.connected());

  // Every rate was tried before giving up.
  EXPECT_EQ(port_.opened_rates(),
            (std::vector<uint32_t>{9600, 4800, 2400, 1200}));
}

TEST_F(PlayerSessionTest, APortThatWillNotOpenIsReportedAsSuch) {
  // Busy, absent, or not permitted — on Linux the last of those is the single
  // most likely first-run experience, and it is not the same as "no player".
  port_.set_open_fails(true);

  const ProbeResult result = session_.Probe("/dev/ttyUSB0");

  EXPECT_EQ(result.status, ProbeResult::Status::kPortUnavailable);
  EXPECT_FALSE(session_.connected());
}

TEST_F(PlayerSessionTest, AFixedRateIsNeverDepartedFrom) {
  // If the user says the player is at 1200, an application that quietly found
  // it at 9600 would leave them with a setting that does not describe their
  // hardware and a fault that reappears on the next machine.
  port_.AddPioneerPlayer(9600, kLdV8000Reply);

  const ProbeResult result = session_.Probe("/dev/ttyUSB0", 1200);

  EXPECT_EQ(result.status, ProbeResult::Status::kNoAnswer);
  EXPECT_EQ(port_.opened_rates(), (std::vector<uint32_t>{1200}));
}

TEST_F(PlayerSessionTest,
       AFixedRateIsTriedOnceAndSearchingIsTriedSeveralTimes) {
  // Searching is cheap per rate and patient once the rate is known: most of the
  // rates tried in a search are wrong, and being patient at each would be paid
  // for four times over.
  FakeSerialPort searching;
  PlayerSession search_session(&searching, searching.clock());
  search_session.Probe("/dev/ttyUSB0");
  EXPECT_EQ(searching.writes().size(), 12U);  // four rates, three attempts each

  FakeSerialPort fixed;
  PlayerSession fixed_session(&fixed, fixed.clock());
  fixed_session.Probe("/dev/ttyUSB0", 9600);
  EXPECT_EQ(fixed.writes().size(), 1U);
}

TEST_F(PlayerSessionTest, AnAnswerThatArrivesTooLateIsNoAnswer) {
  port_.AddPioneerPlayer(9600, kLdV8000Reply);
  port_.set_late_by_reads(1);

  const ProbeResult result = session_.Probe("/dev/ttyUSB0", 9600);

  EXPECT_EQ(result.status, ProbeResult::Status::kNoAnswer);
  EXPECT_FALSE(session_.connected());
  EXPECT_FALSE(port_.IsOpen());

  // And the session is left reusable rather than half-open, which is what makes
  // the automatic retry in the layer above safe.
  const ProbeResult retry = session_.Probe("/dev/ttyUSB0", 9600);
  EXPECT_TRUE(retry.connected());
}

TEST_F(PlayerSessionTest, AReplyArrivingInPiecesIsStillOneReply) {
  // A slow link delivers a reply a byte at a time. Reading once and giving up
  // would time out on a player that answered perfectly well.
  port_.set_chunk_size(1);
  port_.AddPioneerPlayer(9600, kLdV8000Reply);

  const ProbeResult result = session_.Probe("/dev/ttyUSB0", 9600);

  EXPECT_TRUE(result.connected());
  EXPECT_EQ(result.identity.id_code, "06");
}

TEST_F(PlayerSessionTest, ACommandIsSentAndItsAcknowledgementRead) {
  ConnectAt(9600);
  port_.AddResponse(9600, "PL\r", "R\r");

  const Reply reply = session_.Execute(PlayerCommand::kPlay);

  EXPECT_EQ(reply.status, ReplyStatus::kOk);
  EXPECT_EQ(port_.writes().back(), "PL\r");
}

TEST_F(PlayerSessionTest, EveryReplyCarriesTheBytesThatProvokedIt) {
  // What makes the log a serial trace rather than a summary of one, and what
  // the remote's manual command field echoes back. This is the only place the
  // bytes exist, so anything above rebuilding them for display would be a
  // second encoder that could disagree with the first.
  ConnectAt(9600);
  port_.AddResponse(9600, "FR100SE\r", "R\r");

  EXPECT_EQ(session_.Execute(PlayerCommand::kSeekFrame, 100).sent, "FR100SE\r");

  // Including the failures: "the link died sending FR100SE" is more use than
  // "the link died".
  port_.set_failing_read(2);
  EXPECT_EQ(session_.Execute(PlayerCommand::kPlay).sent, "PL\r");

  // And nothing where nothing was sent.
  EXPECT_TRUE(session_.Execute(PlayerCommand::kPlay).sent.empty());
}

TEST_F(PlayerSessionTest, ARefusalIsReportedWithItsCode) {
  ConnectAt(9600);
  port_.AddResponse(9600, "RJ\r", "E04\r");

  const Reply reply = session_.Execute(PlayerCommand::kStop);

  EXPECT_EQ(reply.status, ReplyStatus::kRefused);
  EXPECT_EQ(reply.error_code, "E04");

  // Still connected: the player answered, it just said no.
  EXPECT_TRUE(session_.connected());
}

TEST_F(PlayerSessionTest,
       ACommandThePlayerIgnoresTimesOutWithoutDisconnecting) {
  ConnectAt(9600);

  const Reply reply = session_.Execute(PlayerCommand::kPause);

  EXPECT_EQ(reply.status, ReplyStatus::kNoAnswer);
  EXPECT_TRUE(session_.connected());
}

TEST_F(PlayerSessionTest, ALinkThatDiesMidCommandIsADisconnection) {
  ConnectAt(9600);

  // The probe used one write, so the next one is the second.
  port_.set_failing_write(2);

  EXPECT_EQ(session_.Execute(PlayerCommand::kPlay).status,
            ReplyStatus::kLinkFailed);
  EXPECT_FALSE(session_.connected());

  // Reported once. Everything after it is "not connected", which is a state the
  // layer above already knows how to present, rather than a second failure.
  EXPECT_EQ(session_.Execute(PlayerCommand::kPlay).status,
            ReplyStatus::kNotConnected);
}

TEST_F(PlayerSessionTest, ALinkThatDiesWhileReadingIsAlsoADisconnection) {
  ConnectAt(9600);
  port_.set_failing_read(2);

  EXPECT_EQ(session_.Execute(PlayerCommand::kPlay).status,
            ReplyStatus::kLinkFailed);
  EXPECT_FALSE(session_.connected());
}

TEST_F(PlayerSessionTest, ACommandTheModelDoesNotHaveIsNeverSent) {
  ConnectAt(9600, kLdV4300DReply);
  const size_t writes_before = port_.writes().size();

  const Reply reply = session_.Execute(PlayerCommand::kQueryPhysicalPosition);

  EXPECT_EQ(reply.status, ReplyStatus::kUnsupported);
  EXPECT_EQ(port_.writes().size(), writes_before);
}

TEST_F(PlayerSessionTest, ACommandThatCannotBeBuiltIsNeverSent) {
  ConnectAt(9600);
  const size_t writes_before = port_.writes().size();

  // Wider than a frame address can be. Sending a truncated version would seek
  // to a different frame and look like it worked.
  EXPECT_EQ(session_.Execute(PlayerCommand::kSeekFrame, 100000).status,
            ReplyStatus::kInvalidArgument);

  // A seek with nowhere to seek to.
  EXPECT_EQ(session_.Execute(PlayerCommand::kSeekFrame).status,
            ReplyStatus::kInvalidArgument);

  EXPECT_EQ(port_.writes().size(), writes_before);
}

TEST_F(PlayerSessionTest, NothingIsSentBeforeThereIsAPlayerToSendItTo) {
  EXPECT_EQ(session_.Execute(PlayerCommand::kPlay).status,
            ReplyStatus::kNotConnected);
  EXPECT_EQ(session_.SetAudio(AudioMode::kAnalogStereo).status,
            ReplyStatus::kNotConnected);
  EXPECT_EQ(session_.SetSpeed(PlaybackSpeed::kNormal).status,
            ReplyStatus::kNotConnected);
  EXPECT_EQ(
      session_
          .SendRaw("PL", ResponseKind::kAcknowledgement, TimeoutClass::kNormal)
          .status,
      ReplyStatus::kNotConnected);
  EXPECT_TRUE(port_.writes().empty());
}

TEST_F(PlayerSessionTest, AudioAndSpeedGoThroughTheModelsParameterTables) {
  ConnectAt(9600);
  port_.AddResponse(9600, "7AD\r", "R\r");
  port_.AddResponse(9600, "4SP\r", "R\r");

  EXPECT_EQ(session_.SetAudio(AudioMode::kDigitalStereo).status,
            ReplyStatus::kOk);
  EXPECT_EQ(port_.writes().back(), "7AD\r");

  EXPECT_EQ(session_.SetSpeed(PlaybackSpeed::kNormal).status, ReplyStatus::kOk);
  EXPECT_EQ(port_.writes().back(), "4SP\r");
}

TEST_F(PlayerSessionTest, ARawCommandGoesThroughTheSamePathAndGetsATerminator) {
  // The manual command field, and the only way to find out what an unrecognised
  // player does.
  ConnectAt(9600);
  port_.AddResponse(9600, "1KL\r", "R\r");

  const Reply reply = session_.SendRaw("1KL", ResponseKind::kAcknowledgement,
                                       TimeoutClass::kNormal);

  EXPECT_EQ(reply.status, ReplyStatus::kOk);
  EXPECT_EQ(port_.writes().back(), "1KL\r");
}

TEST_F(PlayerSessionTest, ACommandThatAnswersTwiceIsWaitedOutInFull) {
  ConnectAt(9600);
  port_.AddResponse(9600, "?U\r", "FIRST\rSECOND\r");

  const Reply reply =
      session_.SendRaw("?U", ResponseKind::kText, TimeoutClass::kNormal, 2);

  EXPECT_EQ(reply.status, ReplyStatus::kOk);
  EXPECT_EQ(reply.text, "FIRST\rSECOND");
}

TEST_F(PlayerSessionTest, DisconnectingClosesThePortAndForgetsThePlayer) {
  ConnectAt(9600);

  session_.Disconnect();

  EXPECT_FALSE(session_.connected());
  EXPECT_FALSE(port_.IsOpen());
  EXPECT_EQ(port_.close_count(), 1);
  EXPECT_FALSE(session_.identity().valid());
  EXPECT_TRUE(session_.port_path().empty());
  EXPECT_EQ(session_.baud_rate(), 0U);
}

TEST_F(PlayerSessionTest, ProbingAgainDropsTheConnectionItAlreadyHad) {
  ConnectAt(9600);
  ASSERT_TRUE(session_.connected());

  port_.set_open_fails(true);
  const ProbeResult result = session_.Probe("/dev/ttyUSB1");

  EXPECT_EQ(result.status, ProbeResult::Status::kPortUnavailable);
  EXPECT_FALSE(session_.connected());
  EXPECT_TRUE(session_.port_path().empty());
}

}  // namespace
}  // namespace ddd::player
