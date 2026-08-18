/************************************************************************

    test_free_space.cpp

    T1 tests for how much longer the disk will last
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <filesystem>

#include "free_space.h"
#include "sample_format.h"

namespace ddd::capture {
namespace {

TEST(FreeSpaceTest, AnHourOfCaptureIsAboutAHundredAndFortyGigabytes) {
  // The figure a user is deciding on. If this is wrong, everything the panel
  // says about whether a disc side will fit is wrong with it.
  const uint64_t hour = CaptureBytesForSeconds(3600.0);
  EXPECT_NEAR(static_cast<double>(hour), 144.0e9, 1.0e9);
}

TEST(FreeSpaceTest, TheEstimateIsWellUnderTheWireRate) {
  // FLAC roughly halves the stream. An estimate at or above the wire rate would
  // be no estimate at all, and one far below it would refuse captures that fit.
  EXPECT_LT(kEstimatedCaptureBytesPerSecond,
            static_cast<double>(kWireBytesPerSecond));
  EXPECT_GT(kEstimatedCaptureBytesPerSecond,
            static_cast<double>(kWireBytesPerSecond) / 4.0);
}

TEST(FreeSpaceTest, TimeAndSizeAreTheSameStatementBothWaysRound) {
  const uint64_t bytes = CaptureBytesForSeconds(1800.0);
  EXPECT_NEAR(CaptureSecondsRemaining(bytes), 1800.0, 1.0);
}

TEST(FreeSpaceTest, AnEmptyVolumeLeavesNoTime) {
  EXPECT_DOUBLE_EQ(CaptureSecondsRemaining(0), 0.0);
  EXPECT_EQ(CaptureBytesForSeconds(0.0), 0U);
  EXPECT_EQ(CaptureBytesForSeconds(-5.0), 0U);
}

TEST(FreeSpaceTest, ANonsensicalRateIsRefusedRatherThanDividedBy) {
  EXPECT_DOUBLE_EQ(CaptureSecondsRemaining(1'000'000, 0.0), 0.0);
  EXPECT_DOUBLE_EQ(CaptureSecondsRemaining(1'000'000, -1.0), 0.0);
}

TEST(FreeSpaceTest, ARealDirectoryReportsRealSpace) {
  const FreeSpace space =
      AvailableSpace(std::filesystem::temp_directory_path());
  ASSERT_TRUE(space.known);
  EXPECT_GT(space.bytes_available, 0U);
}

// Unknown, and specifically not zero. Zero reads as "the disk is full" and
// would stop somebody capturing to a directory they were about to create.
TEST(FreeSpaceTest, ADirectoryThatIsNotThereIsUnknownRatherThanFull) {
  const FreeSpace space = AvailableSpace(
      std::filesystem::temp_directory_path() / "ddd-no-such-directory-here");
  EXPECT_FALSE(space.known);
}

TEST(FreeSpaceTest, NoDirectoryAtAllIsUnknownRatherThanAnError) {
  const FreeSpace space = AvailableSpace({});
  EXPECT_FALSE(space.known);
  EXPECT_EQ(space.bytes_available, 0U);
}

}  // namespace
}  // namespace ddd::capture
