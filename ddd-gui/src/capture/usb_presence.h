/************************************************************************

    usb_presence.h

    Whether a device is on the bus at all, without opening it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>

namespace ddd::capture {

// "Is anything with these identifiers plugged in?", answered without opening
// anything.
//
// A narrow question with one job, and the job is telling two failures apart.
// Everything else in this application reaches a device by opening it, and an
// open that fails cannot say why: a cable that is not attached and a cable
// that is attached but not permitted look identical to the caller. Those two
// have completely different remedies — plug it in, versus install the udev
// rules or bind the driver — and the bring-up wizard's connectivity page is
// where a user finds out which of them they have.
//
// So this reads descriptors and stops. On Linux and macOS that is libusb's
// device list, which needs no permission on any of the three platforms'
// default configurations; on Windows it is the enumerated interface paths,
// which carry the identifiers in their text and are visible whatever driver a
// device is bound to.
//
// Thread-safety: safe to call from any thread. Each call sets up and tears
// down whatever it needs, which costs a few milliseconds and is called at
// human speed.

enum class UsbPresence {
  // Something with these identifiers is attached.
  kPresent,

  // Nothing is.
  kAbsent,

  // The bus could not be read, so neither answer may be given. Reported rather
  // than folded into kAbsent, because "not attached" is a claim and this is
  // the state where no claim can be made — and a wizard that told a user to
  // check a cable that is in fact plugged in has sent them the wrong way.
  kUnknown,
};

UsbPresence UsbDeviceAttached(uint16_t vendor_id, uint16_t product_id);

// The SuperSpeed Explorer Kit's on-board USB-UART bridge.
//
// Not a Duplicator and never enumerated as one — the personality table matches
// exact pairs precisely so that this does not appear in the device list as a
// board in recovery. It is worth asking about all the same: it is powered
// whenever the kit is, so seeing it while seeing no FX3 says the kit has power
// and its USB 3.0 link is not answering, which is a different problem from an
// unpowered board and has a different first thing to check.
inline constexpr uint16_t kCypressDebugBridgeVendorId = 0x04b4;
inline constexpr uint16_t kCypressDebugBridgeProductId = 0x0007;

}  // namespace ddd::capture
