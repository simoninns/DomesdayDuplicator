/************************************************************************

    usb_device_info.h

    What the application knows about a device before it opens it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <string>
#include <vector>

namespace ddd::capture {

// The link speed a device negotiated.
//
// Ordered, and the ordering is used: anything at or above kSuper can carry the
// stream, anything below cannot. The values mirror the USB specification's own
// tiers rather than either backend's numbering, so the same comparison works on
// both.
enum class DeviceSpeed {
  kUnknown,
  kLow,
  kFull,
  kHigh,
  kSuper,
  kSuperPlus,
};

// A short label for logs and for the device list ("SuperSpeed", "High-speed").
const char* DeviceSpeedName(DeviceSpeed speed);

// Whether a link at this speed can carry a capture.
//
// This is the check the old backends did not make. High-speed USB 2 tops out at
// 60 MB/s of theoretical bulk bandwidth and rather less in practice, against
// the 80 MB/s the device produces continuously — so a device on a USB 2 port
// cannot work, and the old application would open it anyway and fail some
// seconds later with a sequence mismatch. That error sends a user looking for a
// bad cable or a slow disc; the real answer is the port they plugged into.
//
// kUnknown is treated as sufficient rather than insufficient. A backend that
// cannot determine the speed should not be able to veto a device that might be
// fine — the sequence markers will catch it if it is not, which is the same
// position the old application was in and no worse.
inline bool SpeedCanCarryCapture(DeviceSpeed speed) {
  return speed == DeviceSpeed::kUnknown || speed >= DeviceSpeed::kSuper;
}

// One attached device, as discovered without opening it for capture.
struct DeviceInfo {
  // A stable identifier for this physical port, used to remember which device a
  // user chose when several are attached. Built from bus and port numbers on
  // libusb and from the device instance path on Windows, so it survives a
  // reboot but not moving the cable to another socket — which is the behaviour
  // wanted, since moving the cable is how a user picks a different device.
  std::string path;

  // The USB product string, which carries the firmware's commit hash. Empty if
  // it could not be read; see firmware_version.h for what is done with it.
  std::string product_string;

  DeviceSpeed speed = DeviceSpeed::kUnknown;

  bool CanCarryCapture() const { return SpeedCanCarryCapture(speed); }

  // Compared field by field so that a device which has changed — replugged into
  // a USB 3 port, or updated to different firmware — counts as a change and is
  // reported, rather than being treated as the same device because it is at the
  // same path.
  bool operator==(const DeviceInfo& other) const {
    return path == other.path && product_string == other.product_string &&
           speed == other.speed;
  }
  bool operator!=(const DeviceInfo& other) const { return !(*this == other); }
};

// Pick the device to use from what is attached.
//
// Returns the preferred path if it is present, the first attached device if it
// is not, and nothing at all if the list is empty. Split out from the backends
// because both had their own copy of it and because the rule — a remembered
// preference is a preference, not a requirement — is worth stating once and
// testing once.
const DeviceInfo* SelectDevice(const std::vector<DeviceInfo>& devices,
                               const std::string& preferred_path);

}  // namespace ddd::capture
