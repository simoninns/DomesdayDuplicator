/************************************************************************

    player_text.h

    What the interface says about the player
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <cstdint>

#include "player_connection.h"
#include "player_state.h"
#include "player_status.h"

namespace ddd::gui {

// Every user-visible string about the player, as pure functions of a value.
//
// Separated from the panel that shows them for the same reason firmware_text.h
// is: most of what these have to say is about things that are absent — no
// player, the wrong player, a player that will not say what it is — and a
// wording that can only be seen by arranging the hardware to misbehave is a
// wording nobody checks.
//
// It also keeps the panel, the status bar and the log saying the same thing,
// which they did not in the old application.

// The headline: one short line naming the state.
QString PlayerConnectionSummary(const PlayerConnection& connection);

// The sentence under it. Empty when there is nothing worth adding — a working
// connection to the expected model does not need explaining.
QString PlayerConnectionDetail(const PlayerConnection& connection);

// How the player was reached: port, rate, model and firmware. Empty when there
// is no connection.
QString PlayerConnectionSource(const PlayerConnection& connection);

// The player's own name for what it is doing.
QString PlayerStateName(player::PlayerState state);

QString TrayStateName(player::TrayState tray);

QString DiscTypeName(player::DiscType type);

// A time code as a clock: 1234500 is 1:23:45.
//
// The player reports it as seven digits — hours, minutes, seconds, frames —
// and shows it to a user the way the disc sleeve does.
QString FormatTimeCode(int32_t time_code);

// Where the player is, in whichever way this disc is addressed. "Lead-in" and
// "Lead-out" are positions in their own right and are said rather than shown as
// a number, because the number means nothing there.
QString PlayerAddressText(const player::PlayerStatus& status);

// The optical assembly's position, on the one model that reports it. Empty
// everywhere else, so a panel can simply hide the row.
QString PhysicalPositionText(const player::PlayerStatus& status);

// The single line for the status bar, which cannot be hidden and so has to
// carry the state whatever else is on screen.
QString PlayerStatusBarText(const PlayerConnection& connection,
                            const player::PlayerStatus& status);

// The note shown for a model whose definition has never met the hardware it
// describes. Empty for a definition that has. See players/README.md — this is
// the interface's half of that promise.
QString PlayerVerificationNote(const PlayerConnection& connection);

}  // namespace ddd::gui
