/************************************************************************

    pioneer_vc_v330.h

    Pioneer VC-V330
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include "pioneer_level_iii.h"
#include "player_definition.h"

namespace ddd::player::pioneer {

// Inherits the Level III set unchanged; not yet bench-verified.
//
// The old application's enumerator called this one pioneerLCV330 while naming
// it "Pioneer VC-V330". The name it displayed is the one carried forward.
inline constexpr PlayerDefinition kVcV330 = [] {
  PlayerDefinition definition = LevelIII();
  definition.name = "Pioneer VC-V330";
  definition.id_code = "05";
  return definition;
}();

}  // namespace ddd::player::pioneer
