/************************************************************************

    capture_pipeline.cpp

    The orchestrator: threads, lifetime, and what to do when it goes wrong
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_pipeline.h"

#include <algorithm>
#include <utility>

#include "log_format.h"
#include "logger.h"
#include "sample_format.h"
#include "thread_priority.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

// The back pressure at which a run is worth mentioning in the log, once.
//
// Fifty on this scale is the near-full mark the gateware itself counts against:
// half the room above a packet, which is half of what a stall on this machine
// is paid out of. A capture that reaches it is working, and is closer to not
// working than anybody would guess from the fact that it succeeded.
constexpr int kSqueezedBackPressure = 50;

// How often the control thread looks around. Nothing on a deadline depends on
// this: it is only the resolution of the stall watchdog and of noticing that a
// worker has finished.
constexpr std::chrono::milliseconds kControlPollInterval{50};

}  // namespace

// What the source is allowed to see of the pipeline.
class CapturePipeline::Control : public SourceControl {
 public:
  explicit Control(CapturePipeline* pipeline) : pipeline_(pipeline) {}

  bool StopRequested() const override {
    return pipeline_->stop_requested_.load();
  }

  bool AbortRequested() const override {
    return pipeline_->abort_requested_.load();
  }

  void AddCompletedTransfers(uint64_t count) override {
    pipeline_->transfers_completed_.fetch_add(count);
  }

  void Log(const std::string& message) override {
    if (pipeline_->logger_ != nullptr) {
      pipeline_->logger_->Info(message);
    }
  }

 private:
  CapturePipeline* pipeline_;
};

CapturePipeline::CapturePipeline(ILogger* logger)
    : logger_(logger),
      snapshots_(std::make_unique<SnapshotPublisher>(
          SnapshotPublisher::kDefaultSnapshotBytes)) {}

CapturePipeline::~CapturePipeline() {
  Abort();
  Wait();
}

bool CapturePipeline::Start(ISampleSource* source,
                            std::unique_ptr<ISampleSink> sink,
                            const Options& options) {
  if (running_.load()) {
    LatchResult(TransferResult::kProgramError,
                "A capture is already running on this pipeline");
    return false;
  }

  // Everything a previous run left behind, so that a pipeline can be reused.
  options_ = options;
  source_ = source;
  sink_ = std::move(sink);
  validator_.Reset();
  metrics_.Reset();
  test_pattern_verifier_ = TestPatternVerifier{};
  test_pattern_result_ = TestPatternVerifier::Result{};
  test_pattern_checked_ = false;
  throughput_anchored_ = false;
  throughput_anchor_buffers_ = 0;
  throughput_anchor_seconds_ = 0.0;
  throughput_window_buffers_ = 0;
  throughput_window_seconds_ = 0.0;
  throughput_bytes_per_second_ = 0.0;
  device_buffer_seen_ = false;
  device_buffer_squeezed_ = false;
  device_buffer_latch_ = 0;
  peak_back_pressure_percent_ = 0;
  peak_device_buffer_words_ = 0;
  device_overflow_events_ = 0;
  device_dropped_words_ = 0;
  device_near_full_units_ = 0;
  ring_fill_.Reset();
  device_back_pressure_.Reset();
  capture_span_open_ = false;
  capture_span_start_buffer_ = 0;
  transfers_completed_ = 0;
  buffers_processed_ = 0;
  sink_change_requests_ = 0;
  sink_change_count_ = 0;
  last_sink_change_buffer_ = 0;
  pending_sink_.store(nullptr);
  pending_detach_.store(false);
  stop_requested_ = false;
  abort_requested_ = false;
  transfer_finished_ = false;
  processing_finished_ = false;
  result_ = TransferResult::kRunning;
  result_latched_ = false;
  {
    const std::lock_guard<std::mutex> guard(detail_mutex_);
    result_detail_.clear();
  }

  snapshots_ = std::make_unique<SnapshotPublisher>(options_.snapshot_bytes);

  ring_ = std::make_unique<DiskBufferRing>(
      source_->PlanGeometry(options_.queue_size_bytes));

  if (options_.lock_memory) {
    const std::string failure = ring_->LockIntoMemory();
    if (failure.empty() && logger_ != nullptr) {
      // Worth a line because its absence is worth a line: a log that says the
      // buffers were locked rules out the whole class of fault that an
      // unlocked ring can produce, and a log that says neither leaves a
      // developer guessing which build it was.
      logger_->Debug("The capture buffers are locked into memory.");
    }
    if (!failure.empty() && logger_ != nullptr) {
      // Degradation, not failure. An unlocked capture is more exposed to a page
      // fault at the wrong moment; it is not impossible, and refusing to run
      // would help nobody.
      logger_->Warning(
          "The capture buffers could not be locked into memory, so this "
          "capture is more exposed to interruption: " +
          failure);
    }
  }

  const TransferResult prepared = source_->Prepare(*ring_);
  if (prepared != TransferResult::kSuccess) {
    LatchResult(prepared, std::string("The ") + source_->Name() +
                              " source could not be prepared");
    source_->Finish();
    ring_.reset();
    sink_.reset();
    return false;
  }

  if (logger_ != nullptr) {
    logger_->Info("Starting: source " + std::string(source_->Name()) +
                  ", sink " + std::string(sink_ ? sink_->Name() : "none") +
                  ", " + std::to_string(ring_->slot_count()) + " buffers of " +
                  std::to_string(ring_->slot_size_bytes() / 1024) + " KiB");
  }

  LogStartDetail();

  start_time_ = std::chrono::steady_clock::now();
  running_ = true;

  control_thread_ = std::thread(&CapturePipeline::ControlThread, this);
  return true;
}

void CapturePipeline::RequestStop() {
  stop_requested_ = true;
  control_signal_.notify_all();
}

void CapturePipeline::Abort() {
  if (!running_.load()) {
    return;
  }
  abort_requested_ = true;
  stop_requested_ = true;
  if (ring_ != nullptr) {
    ring_->Abort();
  }
  control_signal_.notify_all();
}

void CapturePipeline::Wait() {
  if (control_thread_.joinable()) {
    control_thread_.join();
  }
}

std::string CapturePipeline::ResultDetail() const {
  const std::lock_guard<std::mutex> guard(detail_mutex_);
  return result_detail_;
}

void CapturePipeline::LatchResult(TransferResult result,
                                  const std::string& detail) {
  bool expected = false;
  if (!result_latched_.compare_exchange_strong(expected, true)) {
    return;
  }

  result_ = result;
  {
    const std::lock_guard<std::mutex> guard(detail_mutex_);
    result_detail_ = detail;
  }

  if (logger_ != nullptr && TransferFailed(result)) {
    logger_->Error(std::string("Capture failed (") +
                   TransferResultName(result) + "): " + detail);
  }
}

uint64_t CapturePipeline::AttachSink(std::unique_ptr<ISampleSink> sink) {
  // The processing thread takes ownership of the raw pointer through the
  // atomic. If a previous request has not been picked up yet, this replaces it
  // and the superseded sink is destroyed here rather than leaked — a caller
  // that attaches twice in one buffer period gets the second one, which is what
  // they asked for.
  ISampleSink* const superseded = pending_sink_.exchange(sink.release());
  delete superseded;

  pending_detach_.store(false);
  return sink_change_requests_.fetch_add(1) + 1;
}

uint64_t CapturePipeline::DetachSink() {
  ISampleSink* const superseded = pending_sink_.exchange(nullptr);
  delete superseded;

  pending_detach_.store(true);
  return sink_change_requests_.fetch_add(1) + 1;
}

std::unique_ptr<ISampleSink> CapturePipeline::TakeRetiredSink() {
  const std::lock_guard<std::mutex> guard(retired_sink_mutex_);
  return std::move(retired_sink_);
}

// The wire rate the configured sample rate implies, in bytes per second. What a
// measured throughput is compared against.
double CapturePipeline::ExpectedBytesPerSecond() const {
  return static_cast<double>(options_.sample_rate_hz) *
         static_cast<double>(kBytesPerSample);
}

void CapturePipeline::LogStartDetail() {
  if (logger_ == nullptr || ring_ == nullptr) {
    return;
  }

  const uint64_t ring_bytes =
      static_cast<uint64_t>(ring_->slot_count()) * ring_->slot_size_bytes();
  const double expected = ExpectedBytesPerSecond();

  // What the ring is, in the terms that decide whether it is big enough: not a
  // size but a length of time, because what a ring buys is how long the
  // consumer may be held up before a sample is lost.
  logger_->Debug(
      "Ring: " + FormatBytes(options_.queue_size_bytes) + " asked for, " +
      std::to_string(ring_->slot_count()) + " slots of " +
      FormatBytes(ring_->slot_size_bytes()) + " = " + FormatBytes(ring_bytes) +
      ", which is " +
      FormatDuration(expected > 0.0 ? static_cast<double>(ring_bytes) / expected
                                    : 0.0) +
      " of headroom at " +
      FormatDecimal(static_cast<double>(options_.sample_rate_hz) / 1.0e6, 1) +
      " Msps (" + FormatDecimal(expected / 1.0e6, 1) + " MB/s)");

  logger_->Debug(
      std::string("Options: test mode ") + (options_.test_mode ? "on" : "off") +
      ", memory locking " + (options_.lock_memory ? "on" : "off") +
      ", priority elevation " + (options_.elevate_priority ? "on" : "off") +
      ", stall timeout " + std::to_string(options_.stall_timeout.count()) +
      " ms, snapshot every " +
      std::to_string(options_.snapshot_interval_buffers) + " buffers of " +
      FormatBytes(options_.snapshot_bytes) + ", throughput window " +
      std::to_string(options_.throughput_window.count()) +
      " ms, progress every " +
      std::to_string(options_.progress_log_interval.count()) + " ms");
}

void CapturePipeline::LogProgress() {
  if (logger_ == nullptr) {
    return;
  }

  // Read through the publisher rather than off the members: this runs on the
  // control thread, and everything it wants belongs to the processing thread.
  // The published block is the one place the two are allowed to meet.
  const CaptureStats stats = stats_.Read();
  const double expected = ExpectedBytesPerSecond();

  std::string line =
      std::string(stats.writing ? "Capturing" : "Streaming") + " for " +
      FormatDuration(stats.elapsed_seconds) + ": " +
      std::to_string(stats.buffers_processed) + " buffers, " +
      FormatDecimal(stats.throughput_bytes_per_second / 1.0e6, 1) + " MB/s";

  // The rate on its own says nothing without what it should have been. A
  // reading below the wire rate for more than a moment is either a device that
  // is not delivering or a host that is not keeping up, and the ring depth
  // beside it is what separates the two.
  if (expected > 0.0 && stats.throughput_bytes_per_second > 0.0) {
    line +=
        " (" +
        FormatDecimal(100.0 * stats.throughput_bytes_per_second / expected, 1) +
        "% of the wire rate)";
  }

  line += ", ring " + std::to_string(stats.slots_in_use) + "/" +
          std::to_string(stats.slot_count) + " slots, peak " +
          std::to_string(stats.peak_slots_in_use);

  if (stats.device_buffer.present) {
    line += "; device buffer " +
            std::to_string(stats.device_buffer.BackPressurePercent()) +
            "% back pressure, peak " +
            std::to_string(stats.peak_back_pressure_percent) + "%";
    if (stats.device_dropped_words > 0) {
      line += ", " + std::to_string(stats.device_dropped_words) +
              " samples lost so far";
    }
  }

  if (stats.writing) {
    line +=
        "; written " + FormatBytes(stats.bytes_written) + " (" +
        FormatSampleDuration(stats.samples_written, options_.sample_rate_hz) +
        " of stream)";
  }

  logger_->Debug(line);
}

void CapturePipeline::LogStopDetail() {
  if (logger_ == nullptr) {
    return;
  }

  const CaptureStats stats = stats_.Read();
  const uint64_t slot_bytes = (ring_ != nullptr) ? ring_->slot_size_bytes() : 0;
  const uint64_t processed_bytes = stats.buffers_processed * slot_bytes;
  const double expected = ExpectedBytesPerSecond();

  // What went through, and what it cost. The transfer count beside the buffer
  // count is the one pairing that shows work in flight at the moment of the
  // stop, which is where a run that ends early loses its last buffers.
  std::string account = "Run: " + FormatDuration(stats.elapsed_seconds) + ", " +
                        std::to_string(stats.transfers_completed) +
                        " transfers, " +
                        std::to_string(stats.buffers_processed) +
                        " buffers = " + FormatBytes(processed_bytes) + " = " +
                        FormatSampleDuration(stats.metrics.sample_count,
                                             options_.sample_rate_hz) +
                        " of stream";
  if (stats.elapsed_seconds > 0.0) {
    const double average =
        static_cast<double>(processed_bytes) / stats.elapsed_seconds;
    account += ", averaging " + FormatDecimal(average / 1.0e6, 1) + " MB/s";
    if (expected > 0.0) {
      // A healthy run reads a little under 100: the opening slots a source
      // discards and anything still in flight when it stopped are both charged
      // against the elapsed time this is divided by.
      account += " (" + FormatDecimal(100.0 * average / expected, 1) +
                 "% of the wire rate, counting the discarded opening slots and "
                 "anything still in flight)";
    }
  }
  logger_->Debug(account);

  if (ring_ != nullptr) {
    logger_->Debug("Ring depth: " + ring_fill_.Describe() + "; peak " +
                   std::to_string(stats.peak_slots_in_use) + " of " +
                   std::to_string(stats.slot_count) + " slots, " +
                   std::to_string(ring_->SlotsFilled()) + " filled, " +
                   std::to_string(ring_->SlotsFreed()) + " freed");
  }

  if (device_buffer_seen_) {
    // The near-full counter is the device's own answer to "for how long", and
    // it is the figure that survives a run whose peak was unremarkable because
    // nobody was reading at the moment it happened.
    const double near_full_seconds =
        options_.sample_rate_hz == 0
            ? 0.0
            : static_cast<double>(device_near_full_units_) *
                  kTelemetryNearFullPrescale /
                  static_cast<double>(options_.sample_rate_hz);

    logger_->Debug("Device back pressure: " + device_back_pressure_.Describe() +
                   "; at or above the gateware's near-full mark for " +
                   FormatDuration(near_full_seconds) + ", peak " +
                   std::to_string(stats.peak_device_buffer_words) + " words");
  }

  // The signal itself, over everything the run saw. Not the file's own figures
  // — those are measured across the span the file covers and reported when it
  // closes — but the session's, which is what says whether the input was
  // sensible at all.
  logger_->Debug(
      "Signal over the run: " + std::to_string(stats.metrics.sample_count) +
      " samples, range " + std::to_string(stats.metrics.minimum_value) + "-" +
      std::to_string(stats.metrics.maximum_value) + " of 1023, RMS " +
      FormatDecimal(stats.metrics.rms, 1) + ", clipped low " +
      std::to_string(stats.metrics.clipped_low_count) + " high " +
      std::to_string(stats.metrics.clipped_high_count) + "; sequence " +
      SequenceStateName(stats.sequence_state));

  if (stats.test_pattern_checked) {
    logger_->Debug(
        std::string("Test pattern: ") +
        (stats.test_pattern_passed ? "intact after " : "broke after ") +
        std::to_string(test_pattern_result_.samples_checked) +
        " samples checked");
  }
}

void CapturePipeline::PerformPendingSinkChange() {
  // Read before the exchanges below, so that a request arriving while this is
  // running is not claimed as applied — it will be picked up at the next buffer
  // and counted then.
  const uint64_t requests_seen = sink_change_requests_.load();

  ISampleSink* const incoming = pending_sink_.exchange(nullptr);
  const bool detaching = pending_detach_.exchange(false);

  if (incoming == nullptr && !detaching) {
    return;
  }

  std::unique_ptr<ISampleSink> replacement(incoming);
  if (replacement == nullptr) {
    replacement = std::make_unique<NullSink>();
  }

  // What the outgoing sink took, gathered before it is finished and handed
  // away. Only used for the line logged below.
  const bool closing_a_file = sink_ != nullptr && sink_->StoresData();
  std::string closing_name;
  uint64_t closing_bytes = 0;
  uint64_t closing_samples = 0;
  double finish_seconds = 0.0;

  if (sink_ != nullptr) {
    closing_name = sink_->Name();

    // Finishing a FLAC file writes its last frame and patches the header, and
    // that is not instant. It happens here, on the processing thread, between
    // two buffers — the ring absorbs it, which is exactly what the ring is for,
    // and doing it anywhere else would mean a file that is closed while data is
    // still arriving for it.
    //
    // Timed because that is the claim: the pause is meant to be short against
    // the ring's headroom, and a log that carries both figures is how anybody
    // checks it on a machine that is not this one.
    const auto finish_started = std::chrono::steady_clock::now();
    if (!sink_->Finish()) {
      LatchResult(TransferResult::kFileWriteError, sink_->LastError());
    }
    finish_seconds = std::chrono::duration<double>(
                         std::chrono::steady_clock::now() - finish_started)
                         .count();

    closing_bytes = sink_->BytesWritten();
    closing_samples = sink_->SamplesWritten();

    const std::lock_guard<std::mutex> guard(retired_sink_mutex_);
    retired_sink_ = std::move(sink_);
  }

  sink_ = std::move(replacement);
  last_sink_change_buffer_ = buffers_processed_.load();

  // Measure the recording separately from the session it sits in. The swap
  // happens between two buffers, so the span this opens and closes holds
  // exactly the samples that reached the file — which is what a file's own
  // metadata has to describe, and not the minute of setting up before it.
  if (sink_->StoresData()) {
    metrics_.BeginCaptureSpan();
  } else {
    metrics_.EndCaptureSpan();
  }

  // Set to the number of requests this swap accounted for, rather than
  // incremented by one.
  //
  // Two requests can collapse into one swap: a capture that is stopped inside a
  // single buffer period leaves an attach and a detach outstanding together,
  // and DetachSink discards the attach that never happened. Counting swaps then
  // leaves the count one short of the request the caller is waiting on for ever
  // — which meant a capture started and stopped in the same instant never
  // reported that it had finished, and so never had its metadata written.
  //
  // The swap itself is unchanged by this; only what is counted is.
  sink_change_count_.store(requests_seen);

  const uint64_t buffer_now = buffers_processed_.load();

  if (logger_ != nullptr) {
    logger_->Info(std::string("Sink changed to ") + sink_->Name() +
                  " at buffer " + std::to_string(buffer_now));
  }

  // The two lines a developer wants around a file: what the ring looked like at
  // the moment it was swapped, and what the file that just closed actually got.
  if (logger_ != nullptr && ring_ != nullptr) {
    logger_->Debug("Sink change at buffer " + std::to_string(buffer_now) +
                   ": ring holding " + std::to_string(ring_->SlotsInUse()) +
                   " of " + std::to_string(ring_->slot_count()) +
                   " slots, peak so far " +
                   std::to_string(ring_->PeakSlotsInUse()));
  }

  if (logger_ != nullptr && closing_a_file) {
    const double span_seconds =
        capture_span_open_
            ? std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                            capture_span_start_time_)
                  .count()
            : 0.0;
    const uint64_t span_buffers =
        capture_span_open_ ? buffer_now - capture_span_start_buffer_ : 0;

    // The three figures that describe a recording: how much reached it, how
    // long that is, and how big it came out — with the ratio between the last
    // two, which is the compression a FLAC capture achieved and the one number
    // that says whether the encoder was doing its job.
    const uint64_t raw_bytes = closing_samples * kBytesPerSample;
    std::string line =
        "Closed " + closing_name + " after " + std::to_string(span_buffers) +
        " buffers over " + FormatDuration(span_seconds) + ": " +
        std::to_string(closing_samples) + " samples = " +
        FormatSampleDuration(closing_samples, options_.sample_rate_hz) +
        " of stream, " + FormatBytes(closing_bytes);
    if (raw_bytes > 0) {
      line += " on disk (" +
              FormatDecimal(100.0 * static_cast<double>(closing_bytes) /
                                static_cast<double>(raw_bytes),
                            1) +
              "% of the " + FormatBytes(raw_bytes) + " that arrived)";
    }
    line += ", finishing took " + FormatDuration(finish_seconds);
    logger_->Debug(line);
  }

  if (sink_->StoresData()) {
    capture_span_open_ = true;
    capture_span_start_buffer_ = buffer_now;
    capture_span_start_time_ = std::chrono::steady_clock::now();
  } else {
    capture_span_open_ = false;
  }
}

double CapturePipeline::MeasureThroughput(uint64_t buffers_processed,
                                          double elapsed_seconds) {
  const size_t slot_bytes = (ring_ != nullptr) ? ring_->slot_size_bytes() : 0;
  if (slot_bytes == 0) {
    return 0.0;
  }

  // Measuring starts at the first buffer, not at the start of the run.
  // Everything before it — the threads starting, the transfers being submitted,
  // the device's opening slots being discarded — is time in which nothing this
  // counts could have arrived, and charging the rate for it is exactly what
  // made the old figure read low.
  if (!throughput_anchored_) {
    if (buffers_processed == 0) {
      return 0.0;
    }
    throughput_anchored_ = true;
    throughput_anchor_buffers_ = buffers_processed;
    throughput_anchor_seconds_ = elapsed_seconds;
    throughput_window_buffers_ = buffers_processed;
    throughput_window_seconds_ = elapsed_seconds;
    return 0.0;
  }

  const double window =
      std::chrono::duration<double>(options_.throughput_window).count();

  if (elapsed_seconds - throughput_anchor_seconds_ >= window) {
    throughput_window_buffers_ = throughput_anchor_buffers_;
    throughput_window_seconds_ = throughput_anchor_seconds_;
    throughput_anchor_buffers_ = buffers_processed;
    throughput_anchor_seconds_ = elapsed_seconds;
  }

  const double span = elapsed_seconds - throughput_window_seconds_;
  if (span < window) {
    // Nothing worth publishing yet. Zero is how the panel is told there is no
    // reading, which is the honest thing to show for the first second of a
    // capture — better than a figure taken across two buffers and wrong by
    // whatever the scheduler happened to do to them.
    return 0.0;
  }

  return static_cast<double>((buffers_processed - throughput_window_buffers_) *
                             slot_bytes) /
         span;
}

void CapturePipeline::PublishStats() {
  CaptureStats stats;
  stats.result = result_.load();

  const auto elapsed = std::chrono::steady_clock::now() - start_time_;
  stats.elapsed_seconds = std::chrono::duration<double>(elapsed).count();

  stats.transfers_completed = transfers_completed_.load();
  stats.buffers_processed = buffers_processed_.load();
  stats.bytes_written = (sink_ != nullptr) ? sink_->BytesWritten() : 0;
  stats.samples_written = (sink_ != nullptr) ? sink_->SamplesWritten() : 0;
  stats.samples_pending = (sink_ != nullptr) ? sink_->SamplesPending() : 0;
  stats.writing = (sink_ != nullptr) && sink_->StoresData();

  // A run that has stopped keeps the last rate it measured. The clock carries
  // on through the join and the encoder's final frame while no further buffer
  // can arrive, so measuring across that would end every capture by reporting a
  // fraction of the rate it actually ran at.
  if (stats.result == TransferResult::kRunning) {
    throughput_bytes_per_second_ =
        MeasureThroughput(stats.buffers_processed, stats.elapsed_seconds);
  }
  stats.throughput_bytes_per_second = throughput_bytes_per_second_;

  if (ring_ != nullptr) {
    stats.slots_in_use = ring_->SlotsInUse();
    stats.peak_slots_in_use = ring_->PeakSlotsInUse();
    stats.slot_count = ring_->slot_count();
  }

  stats.sequence_state = validator_.state();
  stats.test_pattern_checked = test_pattern_checked_;
  stats.test_pattern_passed = !test_pattern_verifier_.HasFailed();
  stats.metrics = metrics_.Snapshot();

  // The device's account of its own capture buffer, and the totals built from
  // it.
  //
  // The device counts per interval and clears when it is read, so a total
  // exists only here. This runs far more often than the source takes a reading,
  // which is why the totals are accumulated only when the latch count moves:
  // adding the same reading in on every publication would multiply every figure
  // by however many buffers went by while it stood still.
  const FpgaTelemetry telemetry =
      (source_ != nullptr) ? source_->DeviceTelemetry() : FpgaTelemetry{};

  if (telemetry.present &&
      (!device_buffer_seen_ || telemetry.latch_count != device_buffer_latch_)) {
    device_buffer_seen_ = true;
    device_buffer_latch_ = telemetry.latch_count;

    device_overflow_events_ += telemetry.overflow_events;
    device_dropped_words_ += telemetry.dropped_words;
    peak_device_buffer_words_ =
        std::max(peak_device_buffer_words_, telemetry.peak);
    peak_back_pressure_percent_ =
        std::max(peak_back_pressure_percent_, telemetry.BackPressurePercent());
    device_back_pressure_.AddPercent(telemetry.BackPressurePercent());
    device_near_full_units_ += telemetry.near_full_units;

    if (logger_ != nullptr) {
      // Per reading rather than per interval of trouble, which bounds this at
      // the rate the source polls — a few lines a second at worst, and only
      // while samples are actually being lost.
      if (telemetry.overflow_events > 0) {
        logger_->Warning(
            "The device's capture buffer overflowed: " +
            std::to_string(telemetry.overflow_events) + " stalls, " +
            std::to_string(telemetry.dropped_words) +
            " samples lost. The host is not taking packets fast enough.");
      } else if (!device_buffer_squeezed_ &&
                 telemetry.BackPressurePercent() >= kSqueezedBackPressure) {
        // Once per run: this is a warning that a capture is running closer to
        // the edge than it should, and repeating it would say nothing new.
        device_buffer_squeezed_ = true;
        logger_->Info("The device's capture buffer reached " +
                      std::to_string(telemetry.PeakPercentOfDepth()) +
                      "% — over half the room a stall is paid out of");
      }
    }
  }

  stats.device_buffer = telemetry;
  stats.peak_back_pressure_percent = peak_back_pressure_percent_;
  stats.peak_device_buffer_words = peak_device_buffer_words_;
  stats.device_overflow_events = device_overflow_events_;
  stats.device_dropped_words = device_dropped_words_;

  stats_.Publish(stats);
}

void CapturePipeline::TransferThread() {
  std::unique_ptr<ScopedThreadPriority> priority;
  if (options_.elevate_priority) {
    priority = std::make_unique<ScopedThreadPriority>();
    if (logger_ != nullptr) {
      logger_->Debug("Transfer thread: " + priority->message());
    }
  }

  Control control(this);
  const TransferResult result = source_->Run(*ring_, control);

  if (TransferFailed(result)) {
    LatchResult(result,
                std::string("The ") + source_->Name() +
                    " source stopped: " + TransferResultDescription(result));
  }

  transfer_finished_ = true;
  control_signal_.notify_all();
}

void CapturePipeline::ProcessingThread() {
  std::unique_ptr<ScopedThreadPriority> priority;
  if (options_.elevate_priority) {
    priority = std::make_unique<ScopedThreadPriority>();
    if (logger_ != nullptr) {
      logger_->Debug("Processing thread: " + priority->message());
    }
  }

  const size_t slot_bytes = ring_->slot_size_bytes();
  const size_t samples_per_slot = slot_bytes / kBytesPerSample;
  size_t slot_index = 0;
  uint64_t buffers_since_snapshot = 0;

  while (true) {
    if (abort_requested_.load()) {
      break;
    }

    // A sink change is applied between buffers and nowhere else, which is what
    // makes "start recording" lose nothing: the boundary is a place where no
    // sample is half-written.
    PerformPendingSinkChange();

    if (!ring_->WaitForSlotFull(slot_index)) {
      // Woken by a dump rather than by data. Either the capture is stopping
      // gracefully and there is nothing more coming, or it is being torn down.
      break;
    }

    // Sampled here rather than after the slot is freed, because this is the
    // backlog the consumer actually faced: the depth on the way in is how far
    // behind the producer this thread was when it started work, and the depth
    // on the way out is that minus the one slot it has just returned.
    //
    // Two atomic loads per two-megabyte buffer, which is nothing beside the
    // pass over the buffer itself.
    ring_fill_.Add(ring_->SlotsInUse(), ring_->slot_count());

    uint8_t* const data = ring_->SlotData(slot_index);

    const SequenceValidator::Outcome outcome =
        validator_.Process(data, slot_bytes);
    metrics_.Accumulate(outcome.tally);

    if (!outcome.ok) {
      // The counters alone say a capture is not bit-perfect. What follows
      // them says where to look: how far short of a full counter period the
      // run ended, and — when the lock was taken in this same buffer — where
      // it was taken and what the buffer opened with. A lock at sample 1 is
      // the buffer head disagreeing with itself rather than the stream having
      // a hole in it, and the two have nothing in common but the message.
      std::string detail =
          "Sequence counter mismatch " +
          std::to_string(outcome.mismatch_sample_index) +
          " samples into buffer " + std::to_string(buffers_processed_.load()) +
          ": expected " + std::to_string(outcome.expected_counter) + ", got " +
          std::to_string(outcome.actual_counter) + ". The counter advanced " +
          std::to_string(outcome.samples_expected_remaining) + " samples early";
      if (outcome.synchronised_here) {
        detail += ", and the validator locked on at sample " +
                  std::to_string(outcome.synchronisation_sample_index) +
                  " of this buffer, which opened on counter " +
                  std::to_string(outcome.first_counter);
      }
      LatchResult(TransferResult::kSequenceMismatch, detail);
      ring_->MarkSlotFree(slot_index);
      break;
    }

    if (options_.test_mode) {
      test_pattern_checked_ = true;
      if (!test_pattern_verifier_.FeedWireBytes(data, slot_bytes)) {
        const TestPatternVerifier::Result& verdict =
            test_pattern_verifier_.GetResult();
        LatchResult(TransferResult::kVerificationError,
                    "The device's test ramp broke after " +
                        std::to_string(verdict.samples_checked) +
                        " samples: expected " +
                        std::to_string(verdict.expected_value) + ", got " +
                        std::to_string(verdict.actual_value));
        ring_->MarkSlotFree(slot_index);
        break;
      }
    }

    ++buffers_since_snapshot;
    if (buffers_since_snapshot >= options_.snapshot_interval_buffers) {
      snapshots_->Publish(data, slot_bytes);
      buffers_since_snapshot = 0;
    }

    if (sink_ != nullptr && !sink_->Write(data, samples_per_slot)) {
      LatchResult(TransferResult::kFileWriteError, sink_->LastError());
      ring_->MarkSlotFree(slot_index);
      break;
    }

    ring_->MarkSlotFree(slot_index);
    buffers_processed_.fetch_add(1);

    PublishStats();

    slot_index = (slot_index + 1) % ring_->slot_count();
  }

  if (options_.test_mode) {
    test_pattern_result_ = test_pattern_verifier_.GetResult();
  }

  PublishStats();

  processing_finished_ = true;
  control_signal_.notify_all();
}

void CapturePipeline::ControlThread() {
  transfer_thread_ = std::thread(&CapturePipeline::TransferThread, this);
  processing_thread_ = std::thread(&CapturePipeline::ProcessingThread, this);

  auto last_progress_time = std::chrono::steady_clock::now();
  auto last_progress_log = last_progress_time;
  uint64_t last_transfer_count = 0;

  while (!processing_finished_.load()) {
    {
      std::unique_lock<std::mutex> lock(control_mutex_);
      control_signal_.wait_for(lock, kControlPollInterval);
    }

    // A line every so often while the run is going, at debug level. This
    // thread is the right one to do it from: it is already awake on a timer,
    // nothing waits on it, and the figures it wants are in the published
    // statistics block rather than on the processing thread's own state.
    if (options_.progress_log_interval.count() > 0) {
      const auto progress_now = std::chrono::steady_clock::now();
      if (progress_now - last_progress_log >= options_.progress_log_interval) {
        last_progress_log = progress_now;
        LogProgress();
      }
    }

    // The stall watchdog. A device that stops delivering without ever failing a
    // transfer is invisible to every other check here: no error is raised, no
    // thread returns, and the application simply waits. That is the failure
    // mode this exists for, and it is the reason a capture can no longer hang.
    const uint64_t transfers = transfers_completed_.load();
    const auto now = std::chrono::steady_clock::now();
    if (transfers != last_transfer_count) {
      last_transfer_count = transfers;
      last_progress_time = now;
    } else if (!transfer_finished_.load() && !stop_requested_.load() &&
               (now - last_progress_time) > options_.stall_timeout) {
      LatchResult(TransferResult::kSourceStalled,
                  "Nothing arrived from the " + std::string(source_->Name()) +
                      " source for " +
                      std::to_string(options_.stall_timeout.count()) + " ms");
      abort_requested_ = true;
      ring_->Abort();
    }

    // The transfer side has finished, one way or another. Release the
    // processing side from any slot it is waiting on that will now never be
    // filled.
    //
    // Every iteration, not once. The processing thread is still draining, and
    // each buffer it finishes returns a slot to the empty state — so a slot
    // padded a moment ago can be empty again by the time the consumer wraps
    // round to it, and it would then wait on a producer that has already gone.
    // Marking once is a deadlock that only appears when the consumer happens to
    // be a full lap behind at the moment the producer stops, which is to say
    // occasionally, and never on the machine where it was written.
    if (transfer_finished_.load()) {
      ring_->MarkEmptySlotsDumped();
    }

    // Something failed. Stop everything now rather than draining data that is
    // already known to be wrong.
    if (result_latched_.load() && TransferFailed(result_.load())) {
      abort_requested_ = true;
      ring_->Abort();
    }
  }

  // The processing side has stopped, so nothing will empty the ring again. If
  // the transfer side is still filling it, it must be released or it will wait
  // on a slot forever.
  if (!transfer_finished_.load()) {
    stop_requested_ = true;
    ring_->Abort();
  }

  if (transfer_thread_.joinable()) {
    transfer_thread_.join();
  }
  if (processing_thread_.joinable()) {
    processing_thread_.join();
  }

  source_->Finish();

  // Close the file last, and only after both workers have stopped, so nothing
  // can be mid-write while the stream header is being patched.
  if (sink_ != nullptr && !sink_->Finish()) {
    LatchResult(TransferResult::kFileWriteError, sink_->LastError());
  }

  if (!result_latched_.load()) {
    result_ = TransferResult::kSuccess;
  }

  PublishStats();
  running_ = false;

  if (logger_ != nullptr) {
    const CaptureStats final_stats = stats_.Read();
    logger_->Info(
        "Stopped (" + std::string(TransferResultName(result_.load())) +
        "): " + std::to_string(final_stats.buffers_processed) + " buffers, " +
        std::to_string(final_stats.metrics.sample_count) +
        " samples, peak queue depth " +
        std::to_string(final_stats.peak_slots_in_use) + " of " +
        std::to_string(final_stats.slot_count));

    // The device's half of the same account. Worth a line of its own at the end
    // of every run, because it is the figure that says whether a capture that
    // succeeded was ever in danger — and the one nobody thinks to look for
    // until a later capture fails.
    if (device_buffer_seen_) {
      logger_->Info(
          "Device buffer: peak back pressure " +
          std::to_string(final_stats.peak_back_pressure_percent) + "%, peak " +
          std::to_string(final_stats.peak_device_buffer_words) + " words, " +
          std::to_string(final_stats.device_overflow_events) + " overflows, " +
          std::to_string(final_stats.device_dropped_words) +
          " samples dropped");
    }

    // And the developer's account of the same run, which is several lines and
    // is why it is behind a level. Everything it reads belongs to the
    // processing thread, and both workers were joined above — that join is what
    // makes reading it here safe rather than a race.
    LogStopDetail();
  }
}

}  // namespace ddd::capture
