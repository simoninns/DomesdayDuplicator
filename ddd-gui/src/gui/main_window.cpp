/************************************************************************

    main_window.cpp

    Application main window and panel framework
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "main_window.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>

#include "about_text.h"
#include "application_logger.h"
#include "log_message_model.h"
#include "log_panel.h"
#include "panel_placeholder.h"
#include "theme_controller.h"

namespace ddd::gui {
namespace {

constexpr const char* kGeometrySettingsKey = "main_window/geometry";
constexpr const char* kStateSettingsKey = "main_window/state";

}  // namespace

MainWindow::MainWindow(ThemeController* theme_controller,
                       ApplicationLogger* logger, QWidget* parent)
    : QMainWindow(parent),
      theme_controller_(theme_controller),
      logger_(logger),
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
  // nothing.
  auto* central = new QWidget(this);
  central->setMaximumSize(0, 0);
  setCentralWidget(central);

  BuildCaptureDock();
  BuildStatisticsDock();
  BuildWaveformDock();
  BuildSpectrumDock();
  BuildAmplitudeDock();
  BuildLogDock();

  BuildMenus();

  statusBar()->showMessage(tr("No capture device attached"));

  RestoreWindowLayout();
}

void MainWindow::BuildCaptureDock() {
  capture_dock_ = new QDockWidget(tr("Capture"), this);
  // Object names are what saveState()/restoreState() match docks by. Without
  // one, a dock is silently dropped from the restored layout.
  capture_dock_->setObjectName(QStringLiteral("capture_dock"));
  capture_dock_->setWidget(new PanelPlaceholder(
      tr("Capture"),
      tr("Device selection, monitor and capture controls, destination and "
         "format."),
      capture_dock_));
  addDockWidget(Qt::LeftDockWidgetArea, capture_dock_);
}

void MainWindow::BuildStatisticsDock() {
  statistics_dock_ = new QDockWidget(tr("Statistics"), this);
  statistics_dock_->setObjectName(QStringLiteral("statistics_dock"));
  statistics_dock_->setWidget(new PanelPlaceholder(
      tr("Statistics"),
      tr("Throughput, transfer counts, sequence-marker state, buffer fill and "
         "sample extremes."),
      statistics_dock_));
  addDockWidget(Qt::LeftDockWidgetArea, statistics_dock_);
  splitDockWidget(capture_dock_, statistics_dock_, Qt::Vertical);
}

void MainWindow::BuildWaveformDock() {
  waveform_dock_ = new QDockWidget(tr("Waveform"), this);
  waveform_dock_->setObjectName(QStringLiteral("waveform_dock"));
  waveform_dock_->setWidget(new PanelPlaceholder(
      tr("Waveform"),
      tr("Time-domain view of the incoming samples, with clip levels and a "
         "cursor readout."),
      waveform_dock_));
  addDockWidget(Qt::RightDockWidgetArea, waveform_dock_);
}

void MainWindow::BuildSpectrumDock() {
  spectrum_dock_ = new QDockWidget(tr("Spectrum"), this);
  spectrum_dock_->setObjectName(QStringLiteral("spectrum_dock"));
  spectrum_dock_->setWidget(new PanelPlaceholder(
      tr("Spectrum"),
      tr("Frequency content from DC to 20 MHz, with averaging and peak hold."),
      spectrum_dock_));
  addDockWidget(Qt::RightDockWidgetArea, spectrum_dock_);
  splitDockWidget(waveform_dock_, spectrum_dock_, Qt::Vertical);
}

void MainWindow::BuildAmplitudeDock() {
  amplitude_dock_ = new QDockWidget(tr("Amplitude History"), this);
  amplitude_dock_->setObjectName(QStringLiteral("amplitude_dock"));
  amplitude_dock_->setWidget(new PanelPlaceholder(
      tr("Amplitude History"),
      tr("Signal level over time, with the min/max envelope and clip events."),
      amplitude_dock_));
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

  QMenu* help_menu = menuBar()->addMenu(tr("&Help"));
  help_menu->addAction(tr("&About"), this, &MainWindow::ShowAboutDialog);
}

void MainWindow::ShowAboutDialog() {
  QMessageBox::about(this, tr("About Domesday Duplicator"), AboutText());
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
