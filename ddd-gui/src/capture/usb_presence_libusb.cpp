/************************************************************************

    usb_presence_libusb.cpp

    Whether a device is on the bus at all, without opening it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <libusb.h>

#include "usb_presence.h"

namespace ddd::capture {

UsbPresence UsbDeviceAttached(uint16_t vendor_id, uint16_t product_id) {
  libusb_context* context = nullptr;
#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x0100010A
  const int started = libusb_init_context(&context, nullptr, 0);
#else
  const int started = libusb_init(&context);
#endif
  if (started != 0) {
    return UsbPresence::kUnknown;
  }

  libusb_device** list = nullptr;
  const ssize_t count = libusb_get_device_list(context, &list);
  if (count < 0) {
    libusb_exit(context);
    return UsbPresence::kUnknown;
  }

  // The device descriptor is cached by the kernel and handed over without a
  // transfer, so nothing here opens a device or needs permission to. That is
  // the whole point: this has to answer for a cable the caller is about to
  // discover it may not open.
  UsbPresence presence = UsbPresence::kAbsent;
  for (ssize_t index = 0; index < count; ++index) {
    libusb_device_descriptor descriptor = {};
    if (libusb_get_device_descriptor(list[index], &descriptor) != 0) {
      continue;
    }
    if (descriptor.idVendor == vendor_id &&
        descriptor.idProduct == product_id) {
      presence = UsbPresence::kPresent;
      break;
    }
  }

  libusb_free_device_list(list, 1);
  libusb_exit(context);
  return presence;
}

}  // namespace ddd::capture
