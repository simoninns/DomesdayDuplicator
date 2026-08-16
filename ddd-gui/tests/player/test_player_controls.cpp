/************************************************************************

    test_player_controls.cpp

    T1 tests for what a connected player can be asked to do
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include "player_controls.h"
#include "player_registry.h"
#include "players/pioneer_level_iii.h"

namespace ddd::player {
namespace {

const PlayerDefinition& Model(std::string_view id_code) {
  const PlayerDefinition* const found = FindPlayerByIdCode(id_code);
  EXPECT_NE(found, nullptr);
  return found != nullptr ? *found : GenericPlayer();
}

TEST(PlayerControlsTest, NothingIsOfferedForNoPlayerAtAll) {
  // The property the remote leans on: a default-constructed value gates every
  // control off, so a dialog with nothing connected needs no separate test for
  // whether there is a player.
  const PlayerControls controls;

  EXPECT_FALSE(controls.any());
  EXPECT_FALSE(controls.Has(PlayerCommand::kPlay));
  EXPECT_FALSE(controls.Has(AudioMode::kAnalogStereo));
  EXPECT_FALSE(controls.Has(PlaybackSpeed::kNormal));
}

TEST(PlayerControlsTest, TheSentinelIsAnswerdRatherThanIndexedWith) {
  // Each of these enumerations ends in a kCount that is an ordinary value of
  // its type, so it can be passed here by something that has gone wrong further
  // up. The answer is "no", not whatever byte follows the array.
  const PlayerControls controls = ControlsFor(Model("15"), "A1");

  EXPECT_FALSE(controls.Has(PlayerCommand::kCount));
  EXPECT_FALSE(controls.Has(AudioMode::kCount));
  EXPECT_FALSE(controls.Has(PlaybackSpeed::kCount));
}

TEST(PlayerControlsTest, ThePioneerSetOffersTheWholeTransport) {
  const PlayerControls controls = ControlsFor(Model("15"), "A1");

  EXPECT_TRUE(controls.any());
  for (const PlayerCommand command :
       {PlayerCommand::kPlay, PlayerCommand::kPause, PlayerCommand::kStillFrame,
        PlayerCommand::kStop, PlayerCommand::kStepForward,
        PlayerCommand::kScanReverse, PlayerCommand::kMultiSpeedForward,
        PlayerCommand::kSeekFrame, PlayerCommand::kSeekTimeCode,
        PlayerCommand::kSeekChapter, PlayerCommand::kKeyLockOn,
        PlayerCommand::kQueryPioneerUserCode}) {
    EXPECT_TRUE(controls.Has(command))
        << "missing command index " << Index(command);
  }
}

TEST(PlayerControlsTest, TheCommandTableDecidesWhatIsOffered) {
  // A definition with nothing to send for a control does not offer it, however
  // the capability flags read — this is the half that stops a button existing
  // for a sequence nobody wrote.
  PlayerDefinition definition = pioneer::LevelIII();
  definition.name = "Test player";
  definition.capabilities.step = false;
  definition.commands[Index(PlayerCommand::kStepForward)] = CommandSpec{};
  definition.commands[Index(PlayerCommand::kStepReverse)] = CommandSpec{};

  const PlayerControls controls = ControlsFor(definition, "A1");

  EXPECT_FALSE(controls.Has(PlayerCommand::kStepForward));
  EXPECT_FALSE(controls.Has(PlayerCommand::kStepReverse));
  EXPECT_TRUE(controls.Has(PlayerCommand::kPlay));
}

TEST(PlayerControlsTest, ADeclaredLackWinsOverAnInheritedCommand) {
  // The case that matters, and the reason both halves of the schema are
  // consulted: a model inherits the shared command set and then says it does
  // not have scan. There is a sequence to send, and it must not be offered —
  // otherwise the declaration is decoration and the user gets a button that
  // appears to work.
  PlayerDefinition definition = pioneer::LevelIII();
  definition.name = "Test player";
  definition.capabilities.scan = false;

  const PlayerControls controls = ControlsFor(definition, "A1");

  EXPECT_FALSE(controls.Has(PlayerCommand::kScanForward));
  EXPECT_FALSE(controls.Has(PlayerCommand::kScanReverse));
  EXPECT_TRUE(controls.Has(PlayerCommand::kStepForward));
}

TEST(PlayerControlsTest, AModeWithNoWireParameterIsNotOffered) {
  // Audio and speed are one command each with a per-model parameter table, so
  // "can it" is a question about the table rather than about the command.
  PlayerDefinition definition = pioneer::LevelIII();
  definition.name = "Test player";
  definition.audio_parameters[static_cast<size_t>(AudioMode::kDigitalStereo)] =
      kParameterUnsupported;
  definition.speed_parameters[static_cast<size_t>(PlaybackSpeed::kQuadruple)] =
      kParameterUnsupported;

  const PlayerControls controls = ControlsFor(definition, "A1");

  EXPECT_TRUE(controls.Has(PlayerCommand::kSetAudio));
  EXPECT_FALSE(controls.Has(AudioMode::kDigitalStereo));
  EXPECT_TRUE(controls.Has(AudioMode::kAnalogStereo));

  EXPECT_FALSE(controls.Has(PlaybackSpeed::kQuadruple));
  EXPECT_TRUE(controls.Has(PlaybackSpeed::kNormal));
}

TEST(PlayerControlsTest, APlayerWithNoDigitalAudioIsOfferedNone) {
  PlayerDefinition definition = pioneer::LevelIII();
  definition.name = "Test player";
  definition.capabilities.digital_audio = false;

  const PlayerControls controls = ControlsFor(definition, "A1");

  EXPECT_FALSE(controls.Has(AudioMode::kDigitalChannel1));
  EXPECT_FALSE(controls.Has(AudioMode::kDigitalChannel2));
  EXPECT_FALSE(controls.Has(AudioMode::kDigitalStereo));
  EXPECT_TRUE(controls.Has(AudioMode::kAnalogStereo));
  EXPECT_TRUE(controls.Has(AudioMode::kOff));
}

TEST(PlayerControlsTest, NoAudioSelectionMeansNoModes) {
  PlayerDefinition definition = pioneer::LevelIII();
  definition.name = "Test player";
  definition.capabilities.audio_selection = false;

  const PlayerControls controls = ControlsFor(definition, "A1");

  EXPECT_FALSE(controls.Has(PlayerCommand::kSetAudio));
  for (size_t index = 0; index < kAudioModeCount; ++index) {
    EXPECT_FALSE(controls.Has(static_cast<AudioMode>(index)));
  }
}

TEST(PlayerControlsTest, TheFirmwareDecidesWhereTheModelCannotAlone) {
  // The LD-V8000 reports the optical assembly's position from firmware A9
  // onwards, so the answer is not a property of the model. Resolving it here is
  // what stops every caller having to remember that.
  const PlayerDefinition& v8000 = Model("06");

  EXPECT_TRUE(
      ControlsFor(v8000, "A9").Has(PlayerCommand::kQueryPhysicalPosition));
  EXPECT_FALSE(
      ControlsFor(v8000, "A1").Has(PlayerCommand::kQueryPhysicalPosition));

  // A player that would not say which firmware it runs does not get a control
  // that depends on knowing.
  EXPECT_FALSE(
      ControlsFor(v8000, "").Has(PlayerCommand::kQueryPhysicalPosition));

  // And a model that never had it does not acquire it by running A9.
  EXPECT_FALSE(ControlsFor(Model("15"), "A9")
                   .Has(PlayerCommand::kQueryPhysicalPosition));
}

TEST(PlayerControlsTest, EveryRegisteredModelOffersWhatTheApplicationNeeds) {
  // A sweep rather than a case each: the point of the registry is that adding a
  // model is a header and a line, and this is what makes that safe. Every model
  // has to be drivable — transport, both searches, and the three queries the
  // status poll makes — or the remote and the automatic capture have a hole in
  // them that nothing else would find.
  for (const PlayerDefinition* definition : RegisteredPlayers()) {
    const PlayerControls controls =
        ControlsFor(*definition, definition->physical_position_firmware);

    for (const PlayerCommand command :
         {PlayerCommand::kPlay, PlayerCommand::kPause, PlayerCommand::kStop,
          PlayerCommand::kSeekFrame, PlayerCommand::kSeekTimeCode,
          PlayerCommand::kQueryActiveMode, PlayerCommand::kQueryAddress,
          PlayerCommand::kQueryDiscStatus}) {
      EXPECT_TRUE(controls.Has(command))
          << definition->name << " cannot be asked for command index "
          << Index(command);
    }

    EXPECT_TRUE(controls.Has(PlaybackSpeed::kNormal)) << definition->name;
    EXPECT_TRUE(controls.Has(AudioMode::kOff)) << definition->name;
  }
}

TEST(PlayerControlsTest, TheGenericFallbackIsFullyDrivable) {
  // The player nothing recognises still gets the whole Pioneer set, because
  // that is the best available guess and a usable one — and because the manual
  // command field alone would be a poor way to work a disc.
  const PlayerControls controls = ControlsFor(GenericPlayer(), "");

  EXPECT_TRUE(controls.any());
  EXPECT_TRUE(controls.Has(PlayerCommand::kPlay));
  EXPECT_TRUE(controls.Has(PlayerCommand::kSeekFrame));
}

}  // namespace
}  // namespace ddd::player
