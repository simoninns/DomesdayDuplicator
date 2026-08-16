/************************************************************************

    test_spectrogram_history.cpp

    The spectrum over time, which is where a drift shows up
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <vector>

#include "sample_format.h"
#include "spectrogram_history.h"
#include "spectrum_analyser.h"

namespace ddd::analysis {
namespace {

std::vector<double> Flat(size_t bins, double level) {
  return std::vector<double>(bins, level);
}

TEST(SpectrogramHistoryTest, AFreshHistoryHoldsNothing) {
  const SpectrogramHistory history(64, 8);

  EXPECT_TRUE(history.empty());
  EXPECT_EQ(history.size(), 0U);
  EXPECT_EQ(history.columns(), 64U);
  EXPECT_EQ(history.rows(), 8U);
}

TEST(SpectrogramHistoryTest, RowsComeBackOldestFirst) {
  SpectrogramHistory history(4, 8);
  history.Append(Flat(16, -30.0), 1.0);
  history.Append(Flat(16, -20.0), 2.0);

  ASSERT_EQ(history.size(), 2U);
  EXPECT_DOUBLE_EQ(history.At(0, 0), -30.0);
  EXPECT_DOUBLE_EQ(history.At(1, 0), -20.0);
}

TEST(SpectrogramHistoryTest, TheRingDropsTheOldestRatherThanGrowing) {
  SpectrogramHistory history(4, 3);
  for (int level = 0; level < 5; ++level) {
    history.Append(Flat(16, -static_cast<double>(level)), level * 0.1);
  }

  ASSERT_EQ(history.size(), 3U);

  // -0 and -1 have fallen off; what is left is still in order.
  EXPECT_DOUBLE_EQ(history.At(0, 0), -2.0);
  EXPECT_DOUBLE_EQ(history.At(1, 0), -3.0);
  EXPECT_DOUBLE_EQ(history.At(2, 0), -4.0);
}

TEST(SpectrogramHistoryTest, AColumnKeepsTheHighestBinItCovers) {
  // The same rule as the spectrum trace, and for the same reason: there are
  // more bins than columns, and a narrow carrier that fell between two sampled
  // bins would simply not be recorded.
  SpectrogramHistory history(4, 4);

  std::vector<double> spectrum = Flat(16, -90.0);
  spectrum[9] = -6.0;  // in the third column of four
  history.Append(spectrum, 0.0);

  ASSERT_EQ(history.size(), 1U);
  EXPECT_DOUBLE_EQ(history.At(0, 2), -6.0);
  EXPECT_DOUBLE_EQ(history.At(0, 0), -90.0);
  EXPECT_DOUBLE_EQ(history.At(0, 3), -90.0);
}

TEST(SpectrogramHistoryTest, EveryBinReachesSomeColumn) {
  // A carrier is one bin wide. Whichever bin it lands in, it has to appear
  // somewhere — a decimation that skipped bins would lose signals silently and
  // only at some frequencies.
  constexpr size_t kBins = 2049;
  constexpr size_t kColumns = 256;

  for (size_t bin = 0; bin < kBins; bin += 7) {
    SpectrogramHistory history(kColumns, 2);

    std::vector<double> spectrum = Flat(kBins, -90.0);
    spectrum[bin] = -3.0;
    history.Append(spectrum, 0.0);

    bool found = false;
    for (size_t column = 0; column < kColumns && !found; ++column) {
      found = history.At(0, column) == -3.0;
    }
    EXPECT_TRUE(found) << "bin " << bin << " reached no column";
  }
}

TEST(SpectrogramHistoryTest, ColumnsSpanTheWholeRangeRegardlessOfTheDisplay) {
  // History is kept to Nyquist whatever the panel is showing, so narrowing the
  // displayed range re-draws what is already held at higher resolution instead
  // of discarding it. A signal at the very top of the range must therefore land
  // in the very last column.
  SpectrogramHistory history(8, 2);

  std::vector<double> spectrum = Flat(64, -90.0);
  spectrum.back() = -10.0;
  history.Append(spectrum, 0.0);

  EXPECT_DOUBLE_EQ(history.At(0, 7), -10.0);
}

TEST(SpectrogramHistoryTest, AnEmptySpectrumIsIgnoredRatherThanRecorded) {
  SpectrogramHistory history(4, 4);
  history.Append({}, 0.0);

  EXPECT_TRUE(history.empty());
}

TEST(SpectrogramHistoryTest, ReadingPastTheEndGivesTheFloor) {
  SpectrogramHistory history(4, 4);
  history.Append(Flat(16, -20.0), 0.0);

  EXPECT_DOUBLE_EQ(history.At(9, 0), SpectrumAnalyser::kFloorDecibels);
  EXPECT_DOUBLE_EQ(history.At(0, 9), SpectrumAnalyser::kFloorDecibels);
}

TEST(SpectrogramHistoryTest, ClearingLeavesItAsItStarted) {
  SpectrogramHistory history(4, 4);
  history.Append(Flat(16, -20.0), 0.0);
  history.Clear();

  EXPECT_TRUE(history.empty());
  EXPECT_DOUBLE_EQ(history.At(0, 0), SpectrumAnalyser::kFloorDecibels);
}

TEST(SpectrogramHistoryTest, TheFrameRateIsMeasuredFromTheFramesThemselves) {
  // Nothing tells the display how often the pipeline publishes a snapshot — it
  // depends on the ring geometry the run was started with — so the time axis
  // has to be derived from when the frames actually arrived.
  SpectrogramHistory history(4, 100);

  for (int frame = 0; frame < 11; ++frame) {
    history.Append(Flat(16, -40.0), frame * 0.1);
  }

  EXPECT_NEAR(history.SpanSeconds(), 1.0, 1e-9);
  EXPECT_NEAR(history.IntervalSeconds(), 0.1, 1e-9);

  // A full ring of 100 rows at a tenth of a second each is ten seconds of
  // display, whether or not it has filled yet: the window is fixed and the
  // picture grows into it.
  EXPECT_NEAR(history.WindowSeconds(), 10.0, 1e-9);
}

TEST(SpectrogramHistoryTest, OneFrameEstablishesNoInterval) {
  // And so nothing can be labelled yet. Better than labelling it with a guess.
  SpectrogramHistory history(4, 10);
  history.Append(Flat(16, -40.0), 5.0);

  EXPECT_DOUBLE_EQ(history.SpanSeconds(), 0.0);
  EXPECT_DOUBLE_EQ(history.IntervalSeconds(), 0.0);
  EXPECT_DOUBLE_EQ(history.WindowSeconds(), 0.0);
  EXPECT_DOUBLE_EQ(history.SecondsAt(0), 5.0);
}

TEST(SpectrogramHistoryTest, TimestampsFallOffWithTheRowsTheyBelongTo) {
  SpectrogramHistory history(4, 3);
  for (int frame = 0; frame < 5; ++frame) {
    history.Append(Flat(16, -40.0), frame * 2.0);
  }

  // Frames at 0 and 2 seconds have gone; 4, 6 and 8 remain.
  EXPECT_DOUBLE_EQ(history.SecondsAt(0), 4.0);
  EXPECT_DOUBLE_EQ(history.SecondsAt(2), 8.0);
  EXPECT_DOUBLE_EQ(history.SpanSeconds(), 4.0);
}

TEST(SpectrogramHistoryTest, ClearingForgetsTheClockAsWellAsTheLevels) {
  SpectrogramHistory history(4, 10);
  history.Append(Flat(16, -20.0), 7.0);
  history.Clear();

  EXPECT_DOUBLE_EQ(history.SecondsAt(0), 0.0);
  EXPECT_DOUBLE_EQ(history.WindowSeconds(), 0.0);
}

TEST(SpectrogramHistoryTest, TheFilterCornerIsInsideWhatTheDisplayShows) {
  // The display shows everything the converter can represent, so the only thing
  // left to check about the board's anti-aliasing filter is that its corner
  // falls inside that — a display that stopped before the corner could not show
  // the filter working, which is a real question somebody asks of a new board.
  EXPECT_LT(kLowPassCornerHz,
            static_cast<double>(capture::kSampleRateHz) / 2.0);
}

}  // namespace
}  // namespace ddd::analysis
