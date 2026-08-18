/************************************************************************

    test_waveform_mapping.cpp

    Where a sample lands on the screen, and what a screen position means
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <vector>

#include "monitor_tap.h"
#include "sample_format.h"
#include "waveform_mapping.h"

namespace ddd::analysis {
namespace {

WaveformMapping MakeMapping(int width = 600, int height = 400, size_t first = 0,
                            size_t span = 1200) {
  WaveformMapping mapping;
  mapping.width_pixels = width;
  mapping.height_pixels = height;
  mapping.first_sample = first;
  mapping.sample_span = span;
  return mapping;
}

TEST(WaveformMappingTest, TheWindowFillsThePlotExactly) {
  const WaveformMapping mapping = MakeMapping();

  EXPECT_DOUBLE_EQ(mapping.SampleToX(0), 0.0);
  EXPECT_DOUBLE_EQ(mapping.SampleToX(1200), 600.0);
  EXPECT_DOUBLE_EQ(mapping.SampleToX(600), 300.0);
}

TEST(WaveformMappingTest, TheFullCodeRangeFillsThePlotWithTheLargestAtTheTop) {
  const WaveformMapping mapping = MakeMapping();

  EXPECT_DOUBLE_EQ(mapping.CodeToY(1023.0), 0.0);
  EXPECT_DOUBLE_EQ(mapping.CodeToY(0.0), 400.0);

  // Mid-scale sits half a code above the centre line, because 512 is not the
  // midpoint of 0..1023. Asserted rather than rounded away so that a change to
  // the convention has to be deliberate.
  EXPECT_NEAR(mapping.CodeToY(511.5), 200.0, 1e-9);
}

TEST(WaveformMappingTest, TheCursorReadsBackTheSampleUnderIt) {
  const WaveformMapping mapping = MakeMapping();

  // Two samples per pixel here, so the exact inverse is impossible: what has to
  // hold is that the sample reported for a position is one of the samples drawn
  // at it.
  for (size_t sample = 0; sample < 1200; sample += 7) {
    const double x = mapping.SampleToX(static_cast<double>(sample));
    const size_t recovered = mapping.XToSample(x);
    EXPECT_LE(recovered, sample);
    EXPECT_LT(sample - recovered, 2u) << "sample " << sample;
  }
}

TEST(WaveformMappingTest, TheCursorReadsBackTheCodeUnderIt) {
  const WaveformMapping mapping = MakeMapping();

  // Stepped by an index rather than by accumulating the code itself, so that
  // the value under test is exact at every step and a round-trip failure can
  // only be the mapping's.
  for (int step = 0; step * 37 <= 1023; ++step) {
    const double code = step * 37;
    EXPECT_NEAR(mapping.YToCode(mapping.CodeToY(code)), code, 1e-9);
  }
}

TEST(WaveformMappingTest, ACursorOffTheEndsIsClampedToTheWindow) {
  const WaveformMapping mapping = MakeMapping(600, 400, 5000, 1200);

  EXPECT_EQ(mapping.XToSample(-50.0), 5000u);
  EXPECT_EQ(mapping.XToSample(0.0), 5000u);
  EXPECT_EQ(mapping.XToSample(10'000.0), 5000u + 1200u - 1u);
}

TEST(WaveformMappingTest, AWindowStartingPartwayThroughIsOffsetNotRescaled) {
  const WaveformMapping mapping = MakeMapping(600, 400, 5000, 1200);

  EXPECT_DOUBLE_EQ(mapping.SampleToX(5000), 0.0);
  EXPECT_DOUBLE_EQ(mapping.SampleToX(6200), 600.0);
  EXPECT_EQ(mapping.XToSample(300.0), 5600u);
}

TEST(WaveformMappingTest, ZoomingInSpreadsTheSameSamplesFurtherApart) {
  const WaveformMapping wide = MakeMapping(600, 400, 0, 1200);
  const WaveformMapping narrow = MakeMapping(600, 400, 0, 300);

  EXPECT_LT(wide.SampleToX(100), narrow.SampleToX(100));
  EXPECT_DOUBLE_EQ(narrow.SampleToX(300), 600.0);
}

TEST(WaveformMappingTest, TimeIsMeasuredFromTheLeftEdgeOfTheWindow) {
  const WaveformMapping mapping = MakeMapping(600, 400, 5000, 40'000);

  EXPECT_DOUBLE_EQ(mapping.SampleToSeconds(5000, capture::kSampleRateHz), 0.0);

  // 40,000 samples at 40 Msps is exactly a millisecond, which is what makes
  // this span the readable one on screen.
  EXPECT_NEAR(mapping.SampleToSeconds(45'000, capture::kSampleRateHz), 0.001,
              1e-12);
}

TEST(WaveformMappingTest, AnEmptyPlotIsNotValidAndMapsNothing) {
  WaveformMapping mapping;
  EXPECT_FALSE(mapping.Valid());
  EXPECT_DOUBLE_EQ(mapping.SampleToX(100), 0.0);
  EXPECT_DOUBLE_EQ(mapping.CodeToY(500), 0.0);

  mapping = MakeMapping(600, 400, 0, 0);
  EXPECT_FALSE(mapping.Valid());
}

TEST(WaveformMappingTest, ColumnsKeepTheExtremesOfWhatTheyCover) {
  // The property the display depends on. A 5 MHz carrier at 40 Msps is eight
  // samples a cycle, so at any useful span a column covers whole cycles: one
  // sample per column would draw whatever phase it happened to land on, and the
  // envelope would appear to wander.
  const WaveformMapping mapping = MakeMapping(10, 400, 0, 100);

  std::vector<uint16_t> codes(100);
  for (size_t index = 0; index < codes.size(); ++index) {
    codes[index] = static_cast<uint16_t>((index % 10) * 100);
  }

  std::vector<WaveformColumn> columns;
  DecimateToColumns(codes.data(), codes.size(), mapping, columns);

  ASSERT_EQ(columns.size(), 10u);
  for (const WaveformColumn& column : columns) {
    EXPECT_TRUE(column.populated);
    EXPECT_EQ(column.minimum, 0);
    EXPECT_EQ(column.maximum, 900);
  }
}

TEST(WaveformMappingTest, AColumnWithNoSamplesIsLeftEmptyRatherThanInvented) {
  // Zoomed in past one sample per pixel. Drawing something between the samples
  // would be drawing a signal nobody measured.
  const WaveformMapping mapping = MakeMapping(100, 400, 0, 10);

  std::vector<uint16_t> codes(10, 500);

  std::vector<WaveformColumn> columns;
  DecimateToColumns(codes.data(), codes.size(), mapping, columns);

  ASSERT_EQ(columns.size(), 100u);

  size_t populated = 0;
  for (const WaveformColumn& column : columns) {
    if (column.populated) {
      ++populated;
      EXPECT_EQ(column.minimum, 500);
    }
  }
  EXPECT_EQ(populated, 10u);
}

TEST(WaveformMappingTest, AWindowRunningPastTheDataStopsAtTheData) {
  const WaveformMapping mapping = MakeMapping(50, 400, 0, 500);

  std::vector<uint16_t> codes(120, 700);

  std::vector<WaveformColumn> columns;
  DecimateToColumns(codes.data(), codes.size(), mapping, columns);

  ASSERT_EQ(columns.size(), 50u);

  // 120 samples across a 500-sample window is the leftmost 12 columns.
  for (size_t index = 0; index < columns.size(); ++index) {
    EXPECT_EQ(columns[index].populated, index < 12) << "column " << index;
  }
}

TEST(WaveformMappingTest, NoDataProducesNoColumns) {
  const WaveformMapping mapping = MakeMapping();
  std::vector<WaveformColumn> columns;

  DecimateToColumns(nullptr, 0, mapping, columns);
  EXPECT_EQ(columns.size(), 600u);
  for (const WaveformColumn& column : columns) {
    EXPECT_FALSE(column.populated);
  }

  const WaveformMapping invalid;
  DecimateToColumns(nullptr, 0, invalid, columns);
  EXPECT_TRUE(columns.empty());
}

// The spans on offer, pinned. Two properties rather than one: they are the
// intended figures, and none of them asks for more samples than a snapshot
// holds — a span that did would be silently clamped and would show less time
// than its own label claimed, which is the quietest kind of wrong a display can
// be.
TEST(WaveformMappingTest, TheOfferedSpansAreMicrosecondsAndAllFitInASnapshot) {
  // In nanoseconds, because the short end of the ladder is where the point of
  // it is and those spans are fractions of a microsecond.
  const size_t expected_nanoseconds[] = {
      500, 1'000, 2'000, 5'000, 10'000, 50'000, 100'000, 200'000, 500'000};

  ASSERT_EQ(kWaveformSpanChoiceCount,
            sizeof(expected_nanoseconds) / sizeof(expected_nanoseconds[0]));

  for (size_t index = 0; index < kWaveformSpanChoiceCount; ++index) {
    const size_t samples = kWaveformSpanChoices[index];

    EXPECT_EQ(samples * 1'000'000'000 / capture::kSampleRateHz,
              expected_nanoseconds[index]);

    EXPECT_LE(samples, capture::SnapshotPublisher::kDefaultSnapshotBytes /
                           capture::kBytesPerSample)
        << "span " << samples << " is longer than a snapshot";
  }
}

TEST(WaveformMappingTest, TheDefaultSpanShowsAFewCyclesOfTheCarrier) {
  // The whole reason the ladder was extended downwards. An 8 MHz carrier is
  // five samples a cycle at 40 Msps; the display has to open on a span where
  // those cycles are individually visible rather than on one showing eighty of
  // them, which is a band of fuzz whatever else is right about the drawing.
  ASSERT_LT(kDefaultWaveformSpanIndex, kWaveformSpanChoiceCount);

  const double samples =
      static_cast<double>(kWaveformSpanChoices[kDefaultWaveformSpanIndex]);
  const double cycles = samples / 5.0;

  EXPECT_GE(cycles, 4.0);
  EXPECT_LE(cycles, 16.0) << "the default span shows " << cycles << " cycles";
}

TEST(WaveformMappingTest, TheDrawingStyleFollowsHowCrowdedThePixelsAre) {
  // One rule cannot serve a ladder that runs from thirty-three samples in a
  // pixel to fifteen pixels between samples.
  WaveformMapping mapping = MakeMapping(600, 400, 0, 20'000);
  EXPECT_EQ(mapping.DrawStyle(), WaveformDrawStyle::kEnvelope);
  EXPECT_NEAR(mapping.SamplesPerPixel(), 33.33, 0.01);
  EXPECT_FALSE(mapping.ShouldMarkSamples());

  // Right at the changeover, and just under it.
  mapping.sample_span = 1200;
  EXPECT_EQ(mapping.DrawStyle(), WaveformDrawStyle::kEnvelope);
  mapping.sample_span = 900;
  EXPECT_EQ(mapping.DrawStyle(), WaveformDrawStyle::kPolyline);
  mapping.sample_span = 600;
  EXPECT_EQ(mapping.DrawStyle(), WaveformDrawStyle::kPolyline);
  mapping.sample_span = 500;
  EXPECT_EQ(mapping.DrawStyle(), WaveformDrawStyle::kReconstructed);

  // The default span: fifteen pixels a sample, so the samples are worth
  // marking and the waveform between them has to be reconstructed.
  mapping.sample_span = kWaveformSpanChoices[kDefaultWaveformSpanIndex];
  EXPECT_EQ(mapping.DrawStyle(), WaveformDrawStyle::kReconstructed);
  EXPECT_TRUE(mapping.ShouldMarkSamples());
}

TEST(WaveformMappingTest, ATriggerBetweenSamplesShiftsTheWholeWindow) {
  // A trigger lands between two samples and the window has to start there, not
  // at the nearest one: at five samples a cycle, rounding is a fifth of a cycle
  // of jitter and most of the shimmer the trigger exists to remove.
  WaveformMapping mapping = MakeMapping(600, 400, 100, 600);

  const double before = mapping.SampleToX(400);
  mapping.sub_sample_offset = 0.5;
  const double after = mapping.SampleToX(400);

  // Half a sample is half a pixel here, and the trace moves left as the window
  // moves right.
  EXPECT_NEAR(before - after, 0.5, 1e-9);

  // The window's own start is still the left-hand edge.
  EXPECT_NEAR(mapping.SampleToX(100.5), 0.0, 1e-9);
}

TEST(WaveformMappingTest, APixelMapsToAFractionalSamplePosition) {
  // What a reconstructed trace asks for: at a span narrower than the plot is
  // wide every pixel falls between samples, and rounding to one of them is the
  // aliasing the reconstruction exists to avoid.
  WaveformMapping mapping = MakeMapping(600, 400, 0, 40);
  mapping.sub_sample_offset = 0.25;

  EXPECT_NEAR(mapping.XToSamplePosition(0.0), 0.25, 1e-9);
  EXPECT_NEAR(mapping.XToSamplePosition(600.0), 40.25, 1e-9);
  EXPECT_NEAR(mapping.XToSamplePosition(15.0), 1.25, 1e-9);

  // And it is the exact inverse of SampleToX, which is the property the trace
  // and the cursor both depend on.
  //
  // Stepped by an integer and multiplied out rather than by accumulating the
  // step into a double: repeated addition drifts, so the last position tested
  // would not be the one this reads as, and how many get tested at all would
  // depend on the rounding. That is also what cert-flp30-c is for.
  constexpr double kFirstPosition = 0.25;
  constexpr double kPositionStep = 0.7;
  for (int step = 0; kFirstPosition + (step * kPositionStep) < 40.0; ++step) {
    const double position = kFirstPosition + (step * kPositionStep);
    EXPECT_NEAR(mapping.XToSamplePosition(mapping.SampleToX(position)),
                position, 1e-9);
  }
}

}  // namespace
}  // namespace ddd::analysis
