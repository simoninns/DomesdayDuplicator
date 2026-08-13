/************************************************************************

    libusb_device.h

    Finding and configuring the device with libusb (Linux and macOS)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <memory>

#include "usb_device.h"

namespace ddd::capture {

class ILogger;

// The libusb backend. Returns nothing if libusb itself could not be started,
// which is the only failure that is not a property of a particular device.
//
// libusb.h stays out of this header on purpose: it is found through pkg-config
// and its include path differs between distributions and between Homebrew and
// MacPorts, so every file that included it would inherit that. The
// implementation includes it; nothing else needs to.
std::unique_ptr<IUsbDevice> MakeLibUsbDevice(ILogger* logger);

}  // namespace ddd::capture
