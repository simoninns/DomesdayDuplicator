/************************************************************************

    wire_protocol.h

    How the host addresses the device over USB
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
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

// The FPGA register bank.
//
// The FX3 reaches a set of registers in the gateware over a private SPI link
// and relays them to the host through these two requests, which replaced the
// bit-flag configuration request 0xB6. Because they address registers rather
// than named settings, a register added to the gateware later needs no
// firmware change and no new request number to become reachable from here.
//
// The full contract is the "FPGA register interface" page of the
// documentation site.

// Read registers. wValue is the first address, wLength the byte count; the
// address auto-increments, so the identity block is one transfer.
inline constexpr uint8_t kRegisterReadRequest = 0xB7;

// Write one register. The high byte of wValue is the address and the low byte
// is the value, so there is no data stage.
inline constexpr uint8_t kRegisterWriteRequest = 0xB8;

// Register addresses.
inline constexpr uint8_t kRegisterId = 0x00;
inline constexpr uint8_t kRegisterMapVersion = 0x01;
inline constexpr uint8_t kRegisterBuildFlags = 0x02;
inline constexpr uint8_t kRegisterCommit = 0x03;
inline constexpr uint8_t kRegisterTestMode = 0x10;

// The identity block: signature, map version, build flags and eight commit
// characters, contiguous so that one request fetches all of it.
inline constexpr uint8_t kIdentityLength = 11;
inline constexpr uint8_t kCommitLength = 8;

// The fixed value at kRegisterId.
//
// Neither 0x00 nor 0xFF deliberately: SPI has no acknowledgement, so an FPGA
// that is absent or still loading its configuration does not fail a transfer,
// it returns whatever its MISO line carries. This value is what separates a
// real register bank from a floating wire.
inline constexpr uint8_t kIdentityValue = 0x44;

// The register map version this build understands.
inline constexpr uint8_t kIdentityMapVersion = 0x01;

// Bits of the build flags register.
inline constexpr uint8_t kBuildFlagDirty = 0x01;
inline constexpr uint8_t kBuildFlagCommit = 0x02;

// Build the wValue for a register write.
inline constexpr uint16_t MakeRegisterWrite(uint8_t address, uint8_t value) {
  return static_cast<uint16_t>((static_cast<uint16_t>(address) << 8) | value);
}

// Build the wValue that turns the gateware's test pattern on or off.
inline constexpr uint16_t MakeTestModeWrite(bool test_mode) {
  return MakeRegisterWrite(kRegisterTestMode, test_mode ? 1 : 0);
}

// The device update agent.
//
// Six requests on endpoint 0, by which the host hands the FX3 a firmware or
// gateware image and the FX3 writes it to the medium the host cannot reach.
// The full contract is the "Device update mechanism" page of the
// documentation site; this is the host's copy of the numbers.
inline constexpr uint8_t kUpdateStatusRequest = 0xD0;
inline constexpr uint8_t kUpdateBeginRequest = 0xD1;
inline constexpr uint8_t kUpdateDataRequest = 0xD2;
inline constexpr uint8_t kUpdateFinishRequest = 0xD3;
inline constexpr uint8_t kDeviceResetRequest = 0xD4;
inline constexpr uint8_t kFpgaReconfigRequest = 0xD5;

// wIndex selects the target throughout: 0 is the FX3's boot EEPROM, 1 is
// the FPGA's EPCS application image.
inline constexpr uint16_t kUpdateTargetFirmware = 0;
inline constexpr uint16_t kUpdateTargetGateware = 1;

// The fixed sizes of the two packets that are not payload.
inline constexpr size_t kUpdateStatusLength = 16;
inline constexpr size_t kUpdateBeginLength = 40;

// The EEPROM page size, which is the alignment every chunk but the last has
// to respect so the firmware can write a chunk straight to the medium.
inline constexpr size_t kUpdateChunkAlignment = 64;

// The vendor protocol version this build understands, as the firmware
// advertises it in the high byte of bcdDevice.
//
// A range and not a single value, and that is the point of it. A build that
// only accepted the version it shipped alongside would treat every additive
// change as an incompatibility, which is the same as having no versioning
// at all. Below the minimum is firmware too old to speak to; above the
// maximum is firmware that may mean something different by a field of the
// same name, and guessing is how a device gets flashed with something
// nobody described.
inline constexpr int kProtocolVersionMinimum = 1;
inline constexpr int kProtocolVersionMaximum = 1;

// Firmware predating the field at all reports zero, because bcdDevice was a
// dead 0x0000 until the update work needed somewhere to state what the
// firmware speaks. It is not a version and does not compare as one.
inline constexpr int kProtocolVersionUnknown = 0;

// The register map versions this build understands, on the same rule.
inline constexpr int kRegisterMapVersionMinimum = 1;
inline constexpr int kRegisterMapVersionMaximum = 2;

// The dormant start/stop request.
//
// Recorded rather than used. The current gateware samples continuously from the
// moment the device is opened, so the host never sends this, and a capture
// starts and stops by attaching and detaching a writer rather than by telling
// the device anything. It is here so that a future firmware which does honour
// it does not have to rediscover the request number.
inline constexpr uint8_t kStartStopRequest = 0xB5;

}  // namespace ddd::capture
