/************************************************************************

    gui_test_main.cpp

    GoogleTest entry point providing a QCoreApplication
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>

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

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
