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

  EXPECT_FALSE(
      DescribeFirmware("Domesday Duplicator (bb65470-dirty)").ShouldWarn());
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

// --- Tolerance to anything named before the bracket ------------------------

// The descriptor names a commit and nothing else, so only the first of these
// is a string the firmware produces. The second is kept deliberately: the
// commit is the *last* bracketed group, so an application built today goes on
// reading a device that ever names something in front of it. That tolerance is
// inherent to the parser rather than an added feature, and it costs nothing to
// have a test saying so.
TEST(FirmwareVersionTest, TheCommitIsReadOutOfEitherFormOfTheString) {
  EXPECT_EQ(ParseFirmwareCommit("Domesday Duplicator (a1b2c3d4)"),
            std::optional<std::string>("a1b2c3d4"));
  EXPECT_EQ(ParseFirmwareCommit("Domesday Duplicator 1.5.0 (a1b2c3d4)"),
            std::optional<std::string>("a1b2c3d4"));
}

// The comparison this file used to make, and why it is gone.
//
// It compared the device's firmware commit against the application's own, on
// the premise that one commit built both. Two release streams — gui-v* for the
// application, fw-v* for the firmware and gateware — make that false, so the
// warning fired on a correctly updated Duplicator. There is no application
// commit in this API any more, and these tests exist to keep it that way.
TEST(FirmwareVersionTest, AFirmwareThatNamesItsBuildIsNotWarnedAbout) {
  const FirmwareIdentity identity =
      DescribeFirmware("Domesday Duplicator (a1b2c3d4)");

  EXPECT_EQ(identity.commit, "a1b2c3d4");
  EXPECT_TRUE(identity.NamesCommit());
  EXPECT_FALSE(identity.ShouldWarn());
  EXPECT_TRUE(identity.message.empty());
}

TEST(FirmwareVersionTest, ADirtyFirmwareIsStillAFirmwareThatNamedItsBuild) {
  const FirmwareIdentity identity =
      DescribeFirmware("Domesday Duplicator (bb65470-dirty)");

  EXPECT_EQ(identity.commit, "bb65470");
  EXPECT_FALSE(identity.ShouldWarn());
}

// The one case left that is worth interrupting somebody for, and the message
// has to name both of its causes: the likely one is a device that could not be
// opened to be asked, not firmware old enough to predate the stamp.
TEST(FirmwareVersionTest, FirmwareWithNoCommitWarnsRatherThanFails) {
  const FirmwareIdentity identity = DescribeFirmware("Domesday Duplicator");

  EXPECT_FALSE(identity.NamesCommit());
  EXPECT_TRUE(identity.ShouldWarn());
  EXPECT_NE(identity.message.find("udev"), std::string::npos)
      << identity.message;
  EXPECT_NE(identity.message.find("older"), std::string::npos)
      << identity.message;
}

// A product string that could not be read at all arrives as an empty one,
// which is the same state and gets the same answer.
TEST(FirmwareVersionTest, AProductStringThatCouldNotBeReadIsTheSameCase) {
  const FirmwareIdentity identity = DescribeFirmware("");

  EXPECT_TRUE(identity.ShouldWarn());
  EXPECT_FALSE(identity.message.empty());
}

// The prefix rule, which survives because the update path still uses it: a
// bundle declares the commit its payload was built from, and the device is
// asked afterwards whether that is what it is running. The firmware asks git
// for eight characters and a Nix build passes seven, so two artefacts of one
// commit must never be reported as differing.
TEST(FirmwareVersionTest, StampsOfDifferentLengthsFromOneCommitStillMatch) {
  EXPECT_TRUE(CommitsMatch("a1b2c3d4", "a1b2c3d"));
  EXPECT_TRUE(CommitsMatch("a1b2c3d4-dirty", "a1b2c3d4"));
  EXPECT_FALSE(CommitsMatch("a1b2c3d4", "99887766"));
}

// Nothing compares equal to a stamp that names no commit, including another
// one that names no commit. Two devices that both failed to say what they are
// running have not been shown to agree.
TEST(FirmwareVersionTest, AStampThatNamesNoCommitMatchesNothing) {
  EXPECT_FALSE(CommitsMatch("unknown", "unknown"));
  EXPECT_FALSE(CommitsMatch("", ""));
  EXPECT_FALSE(CommitsMatch("a1b2c3d4", "unknown"));
}

}  // namespace
}  // namespace ddd::capture
