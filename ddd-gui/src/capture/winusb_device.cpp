/************************************************************************

    winusb_device.cpp

    Finding and configuring the device with WinUSB (Windows)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2024 Roger Sanders
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "winusb_device.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <usbspec.h>
#include <winusb.h>

#include <array>
#include <string>
#include <vector>

#include "logger.h"
#include "winusb_source.h"
#include "wire_protocol.h"

// The USB device interface class. Declared here rather than linked from a
// library because the import library that carries it differs between toolchains
// and the MSYS2 build does not have it at all.
DEFINE_GUID(GUID_DEVINTERFACE_DDD_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, 0x90,
            0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);

namespace ddd::capture {
namespace {

constexpr uint8_t kVendorRequestType = 0x40;

std::string ToUtf8(const std::wstring& text) {
  if (text.empty()) {
    return {};
  }
  const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()), nullptr,
                                         0, nullptr, nullptr);
  if (length <= 0) {
    return {};
  }
  std::string out(static_cast<size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                      out.data(), length, nullptr, nullptr);
  return out;
}

std::wstring ToWide(const std::string& text) {
  if (text.empty()) {
    return {};
  }
  const int length = MultiByteToWideChar(
      CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) {
    return {};
  }
  std::wstring out(static_cast<size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                      out.data(), length);
  return out;
}

// WinUSB's DEVICE_SPEED query answers with the driver's own three-value
// enumeration, which stops at HighSpeed: there is no SuperSpeed constant. A
// SuperSpeed device therefore reports HighSpeed here, and the query alone
// cannot tell a USB 3 link from a USB 2 one.
//
// The pipe's maximum packet size can. A bulk endpoint is 512 bytes at
// High-speed and 1024 at SuperSpeed, fixed by the specification, so the
// endpoint descriptor of a device already known to be USB 3 capable says which
// kind of port it is plugged into. That is the check the old backend was
// missing, and this is the only way WinUSB offers to make it.
constexpr size_t kSuperSpeedBulkPacketBytes = 1024;

DeviceSpeed SpeedFromPacketSize(size_t max_packet_bytes) {
  if (max_packet_bytes >= kSuperSpeedBulkPacketBytes) {
    return DeviceSpeed::kSuper;
  }
  if (max_packet_bytes > 0) {
    return DeviceSpeed::kHigh;
  }
  return DeviceSpeed::kUnknown;
}

// An open WinUSB handle pair that closes itself.
class ScopedWinUsbHandles {
 public:
  ScopedWinUsbHandles() = default;
  ~ScopedWinUsbHandles() { Close(); }

  ScopedWinUsbHandles(const ScopedWinUsbHandles&) = delete;
  ScopedWinUsbHandles& operator=(const ScopedWinUsbHandles&) = delete;
  ScopedWinUsbHandles(ScopedWinUsbHandles&&) = delete;
  ScopedWinUsbHandles& operator=(ScopedWinUsbHandles&&) = delete;

  bool Open(const std::wstring& instance_path) {
    Close();

    device_ =
        CreateFileW(instance_path.c_str(), GENERIC_WRITE | GENERIC_READ,
                    FILE_SHARE_WRITE | FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    if (device_ == INVALID_HANDLE_VALUE) {
      device_ = nullptr;
      return false;
    }

    if (WinUsb_Initialize(device_, &interface_) == FALSE) {
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
  }

  // Hand the handles to a caller that will close them itself.
  void Release(HANDLE& device, WINUSB_INTERFACE_HANDLE& interface_handle) {
    device = device_;
    interface_handle = interface_;
    device_ = nullptr;
    interface_ = nullptr;
  }

  HANDLE device() const { return device_; }
  WINUSB_INTERFACE_HANDLE interface_handle() const { return interface_; }

 private:
  HANDLE device_ = nullptr;
  WINUSB_INTERFACE_HANDLE interface_ = nullptr;
};

class WinUsbDevice : public IUsbDevice {
 public:
  explicit WinUsbDevice(ILogger* logger) : logger_(logger) {}

  const char* Name() const override { return "winusb"; }

  bool Enumerate(std::vector<DeviceInfo>& devices) override {
    devices.clear();

    std::vector<std::wstring> paths;
    if (!ListUsbInterfacePaths(paths)) {
      return false;
    }

    for (const std::wstring& path : paths) {
      ScopedWinUsbHandles handles;
      // Most USB devices are not ours and will not open under WinUSB at all,
      // which is the cheapest possible filter and the reason this is not an
      // error.
      if (!handles.Open(path)) {
        continue;
      }

      USB_DEVICE_DESCRIPTOR descriptor = {};
      ULONG transferred = 0;
      if (WinUsb_GetDescriptor(handles.interface_handle(),
                               USB_DEVICE_DESCRIPTOR_TYPE, 0, 0,
                               reinterpret_cast<UCHAR*>(&descriptor),
                               sizeof(descriptor), &transferred) == FALSE) {
        continue;
      }
      if (descriptor.idVendor != kVendorId ||
          descriptor.idProduct != kProductId) {
        continue;
      }

      DeviceInfo info;
      info.path = ToUtf8(path);

      UCHAR pipe_id = 0;
      size_t max_packet_bytes = 0;
      size_t maximum_transfer_bytes = 0;
      if (FindBulkInPipe(handles.interface_handle(), pipe_id, max_packet_bytes,
                         maximum_transfer_bytes)) {
        info.speed = SpeedFromPacketSize(max_packet_bytes);
      }

      info.product_string =
          ReadProductString(handles.interface_handle(), descriptor.iProduct);

      devices.push_back(std::move(info));
    }

    return true;
  }

  bool SendConfiguration(const std::string& path, bool test_mode) override {
    ScopedWinUsbHandles handles;
    if (!OpenSelected(path, handles, nullptr)) {
      return false;
    }

    WINUSB_SETUP_PACKET setup = {};
    setup.RequestType = kVendorRequestType;
    setup.Request = kConfigurationRequest;
    setup.Value = MakeConfigurationFlags(test_mode);
    setup.Index = 0;
    setup.Length = 0;

    if (WinUsb_ControlTransfer(handles.interface_handle(), setup, nullptr, 0,
                               nullptr, nullptr) != TRUE) {
      if (logger_ != nullptr) {
        logger_->Error("Sending the configuration request failed with error " +
                       std::to_string(GetLastError()));
      }
      return false;
    }
    return true;
  }

  std::unique_ptr<ISampleSource> OpenSource(const std::string& path,
                                            const UsbSourceOptions& options,
                                            TransferResult& result) override {
    result = TransferResult::kConnectionFailure;

    ScopedWinUsbHandles handles;
    DeviceInfo info;
    if (!OpenSelected(path, handles, &info)) {
      return nullptr;
    }

    UCHAR pipe_id = 0;
    size_t max_packet_bytes = 0;
    size_t maximum_transfer_bytes = 0;
    if (!FindBulkInPipe(handles.interface_handle(), pipe_id, max_packet_bytes,
                        maximum_transfer_bytes)) {
      if (logger_ != nullptr) {
        logger_->Error("The device has no bulk input pipe");
      }
      return nullptr;
    }

    const DeviceSpeed speed = SpeedFromPacketSize(max_packet_bytes);
    if (!SpeedCanCarryCapture(speed)) {
      if (logger_ != nullptr) {
        logger_->Error(std::string("The device is attached at ") +
                       DeviceSpeedName(speed) +
                       ", which cannot carry 80 MB/s. Move it to a USB 3 "
                       "port.");
      }
      return nullptr;
    }

    if (logger_ != nullptr) {
      logger_->Info("Opened the device at " + info.path + " (" +
                    DeviceSpeedName(speed) + "), pipe packet size " +
                    std::to_string(max_packet_bytes) +
                    " bytes, driver transfer limit " +
                    std::to_string(maximum_transfer_bytes / 1024) + " KiB");
    }

    HANDLE device_handle = nullptr;
    WINUSB_INTERFACE_HANDLE interface_handle = nullptr;
    handles.Release(device_handle, interface_handle);

    result = TransferResult::kSuccess;
    return std::make_unique<WinUsbSource>(
        device_handle, interface_handle, pipe_id, max_packet_bytes,
        maximum_transfer_bytes, options, logger_);
  }

 private:
  bool ListUsbInterfacePaths(std::vector<std::wstring>& paths) {
    paths.clear();

    // Retried rather than sized once: a device can appear between asking how
    // much room the list needs and asking for the list itself.
    for (int attempt = 0; attempt < 4; ++attempt) {
      ULONG required = 0;
      if (CM_Get_Device_Interface_List_SizeW(
              &required, const_cast<LPGUID>(&GUID_DEVINTERFACE_DDD_USB_DEVICE),
              nullptr, CM_GET_DEVICE_INTERFACE_LIST_PRESENT) != CR_SUCCESS) {
        if (logger_ != nullptr) {
          logger_->Error("Listing USB device interfaces failed");
        }
        return false;
      }

      std::vector<wchar_t> buffer(required + 1, L'\0');
      const CONFIGRET listed = CM_Get_Device_Interface_ListW(
          const_cast<LPGUID>(&GUID_DEVINTERFACE_DDD_USB_DEVICE), nullptr,
          buffer.data(), static_cast<ULONG>(buffer.size()),
          CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
      if (listed == CR_BUFFER_SMALL) {
        continue;
      }
      if (listed != CR_SUCCESS) {
        if (logger_ != nullptr) {
          logger_->Error("Listing USB device interfaces failed");
        }
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

  std::string ReadProductString(WINUSB_INTERFACE_HANDLE handle, UCHAR index) {
    if (index == 0) {
      return {};
    }

    // A string descriptor is a length byte, a type byte and UTF-16LE text.
    std::array<UCHAR, 256> buffer{};
    ULONG transferred = 0;
    if (WinUsb_GetDescriptor(handle, USB_STRING_DESCRIPTOR_TYPE, index, 0x0409,
                             buffer.data(), static_cast<ULONG>(buffer.size()),
                             &transferred) == FALSE) {
      return {};
    }
    if (transferred < 2) {
      return {};
    }

    const size_t characters = (transferred - 2) / sizeof(wchar_t);
    std::wstring wide(reinterpret_cast<const wchar_t*>(buffer.data() + 2),
                      characters);
    return ToUtf8(wide);
  }

  bool FindBulkInPipe(WINUSB_INTERFACE_HANDLE handle, UCHAR& pipe_id,
                      size_t& max_packet_bytes,
                      size_t& maximum_transfer_bytes) {
    USB_INTERFACE_DESCRIPTOR descriptor = {};
    if (WinUsb_QueryInterfaceSettings(handle, 0, &descriptor) != TRUE) {
      return false;
    }

    for (UCHAR index = 0; index < descriptor.bNumEndpoints; ++index) {
      WINUSB_PIPE_INFORMATION pipe = {};
      if (WinUsb_QueryPipe(handle, 0, index, &pipe) != TRUE) {
        continue;
      }
      if (pipe.PipeType != UsbdPipeTypeBulk ||
          !USB_ENDPOINT_DIRECTION_IN(pipe.PipeId)) {
        continue;
      }

      pipe_id = pipe.PipeId;
      max_packet_bytes = pipe.MaximumPacketSize;

      ULONG limit = 0;
      ULONG limit_size = sizeof(limit);
      if (WinUsb_GetPipePolicy(handle, pipe.PipeId, MAXIMUM_TRANSFER_SIZE,
                               &limit_size, &limit) == TRUE &&
          limit_size == sizeof(limit)) {
        maximum_transfer_bytes = limit;
      }
      return true;
    }

    return false;
  }

  // Open the preferred device, or the first one attached if it is not there.
  bool OpenSelected(const std::string& preferred_path,
                    ScopedWinUsbHandles& handles, DeviceInfo* chosen) {
    std::vector<DeviceInfo> devices;
    if (!Enumerate(devices)) {
      return false;
    }

    const DeviceInfo* const selected = SelectDevice(devices, preferred_path);
    if (selected == nullptr) {
      if (logger_ != nullptr) {
        logger_->Error("No Domesday Duplicator is attached");
      }
      return false;
    }

    if (!handles.Open(ToWide(selected->path))) {
      if (logger_ != nullptr) {
        logger_->Error("Opening the device failed with error " +
                       std::to_string(GetLastError()));
      }
      return false;
    }

    if (chosen != nullptr) {
      *chosen = *selected;
    }
    return true;
  }

  ILogger* logger_ = nullptr;
};

}  // namespace

std::unique_ptr<IUsbDevice> MakeWinUsbDevice(ILogger* logger) {
  return std::make_unique<WinUsbDevice>(logger);
}

}  // namespace ddd::capture

#endif  // _WIN32
