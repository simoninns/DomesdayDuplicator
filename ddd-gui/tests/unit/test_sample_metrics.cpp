/************************************************************************

    test_sample_metrics.cpp

    T1 tests for measuring the recording apart from the session
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include "sample_metrics.h"

namespace ddd::capture {
namespace {

// One buffer's worth of signal, described by what matters about it.
BufferTally Buffer(uint64_t samples, uint16_t minimum, uint16_t maximum,
                   uint64_t clipped_low = 0, uint64_t clipped_high = 0) {
  BufferTally tally;
  tally.sample_count = samples;
  tally.minimum_value = minimum;
  tally.maximum_value = maximum;
  tally.clipped_low_count = clipped_low;
  tally.clipped_high_count = clipped_high;

  // A flat signal at the maximum, so the RMS is a number the test can predict
  // without repeating the implementation's arithmetic.
  tally.sum_of_squares = samples * uint64_t{maximum} * uint64_t{maximum};
  return tally;
}

TEST(SampleMetricsTest, WithNoCaptureTheFileFiguresAreEmpty) {
  // The state every monitoring session sits in. Nothing has been recorded, so
  // there is nothing to say about a recording — and zero is the honest answer
  // rather than the session's own figures leaking into the file's.
  SampleMetrics metrics;
  metrics.Accumulate(Buffer(1000, 100, 900));

  const SampleMetricsSnapshot snapshot = metrics.Snapshot();
  EXPECT_EQ(snapshot.sample_count, 1000U);
  EXPECT_EQ(snapshot.capture_sample_count, 0U);
  EXPECT_EQ(snapshot.capture_minimum_value, 0U);
  EXPECT_EQ(snapshot.capture_maximum_value, 0U);
  EXPECT_DOUBLE_EQ(snapshot.capture_rms, 0.0);
}

TEST(SampleMetricsTest, TheFileFiguresExcludeWhatCameBeforeIt) {
  // The whole point. A minute of setting up before the writer was attached is
  // not in the file, so a maximum that includes it is a claim about a recording
  // that never contained it.
  SampleMetrics metrics;
  metrics.Accumulate(Buffer(1000, 10, 1010, 5, 7));

  metrics.BeginCaptureSpan();
  metrics.Accumulate(Buffer(2000, 400, 600, 1, 2));

  const SampleMetricsSnapshot snapshot = metrics.Snapshot();

  // The session saw the loud buffer; the file did not.
  EXPECT_EQ(snapshot.minimum_value, 10U);
  EXPECT_EQ(snapshot.maximum_value, 1010U);
  EXPECT_EQ(snapshot.clipped_low_count, 6U);
  EXPECT_EQ(snapshot.clipped_high_count, 9U);

  EXPECT_EQ(snapshot.capture_sample_count, 2000U);
  EXPECT_EQ(snapshot.capture_minimum_value, 400U);
  EXPECT_EQ(snapshot.capture_maximum_value, 600U);
  EXPECT_EQ(snapshot.capture_clipped_low_count, 1U);
  EXPECT_EQ(snapshot.capture_clipped_high_count, 2U);
  EXPECT_DOUBLE_EQ(snapshot.capture_rms, 600.0);
}

TEST(SampleMetricsTest, TheFileFiguresExcludeWhatCameAfterIt) {
  // Without the closing half of the span the figures would go on growing
  // through the tick or two between the file closing and its metadata being
  // written, and the file would be described as containing samples that
  // reached no file at all.
  SampleMetrics metrics;

  metrics.BeginCaptureSpan();
  metrics.Accumulate(Buffer(2000, 400, 600));
  metrics.EndCaptureSpan();

  metrics.Accumulate(Buffer(1000, 10, 1010, 5, 7));

  const SampleMetricsSnapshot snapshot = metrics.Snapshot();
  EXPECT_EQ(snapshot.capture_sample_count, 2000U);
  EXPECT_EQ(snapshot.capture_minimum_value, 400U);
  EXPECT_EQ(snapshot.capture_maximum_value, 600U);
  EXPECT_EQ(snapshot.capture_clipped_low_count, 0U);
  EXPECT_EQ(snapshot.capture_clipped_high_count, 0U);
}

TEST(SampleMetricsTest, ASecondCaptureDoesNotInheritTheFirst) {
  // Two sides of a disc, captured from one monitoring session. The second
  // file's metadata must describe the second file.
  SampleMetrics metrics;

  metrics.BeginCaptureSpan();
  metrics.Accumulate(Buffer(2000, 100, 1000, 3, 4));
  metrics.EndCaptureSpan();

  metrics.BeginCaptureSpan();
  metrics.Accumulate(Buffer(500, 480, 520));

  const SampleMetricsSnapshot snapshot = metrics.Snapshot();
  EXPECT_EQ(snapshot.capture_sample_count, 500U);
  EXPECT_EQ(snapshot.capture_minimum_value, 480U);
  EXPECT_EQ(snapshot.capture_maximum_value, 520U);
  EXPECT_EQ(snapshot.capture_clipped_low_count, 0U);
  EXPECT_EQ(snapshot.capture_clipped_high_count, 0U);
}

TEST(SampleMetricsTest, TheSpanSurvivesAcrossBuffers) {
  SampleMetrics metrics;
  metrics.BeginCaptureSpan();
  metrics.Accumulate(Buffer(1000, 300, 700, 1, 0));
  metrics.Accumulate(Buffer(1000, 200, 600, 0, 2));

  const SampleMetricsSnapshot snapshot = metrics.Snapshot();
  EXPECT_EQ(snapshot.capture_sample_count, 2000U);
  EXPECT_EQ(snapshot.capture_minimum_value, 200U);
  EXPECT_EQ(snapshot.capture_maximum_value, 700U);
  EXPECT_EQ(snapshot.capture_clipped_low_count, 1U);
  EXPECT_EQ(snapshot.capture_clipped_high_count, 2U);
}

TEST(SampleMetricsTest, AResetClosesAnOpenSpan) {
  // A new run starts from nothing, including the span: an accumulator left open
  // across a Reset would carry the previous run's file into this one's.
  SampleMetrics metrics;
  metrics.BeginCaptureSpan();
  metrics.Accumulate(Buffer(1000, 300, 700));

  metrics.Reset();
  metrics.Accumulate(Buffer(1000, 10, 1010));

  const SampleMetricsSnapshot snapshot = metrics.Snapshot();
  EXPECT_EQ(snapshot.capture_sample_count, 0U);
  EXPECT_EQ(snapshot.capture_maximum_value, 0U);
}

}  // namespace
}  // namespace ddd::capture
