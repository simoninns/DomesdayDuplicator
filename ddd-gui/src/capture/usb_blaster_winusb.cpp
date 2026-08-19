/************************************************************************

    usb_blaster_winusb.cpp

    Finding the DE0-Nano's on-board USB-Blaster and moving bytes to it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <windows.h>
// windows.h first: everything below depends on its types.
#include <cfgmgr32.h>
#include <initguid.h>
#include <usbiodef.h>
#include <usbspec.h>
#include <winusb.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "jtag_cable.h"
#include "logger.h"
#include "usb_blaster_cable.h"
#include "usb_presence.h"

// The Windows twin of usb_blaster_libusb.cpp, and it is only the pipe: what
// the bytes on it mean is usb_blaster_cable.cpp, which knows about neither
// backend and is tested against a fake.
//
// The FT245's own interface is the same on both platforms — the same vendor
// requests, the same bulk endpoints, the same two status bytes at the front
// of every IN packet — so the two files differ in the API they call and in
// one thing they say. Windows binds drivers by USB identifier, so a cable
// that is plainly plugged in can be one this application may not open, and
// naming that is most of the value of this file. Everything else is
// WinUsb_ReadPipe and WinUsb_WritePipe.
namespace ddd::capture {
namespace {

// Long enough that a cable which is merely busy is not declared dead, short
// enough that a cable which has stopped answering does not hang a wizard.
constexpr ULONG kTransferTimeoutMilliseconds = 5000;

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

// The control pipe, which WinUSB addresses as pipe zero rather than by an
// endpoint address.
constexpr UCHAR kControlPipeId = 0;

// What the cable's device-interface paths would be found under, if any are.
//
// Every USB device Windows has enumerated appears under this class whatever
// driver it is bound to, because the hub driver registers it — which is the
// property this file leans on twice. Once to find the cable's path to open,
// and once to be able to say that a cable which will not open is nonetheless
// there.
bool ListUsbInterfacePaths(std::vector<std::wstring>& paths) {
  paths.clear();

  // Retried rather than sized once: a device can appear between asking how
  // much room the list needs and asking for the list itself.
  for (int attempt = 0; attempt < 4; ++attempt) {
    ULONG required = 0;
    if (CM_Get_Device_Interface_List_SizeW(
            &required, const_cast<LPGUID>(&GUID_DEVINTERFACE_USB_DEVICE),
            nullptr, CM_GET_DEVICE_INTERFACE_LIST_PRESENT) != CR_SUCCESS) {
      return false;
    }

    std::vector<wchar_t> buffer(required + 1, L'\0');
    const CONFIGRET listed = CM_Get_Device_Interface_ListW(
        const_cast<LPGUID>(&GUID_DEVINTERFACE_USB_DEVICE), nullptr,
        buffer.data(), static_cast<ULONG>(buffer.size()),
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (listed == CR_BUFFER_SMALL) {
      continue;
    }
    if (listed != CR_SUCCESS) {
      return false;
    }

    // A double-null-terminated run of strings.
    size_t index = 0;
    while (buffer[index] != L'\0') {
      std::wstring path = &buffer[index];
      index += path.size() + 1;
      paths.push_back(std::move(path));
    }
    return true;
  }

  return false;
}

// Those of them that spell out these identifiers.
//
// An interface path carries them as text — \\?\USB#VID_09FB&PID_6001#… — and
// the case varies between Windows versions, so both sides are lowered before
// they are compared. This is the same match usb_presence_winusb.cpp makes;
// what differs is that this one keeps the path, because a path is what
// CreateFileW opens.
//
// False means the bus could not be read, which is not the same answer as an
// empty list and is kept apart from it for the reason usb_presence.h gives at
// length: "not attached" is a claim, and this is the state in which no claim
// can be made.
bool PathsFor(uint16_t vendor_id, uint16_t product_id,
              std::vector<std::wstring>& matched) {
  matched.clear();

  std::vector<std::wstring> paths;
  if (!ListUsbInterfacePaths(paths)) {
    return false;
  }

  wchar_t wanted[32] = {};
  std::swprintf(wanted, sizeof(wanted) / sizeof(wanted[0]),
                L"vid_%04x&pid_%04x", vendor_id, product_id);

  for (const std::wstring& path : paths) {
    std::wstring lowered = path;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](wchar_t character) {
                     return static_cast<wchar_t>(std::towlower(character));
                   });
    if (lowered.find(wanted) != std::wstring::npos) {
      matched.push_back(path);
    }
  }
  return true;
}

// The byte pipe, and the owner of everything that has to be given back.
class WinUsbFtdiTransport : public IFtdiTransport {
 public:
  WinUsbFtdiTransport(ILogger* logger, std::string* problem)
      : logger_(logger), problem_(problem) {}

  ~WinUsbFtdiTransport() override { Close(); }

  bool Open() {
    std::vector<std::wstring> paths;
    if (!PathsFor(kAlteraVendorId, kUsbBlasterProductId, paths)) {
      Fail(
          "Windows would not list the USB devices attached, so whether a "
          "USB-Blaster is there could not be established.");
      return false;
    }
    if (paths.empty()) {
      ReportNoCable();
      return false;
    }

    // A machine can have more than one, and one of them being held by
    // something else is not a reason to ignore the rest. The first that
    // opens and looks like an FT245 is the one used.
    bool opened_any = false;
    for (const std::wstring& path : paths) {
      if (!OpenPath(path)) {
        continue;
      }
      opened_any = true;
      if (FindBulkPipes() && Configure()) {
        // A cable earlier in the list that would not work has left a sentence
        // behind, and this one working makes it the answer to nothing. The
        // log keeps it; the caller's screen should not.
        if (problem_ != nullptr) {
          problem_->clear();
        }
        return true;
      }
      Close();
    }

    if (!opened_any) {
      ReportUnopenable();
    }
    return false;
  }

  bool Write(std::span<const uint8_t> bytes) override {
    if (interface_ == nullptr || bytes.empty()) {
      return interface_ != nullptr;
    }

    ULONG transferred = 0;
    if (WinUsb_WritePipe(
            interface_, write_pipe_, const_cast<PUCHAR>(bytes.data()),
            static_cast<ULONG>(bytes.size()), &transferred, nullptr) != TRUE) {
      Fail("Sending to the USB-Blaster failed with error " +
           std::to_string(GetLastError()) + ".");
      return false;
    }
    if (transferred != bytes.size()) {
      Fail("Sending to the USB-Blaster stopped part way through.");
      return false;
    }
    return true;
  }

  bool Read(std::span<uint8_t> buffer, size_t& received) override {
    received = 0;
    if (interface_ == nullptr) {
      return false;
    }

    ULONG transferred = 0;
    if (WinUsb_ReadPipe(interface_, read_pipe_, buffer.data(),
                        static_cast<ULONG>(buffer.size()), &transferred,
                        nullptr) != TRUE) {
      Fail("Reading from the USB-Blaster failed with error " +
           std::to_string(GetLastError()) + ".");
      return false;
    }
    received = static_cast<size_t>(transferred);
    return true;
  }

  size_t packet_bytes() const override { return packet_bytes_; }

 private:
  bool OpenPath(const std::wstring& path) {
    // Overlapped, as the capture backend opens its device: nothing here
    // waits on an OVERLAPPED — WinUSB runs a transfer synchronously when it
    // is handed none — but the flag costs nothing and keeps the two file
    // handles in this application the same shape.
    device_ =
        CreateFileW(path.c_str(), GENERIC_WRITE | GENERIC_READ,
                    FILE_SHARE_WRITE | FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    if (device_ == INVALID_HANDLE_VALUE) {
      // Kept now rather than read later: CloseHandle and the next attempt
      // both set it, so by the time anything reports a failure the number
      // belongs to whatever happened after it.
      open_error_ = GetLastError();
      device_ = nullptr;
      return false;
    }

    if (WinUsb_Initialize(device_, &interface_) != TRUE) {
      open_error_ = GetLastError();
      CloseHandle(device_);
      device_ = nullptr;
      interface_ = nullptr;
      return false;
    }
    return true;
  }

  void Close() {
    if (interface_ != nullptr) {
      WinUsb_Free(interface_);
      interface_ = nullptr;
    }
    if (device_ != nullptr) {
      CloseHandle(device_);
      device_ = nullptr;
    }
    read_pipe_ = 0;
    write_pipe_ = 0;
    packet_bytes_ = 0;
  }

  bool FindBulkPipes() {
    USB_INTERFACE_DESCRIPTOR descriptor = {};
    if (WinUsb_QueryInterfaceSettings(interface_, 0, &descriptor) != TRUE) {
      Fail("Reading the cable's interface failed with error " +
           std::to_string(GetLastError()) + ".");
      return false;
    }

    bool found_in = false;
    bool found_out = false;
    for (UCHAR index = 0; index < descriptor.bNumEndpoints; ++index) {
      WINUSB_PIPE_INFORMATION pipe = {};
      if (WinUsb_QueryPipe(interface_, 0, index, &pipe) != TRUE) {
        continue;
      }
      if (pipe.PipeType != UsbdPipeTypeBulk) {
        continue;
      }
      if (USB_ENDPOINT_DIRECTION_IN(pipe.PipeId)) {
        read_pipe_ = pipe.PipeId;
        packet_bytes_ = pipe.MaximumPacketSize;
        found_in = true;
      } else {
        write_pipe_ = pipe.PipeId;
        found_out = true;
      }
    }

    if (!found_in || !found_out || packet_bytes_ <= kFtdiStatusBytes) {
      Fail(
          "This cable answers to the USB-Blaster's identifiers but does not "
          "have the endpoints one has.");
      return false;
    }

    // WinUSB's transfer timeout is a policy on the pipe rather than an
    // argument to the transfer, so it is set once here and applies to every
    // read and write afterwards. Without it a cable that stops answering
    // waits forever, on a worker thread a wizard is waiting for.
    return SetTimeout(read_pipe_) && SetTimeout(write_pipe_) &&
           SetTimeout(kControlPipeId);
  }

  bool SetTimeout(UCHAR pipe_id) {
    ULONG timeout = kTransferTimeoutMilliseconds;
    if (WinUsb_SetPipePolicy(interface_, pipe_id, PIPE_TRANSFER_TIMEOUT,
                             sizeof(timeout), &timeout) != TRUE) {
      Fail("Setting the USB-Blaster's transfer timeout failed with error " +
           std::to_string(GetLastError()) + ".");
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
    WINUSB_SETUP_PACKET setup = {};
    setup.RequestType = kFtdiRequestType;
    setup.Request = request;
    setup.Value = value;
    setup.Index = kFtdiPortIndex;
    setup.Length = 0;

    if (WinUsb_ControlTransfer(interface_, setup, nullptr, 0, nullptr,
                               nullptr) != TRUE) {
      Fail("Setting the USB-Blaster up failed with error " +
           std::to_string(GetLastError()) + ".");
      return false;
    }
    return true;
  }

  // Nothing at 09fb:6001. Whether another cable of the family is attached
  // decides which of two quite different things has happened, and saying
  // "that is a USB-Blaster II" is worth much more than the truthful but
  // useless "no cable found".
  //
  // Asked through UsbDeviceAttached rather than by opening anything, so that
  // a cable of the family sitting on Altera's own driver is still recognised
  // and named — which on Windows is the likely state of one.
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
      if (UsbDeviceAttached(kAlteraVendorId, other.product) !=
          UsbPresence::kPresent) {
        continue;
      }
      Fail(std::string("The cable attached is ") + other.description +
           ", which this application does not drive. The DE0-Nano's on-board "
           "cable is a USB-Blaster (09fb:6001).");
      return;
    }

    Fail(
        "No USB-Blaster is attached. Connect the DE0-Nano's mini-USB "
        "connector — and note that a charge-only cable lights the board and "
        "enumerates nothing.");
  }

  // The cable is there and would not open, which on Windows is one thing far
  // more often than it is anything else: WinUSB is not what is bound to it.
  //
  // Worth its own sentence rather than an error number, because the number
  // is the same whether Altera's driver has the cable, Quartus's jtagd is
  // running, or nothing at all is bound — and the remedy is the same Zadig
  // step in the first and third of those.
  void ReportUnopenable() {
    Fail(
        "A USB-Blaster is attached but this application cannot open it: "
        "Windows has something other than WinUSB bound to 09FB:6001. Bind it "
        "with Zadig (Options > List All Devices, pick 09FB:6001, choose "
        "WinUSB, Replace Driver). If Quartus is installed, its own Altera "
        "USB-Blaster driver is what holds the cable, and replacing it takes "
        "the cable away from Quartus until you put it back in Device "
        "Manager. Windows error " +
        std::to_string(open_error_) + ".");
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
  DWORD open_error_ = 0;
  HANDLE device_ = nullptr;
  WINUSB_INTERFACE_HANDLE interface_ = nullptr;
  UCHAR read_pipe_ = 0;
  UCHAR write_pipe_ = 0;
  size_t packet_bytes_ = 0;
};

// A cable that owns the pipe it rides on, so a caller gets one object with
// one lifetime rather than two it has to keep in step.
class OwningCable : public IJtagCable {
 public:
  OwningCable(std::unique_ptr<WinUsbFtdiTransport> transport,
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
  std::unique_ptr<WinUsbFtdiTransport> transport_;
  std::unique_ptr<IJtagCable> cable_;
};

}  // namespace

std::unique_ptr<IJtagCable> MakeUsbBlasterCable(ILogger* logger,
                                                std::string* problem) {
  if (problem != nullptr) {
    problem->clear();
  }

  auto transport = std::make_unique<WinUsbFtdiTransport>(logger, problem);
  if (!transport->Open()) {
    return nullptr;
  }

  std::unique_ptr<IJtagCable> cable =
      MakeUsbBlasterCableOver(*transport, logger);
  return std::make_unique<OwningCable>(std::move(transport), std::move(cable));
}

}  // namespace ddd::capture
