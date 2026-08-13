/************************************************************************

    test_main_window_panels.cpp

    T1 tests for the dock panel framework and its persistence
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStringList>

#include "application_logger.h"
#include "main_window.h"
#include "theme_controller.h"

namespace ddd::gui {
namespace {

// Every panel the application is expected to offer, by object name. Object
// names are not cosmetic here: saveState()/restoreState() match docks by them,
// so a dock that loses or changes its name silently drops out of every saved
// layout.
const QStringList& ExpectedDockNames() {
  static const QStringList names{
      QStringLiteral("capture_dock"),   QStringLiteral("statistics_dock"),
      QStringLiteral("waveform_dock"),  QStringLiteral("spectrum_dock"),
      QStringLiteral("amplitude_dock"), QStringLiteral("log_dock")};
  return names;
}

QDockWidget* DockNamed(const MainWindow& window, const QString& object_name) {
  return window.findChild<QDockWidget*>(object_name);
}

// The View > Panels submenu.
QMenu* PanelsMenu(const MainWindow& window) {
  const QList<QAction*> menu_actions = window.menuBar()->actions();
  for (QAction* menu_action : menu_actions) {
    QMenu* menu = menu_action->menu();
    if (menu == nullptr) {
      continue;
    }
    const QList<QAction*> entries = menu->actions();
    for (QAction* entry : entries) {
      if (entry->menu() != nullptr &&
          entry->text().contains(QStringLiteral("Panels"))) {
        return entry->menu();
      }
    }
  }
  return nullptr;
}

// Each test gets a settings file of its own, named after the test, so none can
// touch the developer's real settings and no two can touch each other's.
//
// The per-test name is not belt and braces. gtest_discover_tests registers
// every test as its own CTest case, so they run as separate processes and CTest
// may run several at once; a shared file would then have one test's clear()
// land in the middle of another's save-and-restore. That is a race that passes
// on a machine running the suite serially and fails on a build farm.
class MainWindowTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-widget-tests-%1")
            .arg(QString::fromLatin1(info != nullptr ? info->name()
                                                     : "unknown")));

    QSettings settings;
    settings.clear();
    settings.sync();
  }

  void TearDown() override {
    QSettings settings;
    settings.clear();
    settings.sync();
  }

  // A window wired the way main() wires one.
  std::unique_ptr<MainWindow> MakeWindow() {
    return std::make_unique<MainWindow>(&theme_controller_, &logger_);
  }

  ThemeController theme_controller_{
      qobject_cast<QApplication*>(QCoreApplication::instance())};
  ApplicationLogger logger_;
};

TEST_F(MainWindowTest, OffersEveryExpectedPanel) {
  const std::unique_ptr<MainWindow> window = MakeWindow();

  for (const QString& name : ExpectedDockNames()) {
    EXPECT_NE(DockNamed(*window, name), nullptr)
        << "missing dock: " << name.toStdString();
  }
}

TEST_F(MainWindowTest, EveryPanelCanFloatOutIntoItsOwnWindow) {
  const std::unique_ptr<MainWindow> window = MakeWindow();

  for (const QString& name : ExpectedDockNames()) {
    QDockWidget* dock = DockNamed(*window, name);
    ASSERT_NE(dock, nullptr);
    EXPECT_TRUE(dock->features().testFlag(QDockWidget::DockWidgetFloatable))
        << "not floatable: " << name.toStdString();
    EXPECT_TRUE(dock->features().testFlag(QDockWidget::DockWidgetClosable))
        << "not closable: " << name.toStdString();
  }
}

TEST_F(MainWindowTest, TheViewMenuOffersOneToggleForEachPanel) {
  const std::unique_ptr<MainWindow> window = MakeWindow();

  QMenu* panels = PanelsMenu(*window);
  ASSERT_NE(panels, nullptr);
  EXPECT_EQ(panels->actions().size(), ExpectedDockNames().size());

  // The entries must be the docks' own toggle actions rather than lookalikes,
  // which is what keeps the menu and the layout from disagreeing.
  for (const QString& name : ExpectedDockNames()) {
    QDockWidget* dock = DockNamed(*window, name);
    ASSERT_NE(dock, nullptr);
    EXPECT_TRUE(panels->actions().contains(dock->toggleViewAction()))
        << "no menu entry for: " << name.toStdString();
  }
}

TEST_F(MainWindowTest, TheMenuTogglesHideAndShowTheirPanels) {
  const std::unique_ptr<MainWindow> window = MakeWindow();
  window->show();

  QDockWidget* dock = DockNamed(*window, QStringLiteral("waveform_dock"));
  ASSERT_NE(dock, nullptr);
  ASSERT_FALSE(dock->isHidden());

  dock->toggleViewAction()->trigger();
  EXPECT_TRUE(dock->isHidden());

  dock->toggleViewAction()->trigger();
  EXPECT_FALSE(dock->isHidden());
}

TEST_F(MainWindowTest, TheLogPanelStartsHidden) {
  const std::unique_ptr<MainWindow> window = MakeWindow();
  window->show();

  QDockWidget* log = DockNamed(*window, QStringLiteral("log_dock"));
  ASSERT_NE(log, nullptr);
  EXPECT_TRUE(log->isHidden());
}

TEST_F(MainWindowTest, DebugRunsRevealTheLogPanel) {
  const std::unique_ptr<MainWindow> window = MakeWindow();
  window->show();

  window->ShowLogPanel();

  QDockWidget* log = DockNamed(*window, QStringLiteral("log_dock"));
  ASSERT_NE(log, nullptr);
  EXPECT_FALSE(log->isHidden());
}

TEST_F(MainWindowTest, PanelArrangementSurvivesRestart) {
  // The promise the docks make to the user: rearrange once, and the application
  // comes back the same way. Closing is what commits it, so the sequence here
  // is the one a user actually performs.
  {
    const std::unique_ptr<MainWindow> window = MakeWindow();
    window->show();

    DockNamed(*window, QStringLiteral("spectrum_dock"))->hide();
    DockNamed(*window, QStringLiteral("log_dock"))->show();
    DockNamed(*window, QStringLiteral("statistics_dock"))->setFloating(true);

    window->close();
  }

  const std::unique_ptr<MainWindow> restored = MakeWindow();
  restored->show();

  EXPECT_TRUE(
      DockNamed(*restored, QStringLiteral("spectrum_dock"))->isHidden());
  EXPECT_FALSE(DockNamed(*restored, QStringLiteral("log_dock"))->isHidden());
  EXPECT_TRUE(
      DockNamed(*restored, QStringLiteral("statistics_dock"))->isFloating());

  // A panel that was left alone must come back as it was, too.
  EXPECT_FALSE(
      DockNamed(*restored, QStringLiteral("waveform_dock"))->isHidden());
}

TEST_F(MainWindowTest, OffersAnAboutEntryUnderHelp) {
  // The route a user without a terminal takes to the build version. The text
  // itself is asserted in test_about_text.cpp; this is the part that proves it
  // is reachable at all.
  const std::unique_ptr<MainWindow> window = MakeWindow();

  QMenu* help = nullptr;
  const QList<QAction*> menu_actions = window->menuBar()->actions();
  for (QAction* menu_action : menu_actions) {
    if (menu_action->text().contains(QStringLiteral("Help"))) {
      help = menu_action->menu();
    }
  }
  ASSERT_NE(help, nullptr) << "no Help menu";

  bool found_about = false;
  const QList<QAction*> help_entries = help->actions();
  for (QAction* entry : help_entries) {
    found_about =
        found_about || entry->text().contains(QStringLiteral("About"));
  }
  EXPECT_TRUE(found_about) << "no About entry under Help";
}

TEST_F(MainWindowTest, AFirstRunWithNoSavedLayoutShowsTheDefaultArrangement) {
  const std::unique_ptr<MainWindow> window = MakeWindow();
  window->show();

  for (const QString& name : ExpectedDockNames()) {
    QDockWidget* dock = DockNamed(*window, name);
    ASSERT_NE(dock, nullptr);
    const bool should_be_hidden = name == QStringLiteral("log_dock");
    EXPECT_EQ(dock->isHidden(), should_be_hidden)
        << "unexpected initial visibility: " << name.toStdString();
    EXPECT_FALSE(dock->isFloating()) << name.toStdString();
  }
}

}  // namespace
}  // namespace ddd::gui
