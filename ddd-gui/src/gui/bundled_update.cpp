/************************************************************************

    bundled_update.cpp

    Finding the provisioning set a packaged build carries
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "bundled_update.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace ddd::gui {
namespace {

// The application identifier, which is also the name of the directory a Linux
// install puts its data in. Passed in by CMake from the one definition of it
// (DDD_APP_ID in the top-level CMakeLists.txt), so the search and the install
// rule cannot name different directories.
#ifndef DDD_APP_ID
#define DDD_APP_ID "io.github.simoninns.DddGui"
#endif

QString Joined(const QString& directory, const QString& tail) {
  return QDir::cleanPath(directory + QLatin1Char('/') + tail);
}

}  // namespace

QStringList BundledUpdateSearchPaths(const QString& application_dir,
                                     const QStringList& generic_data_dirs) {
  const QString name = QLatin1String(kBundledUpdateName);
  const QString app_id = QLatin1String(DDD_APP_ID);

  QStringList candidates;

  if (!application_dir.isEmpty()) {
    // Beside the executable. The Windows installer harvests a staging
    // directory into one folder, so this is where the MSI's copy lands, and it
    // is also what a build run out of its own build directory sees.
    candidates.append(Joined(application_dir, name));

    // Inside a macOS .app, where the executable is Contents/MacOS/ddd-gui.
    candidates.append(
        Joined(application_dir, QStringLiteral("../Resources/") + name));

    // A prefix install reached relative to the binary rather than through
    // XDG, which is what an application installed somewhere unusual — or run
    // from a relocated tree — has to fall back on.
    candidates.append(
        Joined(application_dir,
               QStringLiteral("../share/") + app_id + QLatin1Char('/') + name));
  }

  // The XDG data path, most-preferred first: inside a Flatpak that begins with
  // /app/share, and on an ordinary desktop it begins with the user's own
  // ~/.local/share — which is how somebody can supply a set to an installed
  // build without a package or a build flag. It is verified like every other,
  // so trusting the directory is not something this has to do.
  for (const QString& directory : generic_data_dirs) {
    if (directory.isEmpty()) {
      continue;
    }
    candidates.append(Joined(directory, app_id + QLatin1Char('/') + name));
  }

  candidates.removeDuplicates();
  return candidates;
}

QString BundledUpdatePath() {
  const QString application_dir = QCoreApplication::instance() != nullptr
                                      ? QCoreApplication::applicationDirPath()
                                      : QString();

  const QStringList candidates = BundledUpdateSearchPaths(
      application_dir,
      QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation));

  for (const QString& candidate : candidates) {
    const QFileInfo file(candidate);
    if (file.isFile() && file.isReadable()) {
      return file.absoluteFilePath();
    }
  }

  return QString();
}

}  // namespace ddd::gui
