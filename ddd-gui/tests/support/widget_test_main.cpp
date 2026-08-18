/************************************************************************

    widget_test_main.cpp

    GoogleTest entry point providing a QApplication
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>

// Widgets need a QApplication and a platform plugin, unlike the models and
// controllers tested under the QCoreApplication entry point. Qt's offscreen
// plugin provides one without a display, so these still run on a CI runner; it
// is forced here rather than left to the environment so the tests cannot depend
// on how they were invoked.
int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");

  QApplication app(argc, argv);

  QCoreApplication::setOrganizationName(QStringLiteral("Domesday86"));
  QCoreApplication::setApplicationName(QStringLiteral("ddd-gui-widget-tests"));

  // These tests exercise layout persistence, so they need somewhere QSettings
  // can genuinely write. Two reasons not to leave that to the default location:
  // it is the developer's own configuration directory, and in a packaging
  // sandbox HOME does not exist at all — QSettings then discards every write
  // silently, and a persistence test cannot tell that from a persistence bug.
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
