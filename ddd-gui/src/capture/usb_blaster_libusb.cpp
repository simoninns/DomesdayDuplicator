/************************************************************************

    usb_blaster_libusb.cpp

    Finding the DE0-Nano's on-board USB-Blaster and moving bytes to it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <libusb.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>

#include "jtag_cable.h"
#include "logger.h"
#include "usb_blaster_cable.h"

// Everything here is about *reaching* the cable: opening it, setting the
// FT245 up, and moving bytes both ways. What those bytes mean is
// usb_blaster_cable.cpp, which knows nothing about libusb and is tested
// against a fake pipe.
namespace ddd::capture {
namespace {

// Long enough that a cable which is merely busy is not declared dead, short
// enough that a cable which has stopped answering does not hang a wizard.
constexpr unsigned int kTransferTimeoutMilliseconds = 5000;

// FTDI control requests, from the chip's published interface. Vendor
// requests, host to device.
constexpr uint8_t kFtdiRequestType = 0x40;
constexpr uint8_t kFtdiResetRequest = 0x00;
constexpr uint8_t kFtdiSetLatencyRequest = 0x09;

// Reset request values: reset the chip, then discard whatever each direction
// was holding. A cable left mid-transfer by an interrupted run has bytes in
// it, and they would otherwise be read as the first answer of the next run.
constexpr uint16_t kFtdiResetAll = 0;
constexpr uint16_t kFtdiPurgeReadBuffer = 1;
constexpr uint16_t kFtdiPurgeWriteBuffer = 2;

// The FT245 has one port, and its requests address it as index 1.
constexpr uint16_t kFtdiPortIndex = 1;

// How long the chip waits for more data before sending a short packet back.
// The default is 16 ms, which would be added to every request for TDO in a
// programming run.
constexpr uint16_t kFtdiLatencyMilliseconds = 2;

// The byte pipe, and the owner of everything that has to be given back.
class LibUsbFtdiTransport : public IFtdiTransport {
 public:
  LibUsbFtdiTransport(ILogger* logger, std::string* problem)
      : logger_(logger), problem_(problem) {}

  ~LibUsbFtdiTransport() override {
    if (handle_ != nullptr) {
      libusb_release_interface(handle_, interface_number_);
      libusb_close(handle_);
    }
    if (context_ != nullptr) {
      libusb_exit(context_);
    }
  }

  bool Open() {
#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x0100010A
    const int started = libusb_init_context(&context_, nullptr, 0);
#else
    const int started = libusb_init(&context_);
#endif
    if (started != 0) {
      context_ = nullptr;
      Fail(std::string("libusb could not be started: ") +
           libusb_error_name(started));
      return false;
    }

    handle_ = libusb_open_device_with_vid_pid(context_, kAlteraVendorId,
                                              kUsbBlasterProductId);
    if (handle_ == nullptr) {
      ReportNoCable();
      return false;
    }

    // Nothing in a stock Linux binds this device — the FTDI serial driver
    // matches FTDI's own identifiers and this cable wears Altera's — but a
    // machine with Altera's tools installed is a different matter, and
    // asking libusb to take the interface back costs one call.
    libusb_set_auto_detach_kernel_driver(handle_, 1);

    if (!FindBulkEndpoints()) {
      return false;
    }

    const int claimed = libusb_claim_interface(handle_, interface_number_);
    if (claimed != 0) {
      Fail(std::string("The USB-Blaster could not be claimed: ") +
           libusb_error_name(claimed) +
           ". Quartus's own jtagd holds the cable open whenever it is "
           "running; stop it and try again.");
      libusb_close(handle_);
      handle_ = nullptr;
      return false;
    }

    return Configure();
  }

  bool Write(std::span<const uint8_t> bytes) override {
    if (handle_ == nullptr || bytes.empty()) {
      return handle_ != nullptr;
    }

    int transferred = 0;
    const int sent = libusb_bulk_transfer(
        handle_, write_endpoint_, const_cast<uint8_t*>(bytes.data()),
        static_cast<int>(bytes.size()), &transferred,
        kTransferTimeoutMilliseconds);
    if (sent != 0 || static_cast<size_t>(transferred) != bytes.size()) {
      Fail(std::string("Sending to the USB-Blaster failed: ") +
           libusb_error_name(sent));
      return false;
    }
    return true;
  }

  bool Read(std::span<uint8_t> buffer, size_t& received) override {
    received = 0;
    if (handle_ == nullptr) {
      return false;
    }

    int transferred = 0;
    const int got = libusb_bulk_transfer(
        handle_, read_endpoint_, buffer.data(), static_cast<int>(buffer.size()),
        &transferred, kTransferTimeoutMilliseconds);
    if (got != 0) {
      Fail(std::string("Reading from the USB-Blaster failed: ") +
           libusb_error_name(got));
      return false;
    }
    received = static_cast<size_t>(transferred);
    return true;
  }

  size_t packet_bytes() const override { return packet_bytes_; }

 private:
  bool FindBulkEndpoints() {
    libusb_config_descriptor* configuration = nullptr;
    const int got = libusb_get_active_config_descriptor(
        libusb_get_device(handle_), &configuration);
    if (got != 0) {
      Fail(std::string("Reading the cable's configuration failed: ") +
           libusb_error_name(got));
      libusb_close(handle_);
      handle_ = nullptr;
      return false;
    }

    bool found_in = false;
    bool found_out = false;
    for (uint8_t index = 0; index < configuration->bNumInterfaces; ++index) {
      const libusb_interface& interface = configuration->interface[index];
      if (interface.num_altsetting <= 0) {
        continue;
      }
      const libusb_interface_descriptor& setting = interface.altsetting[0];
      for (uint8_t endpoint_index = 0; endpoint_index < setting.bNumEndpoints;
           ++endpoint_index) {
        const libusb_endpoint_descriptor& endpoint =
            setting.endpoint[endpoint_index];
        if ((endpoint.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) !=
            LIBUSB_TRANSFER_TYPE_BULK) {
          continue;
        }
        if ((endpoint.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ==
            LIBUSB_ENDPOINT_IN) {
          read_endpoint_ = endpoint.bEndpointAddress;
          packet_bytes_ = endpoint.wMaxPacketSize;
          found_in = true;
        } else {
          write_endpoint_ = endpoint.bEndpointAddress;
          found_out = true;
        }
      }
      if (found_in && found_out) {
        interface_number_ = setting.bInterfaceNumber;
        break;
      }
    }

    libusb_free_config_descriptor(configuration);

    if (!found_in || !found_out || packet_bytes_ <= kFtdiStatusBytes) {
      Fail(
          "This cable answers to the USB-Blaster's identifiers but does not "
          "have the endpoints one has.");
      libusb_close(handle_);
      handle_ = nullptr;
      return false;
    }
    return true;
  }

  // Put the FT245 into a known state: reset, both buffers emptied, and a
  // latency short enough that a request for TDO is answered promptly.
  bool Configure() {
    const std::array<uint16_t, 3> resets{kFtdiResetAll, kFtdiPurgeReadBuffer,
                                         kFtdiPurgeWriteBuffer};
    for (uint16_t value : resets) {
      if (!Control(kFtdiResetRequest, value)) {
        return false;
      }
    }
    return Control(kFtdiSetLatencyRequest, kFtdiLatencyMilliseconds);
  }

  bool Control(uint8_t request, uint16_t value) {
    const int sent = libusb_control_transfer(handle_, kFtdiRequestType, request,
                                             value, kFtdiPortIndex, nullptr, 0,
                                             kTransferTimeoutMilliseconds);
    if (sent < 0) {
      Fail(std::string("Setting the USB-Blaster up failed: ") +
           libusb_error_name(sent));
      return false;
    }
    return true;
  }

  // Nothing at 09fb:6001. Whether another cable of the family is attached
  // decides which of two quite different things has happened, and saying
  // "that is a USB-Blaster II" is worth much more than the truthful but
  // useless "no cable found".
  void ReportNoCable() {
    struct KnownCable {
      uint16_t product;
      const char* description;
    };
    static constexpr std::array<KnownCable, 4> kOthers{
        {{kUsbBlasterRevisionBProductId, "a later USB-Blaster revision"},
         {kUsbBlasterRevisionCProductId, "a later USB-Blaster revision"},
         {kUsbBlasterTwoProductId, "a USB-Blaster II"},
         {kUsbBlasterTwoAlternateProductId, "a USB-Blaster II"}}};

    for (const KnownCable& other : kOthers) {
      libusb_device_handle* found = libusb_open_device_with_vid_pid(
          context_, kAlteraVendorId, other.product);
      if (found == nullptr) {
        continue;
      }
      libusb_close(found);
      Fail(std::string("The cable attached is ") + other.description +
           ", which this application does not drive. The DE0-Nano's on-board "
           "cable is a USB-Blaster (09fb:6001).");
      return;
    }

    Fail(
        "No USB-Blaster is attached. Connect the DE0-Nano's mini-USB "
        "connector, and on Linux check that the udev rules are installed.");
  }

  // One sentence, to the log and — for a caller that has to put it on a
  // screen — to whatever it handed in. The first is kept rather than the last:
  // the reason a cable could not be opened is decided at the point it failed,
  // and everything after that is consequence.
  void Fail(const std::string& message) {
    if (logger_ != nullptr) {
      logger_->Error(message);
    }
    if (problem_ != nullptr && problem_->empty()) {
      *problem_ = message;
    }
  }

  ILogger* logger_ = nullptr;
  std::string* problem_ = nullptr;
  libusb_context* context_ = nullptr;
  libusb_device_handle* handle_ = nullptr;
  int interface_number_ = 0;
  uint8_t read_endpoint_ = 0;
  uint8_t write_endpoint_ = 0;
  size_t packet_bytes_ = 0;
};

// A cable that owns the pipe it rides on, so a caller gets one object with
// one lifetime rather than two it has to keep in step.
class OwningCable : public IJtagCable {
 public:
  OwningCable(std::unique_ptr<LibUsbFtdiTransport> transport,
              std::unique_ptr<IJtagCable> cable)
      : transport_(std::move(transport)), cable_(std::move(cable)) {}

  bool Shift(std::span<const uint8_t> tms, std::span<const uint8_t> tdi,
             size_t bit_count, std::vector<uint8_t>* tdo) override {
    return cable_->Shift(tms, tdi, bit_count, tdo);
  }
  bool RunClock(size_t count) override { return cable_->RunClock(count); }
  bool Flush() override { return cable_->Flush(); }
  const char* Name() const override { return cable_->Name(); }

 private:
  std::unique_ptr<LibUsbFtdiTransport> transport_;
  std::unique_ptr<IJtagCable> cable_;
};

}  // namespace

std::unique_ptr<IJtagCable> MakeUsbBlasterCable(ILogger* logger,
                                                std::string* problem) {
  if (problem != nullptr) {
    problem->clear();
  }

  auto transport = std::make_unique<LibUsbFtdiTransport>(logger, problem);
  if (!transport->Open()) {
    return nullptr;
  }

  std::unique_ptr<IJtagCable> cable =
      MakeUsbBlasterCableOver(*transport, logger);
  return std::make_unique<OwningCable>(std::move(transport), std::move(cable));
}

}  // namespace ddd::capture
