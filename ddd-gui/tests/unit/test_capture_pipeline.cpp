/************************************************************************

    test_capture_pipeline.cpp

    T1 tests for the orchestrator: lifetime, errors, and the sink swap
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "capture_pipeline.h"
#include "logger.h"
#include "recording_sink.h"
#include "synthetic_source.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

using namespace std::chrono_literals;

// Small, fast slots — but not arbitrarily small.
//
// The floor is one sequence-counter period. The validator locks on by finding a
// counter change, and a counter covers 65,536 samples, so a buffer shorter than
// that may contain no change at all and the validator would sit unsynchronised
// for as many buffers as it took for one to land inside one. Real 2 MB buffers
// hold sixteen counter periods and always lock on immediately; a test geometry
// that did not would be exercising a situation the application never
// encounters, and would quietly stop detecting the sequence faults these tests
// inject.
//
// 256 KiB is 131,072 samples: two counter periods, a fiftieth of a real buffer,
// and small enough that a whole pipeline test runs in milliseconds.
constexpr size_t kTestSlotBytes = size_t{256} << 10;
constexpr size_t kTestSlotSamples = kTestSlotBytes / kBytesPerSample;
constexpr size_t kTestSlotCount = 6;

SyntheticSource::Options BaseSourceOptions() {
  SyntheticSource::Options options;
  options.pattern = SyntheticSource::Pattern::kRamp;
  options.slot_size_bytes = kTestSlotBytes;
  options.slot_count = kTestSlotCount;
  return options;
}

CapturePipeline::Options BasePipelineOptions() {
  CapturePipeline::Options options;

  // Locking needs a raised RLIMIT_MEMLOCK that a CI runner will not have, and
  // priority elevation needs privileges it will not have either. Both are
  // degradations rather than requirements — see their own headers — so leaving
  // them off here tests the pipeline rather than the sandbox.
  options.lock_memory = false;
  options.elevate_priority = false;

  options.stall_timeout = 400ms;
  options.snapshot_interval_buffers = 1;
  options.snapshot_bytes = 512;
  return options;
}

// Runs a pipeline to completion and hands back what happened.
struct RunResult {
  TransferResult result = TransferResult::kRunning;
  CaptureStats stats;
};

// Waits for a condition, failing rather than hanging. Every test here is about
// a pipeline that must not hang, so nothing waits without a deadline.
template <typename Predicate>
bool WaitFor(Predicate predicate, std::chrono::milliseconds limit = 5000ms) {
  const auto deadline = std::chrono::steady_clock::now() + limit;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(1ms);
  }
  return predicate();
}

// A bare Wait() would block forever on a pipeline that has failed to stop, and
// the test would then be killed by the harness with nothing to say about which
// assertion it was on. Waiting with a deadline and then forcing the issue turns
// a hang into an ordinary failure with a message.
RunResult RunToCompletion(CapturePipeline& pipeline,
                          std::chrono::milliseconds limit = 10000ms) {
  if (!WaitFor([&] { return !pipeline.Running(); }, limit)) {
    ADD_FAILURE() << "the pipeline was still running after " << limit.count()
                  << " ms; forcing it to stop";
    pipeline.Abort();
  }

  pipeline.Wait();

  RunResult outcome;
  outcome.result = pipeline.Result();
  outcome.stats = pipeline.stats().Read();
  return outcome;
}

class CapturePipelineTest : public ::testing::Test {
 protected:
  CapturePipelineTest()
      : logger_(
            [this](LogLevel level, const std::string& message) {
              log_.emplace_back(level, message);
            },
            LogLevel::kDebug) {}

  bool LogContains(const std::string& fragment) const {
    for (const auto& [level, message] : log_) {
      if (message.find(fragment) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  std::vector<std::pair<LogLevel, std::string>> log_;
  CallbackLogger logger_;
};

// A source that reports a device buffer, so that everything downstream of the
// reading can be tested with nothing plugged in.
//
// `advance` is what separates the two things worth checking: a device that
// keeps answering with the same reading, and one that answers with a new one.
// The pipeline publishes statistics far more often than a real source takes a
// reading, so telling those apart is the whole job.
class TelemetrySource : public SyntheticSource {
 public:
  TelemetrySource(const Options& options, FpgaTelemetry reading, bool advance)
      : SyntheticSource(options), reading_(reading), advance_(advance) {}

  FpgaTelemetry DeviceTelemetry() const override {
    if (advance_) {
      ++reading_.latch_count;
    }
    return reading_;
  }

 private:
  // Mutable because DeviceTelemetry() is const on the interface — a real
  // backend reads a wait-free tap there rather than changing anything. Touched
  // only by the processing thread, which is the only caller.
  mutable FpgaTelemetry reading_;
  bool advance_ = false;
};

FpgaTelemetry MakeReading() {
  FpgaTelemetry reading;
  reading.present = true;
  reading.format = kTelemetryFormat;
  reading.latch_count = 3;
  reading.used_now = 4000;
  reading.peak = 12288;
  reading.peak_since_open = 12288;
  reading.overflow_events = 2;
  reading.dropped_words = 40;
  reading.depth_words = 16384;
  reading.packet_words = 8192;
  reading.near_full_words = 12288;
  return reading;
}

// --- The device's buffer ---------------------------------------------------

TEST_F(CapturePipelineTest, TheDeviceBufferReachesTheStatistics) {
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.slot_limit = 20;
  TelemetrySource source(source_options, MakeReading(), false);

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  const RunResult outcome = RunToCompletion(pipeline);
  EXPECT_EQ(outcome.result, TransferResult::kSuccess);

  EXPECT_TRUE(outcome.stats.device_buffer.present);
  EXPECT_EQ(outcome.stats.device_buffer.peak, 12288);

  // Half the headroom above the packet threshold, which is the scale the
  // indicator is drawn on
  EXPECT_EQ(outcome.stats.peak_back_pressure_percent, 100);
  EXPECT_EQ(outcome.stats.peak_device_buffer_words, 12288);
}

TEST_F(CapturePipelineTest, OneReadingIsCountedOnce) {
  // The pipeline publishes statistics every buffer and a real source takes a
  // reading a few times a second, so the same reading is seen many times over.
  // Adding it in each time would multiply every total by however many buffers
  // went by while the device stood still.
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.slot_limit = 20;
  TelemetrySource source(source_options, MakeReading(), false);

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  const RunResult outcome = RunToCompletion(pipeline);

  EXPECT_EQ(outcome.stats.device_overflow_events, 2u);
  EXPECT_EQ(outcome.stats.device_dropped_words, 40u);
}

TEST_F(CapturePipelineTest, EachNewReadingIsAccumulated) {
  // The device's counters clear when they are read, so the totals over a run
  // exist only as the sum of the readings taken during it.
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.slot_limit = 20;
  TelemetrySource source(source_options, MakeReading(), true);

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  const RunResult outcome = RunToCompletion(pipeline);

  EXPECT_GE(outcome.stats.device_overflow_events, 4u);
  EXPECT_GE(outcome.stats.device_dropped_words, 80u);
}

// --- Lifetime --------------------------------------------------------------

TEST_F(CapturePipelineTest, ABoundedRunCompletesOnItsOwn) {
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.slot_limit = 20;
  SyntheticSource source(source_options);

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  const RunResult outcome = RunToCompletion(pipeline);
  EXPECT_EQ(outcome.result, TransferResult::kSuccess);
  EXPECT_FALSE(pipeline.Running());
  EXPECT_EQ(outcome.stats.buffers_processed, 20U);
  EXPECT_EQ(outcome.stats.metrics.sample_count, 20U * kTestSlotSamples);
}

TEST_F(CapturePipelineTest, AGracefulStopWritesOutWhatWasAlreadyBuffered) {
  SyntheticSource source(BaseSourceOptions());

  auto sink = std::make_unique<test::RecordingSink>();
  test::RecordingSink* sink_view = sink.get();

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::move(sink), BasePipelineOptions()));

  ASSERT_TRUE(
      WaitFor([&] { return pipeline.stats().Read().buffers_processed > 5; }));
  pipeline.RequestStop();

  const RunResult outcome = RunToCompletion(pipeline);
  EXPECT_EQ(outcome.result, TransferResult::kSuccess);

  // Everything the source handed over reached the sink. A graceful stop that
  // dropped the tail would still look like a success from the outside, which is
  // exactly why this is checked by count rather than by status.
  EXPECT_EQ(sink_view->SamplesWritten(),
            outcome.stats.buffers_processed * kTestSlotSamples);
  EXPECT_TRUE(sink_view->finished());
}

TEST_F(CapturePipelineTest, AnAbortStopsWithoutWaitingForTheBuffersToDrain) {
  SyntheticSource source(BaseSourceOptions());

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  ASSERT_TRUE(
      WaitFor([&] { return pipeline.stats().Read().buffers_processed > 2; }));
  pipeline.Abort();

  const RunResult outcome = RunToCompletion(pipeline);
  EXPECT_TRUE(TransferFinished(outcome.result));
  EXPECT_FALSE(pipeline.Running());
}

TEST_F(CapturePipelineTest, APipelineCanBeRunTwice) {
  // The application monitors, captures, stops and monitors again without ever
  // constructing a second pipeline, so leftover state from one run showing up
  // in the next is a real failure mode rather than a hypothetical one.
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.slot_limit = 10;

  CapturePipeline pipeline(&logger_);

  for (int run = 0; run < 2; ++run) {
    SyntheticSource source(source_options);
    ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                               BasePipelineOptions()))
        << "run " << run;
    const RunResult outcome = RunToCompletion(pipeline);
    EXPECT_EQ(outcome.result, TransferResult::kSuccess) << "run " << run;
    EXPECT_EQ(outcome.stats.buffers_processed, 10U) << "run " << run;
  }
}

TEST_F(CapturePipelineTest, StartingATwiceRunningPipelineIsRefused) {
  SyntheticSource source(BaseSourceOptions());

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  SyntheticSource second(BaseSourceOptions());
  EXPECT_FALSE(pipeline.Start(&second, std::make_unique<NullSink>(),
                              BasePipelineOptions()));

  pipeline.Abort();
  pipeline.Wait();
}

// --- Sequence and pattern integrity ----------------------------------------

TEST_F(CapturePipelineTest, TheValidatorLocksOnToTheSyntheticStream) {
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.slot_limit = 40;
  SyntheticSource source(source_options);

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  const RunResult outcome = RunToCompletion(pipeline);
  EXPECT_EQ(outcome.result, TransferResult::kSuccess);
  EXPECT_EQ(outcome.stats.sequence_state, SequenceState::kRunning);
}

TEST_F(CapturePipelineTest, TestModeVerifiesTheDeviceRamp) {
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.slot_limit = 20;
  SyntheticSource source(source_options);

  CapturePipeline::Options options = BasePipelineOptions();
  options.test_mode = true;

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(), options));

  const RunResult outcome = RunToCompletion(pipeline);
  EXPECT_EQ(outcome.result, TransferResult::kSuccess);
  EXPECT_TRUE(outcome.stats.test_pattern_checked);
  EXPECT_TRUE(outcome.stats.test_pattern_passed);
  EXPECT_TRUE(pipeline.test_pattern_result().passed);
  EXPECT_EQ(pipeline.test_pattern_result().sequence_length,
            SyntheticSource::kRampLength);
}

TEST_F(CapturePipelineTest, DiscardedStartupSlotsNeverReachTheSink) {
  // The device is already streaming when the host opens it, so the first
  // transfers hold whatever was mid-flight. Discarding them is what keeps every
  // capture from starting with a spurious sequence error.
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.discard_slots = 4;
  source_options.slot_limit = 10;
  SyntheticSource source(source_options);

  auto sink = std::make_unique<test::RecordingSink>();
  test::RecordingSink* sink_view = sink.get();

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::move(sink), BasePipelineOptions()));

  const RunResult outcome = RunToCompletion(pipeline);
  EXPECT_EQ(outcome.result, TransferResult::kSuccess);
  EXPECT_EQ(sink_view->SamplesWritten(), 10U * kTestSlotSamples)
      << "the discarded slots must not be counted as delivered";
}

// --- Injected faults -------------------------------------------------------

TEST_F(CapturePipelineTest, ASequenceBreakStopsTheCaptureWithItsOwnCode) {
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.fault = SyntheticSource::Fault::kSequenceBreak;
  source_options.fault_at_slot = 3;
  SyntheticSource source(source_options);

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  const RunResult outcome = RunToCompletion(pipeline);
  EXPECT_EQ(outcome.result, TransferResult::kSequenceMismatch);
  EXPECT_EQ(outcome.stats.sequence_state, SequenceState::kFailed);
  EXPECT_NE(pipeline.ResultDetail().find("Sequence counter mismatch"),
            std::string::npos);
}

TEST_F(CapturePipelineTest, AShortDeliveryIsFatalRatherThanAbsorbed) {
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.fault = SyntheticSource::Fault::kShortDelivery;
  source_options.fault_at_slot = 3;
  SyntheticSource source(source_options);

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  const RunResult outcome = RunToCompletion(pipeline);
  EXPECT_EQ(outcome.result, TransferResult::kUsbTransferFailure);
}

TEST_F(CapturePipelineTest, ATransferFailureSurfacesAsItself) {
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.fault = SyntheticSource::Fault::kTransferFailure;
  source_options.fault_at_slot = 2;
  SyntheticSource source(source_options);

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  const RunResult outcome = RunToCompletion(pipeline);
  EXPECT_EQ(outcome.result, TransferResult::kUsbTransferFailure);
}

TEST_F(CapturePipelineTest, ASilentSourceIsDeclaredStalledRatherThanWaitedFor) {
  // The failure the old engine could not see. A device that stops delivering
  // without failing a transfer raises no error and returns from no call, so the
  // application simply waits — and a user watching a frozen progress figure has
  // no way to tell that from a slow disc.
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.fault = SyntheticSource::Fault::kStall;
  source_options.fault_at_slot = 3;
  SyntheticSource source(source_options);

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  const auto started = std::chrono::steady_clock::now();
  const RunResult outcome = RunToCompletion(pipeline);
  const auto elapsed = std::chrono::steady_clock::now() - started;

  EXPECT_EQ(outcome.result, TransferResult::kSourceStalled);
  EXPECT_LT(elapsed, 5s)
      << "the watchdog must fire rather than the test timing "
         "out";
}

TEST_F(CapturePipelineTest, AWriteFailureStopsTheCaptureAndIsReported) {
  SyntheticSource source(BaseSourceOptions());

  auto sink = std::make_unique<test::RecordingSink>();
  sink->FailNextWrite("the volume is full");

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::move(sink), BasePipelineOptions()));

  const RunResult outcome = RunToCompletion(pipeline);
  EXPECT_EQ(outcome.result, TransferResult::kFileWriteError);
  EXPECT_EQ(pipeline.ResultDetail(), "the volume is full");
}

TEST_F(CapturePipelineTest, TheFirstFailureIsTheOneReported) {
  // A sequence mismatch will make the source overflow moments later, because
  // nothing is emptying the ring any more. The user needs to be told about the
  // mismatch: it is the one that says what went wrong.
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.fault = SyntheticSource::Fault::kSequenceBreak;
  source_options.fault_at_slot = 2;
  SyntheticSource source(source_options);

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  const RunResult outcome = RunToCompletion(pipeline);
  EXPECT_EQ(outcome.result, TransferResult::kSequenceMismatch)
      << "a later consequence must not overwrite the original cause";
}

// --- The sink swap ---------------------------------------------------------

TEST_F(CapturePipelineTest, ASinkAttachedMidStreamGetsWholeBuffersOnly) {
  // What "start recording" has to mean: the file begins at a buffer boundary,
  // so no sample is half-written and none is written twice.
  SyntheticSource source(BaseSourceOptions());

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  ASSERT_TRUE(
      WaitFor([&] { return pipeline.stats().Read().buffers_processed > 3; }));

  auto sink = std::make_unique<test::RecordingSink>();
  test::RecordingSink* sink_view = sink.get();
  const uint64_t request = pipeline.AttachSink(std::move(sink));

  ASSERT_TRUE(WaitFor([&] { return pipeline.SinkChangeCount() >= request; }));
  ASSERT_TRUE(WaitFor([&] { return sink_view->write_calls() > 5; }));

  pipeline.RequestStop();
  const RunResult outcome = RunToCompletion(pipeline);
  ASSERT_EQ(outcome.result, TransferResult::kSuccess);

  ASSERT_GT(sink_view->write_calls(), 0U);
  for (size_t samples : sink_view->samples_per_write()) {
    EXPECT_EQ(samples, kTestSlotSamples) << "a partial buffer reached the sink";
  }
  EXPECT_EQ(sink_view->SamplesWritten() % kTestSlotSamples, 0U);
}

TEST_F(CapturePipelineTest, NoSampleIsLostOrRepeatedAcrossASinkChange) {
  // The strongest statement the synthetic source makes possible. Its ramp means
  // every sample's correct successor is known, so a gap or a repeat at the swap
  // point shows up as a break in the recorded values — which counting bytes
  // could never reveal.
  SyntheticSource source(BaseSourceOptions());

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  ASSERT_TRUE(
      WaitFor([&] { return pipeline.stats().Read().buffers_processed > 3; }));

  auto sink = std::make_unique<test::RecordingSink>();
  test::RecordingSink* sink_view = sink.get();
  const uint64_t request = pipeline.AttachSink(std::move(sink));
  ASSERT_TRUE(WaitFor([&] { return pipeline.SinkChangeCount() >= request; }));
  ASSERT_TRUE(WaitFor([&] { return sink_view->write_calls() > 8; }));

  pipeline.RequestStop();
  const RunResult outcome = RunToCompletion(pipeline);
  ASSERT_EQ(outcome.result, TransferResult::kSuccess);

  const std::vector<uint16_t>& values = sink_view->values();
  ASSERT_GT(values.size(), kTestSlotSamples);

  // Scanned into one verdict rather than asserted per sample: there are
  // hundreds of thousands of them, and a million passing assertions would cost
  // more time than the rest of the suite put together while saying nothing
  // extra.
  size_t first_break = 0;
  for (size_t index = 1; index < values.size(); ++index) {
    const uint16_t expected = static_cast<uint16_t>(
        (values[index - 1] + 1) % SyntheticSource::kRampLength);
    if (values[index] != expected) {
      first_break = index;
      break;
    }
  }

  EXPECT_EQ(first_break, 0U)
      << "the ramp broke at sample " << first_break << " of " << values.size()
      << ", which is "
      << (first_break % kTestSlotSamples == 0 ? "a buffer boundary"
                                              : "inside a buffer");
}

TEST_F(CapturePipelineTest, DetachingFinishesTheSinkAndLeavesTheStreamRunning) {
  // Stopping a capture must not stop monitoring: the user is still watching the
  // signal, and tearing the pipeline down to close a file would put a gap in
  // the display for no reason.
  SyntheticSource source(BaseSourceOptions());

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  auto sink = std::make_unique<test::RecordingSink>();
  test::RecordingSink* sink_view = sink.get();
  uint64_t request = pipeline.AttachSink(std::move(sink));
  ASSERT_TRUE(WaitFor([&] { return pipeline.SinkChangeCount() >= request; }));
  ASSERT_TRUE(WaitFor([&] { return sink_view->write_calls() > 3; }));

  request = pipeline.DetachSink();
  ASSERT_TRUE(WaitFor([&] { return pipeline.SinkChangeCount() >= request; }));

  EXPECT_TRUE(sink_view->finished());
  const uint64_t writes_at_detach = sink_view->write_calls();

  // The pipeline is still going, and the retired sink is not receiving any more
  const uint64_t buffers_at_detach = pipeline.stats().Read().buffers_processed;
  ASSERT_TRUE(WaitFor([&] {
    return pipeline.stats().Read().buffers_processed > buffers_at_detach + 3;
  }));
  EXPECT_EQ(sink_view->write_calls(), writes_at_detach);
  EXPECT_TRUE(pipeline.Running());

  pipeline.RequestStop();
  EXPECT_EQ(RunToCompletion(pipeline).result, TransferResult::kSuccess);

  // The retired sink is handed back so the caller can report on the file it
  // wrote — its size, its path — after it has been closed.
  const std::unique_ptr<ISampleSink> retired = pipeline.TakeRetiredSink();
  ASSERT_NE(retired, nullptr);
  EXPECT_EQ(retired.get(), sink_view);
}

TEST_F(CapturePipelineTest, ASwapLandsBetweenBuffersRatherThanDuringOne) {
  SyntheticSource source(BaseSourceOptions());

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  ASSERT_TRUE(
      WaitFor([&] { return pipeline.stats().Read().buffers_processed > 3; }));

  auto sink = std::make_unique<test::RecordingSink>();
  test::RecordingSink* sink_view = sink.get();
  const uint64_t request = pipeline.AttachSink(std::move(sink));
  ASSERT_TRUE(WaitFor([&] { return pipeline.SinkChangeCount() >= request; }));

  const uint64_t swap_buffer = pipeline.LastSinkChangeBuffer();
  ASSERT_TRUE(WaitFor([&] { return sink_view->write_calls() > 3; }));

  pipeline.RequestStop();
  const RunResult outcome = RunToCompletion(pipeline);
  ASSERT_EQ(outcome.result, TransferResult::kSuccess);

  // Everything from the swap point onwards went to the new sink, and nothing
  // before it did.
  EXPECT_EQ(sink_view->write_calls(),
            outcome.stats.buffers_processed - swap_buffer);
}

// --- Throughput -------------------------------------------------------------

// An eighth of wire rate: 10 MB/s. Fast enough that a window holds a good many
// buffers, slow enough that a loaded CI runner is not the thing being measured.
//
// The ceiling matters more than it looks. A paced source can only run late, so
// on a host that cannot generate, validate and write at this rate the pacing
// simply never applies and these tests measure the machine instead of the
// pipeline. Half wire rate was inside the ceiling of every runner here except
// the macOS one, which managed around 17 MB/s and failed both tests on the
// lower bound alone. An eighth leaves that host most of its headroom, and
// PacingSlipSeconds() below says so outright when a slower one comes along.
constexpr uint64_t kTestPacedBytesPerSecond = kWireBytesPerSecond / 8;

// Long enough at the paced rate to hold a dozen buffers, so the figure is a
// rate rather than a verdict on the last two buffers' scheduling.
constexpr auto kTestThroughputWindow = 300ms;

// The pacing has to have applied for anything measured against the configured
// rate to mean anything. Scheduler jitter puts a source a fraction of a
// millisecond behind; a host that cannot keep up falls behind without bound, so
// anything approaching a whole window apart says the run was never paced.
void ExpectThePacingApplied(const SyntheticSource& source) {
  const double window =
      std::chrono::duration<double>(kTestThroughputWindow).count();
  EXPECT_LT(source.PacingSlipSeconds(), window)
      << "the source fell " << source.PacingSlipSeconds() << " s behind its "
      << (kTestPacedBytesPerSecond / 1'000'000)
      << " MB/s pacing, so this host never ran the capture at the rate the "
         "test asked for and the figures below are measuring the machine";
}

// How far either side of the paced rate a reading may fall and still be the
// pipeline's answer rather than the host's.
//
// A source that ran late does not settle for a lower rate: PaceForBytes works
// every deadline out from the origin, so the moment the host catches up it
// hands over the backlog as fast as it can generate it. A window holding that
// catch-up reads high by the share of the debt repaid inside it, and a window
// holding the slip itself reads low by the same measure, so a run that fell s
// behind can be out by s/window in either direction with nothing wrong with
// the figure being published. The twenty percent on top is buffer granularity:
// a window spans about a dozen slots and can only ever count whole ones.
double ThroughputBandFraction(const SyntheticSource& source) {
  const double window =
      std::chrono::duration<double>(kTestThroughputWindow).count();
  return 0.2 + source.PacingSlipSeconds() / window;
}

TEST_F(CapturePipelineTest, ThroughputIsTheRecentRateNotTheAverageSinceStart) {
  // The figure a user reads has to say what the capture is doing now. An
  // average since the start cannot: the run begins with time in which no buffer
  // could have been processed — threads starting, the source's opening slots
  // being discarded — and that dead time stays in the denominator forever,
  // holding the figure below the true rate for as long as anyone is looking at
  // it.
  //
  // Twelve discarded slots is a third of a second at the paced rate,
  // exaggerating what the real device does with four so that the two
  // definitions give visibly different answers.
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.rate_bytes_per_second = kTestPacedBytesPerSecond;
  source_options.discard_slots = 12;
  SyntheticSource source(source_options);

  CapturePipeline::Options options = BasePipelineOptions();
  options.throughput_window = kTestThroughputWindow;
  options.stall_timeout = 2000ms;

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(), options));

  CaptureStats stats;
  ASSERT_TRUE(WaitFor([&] {
    stats = pipeline.stats().Read();
    return stats.throughput_bytes_per_second > 0.0;
  }));

  pipeline.Abort();
  pipeline.Wait();

  ExpectThePacingApplied(source);

  const double measured = stats.throughput_bytes_per_second;
  const auto paced = static_cast<double>(kTestPacedBytesPerSecond);
  const double band = ThroughputBandFraction(source);
  EXPECT_GT(measured, paced * (1.0 - band));
  EXPECT_LT(measured, paced * (1.0 + band));

  // And the same snapshot's average since the start, which is what used to be
  // published. It is still there to be worked out; it is simply not the answer
  // to the question the panel asks.
  const double lifetime =
      static_cast<double>(stats.buffers_processed * kTestSlotBytes) /
      stats.elapsed_seconds;
  EXPECT_GT(measured, lifetime * 1.5)
      << "the published rate is tracking the lifetime average rather than the "
         "window";
}

TEST_F(CapturePipelineTest, NoThroughputIsPublishedUntilAWindowHasPassed) {
  // A rate taken across two buffers says more about what the scheduler did to
  // them than about the capture. Until there is a window to measure, the
  // statistic is zero — which the panel shows as no reading rather than as a
  // number.
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.slot_limit = 8;
  SyntheticSource source(source_options);

  CapturePipeline::Options options = BasePipelineOptions();
  options.throughput_window = 5000ms;

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(), options));

  const RunResult outcome = RunToCompletion(pipeline);
  ASSERT_EQ(outcome.result, TransferResult::kSuccess);
  ASSERT_EQ(outcome.stats.buffers_processed, 8U);

  EXPECT_EQ(outcome.stats.throughput_bytes_per_second, 0.0);
}

TEST_F(CapturePipelineTest, AStoppedCaptureKeepsTheLastRateItMeasured) {
  // The clock runs on through the join and the encoder's final frame while no
  // further buffer can arrive. Measuring across that would end every capture by
  // reporting a fraction of the rate it actually ran at, which is the one
  // reading a user is most likely to write down.
  //
  // Thirty-two slots is most of a second at the paced rate: enough that the
  // window has turned over several times before the stop, so the figure being
  // checked is a settled one rather than the first the run produced.
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.rate_bytes_per_second = kTestPacedBytesPerSecond;
  source_options.slot_limit = 32;
  SyntheticSource source(source_options);

  CapturePipeline::Options options = BasePipelineOptions();
  options.throughput_window = kTestThroughputWindow;
  options.stall_timeout = 2000ms;

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(), options));

  const RunResult outcome = RunToCompletion(pipeline);
  ASSERT_EQ(outcome.result, TransferResult::kSuccess);

  ExpectThePacingApplied(source);

  const auto paced = static_cast<double>(kTestPacedBytesPerSecond);
  const double band = ThroughputBandFraction(source);
  EXPECT_GT(outcome.stats.throughput_bytes_per_second, paced * (1.0 - band));
  EXPECT_LT(outcome.stats.throughput_bytes_per_second, paced * (1.0 + band));
}

// --- The monitor tap under a running pipeline -------------------------------

TEST_F(CapturePipelineTest, SnapshotsArriveWhileTheCaptureRuns) {
  SyntheticSource source(BaseSourceOptions());

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  std::vector<uint8_t> snapshot;
  uint64_t generation = 0;
  ASSERT_TRUE(WaitFor(
      [&] { return pipeline.snapshots().TryRead(snapshot, generation); }));

  EXPECT_EQ(snapshot.size(), 512U);
  EXPECT_GT(generation, 0U);

  pipeline.Abort();
  pipeline.Wait();
}

// --- What the log says about a run -----------------------------------------
//
// These pin the debug-level account of a capture, which is the only record of
// how a run behaved on somebody else's machine. Asserted on by fragment rather
// than by whole line: the wording is meant to be edited, and what must not
// change is that each figure is reported at all.

TEST_F(CapturePipelineTest, TheStartIsLoggedWithTheGeometryItWillRunWith) {
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.slot_limit = 4;
  SyntheticSource source(source_options);

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));
  RunToCompletion(pipeline);

  // The ring, as a size and as the length of time it buys — which is the term
  // that decides whether it is big enough.
  EXPECT_TRUE(LogContains("Ring: "));
  EXPECT_TRUE(LogContains("of headroom"));
  EXPECT_TRUE(LogContains("slots of"));

  // And the settings the run was made under, so a log can be read without the
  // settings file that has since been changed.
  EXPECT_TRUE(LogContains("Options: test mode off"));
  EXPECT_TRUE(LogContains("stall timeout"));
}

TEST_F(CapturePipelineTest, TheStopIsLoggedWithWhatWentThroughAndWhatItCost) {
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.slot_limit = 8;
  SyntheticSource source(source_options);

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));
  const RunResult outcome = RunToCompletion(pipeline);
  ASSERT_EQ(outcome.result, TransferResult::kSuccess);

  EXPECT_TRUE(LogContains("Run: "));
  EXPECT_TRUE(LogContains("transfers"));
  EXPECT_TRUE(LogContains("of stream"));

  // The ring's own account. A peak alone cannot tell a run that touched a level
  // once from one that sat there, which is why the mean and the counts are
  // beside it.
  EXPECT_TRUE(LogContains("Ring depth: mean "));
  EXPECT_TRUE(LogContains(" filled, "));

  // Accumulated a buffer at a time rather than worked out at the end from the
  // peak: one reading per buffer that was processed, which is what makes the
  // mean mean anything.
  EXPECT_EQ(pipeline.ring_fill().readings(), outcome.stats.buffers_processed);
  EXPECT_GT(pipeline.ring_fill().peak_percent(), 0);

  EXPECT_TRUE(LogContains("Signal over the run: "));
  EXPECT_TRUE(LogContains("of 1023"));
}

TEST_F(CapturePipelineTest, TheDeviceBackPressureIsSummarisedWhenTheRunEnds) {
  SyntheticSource::Options source_options = BaseSourceOptions();
  source_options.slot_limit = 20;

  // A reading that advances, so the pipeline accumulates it rather than seeing
  // the same interval over and over.
  TelemetrySource source(source_options, MakeReading(), true);

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));
  RunToCompletion(pipeline);

  EXPECT_TRUE(LogContains("Device back pressure: mean "));
  EXPECT_TRUE(LogContains("near-full mark for"));

  // The readings themselves, and not only the sentence built from them. Every
  // interval this source reports lost samples, and an interval that lost a
  // sample is full back pressure whatever its occupancy arithmetic says — so
  // both figures must land on exactly 100 rather than on the 50 the peak of
  // 12,288 words in a 16,384-word buffer would otherwise give.
  EXPECT_GT(pipeline.device_back_pressure().readings(), 0U);
  EXPECT_EQ(pipeline.device_back_pressure().peak_percent(), 100);
  EXPECT_DOUBLE_EQ(pipeline.device_back_pressure().mean_percent(), 100.0);
}

TEST_F(CapturePipelineTest, AClosingFileIsLoggedWithWhatReachedIt) {
  SyntheticSource source(BaseSourceOptions());

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                             BasePipelineOptions()));

  auto sink = std::make_unique<test::RecordingSink>();
  test::RecordingSink* sink_view = sink.get();
  uint64_t request = pipeline.AttachSink(std::move(sink));
  ASSERT_TRUE(WaitFor([&] { return pipeline.SinkChangeCount() >= request; }));
  ASSERT_TRUE(WaitFor([&] { return sink_view->write_calls() > 3; }));

  request = pipeline.DetachSink();
  ASSERT_TRUE(WaitFor([&] { return pipeline.SinkChangeCount() >= request; }));

  pipeline.RequestStop();
  ASSERT_EQ(RunToCompletion(pipeline).result, TransferResult::kSuccess);

  // How much reached the file, how long that is, and what the encoder's own
  // finish cost — which happens on the processing thread between two buffers
  // and is paid for out of the ring's headroom.
  EXPECT_TRUE(LogContains("Closed recording after "));
  EXPECT_TRUE(LogContains("samples = "));
  EXPECT_TRUE(LogContains("finishing took "));

  // And the state of the ring at each swap, which is where a slow finish shows
  // up as a backlog rather than as a lost sample.
  EXPECT_TRUE(LogContains("Sink change at buffer "));
  EXPECT_TRUE(LogContains("peak so far"));
}

// Paced rather than free-running, so that the run lasts long enough for the
// control thread's own poll to come round several times. Four slots a second
// over three slots is about three quarters of a second — long by the standards
// of this file and the only way to test something that happens on a timer
// without polling the log from another thread while it is being written.
SyntheticSource::Options PacedSourceOptions() {
  SyntheticSource::Options options = BaseSourceOptions();
  options.rate_bytes_per_second = 4 * kTestSlotBytes;
  options.slot_limit = 3;
  return options;
}

TEST_F(CapturePipelineTest, ProgressIsLoggedWhileTheRunGoesOn) {
  SyntheticSource source(PacedSourceOptions());

  CapturePipeline::Options options = BasePipelineOptions();

  // Ten seconds in the application, which no test can wait for. What is being
  // checked is that the interval is honoured at all and that the line carries
  // the figures somebody watching a long capture needs.
  options.progress_log_interval = 100ms;

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(), options));
  RunToCompletion(pipeline);

  // "Streaming" and not "Capturing": nothing is being written, and the line
  // says which of the two a run was without anybody having to work it out.
  EXPECT_TRUE(LogContains("Streaming for "));
  EXPECT_TRUE(LogContains(" slots, peak "));
  EXPECT_TRUE(LogContains("MB/s"));
}

TEST_F(CapturePipelineTest, ProgressIsNotLoggedWhenTheIntervalIsZero) {
  SyntheticSource source(PacedSourceOptions());

  CapturePipeline::Options options = BasePipelineOptions();
  options.progress_log_interval = std::chrono::milliseconds{0};

  CapturePipeline pipeline(&logger_);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(), options));
  RunToCompletion(pipeline);

  EXPECT_FALSE(LogContains("Streaming for "));
}

TEST_F(CapturePipelineTest,
       TheDestructorStopsARunningCaptureRatherThanHanging) {
  // A pipeline destroyed while running is what happens when the application is
  // closed mid-monitor, and it must not deadlock in the destructor.
  SyntheticSource source(BaseSourceOptions());

  {
    CapturePipeline pipeline(&logger_);
    ASSERT_TRUE(pipeline.Start(&source, std::make_unique<NullSink>(),
                               BasePipelineOptions()));
    ASSERT_TRUE(
        WaitFor([&] { return pipeline.stats().Read().buffers_processed > 2; }));
  }

  SUCCEED();
}

}  // namespace
}  // namespace ddd::capture
