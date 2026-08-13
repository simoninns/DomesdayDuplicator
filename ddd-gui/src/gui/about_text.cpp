/************************************************************************

    about_text.cpp

    Text of the About dialog, including the build provenance
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "about_text.h"

#include <QCoreApplication>

#include "version.h"

namespace ddd::gui {

QString AboutText() {
  const auto version = capture::Version();
  const QString version_text =
      QString::fromUtf8(version.data(), static_cast<qsizetype>(version.size()));

  return QCoreApplication::translate(
             "AboutText",
             "<h3>Domesday Duplicator</h3>"
             "<p>Capture application for the Domesday Duplicator, a LaserDisc "
             "RF sampler running at 40 million samples per second with 10-bit "
             "resolution over USB 3.0.</p>"
             "<p>Build: %1</p>"
             "<p>Licensed under the GNU General Public License v3 or "
             "later.</p>")
      .arg(version_text);
}

}  // namespace ddd::gui
