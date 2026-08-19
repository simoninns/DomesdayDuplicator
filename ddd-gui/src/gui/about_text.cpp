/************************************************************************

    about_text.cpp

    What the About dialog says, and the artwork it says it with
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "about_text.h"

#include <QCoreApplication>
#include <QIcon>
#include <string>

#include "version.h"

// At global scope, and it has to be: Q_INIT_RESOURCE declares an extern "C"
// symbol, and inside a namespace it would declare a namespaced one that does
// not exist.
//
// Needed at all because the graphics are compiled into a static library. A
// resource's registration runs from a static initialiser, and a linker is
// entitled to drop an object file from a static library when nothing references
// it — so the resource silently does not exist, and only in the real
// application, because the test binaries link the library differently. Calling
// this explicitly is the documented way out.
static void DddGuiInitialiseResources() { Q_INIT_RESOURCE(ddd_gui_resources); }

namespace ddd::gui {
namespace {

// Where a user can find the source, which the licence entitles them to.
constexpr const char* kProjectUrl =
    "https://github.com/simoninns/DomesdayDuplicator";

// The commit, which is what "Build:" means here. It is the same kind of stamp
// the FX3 firmware and the FPGA gateware report, so a bug report quotes three
// hashes that can be set beside one another — see capture/version.h.
QString BuildCommit() {
  return QString::fromStdString(std::string(capture::Commit()));
}

}  // namespace

QString AboutText() {
  return QCoreApplication::translate(
             "AboutText",
             "<h3>Domesday Duplicator</h3>"
             "<p>Capture application for the Domesday Duplicator, a LaserDisc "
             "RF sampler running at 40 million samples per second with 10-bit "
             "resolution over USB 3.0.</p>"
             "<p>Build: %1</p>"
             "<p>Written by Simon Inns.<br>"
             "Copyright © 2018–2026 Simon Inns.</p>"
             "<p>This program is free software: you may redistribute it and "
             "modify it under the terms of the GNU General Public License, "
             "version 3 or later, as published by the Free Software "
             "Foundation.</p>"
             "<p>It is distributed in the hope that it will be useful, but "
             "<b>with no warranty whatsoever</b> — without even the implied "
             "warranty of merchantability or fitness for a particular "
             "purpose. See the GNU General Public License for the details.</p>"
             "<p><a href=\"%2\">%2</a></p>")
      .arg(BuildCommit(), QString::fromUtf8(kProjectUrl));
}

QPixmap AboutLogo() {
  DddGuiInitialiseResources();
  return QPixmap(QStringLiteral(":/graphics/logo-250.png"));
}

QIcon ApplicationIcon() {
  DddGuiInitialiseResources();

  QIcon icon;
  for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
    icon.addFile(QStringLiteral(":/graphics/icon-%1.png").arg(size));
  }
  return icon;
}

}  // namespace ddd::gui
