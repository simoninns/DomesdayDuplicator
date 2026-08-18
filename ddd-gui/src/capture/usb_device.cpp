/************************************************************************

    usb_device.cpp

    The seam between the application and a USB backend
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "usb_device.h"

#include <algorithm>

#include "logger.h"

#ifdef _WIN32
#include "winusb_device.h"
#else
#include "libusb_device.h"
#endif

namespace ddd::capture {
namespace {

// The size a small transfer aims for before it is rounded to whole packets.
//
// 128 KB, which is what the old engine used and what the field evidence is
// about. It is a compromise between two failures: transfers too large leave
// gaps between completion and resubmission that the device's 64 KB FIFO cannot
// cover, and transfers too small spend the CPU on submission overhead at 80
// MB/s.
constexpr size_t kTargetSmallTransferBytes = size_t{128} << 10;

}  // namespace

TransferLayout PlanTransferLayout(size_t slot_bytes, size_t slot_count,
                                  size_t endpoint_max_packet_bytes,
                                  const UsbSourceOptions& options) {
  TransferLayout layout;

  if (endpoint_max_packet_bytes == 0) {
    layout.problem = "the device reported no endpoint packet size";
    return layout;
  }
  if (slot_bytes == 0 || (slot_bytes % endpoint_max_packet_bytes) != 0) {
    layout.problem =
        "the buffer size is not a whole number of endpoint packets";
    return layout;
  }

  // Four slots is the smallest ring the rolling scheme works in: one being
  // filled, one being processed, one for the producer to wait on, and one of
  // slack. Below that the transfer span collapses to nothing.
  if (slot_count < 4) {
    layout.problem = "the buffer queue is too small to stream from";
    return layout;
  }

  const size_t packets_per_slot = slot_bytes / endpoint_max_packet_bytes;

  if (!options.small_transfers) {
    layout.transfer_bytes = slot_bytes;
    layout.transfers_per_slot = 1;

    // Every slot but one has a transfer aimed at it, leaving exactly one for
    // the consumer to be working on.
    layout.slot_span = slot_count - 1;
  } else {
    // Worked out in packets rather than bytes so the division is exact by
    // construction. A transfer that ended part-way through a packet would make
    // a short read ambiguous — is it the end of the data, or a fault? — and
    // that ambiguity is what the SHORT_NOT_OK flag exists to refuse.
    const size_t target_packets = std::max(
        size_t{1}, kTargetSmallTransferBytes / endpoint_max_packet_bytes);

    size_t transfers_per_slot = std::max(
        size_t{1}, (packets_per_slot + target_packets - 1) / target_packets);

    // Nudge upwards until it divides. It always terminates: at worst it reaches
    // packets_per_slot, one packet per transfer.
    while ((packets_per_slot % transfers_per_slot) != 0) {
      ++transfers_per_slot;
    }

    layout.transfers_per_slot = transfers_per_slot;
    layout.transfer_bytes =
        (packets_per_slot / transfers_per_slot) * endpoint_max_packet_bytes;

    // Two slots held back rather than one: the consumer needs a slot to be
    // working on, and the producer needs one it can block on without having
    // already run out of anywhere to put data.
    const size_t queue_span =
        std::max(size_t{1}, options.transfer_queue_bytes / slot_bytes);
    layout.slot_span = std::min(queue_span, slot_count - 2);
  }

  layout.transfer_count = layout.transfers_per_slot * layout.slot_span;

  // A discard longer than the ring would wrap past its own starting point and
  // throw away data it had already handed over.
  layout.discard_slots = std::min<uint64_t>(options.discard_slots, slot_count);
  layout.discard_transfers = layout.discard_slots * layout.transfers_per_slot;
  layout.first_slot_index =
      (slot_count - static_cast<size_t>(layout.discard_slots)) % slot_count;

  layout.valid = true;
  return layout;
}

std::unique_ptr<IUsbDevice> MakeUsbDevice(ILogger* logger) {
#ifdef _WIN32
  return MakeWinUsbDevice(logger);
#else
  return MakeLibUsbDevice(logger);
#endif
}

}  // namespace ddd::capture
