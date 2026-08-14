/************************************************************************

    libusb_device.cpp

    Finding and configuring the device with libusb (Linux and macOS)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2024 Roger Sanders
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "libusb_device.h"

#include <libusb.h>

#include <array>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "libusb_source.h"
#include "logger.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

// USB allows at most seven hubs between a root port and a device, so a path can
// never need more entries than this.
constexpr int kMaximumPortDepth = 7;

// Control transfers have no deadline. The device answers immediately or it has
// gone, and a timeout here would only turn a hung device into a hung
// application some seconds later.
constexpr unsigned int kControlTimeoutMilliseconds = 0;

// Request type for a vendor-specific command, host to device, no data stage.
constexpr uint8_t kVendorRequestType = 0x40;

// The same, device to host, with a data stage.
constexpr uint8_t kVendorReadRequestType = 0xC0;

// Register reads do get a deadline, unlike every other control transfer here.
//
// The reason is where they run: the register read happens on the GUI thread,
// as part of noticing a device has arrived, so a device that neither answers
// nor stalls would freeze the window rather than merely one operation. The
// expected failure — firmware with no register interface — stalls and returns
// at once, so this deadline is only ever reached by something genuinely stuck.
constexpr unsigned int kRegisterReadTimeoutMilliseconds = 1000;

// Which of the three identities this descriptor is, if it is one of them.
//
// The match is on exact identifier pairs and not on the Cypress vendor
// identifier alone, and that matters: the SuperSpeed Explorer Kit carries an
// on-board USB-UART at 04b4:0007 which is powered whenever the board is, and a
// wildcard would list the debug serial port as a Duplicator in recovery.
std::optional<DevicePersonality> PersonalityFromDescriptor(
    const libusb_device_descriptor& descriptor) {
  if (descriptor.idVendor == kVendorId && descriptor.idProduct == kProductId) {
    return DevicePersonality::kApplication;
  }
  if (descriptor.idVendor == kCypressVendorId) {
    if (descriptor.idProduct == kRecoveryProductId) {
      return DevicePersonality::kRecovery;
    }
    if (descriptor.idProduct == kFlashProgrammerProductId) {
      return DevicePersonality::kFlashProgrammer;
    }
  }
  return std::nullopt;
}

DeviceSpeed SpeedFromLibUsb(int speed) {
  switch (speed) {
    case LIBUSB_SPEED_LOW:
      return DeviceSpeed::kLow;
    case LIBUSB_SPEED_FULL:
      return DeviceSpeed::kFull;
    case LIBUSB_SPEED_HIGH:
      return DeviceSpeed::kHigh;
    case LIBUSB_SPEED_SUPER:
      return DeviceSpeed::kSuper;
    default:
      break;
  }

  // Named by comparison rather than by constant so that this builds against
  // libusb releases predating SUPER_PLUS and its 20 Gbit successor, both of
  // which are simply "fast enough" as far as this application is concerned.
  if (speed > LIBUSB_SPEED_SUPER) {
    return DeviceSpeed::kSuperPlus;
  }
  return DeviceSpeed::kUnknown;
}

// An identifier for the physical port a device is plugged into.
//
// Shaped like a Linux sysfs USB path because that makes it recognisable to
// anyone who has looked at one, but it is built by hand: libusb has no notion
// of device paths, and something stable across reboots is needed to remember
// which of several attached devices a user chose.
std::string BuildDevicePath(libusb_device* device) {
  std::array<uint8_t, kMaximumPortDepth> ports{};
  const int port_count =
      libusb_get_port_numbers(device, ports.data(), kMaximumPortDepth);
  if (port_count <= 0) {
    return {};
  }

  std::string path = "/sys/bus/usb/devices/" +
                     std::to_string(libusb_get_bus_number(device)) + "-" +
                     std::to_string(ports[0]);
  for (int index = 1; index < port_count; ++index) {
    path += "." + std::to_string(ports[index]);
  }
  return path;
}

// A device list that frees itself. libusb hands out a reference-counted array
// and every early return in the code below would otherwise have to remember to
// give it back.
class ScopedDeviceList {
 public:
  ScopedDeviceList() = default;
  ~ScopedDeviceList() {
    if (list_ != nullptr) {
      libusb_free_device_list(list_, 1);
    }
  }

  ScopedDeviceList(const ScopedDeviceList&) = delete;
  ScopedDeviceList& operator=(const ScopedDeviceList&) = delete;
  ScopedDeviceList(ScopedDeviceList&&) = delete;
  ScopedDeviceList& operator=(ScopedDeviceList&&) = delete;

  ssize_t Acquire(libusb_context* context) {
    count_ = libusb_get_device_list(context, &list_);
    return count_;
  }

  libusb_device* At(ssize_t index) const { return list_[index]; }
  ssize_t count() const { return count_; }

 private:
  libusb_device** list_ = nullptr;
  ssize_t count_ = 0;
};

// A device held open for a run of control transfers — the update path's
// channel. See IUsbControlChannel for why the register requests do not use
// one and this does.
class LibUsbControlChannel : public IUsbControlChannel {
 public:
  LibUsbControlChannel(libusb_device_handle* handle, bool claimed,
                       ILogger* logger)
      : handle_(handle), claimed_(claimed), logger_(logger) {}

  ~LibUsbControlChannel() override {
    if (claimed_) {
      libusb_release_interface(handle_, kInterfaceNumber);
    }
    libusb_close(handle_);
  }

  int Transfer(uint8_t request_type, uint8_t request, uint16_t value,
               uint16_t index, std::span<uint8_t> data,
               unsigned int timeout_milliseconds) override {
    const int result = libusb_control_transfer(
        handle_, request_type, request, value, index, data.data(),
        static_cast<uint16_t>(data.size()), timeout_milliseconds);

    // A stall arrives as LIBUSB_ERROR_PIPE and is how a device says it does
    // not implement a request. It is the caller's business, not an error to
    // log: the update flow asks devices things they may not be able to do,
    // and finding out is the point of asking.
    if (result < 0 && result != LIBUSB_ERROR_PIPE && logger_ != nullptr) {
      logger_->Debug(std::string("Update control transfer 0x") +
                     std::to_string(request) +
                     " failed: " + libusb_error_name(result));
    }
    return result;
  }

 private:
  libusb_device_handle* handle_ = nullptr;
  bool claimed_ = false;
  ILogger* logger_ = nullptr;
};

class LibUsbDevice : public IUsbDevice {
 public:
  explicit LibUsbDevice(ILogger* logger) : logger_(logger) {}

  ~LibUsbDevice() override {
    if (context_ != nullptr) {
      libusb_exit(context_);
    }
  }

  bool Initialise() {
#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x0100010A
    const int started = libusb_init_context(&context_, nullptr, 0);
#else
    const int started = libusb_init(&context_);
#endif
    if (started != 0) {
      context_ = nullptr;
      if (logger_ != nullptr) {
        logger_->Error(std::string("libusb could not be started: ") +
                       libusb_error_name(started));
      }
      return false;
    }
    return true;
  }

  const char* Name() const override { return "libusb"; }

  bool Enumerate(std::vector<DeviceInfo>& devices) override {
    devices.clear();

    ScopedDeviceList list;
    const ssize_t count = list.Acquire(context_);
    if (count < 0) {
      if (logger_ != nullptr) {
        logger_->Error(std::string("Listing USB devices failed: ") +
                       libusb_error_name(static_cast<int>(count)));
      }
      return false;
    }

    for (ssize_t index = 0; index < count; ++index) {
      libusb_device* const device = list.At(index);

      libusb_device_descriptor descriptor{};
      if (libusb_get_device_descriptor(device, &descriptor) != 0) {
        continue;
      }
      const std::optional<DevicePersonality> personality =
          PersonalityFromDescriptor(descriptor);
      if (!personality.has_value()) {
        continue;
      }

      // The flash programmer's identifier is a hint and the 0xB0 probe is the
      // confirmation. A Cypress board that happens to share the identifier but
      // does not answer the probe is somebody else's device and is dropped
      // rather than announced as a half-programmed Duplicator.
      if (*personality == DevicePersonality::kFlashProgrammer &&
          !AnswersFlashProgrammerProbe(device)) {
        continue;
      }

      DeviceInfo info;
      info.path = BuildDevicePath(device);
      info.speed = SpeedFromLibUsb(libusb_get_device_speed(device));
      info.product_string = ReadProductString(device, descriptor.iProduct);
      info.personality = *personality;

      // The vendor protocol version lives in the high byte of bcdDevice, so
      // it arrives with the descriptor and costs nothing: no open, no
      // claim, no request.
      //
      // Only for our own firmware, though. The boot ROM and the secondary
      // loader put their own numbering in that field, and reading one of
      // those as a protocol version would have the application deciding what
      // a device supports from a number that means something else.
      if (*personality == DevicePersonality::kApplication) {
        info.protocol_version = (descriptor.bcdDevice >> 8) & 0xFF;
      }

      devices.push_back(std::move(info));
    }

    return true;
  }

  bool WriteRegister(const std::string& path, uint8_t address,
                     uint8_t value) override {
    libusb_device_handle* handle = nullptr;
    if (!Open(path, DeviceSelection::kCaptureCapable, handle, nullptr)) {
      return false;
    }

    bool claimed = libusb_claim_interface(handle, kInterfaceNumber) == 0;
    const int sent = libusb_control_transfer(
        handle, kVendorRequestType, kRegisterWriteRequest,
        MakeRegisterWrite(address, value), 0, nullptr, 0,
        kControlTimeoutMilliseconds);

    if (claimed) {
      libusb_release_interface(handle, kInterfaceNumber);
    }
    libusb_close(handle);

    if (sent < 0) {
      if (logger_ != nullptr) {
        logger_->Error(std::string("Writing a device register failed: ") +
                       libusb_error_name(sent));
      }
      return false;
    }
    return true;
  }

  bool ReadRegisters(const std::string& path, uint8_t address, uint8_t length,
                     std::vector<uint8_t>& data) override {
    if (length == 0) {
      return false;
    }

    libusb_device_handle* handle = nullptr;
    if (!Open(path, DeviceSelection::kCaptureCapable, handle, nullptr)) {
      return false;
    }

    std::vector<uint8_t> buffer(length, 0);

    bool claimed = libusb_claim_interface(handle, kInterfaceNumber) == 0;
    const int received = libusb_control_transfer(
        handle, kVendorReadRequestType, kRegisterReadRequest, address, 0,
        buffer.data(), length, kRegisterReadTimeoutMilliseconds);

    if (claimed) {
      libusb_release_interface(handle, kInterfaceNumber);
    }
    libusb_close(handle);

    // A device that does not implement the register interface stalls, which
    // arrives here as LIBUSB_ERROR_PIPE. That is the expected answer from
    // firmware older than the interface, so it is not logged as an error —
    // the caller reports "unknown" and carries on.
    if (received != static_cast<int>(length)) {
      return false;
    }

    data = std::move(buffer);
    return true;
  }

  std::unique_ptr<ISampleSource> OpenSource(const std::string& path,
                                            const UsbSourceOptions& options,
                                            TransferResult& result) override {
    result = TransferResult::kConnectionFailure;

    libusb_device_handle* handle = nullptr;
    DeviceInfo info;
    if (!Open(path, DeviceSelection::kCaptureCapable, handle, &info)) {
      return nullptr;
    }

    // The speed check the old backends did not make. Refusing here, before a
    // single transfer, is the difference between "this port cannot carry a
    // capture" and a sequence mismatch ten seconds in that reads like a
    // hardware fault.
    if (!info.CanCarryCapture()) {
      libusb_close(handle);
      if (logger_ != nullptr) {
        logger_->Error(
            std::string("The device is attached at ") +
            DeviceSpeedName(info.speed) +
            ", which cannot carry 80 MB/s. Move it to a USB 3 port.");
      }
      return nullptr;
    }

    // Claimed before the descriptor is read, not after: libusb only reports a
    // meaningful wMaxPacketSize for an interface the caller owns, and that
    // figure is what every buffer size downstream is rounded to.
    const int claimed = libusb_claim_interface(handle, kInterfaceNumber);
    if (claimed != 0) {
      libusb_close(handle);
      if (logger_ != nullptr) {
        logger_->Error(std::string("Claiming the USB interface failed: ") +
                       libusb_error_name(claimed) +
                       ". Another application may be using the device.");
      }
      return nullptr;
    }

    uint8_t endpoint = 0;
    size_t max_packet_bytes = 0;
    if (!FindBulkInEndpoint(handle, endpoint, max_packet_bytes)) {
      libusb_release_interface(handle, kInterfaceNumber);
      libusb_close(handle);
      return nullptr;
    }

    if (logger_ != nullptr) {
      logger_->Info("Opened the device at " + info.path + " (" +
                    DeviceSpeedName(info.speed) + "), endpoint packet size " +
                    std::to_string(max_packet_bytes) + " bytes");
    }

    result = TransferResult::kSuccess;
    return std::make_unique<LibUsbSource>(context_, handle, endpoint,
                                          max_packet_bytes, options, logger_);
  }

  std::unique_ptr<IUsbControlChannel> OpenControlChannel(
      const std::string& path) override {
    libusb_device_handle* handle = nullptr;
    // Any personality: this is the channel the update and recovery paths
    // use, and a device in recovery is precisely the one they open.
    if (!Open(path, DeviceSelection::kAny, handle, nullptr)) {
      return nullptr;
    }

    // The interface is claimed if it can be, and the channel works either
    // way. Control transfers on endpoint zero do not require a claim, and
    // insisting on one would refuse to update a device that some other
    // process happened to have open — which is exactly the state a device
    // gets into when a previous attempt left something behind.
    const bool claimed = libusb_claim_interface(handle, kInterfaceNumber) == 0;

    return std::make_unique<LibUsbControlChannel>(handle, claimed, logger_);
  }

 private:
  // Read the USB product string, which is where the firmware puts its commit
  // hash. Failure is ordinary rather than exceptional — on Linux it usually
  // means the udev rule is missing and the device cannot be opened at all — so
  // it produces an empty string rather than dropping the device from the list.
  std::string ReadProductString(libusb_device* device, uint8_t index) {
    if (index == 0) {
      return {};
    }

    libusb_device_handle* handle = nullptr;
    if (libusb_open(device, &handle) != 0) {
      return {};
    }

    std::array<unsigned char, 256> buffer{};
    const int length = libusb_get_string_descriptor_ascii(
        handle, index, buffer.data(), static_cast<int>(buffer.size()));
    libusb_close(handle);

    if (length <= 0) {
      return {};
    }
    return std::string(reinterpret_cast<const char*>(buffer.data()),
                       static_cast<size_t>(length));
  }

  // Ask a device whether it is the Cypress secondary loader.
  //
  // Opened and closed here rather than left open: this runs during
  // enumeration, five times a second, and only for a device already wearing
  // the loader's identifier — which is a device that has been left behind by
  // a programming session and is not present on any ordinary run.
  static bool AnswersFlashProgrammerProbe(libusb_device* device) {
    libusb_device_handle* handle = nullptr;
    if (libusb_open(device, &handle) != 0) {
      return false;
    }

    std::array<unsigned char, kFlashProgrammerProbeLength> answer{};
    const int read = libusb_control_transfer(
        handle, kVendorReadRequestType, kFlashProgrammerProbeRequest, 0, 0,
        answer.data(), static_cast<uint16_t>(answer.size()),
        kRegisterReadTimeoutMilliseconds);
    libusb_close(handle);

    if (read != static_cast<int>(answer.size())) {
      return false;
    }
    return std::memcmp(answer.data(), kFlashProgrammerMagic,
                       std::strlen(kFlashProgrammerMagic)) == 0;
  }

  // Open the preferred device, or the first acceptable one attached if it is
  // not there.
  bool Open(const std::string& preferred_path, DeviceSelection selection,
            libusb_device_handle*& handle, DeviceInfo* chosen) {
    handle = nullptr;

    ScopedDeviceList list;
    const ssize_t count = list.Acquire(context_);
    if (count < 0) {
      return false;
    }

    std::vector<DeviceInfo> infos;
    std::vector<libusb_device*> matches;
    for (ssize_t index = 0; index < count; ++index) {
      libusb_device* const device = list.At(index);

      libusb_device_descriptor descriptor{};
      if (libusb_get_device_descriptor(device, &descriptor) != 0) {
        continue;
      }
      const std::optional<DevicePersonality> personality =
          PersonalityFromDescriptor(descriptor);
      if (!personality.has_value()) {
        continue;
      }

      DeviceInfo info;
      info.path = BuildDevicePath(device);
      info.speed = SpeedFromLibUsb(libusb_get_device_speed(device));
      info.personality = *personality;
      infos.push_back(std::move(info));
      matches.push_back(device);
    }

    const DeviceInfo* const selected =
        SelectDevice(infos, preferred_path, selection);
    if (selected == nullptr) {
      if (logger_ != nullptr) {
        logger_->Error("No Domesday Duplicator is attached");
      }
      return false;
    }

    const size_t position = static_cast<size_t>(selected - infos.data());
    const int opened = libusb_open(matches[position], &handle);
    if (opened != 0) {
      handle = nullptr;
      if (logger_ != nullptr) {
        logger_->Error(std::string("Opening the device failed: ") +
                       libusb_error_name(opened) +
                       ". On Linux this is usually a missing udev rule.");
      }
      return false;
    }

    if (chosen != nullptr) {
      *chosen = *selected;
    }
    return true;
  }

  bool FindBulkInEndpoint(libusb_device_handle* handle, uint8_t& endpoint,
                          size_t& max_packet_bytes) {
    libusb_config_descriptor* configuration = nullptr;
    const int got = libusb_get_active_config_descriptor(
        libusb_get_device(handle), &configuration);
    if (got != 0) {
      if (logger_ != nullptr) {
        logger_->Error(
            std::string("Reading the device's configuration failed: ") +
            libusb_error_name(got));
      }
      return false;
    }

    bool found = false;
    for (uint8_t interface_index = 0;
         interface_index < configuration->bNumInterfaces && !found;
         ++interface_index) {
      const libusb_interface& interface =
          configuration->interface[interface_index];
      if (interface.num_altsetting <= 0) {
        continue;
      }

      const libusb_interface_descriptor& setting = interface.altsetting[0];
      for (uint8_t endpoint_index = 0; endpoint_index < setting.bNumEndpoints;
           ++endpoint_index) {
        const libusb_endpoint_descriptor& descriptor =
            setting.endpoint[endpoint_index];
        const bool is_bulk = (descriptor.bmAttributes & 0x03) ==
                             LIBUSB_ENDPOINT_TRANSFER_TYPE_BULK;
        const bool is_in =
            (descriptor.bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_IN;
        if (is_bulk && is_in) {
          endpoint = descriptor.bEndpointAddress;
          max_packet_bytes = descriptor.wMaxPacketSize;
          found = true;
          break;
        }
      }
    }

    libusb_free_config_descriptor(configuration);

    if (!found && logger_ != nullptr) {
      logger_->Error("The device has no bulk input endpoint");
    }
    return found;
  }

  ILogger* logger_ = nullptr;
  libusb_context* context_ = nullptr;
};

}  // namespace

std::unique_ptr<IUsbDevice> MakeLibUsbDevice(ILogger* logger) {
  auto device = std::make_unique<LibUsbDevice>(logger);
  if (!device->Initialise()) {
    return nullptr;
  }
  return device;
}

}  // namespace ddd::capture
