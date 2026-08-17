/************************************************************************

    main_window.h

    Application main window and panel framework
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QMainWindow>
#include <QPointer>
#include <utility>

// Included rather than forward declared: moc needs the metatype declaration
// for DiscProfile to be visible where it generates code for the slot below.
#include "player_metatypes.h"
#include "settings_dialog.h"
#include "update_key.h"

class QAction;
class QDockWidget;
class QLabel;
class QMenu;

namespace ddd::gui {

class ExamineDialog;
class GuidedCaptureDialog;
class PlayerRemoteDialog;

class ApplicationLogger;
class CaptureController;
class AmplitudePanel;
class LogMessageModel;
class AutoCaptureController;
class PlayerController;
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
             PlayerController* player_controller = nullptr,
             AutoCaptureController* auto_capture_controller = nullptr,
             QWidget* parent = nullptr);

  // Reveals the Log dock. Used by --debug, where the point of the run is to
  // watch the diagnostics.
  void ShowLogPanel();

  // Which signatures the firmware dialog's update page accepts. Used by
  // --dev-update-key, and set once at startup rather than reachable from the
  // interface: widening what this build trusts is a decision made when the
  // application is launched, not one a user can be talked into part way
  // through an install.
  void SetUpdateKeyPolicy(capture::UpdateKeyPolicy policy) {
    update_key_policy_ = std::move(policy);
  }

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
  void BuildToolsMenu();

  // The player's entries, added to the menu passed in — which is Tools. See the
  // definition for why the player has no menu and no dock of its own.
  void BuildPlayerSection(QMenu* player_menu);

  // Put the gateware into — or out of — test-pattern mode, through the settings
  // rather than directly, so that everything showing the mode agrees about it.
  void SetTestMode(bool enabled);

  void RestoreWindowLayout();
  void ShowAboutDialog();
  void ShowFirmwareDialog();
  // The one settings dialog, opened on whichever tab the entry is about.
  void ShowSettingsDialog(
      SettingsDialog::Tab tab = SettingsDialog::Tab::kCapture);
  void ShowAnalysisDialog();

  // Bring up the remote, or bring the one that is already up to the front.
  void ShowRemoteDialog();

  // The same, for the examine window.
  void ShowExamineDialog();

  // And for the guided setup, which is reached from the examine window's report
  // and carries the profile that report was written from.
  void ShowGuidedCaptureDialog(const ddd::player::DiscProfile& disc);

  void ShowCaptureFinished(const QString& file_path, quint64 bytes);
  void ShowFirmwareWarning(const QString& message);
  void ShowFailure(const QString& title, const QString& detail);

  // True while the Firmware window is up. The version-mismatch warning is a
  // note rather than a fault and must never appear over that window, least of
  // all over an update's own explanation of what went wrong.
  bool firmware_dialog_open_ = false;

  // What the update page verifies against, unless --dev-update-key widened it.
  capture::UpdateKeyPolicy update_key_policy_ =
      capture::DefaultUpdateKeyPolicy();

  ThemeController* theme_controller_;
  ApplicationLogger* logger_;
  CaptureController* capture_controller_;
  PlayerController* player_controller_;
  AutoCaptureController* auto_capture_controller_;
  LogMessageModel* log_model_;

  QDockWidget* capture_dock_ = nullptr;
  QDockWidget* statistics_dock_ = nullptr;
  QDockWidget* waveform_dock_ = nullptr;
  QDockWidget* spectrum_dock_ = nullptr;
  QDockWidget* amplitude_dock_ = nullptr;
  QDockWidget* log_dock_ = nullptr;

  // Tools ▸ Test data mode. Held so that a change made anywhere else can be
  // reflected in the tick, and so the entry can be taken away while streaming.
  QAction* test_mode_action_ = nullptr;

  // Tools ▸ Player control. Held for the same reason: the remote's Connection
  // tab and the settings dialog change the same setting, and all three have to
  // agree.
  QAction* player_enabled_action_ = nullptr;

  // The player's state in the status bar. A permanent widget rather than a
  // message, so it does not take turns with the capture state — the two are
  // about different pieces of equipment and a user watching one should not lose
  // sight of the other.
  QLabel* player_status_label_ = nullptr;

  // The remote, if it is open. One window however many ways there are of
  // reaching it — two remotes would be two things sending commands down one
  // cable — and a QPointer because it deletes itself when it is closed.
  QPointer<PlayerRemoteDialog> remote_dialog_;

  // And the examine window, on the same terms and for the same reason: two
  // examinations would be two sequences seeking one player.
  QPointer<ExamineDialog> examine_dialog_;

  // And the guided setup, on the same terms again: two of these would be two
  // sequences driving one player and one capture engine.
  QPointer<GuidedCaptureDialog> guided_dialog_;

  // Held so the two can be related to one another after both exist: the
  // Amplitude panel can be asked to keep pace with the spectrogram, and neither
  // panel knows about the other.
  SpectrumPanel* spectrum_panel_ = nullptr;
  AmplitudePanel* amplitude_panel_ = nullptr;
};

}  // namespace ddd::gui
