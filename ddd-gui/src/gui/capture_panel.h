/************************************************************************

    capture_panel.h

    Device selection, the destination, and the two controls
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <QWidget>
#include <vector>

#include "capture_metatypes.h"
#include "usb_device_info.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QEvent;
class QSpinBox;
class QTimer;

namespace ddd::gui {

class CaptureController;

// The panel a capture is driven from: which device, where the file goes, and
// the two buttons that start and stop.
//
// Two buttons and not four. Monitoring and capturing are each a state rather
// than an action pair, and a Start beside a Stop leaves one of them wrong at
// all times — the user has to read which is disabled to find out what the
// application is doing. A button whose label is the next thing that will happen
// cannot be ambiguous.
//
// Capture and monitor are separate buttons because they are separate decisions:
// monitoring is what a user sits in while setting up a player, and capture is
// what they press when the disc is spinning. Pressing Capture from idle starts
// both, so the common case is still one press.
//
// The monitor button goes dead while a capture is running. A capture is the
// monitored stream with a file on the end of it, so stopping the stream ends
// the capture with it — which made this a second, unlabelled stop button for
// the recording, sitting directly above the real one. Stopping the capture
// leaves the stream running, so nothing is taken away by closing that route.
//
// Test mode is not here. It lives in the Tools menu, because it is a diagnostic
// rather than part of setting up a capture; this panel reflects it in the name
// it offers and in what it lets be changed.
class CapturePanel : public QWidget {
  Q_OBJECT

 public:
  explicit CapturePanel(CaptureController* controller,
                        QWidget* parent = nullptr);

  // Named so the widget tests can find them without depending on layout order.
  static constexpr const char* kDeviceComboName = "capture_device_combo";
  static constexpr const char* kMonitorButtonName = "capture_monitor_button";
  static constexpr const char* kCaptureButtonName = "capture_capture_button";
  static constexpr const char* kStatusLabelName = "capture_status_label";
  static constexpr const char* kDirectoryEditName = "capture_directory_edit";
  static constexpr const char* kBrowseButtonName = "capture_browse_button";
  static constexpr const char* kNameEditName = "capture_name_edit";
  static constexpr const char* kNamingButtonName = "capture_naming_button";
  static constexpr const char* kFormatComboName = "capture_format_combo";
  static constexpr const char* kSampleRateComboName =
      "capture_sample_rate_combo";
  static constexpr const char* kCompressionSpinName =
      "capture_compression_spin";
  static constexpr const char* kDurationSpinName = "capture_duration_spin";
  static constexpr const char* kDurationResetButtonName =
      "capture_duration_reset_button";
  static constexpr const char* kLowSpaceSpinName = "capture_low_space_spin";
  static constexpr const char* kFreeSpaceLabelName = "capture_free_space_label";
  static constexpr const char* kNameTakenLabelName = "capture_name_taken_label";

  // How often the destination volume is interrogated for the free-space
  // readout. The plan asks for it "refreshed continuously", which in practice
  // means often enough that it visibly moves during a capture and rarely enough
  // that a filesystem call is not on the display path: at 40 MB/s two seconds
  // is 80 MB, which is a figure that has visibly changed.
  static constexpr int kFreeSpaceIntervalMilliseconds = 2000;

 signals:
  // The naming dialog is a window, and windows belong to the main window: the
  // panel asks rather than opening one itself.
  //
  // Not merely for tidiness. The dialog can offer to ask the player what the
  // disc is, which needs a PlayerController — and this panel has no business
  // knowing a player exists. The main window holds both controllers, so it is
  // the one place that can build the dialog whole.
  void NamingRequested();

 protected:
  void changeEvent(QEvent* event) override;

 private slots:
  void OnDevicesChanged(const std::vector<ddd::capture::DeviceInfo>& devices);
  void OnMonitoringChanged(bool monitoring);
  void OnCapturingChanged(bool capturing, const QString& file_path);
  void OnMonitorButtonPressed();
  void OnCaptureButtonPressed();
  void OnDeviceSelected(int index);
  void OnBrowsePressed();

  // Ask for the naming dialog, where what the disc is gets typed. A button
  // rather than more rows here: those are eight fields set once per disc, and
  // this panel shares a dock column with the signal displays.
  void OnNamingPressed();

  // Say, as the name is typed, what the file will really be called. Cheap
  // enough for every keystroke: it asks the filesystem what is there and
  // nothing else.
  void RefreshNameNote();
  void OnDurationResetPressed();
  void RefreshFreeSpace();

 private:
  void UpdateEnabledState();

  // Colour the two buttons for what they are currently doing: green while
  // monitoring, red while capturing, and the window's own button colour
  // otherwise. Redundant with the labels on purpose — a state worth a label is
  // worth being able to see without reading.
  void ApplyButtonColours();

  // Whether the next capture would go out with nothing but a timestamp for a
  // name: no name typed, and nothing said about the disc.
  //
  // A perfectly legitimate way to work, so this is a nudge and never a block —
  // the Start button is not touched by it. What it prevents is the other
  // ordinary way this goes: somebody sets up carefully, captures both sides of
  // a disc, and finds two files called RF-Sample_ afterwards with no way left
  // to tell which was which.
  bool NamingWantsAttention() const;

  // Colour the Naming button when it does, through the same pinned-height
  // stylesheet the two capture buttons use.
  void ApplyNamingAttention();

  void ApplySettingsFromWidgets();
  void ShowSettings();

  // The name the next capture will be given, as a placeholder rather than as
  // text. A generated name shown as real text would be saved as though the user
  // had typed it, and every later capture would then reuse the timestamp of the
  // first.
  void UpdateNamePlaceholder();

  CaptureController* controller_ = nullptr;

  QComboBox* device_combo_ = nullptr;
  QPushButton* monitor_button_ = nullptr;
  QPushButton* capture_button_ = nullptr;
  QLabel* status_label_ = nullptr;

  QLineEdit* directory_edit_ = nullptr;
  QPushButton* browse_button_ = nullptr;
  QLineEdit* name_edit_ = nullptr;
  QPushButton* naming_button_ = nullptr;
  QComboBox* format_combo_ = nullptr;
  QComboBox* sample_rate_combo_ = nullptr;
  QSpinBox* compression_spin_ = nullptr;
  QSpinBox* duration_spin_ = nullptr;
  QPushButton* duration_reset_button_ = nullptr;
  QSpinBox* low_space_spin_ = nullptr;
  QLabel* free_space_label_ = nullptr;

  // Shown only while the typed name is one a capture already has. Hidden rather
  // than blank, so the form does not carry a permanently empty row.
  QLabel* name_taken_label_ = nullptr;

  QTimer* free_space_timer_ = nullptr;

  // The height the two buttons have with no stylesheet on them, measured once
  // before either is ever coloured.
  int natural_button_height_ = 0;

  std::vector<ddd::capture::DeviceInfo> devices_;
  bool monitoring_ = false;
  bool capturing_ = false;

  // Whether the gateware is being asked for its test pattern. Held rather than
  // read from a control, because the control is in the Tools menu now — this
  // panel only reflects it, in the name it offers and in what it lets be
  // changed.
  bool test_mode_ = false;

  // True while the widgets are being filled from the settings, so that the
  // change signals they emit do not write those same settings back — which
  // would be harmless but would also clobber anything a test or another panel
  // had just set.
  bool loading_ = false;
};

}  // namespace ddd::gui
