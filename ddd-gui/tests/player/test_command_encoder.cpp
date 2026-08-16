/************************************************************************

    test_command_encoder.cpp

    T1 tests pinning the bytes every command puts on the wire
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "command_encoder.h"
#include "player_command.h"
#include "player_definition.h"
#include "player_registry.h"
#include "players/pioneer_ld_v4300d.h"
#include "players/pioneer_ld_v8000.h"

namespace ddd::player {
namespace {

// The exact bytes the previous capture application sent, command by command.
//
// This is the test that proves the port did not change the wire protocol. Every
// sequence here was read out of
// gui/src/DomesdayDuplicator/playercommunication.cpp, which is what years of
// field captures were taken with — so a change to any of these is a change to
// behaviour known to work, and has to be a deliberate one with a player on the
// bench behind it.
struct ExpectedCommand {
  PlayerCommand command;
  std::string_view bytes;
};

constexpr ExpectedCommand kLevelIIIWireFormat[] = {
    // The tray. Note which is which: the old application's comments have these
    // two transposed, and the commands rather than the comments are what its
    // players responded to.
    {PlayerCommand::kTrayOpen, "OP\r"},
    {PlayerCommand::kTrayClose, "CO\r"},

    {PlayerCommand::kPlay, "PL\r"},
    {PlayerCommand::kPlayWithoutStopCodes, "PL64RBMF\r"},
    {PlayerCommand::kPause, "PA\r"},
    {PlayerCommand::kStillFrame, "ST\r"},
    {PlayerCommand::kStop, "RJ\r"},

    {PlayerCommand::kStepForward, "SF\r"},
    {PlayerCommand::kStepReverse, "SR\r"},
    {PlayerCommand::kScanForward, "NF\r"},
    {PlayerCommand::kScanReverse, "NR\r"},
    {PlayerCommand::kMultiSpeedForward, "MF\r"},
    {PlayerCommand::kMultiSpeedReverse, "MB\r"},

    {PlayerCommand::kDisplayOn, "1DS\r"},
    {PlayerCommand::kDisplayOff, "0DS\r"},
    {PlayerCommand::kKeyLockOn, "1KL\r"},
    {PlayerCommand::kKeyLockOff, "0KL\r"},

    {PlayerCommand::kQueryActiveMode, "?P\r"},
    {PlayerCommand::kQueryAddress, "?F\r"},
    {PlayerCommand::kQueryDiscStatus, "?D\r"},
    {PlayerCommand::kQueryStandardUserCode, "$Y\r"},
    {PlayerCommand::kQueryPioneerUserCode, "?U\r"},
};

TEST(CommandEncoderTest, EveryPlayerSendsTheSameBytesForTheSharedCommands) {
  for (const PlayerDefinition* definition : RegisteredPlayers()) {
    for (const ExpectedCommand& expected : kLevelIIIWireFormat) {
      const EncodedCommand encoded =
          EncodeCommand(*definition, expected.command);
      ASSERT_TRUE(encoded.ok()) << definition->name << " cannot encode command "
                                << static_cast<int>(expected.command);
      EXPECT_EQ(encoded.bytes, expected.bytes) << definition->name;
    }
  }
}

TEST(CommandEncoderTest, TheGenericPlayerSendsTheSameBytesToo) {
  // An unrecognised player is driven with the shared set, so it has to encode
  // it. This is what makes "your player is not one I know, here is the remote
  // anyway" an honest offer.
  for (const ExpectedCommand& expected : kLevelIIIWireFormat) {
    const EncodedCommand encoded =
        EncodeCommand(GenericPlayer(), expected.command);
    ASSERT_TRUE(encoded.ok());
    EXPECT_EQ(encoded.bytes, expected.bytes);
  }
}

TEST(CommandEncoderTest, AddressesAreDecimalAndUnpadded) {
  const PlayerDefinition& player = pioneer::kLdV4300D;

  // Unpadded is what the old application sent: "FR1SE", not "FR00001SE".
  EXPECT_EQ(EncodeCommand(player, PlayerCommand::kSeekFrame, 1).bytes,
            "FR1SE\r");

  // The impossible frame the disc-length measurement seeks to.
  EXPECT_EQ(EncodeCommand(player, PlayerCommand::kSeekFrame, 60000).bytes,
            "FR60000SE\r");

  // A time code is the same command with a wider address — the Pioneer format,
  // not an oversight. This is the impossible time code the CLV length
  // measurement seeks to.
  EXPECT_EQ(EncodeCommand(player, PlayerCommand::kSeekTimeCode, 1595900).bytes,
            "FR1595900SE\r");

  EXPECT_EQ(EncodeCommand(player, PlayerCommand::kSeekChapter, 12).bytes,
            "CH12SE\r");
}

TEST(CommandEncoderTest, TheArgumentCanComeFirst) {
  // "1AD" and "4SP" put the parameter before the mnemonic, which is why the
  // spec has a prefix and a suffix rather than a command and an argument.
  const PlayerDefinition& player = pioneer::kLdV4300D;

  EXPECT_EQ(EncodeAudio(player, AudioMode::kOff).bytes, "0AD\r");
  EXPECT_EQ(EncodeAudio(player, AudioMode::kAnalogChannel1).bytes, "1AD\r");
  EXPECT_EQ(EncodeAudio(player, AudioMode::kAnalogChannel2).bytes, "2AD\r");
  EXPECT_EQ(EncodeAudio(player, AudioMode::kAnalogStereo).bytes, "3AD\r");

  // 4 is skipped: the digital modes are 5, 6 and 7, and the old application
  // never sent 4. The parameter table is per-model data precisely so that gap
  // is described rather than computed.
  EXPECT_EQ(EncodeAudio(player, AudioMode::kDigitalChannel1).bytes, "5AD\r");
  EXPECT_EQ(EncodeAudio(player, AudioMode::kDigitalChannel2).bytes, "6AD\r");
  EXPECT_EQ(EncodeAudio(player, AudioMode::kDigitalStereo).bytes, "7AD\r");

  EXPECT_EQ(EncodeSpeed(player, PlaybackSpeed::kSixth).bytes, "0SP\r");
  EXPECT_EQ(EncodeSpeed(player, PlaybackSpeed::kNormal).bytes, "4SP\r");
  EXPECT_EQ(EncodeSpeed(player, PlaybackSpeed::kQuadruple).bytes, "7SP\r");
}

TEST(CommandEncoderTest, OnlyTheModelWithThePositionCommandCanSendIt) {
  EXPECT_EQ(
      EncodeCommand(pioneer::kLdV8000, PlayerCommand::kQueryPhysicalPosition)
          .bytes,
      "2962MQ\r");

  const EncodedCommand refused =
      EncodeCommand(pioneer::kLdV4300D, PlayerCommand::kQueryPhysicalPosition);
  EXPECT_FALSE(refused.ok());
  EXPECT_EQ(refused.status, EncodeStatus::kCommandUnsupported);
  EXPECT_TRUE(refused.bytes.empty());
}

TEST(CommandEncoderTest, AnAddressThatIsNotAnAddressIsRefused) {
  const PlayerDefinition& player = pioneer::kLdV4300D;

  // Negative is not a position on a disc.
  EXPECT_EQ(EncodeCommand(player, PlayerCommand::kSeekFrame, -1).status,
            EncodeStatus::kArgumentOutOfRange);

  // Wider than the format allows. Refused rather than truncated: truncation
  // would send a valid command naming a different frame, and there would be
  // nothing afterwards to show that had happened.
  EXPECT_EQ(EncodeCommand(player, PlayerCommand::kSeekFrame, 100000).status,
            EncodeStatus::kArgumentOutOfRange);

  // The widest each address may be.
  EXPECT_TRUE(EncodeCommand(player, PlayerCommand::kSeekFrame, 99999).ok());
  EXPECT_TRUE(
      EncodeCommand(player, PlayerCommand::kSeekTimeCode, 9999999).ok());
  EXPECT_EQ(
      EncodeCommand(player, PlayerCommand::kSeekTimeCode, 10000000).status,
      EncodeStatus::kArgumentOutOfRange);
}

TEST(CommandEncoderTest, AMissingOrSpuriousArgumentIsRefused) {
  const PlayerDefinition& player = pioneer::kLdV4300D;

  // A seek with nowhere to seek to.
  EXPECT_EQ(EncodeCommand(player, PlayerCommand::kSeekFrame).status,
            EncodeStatus::kArgumentRequired);

  // A play with a frame number is somebody's mistake further up, and running it
  // as an ordinary play would hide that.
  EXPECT_EQ(EncodeCommand(player, PlayerCommand::kPlay, 100).status,
            EncodeStatus::kUnexpectedArgument);
}

TEST(CommandEncoderTest, AModeTheModelHasNoParameterForIsRefused) {
  PlayerDefinition player = pioneer::kLdV4300D;
  player.capabilities.digital_audio = false;
  player.audio_parameters[static_cast<size_t>(AudioMode::kDigitalStereo)] =
      kParameterUnsupported;

  const EncodedCommand refused = EncodeAudio(player, AudioMode::kDigitalStereo);
  EXPECT_FALSE(refused.ok());
  EXPECT_EQ(refused.status, EncodeStatus::kParameterUnsupported);

  // The analogue modes it does have still work.
  EXPECT_TRUE(EncodeAudio(player, AudioMode::kAnalogStereo).ok());
}

TEST(CommandEncoderTest, ACommandTooLongForAPlayerIsRefused) {
  // Twenty characters is what a player accepts, and the old application
  // truncated to it on the way out — turning an over-long command into a
  // different, valid one. Nothing in the registry can produce this (the
  // consistency check rejects it at compile time), so it is provoked here.
  PlayerDefinition player = pioneer::kLdV4300D;
  player.commands[Index(PlayerCommand::kSeekFrame)].prefix =
      "AVERYLONGPREFIXINDEED";

  const EncodedCommand refused =
      EncodeCommand(player, PlayerCommand::kSeekFrame, 1);
  EXPECT_FALSE(refused.ok());
  EXPECT_EQ(refused.status, EncodeStatus::kCommandTooLong);
  EXPECT_TRUE(refused.bytes.empty());
}

TEST(CommandEncoderTest, EveryCommandAModelClaimsCanActuallyBeEncoded) {
  // The sweep that catches a definition whose command table and argument widths
  // disagree — a spec taking a five-digit argument that no value can satisfy,
  // say. Every present command is encoded with an argument if it wants one.
  for (const PlayerDefinition* definition : RegisteredPlayers()) {
    for (size_t index = 0; index < kPlayerCommandCount; ++index) {
      const PlayerCommand command = static_cast<PlayerCommand>(index);
      const CommandSpec& spec = Spec(*definition, command);
      if (!spec.present()) {
        continue;
      }

      const EncodedCommand encoded =
          spec.takes_argument() ? EncodeCommand(*definition, command, 1)
                                : EncodeCommand(*definition, command);

      EXPECT_TRUE(encoded.ok())
          << definition->name << " command " << index << " status "
          << static_cast<int>(encoded.status);
      EXPECT_LE(encoded.bytes.size(), kMaximumCommandLength);
      EXPECT_EQ(encoded.bytes.back(), kCommandTerminator);
    }
  }
}

}  // namespace
}  // namespace ddd::player
