/************************************************************************

    device_programmer.cpp

    The seam between the recovery flow and a device with no firmware
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "device_programmer.h"

#include <algorithm>
#include <cstddef>
#include <set>
#include <thread>
#include <vector>

#include "logger.h"
#include "usb_device.h"
#include "usb_device_info.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

// Request type for a vendor-specific command, host to device.
constexpr uint8_t kVendorWriteRequestType = 0x40;

// The largest single download transfer.
//
// The same figure fx3-programmer has used against this boot ROM since the
// project had a programmer at all, and it is kept rather than raised: the
// application note allows more, but 2 KiB is what years of successful
// programming on this hardware were done with, and the whole download is a
// hundred kilobytes either way.
constexpr size_t kMaximumDownloadBytes = 2048;

// The boot ROM answers at once or not at all, so this deadline is only ever
// reached by something genuinely stuck.
constexpr unsigned int kDownloadTimeoutMilliseconds = 5000;

// How often to look for the device coming back under its new identity. It
// re-enumerates in a second or two.
constexpr auto kReturnPollInterval = std::chrono::milliseconds(250);

class UsbDeviceProgrammer : public IDeviceProgrammer {
 public:
  UsbDeviceProgrammer(IUsbDevice& usb, std::string path,
                      std::unique_ptr<IUsbControlChannel> channel,
                      ILogger* logger)
      : usb_(usb),
        path_(std::move(path)),
        channel_(std::move(channel)),
        logger_(logger) {
    // Which Duplicators were already working when this started. Recorded
    // before anything is sent, because the device that comes back is the one
    // that was not in this set — which is how a second, healthy Duplicator
    // attached to the same machine cannot be mistaken for the one being
    // recovered.
    std::vector<DeviceInfo> devices;
    if (usb_.Enumerate(devices)) {
      for (const DeviceInfo& device : devices) {
        if (device.is_application()) {
          known_application_paths_.insert(device.path);
        }
      }
    }
  }

  bool WriteRam(uint32_t address, std::span<const uint8_t> data) override {
    if (channel_ == nullptr) {
      return false;
    }

    size_t sent = 0;
    while (sent < data.size()) {
      const size_t block = std::min(kMaximumDownloadBytes, data.size() - sent);
      const uint32_t target = address + static_cast<uint32_t>(sent);

      // The address is split across wValue and wIndex, low half first. That
      // is the boot ROM's own convention and the reason the command needs no
      // header of its own.
      buffer_.assign(data.begin() + static_cast<ptrdiff_t>(sent),
                     data.begin() + static_cast<ptrdiff_t>(sent + block));

      const int written = channel_->Transfer(
          kVendorWriteRequestType, kRamDownloadRequest,
          static_cast<uint16_t>(target & 0xFFFF),
          static_cast<uint16_t>((target >> 16) & 0xFFFF),
          std::span<uint8_t>(buffer_), kDownloadTimeoutMilliseconds);

      if (written != static_cast<int>(block)) {
        if (logger_ != nullptr) {
          logger_->Error(
              "The device stopped accepting firmware part way "
              "through");
        }
        return false;
      }

      sent += block;
    }

    return true;
  }

  bool Start(uint32_t entry_address) override {
    if (channel_ == nullptr) {
      return false;
    }

    // A download with no data stage is the jump. The device goes away while
    // answering it, so a transport error here is the expected outcome rather
    // than a failure — see WaitForApplication, which is the evidence that
    // means anything.
    channel_->Transfer(kVendorWriteRequestType, kRamDownloadRequest,
                       static_cast<uint16_t>(entry_address & 0xFFFF),
                       static_cast<uint16_t>((entry_address >> 16) & 0xFFFF),
                       {}, kDownloadTimeoutMilliseconds);

    // Closed here rather than at destruction: the handle refers to a device
    // that has stopped existing under that identity, and holding it open
    // would keep the operating system from settling the new one.
    channel_.reset();
    return true;
  }

  std::optional<std::string> WaitForApplication(
      std::chrono::milliseconds timeout) override {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(kReturnPollInterval);

      std::vector<DeviceInfo> devices;
      if (!usb_.Enumerate(devices)) {
        continue;
      }

      // The path it went away from, first. On libusb that is a bus and port
      // number and does not change with the personality, so this is the
      // answer on Linux and macOS and it is exact.
      for (const DeviceInfo& device : devices) {
        if (device.path == path_ && device.is_application()) {
          return device.path;
        }
      }

      // Windows builds its paths from the device interface, which carries the
      // product identifier, so the path does change when the personality
      // does. The device that came back is then the application-personality
      // device that was not there before this started.
      for (const DeviceInfo& device : devices) {
        if (device.is_application() &&
            known_application_paths_.count(device.path) == 0) {
          return device.path;
        }
      }
    }

    if (logger_ != nullptr) {
      logger_->Error(
          "The device did not restart with the firmware it was given");
    }
    return std::nullopt;
  }

 private:
  IUsbDevice& usb_;
  std::string path_;
  std::unique_ptr<IUsbControlChannel> channel_;
  ILogger* logger_ = nullptr;
  std::vector<uint8_t> buffer_;
  std::set<std::string> known_application_paths_;
};

}  // namespace

std::unique_ptr<IDeviceProgrammer> MakeDeviceProgrammer(IUsbDevice& usb,
                                                        const std::string& path,
                                                        ILogger* logger) {
  std::unique_ptr<IUsbControlChannel> channel = usb.OpenControlChannel(path);
  if (channel == nullptr) {
    if (logger != nullptr) {
      logger->Error("The device in recovery mode could not be opened");
    }
    return nullptr;
  }

  return std::make_unique<UsbDeviceProgrammer>(usb, path, std::move(channel),
                                               logger);
}

}  // namespace ddd::capture
