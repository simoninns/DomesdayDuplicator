/************************************************************************

    about_text.h

    Text of the About dialog, including the build provenance
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>

namespace ddd::gui {

// The body of the About dialog, as rich text.
//
// A free function rather than dialog code so that what it promises can be
// tested: the text must carry the commit this binary was built from, and a
// modal dialog cannot be asserted on.
//
// That version line is a requirement, not decoration. It is the second of two
// routes to the build's identity, and on Windows it is the only one that works
// — the application is linked as a GUI subsystem executable, so `--version`
// writes to a console nobody is attached to. A user reporting a bad capture has
// to be able to say which build made it.
QString AboutText();

}  // namespace ddd::gui
