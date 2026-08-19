/************************************************************************

    main_window.cpp

    Application main window and panel framework
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "main_window.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <algorithm>

#include "about_dialog.h"
#include "about_text.h"
#include "amplitude_panel.h"
#include "analysis_dialog.h"
#include "application_logger.h"
#include "auto_capture_controller.h"
#include "auto_capture_wizard.h"
#include "board_bringup_wizard.h"
#include "bundled_update.h"
#include "capture_controller.h"
#include "capture_naming_dialog.h"
#include "capture_panel.h"
#include "device_programmer.h"
#include "device_updater.h"
#include "examine_dialog.h"
#include "firmware_dialog.h"
#include "log_message_model.h"
#include "log_panel.h"
#include "player_controller.h"
#include "player_remote_dialog.h"
#include "player_text.h"
#include "serial_port_scanner.h"
#include "settings_dialog.h"
#include "spectrum_panel.h"
#include "statistics_panel.h"
#include "theme_controller.h"
#include "usb_device_info.h"
#include "version.h"
#include "waveform_panel.h"

namespace ddd::gui {
namespace {

constexpr const char* kGeometrySettingsKey = "main_window/geometry";
constexpr const char* kStateSettingsKey = "main_window/state";

}  // namespace

MainWindow::MainWindow(ThemeController* theme_controller,
                       ApplicationLogger* logger,
                       CaptureController* capture_controller,
                       PlayerController* player_controller,
                       AutoCaptureController* auto_capture_controller,
                       QWidget* parent)
    : QMainWindow(parent),
      theme_controller_(theme_controller),
      logger_(logger),
      capture_controller_(capture_controller),
      player_controller_(player_controller),
      auto_capture_controller_(auto_capture_controller),
      log_model_(new LogMessageModel(this)) {
  setWindowTitle(tr("Domesday Duplicator"));

  // Log records arrive from whichever thread produced them; AutoConnection
  // makes the delivery queued when that is not this one, so the model is only
  // ever touched here.
  connect(logger_, &ApplicationLogger::RecordLogged, log_model_,
          &LogMessageModel::Append);

  // Docks may be dropped alongside one another rather than only in rows and
  // columns, which is what makes a useful side-by-side scope and spectrum
  // possible without a fixed layout.
  setDockNestingEnabled(true);

  // See the class comment: the docks own the window, so the central widget is
  // present only because QMainWindow's layout requires one, and is squeezed to
  // nothing horizontally.
  //
  // Its height is deliberately left unbounded, and that is the whole of this
  // comment. QMainWindow arranges its dock areas around the central widget in a
  // three-by-three grid, and the left and right dock areas share the centre row
  // with it. Capping the central widget's height caps that row — so the row can
  // never be given more than the docks' own minimum heights force it to take.
  // Every panel in it then sits at exactly its minimum, the bottom dock area
  // keeps all the space that is left, and the separator between them will not
  // move in either direction.
  //
  // What that looks like to a user is a window whose panels resize horizontally
  // and not vertically, which is a strange enough symptom to be worth naming:
  // the width is capped too, but capping the width only stops a *central view*
  // from taking space the docks divide between themselves anyway, while capping
  // the height stops the docks getting any.
  auto* central = new QWidget(this);
  central->setMaximumWidth(0);
  setCentralWidget(central);

  BuildCaptureDock();
  BuildStatisticsDock();
  BuildWaveformDock();
  BuildSpectrumDock();
  BuildAmplitudeDock();
  BuildLogDock();

  BuildMenus();

  statusBar()->showMessage(tr("No capture device attached"));

  // The player gets its own corner of the status bar rather than sharing the
  // message area: a capture and a player are two pieces of equipment, and a
  // user watching one must not lose sight of the other.
  player_status_label_ = new QLabel(this);
  statusBar()->addPermanentWidget(player_status_label_);

  if (player_controller_ != nullptr) {
    const auto refresh_player_status = [this] {
      player_status_label_->setText(PlayerStatusBarText(
          player_controller_->connection(), player_controller_->status()));
    };

    connect(player_controller_, &PlayerController::ConnectionChanged, this,
            [refresh_player_status](const PlayerConnection&) {
              refresh_player_status();
            });
    connect(player_controller_, &PlayerController::StatusUpdated, this,
            [refresh_player_status](const player::PlayerStatus&) {
              refresh_player_status();
            });

    refresh_player_status();
  }

  if (capture_controller_ != nullptr) {
    // The status bar says the same thing the Capture panel does, because the
    // panel can be closed and the status bar cannot. Someone who has hidden
    // every panel but the spectrum should still be able to see that the device
    // has been unplugged.
    connect(capture_controller_, &CaptureController::DevicesChanged, this,
            [this](const std::vector<ddd::capture::DeviceInfo>& devices) {
              const auto capturable =
                  std::count_if(devices.begin(), devices.end(),
                                [](const ddd::capture::DeviceInfo& device) {
                                  return device.is_application();
                                });

              // Every attached device is running the firmware from before the
              // update mechanism. Answered ahead of the line below because
              // that line's advice would be wrong here — these devices have
              // firmware, and the dialog it names cannot reach them — and
              // only when *every* device is one, so that a repairable board
              // attached alongside still gets the message with something to
              // do about it.
              const auto legacy = std::count_if(
                  devices.begin(), devices.end(),
                  [](const ddd::capture::DeviceInfo& device) {
                    return device.personality ==
                           ddd::capture::DevicePersonality::kLegacy;
                  });

              // A device in recovery mode is not "no device attached", and
              // saying so to somebody looking straight at one is how a user
              // decides the application is broken. It is also not a device
              // that can capture, so it is not counted as one.
              if (devices.empty()) {
                statusBar()->showMessage(tr("No capture device attached"));
              } else if (static_cast<size_t>(legacy) == devices.size()) {
                statusBar()->showMessage(
                    tr("Original Duplicator firmware — too old for this "
                       "application to use"));
              } else if (capturable == 0) {
                statusBar()->showMessage(
                    tr("Device attached with no firmware — Tools ▸ Firmware ▸ "
                       "Update firmware… can program it"));
              } else {
                statusBar()->showMessage(
                    tr("%1 device(s) attached").arg(capturable));
              }
            });
    connect(capture_controller_, &CaptureController::MonitoringChanged, this,
            [this](bool monitoring) {
              if (monitoring) {
                statusBar()->showMessage(tr("Monitoring"));
              }
            });
    connect(capture_controller_, &CaptureController::CapturingChanged, this,
            [this](bool capturing, const QString& file_path) {
              if (capturing) {
                statusBar()->showMessage(tr("Capturing to %1").arg(file_path));
              } else if (capture_controller_->monitoring()) {
                statusBar()->showMessage(tr("Monitoring"));
              }
            });
    connect(capture_controller_, &CaptureController::CaptureFinished, this,
            &MainWindow::ShowCaptureFinished);

    // Said in the status bar and in the log rather than in a box to dismiss.
    // Nothing was overwritten — the engine resolves the path before it opens
    // anything — so this is a fact about the file's name, and a modal in front
    // of a capture that has just started would be in the way of the very thing
    // somebody is watching.
    connect(capture_controller_, &CaptureController::CaptureRenamed, this,
            [this](const QString& requested, const QString& written) {
              const QString message =
                  tr("\u201c%1\u201d was already taken — capturing to "
                     "\u201c%2\u201d instead.")
                      .arg(requested, written);
              statusBar()->showMessage(message);
              if (logger_ != nullptr) {
                logger_->Info(message.toStdString());
              }
            });
    // The status bar and the log, never a box to dismiss. The recording is on
    // disk and is complete; only the text file beside it failed, and a modal
    // saying otherwise would send somebody looking for a fault in the
    // recording.
    connect(capture_controller_, &CaptureController::MetadataWriteFailed, this,
            [this](const QString& detail) {
              statusBar()->showMessage(
                  tr("The capture is written. Its metadata file is not: %1")
                      .arg(detail));
            });
    connect(capture_controller_, &CaptureController::LowSpaceWarning, this,
            [this](const QString& message) {
              QMessageBox::warning(this, tr("Running out of space"), message);
            });
    connect(capture_controller_, &CaptureController::FirmwareWarning, this,
            &MainWindow::ShowFirmwareWarning);
    connect(capture_controller_, &CaptureController::Failed, this,
            &MainWindow::ShowFailure);
  }

  RestoreWindowLayout();
}

void MainWindow::BuildCaptureDock() {
  capture_dock_ = new QDockWidget(tr("Capture"), this);
  // Object names are what saveState()/restoreState() match docks by. Without
  // one, a dock is silently dropped from the restored layout.
  capture_dock_->setObjectName(QStringLiteral("capture_dock"));

  auto* panel = new CapturePanel(capture_controller_, capture_dock_);
  connect(panel, &CapturePanel::NamingRequested, this,
          &MainWindow::ShowNamingDialog);
  connect(panel, &CapturePanel::AutomaticCaptureRequested, this,
          &MainWindow::ShowAutoCaptureWizard);

  // The panel is told whether a player is there rather than reaching for one,
  // so it stays ignorant of what a player is. Both the initial state and every
  // change, because a panel that only learnt on the first reconnection would
  // offer a capture with no player behind it until then.
  if (player_controller_ != nullptr && auto_capture_controller_ != nullptr) {
    connect(player_controller_, &PlayerController::ConnectionChanged, panel,
            [panel](const PlayerConnection& connection) {
              panel->SetAutomaticCaptureAvailable(connection.live());
            });
    panel->SetAutomaticCaptureAvailable(
        player_controller_->connection().live());
  }

  capture_dock_->setWidget(panel);
  addDockWidget(Qt::LeftDockWidgetArea, capture_dock_);
}

void MainWindow::ShowNamingDialog() {
  // Built afresh each time rather than kept, so the fields it shows are the
  // settings as they are now — the automatic capture writes a name into them,
  // and a dialog held from before that would show the previous one.
  //
  // Both controllers, which is the whole reason this is here rather than in the
  // panel: the naming fields can be filled in by asking the player what the
  // disc is, and the panel knows nothing about a player.
  CaptureNamingDialog dialog(capture_controller_, player_controller_, this);
  dialog.exec();
}

void MainWindow::BuildStatisticsDock() {
  statistics_dock_ = new QDockWidget(tr("Statistics"), this);
  statistics_dock_->setObjectName(QStringLiteral("statistics_dock"));
  statistics_dock_->setWidget(
      new StatisticsPanel(capture_controller_, statistics_dock_));
  addDockWidget(Qt::LeftDockWidgetArea, statistics_dock_);
  splitDockWidget(capture_dock_, statistics_dock_, Qt::Vertical);
}

void MainWindow::BuildWaveformDock() {
  waveform_dock_ = new QDockWidget(tr("Waveform"), this);
  waveform_dock_->setObjectName(QStringLiteral("waveform_dock"));
  waveform_dock_->setWidget(
      new WaveformPanel(capture_controller_, waveform_dock_));
  addDockWidget(Qt::RightDockWidgetArea, waveform_dock_);
}

void MainWindow::BuildSpectrumDock() {
  spectrum_dock_ = new QDockWidget(tr("Spectrum"), this);
  spectrum_dock_->setObjectName(QStringLiteral("spectrum_dock"));
  spectrum_panel_ = new SpectrumPanel(capture_controller_, spectrum_dock_);
  spectrum_dock_->setWidget(spectrum_panel_);
  addDockWidget(Qt::RightDockWidgetArea, spectrum_dock_);
  splitDockWidget(waveform_dock_, spectrum_dock_, Qt::Vertical);
}

void MainWindow::BuildAmplitudeDock() {
  amplitude_dock_ = new QDockWidget(tr("Amplitude History"), this);
  amplitude_dock_->setObjectName(QStringLiteral("amplitude_dock"));
  amplitude_panel_ = new AmplitudePanel(capture_controller_, amplitude_dock_);
  amplitude_dock_->setWidget(amplitude_panel_);

  // The one place the two panels are related. Neither knows about the other,
  // and this is the object that owns both.
  connect(spectrum_panel_, &SpectrumPanel::TimeWindowChanged, amplitude_panel_,
          &AmplitudePanel::SetMatchedWindowSeconds);
  addDockWidget(Qt::RightDockWidgetArea, amplitude_dock_);
  splitDockWidget(spectrum_dock_, amplitude_dock_, Qt::Vertical);
}

void MainWindow::BuildLogDock() {
  log_dock_ = new QDockWidget(tr("Log"), this);
  log_dock_->setObjectName(QStringLiteral("log_dock"));
  log_dock_->setWidget(new LogPanel(log_model_, log_dock_));
  addDockWidget(Qt::BottomDockWidgetArea, log_dock_);
  // Hidden by default: it is a diagnostic view, and the first run should show
  // the signal, not the plumbing. The View menu and --debug both reveal it.
  log_dock_->hide();
}

void MainWindow::ShowLogPanel() {
  log_dock_->show();
  log_dock_->raise();
}

void MainWindow::BuildMenus() {
  QMenu* file_menu = menuBar()->addMenu(tr("&File"));
  // Through a lambda rather than the member directly: the member takes which
  // tab to open on, and a default argument is not something a signal can
  // supply.
  file_menu->addAction(tr("&Settings…"), QKeySequence::Preferences, this,
                       [this] { ShowSettingsDialog(); });
  file_menu->addSeparator();
  file_menu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

  QMenu* view_menu = menuBar()->addMenu(tr("&View"));

  // Built from each dock's own toggle action, so the menu cannot drift from the
  // layout: closing a dock by its title-bar button unchecks the menu entry
  // without any code here.
  QMenu* panels_menu = view_menu->addMenu(tr("&Panels"));
  panels_menu->addAction(capture_dock_->toggleViewAction());
  panels_menu->addAction(statistics_dock_->toggleViewAction());
  panels_menu->addAction(waveform_dock_->toggleViewAction());
  panels_menu->addAction(spectrum_dock_->toggleViewAction());
  panels_menu->addAction(amplitude_dock_->toggleViewAction());
  panels_menu->addAction(log_dock_->toggleViewAction());

  view_menu->addSeparator();

  QMenu* theme_menu = view_menu->addMenu(tr("&Theme"));
  auto* theme_group = new QActionGroup(theme_menu);
  theme_group->setExclusive(true);

  const struct {
    ThemeManager::Mode mode;
    QString label;
  } theme_entries[] = {
      {ThemeManager::Mode::kAuto, tr("&Auto")},
      {ThemeManager::Mode::kLight, tr("&Light")},
      {ThemeManager::Mode::kDark, tr("&Dark")},
  };

  for (const auto& entry : theme_entries) {
    QAction* action = theme_menu->addAction(entry.label);
    action->setCheckable(true);
    action->setChecked(theme_controller_->mode() == entry.mode);
    theme_group->addAction(action);
    const ThemeManager::Mode mode = entry.mode;
    connect(action, &QAction::triggered, this,
            [this, mode] { theme_controller_->SetMode(mode); });
  }

  BuildToolsMenu();

  QMenu* help_menu = menuBar()->addMenu(tr("&Help"));
  help_menu->addAction(tr("&About"), this, &MainWindow::ShowAboutDialog);
}

void MainWindow::BuildPlayerSection(QMenu* player_menu) {
  // A submenu of Tools rather than a menu of its own, and this is where the
  // whole of the player's standing interface now lives. Almost every user has
  // one player, set up once and never touched again: it does not earn a dock
  // that is always on screen, and it does not earn a top-level menu either. The
  // status bar says whether it is connected, and everything else is here.
  player_enabled_action_ = player_menu->addAction(tr("&Player control"));
  player_enabled_action_->setCheckable(true);
  player_enabled_action_->setStatusTip(
      tr("Look for a LaserDisc player on the serial ports"));
  player_enabled_action_->setToolTip(
      tr("While this is off, no serial port on this machine is opened or "
         "written to."));

  QAction* const search_action = player_menu->addAction(tr("&Search now"));
  search_action->setStatusTip(
      tr("Look for the player again straight away rather than waiting"));

  player_menu->addSeparator();
  QAction* const remote_action = player_menu->addAction(tr("&Remote control…"));
  remote_action->setStatusTip(
      tr("Drive the player by hand, and see how it is connected"));

  QAction* const examine_action = player_menu->addAction(tr("&Examine disc…"));
  examine_action->setStatusTip(
      tr("Work out the disc's type, addressing and length"));

  QAction* const wizard_action =
      player_menu->addAction(tr("&Automatic capture…"));
  wizard_action->setStatusTip(
      tr("Examine the disc, name the capture, take it, and see what was "
         "written"));

  // No "Player settings…" here. It opened File ▸ Settings… on its Player tab,
  // which is the same dialog reached the same way from one menu along — a
  // second door into one room, and the only thing it bought was not having to
  // click a tab. The Player tab is where player settings live, and this menu
  // is the things you do to a player rather than the things you configure
  // about one.

  if (player_controller_ == nullptr) {
    // Shown rather than hidden, so the menu has the same shape in every build
    // of the window, and disabled rather than left to do nothing when pressed.
    player_enabled_action_->setEnabled(false);
    search_action->setEnabled(false);
    remote_action->setEnabled(false);
    examine_action->setEnabled(false);
    wizard_action->setEnabled(false);
    return;
  }

  player_enabled_action_->setChecked(player_controller_->settings().enabled);

  connect(player_enabled_action_, &QAction::triggered, player_controller_,
          &PlayerController::SetEnabled);
  connect(search_action, &QAction::triggered, player_controller_,
          &PlayerController::SearchNow);
  connect(remote_action, &QAction::triggered, this,
          &MainWindow::ShowRemoteDialog);
  connect(examine_action, &QAction::triggered, this,
          &MainWindow::ShowExamineDialog);
  connect(wizard_action, &QAction::triggered, this,
          &MainWindow::ShowAutoCaptureWizard);
  // The same dialog the File menu opens, on the tab this menu is about. A
  // second dialog for the same settings would be a second place for them to
  // disagree.

  // The settings are the single source of truth, so the panel's checkbox and
  // the dialog are reflected here rather than leaving the tick disagreeing with
  // what the application is doing.
  connect(player_controller_, &PlayerController::SettingsChanged, this,
          [this](const PlayerSettings& settings) {
            const QSignalBlocker blocker(player_enabled_action_);
            player_enabled_action_->setChecked(settings.enabled);
          });

  connect(player_controller_, &PlayerController::ConnectionChanged, this,
          [remote_action, search_action, examine_action,
           wizard_action](const PlayerConnection& connection) {
            search_action->setEnabled(connection.state ==
                                      PlayerConnectionState::kDisconnected);

            // There is nothing to examine without a player, and a disc the
            // application cannot reach is not one it can report on. An
            // automatic capture begins with an examination, so it is gated on
            // exactly the same thing. The remote is a window for driving a
            // player, so it goes the same way: an entry that opens onto
            // controls that can do nothing is an entry that does nothing.
            remote_action->setEnabled(connection.live());
            examine_action->setEnabled(connection.live());
            wizard_action->setEnabled(connection.live());
          });

  search_action->setEnabled(player_controller_->connection().state ==
                            PlayerConnectionState::kDisconnected);

  // Both starting states, not just the search's. Without these lines the gated
  // entries sit enabled from the moment the window opens until the first
  // connection report arrives — and with player control switched off no report
  // ever comes, so they stay enabled for the whole session with no player
  // behind them.
  remote_action->setEnabled(player_controller_->connection().live());
  examine_action->setEnabled(player_controller_->connection().live());
  wizard_action->setEnabled(player_controller_->connection().live());

  // What the remote's Connection tab used to be the only route to — why nothing
  // is connected — is the status bar's job when the menu will not open it, and
  // a remote already open when the link goes stays open and greys its own
  // controls rather than shutting in the user's face.
}

void MainWindow::BuildToolsMenu() {
  // A menu of its own, because every entry is about the equipment rather than
  // about taking a recording. Test mode in particular was on the Capture panel,
  // where it sat among the settings of an ordinary capture and read as one of
  // them — it is the opposite: it replaces the signal with a ramp and produces
  // a file with no recording in it at all.
  QMenu* tools_menu = menuBar()->addMenu(tr("&Tools"));

  // Three submenus and nothing loose, so the menu is a list of the things on
  // the bench rather than a column of entries at two different altitudes. The
  // player first, and separated from what follows: above the line is the other
  // machine on the bench, below it is this one.
  BuildPlayerSection(tools_menu->addMenu(tr("&Player")));
  tools_menu->addSeparator();

  // Making a test capture and reading one back are two halves of the same job —
  // you take the ramp in order to look at it — so they are one submenu rather
  // than two entries that happen to sit next to each other.
  QMenu* const test_data_menu = tools_menu->addMenu(tr("&Test data"));

  test_mode_action_ = test_data_menu->addAction(tr("&Test data mode"));
  test_mode_action_->setCheckable(true);
  test_mode_action_->setStatusTip(
      tr("Capture the gateware's test pattern instead of the RF input"));
  test_mode_action_->setToolTip(
      tr("Ask the gateware for its internal test pattern instead of the RF "
         "input, so that the whole capture path can be checked against a known "
         "signal. Test captures are always named TestData_ so they cannot be "
         "mistaken for a recording."));

  test_data_menu->addSeparator();
  test_data_menu->addAction(tr("&Analyse test data…"), this,
                            &MainWindow::ShowAnalysisDialog);

  // Everything to do with what the device is running, in one place. The
  // ordinary update path is first and keeps its behaviour; bring-up is below
  // the separator because it is what somebody reaches for once in the life of
  // a board and the update path is what they reach for every release.
  QMenu* const firmware_menu = tools_menu->addMenu(tr("&Firmware"));
  firmware_update_action_ = firmware_menu->addAction(
      tr("&Update firmware…"), this, &MainWindow::ShowFirmwareDialog);
  firmware_update_action_->setStatusTip(
      tr("Show what the device is running, and install a firmware and gateware "
         "update onto it — not while a capture is running"));

  firmware_menu->addSeparator();

  bringup_action_ =
      firmware_menu->addAction(tr("&Bring up a new or legacy board…"), this,
                               &MainWindow::ShowBringUpWizard);
  bringup_action_->setStatusTip(
      tr("Program a board from nothing to fully up to date — a newly built "
         "one, or one running the original Duplicator firmware. Not while a "
         "capture is running"));

  if (capture_controller_ == nullptr) {
    // Nothing to put the device into test mode with. Shown rather than hidden
    // so the menu has the same shape in every build of the window, and disabled
    // rather than left to do nothing when pressed.
    test_mode_action_->setEnabled(false);
    return;
  }

  test_mode_action_->setChecked(capture_controller_->settings().test_mode);

  connect(test_mode_action_, &QAction::triggered, this,
          [this](bool checked) { SetTestMode(checked); });

  // The settings are the single source of truth, so anything else that changes
  // them is reflected here rather than leaving the tick and the device
  // disagreeing.
  connect(capture_controller_, &CaptureController::SettingsChanged, this,
          [this](const CaptureSettings& settings) {
            const QSignalBlocker blocker(test_mode_action_);
            test_mode_action_->setChecked(settings.test_mode);
          });

  // The mode reaches the gateware when the stream is opened and there is no
  // acknowledgement, so changing it under a running stream would put the change
  // somewhere unpredictable in the data. Off while streaming, as the checkbox
  // it replaces was.
  connect(
      capture_controller_, &CaptureController::MonitoringChanged, this,
      [this](bool monitoring) { test_mode_action_->setEnabled(!monitoring); });

  // And both firmware entries off while a capture is running. Everything
  // behind them resets the device, rewrites its flash or reconfigures its
  // FPGA, and a capture is a bulk transfer from that same device — so opening
  // either one mid-capture would end the recording in progress and could leave
  // a half-written flash behind it.
  //
  // Disabled rather than offered with a warning to confirm. Stopping a capture
  // that may be hours old is a decision for the person who started it, not a
  // side effect of opening a menu, and the status tips say what to do about it.
  // Monitoring is the other case entirely and is handled by
  // QuietenCaptureForDeviceWork: nothing is being written, so it is simply put
  // down and picked up again.
  const auto follow_capture = [this](bool capturing) {
    firmware_update_action_->setEnabled(!capturing);
    bringup_action_->setEnabled(!capturing);
  };
  follow_capture(capture_controller_->capturing());
  connect(capture_controller_, &CaptureController::CapturingChanged, this,
          [follow_capture](bool capturing, const QString&) {
            follow_capture(capturing);
          });
}

void MainWindow::QuietenCaptureForDeviceWork() {
  if (capture_controller_ == nullptr || !capture_controller_->monitoring()) {
    return;
  }

  monitoring_paused_for_device_work_ = true;
  capture_controller_->StopMonitoring();
}

void MainWindow::RestoreCaptureAfterDeviceWork() {
  if (!monitoring_paused_for_device_work_) {
    return;
  }
  monitoring_paused_for_device_work_ = false;

  if (capture_controller_ == nullptr) {
    return;
  }

  // The narrow selection, which is the point of the check: a device in
  // recovery, one running the original firmware, or one that has not finished
  // restarting is not a device to open a stream on. The device monitor may
  // also be a fraction of a second behind here, having just been resumed — so
  // this is deliberately the cheap, current answer rather than a wait for a
  // fresh one. Getting it wrong costs a press of Start monitoring; waiting for
  // certainty would cost a window that sits there doing nothing first.
  const std::vector<capture::DeviceInfo> devices =
      capture_controller_->devices();
  const std::string preferred =
      capture_controller_->settings().preferred_device_path.toStdString();

  if (capture::SelectDevice(devices, preferred) == nullptr) {
    return;
  }

  capture_controller_->StartMonitoring();
}

void MainWindow::SetTestMode(bool enabled) {
  if (capture_controller_ == nullptr) {
    return;
  }

  CaptureSettings settings = capture_controller_->settings();
  if (settings.test_mode == enabled) {
    return;
  }

  settings.test_mode = enabled;
  capture_controller_->SetSettings(settings);
}

void MainWindow::ShowAboutDialog() {
  AboutDialog about(this);
  about.exec();
}

void MainWindow::ShowFirmwareDialog() {
  FirmwareVersions versions;

  versions.application = QString::fromStdString(std::string(capture::Commit()));

  // Everything about the device comes from what the controller already knows,
  // so opening this reads nothing and cannot block. It also means the dialog
  // works with no controller at all, which is how the widget tests build the
  // window: it then shows this build and says no device is attached.
  if (capture_controller_ == nullptr) {
    FirmwareDialog dialog(versions, this);
    dialog.exec();
    return;
  }

  // Before anything is read off the device, and before the dialog can offer to
  // write to it. See QuietenCaptureForDeviceWork.
  QuietenCaptureForDeviceWork();

  const std::vector<capture::DeviceInfo> devices =
      capture_controller_->devices();

  // Any personality here, unlike everywhere else in this window: a device
  // that cannot capture is exactly the device this dialog exists to fix, and
  // a firmware dialog that reported "no device attached" to somebody looking
  // at one in recovery mode would be the worst place in the application to
  // be unhelpful.
  const capture::DeviceInfo* const selected = capture::SelectDevice(
      devices,
      capture_controller_->settings().preferred_device_path.toStdString(),
      capture::DeviceSelection::kAny);

  UpdatePage::Device device;
  std::string device_path;

  if (selected != nullptr) {
    versions.device_attached = true;
    versions.personality = selected->personality;
    device_path = selected->path;
    device.attached = true;
    device.personality = selected->personality;

    // A device that is not running the Duplicator's firmware has no versions
    // to report, and the gateware reading the controller holds is from
    // before it stopped. Left empty rather than shown stale.
    if (selected->is_application()) {
      versions.product_string =
          QString::fromStdString(selected->product_string);
      versions.gateware = capture_controller_->fpga_version();

      device.identity.product_string = selected->product_string;
      device.identity.protocol_version = selected->protocol_version;
      device.identity.gateware_present = versions.gateware.present;
      device.identity.register_map_version = versions.gateware.map_version;
      device.identity.image_role = versions.gateware.image_role;
      device.identity.gateware_commit = versions.gateware.commit;
    }
  }

  // Opened lazily, when a user has chosen a file and confirmed. The dialog
  // itself still touches nothing.
  capture::IUsbDevice* const usb = capture_controller_->usb_device();
  auto* const logger = static_cast<capture::ILogger*>(logger_);

  device.open =
      [usb, device_path, logger](
          const std::string& path) -> std::unique_ptr<capture::IDeviceUpdater> {
    // An empty path means the device the dialog was opened on. A path is
    // given when a device has just been woken out of recovery, because on
    // Windows it comes back at a different one.
    const std::string& target = path.empty() ? device_path : path;
    if (usb == nullptr || target.empty()) {
      return nullptr;
    }
    return capture::MakeDeviceUpdater(*usb, target, logger);
  };

  device.open_programmer =
      [usb, device_path,
       logger]() -> std::unique_ptr<capture::IDeviceProgrammer> {
    if (usb == nullptr || device_path.empty()) {
      return nullptr;
    }
    return capture::MakeDeviceProgrammer(*usb, device_path, logger);
  };

  FirmwareDialog dialog(versions, std::move(device), this);

  // Nothing raised by the device monitor may interrupt this window while it is
  // open — see ShowFirmwareWarning. Reset on the way out rather than on
  // destruction because exec() below is where the time is spent.
  firmware_dialog_open_ = true;

  if (UpdatePage* const page = dialog.update_page(); page != nullptr) {
    // Before anything can be chosen, because it decides what verifies.
    page->SetKeyPolicy(update_key_policy_);

    // The monitor opens every attached device to read its identity, and an
    // update holds one open for minutes and makes it disappear and come back
    // in the middle. Suspending it for the duration is what keeps those two
    // out of each other's way — and what keeps the device list from
    // announcing a disconnection the user was told to expect.
    connect(page, &UpdatePage::BusyChanged, this, [this](bool busy) {
      if (capture_controller_ != nullptr) {
        capture_controller_->SetDeviceMonitorSuspended(busy);
      }
    });
  }

  dialog.exec();
  firmware_dialog_open_ = false;

  RestoreCaptureAfterDeviceWork();
}

void MainWindow::ShowSettingsDialog(SettingsDialog::Tab tab) {
  if (capture_controller_ == nullptr && player_controller_ == nullptr) {
    return;
  }

  // Both halves are shown whichever entry opened the dialog, and each is
  // applied to its own controller. The serial ports are enumerated here rather
  // than held: the list is only wanted while somebody is looking at it, and an
  // adapter plugged in a moment ago should be in it.
  const CaptureSettings capture = capture_controller_ != nullptr
                                      ? capture_controller_->settings()
                                      : CaptureSettings{};
  const std::vector<capture::DeviceInfo> devices =
      capture_controller_ != nullptr ? capture_controller_->devices()
                                     : std::vector<capture::DeviceInfo>{};
  const PlayerSettings player = player_controller_ != nullptr
                                    ? player_controller_->settings()
                                    : PlayerSettings{};

  SettingsDialog dialog(capture, devices, player, EnumerateSerialPorts(), tab,
                        this);

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  if (capture_controller_ != nullptr) {
    capture_controller_->SetSettings(dialog.Settings());
  }
  if (player_controller_ != nullptr) {
    player_controller_->SetSettings(dialog.Player());
  }
}

void MainWindow::ShowAnalysisDialog() {
  // Opened on the capture folder, because the file somebody wants to check is
  // almost always the one they have just taken.
  const QString starting_directory =
      capture_controller_ != nullptr
          ? capture_controller_->settings().ResolvedCaptureDirectory()
          : DefaultCaptureDirectory();

  AnalysisDialog dialog(this);
  dialog.ChooseFileAndAnalyse(starting_directory);
}

void MainWindow::ShowRemoteDialog() {
  if (player_controller_ == nullptr) {
    return;
  }

  // One remote, however it was reached. Two of them would be two things sending
  // commands down one cable, and the second would be showing the first one's
  // replies.
  if (remote_dialog_.isNull()) {
    remote_dialog_ = new PlayerRemoteDialog(player_controller_, this);

    // Deleted when it is closed rather than kept about, so a remote that is not
    // open is not receiving status updates four times a second.
    remote_dialog_->setAttribute(Qt::WA_DeleteOnClose);

    // Opened on the tab that answers the question the user has. The menu entry
    // only opens with a player connected, so that is the transport; the
    // Connection tab is left as the answer if this is ever reached without one.
    //
    // Only on the way in. Somebody who opened the remote, left it on the manual
    // command page and came back to it through the menu meant to return to what
    // they were doing, not to be moved.
    remote_dialog_->ShowTab(player_controller_->connection().live()
                                ? PlayerRemoteDialog::Tab::kControl
                                : PlayerRemoteDialog::Tab::kConnection);
  }

  remote_dialog_->show();
  remote_dialog_->raise();
  remote_dialog_->activateWindow();
}

void MainWindow::ShowExamineDialog() {
  if (player_controller_ == nullptr) {
    return;
  }

  // One examine window, however it was reached. Two of them would be two
  // sequences seeking one disc, and the second would be reporting the first
  // one's answers.
  if (examine_dialog_.isNull()) {
    examine_dialog_ = new ExamineDialog(player_controller_, this);
    examine_dialog_->setAttribute(Qt::WA_DeleteOnClose);
    connect(examine_dialog_, &ExamineDialog::SetUpCaptureRequested, this,
            &MainWindow::ShowAutoCaptureWizardFor);
  }

  examine_dialog_->show();
  examine_dialog_->raise();
  examine_dialog_->activateWindow();
}

void MainWindow::ShowAutoCaptureWizard() {
  if (auto_capture_controller_ == nullptr) {
    return;
  }

  // One wizard, however it was reached. Two would be two sequences driving one
  // player and one capture engine, and the second would be reporting the
  // first's progress.
  if (wizard_.isNull()) {
    wizard_ = new AutoCaptureWizard(auto_capture_controller_, this);

    // Deleted when it is closed rather than kept about, so a wizard that is not
    // open is not examining discs or listening for run progress. It refuses to
    // close while a capture is running, so this cannot take a run's only
    // display away from it.
    wizard_->setAttribute(Qt::WA_DeleteOnClose);
  }

  wizard_->show();
  wizard_->raise();
  wizard_->activateWindow();
}

void MainWindow::ShowBringUpWizard() {
  // One wizard, however it was reached: two would be two things programming
  // one board.
  if (bringup_wizard_.isNull()) {
    capture::IUsbDevice* const usb = capture_controller_ != nullptr
                                         ? capture_controller_->usb_device()
                                         : nullptr;
    auto* const logger = static_cast<capture::ILogger*>(logger_);

    BoardBringUpWizard::Access access;

    // The device list the monitor already keeps, rather than a second
    // enumeration of the bus. It is refreshed five times a second and the
    // wizard reads it twice a second, so what the wizard sees is what the
    // rest of the window sees.
    access.devices = [this] {
      return capture_controller_ != nullptr
                 ? capture_controller_->devices()
                 : std::vector<capture::DeviceInfo>{};
    };

    access.presence = [](uint16_t vendor, uint16_t product) {
      return capture::UsbDeviceAttached(vendor, product);
    };

    access.open_cable =
        [logger](std::string* problem) -> std::unique_ptr<capture::IJtagCable> {
      return capture::MakeUsbBlasterCable(logger, problem);
    };

    access.open_programmer = [usb, logger](const std::string& path)
        -> std::unique_ptr<capture::IDeviceProgrammer> {
      if (usb == nullptr || path.empty()) {
        return nullptr;
      }
      return capture::MakeDeviceProgrammer(*usb, path, logger);
    };

    access.open_updater = [usb, logger](const std::string& path)
        -> std::unique_ptr<capture::IDeviceUpdater> {
      if (usb == nullptr || path.empty()) {
        return nullptr;
      }
      return capture::MakeDeviceUpdater(*usb, path, logger);
    };

    // What this build was packaged with, if anything. Verified by the wizard
    // like any other file — this only says where to look.
    access.bundled_file = [] { return BundledUpdatePath(); };

    bringup_wizard_ = new BoardBringUpWizard(std::move(access), this);
    bringup_wizard_->setAttribute(Qt::WA_DeleteOnClose);
    bringup_wizard_->SetKeyPolicy(update_key_policy_);

    // The monitor opens every attached device to read its identity, and
    // programming holds one open for minutes and makes it disappear and come
    // back in the middle. Suspending it for the duration is what keeps those
    // two out of each other's way — exactly as the update page does.
    connect(bringup_wizard_, &BoardBringUpWizard::BusyChanged, this,
            [this](bool busy) {
              if (capture_controller_ != nullptr) {
                capture_controller_->SetDeviceMonitorSuspended(busy);
              }
            });

    // This wizard is modeless, so there is no exec() to put the restore after.
    // destroyed rather than closed: it deletes itself on close, and a wizard
    // that has been closed but not yet deleted is one whose programming run,
    // if any, has already finished.
    connect(bringup_wizard_, &QObject::destroyed, this,
            [this] { RestoreCaptureAfterDeviceWork(); });

    QuietenCaptureForDeviceWork();
  }

  bringup_wizard_->show();
  bringup_wizard_->raise();
  bringup_wizard_->activateWindow();
}

void MainWindow::ShowAutoCaptureWizardFor(const player::DiscProfile& disc) {
  if (auto_capture_controller_ == nullptr) {
    return;
  }

  // Reached from the Examine window's report, carrying the disc that report was
  // written from. A wizard already watching a run is left alone and merely
  // raised: replacing its disc would rewrite the description of a capture that
  // is already being taken.
  const bool fresh = wizard_.isNull();
  ShowAutoCaptureWizard();

  if (wizard_.isNull() || wizard_->running()) {
    return;
  }

  // Only worth handing over on a wizard that has not been sent somewhere else
  // in the meantime; one just built is on the disc page waiting for exactly
  // this. Either way it skips the examination, which would spend a minute
  // rediscovering what the report already says.
  if (fresh || wizard_->page() == AutoCaptureWizard::Page::kDisc) {
    wizard_->StartFromProfile(disc);
  }
}

void MainWindow::ShowCaptureFinished(const QString& file_path, quint64 bytes) {
  // The status bar rather than a message box. A capture ending is the expected
  // outcome, and a modal to dismiss after every one would be in the way of
  // somebody taking both sides of a disc.
  statusBar()->showMessage(
      tr("Capture written: %1  (%2 MB)")
          .arg(file_path)
          .arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 1));
}

void MainWindow::ShowFirmwareWarning(const QString& message) {
  // Never while a firmware window is open, and this is not a nicety.
  //
  // An update makes the device disappear and reappear, and the version check
  // fires on every reconnection — so a device that came back reporting no
  // commit, which is exactly what a failed or interrupted update leaves,
  // raises this warning at the precise moment the update page is explaining
  // what went wrong. The modal lands on top of that explanation and covers
  // it, so the one message the user needs is the one they do not get.
  //
  // The bring-up wizard is worse again, and it took a bench run to see it.
  // Its FX3 step *deliberately* makes a device appear in the middle: the boot
  // ROM is handed firmware which then runs and enumerates. That firmware is
  // whatever the provisioning set carries, which has no reason to be the
  // build the application was compiled from — so during bring-up this warning
  // is not merely badly timed but guaranteed, and it says nothing the user
  // can act on while a flash is being written underneath it.
  //
  // Suppressing it loses nothing. Both windows show the same versions in more
  // detail than this sentence does, and one of them is open.
  if (FirmwareWindowIsOpen()) {
    return;
  }

  // Otherwise a warning and not a critical error, and it does not stop
  // anything. See firmware_version.h: differing builds are worth mentioning
  // and are not known to be broken, and a modal that blocked capture would be
  // punishing a user for a device that works.
  QMessageBox::warning(this, tr("Firmware version"), message);
}

bool MainWindow::FirmwareWindowIsOpen() const {
  // The update dialog is modal and exec()s, so a flag around that call is what
  // "open" means for it. The wizard is modeless, and *visible* rather than
  // merely alive is what open means there: it deletes itself on close, but
  // through deleteLater, so a wizard somebody has just closed outlives the
  // moment they closed it — and suppressing the warning for that gap would be
  // deciding when a user gets told something on the strength of an event-loop
  // detail.
  //
  // The bring-up wizard counts for a reason of its own: it makes a device
  // disappear and come back as a *different* device, so a version-mismatch
  // warning is not merely likely while it is open — it is the expected
  // consequence of its pages being followed correctly.
  return firmware_dialog_open_ ||
         (!bringup_wizard_.isNull() && bringup_wizard_->isVisible());
}

void MainWindow::ShowFailure(const QString& title, const QString& detail) {
  QMessageBox::critical(this, title, detail);
}

void MainWindow::RestoreWindowLayout() {
  const QSettings settings;

  const QByteArray geometry =
      settings.value(QLatin1String(kGeometrySettingsKey)).toByteArray();
  if (geometry.isEmpty()) {
    resize(1280, 800);
  } else {
    restoreGeometry(geometry);
  }

  // restoreState() re-applies dock positions, sizes, floating state and
  // visibility, so it is the whole of the "arrangement survives restart"
  // promise. An empty value on first run leaves the layout built above.
  const QByteArray state =
      settings.value(QLatin1String(kStateSettingsKey)).toByteArray();
  if (!state.isEmpty()) {
    restoreState(state);
  }
}

void MainWindow::closeEvent(QCloseEvent* event) {
  QSettings settings;
  settings.setValue(QLatin1String(kGeometrySettingsKey), saveGeometry());
  settings.setValue(QLatin1String(kStateSettingsKey), saveState());
  QMainWindow::closeEvent(event);
}

}  // namespace ddd::gui
