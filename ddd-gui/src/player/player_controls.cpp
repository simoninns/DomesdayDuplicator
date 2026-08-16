/************************************************************************

    player_controls.cpp

    What the connected player can actually be asked to do
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "player_controls.h"

#include <initializer_list>

#include "player_capabilities.h"

namespace ddd::player {

PlayerControls ControlsFor(const PlayerDefinition& definition,
                           std::string_view firmware) {
  PlayerControls controls;

  // Start from what there is something to send for.
  for (size_t index = 0; index < kPlayerCommandCount; ++index) {
    controls.commands[index] = definition.commands[index].present();
  }

  // Then take away what the model says it does not have. This direction round
  // matters: the capability flags all default to true, so a definition that
  // simply inherits the shared command set gets the whole of it, and one that
  // has to say what it lacks says it once.
  const PlayerCapabilities& able = definition.capabilities;

  const auto withhold =
      [&controls](bool allowed, std::initializer_list<PlayerCommand> which) {
        if (allowed) {
          return;
        }
        for (const PlayerCommand command : which) {
          controls.commands[Index(command)] = false;
        }
      };

  withhold(able.tray_control,
           {PlayerCommand::kTrayOpen, PlayerCommand::kTrayClose});
  withhold(able.stop_codes_disabled_play,
           {PlayerCommand::kPlayWithoutStopCodes});
  withhold(able.step,
           {PlayerCommand::kStepForward, PlayerCommand::kStepReverse});
  withhold(able.scan,
           {PlayerCommand::kScanForward, PlayerCommand::kScanReverse});
  withhold(able.multi_speed, {PlayerCommand::kMultiSpeedForward,
                              PlayerCommand::kMultiSpeedReverse});
  withhold(able.speed_selection, {PlayerCommand::kSetSpeed});
  withhold(able.frame_search, {PlayerCommand::kSeekFrame});
  withhold(able.time_code_search, {PlayerCommand::kSeekTimeCode});
  withhold(able.chapter_search, {PlayerCommand::kSeekChapter});
  withhold(able.on_screen_display,
           {PlayerCommand::kDisplayOn, PlayerCommand::kDisplayOff});
  withhold(able.audio_selection, {PlayerCommand::kSetAudio});
  withhold(able.key_lock,
           {PlayerCommand::kKeyLockOn, PlayerCommand::kKeyLockOff});
  withhold(able.standard_user_code, {PlayerCommand::kQueryStandardUserCode});
  withhold(able.pioneer_user_code, {PlayerCommand::kQueryPioneerUserCode});

  // The one control whose availability is not a property of the model alone.
  if (!SupportsPhysicalPosition(definition, firmware)) {
    controls.commands[Index(PlayerCommand::kQueryPhysicalPosition)] = false;
  }

  // A mode with no wire parameter is a mode this model cannot be put into, even
  // where the command that would do it exists — one command with a parameter
  // table is how both of these work, so the table is where the answer is.
  const bool audio = controls.Has(PlayerCommand::kSetAudio);
  for (size_t index = 0; index < kAudioModeCount; ++index) {
    controls.audio_modes[index] =
        audio && definition.audio_parameters[index] != kParameterUnsupported;
  }

  if (!able.digital_audio) {
    for (const AudioMode mode :
         {AudioMode::kDigitalChannel1, AudioMode::kDigitalChannel2,
          AudioMode::kDigitalStereo}) {
      controls.audio_modes[static_cast<size_t>(mode)] = false;
    }
  }

  const bool speed = controls.Has(PlayerCommand::kSetSpeed);
  for (size_t index = 0; index < kPlaybackSpeedCount; ++index) {
    controls.speeds[index] =
        speed && definition.speed_parameters[index] != kParameterUnsupported;
  }

  return controls;
}

}  // namespace ddd::player
