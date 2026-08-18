/************************************************************************

    test_amplitude_history.cpp

    Signal level over minutes rather than microseconds
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <optional>

#include "amplitude_history.h"

namespace ddd::analysis {
namespace {

AmplitudePoint MakePoint(double seconds, double rms, uint16_t minimum,
                         uint16_t maximum, uint64_t clipped = 0) {
  AmplitudePoint point;
  point.seconds = seconds;
  point.rms_codes = rms;
  point.minimum_code = minimum;
  point.maximum_code = maximum;
  point.clipped_count = clipped;
  return point;
}

capture::SampleMetricsSnapshot MakeMetrics(double rms, uint16_t minimum,
                                           uint16_t maximum,
                                           uint64_t clipped_low = 0,
                                           uint64_t clipped_high = 0) {
  capture::SampleMetricsSnapshot metrics;
  metrics.sample_count = 1'000'000;
  metrics.recent_rms = rms;
  metrics.recent_minimum_value = minimum;
  metrics.recent_maximum_value = maximum;
  metrics.clipped_low_count = clipped_low;
  metrics.clipped_high_count = clipped_high;
  return metrics;
}

// The static analysis cannot follow gtest's ASSERT_TRUE into the assertion it
// makes, so a dereference after one still reads to it as unchecked. Taking the
// value with value_or keeps the assertion above meaningful and the analysis
// satisfied without scattering suppressions through the file.
AmplitudePoint Recorded(const std::optional<AmplitudePoint>& point) {
  return point.value_or(AmplitudePoint{});
}

TEST(AmplitudeHistoryTest, AnEmptyHistoryHasNothingToSay) {
  const AmplitudeHistory history(10);

  EXPECT_TRUE(history.empty());
  EXPECT_EQ(history.size(), 0u);
  EXPECT_DOUBLE_EQ(history.SpanSeconds(), 0.0);
  EXPECT_EQ(history.PeakCode(), 0);
  EXPECT_EQ(history.TroughCode(), 0);
  EXPECT_EQ(history.TotalClipped(), 0u);
}

TEST(AmplitudeHistoryTest, PointsComeBackOldestFirst) {
  AmplitudeHistory history(10);
  history.Append(MakePoint(1.0, 100.0, 400, 600));
  history.Append(MakePoint(2.0, 200.0, 300, 700));

  ASSERT_EQ(history.size(), 2u);
  EXPECT_DOUBLE_EQ(history.At(0).seconds, 1.0);
  EXPECT_DOUBLE_EQ(history.At(1).seconds, 2.0);
  EXPECT_DOUBLE_EQ(history.Newest().seconds, 2.0);
  EXPECT_DOUBLE_EQ(history.SpanSeconds(), 1.0);
}

TEST(AmplitudeHistoryTest, TheRingDropsTheOldestRatherThanGrowing) {
  AmplitudeHistory history(3);
  for (int index = 0; index < 5; ++index) {
    history.Append(MakePoint(index, 0.0, 0, 0));
  }

  ASSERT_EQ(history.size(), 3u);
  EXPECT_EQ(history.capacity(), 3u);

  // The window slid: 0 and 1 are gone, and what is left is still in order.
  EXPECT_DOUBLE_EQ(history.At(0).seconds, 2.0);
  EXPECT_DOUBLE_EQ(history.At(1).seconds, 3.0);
  EXPECT_DOUBLE_EQ(history.At(2).seconds, 4.0);
}

TEST(AmplitudeHistoryTest, ExtremesFallOffTheBackWithThePointsThatCarriedThem) {
  // Deliberately different from the capture's own extremes, which never come
  // back down. This answers "how is it doing now", and a peak from four minutes
  // ago is not now.
  AmplitudeHistory history(2);
  history.Append(MakePoint(1.0, 0.0, 10, 1000));
  history.Append(MakePoint(2.0, 0.0, 400, 600));

  EXPECT_EQ(history.PeakCode(), 1000);
  EXPECT_EQ(history.TroughCode(), 10);

  history.Append(MakePoint(3.0, 0.0, 450, 550));

  EXPECT_EQ(history.PeakCode(), 600);
  EXPECT_EQ(history.TroughCode(), 400);
}

TEST(AmplitudeHistoryTest, ClipCountsAcrossTheWindowAreSummed) {
  AmplitudeHistory history(10);
  history.Append(MakePoint(1.0, 0.0, 0, 1023, 12));
  history.Append(MakePoint(2.0, 0.0, 0, 1023, 30));

  EXPECT_EQ(history.TotalClipped(), 42u);
}

TEST(AmplitudeHistoryTest, ClearingLeavesItAsItStarted) {
  AmplitudeHistory history(4);
  history.Append(MakePoint(1.0, 100.0, 0, 1023, 5));
  history.Clear();

  EXPECT_TRUE(history.empty());
  EXPECT_EQ(history.TotalClipped(), 0u);
  EXPECT_EQ(history.capacity(), 4u);
}

TEST(AmplitudeHistoryTest, AnIndexPastTheEndIsHarmless) {
  AmplitudeHistory history(4);
  history.Append(MakePoint(1.0, 100.0, 200, 800));

  const AmplitudePoint& missing = history.At(99);
  EXPECT_DOUBLE_EQ(missing.seconds, 0.0);
  EXPECT_EQ(missing.maximum_code, 0);
}

TEST(AmplitudeSamplerTest, NothingIsRecordedUntilAnIntervalHasPassed) {
  AmplitudeSampler sampler(1.0);

  EXPECT_FALSE(sampler.Observe(0.0, MakeMetrics(100.0, 400, 600)).has_value());
  EXPECT_FALSE(sampler.Observe(0.5, MakeMetrics(100.0, 400, 600)).has_value());
  EXPECT_TRUE(sampler.Observe(1.0, MakeMetrics(100.0, 400, 600)).has_value());
}

TEST(AmplitudeSamplerTest, APointCarriesTheExtremesOfTheWholeInterval) {
  // Not of the last update in it. A dropout lasting one buffer is exactly what
  // this panel exists to show, and taking the most recent reading at the
  // interval boundary would miss it four times out of five.
  AmplitudeSampler sampler(1.0);

  sampler.Observe(0.0, MakeMetrics(100.0, 400, 600));
  sampler.Observe(0.3, MakeMetrics(100.0, 100, 900));
  sampler.Observe(0.6, MakeMetrics(100.0, 450, 550));
  const auto point = sampler.Observe(1.0, MakeMetrics(100.0, 480, 520));

  ASSERT_TRUE(point.has_value());
  EXPECT_EQ(Recorded(point).minimum_code, 100);
  EXPECT_EQ(Recorded(point).maximum_code, 900);
}

TEST(AmplitudeSamplerTest, TheRecordedLevelIsTheMeanAcrossTheInterval) {
  AmplitudeSampler sampler(1.0);

  sampler.Observe(0.0, MakeMetrics(100.0, 400, 600));
  sampler.Observe(0.5, MakeMetrics(300.0, 400, 600));
  const auto point = sampler.Observe(1.0, MakeMetrics(200.0, 400, 600));

  ASSERT_TRUE(point.has_value());
  EXPECT_DOUBLE_EQ(Recorded(point).rms_codes, 200.0);
}

TEST(AmplitudeSamplerTest, ClipCountsAreForTheIntervalAndNotTheWholeCapture) {
  // The statistics carry running totals, which would draw a staircase that
  // never comes down. A tick where the clipping happened is what a user is
  // looking for.
  AmplitudeSampler sampler(1.0);

  sampler.Observe(0.0, MakeMetrics(100.0, 0, 1023, 0, 0));
  const auto first = sampler.Observe(1.0, MakeMetrics(100.0, 0, 1023, 10, 5));
  ASSERT_TRUE(first.has_value());

  // The first point has no earlier total to subtract from, so it reports none
  // rather than the whole capture's count as though it had all just happened.
  EXPECT_EQ(Recorded(first).clipped_count, 0u);

  const auto second = sampler.Observe(2.0, MakeMetrics(100.0, 0, 1023, 40, 5));
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(Recorded(second).clipped_count, 30u);

  const auto third = sampler.Observe(3.0, MakeMetrics(100.0, 0, 1023, 40, 5));
  ASSERT_TRUE(third.has_value());
  EXPECT_EQ(Recorded(third).clipped_count, 0u);
}

TEST(AmplitudeSamplerTest, ARunWithNoSamplesYetContributesNothing) {
  // A device that has been opened but has not delivered a buffer would
  // otherwise draw a moment where the signal vanished.
  AmplitudeSampler sampler(0.1);

  capture::SampleMetricsSnapshot empty;
  EXPECT_FALSE(sampler.Observe(0.0, empty).has_value());
  EXPECT_FALSE(sampler.Observe(5.0, empty).has_value());
}

TEST(AmplitudeSamplerTest, AGapDoesNotProduceABurstOfCatchUpPoints) {
  AmplitudeSampler sampler(0.1);

  sampler.Observe(0.0, MakeMetrics(100.0, 400, 600));

  // Ten intervals' worth of silence, then one update. One point, not ten: the
  // display has nothing to draw for a period nothing was measured in.
  const auto point = sampler.Observe(1.0, MakeMetrics(100.0, 400, 600));
  ASSERT_TRUE(point.has_value());
  EXPECT_FALSE(sampler.Observe(1.01, MakeMetrics(100.0, 400, 600)).has_value());
}

TEST(AmplitudeSamplerTest, ResettingForgetsTheRunningTotals) {
  AmplitudeSampler sampler(1.0);

  sampler.Observe(0.0, MakeMetrics(100.0, 400, 600, 100, 100));
  sampler.Observe(1.0, MakeMetrics(100.0, 400, 600, 100, 100));

  sampler.Reset();

  // A new run starts its clock again and has no earlier clip total to compare
  // against, so its first point reports none rather than a negative jump.
  EXPECT_FALSE(sampler.Observe(0.0, MakeMetrics(100.0, 400, 600)).has_value());
  const auto point = sampler.Observe(1.0, MakeMetrics(100.0, 400, 600, 3, 0));
  ASSERT_TRUE(point.has_value());
  EXPECT_EQ(Recorded(point).clipped_count, 0u);
}

TEST(AmplitudeSamplerTest, ARunClockThatRestartsDoesNotBlankTheDisplay) {
  // The interval is tracked as a position on the run's clock, and that clock
  // starts again at zero for every run. If a run ends without the sampler being
  // told, the next one is compared against a deadline from the last — so a
  // twenty-second run is followed by twenty seconds in which every point is
  // suppressed and the panel reads "Nothing recorded yet" over a healthy
  // stream. Time going backwards is the unambiguous signal that this is a new
  // run, and it is the sampler's job to notice rather than to trust that
  // somebody remembered to call Reset.
  AmplitudeSampler sampler(0.1);

  // A twenty-second run, so the deadline ends up out at twenty seconds. Stepped
  // by an integer rather than by accumulating into a double, so the run really
  // is the length this says it is.
  for (int step = 0; step < 200; ++step) {
    sampler.Observe(step * 0.1, MakeMetrics(100.0, 400, 600));
  }

  // The next run, with no Reset between them.
  EXPECT_FALSE(sampler.Observe(0.0, MakeMetrics(100.0, 400, 600)).has_value());

  const auto point = sampler.Observe(0.1, MakeMetrics(100.0, 400, 600));
  ASSERT_TRUE(point.has_value())
      << "the new run was measured against the old run's deadline";

  // Measured on the new clock, not carrying the old run's position with it.
  EXPECT_DOUBLE_EQ(Recorded(point).seconds, 0.1);
}

TEST(AmplitudeSamplerTest, ARestartedRunDoesNotInheritTheOldRunsExtremes) {
  // The partial interval in progress when the clock restarted belongs to the
  // run that has ended. Carried over it would put a level the previous disc
  // reached into the first point of the next one.
  AmplitudeSampler sampler(0.1);

  sampler.Observe(0.0, MakeMetrics(100.0, 0, 1023));
  sampler.Observe(0.05, MakeMetrics(100.0, 0, 1023));

  sampler.Observe(0.0, MakeMetrics(100.0, 500, 520));
  const auto point = sampler.Observe(0.1, MakeMetrics(100.0, 500, 520));
  ASSERT_TRUE(point.has_value());

  EXPECT_EQ(Recorded(point).minimum_code, 500);
  EXPECT_EQ(Recorded(point).maximum_code, 520);
}

TEST(AmplitudeSamplerTest, TheDefaultIntervalFillsTheDefaultRingWithMinutes) {
  // The two defaults are chosen together, and a change to either without the
  // other would quietly shorten or lengthen how much history the panel holds.
  const double seconds = AmplitudeHistory::kDefaultCapacity *
                         AmplitudeSampler::kDefaultIntervalSeconds;
  EXPECT_NEAR(seconds, 300.0, 1e-9);
}

}  // namespace
}  // namespace ddd::analysis
