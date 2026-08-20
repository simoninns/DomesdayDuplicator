/************************************************************************

    device_updater.cpp

    The seam between the update flow and a device that can be updated
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "device_updater.h"

#include <algorithm>
#include <array>
#include <thread>
#include <vector>

#include "fpga_version.h"
#include "logger.h"
#include "usb_device.h"
#include "usb_device_info.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

// Request type for a vendor-specific command, host to device.
constexpr uint8_t kVendorWriteRequestType = 0x40;

// The same, device to host.
constexpr uint8_t kVendorReadRequestType = 0xC0;

// A deadline for the requests that only read or open, so that a device which
// has gone away mid-update does not hang the worker thread forever. Generous:
// the expected failure is a stall, which returns immediately.
constexpr unsigned int kShortTimeoutMilliseconds = 2000;

// And none for the requests that write to a medium. An UPDATE_DATA chunk is
// thirty-two EEPROM page writes with an acknowledgement poll after each, and
// a host deadline shorter than that would abandon a transfer that was going
// perfectly well — leaving a half-written EEPROM for no reason at all.
constexpr unsigned int kWriteTimeoutMilliseconds = 0;

// How often to look for a device that is coming back after a reset. The FX3
// re-enumerates in a second or two; polling four times a second costs nothing
// and keeps the "restarting" stage honest about when it ended.
constexpr auto kReturnPollInterval = std::chrono::milliseconds(250);

uint16_t TargetIndex(UpdateTarget target) {
  switch (target) {
    case UpdateTarget::kFirmware:
      return kUpdateTargetFirmware;
    case UpdateTarget::kGateware:
      return kUpdateTargetGateware;
    case UpdateTarget::kEpcsFactory:
      return kUpdateTargetFactoryGateware;
  }
  return kUpdateTargetGateware;
}

// The flag word this target's UPDATE_BEGIN must carry.
//
// One place, so that the unlock cannot be spelled differently by two
// callers, and so that adding a target without deciding what it unlocks is
// a compile error rather than a silently zero word.
uint32_t TargetFlags(UpdateTarget target) {
  return target == UpdateTarget::kEpcsFactory ? kUpdateFactoryWriteFlag
                                              : kUpdateFlagsNone;
}

// An updater over a USB control channel. The only implementation that talks
// to hardware; the tests supply their own.
class UsbDeviceUpdater : public IDeviceUpdater {
 public:
  UsbDeviceUpdater(IUsbDevice& usb, std::string path, DeviceInfo enumerated,
                   std::unique_ptr<IUsbControlChannel> channel, ILogger* logger,
                   bool enumerated_known)
      : usb_(usb),
        path_(std::move(path)),
        enumerated_(std::move(enumerated)),
        enumerated_known_(enumerated_known),
        channel_(std::move(channel)),
        logger_(logger) {}

  std::optional<DeviceIdentity> ReadIdentity() override {
    // The channel is closed by Reset() and reopened by WaitForReturn(), so
    // there is a window in which this object has no device. Answering
    // "nothing" is right for it: the device really is not there.
    if (channel_ == nullptr) {
      return std::nullopt;
    }
    if (!enumerated_known_) {
      return std::nullopt;
    }

    DeviceIdentity identity;

    // The product string and the protocol version come from enumeration
    // rather than from a request, because that is where they live: one is a
    // string descriptor and the other is a field of the device descriptor.
    //
    // **From an enumeration made while this object had no channel open**, and
    // that is the whole reason they are carried rather than read here. On the
    // WinUSB backend, listing devices means opening each one — there is no
    // descriptor without a handle — so enumerating from inside a method that
    // holds the device open makes this process ask Windows for a second
    // handle to a device it already has. What comes back is not an error
    // anybody sees: the device is skipped, the list does not contain it, and
    // this returned nothing at all — which read as a board that had not been
    // programmed rather than as a read that could not be made. The libusb
    // backend reads descriptors without opening anything, so the same code
    // was correct on Linux and macOS and wrong on Windows.
    //
    // Refreshed by WaitForReturn, which is the one place the device's
    // descriptors can change underneath this object, and which enumerates
    // before it reopens the channel.
    identity.product_string = enumerated_.product_string;
    identity.protocol_version = enumerated_.protocol_version;

    // The gateware's identity block is read through this channel rather than
    // through IUsbDevice::ReadRegisters, which would open and close the
    // device underneath a channel that already has it open.
    std::vector<uint8_t> registers(kIdentityLength);
    const int read = channel_->Transfer(
        kVendorReadRequestType, kRegisterReadRequest, kRegisterId, 0,
        std::span<uint8_t>(registers), kShortTimeoutMilliseconds);

    if (read == static_cast<int>(registers.size())) {
      // Parsed by the same code the rest of the application parses this
      // block with. Two readers of twelve bytes that disagreed about which
      // byte was the image role would be two readers that disagreed about
      // whether a device could capture.
      const FpgaVersion gateware = ParseFpgaIdentity(registers);

      identity.gateware_present = gateware.present;
      identity.register_map_version = gateware.map_version;
      identity.image_role = gateware.image_role;
      identity.gateware_commit = gateware.commit;
    }

    return identity;
  }

  std::optional<DeviceUpdateStatus> ReadStatus() override {
    if (channel_ == nullptr) {
      return std::nullopt;
    }

    std::array<uint8_t, kUpdateStatusLength> packet{};
    const int read = channel_->Transfer(
        kVendorReadRequestType, kUpdateStatusRequest, 0, 0,
        std::span<uint8_t>(packet), kShortTimeoutMilliseconds);
    if (read != static_cast<int>(packet.size())) {
      return std::nullopt;
    }
    return ParseDeviceUpdateStatus(packet);
  }

  bool Begin(UpdateTarget target, uint64_t length,
             const Sha256Digest& digest) override {
    if (channel_ == nullptr) {
      return false;
    }

    std::array<uint8_t, kUpdateBeginLength> packet{};

    packet[0] = static_cast<uint8_t>(length & 0xFF);
    packet[1] = static_cast<uint8_t>((length >> 8) & 0xFF);
    packet[2] = static_cast<uint8_t>((length >> 16) & 0xFF);
    packet[3] = static_cast<uint8_t>((length >> 24) & 0xFF);
    std::copy(digest.begin(), digest.end(), packet.begin() + 4);

    // Bytes 36 to 39 are the flag word: zero for the two ordinary targets,
    // and the factory-write word for the one target that may write the
    // image a board falls back to. The firmware refuses any other value
    // rather than ignoring it, and refuses each of these two on the targets
    // they do not belong to.
    const uint32_t flags = TargetFlags(target);
    packet[36] = static_cast<uint8_t>(flags & 0xFF);
    packet[37] = static_cast<uint8_t>((flags >> 8) & 0xFF);
    packet[38] = static_cast<uint8_t>((flags >> 16) & 0xFF);
    packet[39] = static_cast<uint8_t>((flags >> 24) & 0xFF);

    const int sent = channel_->Transfer(
        kVendorWriteRequestType, kUpdateBeginRequest, 0, TargetIndex(target),
        std::span<uint8_t>(packet), kShortTimeoutMilliseconds);
    return sent == static_cast<int>(packet.size());
  }

  bool SendChunk(UpdateTarget target, uint16_t index,
                 std::span<const uint8_t> data) override {
    if (channel_ == nullptr) {
      return false;
    }

    // The channel's data stage is mutable because a control transfer may be
    // either direction; this one is out, so the copy is only to satisfy the
    // signature. Reusing one buffer keeps it out of the per-chunk cost.
    chunk_.assign(data.begin(), data.end());

    const int sent = channel_->Transfer(
        kVendorWriteRequestType, kUpdateDataRequest, index, TargetIndex(target),
        std::span<uint8_t>(chunk_), kWriteTimeoutMilliseconds);
    return sent == static_cast<int>(chunk_.size());
  }

  bool Finish(UpdateTarget target) override {
    if (channel_ == nullptr) {
      return false;
    }

    const int sent =
        channel_->Transfer(kVendorWriteRequestType, kUpdateFinishRequest, 0,
                           TargetIndex(target), {}, kShortTimeoutMilliseconds);
    return sent >= 0;
  }

  bool Reset() override {
    if (channel_ == nullptr) {
      return false;
    }

    // The device goes away while answering this, so a transport error here
    // is the expected outcome and not a failure. The caller finds out
    // whether the reset worked by waiting for the device to come back, which
    // is the only evidence that means anything.
    channel_->Transfer(kVendorWriteRequestType, kDeviceResetRequest, 0, 0, {},
                       kShortTimeoutMilliseconds);

    // The channel is closed here rather than at destruction: the handle
    // refers to a device that has stopped existing, and holding it open
    // would keep the operating system from noticing the new one.
    channel_.reset();
    return true;
  }

  bool ReconfigureFpga() override {
    if (channel_ == nullptr) {
      return false;
    }

    const int sent = channel_->Transfer(
        kVendorWriteRequestType, kFpgaReconfigRequest, 0, kUpdateTargetGateware,
        {}, kShortTimeoutMilliseconds);
    return sent >= 0;
  }

  std::optional<DeviceIdentity> WaitForReturn(
      std::chrono::milliseconds timeout) override {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    // Counted so that the failure below can say what was actually done rather
    // than only that it did not work. The two ways of not finding a device
    // read identically to a user and have entirely different causes: an
    // enumeration that kept failing is a USB subsystem in trouble, while
    // hundreds of successful enumerations that never once listed the device
    // is a host that is not being told the device came back — which is what a
    // sandbox with no route for kernel uevents looks like from in here.
    int enumerations = 0;
    int failures = 0;

    while (std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(kReturnPollInterval);

      std::vector<DeviceInfo> devices;
      if (!usb_.Enumerate(devices)) {
        ++failures;
        continue;
      }
      ++enumerations;

      const auto found = std::find_if(
          devices.begin(), devices.end(),
          [this](const DeviceInfo& info) { return info.path == path_; });
      if (found == devices.end()) {
        continue;
      }

      // Taken now, before anything is opened. The device that came back is
      // running different firmware from the one that went away, so its
      // descriptors are the ones this enumeration just read and not the ones
      // this object was built with — and here is the only moment they can be
      // read without a handle of ours in the way. See ReadIdentity.
      enumerated_ = *found;
      enumerated_known_ = true;

      // Present is not the same as ready: on Linux the device node appears
      // before the permissions are settled, so the channel is reopened and
      // only a device that answers counts as back.
      channel_ = usb_.OpenControlChannel(path_);
      if (channel_ == nullptr) {
        continue;
      }

      std::optional<DeviceIdentity> identity = ReadIdentity();
      if (identity.has_value()) {
        return identity;
      }
      channel_.reset();
    }

    if (logger_ != nullptr) {
      logger_->Error("The device did not come back after being restarted: " +
                     std::to_string(enumerations) +
                     " enumerations listed no device at " + path_ + ", and " +
                     std::to_string(failures) + " could not be made at all");
    }
    return std::nullopt;
  }

 private:
  IUsbDevice& usb_;
  std::string path_;

  // What the bus said about this device when nothing of ours had it open.
  DeviceInfo enumerated_;
  bool enumerated_known_ = false;

  std::unique_ptr<IUsbControlChannel> channel_;
  ILogger* logger_ = nullptr;
  std::vector<uint8_t> chunk_;
};

}  // namespace

bool DeviceIdentity::GatewareIsRecovery() const {
  // The same rule FpgaVersion applies to the same registers, and it has to
  // be: a role byte read from a gateware that did not answer is a byte that
  // means nothing, and reading it as "factory" would put a working device
  // into a recovery state that does not exist for it.
  return gateware_present && image_role == kImageRoleFactory;
}

const char* UpdateTargetName(UpdateTarget target) {
  switch (target) {
    case UpdateTarget::kFirmware:
      return "firmware";
    case UpdateTarget::kGateware:
      return "gateware";
    case UpdateTarget::kEpcsFactory:
      return "factory gateware";
  }
  return "unknown";
}

const char* DeviceUpdateErrorName(DeviceUpdateError error) {
  switch (error) {
    case DeviceUpdateError::kNone:
      return "none";
    case DeviceUpdateError::kBusy:
      return "busy";
    case DeviceUpdateError::kTarget:
      return "target";
    case DeviceUpdateError::kLength:
      return "length";
    case DeviceUpdateError::kSequence:
      return "sequence";
    case DeviceUpdateError::kChunk:
      return "chunk";
    case DeviceUpdateError::kOverrun:
      return "overrun";
    case DeviceUpdateError::kShort:
      return "short";
    case DeviceUpdateError::kStreamDigest:
      return "stream digest";
    case DeviceUpdateError::kMediumDigest:
      return "medium digest";
    case DeviceUpdateError::kWrite:
      return "write";
    case DeviceUpdateError::kRead:
      return "read";
    case DeviceUpdateError::kImage:
      return "image";
    case DeviceUpdateError::kHardware:
      return "hardware";
  }
  return "unknown";
}

const char* UpdatePhaseName(UpdatePhase phase) {
  switch (phase) {
    case UpdatePhase::kIdle:
      return "idle";
    case UpdatePhase::kReceiving:
      return "receiving";
    case UpdatePhase::kWriting:
      return "writing";
    case UpdatePhase::kVerifying:
      return "verifying";
    case UpdatePhase::kComplete:
      return "complete";
    case UpdatePhase::kFailed:
      return "failed";
  }
  return "unknown";
}

std::string DeviceUpdateErrorText(DeviceUpdateError error) {
  // Every one of these names what happened and, where it is not obvious,
  // what to do about it. None of them says "error 9". The device is safe in
  // all of them — an update that stops before its last write leaves the
  // previous image in place or leaves the device in a rescue state the
  // application can repair — and the ones a user is most likely to meet say
  // so, because that is the question they are actually asking.
  switch (error) {
    case DeviceUpdateError::kNone:
      return "No error.";
    case DeviceUpdateError::kBusy:
      return "The device is capturing. Stop the capture and try again.";
    case DeviceUpdateError::kTarget:
      return "This device's firmware cannot update that part of itself yet.";
    case DeviceUpdateError::kLength:
      return "The device refused the update's size. The file may not be a "
             "firmware image for this device.";
    case DeviceUpdateError::kSequence:
      return "The device and the application lost step with each other. "
             "Nothing was changed on the device; try again.";
    case DeviceUpdateError::kChunk:
      return "The device refused a block of the update. Nothing was changed "
             "on the device; try again.";
    case DeviceUpdateError::kOverrun:
      return "More data arrived than the update said it would send. Nothing "
             "was changed on the device.";
    case DeviceUpdateError::kShort:
      return "The update finished before all of it had been sent. Nothing was "
             "changed on the device.";
    case DeviceUpdateError::kStreamDigest:
      return "The update did not arrive at the device intact. Nothing was "
             "written; try again, and try a different USB port or cable if it "
             "happens twice.";
    case DeviceUpdateError::kMediumDigest:
      return "The device could not confirm what it had written. The update "
             "was not completed, and the device will start in recovery mode "
             "so it can be repaired.";
    case DeviceUpdateError::kWrite:
      return "The device could not write to its own memory. The update was "
             "not completed.";
    case DeviceUpdateError::kRead:
      return "The device could not read back what it had written. The update "
             "was not completed.";
    case DeviceUpdateError::kImage:
      return "That file is not firmware this device can run.";
    case DeviceUpdateError::kHardware:
      return "This device cannot update its own firmware — the part of it "
             "that writes the firmware did not start.";
  }

  // A device reporting a code this build does not know is a device speaking a
  // protocol this build should not be narrating.
  return "The device reported a problem this version of the application does "
         "not recognise.";
}

std::optional<DeviceUpdateStatus> ParseDeviceUpdateStatus(
    std::span<const uint8_t> packet) {
  if (packet.size() != kUpdateStatusLength) {
    return std::nullopt;
  }

  if (packet[0] > static_cast<uint8_t>(UpdatePhase::kFailed)) {
    return std::nullopt;
  }
  if (packet[1] > static_cast<uint8_t>(DeviceUpdateError::kHardware)) {
    return std::nullopt;
  }

  const auto read32 = [&packet](size_t offset) {
    return static_cast<uint32_t>(packet[offset]) |
           (static_cast<uint32_t>(packet[offset + 1]) << 8) |
           (static_cast<uint32_t>(packet[offset + 2]) << 16) |
           (static_cast<uint32_t>(packet[offset + 3]) << 24);
  };

  DeviceUpdateStatus status;
  status.phase = static_cast<UpdatePhase>(packet[0]);
  status.error = static_cast<DeviceUpdateError>(packet[1]);
  status.maximum_chunk_bytes = static_cast<uint16_t>(
      static_cast<uint16_t>(packet[2]) |
      static_cast<uint16_t>(static_cast<uint16_t>(packet[3]) << 8));
  status.bytes_received = read32(4);
  status.bytes_written = read32(8);
  status.bytes_verified = read32(12);

  return status;
}

std::unique_ptr<IDeviceUpdater> MakeDeviceUpdater(IUsbDevice& usb,
                                                  const std::string& path,
                                                  ILogger* logger) {
  // Before the channel, deliberately. Everything the updater reports that
  // comes from a descriptor rather than from a request is read here, while
  // nothing of ours holds the device — which on Windows is the only time it
  // can be read at all. See UsbDeviceUpdater::ReadIdentity.
  DeviceInfo enumerated;
  bool found_it = false;
  std::vector<DeviceInfo> devices;
  if (usb.Enumerate(devices)) {
    const auto found = std::find_if(
        devices.begin(), devices.end(),
        [&path](const DeviceInfo& info) { return info.path == path; });
    if (found != devices.end()) {
      enumerated = *found;
      found_it = true;
    }
  }

  std::unique_ptr<IUsbControlChannel> channel = usb.OpenControlChannel(path);
  if (channel == nullptr) {
    if (logger != nullptr) {
      logger->Error("The device could not be opened for updating");
    }
    return nullptr;
  }

  return std::make_unique<UsbDeviceUpdater>(
      usb, path, std::move(enumerated), std::move(channel), logger, found_it);
}

}  // namespace ddd::capture
