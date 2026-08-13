/************************************************************************

    settings_dialog.h

    Buffer queue, transfer mode and preferred device
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QDialog>
#include <vector>

#include "capture_settings.h"
#include "usb_device_info.h"

class QComboBox;

namespace ddd::gui {

// The settings that affect how a capture is moved off the device.
//
// A dialog rather than a panel, because none of it is worth screen space during
// a capture: these are set once for a machine and then left alone. The panels
// are for what is happening; this is for how.
class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  SettingsDialog(const CaptureSettings& settings,
                 const std::vector<ddd::capture::DeviceInfo>& devices,
                 QWidget* parent = nullptr);

  // What the dialog was left showing. Only meaningful after Accepted.
  CaptureSettings Settings() const;

  static constexpr const char* kQueueSizeComboName = "settings_queue_size";
  static constexpr const char* kTransferModeComboName =
      "settings_transfer_mode";
  static constexpr const char* kDeviceComboName = "settings_device";

 private:
  CaptureSettings settings_;

  QComboBox* queue_size_ = nullptr;
  QComboBox* transfer_mode_ = nullptr;
  QComboBox* device_ = nullptr;
};

}  // namespace ddd::gui
