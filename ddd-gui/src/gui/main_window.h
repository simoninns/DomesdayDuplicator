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
class CaptureController;
class AmplitudePanel;
class LogMessageModel;
class SpectrumPanel;
class ThemeController;

// The application window. Every display is a QDockWidget, so any of them can be
// hidden, rearranged or dragged out into a window of its own; the View menu is
// built from the docks' own toggle actions, which is what keeps the menu and
// the layout from disagreeing.
//
// There is deliberately no central widget. A QMainWindow normally gives its
// centre to one dominant view and treats docks as accessories, but here no
// panel is more important than the others — during a capture the operator may
// want the spectrum filling the window, or the statistics alone. A zero-*width*
// central widget lets the docks own the whole window instead; its height is
// left unbounded on purpose, for the reason set out in the constructor.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  // None of the pointers are owned; all must outlive the window.
  //
  // The controller may be null, and the widget tests pass null deliberately.
  // Every panel then builds and lays out exactly as it does in the real
  // application but drives nothing, which is what lets the layout, the menus
  // and the persistence be tested without a USB subsystem to stand up.
  MainWindow(ThemeController* theme_controller, ApplicationLogger* logger,
             CaptureController* capture_controller = nullptr,
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
  void ShowSettingsDialog();
  void ShowAnalysisDialog();
  void ShowCaptureFinished(const QString& file_path, quint64 bytes);
  void ShowFirmwareWarning(const QString& message);
  void ShowFailure(const QString& title, const QString& detail);

  ThemeController* theme_controller_;
  ApplicationLogger* logger_;
  CaptureController* capture_controller_;
  LogMessageModel* log_model_;

  QDockWidget* capture_dock_ = nullptr;
  QDockWidget* statistics_dock_ = nullptr;
  QDockWidget* waveform_dock_ = nullptr;
  QDockWidget* spectrum_dock_ = nullptr;
  QDockWidget* amplitude_dock_ = nullptr;
  QDockWidget* log_dock_ = nullptr;

  // Held so the two can be related to one another after both exist: the
  // Amplitude panel can be asked to keep pace with the spectrogram, and neither
  // panel knows about the other.
  SpectrumPanel* spectrum_panel_ = nullptr;
  AmplitudePanel* amplitude_panel_ = nullptr;
};

}  // namespace ddd::gui
