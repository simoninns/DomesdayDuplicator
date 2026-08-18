/************************************************************************

    device_programmer.h

    The seam between the recovery flow and a device with no firmware
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace ddd::capture {

class ILogger;
class IUsbDevice;

// A device sitting in its boot ROM, and the only three things it can be asked
// to do.
//
// This is the second seam of the update mechanism, beside IDeviceUpdater, and
// it exists because a device in a recovery personality answers none of the
// update requests: there is no firmware on it to answer them. What answers is
// the FX3's boot ROM, which implements exactly one command — put these bytes
// at this address — and one way of ending — jump there.
//
// That is enough. The bytes handed over are the update bundle's own
// firmware.img, so the device wakes up as an ordinary Domesday Duplicator
// running from RAM, and everything after that point is the update path that
// already exists: the same protocol, the same SHA-256 stream and readback
// checks, the same confirmation. The Cypress secondary loader is deliberately
// not used — this project's own firmware is a better programmer for this
// project's own EEPROM, and it is a programmer that is verified by every
// other test in the suite.
//
// Every call is blocking. This is driven from a worker thread, never from a
// user interface thread.
//
// Thread-safety: NOT thread-safe. One thread owns a programmer for its
// lifetime.
class IDeviceProgrammer {
 public:
  IDeviceProgrammer() = default;
  virtual ~IDeviceProgrammer() = default;

  IDeviceProgrammer(const IDeviceProgrammer&) = delete;
  IDeviceProgrammer& operator=(const IDeviceProgrammer&) = delete;
  IDeviceProgrammer(IDeviceProgrammer&&) = delete;
  IDeviceProgrammer& operator=(IDeviceProgrammer&&) = delete;

  // Put one run of bytes into the device's RAM. Split into whatever transfers
  // the boot ROM accepts by the implementation, so a caller hands over a
  // whole section and does not have to know.
  virtual bool WriteRam(uint32_t address, std::span<const uint8_t> data) = 0;

  // Jump to the entry point.
  //
  // The device stops answering as it runs what it was given, so the ordinary
  // ways a request to a departing device fails are not failures. Whether it
  // worked is answered by WaitForApplication and by nothing else.
  virtual bool Start(uint32_t entry_address) = 0;

  // Wait for a device running the Duplicator's firmware to appear, and return
  // the path it appeared at.
  //
  // The path is returned rather than assumed because it does not survive the
  // change of identity on every platform: it is built from bus and port
  // numbers on libusb, which are the same before and after, but from a device
  // interface path on Windows, which carries the product identifier and
  // therefore changes when the personality does.
  virtual std::optional<std::string> WaitForApplication(
      std::chrono::milliseconds timeout) = 0;
};

// A programmer for the device at `path`, over whichever USB backend this
// build has.
//
// Returns nothing if the device could not be opened. On Windows that is the
// expected outcome until WinUSB has been bound to the recovery personality —
// see the "If an update fails" documentation page, which is where a user is
// told what to do about it.
std::unique_ptr<IDeviceProgrammer> MakeDeviceProgrammer(IUsbDevice& usb,
                                                        const std::string& path,
                                                        ILogger* logger);

}  // namespace ddd::capture
