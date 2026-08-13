/************************************************************************

    monitor_tap.h

    Reading a running capture without being able to disturb it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "sample_metrics.h"
#include "sequence_validator.h"
#include "transfer_result.h"

namespace ddd::capture {

// The rule this file exists to enforce: the pipeline never waits for a
// consumer.
//
// A monitoring GUI is on the far side of a compositor, a theme engine and a
// user who may have just dragged a window across a second monitor. If the
// processing thread could ever block on it — for a lock, for a queue slot, for
// anything — then a stutter in the GUI would become lost samples, and the
// application's own display would be the thing that ruined the capture. So
// every publication here is wait-free on the writer's side, and every consumer
// is free to be slow, to fall behind, or to stop reading altogether. A slow
// consumer misses snapshots. It never costs a sample.

// Everything the panels need, in one value that is published atomically.
struct CaptureStats {
  TransferResult result = TransferResult::kRunning;

  double elapsed_seconds = 0.0;
  double throughput_bytes_per_second = 0.0;

  uint64_t transfers_completed = 0;
  uint64_t buffers_processed = 0;
  uint64_t bytes_written = 0;
  uint64_t samples_written = 0;

  // Samples the sink has taken but not yet committed. See
  // ISampleSink::SamplesPending — zero unless something is compressing.
  uint64_t samples_pending = 0;

  // The sink attached at the moment this was published: "null" while
  // monitoring, "flac" while capturing. Carried in the stats block because the
  // distinction is what the panels label everything else by, and asking the
  // pipeline separately would be a second read that could disagree with this
  // one.
  bool writing = false;

  // Ring depth. The number that says whether this machine is keeping up.
  size_t slots_in_use = 0;
  size_t peak_slots_in_use = 0;
  size_t slot_count = 0;

  SequenceState sequence_state = SequenceState::kSynchronising;

  // Test mode only, and nothing until a capture has run in it
  bool test_pattern_checked = false;
  bool test_pattern_passed = true;

  SampleMetricsSnapshot metrics;
};

// Publishes a value that readers can take a consistent copy of without ever
// making the writer wait.
//
// A sequence lock: the writer bumps an odd counter before touching the value
// and an even one after, and a reader that sees an odd counter, or two
// different counters either side of its read, tries again. The writer's cost is
// two atomic stores and a copy; it never blocks and it never even notices a
// reader.
//
// Thread-safety: exactly one writer thread, any number of readers.
class StatsPublisher {
 public:
  void Publish(const CaptureStats& stats);

  // Take a consistent copy. Retries internally; the writer publishes at buffer
  // rate, so a reader colliding twice in a row is a coincidence rather than a
  // pattern, and there is no unbounded loop in practice.
  CaptureStats Read() const;

  // How many times a value has been published. The tests use this to prove a
  // reader saw a complete generation rather than a plausible-looking mixture of
  // two.
  uint64_t Generation() const { return sequence_.load() / 2; }

 private:
  // Even means settled, odd means a write is in progress.
  std::atomic<uint64_t> sequence_{0};
  CaptureStats value_;
};

// A copy of recent raw samples, for the waveform and spectrum displays.
//
// Triple buffered rather than locked: the writer always has a buffer nobody is
// reading, the reader always has one nobody is writing, and the third is
// whichever was most recently finished. Publishing is one atomic exchange —
// no copy on handover, no wait, and no chance of a reader seeing a buffer being
// filled.
//
// Thread-safety: exactly one writer thread, exactly one reader thread.
class SnapshotPublisher {
 public:
  // A snapshot is a fixed size chosen once. 64 KiB is 32,768 samples, which is
  // far more than any display needs and small enough that copying it out of a
  // 2 MB buffer is a rounding error against the work already being done to that
  // buffer.
  static constexpr size_t kDefaultSnapshotBytes = size_t{64} << 10;

  explicit SnapshotPublisher(size_t snapshot_bytes = kDefaultSnapshotBytes);

  // Copy up to snapshot_bytes from a buffer and make it the current snapshot.
  // Called from the processing thread, at most once every few buffers.
  void Publish(const uint8_t* wire_data, size_t byte_count);

  // Take the most recent snapshot, if one has arrived since the last call.
  // Returns false when there is nothing new, which is the ordinary case for a
  // display refreshing faster than snapshots are published.
  bool TryRead(std::vector<uint8_t>& out, uint64_t& generation);

  // Snapshots published in total. A consumer that falls behind sees this jump
  // by more than one, which is how it knows it dropped some — and dropping them
  // is correct, because an old snapshot of a live signal is of no interest.
  uint64_t Generation() const { return generation_.load(); }

  size_t snapshot_bytes() const { return snapshot_bytes_; }

 private:
  struct Buffer {
    std::vector<uint8_t> data;
    size_t used = 0;
    uint64_t generation = 0;
  };

  // The index of the buffer holding the newest complete snapshot, with the top
  // bit set when it has not been read yet. Packed into one atomic because the
  // handover has to be a single indivisible step: an index and a flag in two
  // atomics could be read half-updated.
  static constexpr unsigned kFreshFlag = 0x4;
  static constexpr unsigned kIndexMask = 0x3;

  size_t snapshot_bytes_;
  Buffer buffers_[3];
  unsigned write_index_ = 0;
  unsigned read_index_ = 1;
  std::atomic<unsigned> ready_{2};
  std::atomic<uint64_t> generation_{0};
};

}  // namespace ddd::capture
