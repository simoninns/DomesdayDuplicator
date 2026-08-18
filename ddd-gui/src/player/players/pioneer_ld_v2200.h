/************************************************************************

    pioneer_ld_v2200.h

    Pioneer LD-V2200
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include "pioneer_level_iii.h"
#include "player_definition.h"

namespace ddd::player::pioneer {

// Inherits the Level III set unchanged; not yet bench-verified.
inline constexpr PlayerDefinition kLdV2200 = [] {
  PlayerDefinition definition = LevelIII();
  definition.name = "Pioneer LD-V2200";
  definition.id_code = "07";
  return definition;
}();

}  // namespace ddd::player::pioneer
