/************************************************************************

    pioneer_ld_v8000.h

    Pioneer LD-V8000
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include "pioneer_level_iii.h"
#include "player_command.h"
#include "player_definition.h"

namespace ddd::player::pioneer {

// The one model with a delta worth having, and the reason the schema carries a
// tri-state for physical position rather than a flag.
//
// "2962MQ" reads a memory location in the player's V25 processor holding the
// slider position in units of 10 micrometres. It exists from firmware A9
// onwards; on an earlier revision the same command reads something else
// entirely, so the capability is gated on the firmware revision the probe
// reported and not on the model alone.
inline constexpr PlayerDefinition kLdV8000 = [] {
  PlayerDefinition definition = LevelIII();
  definition.name = "Pioneer LD-V8000";
  definition.id_code = "06";
  definition.capabilities.physical_position =
      PhysicalPositionSupport::kFirmwareGated;
  definition.physical_position_firmware = "A9";
  definition.commands[Index(PlayerCommand::kQueryPhysicalPosition)] =
      Query("2962MQ");
  return definition;
}();

}  // namespace ddd::player::pioneer
