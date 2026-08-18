/************************************************************************

    test_firmware_version.cpp

    T1 tests for the firmware version comparison
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "firmware_version.h"

namespace ddd::capture {
namespace {

using Status = FirmwareVersionCheck::Status;

TEST(FirmwareVersionTest, TheCommitIsReadOutOfTheProductString) {
  // The exact string the FX3 firmware builds — see
  // fx3/firmware/src/usb-descriptor.c.
  EXPECT_EQ(ParseFirmwareCommit("Domesday Duplicator (a1b2c3d4)"),
            std::optional<std::string>("a1b2c3d4"));
}

TEST(FirmwareVersionTest, ACommitInCapitalsIsTheSameCommit) {
  EXPECT_EQ(ParseFirmwareCommit("Domesday Duplicator (A1B2C3D4)"),
            std::optional<std::string>("a1b2c3d4"));
}

TEST(FirmwareVersionTest, AProductStringWithNoBracketsCarriesNoCommit) {
  // Firmware older than the version check itself. It has to be recognised as
  // "no commit" rather than misparsed into something that would then be
  // compared and always disagree.
  EXPECT_FALSE(ParseFirmwareCommit("Domesday Duplicator").has_value());
}

TEST(FirmwareVersionTest, SomethingOtherThanAHashIsNotAHash) {
  EXPECT_FALSE(
      ParseFirmwareCommit("Domesday Duplicator (release)").has_value());
  EXPECT_FALSE(
      ParseFirmwareCommit("Domesday Duplicator (2026-01-02)").has_value());
}

// Found by plugging in a real device. The FX3 firmware's CMake asks git for the
// commit exactly as the application's does, so a firmware built from a dirty
// tree reports "bb65470-dirty" in its product descriptor — the string an
// attached development device is reporting right now. Rejecting it as
// unparseable would make every development device raise the "did not report
// which firmware build it is running" warning, on every connection.
TEST(FirmwareVersionTest, ADirtyFirmwareBuildStillNamesItsCommit) {
  EXPECT_EQ(ParseFirmwareCommit("Domesday Duplicator (bb65470-dirty)"),
            std::optional<std::string>("bb65470"));

  const FirmwareVersionCheck check =
      CheckFirmwareVersion("Domesday Duplicator (bb65470-dirty)", "bb65470");
  EXPECT_EQ(check.status, Status::kMatch);
  EXPECT_FALSE(check.ShouldWarn());
}

TEST(FirmwareVersionTest, AShortStringIsNotACommit) {
  EXPECT_FALSE(ParseFirmwareCommit("Domesday Duplicator (abc)").has_value());
}

TEST(FirmwareVersionTest, ADirtyBuildStillNamesTheCommitItStartedFrom) {
  // The local edits are to the application, not to the device, so the commit is
  // still the right thing to compare against the firmware.
  EXPECT_EQ(NormaliseCommit("a1b2c3d4-dirty"),
            std::optional<std::string>("a1b2c3d4"));
}

TEST(FirmwareVersionTest, AnUnknownVersionNamesNoCommit) {
  EXPECT_FALSE(NormaliseCommit("unknown").has_value());
  EXPECT_FALSE(NormaliseCommit("").has_value());
}

TEST(FirmwareVersionTest, MatchingBuildsSayNothing) {
  const FirmwareVersionCheck check =
      CheckFirmwareVersion("Domesday Duplicator (a1b2c3d4)", "a1b2c3d4");

  EXPECT_EQ(check.status, Status::kMatch);
  EXPECT_FALSE(check.ShouldWarn());
  EXPECT_TRUE(check.message.empty());
}

// The failure this test exists to prevent, and the reason the comparison is on
// a prefix rather than the whole string.
//
// The firmware asks git for eight characters. The application's stamp comes
// from whatever built it, and Nix supplies seven. Comparing the strings whole
// would report a mismatch between two artefacts of the same commit — a warning
// that fires when nothing is wrong, which is worse than no warning at all,
// because it teaches the user to dismiss the dialog unread.
TEST(FirmwareVersionTest, StampsOfDifferentLengthsFromOneCommitStillMatch) {
  const FirmwareVersionCheck check =
      CheckFirmwareVersion("Domesday Duplicator (a1b2c3d4)", "a1b2c3d");

  EXPECT_EQ(check.status, Status::kMatch);
  EXPECT_FALSE(check.ShouldWarn());
}

TEST(FirmwareVersionTest, DifferentBuildsWarnAndNameBoth) {
  const FirmwareVersionCheck check =
      CheckFirmwareVersion("Domesday Duplicator (a1b2c3d4)", "99887766");

  EXPECT_EQ(check.status, Status::kMismatch);
  EXPECT_TRUE(check.ShouldWarn());
  EXPECT_EQ(check.device_commit, "a1b2c3d4");
  EXPECT_EQ(check.application_commit, "99887766");

  // A user reading this has to be able to tell which is which without going
  // looking, so both hashes have to be in it.
  EXPECT_NE(check.message.find("a1b2c3d4"), std::string::npos);
  EXPECT_NE(check.message.find("99887766"), std::string::npos);
}

TEST(FirmwareVersionTest, TheWarningDoesNotTellTheUserToStop) {
  const FirmwareVersionCheck check =
      CheckFirmwareVersion("Domesday Duplicator (a1b2c3d4)", "99887766");

  // The whole point of it being a warning rather than an error. Old firmware is
  // not known to be broken, and a user in front of a working capture is better
  // served by a note than by a refusal.
  EXPECT_NE(check.message.find("will work normally"), std::string::npos);
}

TEST(FirmwareVersionTest, FirmwareWithNoCommitWarnsRatherThanFails) {
  const FirmwareVersionCheck check =
      CheckFirmwareVersion("Domesday Duplicator", "a1b2c3d4");

  EXPECT_EQ(check.status, Status::kDeviceUnknown);
  EXPECT_TRUE(check.ShouldWarn());
  EXPECT_FALSE(check.message.empty());
}

// A developer build says nothing, and that is deliberate.
//
// An application that cannot name its own commit is in no position to accuse
// the firmware of anything, and warning here would fire on every single
// developer run.
TEST(FirmwareVersionTest, AnApplicationThatCannotNameItsOwnCommitStaysQuiet) {
  const FirmwareVersionCheck check =
      CheckFirmwareVersion("Domesday Duplicator (a1b2c3d4)", "unknown");

  EXPECT_EQ(check.status, Status::kApplicationUnknown);
  EXPECT_FALSE(check.ShouldWarn());
}

TEST(FirmwareVersionTest, NeitherSideKnownStaysQuiet) {
  const FirmwareVersionCheck check =
      CheckFirmwareVersion("Domesday Duplicator", "unknown");

  EXPECT_EQ(check.status, Status::kApplicationUnknown);
  EXPECT_FALSE(check.ShouldWarn());
}

}  // namespace
}  // namespace ddd::capture
