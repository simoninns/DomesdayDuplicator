/************************************************************************

    capture_pipeline.h

    The orchestrator: threads, lifetime, and what to do when it goes wrong
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "disk_buffer_ring.h"
#include "monitor_tap.h"
#include "sample_metrics.h"
#include "sample_sink.h"
#include "sample_source.h"
#include "sequence_validator.h"
#include "test_pattern_verifier.h"
#include "transfer_result.h"

namespace ddd::capture {

class ILogger;

// Runs a capture: three threads, one ring, one sink at a time.
//
//   control     owns the lifetime. Starts the others, watches for a stall,
//               latches the first error, sequences the shutdown, joins.
//   transfer    the source's thread. Fills ring slots and marks them full.
//   processing  validates, measures, publishes to the monitor tap, and writes
//               to whatever sink is attached.
//
// Monitor mode and capture mode are the same pipeline. Monitoring runs with a
// NullSink; starting a capture attaches a FlacSink at the next slot boundary
// without interrupting the stream, and stopping it detaches and finalises the
// file the same way. The device is never told anything — the current gateware
// samples continuously from the moment it is opened, so there is nothing to
// start or stop out there.
//
// Thread-safety: Start, RequestStop, Abort, Wait, AttachSink and DetachSink may
// be called from one controlling thread — in the application, the GUI thread.
// The observers are safe from anywhere. Nothing a caller does can make the
// processing thread wait: sink changes are handed over through an atomic, and
// statistics leave through the wait-free publishers in monitor_tap.h.
class CapturePipeline {
 public:
  struct Options {
    // Total ring size. Clamped to the range in DiskBufferRing.
    size_t queue_size_bytes = DiskBufferRing::kDefaultQueueSizeBytes;

    // Check the device's test-pattern ramp as well as the sequence markers.
    // Only meaningful when the device has been put into test mode.
    bool test_mode = false;

    // Pin the ring into physical memory. Off only for tests that would
    // otherwise need a raised locked-memory limit to run.
    bool lock_memory = true;

    // Raise the worker threads' scheduling priority.
    bool elevate_priority = true;

    // How long the source may deliver nothing before the capture is declared
    // stalled.
    //
    // Generous on purpose. At full rate a slot arrives every 26 ms, so five
    // seconds is around two hundred slots of silence — far outside anything a
    // working device does, and far inside the point at which a user would
    // conclude the application had hung. The old engine had no such check and
    // would wait forever.
    std::chrono::milliseconds stall_timeout{5000};

    // Publish a raw-sample snapshot every this many buffers. At full rate a
    // buffer is 26 ms, so four is about 9 Hz — ahead of what a display can use
    // and well behind what would cost anything.
    uint64_t snapshot_interval_buffers = 4;

    size_t snapshot_bytes = SnapshotPublisher::kDefaultSnapshotBytes;
  };

  explicit CapturePipeline(ILogger* logger);
  ~CapturePipeline();

  CapturePipeline(const CapturePipeline&) = delete;
  CapturePipeline& operator=(const CapturePipeline&) = delete;
  CapturePipeline(CapturePipeline&&) = delete;
  CapturePipeline& operator=(CapturePipeline&&) = delete;

  // Begin streaming. The source is borrowed for the run and must outlive it;
  // the sink is owned. Returns false if the source could not be prepared, in
  // which case Result() says why and nothing was started.
  bool Start(ISampleSource* source, std::unique_ptr<ISampleSink> sink,
             const Options& options);

  // Stop at the next slot boundary, writing out everything already buffered.
  void RequestStop();

  // Stop now, discarding whatever is in flight.
  void Abort();

  // Block until the capture has stopped and its threads have been joined.
  void Wait();

  bool Running() const { return running_.load(); }

  TransferResult Result() const { return result_.load(); }

  // The message for the user, when Result() is a failure and the specific cause
  // is known more precisely than the code alone conveys.
  std::string ResultDetail() const;

  // --- Sink changes --------------------------------------------------------

  // Replace the current sink at the next slot boundary. The old sink is
  // finished and set aside for TakeRetiredSink().
  //
  // Returns the change number this request will carry, so a caller can wait for
  // SinkChangeCount() to reach it rather than guessing when the swap happened.
  uint64_t AttachSink(std::unique_ptr<ISampleSink> sink);

  // Replace the current sink with a null one — stop writing, keep streaming.
  uint64_t DetachSink();

  // Sink changes completed so far
  uint64_t SinkChangeCount() const { return sink_change_count_.load(); }

  // Buffers processed at the moment of the most recent change. Since a change
  // can only happen between buffers, this is what proves the swap was clean.
  uint64_t LastSinkChangeBuffer() const {
    return last_sink_change_buffer_.load();
  }

  // Take ownership of the sink most recently replaced, if it has not been taken
  // already. This is how a caller gets at a finished file's size and path.
  std::unique_ptr<ISampleSink> TakeRetiredSink();

  // --- Observers -----------------------------------------------------------

  const StatsPublisher& stats() const { return stats_; }
  SnapshotPublisher& snapshots() { return *snapshots_; }

  const DiskBufferRing* ring() const { return ring_.get(); }

  // The verifier's findings, valid once a test-mode capture has stopped
  const TestPatternVerifier::Result& test_pattern_result() const {
    return test_pattern_result_;
  }

 private:
  class Control;

  void ControlThread();
  void TransferThread();
  void ProcessingThread();

  // Record the first failure and leave later ones alone. Which error a user is
  // shown matters: a sequence mismatch that then causes a write failure should
  // report the mismatch, because that is the one that says what went wrong.
  void LatchResult(TransferResult result, const std::string& detail);

  void PerformPendingSinkChange();
  void PublishStats();

  ILogger* logger_ = nullptr;
  Options options_;

  ISampleSource* source_ = nullptr;
  std::unique_ptr<DiskBufferRing> ring_;

  std::unique_ptr<ISampleSink> sink_;
  std::atomic<ISampleSink*> pending_sink_{nullptr};
  std::atomic<bool> pending_detach_{false};
  std::atomic<uint64_t> sink_change_requests_{0};
  std::atomic<uint64_t> sink_change_count_{0};
  std::atomic<uint64_t> last_sink_change_buffer_{0};

  // Held only when a sink is attached, detached or collected — never per
  // buffer. The processing thread touches it once per user action, which is
  // orders of magnitude rarer than the encoder finalisation happening beside
  // it.
  mutable std::mutex retired_sink_mutex_;
  std::unique_ptr<ISampleSink> retired_sink_;

  std::thread control_thread_;
  std::thread transfer_thread_;
  std::thread processing_thread_;

  // Woken whenever a worker finishes or the caller asks for something. The
  // control thread is the only waiter and it is not on any deadline, so a
  // condition variable here costs nothing that matters.
  mutable std::mutex control_mutex_;
  std::condition_variable control_signal_;

  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};
  std::atomic<bool> abort_requested_{false};
  std::atomic<bool> transfer_finished_{false};
  std::atomic<bool> processing_finished_{false};

  std::atomic<TransferResult> result_{TransferResult::kSuccess};
  std::atomic<bool> result_latched_{false};
  mutable std::mutex detail_mutex_;
  std::string result_detail_;

  std::atomic<uint64_t> transfers_completed_{0};
  std::atomic<uint64_t> buffers_processed_{0};

  // Processing-thread state. Touched by that thread alone.
  SequenceValidator validator_;
  SampleMetrics metrics_;
  TestPatternVerifier test_pattern_verifier_;
  TestPatternVerifier::Result test_pattern_result_;
  bool test_pattern_checked_ = false;

  // The device's buffer readings, accumulated across the run. The latch count
  // is what tells one reading from the same reading seen again — the source
  // takes one a few times a second and this thread publishes far more often
  // than that.
  bool device_buffer_seen_ = false;
  bool device_buffer_squeezed_ = false;
  uint8_t device_buffer_latch_ = 0;
  int peak_back_pressure_percent_ = 0;
  uint16_t peak_device_buffer_words_ = 0;
  uint64_t device_overflow_events_ = 0;
  uint64_t device_dropped_words_ = 0;

  StatsPublisher stats_;

  // Behind a pointer because a run may ask for a different snapshot size than
  // the last one did, and a class holding atomics cannot be reassigned.
  std::unique_ptr<SnapshotPublisher> snapshots_;

  std::chrono::steady_clock::time_point start_time_;
};

}  // namespace ddd::capture
