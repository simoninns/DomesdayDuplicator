/************************************************************************

    winusb_device.h

    Finding and configuring the device with WinUSB (Windows)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <memory>

#include "usb_device.h"

namespace ddd::capture {

class ILogger;

// The WinUSB backend. Only built on Windows; MakeUsbDevice() chooses between
// this and libusb at compile time rather than at run time, because a build for
// one platform has no use for the other's dependencies.
//
// Windows.h stays out of this header, as it does out of every header here. It
// defines several hundred macros — min, max, ERROR and near enough anything
// else — and a single translation unit that includes it by accident produces
// errors nowhere near the file that caused them.
std::unique_ptr<IUsbDevice> MakeWinUsbDevice(ILogger* logger);

}  // namespace ddd::capture
