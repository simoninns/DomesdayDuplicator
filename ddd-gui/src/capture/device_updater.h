/************************************************************************

    device_updater.h

    The seam between the update flow and a device that can be updated
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "digest.h"
#include "wire_protocol.h"

namespace ddd::capture {

class ILogger;
class IUsbDevice;

// Which half of the device an image is going to.
enum class UpdateTarget : uint8_t {
  // The FX3's boot EEPROM, written by the firmware itself over I2C.
  kFirmware = 0,

  // The FPGA's EPCS configuration flash, reached through the gateware's
  // flash bridge. Named here because the protocol has always had two
  // targets; the firmware that answers for it arrives with the bridge.
  kGateware = 1,

  // The same flash, at the factory image's address — the image a board
  // falls back to when the application image is missing or fails its
  // checksum.
  //
  // Written by bring-up and by nothing else. The ordinary update path cannot
  // reach it: the update dialog offers targets 0 and 1,
  // and the firmware refuses this one unless the request carries
  // kUpdateFactoryWriteFlag, so a host that means kGateware and sends this
  // by mistake is refused rather than obeyed.
  //
  // Adding this target was additive and deliberately did not bump the
  // protocol version. Nothing depends on a fallback for firmware without it:
  // the first release carries all three targets, and everything older is the
  // original firmware, which has no update agent to ask.
  kEpcsFactory = 2,
};

const char* UpdateTargetName(UpdateTarget target);

// Where the device says it has got to.
//
// Reported by the 0xD0 status request and answerable at any time, including
// when no update is in progress — which is also how the host discovers the
// chunk size rather than assuming one.
enum class UpdatePhase : uint8_t {
  kIdle = 0,
  kReceiving = 1,
  kWriting = 2,
  kVerifying = 3,
  kComplete = 4,
  kFailed = 5,
};

// Why the device stopped. Mirrors the firmware's codes; the names are the
// host's, and DeviceUpdateErrorText turns one into a sentence.
enum class DeviceUpdateError : uint8_t {
  kNone = 0,
  kBusy = 1,
  kTarget = 2,
  kLength = 3,
  kSequence = 4,
  kChunk = 5,
  kOverrun = 6,
  kShort = 7,
  kStreamDigest = 8,
  kMediumDigest = 9,
  kWrite = 10,
  kRead = 11,
  kImage = 12,
  kHardware = 13,
};

// One sentence, written for the person looking at the screen rather than
// for the person who wrote the firmware: what happened, and — because it is
// always true and always worth saying — that the device is safe.
std::string DeviceUpdateErrorText(DeviceUpdateError error);

// The 16-byte status packet, decoded.
struct DeviceUpdateStatus {
  UpdatePhase phase = UpdatePhase::kIdle;
  DeviceUpdateError error = DeviceUpdateError::kNone;

  // The largest chunk the device accepts. Read rather than assumed, so a
  // firmware that raises it needs no change here.
  uint16_t maximum_chunk_bytes = 0;

  // Three counters, because they move at very different speeds. Streaming a
  // firmware image over EP0 takes seconds; writing it takes tens of
  // seconds; reading it back takes about as long as writing. A single
  // progress bar driven by one of them would fill quickly and then appear
  // to stop.
  uint32_t bytes_received = 0;
  uint32_t bytes_written = 0;
  uint32_t bytes_verified = 0;
};

// Decode a status packet. Returns nothing if the buffer is not exactly the
// 16 bytes the protocol specifies, or names a phase or error this build
// does not know — a device answering with something unrecognised is a
// device this application must not narrate.
std::optional<DeviceUpdateStatus> ParseDeviceUpdateStatus(
    std::span<const uint8_t> packet);

// Who the device says it is, right now.
//
// Everything here is read from the live device rather than remembered, and
// that is the whole point of the type: it is what the post-update
// confirmation compares against the manifest, so an update is proved by
// asking the device rather than by assuming the write worked.
struct DeviceIdentity {
  // The USB product string, which carries the firmware's commit in
  // brackets. Empty if it could not be read.
  std::string product_string;

  // The vendor protocol version from bcdDevice. Read only from a device
  // running this project's own firmware, because the boot ROM and the
  // secondary loader put their own numbering in that field.
  int protocol_version = 0;

  // Whether the gateware's register bank answered at all. False for a
  // device whose FPGA is unconfigured or whose gateware predates the
  // register interface, which is an ordinary state and not a fault.
  bool gateware_present = false;

  int register_map_version = 0;
  std::string gateware_commit;

  // Which of the two gateware images answered.
  //
  // The default is the application image and not the register's own zero,
  // because zero *is* the factory image: an identity that was filled in
  // without this field would otherwise describe a perfectly good unit as
  // being in recovery. Every path that reads the register sets it.
  int image_role = kImageRoleApplication;

  // Is the FPGA running its resident factory image — a unit in gateware
  // recovery, with a register bank and a flash bridge and no capture path
  // at all?
  //
  // False when the register bank did not answer, because a role byte read
  // from a gateware that is not there is not a role byte.
  bool GatewareIsRecovery() const;
};

// A device that can be updated.
//
// The seam the whole update flow is written against. There are two
// implementations: one over the USB backend, and one in the tests that can
// be told to fail at any point — which is what makes every branch of the
// wizard, including the rescue states, exercisable with nothing plugged in.
//
// Every call is blocking and none of them is fast. This is driven from a
// worker thread, never from a user interface thread.
//
// Thread-safety: NOT thread-safe. One thread owns an updater for its
// lifetime.
class IDeviceUpdater {
 public:
  IDeviceUpdater() = default;
  virtual ~IDeviceUpdater() = default;

  IDeviceUpdater(const IDeviceUpdater&) = delete;
  IDeviceUpdater& operator=(const IDeviceUpdater&) = delete;
  IDeviceUpdater(IDeviceUpdater&&) = delete;
  IDeviceUpdater& operator=(IDeviceUpdater&&) = delete;

  // Read the device's identity. Returns nothing if the device could not be
  // reached at all.
  virtual std::optional<DeviceIdentity> ReadIdentity() = 0;

  // 0xD0. Returns nothing if the request could not be made or the answer
  // was not a status packet.
  virtual std::optional<DeviceUpdateStatus> ReadStatus() = 0;

  // 0xD1. The digest arrives before the first byte of payload, which is
  // what lets the firmware hash the incoming stream as it goes and abort
  // before anything is committed.
  virtual bool Begin(UpdateTarget target, uint64_t length,
                     const Sha256Digest& digest) = 0;

  // 0xD2. `index` starts at zero and increments by one; a chunk that
  // arrives out of order fails the transfer rather than being buffered.
  virtual bool SendChunk(UpdateTarget target, uint16_t index,
                         std::span<const uint8_t> data) = 0;

  // 0xD3. Returns whether the request was accepted, which is not whether
  // the update succeeded — the device reads the written region back off
  // the medium afterwards, and the result of that is read with 0xD0.
  virtual bool Finish(UpdateTarget target) = 0;

  // 0xD4. The device re-enumerates, so this is expected to fail in the
  // ordinary ways a request to a device that has just gone away fails; a
  // false return is not by itself a failed update.
  virtual bool Reset() = 0;

  // 0xD5. Reconfiguration stops the clock underneath the capture path, so
  // this is always followed by a reset.
  virtual bool ReconfigureFpga() = 0;

  // Wait for the device to come back after a reset, and return its
  // identity.
  //
  // Part of the seam rather than the caller's problem because the two
  // implementations answer it very differently: the real one re-enumerates
  // the bus, and the test one simply says yes — and a test that had to
  // wait real seconds for a fake device would be a test nobody runs.
  virtual std::optional<DeviceIdentity> WaitForReturn(
      std::chrono::milliseconds timeout) = 0;
};

// An updater for the device at `path`, over whichever USB backend this
// build has.
//
// Returns nothing if the device could not be opened. The updater holds the
// device open for its lifetime, which is what keeps a chunk from costing an
// open and a close.
std::unique_ptr<IDeviceUpdater> MakeDeviceUpdater(IUsbDevice& usb,
                                                  const std::string& path,
                                                  ILogger* logger);

}  // namespace ddd::capture
