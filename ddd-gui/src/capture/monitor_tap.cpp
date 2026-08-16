/************************************************************************

    monitor_tap.cpp

    Reading a running capture without being able to disturb it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "monitor_tap.h"

#include <algorithm>
#include <cstring>

namespace ddd::capture {

void StatsPublisher::Publish(const CaptureStats& stats) {
  // Odd while the value is in flux. release on the way in so no reader can see
  // the new counter before the writes it announces.
  sequence_.fetch_add(1, std::memory_order_release);
  std::atomic_thread_fence(std::memory_order_release);

  value_ = stats;

  std::atomic_thread_fence(std::memory_order_release);
  sequence_.fetch_add(1, std::memory_order_release);
}

CaptureStats StatsPublisher::Read() const {
  CaptureStats copy;

  while (true) {
    const uint64_t before = sequence_.load(std::memory_order_acquire);
    if ((before & 1U) != 0) {
      // A write is in progress. Nothing to do but look again — the writer is
      // never waiting on us, so this always ends.
      continue;
    }

    std::atomic_thread_fence(std::memory_order_acquire);
    copy = value_;
    std::atomic_thread_fence(std::memory_order_acquire);

    const uint64_t after = sequence_.load(std::memory_order_acquire);
    if (before == after) {
      return copy;
    }
  }
}

void TelemetryPublisher::Publish(const FpgaTelemetry& telemetry) {
  sequence_.fetch_add(1, std::memory_order_release);
  std::atomic_thread_fence(std::memory_order_release);

  value_ = telemetry;

  std::atomic_thread_fence(std::memory_order_release);
  sequence_.fetch_add(1, std::memory_order_release);
}

FpgaTelemetry TelemetryPublisher::Read() const {
  FpgaTelemetry copy;

  while (true) {
    const uint64_t before = sequence_.load(std::memory_order_acquire);
    if ((before & 1U) != 0) {
      continue;
    }

    std::atomic_thread_fence(std::memory_order_acquire);
    copy = value_;
    std::atomic_thread_fence(std::memory_order_acquire);

    const uint64_t after = sequence_.load(std::memory_order_acquire);
    if (before == after) {
      return copy;
    }
  }
}

SnapshotPublisher::SnapshotPublisher(size_t snapshot_bytes)
    : snapshot_bytes_(snapshot_bytes) {
  for (Buffer& buffer : buffers_) {
    buffer.data.resize(snapshot_bytes_);
  }
}

void SnapshotPublisher::Publish(const uint8_t* wire_data, size_t byte_count) {
  Buffer& target = buffers_[write_index_];

  const size_t copied = std::min(byte_count, snapshot_bytes_);
  std::memcpy(target.data.data(), wire_data, copied);
  target.used = copied;
  target.generation = generation_.fetch_add(1) + 1;

  // Hand this buffer over and take whatever the reader is not using. One
  // exchange, so there is no window in which the reader could see a buffer that
  // is about to be written into.
  const unsigned previous =
      ready_.exchange(write_index_ | kFreshFlag, std::memory_order_acq_rel);
  write_index_ = previous & kIndexMask;
}

bool SnapshotPublisher::TryRead(std::vector<uint8_t>& out,
                                uint64_t& generation) {
  if ((ready_.load(std::memory_order_acquire) & kFreshFlag) == 0) {
    return false;
  }

  // Swap our buffer for the fresh one, clearing the flag as we take it.
  const unsigned previous =
      ready_.exchange(read_index_, std::memory_order_acq_rel);
  read_index_ = previous & kIndexMask;

  const Buffer& source = buffers_[read_index_];
  out.assign(source.data.begin(),
             source.data.begin() + static_cast<std::ptrdiff_t>(source.used));
  generation = source.generation;
  return true;
}

}  // namespace ddd::capture
