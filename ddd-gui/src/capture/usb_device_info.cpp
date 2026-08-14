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

const char* DevicePersonalityName(DevicePersonality personality) {
  switch (personality) {
    case DevicePersonality::kApplication:
      return "Duplicator firmware";
    case DevicePersonality::kRecovery:
      return "recovery mode";
    case DevicePersonality::kFlashProgrammer:
      return "Cypress flash programmer";
  }
  return "unknown";
}

const DeviceInfo* SelectDevice(const std::vector<DeviceInfo>& devices,
                               const std::string& preferred_path,
                               DeviceSelection selection) {
  const auto acceptable = [selection](const DeviceInfo& device) {
    return selection == DeviceSelection::kAny || device.is_application();
  };

  if (!preferred_path.empty()) {
    for (const DeviceInfo& device : devices) {
      if (device.path == preferred_path && acceptable(device)) {
        return &device;
      }
    }
  }

  for (const DeviceInfo& device : devices) {
    if (acceptable(device)) {
      return &device;
    }
  }

  return nullptr;
}

}  // namespace ddd::capture
