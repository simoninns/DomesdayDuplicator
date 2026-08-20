/************************************************************************

    qt_message_filter.h

    Keeping the platform plugin's own noise out of the console
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>

namespace ddd::gui {

// Whether a message is one of the platform plugin's that this application
// hides. See InstallQtMessageFilter().
bool IsSilencedPluginMessage(const QString& message);

// Installs a Qt message handler that drops the messages above and passes
// everything else through unchanged.
//
// There is one message this is for. Qt asks the Wayland plugin to grab the
// mouse whenever a dock widget's title bar is dragged, the plugin cannot grab
// it for anything but a popup, and it says so:
//
//   This plugin supports grabbing the mouse only for popup windows
//
// The grab is asked for and refused in Qt's own code, after it has already done
// the drag another way — the drag works, and there is nothing here to fix.
// Nothing can switch the message off either, because the plugin writes it with
// a plain qWarning() and so it carries no category of its own to filter by:
// silencing it by category would mean silencing every uncategorised warning in
// the application, including this one's. Matching the text is the narrower
// choice of the two, and it is why this exists rather than another line beside
// the logging rules in main().
//
// Call it once, before the window is built.
void InstallQtMessageFilter();

}  // namespace ddd::gui
