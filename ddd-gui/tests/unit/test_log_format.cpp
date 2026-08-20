/************************************************************************

    test_log_format.cpp

    T1 tests for the log's number formatting
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include "log_format.h"
#include "sample_format.h"

namespace ddd::capture {
namespace {

TEST(FormatDecimalTest, WritesTheRequestedNumberOfPlaces) {
  EXPECT_EQ(FormatDecimal(1.24, 1), "1.2");
  EXPECT_EQ(FormatDecimal(1.26, 1), "1.3");
  EXPECT_EQ(FormatDecimal(1.25, 3), "1.250");
  EXPECT_EQ(FormatDecimal(0.0, 2), "0.00");
}

// Halfway rounds to even, because the C library does and nothing here overrides
// it. Stated as a test rather than left to be discovered: a figure in a log is
// read, not computed with, and the difference between 1.2 and 1.3 on a line
// about back pressure has never mattered — but a test that assumed the other
// rule would fail on the first tie somebody wrote.
TEST(FormatDecimalTest, RoundsAHalfwayValueToEven) {
  EXPECT_EQ(FormatDecimal(1.25, 1), "1.2");
  EXPECT_EQ(FormatDecimal(-2.5, 0), "-2");
}

// A log file has one spelling of a number whatever the machine's locale asks
// for. This is the guard on that, and it is why the value is not simply handed
// to std::to_string.
TEST(FormatDecimalTest, AlwaysUsesAFullStopAsTheSeparator) {
  const std::string text = FormatDecimal(3.5, 1);
  EXPECT_EQ(text, "3.5");
  EXPECT_EQ(text.find(','), std::string::npos);
}

// Arithmetic nobody checked should not look like a measurement a week later.
TEST(FormatDecimalTest, WritesSomethingThatIsNotANumberAsZero) {
  EXPECT_EQ(FormatDecimal(std::nan(""), 1), "0.0");
  EXPECT_EQ(FormatDecimal(1.0 / 0.0, 1), "0.0");
}

TEST(FormatBytesTest, ClimbsThroughTheBinaryUnits) {
  EXPECT_EQ(FormatBytes(0), "0 B");
  EXPECT_EQ(FormatBytes(512), "512 B");
  EXPECT_EQ(FormatBytes(uint64_t{2} << 10), "2.0 KiB");
  EXPECT_EQ(FormatBytes(uint64_t{2} << 20), "2.0 MiB");
  EXPECT_EQ(FormatBytes(uint64_t{256} << 20), "256.0 MiB");
  EXPECT_EQ(FormatBytes(uint64_t{3} << 30), "3.00 GiB");
}

// Binary rather than decimal, because every size in the engine is a buffer or
// a multiple of one: a 256 MiB ring must not read as 268 MB.
TEST(FormatBytesTest, IsBinaryAndNotDecimal) {
  EXPECT_EQ(FormatBytes(268'435'456), "256.0 MiB");
}

TEST(FormatDurationTest, UsesTheFormThatCarriesMeaningAtThatLength) {
  EXPECT_EQ(FormatDuration(0.0), "0 ms");
  EXPECT_EQ(FormatDuration(0.412), "412 ms");
  EXPECT_EQ(FormatDuration(3.2408), "3.24 s");
  EXPECT_EQ(FormatDuration(59.99), "59.99 s");
  EXPECT_EQ(FormatDuration(247.0), "4 m 07 s");
  EXPECT_EQ(FormatDuration(4324.0), "1 h 12 m 04 s");
}

// A clock reads as a clock only when its fields are two digits: "1 h 12 m 4 s"
// is three unrelated numbers.
TEST(FormatDurationTest, PadsTheMinutesAndSecondsOfALongerDuration) {
  EXPECT_EQ(FormatDuration(3661.0), "1 h 01 m 01 s");
}

TEST(FormatDurationTest, RefusesToReportNonsenseAsTime) {
  EXPECT_EQ(FormatDuration(-5.0), "0 ms");
  EXPECT_EQ(FormatDuration(std::nan("")), "0 ms");
}

TEST(FormatSampleDurationTest, TurnsACountIntoTheStreamItStandsFor) {
  // One second of the converter's own rate.
  EXPECT_EQ(FormatSampleDuration(kSampleRateHz, kSampleRateHz), "1.00 s");

  // The same count at half the rate is twice the stream, which is the whole
  // reason the rate is passed in rather than assumed.
  EXPECT_EQ(FormatSampleDuration(kSampleRateHz, kSampleRateHz / 2), "2.00 s");
}

TEST(FormatSampleDurationTest, SaysNothingRatherThanGuessWithNoRate) {
  EXPECT_EQ(FormatSampleDuration(1'000'000, 0), "0 ms");
}

}  // namespace
}  // namespace ddd::capture
