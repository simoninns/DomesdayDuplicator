/************************************************************************

    test_firmware_text.cpp

    What the Firmware dialog says about the three versions
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QString>

#include "firmware_text.h"
#include "wire_protocol.h"

namespace ddd::gui {
namespace {

// A device running firmware and gateware built from the same commit as this
// hypothetical application.
FirmwareVersions MatchedSet(const QString& commit) {
  FirmwareVersions versions;
  versions.application = commit;
  versions.device_attached = true;
  versions.product_string =
      QStringLiteral("Domesday Duplicator (%1)").arg(commit);
  versions.gateware.present = true;
  versions.gateware.map_version = capture::kIdentityMapVersion;
  versions.gateware.commit = commit.toStdString();
  return versions;
}

TEST(FirmwareTextTest, AllThreeVersionsAreNamed) {
  const QString text = FirmwareText(MatchedSet(QStringLiteral("7713495d")));

  EXPECT_TRUE(text.contains(QStringLiteral("Application")))
      << "the application's own build is not labelled";
  EXPECT_TRUE(text.contains(QStringLiteral("FX3 firmware")))
      << "the firmware's build is not labelled";
  EXPECT_TRUE(text.contains(QStringLiteral("FPGA gateware")))
      << "the gateware's build is not labelled";
  EXPECT_TRUE(text.contains(QStringLiteral("7713495d")))
      << "the commit is not shown anywhere";
}

TEST(FirmwareTextTest, AMatchedSetSaysSo) {
  const QString text = FirmwareText(MatchedSet(QStringLiteral("7713495d")));

  EXPECT_TRUE(text.contains(QStringLiteral("same commit")))
      << "a matched set is not reported as matching";
}

TEST(FirmwareTextTest, TwoLengthsOfTheSameCommitStillMatch) {
  // The one thing this dialog must never do. A Nix build stamps seven
  // characters and CMake eight, so an application and a device built from one
  // commit routinely disagree in length — and a mismatch warning that fires
  // when nothing is wrong teaches people to ignore it.
  FirmwareVersions versions = MatchedSet(QStringLiteral("7713495d"));
  versions.application = QStringLiteral("7713495");

  const QString text = FirmwareText(versions);

  EXPECT_TRUE(text.contains(QStringLiteral("same commit")))
      << "two lengths of one commit were reported as differing";
}

TEST(FirmwareTextTest, ADirtyApplicationBuildStillMatches) {
  // Local edits to the application say nothing about the device.
  FirmwareVersions versions = MatchedSet(QStringLiteral("7713495d"));
  versions.application = QStringLiteral("7713495d-dirty");

  EXPECT_TRUE(FirmwareText(versions).contains(QStringLiteral("same commit")))
      << "a dirty application build was reported as a version mismatch";
}

TEST(FirmwareTextTest, AGenuineMismatchIsReportedWithoutAlarm) {
  FirmwareVersions versions = MatchedSet(QStringLiteral("7713495d"));
  versions.gateware.commit = "0123abcd";

  const QString text = FirmwareText(versions);

  EXPECT_TRUE(text.contains(QStringLiteral("not all built from the same")))
      << "a real mismatch was not reported";

  // Old gateware is not known to be broken, only untested with this build, and
  // the text must not tell somebody in front of a working capture to stop.
  EXPECT_TRUE(text.contains(QStringLiteral("work normally")))
      << "the mismatch text does not say that capture still works";
}

TEST(FirmwareTextTest, NoDeviceIsStatedRatherThanReportedAsMissingVersions) {
  FirmwareVersions versions;
  versions.application = QStringLiteral("7713495d");
  versions.device_attached = false;

  const QString text = FirmwareText(versions);

  EXPECT_TRUE(text.contains(QStringLiteral("No device attached")))
      << "an absent device is not named as the reason the versions are unknown";
  EXPECT_FALSE(text.contains(QStringLiteral("not all built from the same")))
      << "an absent device was reported as a version mismatch";
}

TEST(FirmwareTextTest, AGatewareThatDidNotAnswerSaysWhatToLookAt) {
  FirmwareVersions versions = MatchedSet(QStringLiteral("7713495d"));
  versions.gateware = capture::FpgaVersion{};

  const QString text = FirmwareText(versions);

  EXPECT_TRUE(text.contains(QStringLiteral("Not reported")))
      << "a silent FPGA is not reported";
  EXPECT_TRUE(text.contains(QStringLiteral("not be programmed")))
      << "the text does not suggest what a silent FPGA might mean";
  EXPECT_FALSE(text.contains(QStringLiteral("not all built from the same")))
      << "an unknown version was reported as a mismatch";
}

TEST(FirmwareTextTest, FirmwarePredatingTheVersionStampIsExplained) {
  FirmwareVersions versions = MatchedSet(QStringLiteral("7713495d"));
  versions.product_string = QStringLiteral("Domesday Duplicator");

  const QString text = FirmwareText(versions);

  EXPECT_TRUE(text.contains(QStringLiteral("older")))
      << "firmware with no version stamp is not explained";
  EXPECT_FALSE(text.contains(QStringLiteral("not all built from the same")))
      << "an unknown version was reported as a mismatch";
}

TEST(FirmwareTextTest, AnApplicationThatCannotNameItsBuildAccusesNothing) {
  // A developer build with no git available has nothing to compare against,
  // and saying "your device is wrong" on that basis would be an accusation it
  // is not entitled to make.
  FirmwareVersions versions = MatchedSet(QStringLiteral("7713495d"));
  versions.application = QStringLiteral("unknown");

  const QString text = FirmwareText(versions);

  EXPECT_TRUE(text.contains(QStringLiteral("cannot name the commit")))
      << "an unknown application build is not explained";
  EXPECT_FALSE(text.contains(QStringLiteral("not all built from the same")))
      << "an unknown application build was turned into an accusation";
}

TEST(FirmwareTextTest, AModifiedGatewareIsMarked) {
  FirmwareVersions versions = MatchedSet(QStringLiteral("7713495d"));
  versions.gateware.dirty = true;

  EXPECT_TRUE(FirmwareText(versions).contains(QStringLiteral("modified")))
      << "a gateware built from a modified tree is not marked as such";
}

TEST(FirmwareTextTest, ANewerRegisterMapIsMentioned) {
  FirmwareVersions versions = MatchedSet(QStringLiteral("7713495d"));
  versions.gateware.map_version = capture::kIdentityMapVersion + 1;

  EXPECT_TRUE(
      FirmwareText(versions).contains(QStringLiteral("newer register map")))
      << "gateware implementing a map this build does not know is not "
         "mentioned";
}

}  // namespace
}  // namespace ddd::gui
