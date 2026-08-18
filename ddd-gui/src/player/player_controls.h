/************************************************************************

    player_controls.h

    What the connected player can actually be asked to do
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "player_command.h"
#include "player_definition.h"

namespace ddd::player {

// One answer per control: can the player on the end of this cable do this?
//
// A model's definition and the firmware revision it reported, resolved once at
// connection into a plain value. Two reasons that is worth doing rather than
// asking the definition again at each button.
//
// It is what the remote gates its buttons on, and the remote runs on the
// interface thread while the definition belongs to the session on the worker's.
// A PlayerDefinition pointer would be perfectly valid to send across — they are
// compile-time constants with static storage — and would still be the wrong
// thing to put in a signal, because it invites the interface to start reasoning
// about the protocol on the GUI thread.
//
// And "can it" is not a property of the model alone. Physical position is
// available on the LD-V8000 from one firmware revision onwards, so the answer
// depends on what the player said when it identified itself. Resolving that
// here means one place decides it rather than every caller remembering to.
struct PlayerControls {
  std::array<bool, kPlayerCommandCount> commands{};
  std::array<bool, kAudioModeCount> audio_modes{};
  std::array<bool, kPlaybackSpeedCount> speeds{};

  // Bounds-checked, because each of these enumerations ends in a kCount
  // sentinel that is a perfectly ordinary value of its type. Something asking
  // whether the player can do "kCount" has gone wrong further up, and the right
  // answer to that question is "no" rather than whatever byte follows the
  // array.
  constexpr bool Has(PlayerCommand command) const {
    const size_t index = Index(command);
    return index < kPlayerCommandCount && commands[index];
  }

  constexpr bool Has(AudioMode mode) const {
    const auto index = static_cast<size_t>(mode);
    return index < kAudioModeCount && audio_modes[index];
  }

  constexpr bool Has(PlaybackSpeed speed) const {
    const auto index = static_cast<size_t>(speed);
    return index < kPlaybackSpeedCount && speeds[index];
  }

  // Is anything known about a player at all? False for the default value, which
  // is what the interface holds while there is nothing connected.
  constexpr bool any() const {
    for (const bool present : commands) {
      if (present) {
        return true;
      }
    }
    return false;
  }

  bool operator==(const PlayerControls&) const = default;
};

// Flatten a definition and a firmware revision into one answer per control.
//
// Both halves of the schema have a say, and both are needed. The command table
// says whether there is anything to send; the capability flags say whether the
// model is claimed to have the control at all. A definition that inherits the
// shared Pioneer table but declares, say, no digital audio has a sequence to
// send and no business offering it — and that is exactly the case that would
// otherwise reach a user as a button that appears to work.
PlayerControls ControlsFor(const PlayerDefinition& definition,
                           std::string_view firmware);

}  // namespace ddd::player
