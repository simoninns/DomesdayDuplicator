/************************************************************************

    disk_buffer_ring.cpp

    The producer-to-consumer handoff at the centre of a capture
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "disk_buffer_ring.h"

#include <algorithm>
#include <cassert>

#include "memory_lock.h"

namespace ddd::capture {

DiskBufferRing::Geometry DiskBufferRing::PlanGeometry(
    size_t queue_size_bytes, size_t endpoint_max_packet_bytes) {
  Geometry geometry;

  const size_t clamped_queue = std::clamp(
      queue_size_bytes, kMinimumQueueSizeBytes, kMaximumQueueSizeBytes);

  // A packet size of zero means the caller has no endpoint to ask — a synthetic
  // source, or a test. The slot size is then the target unrounded.
  if (endpoint_max_packet_bytes == 0) {
    geometry.slot_size_bytes = kTargetSlotSizeBytes;
  } else {
    geometry.slot_size_bytes =
        (kTargetSlotSizeBytes / endpoint_max_packet_bytes) *
        endpoint_max_packet_bytes;

    // A packet larger than the target slot would round the slot to zero. One
    // packet per slot is then the smallest thing that can be transferred at
    // all, which is a strange device but not an impossible one.
    if (geometry.slot_size_bytes == 0) {
      geometry.slot_size_bytes = endpoint_max_packet_bytes;
    }
  }

  geometry.slot_count = clamped_queue / geometry.slot_size_bytes;

  // Three is the floor the shutdown protocol needs: the transfer thread can be
  // filling one slot while the processing thread works on another, and the
  // third is what keeps a graceful stop from having to interleave the two.
  geometry.slot_count = std::max<size_t>(geometry.slot_count, 3);
  return geometry;
}

DiskBufferRing::DiskBufferRing(Geometry geometry) : geometry_(geometry) {
  assert(geometry_.slot_count > 0);
  assert(geometry_.slot_size_bytes > 0);

  slots_ = std::make_unique<Slot[]>(geometry_.slot_count);
  for (size_t index = 0; index < geometry_.slot_count; ++index) {
    slots_[index].data.resize(geometry_.slot_size_bytes);
  }
}

DiskBufferRing::~DiskBufferRing() {
  if (memory_locked_) {
    for (size_t index = 0; index < geometry_.slot_count; ++index) {
      UnlockMemoryRegion(slots_[index].data.data(), slots_[index].data.size());
    }
  }
}

uint8_t* DiskBufferRing::SlotData(size_t index) {
  assert(index < geometry_.slot_count);
  return slots_[index].data.data();
}

const uint8_t* DiskBufferRing::SlotData(size_t index) const {
  assert(index < geometry_.slot_count);
  return slots_[index].data.data();
}

bool DiskBufferRing::WaitForSlotFree(size_t index) {
  assert(index < geometry_.slot_count);
  Slot& slot = slots_[index];

  // wait(observed) returns once the value is no longer `observed`, so a slot
  // that is already empty costs one atomic load and no system call. Re-reading
  // after the wake rather than trusting it is what makes a spurious wake
  // harmless.
  uint32_t observed = slot.state.load(std::memory_order_acquire);
  while (observed != kSlotEmpty) {
    if (observed == kSlotAborted) {
      return false;
    }
    slot.state.wait(observed, std::memory_order_acquire);
    observed = slot.state.load(std::memory_order_acquire);
  }

  return true;
}

DiskBufferRing::FillResult DiskBufferRing::MarkSlotFull(size_t index) {
  assert(index < geometry_.slot_count);
  Slot& slot = slots_[index];

  uint32_t expected = kSlotEmpty;
  if (!slot.state.compare_exchange_strong(expected, kSlotFull,
                                          std::memory_order_acq_rel)) {
    return (expected == kSlotAborted) ? FillResult::kAborted
                                      : FillResult::kOverflow;
  }

  const uint64_t filled = slots_filled_.fetch_add(1) + 1;
  const size_t in_use = static_cast<size_t>(filled - slots_freed_.load());

  // Raise the high-water mark. A compare-and-swap loop rather than fetch_max,
  // which C++20 does not have for integers.
  size_t peak = peak_slots_in_use_.load();
  while (in_use > peak &&
         !peak_slots_in_use_.compare_exchange_weak(peak, in_use)) {
  }

  slot.state.notify_all();
  return FillResult::kHandedOver;
}

bool DiskBufferRing::WaitForSlotFull(size_t index) {
  assert(index < geometry_.slot_count);
  Slot& slot = slots_[index];

  uint32_t observed = slot.state.load(std::memory_order_acquire);
  while (observed == kSlotEmpty) {
    slot.state.wait(observed, std::memory_order_acquire);
    observed = slot.state.load(std::memory_order_acquire);
  }

  // Dumped and aborted slots both wake the consumer, and neither holds data.
  return observed == kSlotFull;
}

void DiskBufferRing::MarkSlotFree(size_t index) {
  assert(index < geometry_.slot_count);
  Slot& slot = slots_[index];

  uint32_t expected = kSlotFull;
  if (slot.state.compare_exchange_strong(expected, kSlotEmpty,
                                         std::memory_order_acq_rel)) {
    slots_freed_.fetch_add(1);
    slot.state.notify_all();
    return;
  }

  // A slot released during a graceful stop was never handed over, so returning
  // it must not count against the fill level.
  expected = kSlotDumped;
  if (slot.state.compare_exchange_strong(expected, kSlotEmpty,
                                         std::memory_order_acq_rel)) {
    slot.state.notify_all();
  }

  // Anything else — already empty, or aborted — is left exactly as it is. An
  // aborted slot in particular must never become fillable again.
}

size_t DiskBufferRing::SlotsInUse() const {
  // Read freed first. If a handoff lands between the two loads the result is
  // one too high, which reads as a momentarily deeper queue; reading the other
  // way round could produce a negative difference in unsigned arithmetic, and a
  // fill level of eighteen quintillion is not a useful thing to display.
  const uint64_t freed = slots_freed_.load();
  const uint64_t filled = slots_filled_.load();
  return static_cast<size_t>(filled - std::min(filled, freed));
}

void DiskBufferRing::MarkEmptySlotsDumped() {
  for (size_t index = 0; index < geometry_.slot_count; ++index) {
    Slot& slot = slots_[index];
    uint32_t expected = kSlotEmpty;
    if (slot.state.compare_exchange_strong(expected, kSlotDumped,
                                           std::memory_order_acq_rel)) {
      slot.state.notify_all();
    }
  }
}

void DiskBufferRing::Abort() {
  abort_requested_.store(true);

  // One unconditional store per slot to a value nobody can be waiting on, and
  // no path back from it. Every blocked thread wakes, re-reads, and sees a
  // value that cannot become what it was waiting for again — which is why this
  // cannot leave a waiter behind however the scheduler orders it.
  for (size_t index = 0; index < geometry_.slot_count; ++index) {
    slots_[index].state.store(kSlotAborted, std::memory_order_release);
    slots_[index].state.notify_all();
  }
}

std::string DiskBufferRing::LockIntoMemory() {
  std::string failure;
  size_t locked = 0;

  for (size_t index = 0; index < geometry_.slot_count; ++index) {
    const MemoryLockResult result =
        LockMemoryRegion(slots_[index].data.data(), slots_[index].data.size());
    if (!result.locked) {
      // The first refusal is the informative one; the remaining slots will fail
      // for the same reason, and repeating it in the log adds nothing. Whatever
      // was locked before the refusal is unlocked again, so the ring is either
      // wholly locked or wholly not — a half-locked ring would be a stall
      // waiting for the one slot that happened to fall outside the allowance.
      failure = result.message;
      break;
    }
    ++locked;
  }

  if (!failure.empty()) {
    for (size_t index = 0; index < locked; ++index) {
      UnlockMemoryRegion(slots_[index].data.data(), slots_[index].data.size());
    }
    return failure;
  }

  memory_locked_ = true;
  return {};
}

}  // namespace ddd::capture
