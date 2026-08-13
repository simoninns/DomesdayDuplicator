/************************************************************************

    usb_device.h

    The seam between the application and a USB backend
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "sample_source.h"
#include "transfer_result.h"
#include "usb_device_info.h"

namespace ddd::capture {

class ILogger;

// How a streaming source should talk to the device.
struct UsbSourceOptions {
  // Split each ring slot into several small transfers rather than reading it in
  // one.
  //
  // On by default, and it is the setting that makes the difference on a loaded
  // machine. One 2 MB transfer per slot means that between the moment a
  // transfer completes and the moment the next is submitted, nothing is
  // listening — and at 80 MB/s the device's 64 KB of buffer covers 800
  // microseconds of that. Many smaller transfers, several in flight at once,
  // mean the endpoint always has somewhere to put the next packet.
  bool small_transfers = true;

  // Roughly how many bytes of transfer may be outstanding at once.
  //
  // Capped at 12 MB because of a Linux limit rather than anything about the
  // device: usbfs refuses to map more than 16 MB of transfer buffers per
  // process by default, and a capture that asks for more fails at submission
  // with ENOMEM. See kUsbMemoryLimit, which exists to tell the user that in
  // terms they can act on.
  size_t transfer_queue_bytes = size_t{12} << 20;

  // Ring slots to fill and throw away before handing any downstream.
  //
  // The device is already streaming when the host opens it, so the first
  // transfers hold whatever was mid-flight. Four slots is what the old engine
  // used and what years of field captures were made with.
  uint64_t discard_slots = 4;
};

// What the transfers are going to look like, worked out before any of them are
// created.
//
// A plain value produced by a pure function, because this is the part of the
// USB backends that is most intricate and least observable. Getting the stride
// wrong by one produces a capture that is subtly interleaved rather than one
// that fails, and the only way to find that on hardware is to notice that a
// disc sounds wrong. As arithmetic over a struct it can simply be checked.
struct TransferLayout {
  // Bytes in one USB transfer. Always a whole number of endpoint packets, and
  // always an exact divisor of a slot.
  size_t transfer_bytes = 0;

  size_t transfers_per_slot = 0;

  // How many slots the in-flight transfers span. A transfer that completes
  // against slot i is resubmitted against slot i + slot_span, which is how the
  // fixed set of transfers rolls through a much larger ring.
  size_t slot_span = 0;

  // Transfers created and submitted: transfers_per_slot * slot_span.
  size_t transfer_count = 0;

  // The ring slot the first transfer is aimed at.
  //
  // Not zero, and that is the point of it. The consumer reads slots in order
  // from zero, so the first slot handed over has to be slot zero — which means
  // the discarded slots have to come before it, wrapping round the end of the
  // ring. Starting at slot_count - discard_slots is what makes the first slot
  // the consumer ever sees be the first slot worth having.
  size_t first_slot_index = 0;

  // Transfers to complete and discard before anything is handed over.
  uint64_t discard_transfers = 0;

  uint64_t discard_slots = 0;

  bool valid = false;

  // Why not, when invalid. Empty when valid.
  std::string problem;
};

// Work out the transfer geometry for a ring and an endpoint.
//
// Pure: no device, no state, no allocation beyond the returned string. Both USB
// backends call this and neither has its own copy of the arithmetic, which is a
// change from the old engine where the two backends had the same calculation
// written twice with subtly different starting indices.
TransferLayout PlanTransferLayout(size_t slot_bytes, size_t slot_count,
                                  size_t endpoint_max_packet_bytes,
                                  const UsbSourceOptions& options);

// A USB backend: how devices are found, configured and opened for streaming.
//
// An interface rather than a concrete class for two reasons. One is that there
// are two implementations, libusb and WinUSB, and the application should not
// know which it has. The other is that a test can supply a third — one that
// reports whatever set of devices the test wants and hands back a synthetic
// source — and with that, the whole of the application's device handling,
// including the parts users touch, is testable on a machine with nothing
// plugged into it.
//
// Thread-safety: Enumerate() may be called from the device monitor's thread
// while the other methods are called from the controlling thread.
// Implementations must tolerate that. Enumerate() must not be called while a
// source from OpenSource() is running — see DeviceMonitor::SetSuspended().
class IUsbDevice {
 public:
  IUsbDevice() = default;
  virtual ~IUsbDevice() = default;

  IUsbDevice(const IUsbDevice&) = delete;
  IUsbDevice& operator=(const IUsbDevice&) = delete;
  IUsbDevice(IUsbDevice&&) = delete;
  IUsbDevice& operator=(IUsbDevice&&) = delete;

  // A name for logs ("libusb", "winusb", ...)
  virtual const char* Name() const = 0;

  // Every attached Domesday Duplicator, including any attached at a speed that
  // cannot carry a capture.
  //
  // Those are returned rather than filtered out on purpose: a device that is
  // present but on the wrong port is the case worth reporting precisely, and a
  // backend that dropped it would leave the application saying "no device
  // attached" to someone looking straight at one.
  virtual bool Enumerate(std::vector<DeviceInfo>& devices) = 0;

  // Send the 0xB6 configuration request, whose only defined bit selects the
  // gateware's internal test pattern. Opens the device, sends, and closes.
  virtual bool SendConfiguration(const std::string& path, bool test_mode) = 0;

  // Open a device and prepare a source that will stream from it.
  //
  // Returns nothing and sets `result` on failure, so a caller can tell a device
  // that was not there from one that was there at the wrong speed. The returned
  // source has the device open and its interface claimed — which is what lets
  // it answer PlanGeometry(), since the slot size depends on the endpoint's
  // packet size and that cannot be known before the device is open.
  virtual std::unique_ptr<ISampleSource> OpenSource(
      const std::string& path, const UsbSourceOptions& options,
      TransferResult& result) = 0;
};

// The backend for this platform: WinUSB on Windows, libusb everywhere else.
//
// Returns nothing if the backend could not be initialised at all, which on
// Linux and macOS means libusb itself failed to start.
std::unique_ptr<IUsbDevice> MakeUsbDevice(ILogger* logger);

}  // namespace ddd::capture
