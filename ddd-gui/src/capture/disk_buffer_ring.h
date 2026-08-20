/************************************************************************

    disk_buffer_ring.h

    The producer-to-consumer handoff at the centre of a capture
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ddd::capture {

// A ring of large, page-locked buffers handed from the transfer thread to the
// processing thread.
//
// This is the only slack in the whole system. The device has 64 KB of buffer
// and cannot wait; whatever the processing thread is doing — validating,
// encoding FLAC, writing to a disk that has just decided to flush — has to fit
// inside the time the ring can absorb. At 80 MB/s a 256 MB queue is 3.2
// seconds, and that number is the answer to "how long may a write stall?".
//
// The handoff uses one atomic per slot and no mutex or condition variable
// anywhere. That is not a micro-optimisation: a mutex on this path admits
// priority inversion, where the low-priority thread holding the lock is
// descheduled and the real-time thread waits behind it. C++20's atomic
// wait/notify has the same blocking behaviour without the ownership, so a
// waiter can always be released from outside.
//
// A state value rather than a pair of flags, and that choice is load-bearing.
// The obvious design gives each slot a "full" flag and releases blocked threads
// at shutdown by toggling it — clear to wake the consumer, set to wake the
// producer. It has a race: a waiter that has not been scheduled between the two
// toggles re-reads the flag, finds the value it was already waiting on, and
// blocks again, permanently. Four distinct states mean shutdown moves a slot to
// a value no one is waiting for and leaves it there, so no wake can be missed.
//
// Thread-safety: designed for exactly one producer and one consumer, plus any
// number of threads calling the const observers and Abort(). Two producers or
// two consumers would break it, and nothing in the engine has any use for
// either.
class DiskBufferRing {
 public:
  // How the ring is laid out. Both figures come from PlanGeometry(), which is
  // the only place the sizing rules live.
  struct Geometry {
    size_t slot_size_bytes = 0;
    size_t slot_count = 0;

    size_t TotalBytes() const { return slot_size_bytes * slot_count; }
  };

  // What happened when the producer tried to hand a slot over.
  enum class FillResult {
    kHandedOver,

    // The slot was still full: the consumer never caught up, and the producer
    // has written over data that was never processed. There is no recovery —
    // the samples are gone — so this is the one condition that makes a capture
    // silently wrong, and it is detected here rather than inferred later.
    kOverflow,

    // The ring was torn down underneath the producer. Not an overflow, and
    // reporting it as one would send a user off to buy a faster disk.
    kAborted,
  };

  // Default queue size, and the range the application offers.
  //
  // 256 MB is 3.2 seconds at full rate, which comfortably covers the write
  // stalls seen in the field. Below 64 MB the margin stops covering an ordinary
  // filesystem flush; above 512 MB the extra depth buys nothing that the disk
  // was not already keeping up with, and it is memory locked out of the rest of
  // the machine.
  static constexpr size_t kDefaultQueueSizeBytes = size_t{256} << 20;
  static constexpr size_t kMinimumQueueSizeBytes = size_t{64} << 20;
  static constexpr size_t kMaximumQueueSizeBytes = size_t{512} << 20;

  // Target size of one slot before rounding.
  //
  // 2 MB is the largest single transfer the WinUSB backend will accept, and the
  // libusb backend has no way to ask, so the same figure is used on both. It
  // also sets the granularity of everything downstream: a slot is the unit a
  // sink is attached or detached at, and the unit a sequence error is detected
  // in.
  static constexpr size_t kTargetSlotSizeBytes = size_t{2} << 20;

  // Work out slot size and count for a queue size.
  //
  // The slot size is rounded down to a whole number of endpoint packets so that
  // no transfer ever ends mid-packet, which is what makes a short packet
  // unambiguously an error rather than a boundary effect.
  static Geometry PlanGeometry(size_t queue_size_bytes,
                               size_t endpoint_max_packet_bytes);

  explicit DiskBufferRing(Geometry geometry);
  ~DiskBufferRing();

  DiskBufferRing(const DiskBufferRing&) = delete;
  DiskBufferRing& operator=(const DiskBufferRing&) = delete;
  DiskBufferRing(DiskBufferRing&&) = delete;
  DiskBufferRing& operator=(DiskBufferRing&&) = delete;

  size_t slot_count() const { return geometry_.slot_count; }
  size_t slot_size_bytes() const { return geometry_.slot_size_bytes; }

  // The bytes of slot `index`. The producer writes here before marking the slot
  // full; the consumer reads and rewrites here (marker stripping happens in
  // place) before marking it free. No one may touch a slot outside those two
  // windows.
  uint8_t* SlotData(size_t index);
  const uint8_t* SlotData(size_t index) const;

  // --- Producer side -------------------------------------------------------

  // Block until slot `index` is free. Returns immediately if it already is.
  // Returns false if the wait ended because the ring was torn down.
  bool WaitForSlotFree(size_t index);

  // Hand slot `index` to the consumer.
  FillResult MarkSlotFull(size_t index);

  // --- Consumer side -------------------------------------------------------

  // Block until slot `index` holds data. Returns false if the slot was released
  // rather than filled, which is how a stopping capture frees a consumer that
  // would otherwise wait for a buffer nobody is going to fill.
  bool WaitForSlotFull(size_t index);

  // Return slot `index` to the producer.
  void MarkSlotFree(size_t index);

  // --- Shutdown ------------------------------------------------------------

  // Mark every empty slot as dumped and wake anyone waiting on one. This is the
  // graceful path: a consumer blocked on a slot the producer has stopped
  // filling wakes, sees that the slot holds nothing, and finishes with what it
  // already has. Slots that hold real data are left alone, so nothing already
  // handed over is discarded.
  void MarkEmptySlotsDumped();

  // Release every waiter on both sides, whatever they are waiting for, and
  // refuse every subsequent handover. Slots stay aborted: there is no path back
  // from this, which is exactly what makes it impossible to miss.
  void Abort();

  // Whether Abort() has been called. Both workers check this to tell a forced
  // shutdown from a graceful one.
  bool AbortRequested() const { return abort_requested_.load(); }

  // --- Observers -----------------------------------------------------------

  // Slots currently holding data the consumer has not finished with. This is
  // the fill level the user sees, and the number that says whether a machine is
  // keeping up: a healthy capture sits near zero and never climbs.
  size_t SlotsInUse() const;

  // The high-water mark since construction, which is the honest figure for a
  // capture that was fine except for one stall thirty minutes in.
  size_t PeakSlotsInUse() const { return peak_slots_in_use_.load(); }

  // Slots handed over in total. The consumer's own count of buffers processed
  // should track this exactly; a difference is a dropped handoff.
  uint64_t SlotsFilled() const { return slots_filled_.load(); }

  // Slots given back in total. The pair says what a run ended holding: at rest
  // the two are equal, and a run that stopped with buffers still in the ring
  // ended with the consumer that many behind the producer.
  uint64_t SlotsFreed() const { return slots_freed_.load(); }

  // Pin every slot into physical memory. Returns a description of what could
  // not be locked, or an empty string on complete success — degrading rather
  // than failing, because an unlocked capture is more exposed, not impossible.
  std::string LockIntoMemory();

  bool memory_locked() const { return memory_locked_; }

 private:
  // What a slot is doing. The values are the ones threads wait on, so they must
  // stay distinct: a wait returns precisely when the value stops being the one
  // the waiter last saw.
  //
  //   kEmpty    the producer may fill it
  //   kFull     it holds data the consumer has not taken yet
  //   kDumped   released during a graceful stop without ever being filled
  //   kAborted  the ring has been torn down; nothing may be done with it
  enum : uint32_t {
    kSlotEmpty = 0,
    kSlotFull = 1,
    kSlotDumped = 2,
    kSlotAborted = 3,
  };

  struct Slot {
    std::vector<uint8_t> data;
    std::atomic<uint32_t> state{kSlotEmpty};
  };

  Geometry geometry_;
  std::unique_ptr<Slot[]> slots_;

  // Fill level as two monotonic counters rather than one that goes up and
  // down. Each is written by exactly one thread — filled by the producer, freed
  // by the consumer — so neither needs a read-modify-write loop, and their
  // difference is the depth. A single shared counter would need one, and would
  // still read wrong to an observer that caught it mid-update.
  std::atomic<uint64_t> slots_filled_{0};
  std::atomic<uint64_t> slots_freed_{0};
  std::atomic<size_t> peak_slots_in_use_{0};
  std::atomic<bool> abort_requested_{false};

  bool memory_locked_ = false;
};

}  // namespace ddd::capture
