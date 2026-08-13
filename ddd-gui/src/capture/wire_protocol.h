/************************************************************************

    wire_protocol.h

    How the host addresses the device over USB
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>

// The USB side of the device contract. Nothing here needs libusb, so the values
// are testable and readable by a front end that has not opened a device.
//
// These constants are a deliberate second copy of the ones in gui/ (AGENTS.md
// §2). They are protocol, not shared code.
namespace ddd::capture {

// pid.codes assigned identifiers, in use since the FX3 firmware moved off the
// Cypress defaults.
inline constexpr uint16_t kVendorId = 0x1209;
inline constexpr uint16_t kProductId = 0x2347;

// The bulk IN endpoint the sample stream arrives on
inline constexpr uint8_t kBulkInEndpoint = 0x81;

// The USB interface the endpoint belongs to
inline constexpr uint8_t kInterfaceNumber = 0;

// Vendor request that carries the configuration flags below. The FX3 firmware
// applies them immediately; there is no acknowledgement to read back.
inline constexpr uint8_t kConfigurationRequest = 0xB6;

// Configuration flag bits. Only bit 0 is defined; the rest are reserved and
// must be sent as zero, which is what makes adding a flag later a firmware
// change rather than a protocol break.
inline constexpr uint16_t kConfigurationTestModeFlag = 0x0001;

// Build the configuration word for a given test-mode setting.
inline constexpr uint16_t MakeConfigurationFlags(bool test_mode) {
  return test_mode ? kConfigurationTestModeFlag : uint16_t{0};
}

// The dormant start/stop request.
//
// Recorded rather than used. The current gateware samples continuously from the
// moment the device is opened, so the host never sends this, and a capture
// starts and stops by attaching and detaching a writer rather than by telling
// the device anything. It is here so that a future firmware which does honour
// it does not have to rediscover the request number.
inline constexpr uint8_t kStartStopRequest = 0xB5;

}  // namespace ddd::capture
