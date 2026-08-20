/************************************************************************

    test_fill_history.cpp

    T1 tests for the buffer fill-level history
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <string>

#include "fill_history.h"

namespace ddd::capture {
namespace {

TEST(FillHistoryTest, StartsEmptyAndSaysSo) {
  const FillHistory history;

  EXPECT_EQ(history.readings(), 0U);
  EXPECT_EQ(history.peak_percent(), 0);
  EXPECT_DOUBLE_EQ(history.mean_percent(), 0.0);
  EXPECT_EQ(history.Describe(), "no readings");
}

TEST(FillHistoryTest, KeepsThePeakAndTheMean) {
  FillHistory history;
  history.AddPercent(10);
  history.AddPercent(30);
  history.AddPercent(20);

  EXPECT_EQ(history.readings(), 3U);
  EXPECT_EQ(history.peak_percent(), 30);
  EXPECT_DOUBLE_EQ(history.mean_percent(), 20.0);
}

// The whole reason this exists beside a peak: two runs with the same peak and
// different amounts of time spent near it are not the same run.
TEST(FillHistoryTest, CountsTheReadingsAtOrAboveEachLevel) {
  FillHistory history;
  for (const int percent : {0, 24, 25, 49, 50, 74, 75, 100}) {
    history.AddPercent(percent);
  }

  EXPECT_EQ(history.readings_at_or_above_quarter(), 6U);
  EXPECT_EQ(history.readings_at_or_above_half(), 4U);
  EXPECT_EQ(history.readings_at_or_above_three_quarters(), 2U);
}

TEST(FillHistoryTest, TreatsEachLevelAsAtOrAboveRatherThanAbove) {
  FillHistory history;
  history.AddPercent(FillHistory::kQuarter);
  history.AddPercent(FillHistory::kHalf);
  history.AddPercent(FillHistory::kThreeQuarters);

  EXPECT_EQ(history.readings_at_or_above_quarter(), 3U);
  EXPECT_EQ(history.readings_at_or_above_half(), 2U);
  EXPECT_EQ(history.readings_at_or_above_three_quarters(), 1U);
}

TEST(FillHistoryTest, WorksOutAPercentageFromALevelAgainstACapacity) {
  FillHistory history;
  history.Add(3, 6);
  EXPECT_EQ(history.peak_percent(), 50);

  history.Add(6, 6);
  EXPECT_EQ(history.peak_percent(), 100);

  // Rounded to nearest: one slot of six is 16.67%.
  FillHistory sixths;
  sixths.Add(1, 6);
  EXPECT_EQ(sixths.peak_percent(), 17);
}

// A capacity of zero is a ring that does not exist yet, not a full one.
TEST(FillHistoryTest, IgnoresAReadingWithNoCapacityBehindIt) {
  FillHistory history;
  history.Add(4, 0);

  EXPECT_EQ(history.readings(), 0U);
  EXPECT_EQ(history.peak_percent(), 0);
}

// A reading is a measurement of hardware. One that comes back over full is
// worth recording as full rather than losing.
TEST(FillHistoryTest, ClampsAReadingOutsideTheScale) {
  FillHistory history;
  history.AddPercent(140);
  history.AddPercent(-10);

  EXPECT_EQ(history.readings(), 2U);
  EXPECT_EQ(history.peak_percent(), 100);
  EXPECT_DOUBLE_EQ(history.mean_percent(), 50.0);
}

TEST(FillHistoryTest, ResetForgetsEverything) {
  FillHistory history;
  history.AddPercent(80);
  history.Reset();

  EXPECT_EQ(history.readings(), 0U);
  EXPECT_EQ(history.peak_percent(), 0);
  EXPECT_EQ(history.readings_at_or_above_three_quarters(), 0U);
}

TEST(FillHistoryTest, DescribesAQuietRunWithoutListingZeroes) {
  FillHistory history;
  history.AddPercent(1);
  history.AddPercent(3);

  const std::string text = history.Describe();
  EXPECT_NE(text.find("mean 2.0%"), std::string::npos) << text;
  EXPECT_NE(text.find("peak 3%"), std::string::npos) << text;
  EXPECT_NE(text.find("never over a quarter full"), std::string::npos) << text;
  EXPECT_EQ(text.find("over half"), std::string::npos) << text;
}

TEST(FillHistoryTest, DescribesARunThatWasBusyAtEveryLevel) {
  FillHistory history;
  history.AddPercent(10);
  history.AddPercent(30);
  history.AddPercent(60);
  history.AddPercent(90);

  const std::string text = history.Describe();
  EXPECT_NE(text.find("peak 90%"), std::string::npos) << text;
  EXPECT_NE(text.find("over a quarter for 3"), std::string::npos) << text;
  EXPECT_NE(text.find("over half for 2"), std::string::npos) << text;
  EXPECT_NE(text.find("over three quarters for 1"), std::string::npos) << text;
}

TEST(FillHistoryTest, StopsDescribingAtTheFirstLevelNothingReached) {
  FillHistory history;
  history.AddPercent(30);

  const std::string text = history.Describe();
  EXPECT_NE(text.find("over a quarter for 1"), std::string::npos) << text;
  EXPECT_NE(text.find("never over half"), std::string::npos) << text;
  EXPECT_EQ(text.find("three quarters"), std::string::npos) << text;
}

}  // namespace
}  // namespace ddd::capture
