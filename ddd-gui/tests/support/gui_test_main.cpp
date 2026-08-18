/************************************************************************

    gui_test_main.cpp

    GoogleTest entry point providing a QCoreApplication
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>

// A QCoreApplication, not a QApplication: no display is needed for models,
// controllers and the logging bridge, and requiring one would put these tests
// out of reach of a CI runner. It still gives QObject tests a thread
// dispatcher, so queued connections and timers are delivered when a test pumps
// the event loop.
//
// The settings identity is a test-only one, so a test run cannot overwrite the
// developer's own application settings.
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("Domesday86"));
  QCoreApplication::setApplicationName(QStringLiteral("ddd-gui-tests"));

  // The same writable settings directory the widget entry point sets up, and
  // for the same reason: without it QSettings falls back to the platform
  // default, which is the developer's own configuration directory on a
  // workstation and nowhere at all in a packaging sandbox, where HOME does not
  // exist. In the second case every write is discarded silently, and a test of
  // what was saved cannot tell that from a bug in the saving.
  //
  // Static, so the directory outlives every test and is removed at exit.
  static QTemporaryDir settings_dir;
  if (!settings_dir.isValid()) {
    std::cerr
        << "could not create a writable settings directory for the tests\n";
    return 1;
  }
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                     settings_dir.path());

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
