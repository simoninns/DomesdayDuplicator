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
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "libusb_source.h"
#include "logger.h"
#include "sysfs_device_list.h"
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

// Which of the four identities this descriptor is, if it is one of them.
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
  if (descriptor.idVendor == kLegacyVendorId &&
      descriptor.idProduct == kLegacyProductId) {
    return DevicePersonality::kLegacy;
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
  // The lease is kept for the same reason LibUsbSource keeps one: this handle
  // belongs to a context the backend may otherwise recycle, and an update
  // holds this channel open for minutes while the device monitor carries on
  // enumerating.
  LibUsbControlChannel(libusb_device_handle* handle, bool claimed,
                       std::shared_ptr<const void> lease, ILogger* logger)
      : handle_(handle),
        claimed_(claimed),
        lease_(std::move(lease)),
        logger_(logger) {}

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
  std::shared_ptr<const void> lease_;
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

  // A context, and a token saying it is being used.
  //
  // Everything that touches libusb goes through one of these. The token is
  // opaque and is never read; its existence is the message. See
  // RecycleContext.
  struct Lease {
    libusb_context* context = nullptr;
    std::shared_ptr<const void> token;
  };

  Lease Borrow() {
    const std::lock_guard<std::mutex> guard(mutex_);
    return Lease{context_, lease_};
  }

  bool Initialise() {
    context_ = StartContext();
    if (context_ == nullptr) {
      return false;
    }
    lease_ = std::make_shared<const char>('\0');
    return true;
  }

  const char* Name() const override { return "libusb"; }

  // Called from more than one thread: the device monitor polls here five times
  // a second, and the update path enumerates from its own worker while waiting
  // for a device to come back. Two of them deciding to recycle at once is
  // harmless — the second finds the first's lease outstanding, or restarts a
  // context that was already fresh — so nothing here is serialised beyond what
  // the lease itself needs.
  bool Enumerate(std::vector<DeviceInfo>& devices) override {
    Lease lease = Borrow();

    std::vector<UsbIdentity> seen;
    if (!EnumerateFrom(lease.context, devices, seen)) {
      return false;
    }

    // The second opinion, and what to do when it differs. Nothing at all on a
    // platform that cannot give one, which is how macOS gets the behaviour it
    // has always had without a conditional saying so.
    const std::optional<std::vector<UsbIdentity>> kernel =
        ReadSysfsDeviceIdentities();
    if (!kernel.has_value() || !DeviceViewsDisagree(*kernel, seen)) {
      rescan_is_futile_.store(false);
      return true;
    }
    if (rescan_is_futile_.load()) {
      return true;
    }

    // Our own borrow is the one thing certain to be in the way, so it goes
    // first. Anything else still holding one — a capture, an update's control
    // channel — means the list stands as it is until that has finished, which
    // is right: a device list a fifth of a second out of date is not worth
    // closing a stream over.
    lease.token.reset();
    if (!RecycleContext()) {
      return true;
    }

    lease = Borrow();
    seen.clear();
    if (!EnumerateFrom(lease.context, devices, seen)) {
      return false;
    }

    // A disagreement that survives the rescan is not a stale cache, and
    // rescanning five times a second for ever would be a poor way to find that
    // out. Said once and then left alone until the two agree again.
    if (DeviceViewsDisagree(*kernel, seen)) {
      rescan_is_futile_.store(true);
      if (logger_ != nullptr) {
        logger_->Debug(
            "The kernel and libusb still disagree about what is attached "
            "after a rescan; leaving libusb's list alone");
      }
    }
    return true;
  }

  bool EnumerateFrom(libusb_context* context, std::vector<DeviceInfo>& devices,
                     std::vector<UsbIdentity>& identities) {
    devices.clear();

    ScopedDeviceList list;
    const ssize_t count = list.Acquire(context);
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

      // Recorded here rather than below, which is the whole point: this is
      // what libusb *saw*, and the comparison against the kernel has to be
      // made on that rather than on what survives the probe. See
      // DeviceViewsDisagree.
      identities.push_back(
          UsbIdentity{descriptor.idVendor, descriptor.idProduct});

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
      info.personality = *personality;

      // Every personality but the legacy one has its product string read.
      //
      // Reading it means opening the device, and 1d50:603b is deliberately
      // outside the udev rules this project ships, so the open fails on an
      // ordinary desktop — with nothing behind it, because the legacy
      // firmware reports no version and nothing displays the string of a
      // device that is not running the current firmware. Recognising a
      // legacy board is the whole of what is wanted, and recognition needs
      // no open at all.
      if (*personality != DevicePersonality::kLegacy) {
        info.product_string = ReadProductString(device, descriptor.iProduct);
      }

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
    const Lease lease = Borrow();

    libusb_device_handle* handle = nullptr;
    if (!Open(lease.context, path, DeviceSelection::kCaptureCapable, handle,
              nullptr)) {
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

  bool SetCollecting(const std::string& path, bool collecting) override {
    const Lease lease = Borrow();

    libusb_device_handle* handle = nullptr;
    if (!Open(lease.context, path, DeviceSelection::kCaptureCapable, handle,
              nullptr)) {
      return false;
    }

    bool claimed = libusb_claim_interface(handle, kInterfaceNumber) == 0;
    const int sent = libusb_control_transfer(
        handle, kVendorRequestType, kCollectionRequest,
        collecting ? kCollectionStart : kCollectionStop, 0, nullptr, 0,
        kControlTimeoutMilliseconds);

    if (claimed) {
      libusb_release_interface(handle, kInterfaceNumber);
    }
    libusb_close(handle);

    if (sent < 0) {
      if (logger_ != nullptr) {
        logger_->Error(std::string("Telling the device a capture was ") +
                       (collecting ? "starting" : "stopping") + " failed: " +
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

    const Lease lease = Borrow();

    libusb_device_handle* handle = nullptr;
    if (!Open(lease.context, path, DeviceSelection::kCaptureCapable, handle,
              nullptr)) {
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

    Lease lease = Borrow();

    libusb_device_handle* handle = nullptr;
    DeviceInfo info;
    if (!Open(lease.context, path, DeviceSelection::kCaptureCapable, handle,
              &info)) {
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
    return std::make_unique<LibUsbSource>(lease.context, std::move(lease.token),
                                          handle, endpoint, max_packet_bytes,
                                          options, logger_);
  }

  std::unique_ptr<IUsbControlChannel> OpenControlChannel(
      const std::string& path) override {
    Lease lease = Borrow();

    libusb_device_handle* handle = nullptr;
    // Any personality: this is the channel the update and recovery paths
    // use, and a device in recovery is precisely the one they open.
    if (!Open(lease.context, path, DeviceSelection::kAny, handle, nullptr)) {
      return nullptr;
    }

    // The interface is claimed if it can be, and the channel works either
    // way. Control transfers on endpoint zero do not require a claim, and
    // insisting on one would refuse to update a device that some other
    // process happened to have open — which is exactly the state a device
    // gets into when a previous attempt left something behind.
    const bool claimed = libusb_claim_interface(handle, kInterfaceNumber) == 0;

    return std::make_unique<LibUsbControlChannel>(
        handle, claimed, std::move(lease.token), logger_);
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
  bool Open(libusb_context* context, const std::string& preferred_path,
            DeviceSelection selection, libusb_device_handle*& handle,
            DeviceInfo* chosen) {
    handle = nullptr;

    ScopedDeviceList list;
    const ssize_t count = list.Acquire(context);
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

  // Start a libusb context, or report why not.
  libusb_context* StartContext() {
    libusb_context* context = nullptr;
#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x0100010A
    const int started = libusb_init_context(&context, nullptr, 0);
#else
    const int started = libusb_init(&context);
#endif
    if (started != 0) {
      if (logger_ != nullptr) {
        logger_->Error(std::string("libusb could not be started: ") +
                       libusb_error_name(started));
      }
      return nullptr;
    }
    return context;
  }

  // Take the context out of use and start a fresh one in its place.
  //
  // The only way to make libusb re-read the bus. There is no rescan call: the
  // Linux backend's device list is seeded by one sysfs scan inside
  // libusb_init and maintained after that by hot-plug events alone, so where
  // those events do not arrive a new context is the only thing that produces
  // an up-to-date list. See sysfs_device_list.h.
  //
  // Refused while anything else holds a lease, and that check is the whole
  // safety argument. Every handle this backend hands out belongs to the
  // context it was opened on, so recycling underneath one would close a
  // capture's endpoint or an update's control channel from another thread.
  // Leases are only ever copied by Borrow(), which takes the same lock as
  // this, so a use count of one under the lock means no other copy exists and
  // none can appear before the swap is done.
  bool RecycleContext() {
    const std::lock_guard<std::mutex> guard(mutex_);

    if (lease_.use_count() != 1) {
      return false;
    }

    // The replacement first. A failed start then costs nothing: the old
    // context is still there and still works, which is a stale device list
    // rather than no device list at all.
    libusb_context* const replacement = StartContext();
    if (replacement == nullptr) {
      return false;
    }

    libusb_exit(context_);
    context_ = replacement;
    lease_ = std::make_shared<const char>('\0');

    if (logger_ != nullptr) {
      logger_->Info(
          "The kernel listed a device libusb had not been told about; the USB "
          "context was restarted to pick it up");
    }
    return true;
  }

  ILogger* logger_ = nullptr;

  // The context, the token that says it is in use, and the lock that keeps
  // the two in step. Held while borrowing and while recycling, and never
  // across a transfer: a control request to a device that has stopped
  // answering must not be able to stall the device monitor.
  mutable std::mutex mutex_;
  libusb_context* context_ = nullptr;
  std::shared_ptr<const void> lease_;

  // Set when a rescan did not resolve the disagreement, so that a permanent
  // one costs a restarted context once rather than five times a second.
  //
  // Atomic because Enumerate has more than one caller and more than one
  // thread, and this is the one piece of state it keeps between calls. A race
  // on it would cost an extra rescan or delay one, which is why it is a plain
  // flag rather than something taking the lock.
  std::atomic<bool> rescan_is_futile_{false};
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
