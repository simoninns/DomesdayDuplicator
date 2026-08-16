/************************************************************************

    pioneer_cld_v5000.h

    Pioneer CLD-V5000
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include "pioneer_level_iii.h"
#include "player_definition.h"

namespace ddd::player::pioneer {

// A CLD — it plays CDs as well as LaserDiscs — but it answers the same Level
// III command set. Inherited unchanged; not yet bench-verified.
inline constexpr PlayerDefinition kCldV5000 = [] {
  PlayerDefinition definition = LevelIII();
  definition.name = "Pioneer CLD-V5000";
  definition.id_code = "42";
  return definition;
}();

}  // namespace ddd::player::pioneer
