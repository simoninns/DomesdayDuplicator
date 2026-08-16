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
#include "capture_controller.h"
#include "capture_panel.h"
#include "device_programmer.h"
#include "device_updater.h"
#include "firmware_dialog.h"
#include "log_message_model.h"
#include "log_panel.h"
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
                       CaptureController* capture_controller, QWidget* parent)
    : QMainWindow(parent),
      theme_controller_(theme_controller),
      logger_(logger),
      capture_controller_(capture_controller),
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

              // A device in recovery mode is not "no device attached", and
              // saying so to somebody looking straight at one is how a user
              // decides the application is broken. It is also not a device
              // that can capture, so it is not counted as one.
              if (devices.empty()) {
                statusBar()->showMessage(tr("No capture device attached"));
              } else if (capturable == 0) {
                statusBar()->showMessage(
                    tr("Device attached with no firmware — Help ▸ Firmware… "
                       "can program it"));
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
  capture_dock_->setWidget(
      new CapturePanel(capture_controller_, capture_dock_));
  addDockWidget(Qt::LeftDockWidgetArea, capture_dock_);
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
  file_menu->addAction(tr("&Settings…"), QKeySequence::Preferences, this,
                       &MainWindow::ShowSettingsDialog);
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

void MainWindow::BuildToolsMenu() {
  // A menu of its own, because every entry is about checking the instrument
  // rather than about taking a recording. Test mode in particular was on the
  // Capture panel, where it sat among the settings of an ordinary capture and
  // read as one of them — it is the opposite: it replaces the signal with a
  // ramp and produces a file with no recording in it at all.
  QMenu* tools_menu = menuBar()->addMenu(tr("&Tools"));

  test_mode_action_ = tools_menu->addAction(tr("&Test data mode"));
  test_mode_action_->setCheckable(true);
  test_mode_action_->setStatusTip(
      tr("Capture the gateware's test pattern instead of the RF input"));
  test_mode_action_->setToolTip(
      tr("Ask the gateware for its internal test pattern instead of the RF "
         "input, so that the whole capture path can be checked against a known "
         "signal. Test captures are always named TestData_ so they cannot be "
         "mistaken for a recording."));

  tools_menu->addSeparator();
  tools_menu->addAction(tr("&Analyse test data…"), this,
                        &MainWindow::ShowAnalysisDialog);

  tools_menu->addSeparator();
  tools_menu->addAction(tr("&Firmware…"), this,
                        &MainWindow::ShowFirmwareDialog);

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

  const std::string_view application = capture::Version();
  versions.application = QString::fromUtf8(
      application.data(), static_cast<qsizetype>(application.size()));

  // Everything about the device comes from what the controller already knows,
  // so opening this reads nothing and cannot block. It also means the dialog
  // works with no controller at all, which is how the widget tests build the
  // window: it then shows this build and says no device is attached.
  if (capture_controller_ == nullptr) {
    FirmwareDialog dialog(versions, this);
    dialog.exec();
    return;
  }

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
}

void MainWindow::ShowSettingsDialog() {
  if (capture_controller_ == nullptr) {
    return;
  }

  SettingsDialog dialog(capture_controller_->settings(),
                        capture_controller_->devices(), this);
  if (dialog.exec() == QDialog::Accepted) {
    capture_controller_->SetSettings(dialog.Settings());
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
  // Never while the Firmware window is open, and this is not a nicety.
  //
  // An update makes the device disappear and reappear, and the version check
  // fires on every reconnection — so a device that came back reporting no
  // commit, which is exactly what a failed or interrupted update leaves,
  // raises this warning at the precise moment the update page is explaining
  // what went wrong. The modal lands on top of that explanation and covers
  // it, so the one message the user needs is the one they do not get.
  //
  // Suppressing it loses nothing. The Firmware window shows the same
  // versions in more detail than this sentence does, and it is open.
  if (firmware_dialog_open_) {
    return;
  }

  // Otherwise a warning and not a critical error, and it does not stop
  // anything. See firmware_version.h: differing builds are worth mentioning
  // and are not known to be broken, and a modal that blocked capture would be
  // punishing a user for a device that works.
  QMessageBox::warning(this, tr("Firmware version"), message);
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
