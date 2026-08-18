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
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPoint>
#include <QSettings>
#include <QStringList>
#include <QTest>

#include "application_logger.h"
#include "board_bringup_wizard.h"
#include "capture_controller.h"
#include "fake_usb_device.h"
#include "main_window.h"
#include "player_controller.h"
#include "serial_port_scanner.h"
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

// A top-level menu by the name on its title. The ampersand Qt uses for the
// mnemonic is in the text, so this matches on the rest of it.
QMenu* MenuNamed(const MainWindow& window, const QString& title) {
  const QList<QAction*> menu_actions = window.menuBar()->actions();
  for (QAction* menu_action : menu_actions) {
    if (menu_action->text().contains(title)) {
      return menu_action->menu();
    }
  }
  return nullptr;
}

// An entry in a menu by the words in its label.
//
// The ampersand Qt uses to mark the mnemonic is taken out before comparing,
// because where it falls is a matter of which letters are still free — "Player
// s&ettings…" — and a test that had to know would be asserting the keyboard
// shortcuts by accident.
QAction* EntryNamed(QMenu* menu, const QString& text) {
  if (menu == nullptr) {
    return nullptr;
  }
  const QList<QAction*> entries = menu->actions();
  for (QAction* entry : entries) {
    QString label = entry->text();
    label.remove(QLatin1Char('&'));
    if (label.contains(text)) {
      return entry;
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

  // A controller that will never touch a serial port: player control is off by
  // default and Start() is never called, and the port list is empty in any
  // case. Enough to give the window a live player layer with nothing connected,
  // which is the state the Remote entry's rule is about.
  static PlayerBackend NoPortsBackend() {
    PlayerBackend backend;
    backend.list_ports = [] { return std::vector<SerialPortCandidate>{}; };
    return backend;
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

// --- Tools ----------------------------------------------------------------

// Both entries are about checking the instrument rather than about taking a
// recording, which is why they are not on File. Test mode in particular was on
// the Capture panel, among the settings of an ordinary capture, where it read
// as one of them — it is the opposite: it replaces the signal with a ramp and
// produces a file with no recording in it at all.
TEST_F(MainWindowTest, TheToolsMenuHoldsBothTestDataEntries) {
  const std::unique_ptr<MainWindow> window = MakeWindow();

  QMenu* const tools = MenuNamed(*window, QStringLiteral("Tools"));
  ASSERT_NE(tools, nullptr) << "no Tools menu";

  QAction* const mode = EntryNamed(tools, QStringLiteral("Test data mode"));
  ASSERT_NE(mode, nullptr) << "no Test data mode entry under Tools";
  EXPECT_TRUE(mode->isCheckable())
      << "test mode is a state, so its entry has to show whether it is on";

  EXPECT_NE(EntryNamed(tools, QStringLiteral("Analyse test data")), nullptr)
      << "no Analyse test data entry under Tools";
}

// Everything to do with what the device is running lives under one entry, and
// the ordinary update path is the first entry inside it — the one somebody
// reaches for at every release, ahead of the flows they use once in the life
// of a board.
TEST_F(MainWindowTest, TheUpdatePathIsAnEntryUnderTheFirmwareSubMenu) {
  const std::unique_ptr<MainWindow> window = MakeWindow();

  QAction* const firmware = EntryNamed(
      MenuNamed(*window, QStringLiteral("Tools")), QStringLiteral("Firmware"));
  ASSERT_NE(firmware, nullptr) << "no Firmware entry under Tools";
  ASSERT_NE(firmware->menu(), nullptr)
      << "Firmware opens a dialog directly rather than a sub-menu";

  EXPECT_NE(EntryNamed(firmware->menu(), QStringLiteral("Update firmware")),
            nullptr)
      << "no Update firmware entry under Tools ▸ Firmware";
}

// The bring-up flow, behind a sub-menu of its own. It is not gated on any
// device state — it begins with its own connectivity checks, and a board that
// is not answering is exactly the board it exists to repair, so an entry
// greyed out because nothing is attached would be greyed out for the user who
// most needs it.
TEST_F(MainWindowTest, BringUpIsUnderFirmwareAndAlwaysAvailable) {
  const std::unique_ptr<MainWindow> window = MakeWindow();

  QAction* const firmware = EntryNamed(
      MenuNamed(*window, QStringLiteral("Tools")), QStringLiteral("Firmware"));
  ASSERT_NE(firmware, nullptr);
  ASSERT_NE(firmware->menu(), nullptr);

  QAction* const bringup =
      EntryNamed(firmware->menu(), QStringLiteral("Bring up"));
  ASSERT_NE(bringup, nullptr) << "no bring-up entry under Tools ▸ Firmware";
  EXPECT_TRUE(bringup->isEnabled());

  // The ordinary path keeps its place at the top, above the line.
  const QList<QAction*> entries = firmware->menu()->actions();
  ASSERT_FALSE(entries.isEmpty());
  EXPECT_TRUE(entries.front()->text().contains(QStringLiteral("Update")))
      << "the update path is no longer the first entry under Firmware";
}

// The firmware-version warning is a note, and it must never land modally on
// top of a window whose whole job is changing what the device runs.
//
// Found on the bench, on the bring-up wizard's programming step. That step
// hands the boot ROM a firmware image which then runs and enumerates — so a
// device appears mid-run, the version check fires on it, and the warning
// covers the page reporting how the write went. It is guaranteed rather than
// unlucky: an update file carries whatever firmware it carries, which has no
// reason to be the build the application was compiled from.
TEST_F(MainWindowTest,
       TheVersionWarningIsSuppressedWhileTheBringUpWizardIsOpen) {
  const std::unique_ptr<MainWindow> window = MakeWindow();

  EXPECT_FALSE(window->FirmwareWindowIsOpen())
      << "a window with nothing open is suppressing the version warning";

  QAction* const firmware = EntryNamed(
      MenuNamed(*window, QStringLiteral("Tools")), QStringLiteral("Firmware"));
  ASSERT_NE(firmware, nullptr);
  ASSERT_NE(firmware->menu(), nullptr);
  QAction* const bringup =
      EntryNamed(firmware->menu(), QStringLiteral("Bring up"));
  ASSERT_NE(bringup, nullptr);

  bringup->trigger();

  EXPECT_TRUE(window->FirmwareWindowIsOpen())
      << "the version warning would appear over the bring-up wizard";

  BoardBringUpWizard* const wizard = window->findChild<BoardBringUpWizard*>();
  ASSERT_NE(wizard, nullptr);
  wizard->close();

  // And back again once it has gone, because suppressing it for the rest of
  // the session would be losing the warning rather than timing it.
  EXPECT_FALSE(window->FirmwareWindowIsOpen())
      << "the version warning stayed suppressed after the wizard closed";
}

TEST_F(MainWindowTest, NeitherTestDataEntryIsLeftBehindOnTheFileMenu) {
  const std::unique_ptr<MainWindow> window = MakeWindow();

  QMenu* const file = MenuNamed(*window, QStringLiteral("File"));
  ASSERT_NE(file, nullptr) << "no File menu";

  EXPECT_EQ(EntryNamed(file, QStringLiteral("Analyse test data")), nullptr);
  EXPECT_EQ(EntryNamed(file, QStringLiteral("Test data mode")), nullptr);

  // And what File is for is still there.
  EXPECT_NE(EntryNamed(file, QStringLiteral("Settings")), nullptr);
  EXPECT_NE(EntryNamed(file, QStringLiteral("xit")), nullptr);
}

// The widget tests build a window with no controller, and there is then nothing
// to put into test mode. Shown rather than hidden so the menu has the same
// shape in every build of the window, and disabled rather than left to do
// nothing when pressed.
TEST_F(MainWindowTest, TestModeIsOfferedButDeadWithNoDeviceLayer) {
  const std::unique_ptr<MainWindow> window = MakeWindow();

  QAction* const mode = EntryNamed(MenuNamed(*window, QStringLiteral("Tools")),
                                   QStringLiteral("Test data mode"));
  ASSERT_NE(mode, nullptr);
  EXPECT_FALSE(mode->isEnabled());
  EXPECT_FALSE(mode->isChecked());
}

// With a controller behind it, the entry is the control it looks like: it
// reaches the settings, and it follows them back when something else changes
// them. The tick and the device must never be able to disagree.
TEST_F(MainWindowTest, TheToolsEntryIsTheTestModeSetting) {
  capture::FakeUsbDevice device;
  CaptureController controller(&device, nullptr);
  MainWindow window(&theme_controller_, &logger_, &controller);

  QAction* const mode = EntryNamed(MenuNamed(window, QStringLiteral("Tools")),
                                   QStringLiteral("Test data mode"));
  ASSERT_NE(mode, nullptr);
  ASSERT_TRUE(mode->isEnabled());
  ASSERT_FALSE(mode->isChecked());

  mode->trigger();
  EXPECT_TRUE(mode->isChecked());
  EXPECT_TRUE(controller.settings().test_mode);

  mode->trigger();
  EXPECT_FALSE(controller.settings().test_mode);

  // Changed from elsewhere — the settings are the single source of truth.
  CaptureSettings settings = controller.settings();
  settings.test_mode = true;
  controller.SetSettings(settings);
  EXPECT_TRUE(mode->isChecked());
}

// --- The player, which has no dock and no menu of its own -------------------

// Almost every user has one player, set up once and never touched again. It
// does not earn permanent screen space, and everything the dock showed is
// either in the status bar or on the remote's Connection tab.
TEST_F(MainWindowTest, ThereIsNoPlayerDock) {
  const std::unique_ptr<MainWindow> window = MakeWindow();

  EXPECT_EQ(DockNamed(*window, QStringLiteral("player_dock")), nullptr)
      << "the player dock is back";

  // And nothing offers to show one, which would be a menu entry that toggles
  // nothing.
  QMenu* const panels = PanelsMenu(*window);
  ASSERT_NE(panels, nullptr);
  EXPECT_EQ(EntryNamed(panels, QStringLiteral("Player")), nullptr);
}

TEST_F(MainWindowTest, ThePlayerHasNoTopLevelMenuOfItsOwn) {
  const std::unique_ptr<MainWindow> window = MakeWindow();

  EXPECT_EQ(MenuNamed(*window, QStringLiteral("Player")), nullptr)
      << "the Player menu is back";
}

TEST_F(MainWindowTest, EveryPlayerEntryIsUnderTools) {
  const std::unique_ptr<MainWindow> window = MakeWindow();

  QMenu* const tools = MenuNamed(*window, QStringLiteral("Tools"));
  ASSERT_NE(tools, nullptr) << "no Tools menu";

  for (const QString& entry :
       {QStringLiteral("Player control"), QStringLiteral("Search now"),
        QStringLiteral("Remote control"), QStringLiteral("Examine disc"),
        QStringLiteral("Automatic capture")}) {
    EXPECT_NE(EntryNamed(tools, entry), nullptr)
        << "no " << entry.toStdString() << " entry under Tools";
  }

  // And no "Player settings…", which opened File ▸ Settings… on its Player
  // tab: the same dialog one menu along, so it bought nothing but a saved
  // click and cost a menu entry that read as a second place to configure a
  // player. This menu is what you do to a player; the dialog is what you set
  // about one.
  EXPECT_EQ(EntryNamed(tools, QStringLiteral("Player settings")), nullptr);

  // The player's own state, so its entry has to show whether it is on.
  QAction* const control = EntryNamed(tools, QStringLiteral("Player control"));
  ASSERT_NE(control, nullptr);
  EXPECT_TRUE(control->isCheckable());

  // And the instrument entries are still there, below the player's.
  EXPECT_NE(EntryNamed(tools, QStringLiteral("Test data mode")), nullptr);
  EXPECT_NE(EntryNamed(tools, QStringLiteral("Firmware")), nullptr);
}

// The one entry that is deliberately *not* gated on there being a player. The
// remote's Connection tab is where a user finds out why nothing is connected,
// so greying it out the moment the link goes would withhold the answer at
// exactly the moment it is wanted — which is what the dock used to give.
TEST_F(MainWindowTest, TheRemoteCanBeOpenedWithNoPlayerConnected) {
  PlayerController player(NoPortsBackend());
  MainWindow window(&theme_controller_, &logger_, nullptr, &player);

  ASSERT_FALSE(player.connection().live());

  QMenu* const tools = MenuNamed(window, QStringLiteral("Tools"));
  ASSERT_NE(tools, nullptr);

  QAction* const remote = EntryNamed(tools, QStringLiteral("Remote control"));
  ASSERT_NE(remote, nullptr);
  EXPECT_TRUE(remote->isEnabled())
      << "the remote cannot be reached with nothing connected, so neither can "
         "the tab that says why";

  // Examine is gated, and rightly: there is no disc to report on. An automatic
  // capture begins with an examination, so it is gated on the same thing.
  for (const QString& entry :
       {QStringLiteral("Examine disc"), QStringLiteral("Automatic capture")}) {
    QAction* const action = EntryNamed(tools, entry);
    ASSERT_NE(action, nullptr) << entry.toStdString();
    EXPECT_FALSE(action->isEnabled()) << entry.toStdString();
  }
}

// The upgrade path, and the one thing about removing a dock that could reach a
// user as a fault. Anybody who has run an earlier build has a saved layout with
// a player_dock in it, and that dock no longer exists. Qt matches docks by
// object name, so what has to be true is that the rest of the arrangement comes
// back intact rather than the whole saved state being thrown away.
TEST_F(MainWindowTest, ALayoutSavedWhenThereWasAPlayerDockStillRestores) {
  {
    // A stand-in for the old window: the docks this build still has, plus the
    // one it does not. Saved through QMainWindow's own format, which is what
    // the application wrote.
    QMainWindow before;
    for (const QString& name :
         {QStringLiteral("capture_dock"), QStringLiteral("player_dock"),
          QStringLiteral("statistics_dock"), QStringLiteral("waveform_dock"),
          QStringLiteral("spectrum_dock"), QStringLiteral("amplitude_dock"),
          QStringLiteral("log_dock")}) {
      auto* const dock = new QDockWidget(&before);
      dock->setObjectName(name);
      before.addDockWidget(Qt::LeftDockWidgetArea, dock);
    }

    // Something a user did, so that the restore can be shown to have happened
    // at all rather than merely not crashed.
    before.findChild<QDockWidget*>(QStringLiteral("spectrum_dock"))->hide();

    QSettings settings;
    settings.setValue(QStringLiteral("main_window/state"), before.saveState());
    settings.sync();
  }

  const std::unique_ptr<MainWindow> window = MakeWindow();
  window->show();

  // Every dock this build has is still here, and still reachable from the menu.
  QMenu* const panels = PanelsMenu(*window);
  ASSERT_NE(panels, nullptr);
  for (const QString& name : ExpectedDockNames()) {
    QDockWidget* const dock = DockNamed(*window, name);
    ASSERT_NE(dock, nullptr)
        << "lost after restoring an old layout: " << name.toStdString();
    EXPECT_TRUE(panels->actions().contains(dock->toggleViewAction()))
        << name.toStdString();
  }

  // And the old layout was applied rather than discarded.
  EXPECT_TRUE(DockNamed(*window, QStringLiteral("spectrum_dock"))->isHidden())
      << "the saved arrangement was thrown away rather than carried over";
}

// With no player layer at all the entries are shown and dead, so the menu has
// the same shape in every build of the window.
TEST_F(MainWindowTest, ThePlayerEntriesAreOfferedButDeadWithNoPlayerLayer) {
  const std::unique_ptr<MainWindow> window = MakeWindow();

  QMenu* const tools = MenuNamed(*window, QStringLiteral("Tools"));
  ASSERT_NE(tools, nullptr);

  for (const QString& entry :
       {QStringLiteral("Player control"), QStringLiteral("Search now"),
        QStringLiteral("Remote control"), QStringLiteral("Examine disc")}) {
    QAction* const action = EntryNamed(tools, entry);
    ASSERT_NE(action, nullptr) << entry.toStdString();
    EXPECT_FALSE(action->isEnabled()) << entry.toStdString();
  }
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

// Docks stacked in a column can only be resized against one another as far as
// their minimum heights allow, and nothing about that failure announces itself:
// the separator is drawn, the cursor changes over it, and dragging does
// nothing. It is only visible as "I can resize horizontally but not
// vertically", which is exactly how it was found.
//
// So the property is asserted directly. A panel that insists on being
// comfortable takes the adjustability of every panel sharing its column, and
// the three signal panels share one.
TEST_F(MainWindowTest, NoPanelDemandsSoMuchHeightThatItsColumnCannotBeResized) {
  const std::unique_ptr<MainWindow> window = MakeWindow();
  window->resize(1280, 800);
  window->show();

  // The right-hand column at the default layout: waveform above spectrum above
  // amplitude history.
  const QStringList stacked{QStringLiteral("waveform_dock"),
                            QStringLiteral("spectrum_dock"),
                            QStringLiteral("amplitude_dock")};

  int demanded = 0;
  for (const QString& name : stacked) {
    QDockWidget* const dock = DockNamed(*window, name);
    ASSERT_NE(dock, nullptr);

    const int minimum = dock->minimumSizeHint().height();

    // Roughly a fifth of an ordinary window, each. A plot's size policy is
    // Expanding, so a small minimum costs it nothing when there is room and is
    // the whole of what lets it get out of the way when there is not.
    EXPECT_LE(minimum, 160)
        << name.toStdString() << " demands " << minimum << " px of height";
    demanded += minimum;
  }

  // Half the window, for three panels, leaving the other half to distribute
  // between them. Below this the separators have so little travel that dragging
  // one reads as broken.
  EXPECT_LE(demanded, 400) << "the signal column demands " << demanded
                           << " px of an 800 px window";
}

// The one that was found by using the application rather than by testing it,
// and that no amount of looking at the layout would have caught: a central
// widget whose *height* was capped at zero pinned QMainWindow's centre row at
// its minimum, so every panel sharing that row sat at its own minimum height
// and the separator above the bottom dock would not move in either direction.
//
// Horizontal resizing worked throughout, which is what made it so odd to
// report: capping the central widget's width only stops a central view taking
// space the docks divide between themselves anyway, while capping its height
// stops the docks getting any.
//
// Asserted by dragging, because the layout was never the thing that was broken:
// resizeDocks() moved these panels perfectly well while the mouse could not.
TEST_F(MainWindowTest, TheBottomPanelSeparatorCanBeDraggedInBothDirections) {
  const std::unique_ptr<MainWindow> window = MakeWindow();
  window->resize(1280, 800);
  window->show();

  QDockWidget* const log = DockNamed(*window, QStringLiteral("log_dock"));
  QDockWidget* const above =
      DockNamed(*window, QStringLiteral("amplitude_dock"));
  ASSERT_NE(log, nullptr);
  ASSERT_NE(above, nullptr);

  log->show();
  QApplication::processEvents();

  const auto drag_separator = [&](int by) {
    // The separator lies between the lowest dock of the row above and the
    // bottom dock area.
    const int y = (above->geometry().bottom() + log->geometry().top()) / 2;
    const int x = window->width() / 2;

    QTest::mousePress(window.get(), Qt::LeftButton, {}, QPoint(x, y));
    QApplication::processEvents();
    QTest::mouseMove(window.get(), QPoint(x, y + (by / 2)));
    QApplication::processEvents();
    QTest::mouseMove(window.get(), QPoint(x, y + by));
    QApplication::processEvents();
    QTest::mouseRelease(window.get(), Qt::LeftButton, {}, QPoint(x, y + by));
    QApplication::processEvents();
  };

  const int started_at = log->height();

  drag_separator(-120);
  const int taller = log->height();
  EXPECT_GT(taller, started_at)
      << "dragging the separator up did not make the bottom panel taller";

  drag_separator(200);
  EXPECT_LT(log->height(), taller)
      << "dragging the separator down did not make the bottom panel shorter";
}

// The other half of the same failure: with the centre row pinned, every panel
// in it sat at exactly its minimum height. Proportional sizing is what says the
// row is being given space rather than merely tolerated.
TEST_F(MainWindowTest, PanelsGetMoreThanTheirMinimumHeightWhenThereIsRoom) {
  const std::unique_ptr<MainWindow> window = MakeWindow();
  window->resize(1280, 800);
  window->show();
  DockNamed(*window, QStringLiteral("log_dock"))->show();
  QApplication::processEvents();

  for (const QString& name :
       {QStringLiteral("waveform_dock"), QStringLiteral("spectrum_dock"),
        QStringLiteral("amplitude_dock")}) {
    QDockWidget* const dock = DockNamed(*window, name);
    ASSERT_NE(dock, nullptr);
    EXPECT_GT(dock->height(), dock->minimumSizeHint().height())
        << name.toStdString() << " is pinned at its minimum height";
  }
}

}  // namespace
}  // namespace ddd::gui
