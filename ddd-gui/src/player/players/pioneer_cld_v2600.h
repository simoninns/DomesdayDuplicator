/************************************************************************

    pioneer_cld_v2600.h

    Pioneer CLD-V2600
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include "pioneer_level_iii.h"
#include "player_definition.h"

namespace ddd::player::pioneer {

// Inherits the Level III set unchanged; not yet bench-verified.
inline constexpr PlayerDefinition kCldV2600 = [] {
  PlayerDefinition definition = LevelIII();
  definition.name = "Pioneer CLD-V2600";
  definition.id_code = "27";
  return definition;
}();

}  // namespace ddd::player::pioneer
