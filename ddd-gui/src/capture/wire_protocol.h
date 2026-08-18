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

// The identifiers a Duplicator wears when it is *not* running its own
// firmware.
//
// These are Cypress's, burned into the FX3's boot ROM and into the secondary
// loader, and they are the only way a device with no working firmware can be
// found at all. They are not this project's numbers to change, which is
// exactly why they belong here beside the ones that are: all four are the
// wire, and none of them may be altered by either end alone.
inline constexpr uint16_t kCypressVendorId = 0x04b4;

// The FX3 boot ROM, waiting for a host. What a kit that has never been
// programmed looks like, and what a kit whose EEPROM image the boot ROM
// rejects falls back to.
inline constexpr uint16_t kRecoveryProductId = 0x00f3;

// The Cypress secondary loader, running in RAM. Nothing in this application
// puts a device here; fx3-programmer does, transiently, while it writes an
// EEPROM.
inline constexpr uint16_t kFlashProgrammerProductId = 0x4720;

// The identifiers the Duplicator wore before the pid.codes registration
// above: an OpenMoko-range pair this project never held an allocation for.
//
// Recognised, never spoken to. Firmware wearing these numbers predates the
// register interface and the update protocol alike, so there is nothing this
// application can ask it — but a board running it is a Duplicator, and
// reporting one as "no device attached" to somebody looking straight at it
// is the failure this recognition exists to prevent.
inline constexpr uint16_t kLegacyVendorId = 0x1d50;
inline constexpr uint16_t kLegacyProductId = 0x603b;

// The boot ROM's RAM download command: wValue and wIndex carry the low and
// high halves of the load address, and a transfer with no data stage at the
// end is the jump to the entry point.
//
// Implemented by the boot ROM and by nothing else — once a device is running
// firmware, the ROM is no longer in control and this request is simply not
// answered.
inline constexpr uint8_t kRamDownloadRequest = 0xA0;

// The secondary loader's identification request, and the eight bytes it
// answers with. The only reliable way to tell the loader from anything else
// wearing a Cypress identifier: the product identifier alone is a hint, and
// this is the confirmation.
inline constexpr uint8_t kFlashProgrammerProbeRequest = 0xB0;
inline constexpr size_t kFlashProgrammerProbeLength = 8;
inline constexpr char kFlashProgrammerMagic[] = "FX3PROG";

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
inline constexpr uint8_t kRegisterImageRole = 0x0B;
inline constexpr uint8_t kRegisterTestMode = 0x10;
inline constexpr uint8_t kRegisterDecimation = 0x12;

// What the decimation register holds: the factor, not a flag, so that reading
// it back is a statement of what the capture path is doing rather than an echo
// of what was asked for. The gateware normalises anything it cannot do to
// kDecimationEverySample, which is the only value that cannot produce a wrong
// file.
inline constexpr uint8_t kDecimationEverySample = 0x01;
inline constexpr uint8_t kDecimationHalfRate = 0x02;

// The identity block: signature, map version, build flags, eight commit
// characters and the image role, contiguous so that one request fetches all of
// it. Map version 1 gateware has no image role and returns 0x00 for it, which
// is why the role is only believed when the map version says it exists.
inline constexpr uint8_t kIdentityLength = 12;
inline constexpr uint8_t kCommitLength = 8;

// What the image role register reports. The gateware is two images and only
// one of them can capture, so this is how a host tells a working unit from one
// running its recovery gateware.
inline constexpr uint8_t kImageRoleFactory = 0x00;
inline constexpr uint8_t kImageRoleApplication = 0x01;

// The map version at which the image role became meaningful.
inline constexpr int kRegisterMapVersionWithImageRole = 2;

// The capture buffer's back-pressure instrument.
//
// A read-only block at addresses that used to be unmapped, self-described by a
// signature rather than by a map version — the same arrangement as the remote
// update diagnostics window, and additive in the same way: gateware without it
// reads zero here, and this build then knows it is talking to a device that
// cannot report its buffer rather than to one whose buffer is empty.
//
// Reading kRegisterTelemetryId is what samples the instrument. It is the one
// read in this map with an effect, and it is what makes a reading coherent:
// the counters are sampled into a shadow bank in a single gateware clock and
// the host then reads the shadow, rather than reading live counters a byte at
// a time over a link that takes about 80 microseconds per byte.
inline constexpr uint8_t kRegisterTelemetryId = 0x40;
inline constexpr uint8_t kTelemetryIdValue = 0xBD;

// The whole block, signature through geometry, which is one request.
//
// The geometry is included in every poll rather than read once and remembered
// because a remembered figure is a figure that can belong to a different
// device. Six extra bytes cost about half a millisecond of a poll that happens
// a few times a second.
inline constexpr uint8_t kTelemetryBlockLength = 23;

// The layout this build understands, as the low nibble of the status byte
// reports it. A change to what any field means changes this.
inline constexpr uint8_t kTelemetryFormat = 1;

// Offsets within the block, from kRegisterTelemetryId. Every multi-byte field
// is two bytes, least significant first.
inline constexpr size_t kTelemetryOffsetStatus = 1;
inline constexpr size_t kTelemetryOffsetLatchCount = 2;
inline constexpr size_t kTelemetryOffsetUsedNow = 3;
inline constexpr size_t kTelemetryOffsetPeak = 5;
inline constexpr size_t kTelemetryOffsetPeakLifetime = 7;
inline constexpr size_t kTelemetryOffsetOverflows = 9;
inline constexpr size_t kTelemetryOffsetDropped = 11;
inline constexpr size_t kTelemetryOffsetPackets = 13;
inline constexpr size_t kTelemetryOffsetNearFull = 15;
inline constexpr size_t kTelemetryOffsetDepth = 17;
inline constexpr size_t kTelemetryOffsetPacketWords = 19;
inline constexpr size_t kTelemetryOffsetNearFullWords = 21;

// Bits of the status byte.
inline constexpr uint8_t kTelemetryFormatMask = 0x0F;
inline constexpr uint8_t kTelemetryFlagOverflowSeen = 0x10;
inline constexpr uint8_t kTelemetryFlagSaturated = 0x20;

// Samples per unit of the near-full counter. The gateware prescales it because
// a quarter of a second at or above the threshold is ten million samples and
// the field is sixteen bits.
inline constexpr unsigned kTelemetryNearFullPrescale = 256;

// The fixed value at kRegisterId.
//
// Neither 0x00 nor 0xFF deliberately: SPI has no acknowledgement, so an FPGA
// that is absent or still loading its configuration does not fail a transfer,
// it returns whatever its MISO line carries. This value is what separates a
// real register bank from a floating wire.
inline constexpr uint8_t kIdentityValue = 0x44;

// The newest register map version this build understands. What it *accepts*
// is a range, kRegisterMapVersionMinimum to kRegisterMapVersionMaximum below:
// an application built to accept only the exact version it shipped alongside
// would treat every additive change as an incompatibility, which is the same
// as having no versioning at all.
inline constexpr uint8_t kIdentityMapVersion = 0x02;

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

// Build the wValue that selects the sample rate.
inline constexpr uint16_t MakeDecimationWrite(uint8_t factor) {
  return MakeRegisterWrite(kRegisterDecimation, factor);
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
// the FPGA's EPCS application image, 2 is the same flash at its factory
// address.
inline constexpr uint16_t kUpdateTargetFirmware = 0;
inline constexpr uint16_t kUpdateTargetGateware = 1;
inline constexpr uint16_t kUpdateTargetFactoryGateware = 2;

// The flag words UPDATE_BEGIN may carry, in bytes 36 to 39.
//
// Zero is what an ordinary update sends and the only word the two ordinary
// targets accept. kUpdateFactoryWriteFlag unlocks target 2, which writes the
// image a board falls back to — a thing that is safe when it is meant and
// irreversible when it is not, so the firmware requires the word to be said
// out loud rather than inferring it from the target alone. Copied, never
// computed: the whole value of a magic word is that a host which arrived at
// it by arithmetic has not arrived at it at all.
//
// This is a second copy of UPDATE_FLAG_FACTORY_WRITE in
// fx3/firmware/src/update-protocol.h (AGENTS.md §2 — it is a wire protocol,
// and the two definitions live on two processors).
inline constexpr uint32_t kUpdateFlagsNone = 0x00000000;
inline constexpr uint32_t kUpdateFactoryWriteFlag = 0x57464444;

// The fixed sizes of the two packets that are not payload.
inline constexpr size_t kUpdateStatusLength = 16;
inline constexpr size_t kUpdateBeginLength = 40;

// The alignment every chunk but the last has to respect, so the firmware can
// write a chunk straight to its medium with no assembly buffer in between.
//
// One number for both targets, and it is the larger of the two page sizes:
// the FX3's boot EEPROM has a 64-byte page and the FPGA's configuration
// flash a 256-byte one, and a chunk that is a whole number of the larger is
// a whole number of the smaller as well. A host that rounds the device's
// advertised chunk size down to a multiple of this satisfies both media for
// any advertised size, which is what the alignment is for.
inline constexpr size_t kUpdateChunkAlignment = 256;

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
