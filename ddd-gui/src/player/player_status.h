/************************************************************************

    player_status.h

    What the player last said about itself
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <optional>

#include "player_state.h"
#include "response_parser.h"

namespace ddd::player {

// One reading of the player, as the status poll produces it.
//
// A plain value, and Qt-free like everything else here, so the poll can be
// compared whole in a test and carried across a thread boundary without
// anything having to be locked. Every field carries its own "not known": a
// player that answered one query and not the next produces a reading that says
// so rather than one with a stale figure in it.
struct PlayerStatus {
  // False before the first successful poll, and after a poll in which the
  // player said nothing at all.
  bool valid = false;

  PlayerState state = PlayerState::kUnknown;

  // What the tray is doing, which follows from the state rather than being
  // asked for separately.
  TrayState tray = TrayState::kUnknown;

  DiscType disc_type = DiscType::kUnknown;

  // Where the player is. `valid` false within it means the address query was
  // refused or unreadable — which happens routinely, because a stopped player
  // has no address to report.
  DiscAddress address;

  // Where the optical assembly is, in millimetres, on the one model that can
  // say. Absent everywhere else, rather than zero — zero is a real position.
  std::optional<float> physical_position_mm;

  bool operator==(const PlayerStatus& other) const {
    return valid == other.valid && state == other.state && tray == other.tray &&
           disc_type == other.disc_type &&
           address.valid == other.address.valid &&
           address.value == other.address.value &&
           address.in_lead_in == other.address.in_lead_in &&
           address.in_lead_out == other.address.in_lead_out &&
           physical_position_mm == other.physical_position_mm;
  }
  bool operator!=(const PlayerStatus& other) const { return !(*this == other); }
};

}  // namespace ddd::player
