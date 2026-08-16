/************************************************************************

    player_registry.cpp

    Every player the application knows about
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "player_registry.h"

#include <array>

#include "players/pioneer_cld_v2400.h"
#include "players/pioneer_cld_v2600.h"
#include "players/pioneer_cld_v2800.h"
#include "players/pioneer_cld_v5000.h"
#include "players/pioneer_ld_v2200.h"
#include "players/pioneer_ld_v4200.h"
#include "players/pioneer_ld_v4300d.h"
#include "players/pioneer_ld_v4400.h"
#include "players/pioneer_ld_v8000.h"
#include "players/pioneer_vc_v330.h"

namespace ddd::player {
namespace {

// Adding a player is adding a header beside these and a line to this table.
// Anything more than that is a bug in the schema rather than in the model.
//
// Listed by family and then by model number, which is how somebody looking for
// their player in a drop-down will look for it. The old application listed them
// by descending model ID, which is an implementation detail of the protocol.
constexpr std::array<const PlayerDefinition*, 10> kPlayers{
    &pioneer::kLdV2200,  &pioneer::kLdV4200,  &pioneer::kLdV4300D,
    &pioneer::kLdV4400,  &pioneer::kLdV8000,  &pioneer::kCldV2400,
    &pioneer::kCldV2600, &pioneer::kCldV2800, &pioneer::kCldV5000,
    &pioneer::kVcV330,
};

// A definition that claims a control it has nothing to send for would reach a
// user as a button that does nothing, so it fails the build instead. Checked
// here rather than in each header because this is the one place that has to be
// edited anyway, so there is nowhere for a definition to be added and missed.
static_assert(IsConsistent(pioneer::kLdV2200));
static_assert(IsConsistent(pioneer::kLdV4200));
static_assert(IsConsistent(pioneer::kLdV4300D));
static_assert(IsConsistent(pioneer::kLdV4400));
static_assert(IsConsistent(pioneer::kLdV8000));
static_assert(IsConsistent(pioneer::kCldV2400));
static_assert(IsConsistent(pioneer::kCldV2600));
static_assert(IsConsistent(pioneer::kCldV2800));
static_assert(IsConsistent(pioneer::kCldV5000));
static_assert(IsConsistent(pioneer::kVcV330));
static_assert(IsConsistent(pioneer::kGenericLevelIII));

constexpr std::array<const ProbeSpec*, 1> kProbes{
    &pioneer::kLevelIIIProbe,
};

}  // namespace

std::span<const PlayerDefinition* const> RegisteredPlayers() {
  return kPlayers;
}

const PlayerDefinition* FindPlayerByIdCode(std::string_view id_code) {
  // An empty ID matches nothing. The generic definition carries one, so a
  // player that answered with no ID at all must not resolve to it by accident —
  // it has not been identified, and the caller needs to know that.
  if (id_code.empty()) {
    return nullptr;
  }

  for (const PlayerDefinition* definition : kPlayers) {
    if (definition->id_code == id_code) {
      return definition;
    }
  }

  return nullptr;
}

const PlayerDefinition& GenericPlayer() { return pioneer::kGenericLevelIII; }

std::span<const ProbeSpec* const> RegisteredProbes() { return kProbes; }

}  // namespace ddd::player
