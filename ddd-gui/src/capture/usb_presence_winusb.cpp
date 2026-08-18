/************************************************************************

    usb_presence_winusb.cpp

    Whether a device is on the bus at all, without opening it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <windows.h>
// windows.h first: cfgmgr32.h and initguid.h both depend on its types.
#include <cfgmgr32.h>
#include <initguid.h>
#include <usbiodef.h>

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>

#include "usb_presence.h"

namespace ddd::capture {
namespace {

// Every USB device Windows has enumerated, whatever driver it is bound to,
// named by its device-interface path.
//
// That last part is what makes this the right question to ask here. The
// WinUSB backend can only *open* devices bound to WinUSB, so it cannot tell a
// cable that is absent from one sitting on Altera's own driver — which is
// exactly the Windows failure the bring-up wizard has to name. The interface
// list is visible either way.
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

}  // namespace

UsbPresence UsbDeviceAttached(uint16_t vendor_id, uint16_t product_id) {
  std::vector<std::wstring> paths;
  if (!ListUsbInterfacePaths(paths)) {
    return UsbPresence::kUnknown;
  }

  // An interface path spells the identifiers out — \\?\USB#VID_09FB&PID_6001#…
  // — and the case varies between Windows versions, so both sides are lowered
  // before they are compared.
  wchar_t wanted[32] = {};
  std::swprintf(wanted, sizeof(wanted) / sizeof(wanted[0]),
                L"vid_%04x&pid_%04x", vendor_id, product_id);

  for (std::wstring path : paths) {
    std::transform(path.begin(), path.end(), path.begin(),
                   [](wchar_t character) {
                     return static_cast<wchar_t>(std::towlower(character));
                   });
    if (path.find(wanted) != std::wstring::npos) {
      return UsbPresence::kPresent;
    }
  }

  return UsbPresence::kAbsent;
}

}  // namespace ddd::capture
