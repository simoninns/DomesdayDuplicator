/************************************************************************

    pioneer_ld_v4300d.h

    Pioneer LD-V4300D
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include "pioneer_level_iii.h"
#include "player_definition.h"

namespace ddd::player::pioneer {

// The industrial player this project documents most thoroughly — see
// docs/content/ldv4300d/ — and the one most likely to be on the other end of
// the cable.
//
// No deltas from the Level III set: everything below is inherited. Until the
// bench checklist in players/README.md has been walked with one, that is a
// plausible inheritance rather than an observation, which is what
// bench_verified staying false records.
inline constexpr PlayerDefinition kLdV4300D = [] {
  PlayerDefinition definition = LevelIII();
  definition.name = "Pioneer LD-V4300D";
  definition.id_code = "15";
  return definition;
}();

}  // namespace ddd::player::pioneer
