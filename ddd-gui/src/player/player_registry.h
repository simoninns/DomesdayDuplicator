/************************************************************************

    player_registry.h

    Every player the application knows about
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <span>
#include <string_view>

#include "player_definition.h"

namespace ddd::player {

// The supported models, in the order an interface should list them.
//
// Pointers rather than values because a definition is a constant with an
// address: the session holds one, and comparing "which model is this" is then a
// pointer comparison rather than a string one.
std::span<const PlayerDefinition* const> RegisteredPlayers();

// The definition claiming this model ID, or null if none does.
//
// Null is the interesting case, not an error: a player that answered the model
// request correctly with an ID nothing claims is a real player of an unknown
// model, and the caller falls back to GenericPlayer() and says so.
const PlayerDefinition* FindPlayerByIdCode(std::string_view id_code);

// The definition used for a player whose model ID is not recognised.
const PlayerDefinition& GenericPlayer();

// The distinct probes across every registered definition.
//
// One entry today, because every supported player is a Pioneer answering the
// Level III model request. It is a list rather than a constant so that adding a
// player family that identifies itself differently is a definition header and a
// registry line, exactly as adding a model is — the session iterates this and
// has no idea how many families there are.
std::span<const ProbeSpec* const> RegisteredProbes();

}  // namespace ddd::player
