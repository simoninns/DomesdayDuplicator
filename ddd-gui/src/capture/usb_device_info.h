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

// Which piece of software the FX3 is running, which decides what the device
// can be asked to do.
//
// The FX3 has no flash of its own: it boots from an I2C EEPROM, and if the
// boot ROM does not find a valid image there it stops and waits for a host
// instead. That is not a fault state — it is the state every kit leaves the
// factory in — so the application has to be able to recognise it and say
// something useful about it, rather than reporting "no device attached" to
// somebody looking straight at one.
enum class DevicePersonality {
  // 1209:2347 — the Duplicator's own firmware. The only personality that
  // captures, and the only one that answers the register and update
  // requests.
  kApplication,

  // 04b4:00f3 — the FX3 boot ROM, waiting for a host to hand it something to
  // run. A device is here because its EEPROM has never been written, because
  // an update was interrupted, or because the PMODE jumper is fitted; those
  // are indistinguishable on the wire and all three are repaired the same
  // way.
  kRecovery,

  // 04b4:4720 — the Cypress secondary loader, left running in RAM by an
  // fx3-programmer session that did not finish. Nothing this application
  // does puts a device here, but it is worth recognising, because the only
  // way out is a power cycle and a device that says nothing looks broken.
  kFlashProgrammer,
};

// A short label for logs. What the user sees is the interface layer's
// business — see update_text.h, where the wording matches the documentation.
const char* DevicePersonalityName(DevicePersonality personality);

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

  // The vendor protocol version, from the high byte of the device
  // descriptor's bcdDevice field.
  //
  // Read during enumeration, before the device is opened and without sending
  // it a single request, which is what makes it usable as a compatibility
  // gate: a device speaking a protocol this build does not understand can be
  // recognised before anything is asked of it. Zero for firmware predating
  // the field, where bcdDevice was a dead 0x0000 — that is not a version and
  // must not be compared as one.
  int protocol_version = 0;

  DeviceSpeed speed = DeviceSpeed::kUnknown;

  // Which software the device is running. Determined from the USB identifiers
  // during enumeration, so it costs nothing and is known before anything is
  // asked of the device.
  DevicePersonality personality = DevicePersonality::kApplication;

  // Whether this device is running the Duplicator's own firmware, and so can
  // capture, answer registers and update itself.
  bool is_application() const {
    return personality == DevicePersonality::kApplication;
  }

  // Whether the *link* can carry a capture. Deliberately still only about the
  // speed: a device in a recovery personality is refused a capture by
  // selection, and answering "no" here would put a message about USB 2 ports
  // in front of somebody whose device is in recovery mode.
  bool CanCarryCapture() const { return SpeedCanCarryCapture(speed); }

  // Compared field by field so that a device which has changed — replugged into
  // a USB 3 port, or updated to different firmware — counts as a change and is
  // reported, rather than being treated as the same device because it is at the
  // same path. The personality is in the comparison because a device that has
  // just fallen back to its boot ROM is at the same path as the one that was
  // working a moment ago, and that transition is the single most important
  // change this application can be told about.
  bool operator==(const DeviceInfo& other) const {
    return path == other.path && product_string == other.product_string &&
           protocol_version == other.protocol_version && speed == other.speed &&
           personality == other.personality;
  }
  bool operator!=(const DeviceInfo& other) const { return !(*this == other); }
};

// Which devices a selection is willing to consider.
enum class DeviceSelection {
  // Only devices running the Duplicator's own firmware. Everything to do with
  // capturing, and everything that speaks the register or update protocol.
  kCaptureCapable,

  // Any device this application recognises, including one sitting in its boot
  // ROM. The firmware dialog and ddd-update use this, because a device that
  // can do nothing else is exactly the device they exist to fix.
  kAny,
};

// Pick the device to use from what is attached.
//
// Returns the preferred path if it is present and acceptable, the first
// acceptable device if it is not, and nothing at all if none is. Split out from
// the backends because both had their own copy of it and because the rule — a
// remembered preference is a preference, not a requirement — is worth stating
// once and testing once.
//
// The default is the narrow set on purpose. A caller that has not thought
// about devices in a recovery personality should not be handed one, and the
// two callers that have thought about it say so at the call site.
const DeviceInfo* SelectDevice(
    const std::vector<DeviceInfo>& devices, const std::string& preferred_path,
    DeviceSelection selection = DeviceSelection::kCaptureCapable);

}  // namespace ddd::capture
