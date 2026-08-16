/************************************************************************

    settings_dialog.h

    The application's settings, grouped by what they are about
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QDialog>
#include <vector>

#include "capture_settings.h"
#include "player_settings.h"
#include "serial_port_scanner.h"
#include "usb_device_info.h"

class QCheckBox;
class QComboBox;
class QListWidget;
class QTabWidget;
class QWidget;

namespace ddd::gui {

// Everything that is set once for a machine and then left alone.
//
// A dialog rather than a panel, because none of it is worth screen space during
// a capture: the panels are for what is happening, and this is for how.
//
// Tabbed, and the tabs are the point rather than decoration. These settings are
// about two different pieces of equipment — how this machine moves data off the
// Duplicator, and what is on the other end of a serial cable — and a single
// form would put "which serial port" directly beneath "USB transfer size" under
// one **OK**. Somebody looking for one of them would have to read all of them.
class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  // Which tab to open on. The Player menu's own entry opens straight on the
  // player's settings, so that reaching them is not a hunt through a dialog
  // named after something else.
  enum class Tab {
    kCapture,
    kPlayer,
  };

  SettingsDialog(const CaptureSettings& capture,
                 const std::vector<ddd::capture::DeviceInfo>& devices,
                 const PlayerSettings& player,
                 const std::vector<SerialPortCandidate>& ports,
                 Tab initial_tab = Tab::kCapture, QWidget* parent = nullptr);

  // What the dialog was left showing. Only meaningful after Accepted.
  CaptureSettings Settings() const;
  PlayerSettings Player() const;

  static constexpr const char* kTabsName = "settings_tabs";

  static constexpr const char* kQueueSizeComboName = "settings_queue_size";
  static constexpr const char* kTransferModeComboName =
      "settings_transfer_mode";
  static constexpr const char* kDeviceComboName = "settings_device";
  static constexpr const char* kFrontEndGainComboName =
      "settings_front_end_gain";

  static constexpr const char* kPlayerEnabledCheckName =
      "settings_player_enabled";
  static constexpr const char* kPlayerModelComboName = "settings_player_model";
  static constexpr const char* kPlayerPortComboName = "settings_player_port";
  static constexpr const char* kPlayerBaudComboName = "settings_player_baud";
  static constexpr const char* kPlayerExcludedListName =
      "settings_player_excluded";

 private:
  QWidget* BuildCapturePage(
      const std::vector<ddd::capture::DeviceInfo>& devices);
  QWidget* BuildPlayerPage(const std::vector<SerialPortCandidate>& ports);

  CaptureSettings capture_;
  PlayerSettings player_;

  QTabWidget* tabs_ = nullptr;

  QComboBox* queue_size_ = nullptr;
  QComboBox* transfer_mode_ = nullptr;
  QComboBox* device_ = nullptr;
  QComboBox* front_end_gain_ = nullptr;

  QCheckBox* player_enabled_ = nullptr;
  QComboBox* player_model_ = nullptr;
  QComboBox* player_port_ = nullptr;
  QComboBox* player_baud_ = nullptr;
  QListWidget* player_excluded_ = nullptr;
};

}  // namespace ddd::gui
