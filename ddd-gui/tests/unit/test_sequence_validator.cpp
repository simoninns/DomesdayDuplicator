/************************************************************************

    test_sequence_validator.cpp

    T1 tests for sequence-marker validation and the metrics that share its pass
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

#include "sample_metrics.h"
#include "sequence_validator.h"
#include "wire_data.h"

namespace ddd::capture {
namespace {

// Enough samples that at least one counter change falls inside, which is what
// the validator needs to lock on.
constexpr size_t kLockOnSamples = kSamplesPerSequenceCounter + 16;

TEST(SequenceValidatorTest, ACleanStreamLocksOnAndStaysLocked) {
  test::WireStreamBuilder builder(0, 100);
  builder.AppendConstant(600, kLockOnSamples);

  SequenceValidator validator;
  const SequenceValidator::Outcome outcome =
      validator.Process(builder.bytes().data(), builder.bytes().size());

  EXPECT_TRUE(outcome.ok);
  EXPECT_EQ(validator.state(), SequenceState::kRunning);
  EXPECT_EQ(outcome.tally.sample_count, kLockOnSamples);
}

TEST(SequenceValidatorTest, LockIsAcquiredWithinOneCounterPeriod) {
  // The guarantee the search limit rests on: whatever phase the stream is in
  // when the host starts reading, a change appears within 65,537 samples.
  test::WireStreamBuilder builder(11, 1);
  builder.AppendConstant(512, kSamplesPerSequenceCounter + 1);

  SequenceValidator validator;
  ASSERT_TRUE(
      validator.Process(builder.bytes().data(), builder.bytes().size()).ok);
  EXPECT_EQ(validator.state(), SequenceState::kRunning);
}

TEST(SequenceValidatorTest, AStreamWithoutMarkersDisablesCheckingAndContinues) {
  // Older gateware emits no markers. Refusing to capture from a working device
  // because its firmware predates a diagnostic would be the wrong trade.
  std::vector<uint8_t> bytes((kSamplesPerSequenceCounter + 4) * kBytesPerSample,
                             0);
  for (size_t index = 0; index < bytes.size(); index += kBytesPerSample) {
    bytes[index] = 0x55;
    bytes[index + 1] = 0x01;
  }

  SequenceValidator validator;
  const SequenceValidator::Outcome outcome =
      validator.Process(bytes.data(), bytes.size());

  EXPECT_TRUE(outcome.ok);
  EXPECT_EQ(validator.state(), SequenceState::kDisabled);
  EXPECT_EQ(outcome.tally.sample_count, bytes.size() / kBytesPerSample);
}

TEST(SequenceValidatorTest, AMidStreamMismatchFailsAtTheExactSample) {
  // A first buffer ending exactly on a counter boundary, so the second one
  // starts in a known phase.
  test::WireStreamBuilder builder(0, 100);
  builder.AppendConstant(600, 100);
  builder.AppendConstant(600, kSamplesPerSequenceCounter);

  SequenceValidator validator;
  ASSERT_TRUE(
      validator.Process(builder.bytes().data(), builder.bytes().size()).ok);
  ASSERT_EQ(validator.state(), SequenceState::kRunning);

  // A second buffer whose counter has jumped, exactly as a lost transfer would
  // leave it.
  test::WireStreamBuilder broken(builder.counter(), kSamplesPerSequenceCounter);
  broken.AppendConstant(600, 100);
  broken.SkipCounter();
  broken.AppendConstant(600, 100);

  const SequenceValidator::Outcome outcome =
      validator.Process(broken.bytes().data(), broken.bytes().size());

  EXPECT_FALSE(outcome.ok);
  EXPECT_EQ(validator.state(), SequenceState::kFailed);
  EXPECT_EQ(outcome.mismatch_sample_index, 100U);
  EXPECT_EQ(outcome.tally.sample_count, 100U);
}

TEST(SequenceValidatorTest, TheCounterWrapsAtSixtyTwoRatherThanSixtyThree) {
  // 63 distinct values, not 64. A validator that wrapped at 64 would report a
  // mismatch once every 63 * 65,536 samples — about once every tenth of a
  // second at full rate, which is to say immediately.
  test::WireStreamBuilder builder(61, 4);
  builder.AppendConstant(512, 4);                           // counter 61
  builder.AppendConstant(512, kSamplesPerSequenceCounter);  // counter 62
  builder.AppendConstant(512, kSamplesPerSequenceCounter);  // wraps to 0
  builder.AppendConstant(512, 16);

  SequenceValidator validator;
  const SequenceValidator::Outcome outcome =
      validator.Process(builder.bytes().data(), builder.bytes().size());

  EXPECT_TRUE(outcome.ok);
  EXPECT_EQ(validator.state(), SequenceState::kRunning);
}

TEST(SequenceValidatorTest, ThePhaseSurvivesABufferBoundary) {
  // The device does not know where our buffers end, so a counter period that
  // straddles two of them has to be carried across.
  test::WireStreamBuilder builder(5, 10);
  builder.AppendConstant(512, 10);
  builder.AppendConstant(512, size_t{kSamplesPerSequenceCounter} * 2);

  const size_t half = (builder.bytes().size() / 2) & ~size_t{1};

  SequenceValidator validator;
  ASSERT_TRUE(validator.Process(builder.bytes().data(), half).ok);
  EXPECT_TRUE(
      validator
          .Process(builder.bytes().data() + half, builder.bytes().size() - half)
          .ok);
  EXPECT_EQ(validator.state(), SequenceState::kRunning);
}

TEST(SequenceValidatorTest, MarkersAreStrippedFromTheBufferInPlace) {
  // What reaches the sink must be sample data and nothing else, or every
  // capture would carry the counter as noise in its top bits.
  test::WireStreamBuilder builder(37, 8);
  builder.AppendConstant(0x2AB, 8);
  builder.AppendConstant(0x2AB, 32);

  std::vector<uint8_t> bytes = builder.bytes();
  ASSERT_NE(test::WordAt(bytes, 0), 0x2AB) << "the fixture carries no marker";

  SequenceValidator validator;
  ASSERT_TRUE(validator.Process(bytes.data(), bytes.size()).ok);

  for (size_t index = 0; index < bytes.size() / kBytesPerSample; ++index) {
    EXPECT_EQ(test::WordAt(bytes, index), 0x2AB) << "sample " << index;
  }
}

TEST(SequenceValidatorTest, ClippedSamplesAreCountedAtBothEnds) {
  test::WireStreamBuilder builder(0, 40);
  builder.AppendConstant(kMinimumSampleValue, 7);
  builder.AppendConstant(kMaximumSampleValue, 5);
  builder.AppendConstant(512, 28);
  builder.AppendConstant(512, 64);

  SequenceValidator validator;
  const SequenceValidator::Outcome outcome =
      validator.Process(builder.bytes().data(), builder.bytes().size());

  EXPECT_EQ(outcome.tally.clipped_low_count, 7U);
  EXPECT_EQ(outcome.tally.clipped_high_count, 5U);
  EXPECT_EQ(outcome.tally.minimum_value, kMinimumSampleValue);
  EXPECT_EQ(outcome.tally.maximum_value, kMaximumSampleValue);
}

TEST(SequenceValidatorTest, TheRootMeanSquareIsMeasuredAboutTheMidpoint) {
  // A square wave at +/-100 about 512 has an RMS of exactly 100, which is a
  // value that can be checked rather than eyeballed.
  test::WireStreamBuilder builder(0, 64);
  for (size_t index = 0; index < 64; ++index) {
    builder.Append(static_cast<uint16_t>((index % 2 == 0) ? 612 : 412));
  }
  for (size_t index = 0; index < 64; ++index) {
    builder.Append(static_cast<uint16_t>((index % 2 == 0) ? 612 : 412));
  }

  SequenceValidator validator;
  const SequenceValidator::Outcome outcome =
      validator.Process(builder.bytes().data(), builder.bytes().size());

  SampleMetrics metrics;
  metrics.Accumulate(outcome.tally);
  EXPECT_NEAR(metrics.Snapshot().rms, 100.0, 0.001);
}

TEST(SampleMetricsTest, RecentFiguresTrackTheLastBufferOnly) {
  // A whole-capture maximum records the worst moment since the run started and
  // never comes back down, so it cannot show a user that turning the RF gain
  // down has helped. The recent figures are what can.
  SampleMetrics metrics;

  BufferTally loud;
  loud.sample_count = 100;
  loud.minimum_value = 0;
  loud.maximum_value = 1023;
  loud.clipped_low_count = 10;
  loud.clipped_high_count = 12;
  metrics.Accumulate(loud);

  BufferTally quiet;
  quiet.sample_count = 100;
  quiet.minimum_value = 400;
  quiet.maximum_value = 600;
  metrics.Accumulate(quiet);

  const SampleMetricsSnapshot snapshot = metrics.Snapshot();
  EXPECT_EQ(snapshot.maximum_value, 1023);
  EXPECT_EQ(snapshot.clipped_high_count, 12U);
  EXPECT_EQ(snapshot.recent_maximum_value, 600);
  EXPECT_EQ(snapshot.recent_clipped_high_count, 0U);
  EXPECT_EQ(snapshot.sample_count, 200U);
}

TEST(SampleMetricsTest, NothingMeasuredReportsZeroRatherThanItsSeed) {
  const SampleMetricsSnapshot snapshot = SampleMetrics{}.Snapshot();
  EXPECT_EQ(snapshot.minimum_value, 0);
  EXPECT_EQ(snapshot.maximum_value, 0);
  EXPECT_EQ(snapshot.rms, 0.0);
}

TEST(SampleMetricsTest, ResetPutsItBackToNothingMeasured) {
  SampleMetrics metrics;
  BufferTally tally;
  tally.sample_count = 50;
  tally.minimum_value = 100;
  tally.maximum_value = 900;
  metrics.Accumulate(tally);
  ASSERT_EQ(metrics.Snapshot().sample_count, 50U);

  metrics.Reset();
  EXPECT_EQ(metrics.Snapshot().sample_count, 0U);
  EXPECT_EQ(metrics.Snapshot().maximum_value, 0);
}

// The real-time budget. At 80 MB/s a 2 MB buffer arrives every 26 ms, and this
// pass is the largest single piece of work between arrivals — so if it does not
// fit here, nothing downstream has a chance.
//
// The bound is deliberately loose (half the budget rather than a tight
// fraction) because CI runners are slow, shared and virtualised, and a
// performance test that fails on a busy runner teaches people to ignore
// failures. What it catches is a regression of the kind that matters: a second
// pass over the buffer, an allocation in the loop, a per-sample function call
// that stopped being inlined.
TEST(SequenceValidatorTest, AFullSizeBufferIsValidatedInsideTheBudget) {
  constexpr size_t kSlotBytes = size_t{2} << 20;
  constexpr size_t kSlotSamples = kSlotBytes / kBytesPerSample;

  test::WireStreamBuilder builder(0, kSamplesPerSequenceCounter);
  builder.AppendRamp(kSlotSamples);
  const std::vector<uint8_t> pristine = builder.bytes();
  ASSERT_EQ(pristine.size(), kSlotBytes);

  // The pass rewrites the buffer in place, stripping the markers, so each
  // repetition needs its own copy and its own validator. Both are set up
  // outside the timed region: what is being measured is the pass, not the
  // memcpy that stands in for the USB transfer.
  std::vector<uint8_t> working(pristine.size());
  SampleMetrics metrics;

  const auto time_one_pass = [&]() {
    std::memcpy(working.data(), pristine.data(), pristine.size());
    SequenceValidator validator;

    const auto started = std::chrono::steady_clock::now();
    const SequenceValidator::Outcome outcome =
        validator.Process(working.data(), working.size());
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_TRUE(outcome.ok);
    metrics.Accumulate(outcome.tally);
    return std::chrono::duration<double, std::milli>(elapsed).count();
  };

  // One pass to warm the caches and the branch predictor, so the measurement is
  // of the steady state a capture actually runs in.
  time_one_pass();

  constexpr int kPasses = 8;
  double total_milliseconds = 0.0;
  for (int pass = 0; pass < kPasses; ++pass) {
    total_milliseconds += time_one_pass();
  }
  const double milliseconds_per_buffer = total_milliseconds / kPasses;

  // Printed whether it passes or not: the number is more useful than the
  // verdict when someone is deciding whether a machine can sustain a capture.
  std::cout << "[          ] validation + metrics: " << milliseconds_per_buffer
            << " ms per 2 MB buffer (budget 26 ms)\n";

  EXPECT_LT(milliseconds_per_buffer, 13.0);
}

}  // namespace
}  // namespace ddd::capture
