/************************************************************************

    player_definition.h

    Everything the application knows about one player model, as data
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "player_capabilities.h"
#include "player_command.h"
#include "player_state.h"

namespace ddd::player {

// How to find a player of this family and work out which model it is.
//
// Separate from the command table because it is the one exchange that has to
// happen before any model is known: the application opens a port, asks who is
// there, and only then has a definition to work with. A family — the Pioneer
// Level III players are one — shares a probe, so definitions point at a shared
// spec rather than carrying a copy, and the session can iterate the distinct
// probes without asking each definition in turn.
struct ProbeSpec {
  // The model request, without its terminator.
  std::string_view request;

  // What the start of a usable reply looks like. "P15" for Pioneer Level III:
  // the reply is that prefix, a two-character model ID and a two-character
  // firmware revision, so "P1506A9" is an LD-V8000 running A9.
  std::string_view reply_prefix;

  size_t id_code_offset = 0;
  size_t id_code_length = 0;

  size_t firmware_offset = 0;
  size_t firmware_length = 0;

  // Tried in this order when the rate is being searched for.
  std::span<const uint32_t> baud_rates;

  // Searching: short waits, several attempts, because most of the rates tried
  // are wrong and the cost of being patient at each is paid four times over.
  std::chrono::milliseconds search_timeout{250};
  int search_attempts = 3;

  // A rate the user fixed: one attempt, and wait properly for it. At a known
  // rate a slow answer is still an answer.
  std::chrono::milliseconds fixed_timeout{2500};
  int fixed_attempts = 1;

  // The shortest reply that can still be identified. A reply carrying the
  // prefix and the model ID is enough; the firmware revision is taken when it
  // is there and left empty when it is not, which is what the old application
  // did and what keeps a terse player usable.
  constexpr size_t minimum_reply_length() const {
    return id_code_offset + id_code_length;
  }
};

// One line of a model's active-mode reply table.
struct StateMapping {
  std::string_view code;
  PlayerState state;
};

// How to read this model's answer to the active-mode query.
struct StateDecode {
  // Every mapping starts with this, and a reply that does not is not an
  // active-mode reply at all.
  std::string_view prefix;

  // Searched in order, so a longer code may precede a prefix of itself.
  std::span<const StateMapping> mappings;
};

// How to read this model's answer to the disc-status query.
//
// Type only, for now, and that is a statement about the documentation rather
// than about the schema. The disc-status reply also carries disc size, side and
// CX on at least some models, but the layout varies and this project does not
// have a manual for every one of them; a decode written from guesswork would
// produce a confident wrong answer in the examine report, which is worse than
// the honest "unknown" the profile reports without it. The fields belong here
// when somebody with the manual and the player can fill them in.
struct DiscStatusDecode {
  // Where in the reply the disc type appears.
  size_t disc_type_index = 1;

  char cav_digit = '0';
  char clv_digit = '1';
};

// One player model, entirely as data.
//
// No virtuals and no per-model subclass: a definition is a value a header
// composes and the registry lists. That is what makes adding a player a header
// and a registry line rather than a change to the transport, and what lets one
// test sweep every registered model for the mistakes that matter.
struct PlayerDefinition {
  // --- Identity -----------------------------------------------------------

  std::string_view name = "Unknown player";

  // The model ID as it appears in the probe reply. Empty for the generic
  // fallback, which is therefore never matched by a lookup.
  std::string_view id_code;

  std::string_view manufacturer;

  // True for the fallback definition used when a player answers correctly with
  // an ID code no definition claims. The application says so rather than
  // pretending to know the model — some controls may not work, and the user is
  // better served knowing that than discovering it.
  bool is_generic = false;

  // Whether this definition has been exercised against the model it describes,
  // on a bench, following the checklist in players/README.md. False means the
  // command set is inherited and plausible and has never met the hardware; the
  // interface says so. Nothing in the library behaves differently — this is a
  // claim about evidence, not about capability.
  bool bench_verified = false;

  // --- How to find it -----------------------------------------------------

  // Never null in a registered definition; the registry checks it.
  const ProbeSpec* probe = nullptr;

  // --- What it can do -----------------------------------------------------

  PlayerCapabilities capabilities;

  // The firmware revision from which physical position is available, for a
  // model whose support is firmware-gated. Compared exactly against what the
  // probe reported.
  std::string_view physical_position_firmware;

  // --- What to send -------------------------------------------------------

  std::array<CommandSpec, kPlayerCommandCount> commands;

  // The wire parameter for each audio mode, or kParameterUnsupported.
  std::array<int8_t, kAudioModeCount> audio_parameters{};

  // The wire parameter for each playback speed, or kParameterUnsupported.
  std::array<int8_t, kPlaybackSpeedCount> speed_parameters{};

  // --- How to read what comes back ---------------------------------------

  StateDecode state_decode;
  DiscStatusDecode disc_status;
};

// The command table entry for one command. Not present() when the model does
// not have that command.
constexpr const CommandSpec& Spec(const PlayerDefinition& definition,
                                  PlayerCommand command) {
  return definition.commands[Index(command)];
}

// Does this model, running this firmware, report a physical position?
constexpr bool SupportsPhysicalPosition(const PlayerDefinition& definition,
                                        std::string_view firmware) {
  switch (definition.capabilities.physical_position) {
    case PhysicalPositionSupport::kUnsupported:
      return false;
    case PhysicalPositionSupport::kSupported:
      return true;
    case PhysicalPositionSupport::kFirmwareGated:
      return !definition.physical_position_firmware.empty() &&
             firmware == definition.physical_position_firmware;
  }
  return false;
}

// Is a definition internally consistent?
//
// Every claimed capability must have something to send for it, every present
// command must have a coherent argument, and a parameterised command must have
// a parameter table with at least one usable entry. Written as a constant
// expression so the registry can static_assert it: a definition that claims a
// control it cannot send fails the build, rather than reaching a user as a
// button that does nothing.
constexpr bool IsConsistent(const PlayerDefinition& definition) {
  if (definition.name.empty() || definition.probe == nullptr) {
    return false;
  }

  // A command either takes an argument and says how wide it may be, or takes
  // none and does not.
  for (const CommandSpec& spec : definition.commands) {
    if (!spec.present()) {
      continue;
    }
    if (spec.takes_argument() != (spec.argument_digits > 0)) {
      return false;
    }
    if (spec.argument_digits > 9) {
      return false;
    }
    if (spec.prefix.size() + spec.suffix.size() + spec.argument_digits + 1 >
        kMaximumCommandLength) {
      return false;
    }
  }

  const PlayerCapabilities& able = definition.capabilities;

  const bool present[] = {
      !able.tray_control ||
          (Spec(definition, PlayerCommand::kTrayOpen).present() &&
           Spec(definition, PlayerCommand::kTrayClose).present()),
      !able.stop_codes_disabled_play ||
          Spec(definition, PlayerCommand::kPlayWithoutStopCodes).present(),
      !able.step || (Spec(definition, PlayerCommand::kStepForward).present() &&
                     Spec(definition, PlayerCommand::kStepReverse).present()),
      !able.scan || (Spec(definition, PlayerCommand::kScanForward).present() &&
                     Spec(definition, PlayerCommand::kScanReverse).present()),
      !able.multi_speed ||
          (Spec(definition, PlayerCommand::kMultiSpeedForward).present() &&
           Spec(definition, PlayerCommand::kMultiSpeedReverse).present()),
      !able.speed_selection ||
          Spec(definition, PlayerCommand::kSetSpeed).present(),
      !able.frame_search ||
          Spec(definition, PlayerCommand::kSeekFrame).present(),
      !able.time_code_search ||
          Spec(definition, PlayerCommand::kSeekTimeCode).present(),
      !able.chapter_search ||
          Spec(definition, PlayerCommand::kSeekChapter).present(),
      !able.on_screen_display ||
          (Spec(definition, PlayerCommand::kDisplayOn).present() &&
           Spec(definition, PlayerCommand::kDisplayOff).present()),
      !able.audio_selection ||
          Spec(definition, PlayerCommand::kSetAudio).present(),
      !able.key_lock ||
          (Spec(definition, PlayerCommand::kKeyLockOn).present() &&
           Spec(definition, PlayerCommand::kKeyLockOff).present()),
      !able.standard_user_code ||
          Spec(definition, PlayerCommand::kQueryStandardUserCode).present(),
      !able.pioneer_user_code ||
          Spec(definition, PlayerCommand::kQueryPioneerUserCode).present(),
      able.physical_position == PhysicalPositionSupport::kUnsupported ||
          Spec(definition, PlayerCommand::kQueryPhysicalPosition).present(),
  };

  for (const bool satisfied : present) {
    if (!satisfied) {
      return false;
    }
  }

  // A firmware-gated capability with no gate would be gated on nothing.
  if (able.physical_position == PhysicalPositionSupport::kFirmwareGated &&
      definition.physical_position_firmware.empty()) {
    return false;
  }

  // Speed selection and audio selection are the two parameterised commands, so
  // their tables have to hold something.
  if (able.speed_selection &&
      definition
              .speed_parameters[static_cast<size_t>(PlaybackSpeed::kNormal)] ==
          kParameterUnsupported) {
    return false;
  }

  if (able.audio_selection &&
      definition.audio_parameters[static_cast<size_t>(AudioMode::kOff)] ==
          kParameterUnsupported) {
    return false;
  }

  if (able.digital_audio &&
      definition.audio_parameters[static_cast<size_t>(
          AudioMode::kDigitalStereo)] == kParameterUnsupported) {
    return false;
  }

  // The three queries the application cannot work without: what the player is
  // doing, where it is, and what disc it has.
  return Spec(definition, PlayerCommand::kQueryActiveMode).present() &&
         Spec(definition, PlayerCommand::kQueryAddress).present() &&
         Spec(definition, PlayerCommand::kQueryDiscStatus).present() &&
         !definition.state_decode.mappings.empty();
}

}  // namespace ddd::player
