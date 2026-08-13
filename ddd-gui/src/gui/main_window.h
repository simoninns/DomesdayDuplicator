/************************************************************************

    main_window.h

    Application main window and panel framework
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QMainWindow>

class QDockWidget;

namespace ddd::gui {

class ApplicationLogger;
class LogMessageModel;
class ThemeController;

// The application window. Every display is a QDockWidget, so any of them can be
// hidden, rearranged or dragged out into a window of its own; the View menu is
// built from the docks' own toggle actions, which is what keeps the menu and
// the layout from disagreeing.
//
// There is deliberately no central widget. A QMainWindow normally gives its
// centre to one dominant view and treats docks as accessories, but here no
// panel is more important than the others — during a capture the operator may
// want the spectrum filling the window, or the statistics alone. A zero-size
// central widget lets the docks own the whole window instead.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  // Neither pointer is owned; both must outlive the window.
  MainWindow(ThemeController* theme_controller, ApplicationLogger* logger,
             QWidget* parent = nullptr);

  // Reveals the Log dock. Used by --debug, where the point of the run is to
  // watch the diagnostics.
  void ShowLogPanel();

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void BuildCaptureDock();
  void BuildStatisticsDock();
  void BuildWaveformDock();
  void BuildSpectrumDock();
  void BuildAmplitudeDock();
  void BuildLogDock();
  void BuildMenus();
  void RestoreWindowLayout();
  void ShowAboutDialog();

  ThemeController* theme_controller_;
  ApplicationLogger* logger_;
  LogMessageModel* log_model_;

  QDockWidget* capture_dock_ = nullptr;
  QDockWidget* statistics_dock_ = nullptr;
  QDockWidget* waveform_dock_ = nullptr;
  QDockWidget* spectrum_dock_ = nullptr;
  QDockWidget* amplitude_dock_ = nullptr;
  QDockWidget* log_dock_ = nullptr;
};

}  // namespace ddd::gui
