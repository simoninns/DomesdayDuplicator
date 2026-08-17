/************************************************************************

    usb_blaster_unsupported.cpp

    What MakeUsbBlasterCable does on a platform with no byte pipe yet
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <memory>

#include "jtag_cable.h"
#include "logger.h"

namespace ddd::capture {

// What reaches the cable beside this file is written against libusb, which
// this build does not have: the Windows build talks to USB through WinUSB
// (src/capture/CMakeLists.txt chooses one backend or the other at configure
// time, and compiling both would mean requiring libusb on Windows to reach
// code that would never run there).
//
// So the cable is not available here yet, and this says so in those words
// rather than reporting the same "no cable found" a Linux machine with the
// connector unplugged would report. The two are entirely different problems
// and only one of them is the user's to fix.
//
// What is missing is small and it is named: an IFtdiTransport over WinUSB's
// bulk pipes. The protocol that rides on it, the SVF player above that, and
// every test either has are platform-independent and are built here already.
std::unique_ptr<IJtagCable> MakeUsbBlasterCable(ILogger* logger) {
  if (logger != nullptr) {
    logger->Error(
        "This build cannot drive a USB-Blaster: the cable driver is written "
        "against libusb and this platform's USB backend is WinUSB. Programme "
        "the FPGA with Quartus for now.");
  }
  return nullptr;
}

}  // namespace ddd::capture
