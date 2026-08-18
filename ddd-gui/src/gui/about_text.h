/************************************************************************

    about_text.h

    What the About dialog says, and the artwork it says it with
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QIcon>
#include <QPixmap>
#include <QString>

namespace ddd::gui {

// The About dialog's text, as rich text.
//
// It carries the build's commit, which is one of only two routes to it — the
// other is `--version`, and on Windows a GUI application started from a
// shortcut has no console to print it to. That makes this the route that always
// works, and the reason the About dialog is a stated requirement rather than a
// courtesy.
//
// It also carries the author, the copyright and the licence, because the GPL
// asks an interactive program to show appropriate legal notices and this is
// where a user goes to look for them.
QString AboutText();

// The project's logo, at a size suited to a dialog.
//
// Returns a null pixmap if the resource is missing, which a caller should treat
// as "no logo" rather than as an error: a missing decoration is not a reason to
// withhold the version and licence text underneath it.
QPixmap AboutLogo();

// The application icon, in every size the artwork provides.
QIcon ApplicationIcon();

}  // namespace ddd::gui
