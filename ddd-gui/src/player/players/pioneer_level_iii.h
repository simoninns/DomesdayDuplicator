/************************************************************************

    pioneer_level_iii.h

    The Pioneer Level III command set, shared by every Pioneer model
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <array>
#include <cstdint>

#include "player_command.h"
#include "player_definition.h"
#include "player_state.h"

// The command sequences below are the ones the previous capture application
// sent, carried over unchanged. They are field-proven across years of captures
// and this port deliberately does not improve them; where a comment there
// disagreed with the command beside it, the command won. (In particular: "CO"
// closes the tray and "OP" opens it, whatever gui/playercommunication.cpp's
// comments say.)
//
// Documented in the Pioneer *Level III User's Manual*, which covers the LD-V
// and CLD-V series as one command set. That is why this is a shared base rather
// than ten copies: the models differ in what they have, not in what to send.

namespace ddd::player::pioneer {

// The active-mode replies. Taken from the old application's ?P handling, with
// its two undocumented observations kept and labelled.
inline constexpr std::array<StateMapping, 12> kLevelIIIStates{{
    {"P00", PlayerState::kDoorOpen},
    {"P01", PlayerState::kParked},
    {"P02", PlayerState::kSettingUp},
    {"P03", PlayerState::kUnloading},
    {"P04", PlayerState::kPlaying},
    {"P05", PlayerState::kStillFrame},
    {"P06", PlayerState::kPaused},
    {"P07", PlayerState::kSearching},
    {"P08", PlayerState::kScanning},
    {"P09", PlayerState::kMultiSpeed},

    // Undocumented, and both were found by watching real players. P42 appears
    // during ordinary playback on some models; PA5 appears at the end of a
    // disc. The old application treated both as playing and nothing has been
    // observed to contradict that.
    {"P42", PlayerState::kPlaying},
    {"PA5", PlayerState::kPlaying},
}};

// Fastest first: the rate a player is most likely to be set to, and the rate
// that costs least to be wrong about.
inline constexpr std::array<uint32_t, 4> kLevelIIIBaudRates{9600, 4800, 2400,
                                                            1200};

// "?X" is the LVP Model Name Request. A reply is the fixed prefix "P15", a
// two-character model ID and a two-character firmware revision.
inline constexpr ProbeSpec kLevelIIIProbe{
    .request = "?X",
    .reply_prefix = "P15",
    .id_code_offset = 3,
    .id_code_length = 2,
    .firmware_offset = 5,
    .firmware_length = 2,
    .baud_rates = kLevelIIIBaudRates,
};

// The command set every Pioneer definition starts from. A model header calls
// this and overrides only what differs, so a reader of that header sees the
// deltas and nothing else.
constexpr PlayerDefinition LevelIII() {
  PlayerDefinition definition;

  definition.manufacturer = "Pioneer";
  definition.probe = &kLevelIIIProbe;

  definition.state_decode = StateDecode{
      .prefix = "P",
      .mappings = kLevelIIIStates,
  };

  definition.disc_status = DiscStatusDecode{
      .disc_type_index = 1,
      .cav_digit = '0',
      .clv_digit = '1',
  };

  // "<n>AD". 4 is absent from the sequence deliberately: the old application
  // never sent it and this port is not the place to find out what it does.
  definition.audio_parameters = {0, 1, 2, 3, 5, 6, 7};

  // "<n>SP", 0 through 7 for 1/6, 1/4, 1/3, 1/2, x1, x2, x3, x4.
  definition.speed_parameters = {0, 1, 2, 3, 4, 5, 6, 7};

  std::array<CommandSpec, kPlayerCommandCount>& commands = definition.commands;

  // Tray movement is mechanical, so both get the long timeout.
  commands[Index(PlayerCommand::kTrayOpen)] =
      Command("OP", TimeoutClass::kLong);
  commands[Index(PlayerCommand::kTrayClose)] =
      Command("CO", TimeoutClass::kLong);

  commands[Index(PlayerCommand::kPlay)] = Command("PL", TimeoutClass::kLong);

  // "PL" then "64RB" (keep the audio on through multi-speed) then "MF"
  // (multi-speed forward), as one command: play the disc with its stop codes
  // ignored.
  commands[Index(PlayerCommand::kPlayWithoutStopCodes)] =
      Command("PL64RBMF", TimeoutClass::kLong);

  commands[Index(PlayerCommand::kPause)] = Command("PA");
  commands[Index(PlayerCommand::kStillFrame)] = Command("ST");

  // Reject. Spins the disc down, so it waits like the tray does.
  commands[Index(PlayerCommand::kStop)] = Command("RJ", TimeoutClass::kLong);

  commands[Index(PlayerCommand::kStepForward)] = Command("SF");
  commands[Index(PlayerCommand::kStepReverse)] = Command("SR");
  commands[Index(PlayerCommand::kScanForward)] = Command("NF");
  commands[Index(PlayerCommand::kScanReverse)] = Command("NR");
  commands[Index(PlayerCommand::kMultiSpeedForward)] = Command("MF");
  commands[Index(PlayerCommand::kMultiSpeedReverse)] = Command("MB");

  commands[Index(PlayerCommand::kSetSpeed)] = CommandWithArgument("", "SP", 1);

  // A seek moves the optical assembly across the disc, so all three wait.
  //
  // Frame and time-code seeks are the same command with a differently sized
  // address, which is the Pioneer format rather than an oversight here: five
  // digits is a CAV frame and seven is a CLV time code.
  commands[Index(PlayerCommand::kSeekFrame)] =
      CommandWithArgument("FR", "SE", 5, TimeoutClass::kLong);
  commands[Index(PlayerCommand::kSeekTimeCode)] =
      CommandWithArgument("FR", "SE", 7, TimeoutClass::kLong);
  commands[Index(PlayerCommand::kSeekChapter)] =
      CommandWithArgument("CH", "SE", 2, TimeoutClass::kLong);

  commands[Index(PlayerCommand::kDisplayOn)] = Command("1DS");
  commands[Index(PlayerCommand::kDisplayOff)] = Command("0DS");

  commands[Index(PlayerCommand::kSetAudio)] = CommandWithArgument("", "AD", 1);

  commands[Index(PlayerCommand::kKeyLockOn)] = Command("1KL");
  commands[Index(PlayerCommand::kKeyLockOff)] = Command("0KL");

  // The queries. Their replies are the answer rather than an acknowledgement,
  // so they are not put through the error convention — a user code is arbitrary
  // bytes and may perfectly well contain an 'E'.
  commands[Index(PlayerCommand::kQueryActiveMode)] = Query("?P");
  commands[Index(PlayerCommand::kQueryAddress)] = Query("?F");
  commands[Index(PlayerCommand::kQueryDiscStatus)] = Query("?D");
  commands[Index(PlayerCommand::kQueryStandardUserCode)] = Query("$Y");
  commands[Index(PlayerCommand::kQueryPioneerUserCode)] = Query("?U");

  return definition;
}

// What answers when a player identifies itself correctly with a model ID no
// definition claims.
//
// It gets the whole Level III set, because that is the best available guess and
// a usable one; what it does not get is a claim to be a model. The application
// says the player is unrecognised, and the manual command field in the remote
// is how somebody works out what it actually does — which is how the next
// definition header gets written.
inline constexpr PlayerDefinition kGenericLevelIII = [] {
  PlayerDefinition definition = LevelIII();
  definition.name = "Unrecognised Pioneer player";
  definition.id_code = "";
  definition.is_generic = true;
  return definition;
}();

}  // namespace ddd::player::pioneer
