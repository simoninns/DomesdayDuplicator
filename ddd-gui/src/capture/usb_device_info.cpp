/************************************************************************

    usb_device_info.cpp

    What the application knows about a device before it opens it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "usb_device_info.h"

namespace ddd::capture {

const char* DeviceSpeedName(DeviceSpeed speed) {
  switch (speed) {
    case DeviceSpeed::kUnknown:
      return "unknown speed";
    case DeviceSpeed::kLow:
      return "Low-speed";
    case DeviceSpeed::kFull:
      return "Full-speed";
    case DeviceSpeed::kHigh:
      return "High-speed";
    case DeviceSpeed::kSuper:
      return "SuperSpeed";
    case DeviceSpeed::kSuperPlus:
      return "SuperSpeed+";
  }
  return "unknown speed";
}

const DeviceInfo* SelectDevice(const std::vector<DeviceInfo>& devices,
                               const std::string& preferred_path) {
  if (devices.empty()) {
    return nullptr;
  }

  if (!preferred_path.empty()) {
    for (const DeviceInfo& device : devices) {
      if (device.path == preferred_path) {
        return &device;
      }
    }
  }

  return &devices.front();
}

}  // namespace ddd::capture
