/************************************************************************

    player_metatypes.h

    Player value types, declared once so queued signals can carry them
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QMetaType>

#include "disc_examiner.h"
#include "disc_profile.h"
#include "player_connection.h"
#include "player_request.h"
#include "player_settings.h"
#include "player_status.h"

// The player half of what capture_metatypes.h does for the engine, and split
// from it for the same reason the libraries are split: that header declares
// engine types and this one declares player types, and neither should have to
// be included to use the other.
//
// The rule the comment in capture_metatypes.h is really about still holds, and
// is the one that matters: **one declaration per type, in one header.**
// Q_DECLARE_METATYPE is a template specialisation, so a second header declaring
// a type this one already declares is a build error naming a generated file
// rather than either header.
Q_DECLARE_METATYPE(ddd::gui::PlayerConnection)
Q_DECLARE_METATYPE(ddd::gui::PlayerRequest)
Q_DECLARE_METATYPE(ddd::gui::PlayerReply)
Q_DECLARE_METATYPE(ddd::gui::PlayerSettings)
Q_DECLARE_METATYPE(ddd::player::PlayerStatus)
Q_DECLARE_METATYPE(ddd::player::DiscProfile)
Q_DECLARE_METATYPE(ddd::player::ExamineStage)
Q_DECLARE_METATYPE(ddd::player::ExamineOutcome)
