/************************************************************************

    capture_panel.h

    Device selection and the monitor control
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QWidget>
#include <vector>

#include "capture_metatypes.h"
#include "usb_device_info.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

namespace ddd::gui {

class CaptureController;

// The panel a capture is driven from: which device, whether the gateware is in
// test mode, and the one button that starts and stops.
//
// One button rather than two. Monitoring is a state, not an action pair, and a
// Start beside a Stop leaves one of them wrong at all times — the user has to
// read which is disabled to find out what the application is doing. A single
// button whose label is the next thing that will happen cannot be ambiguous.
class CapturePanel : public QWidget {
  Q_OBJECT

 public:
  explicit CapturePanel(CaptureController* controller,
                        QWidget* parent = nullptr);

  // Named so the widget tests can find them without depending on layout order.
  static constexpr const char* kDeviceComboName = "capture_device_combo";
  static constexpr const char* kMonitorButtonName = "capture_monitor_button";
  static constexpr const char* kTestModeBoxName = "capture_test_mode_box";
  static constexpr const char* kStatusLabelName = "capture_status_label";

 private slots:
  void OnDevicesChanged(const std::vector<ddd::capture::DeviceInfo>& devices);
  void OnMonitoringChanged(bool monitoring);
  void OnMonitorButtonPressed();
  void OnDeviceSelected(int index);
  void OnTestModeToggled(bool enabled);

 private:
  void UpdateEnabledState();

  CaptureController* controller_ = nullptr;

  QComboBox* device_combo_ = nullptr;
  QCheckBox* test_mode_box_ = nullptr;
  QPushButton* monitor_button_ = nullptr;
  QLabel* status_label_ = nullptr;

  std::vector<ddd::capture::DeviceInfo> devices_;
  bool monitoring_ = false;
};

}  // namespace ddd::gui
