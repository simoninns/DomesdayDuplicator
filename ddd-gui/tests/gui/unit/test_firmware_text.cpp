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

// A device whose firmware and gateware were built from one commit — which is
// what a device update installs — beside a hypothetical application release.
//
// The application's stamp is deliberately unrelated to the device's commit.
// The two come from separate release streams and are not expected to match;
// a fixture that made them match would let a comparison between them creep
// back in unnoticed.
FirmwareVersions MatchedSet(const QString& commit) {
  FirmwareVersions versions;
  versions.application = QStringLiteral("1.2.0 (feedface)");
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

TEST(FirmwareTextTest, ADeviceWhoseHalvesMatchSaysSo) {
  const QString text = FirmwareText(MatchedSet(QStringLiteral("7713495d")));

  EXPECT_TRUE(text.contains(QStringLiteral("same build")))
      << "a device installed from one update is not reported as matching";
}

// The comparison this dialog used to make and must never make again. The
// application releases under gui-v* and the device under fw-v*, so an
// application and a firmware from different commits is what a fully up-to-date
// Duplicator looks like. Reporting it as a mismatched set is what sent a user
// looking for a fault that was not there.
TEST(FirmwareTextTest, TheApplicationIsNotComparedAgainstTheDevice) {
  FirmwareVersions versions = MatchedSet(QStringLiteral("7713495d"));
  versions.application = QStringLiteral("9.9.9 (0123abcd)");

  const QString text = FirmwareText(versions);

  EXPECT_TRUE(text.contains(QStringLiteral("same build")))
      << "an application from another release was counted as a mismatch";
  EXPECT_TRUE(text.contains(QStringLiteral("released separately")))
      << "the text does not say why the application need not match";
}

// A device whose two halves disagree is a real thing to report: they are
// installed together from one bundle, so a difference means an update that did
// not finish or a half programmed by hand.
TEST(FirmwareTextTest, ADeviceWhoseHalvesDifferIsReportedWithoutAlarm) {
  FirmwareVersions versions = MatchedSet(QStringLiteral("7713495d"));
  versions.gateware.commit = "0123abcd";

  const QString text = FirmwareText(versions);

  EXPECT_TRUE(text.contains(QStringLiteral("different builds")))
      << "a half-updated device was not reported";

  // Not known to be broken, only mismatched, and the text must not tell
  // somebody in front of a working capture to stop.
  EXPECT_TRUE(text.contains(QStringLiteral("work normally")))
      << "the mismatch text does not say that capture still works";
}

// Two lengths of one commit are one commit. A Nix build stamps seven
// characters and CMake eight, so the device's two halves routinely disagree in
// length — and a warning that fires when nothing is wrong teaches people to
// ignore it.
TEST(FirmwareTextTest, TwoLengthsOfTheSameCommitStillMatch) {
  FirmwareVersions versions = MatchedSet(QStringLiteral("7713495d"));
  versions.gateware.commit = "7713495";

  EXPECT_TRUE(FirmwareText(versions).contains(QStringLiteral("same build")))
      << "two lengths of one commit were reported as differing";
}

TEST(FirmwareTextTest, NoDeviceIsStatedRatherThanReportedAsMissingVersions) {
  FirmwareVersions versions;
  versions.application = QStringLiteral("7713495d");
  versions.device_attached = false;

  const QString text = FirmwareText(versions);

  EXPECT_TRUE(text.contains(QStringLiteral("No device attached")))
      << "an absent device is not named as the reason the versions are unknown";
  EXPECT_FALSE(text.contains(QStringLiteral("different builds")))
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
  EXPECT_FALSE(text.contains(QStringLiteral("different builds")))
      << "an unknown version was reported as a mismatch";
}

TEST(FirmwareTextTest, FirmwarePredatingTheVersionStampIsExplained) {
  FirmwareVersions versions = MatchedSet(QStringLiteral("7713495d"));
  versions.product_string = QStringLiteral("Domesday Duplicator");

  const QString text = FirmwareText(versions);

  EXPECT_TRUE(text.contains(QStringLiteral("older")))
      << "firmware with no version stamp is not explained";
  EXPECT_FALSE(text.contains(QStringLiteral("different builds")))
      << "an unknown version was reported as a mismatch";
}

// A build that cannot name itself says so in its own row and nothing more. It
// has no bearing on whether the device's two halves agree, which is now the
// only question this paragraph answers.
TEST(FirmwareTextTest, AnApplicationThatCannotNameItsBuildAccusesNothing) {
  FirmwareVersions versions = MatchedSet(QStringLiteral("7713495d"));
  versions.application = QStringLiteral("unknown");

  const QString text = FirmwareText(versions);

  EXPECT_TRUE(text.contains(QStringLiteral("same build")))
      << "an unknown application build stopped the device being compared";
  EXPECT_FALSE(text.contains(QStringLiteral("different builds")))
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

// --- A device with no firmware ---------------------------------------------

// Every "not reported" explanation in this dialog is written for a device that
// answered and said something unhelpful. None of them is the right
// explanation for a device that is not running anything at all, and the wrong
// explanation is the one a user would act on.
TEST(FirmwareTextTest, ADeviceInRecoveryIsExplainedRatherThanDiagnosed) {
  FirmwareVersions versions;
  versions.application = QStringLiteral("7713495d");
  versions.device_attached = true;
  versions.personality = capture::DevicePersonality::kRecovery;

  const QString text = FirmwareText(versions);

  EXPECT_TRUE(text.contains(QStringLiteral("recovery mode")));
  EXPECT_TRUE(text.contains(QStringLiteral("not damaged")));
  EXPECT_TRUE(text.contains(QStringLiteral("None installed")));
  EXPECT_TRUE(text.contains(QStringLiteral("Cannot be read")));

  EXPECT_FALSE(text.contains(QStringLiteral("firmware older than")))
      << "a device with no firmware was diagnosed as having old firmware";
  EXPECT_FALSE(text.contains(QStringLiteral("did not answer")))
      << "the FPGA was blamed for not answering a question nothing asked it";
  EXPECT_FALSE(text.contains(QStringLiteral("not all built from the same")))
      << "a device with nothing installed was accused of a version mismatch";

  EXPECT_TRUE(text.contains(QStringLiteral("Bring up a new or legacy board")))
      << "a device whose FPGA nothing can reach was sent to the one window "
         "that cannot program it: "
      << text.toStdString();
}

// A working device with nothing to compare, which is a different sentence
// from a device with nothing installed: the versions are absent because the
// firmware predates the version stamp, not because it is missing.
TEST(FirmwareTextTest, ALegacyDeviceIsNamedRatherThanDiagnosed) {
  FirmwareVersions versions;
  versions.application = QStringLiteral("7713495d");
  versions.device_attached = true;
  versions.personality = capture::DevicePersonality::kLegacy;

  const QString text = FirmwareText(versions);

  EXPECT_TRUE(text.contains(QStringLiteral("original Duplicator firmware")));
  EXPECT_TRUE(text.contains(QStringLiteral("cannot update itself")));

  EXPECT_FALSE(text.contains(QStringLiteral("recovery mode")))
      << "a device with firmware was described as having none";
  EXPECT_FALSE(text.contains(QStringLiteral("firmware older than")))
      << "the version stamp was used to diagnose firmware that predates it";
}

TEST(FirmwareTextTest, ADeviceRunningAProgrammingToolSaysHowToClearIt) {
  FirmwareVersions versions;
  versions.application = QStringLiteral("7713495d");
  versions.device_attached = true;
  versions.personality = capture::DevicePersonality::kFlashProgrammer;

  EXPECT_TRUE(FirmwareText(versions).contains(QStringLiteral("Unplug it")));
}

}  // namespace
}  // namespace ddd::gui
