/************************************************************************

    test_update_text.cpp

    T1 unit test for what the update page says
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include "update_text.h"
#include "wire_protocol.h"

namespace ddd::gui {
namespace {

capture::DeviceIdentity MakeDevice() {
  capture::DeviceIdentity device;
  device.product_string = "Domesday Duplicator (0123abcd)";
  device.protocol_version = 1;
  device.gateware_present = true;
  device.register_map_version = 1;
  device.gateware_commit = "0123abcd";
  return device;
}

capture::UpdateManifest MakeManifest(const std::string& firmware_identity,
                                     const std::string& gateware_identity) {
  capture::UpdateManifest manifest;
  manifest.manifest_version = capture::kUpdateManifestVersion;
  manifest.version = "1.5.0";
  manifest.commit = "89abcdef";
  manifest.release_notes = "Faster gateware.";

  capture::UpdateComponent firmware;
  firmware.identity = firmware_identity;
  firmware.length = 1024;
  manifest.firmware = firmware;

  capture::UpdateComponent gateware;
  gateware.identity = gateware_identity;
  gateware.length = 2048;
  manifest.gateware = gateware;

  return manifest;
}

// The application is in the comparison even though this dialog cannot install
// it, because leaving it out would answer two thirds of "am I up to date".
TEST(UpdateTextTest, TheComparisonCoversAllThreeVersions) {
  const capture::UpdateManifest manifest = MakeManifest("89abcdef", "89abcdef");
  const std::vector<UpdateVersionRow> rows =
      UpdateVersionRows(QStringLiteral("1.4.0"), MakeDevice(), true, &manifest);

  ASSERT_EQ(rows.size(), 3u);
  EXPECT_EQ(rows[0].name, QStringLiteral("Application"));
  EXPECT_EQ(rows[1].name, QStringLiteral("Firmware"));
  EXPECT_EQ(rows[2].name, QStringLiteral("Gateware"));
}

TEST(UpdateTextTest, ItMarksWhatTheUpdateWouldChange) {
  const capture::UpdateManifest manifest = MakeManifest("89abcdef", "89abcdef");
  const std::vector<UpdateVersionRow> rows =
      UpdateVersionRows(QStringLiteral("1.4.0"), MakeDevice(), true, &manifest);

  EXPECT_TRUE(rows[0].changes) << "an older application is not marked";
  EXPECT_TRUE(rows[1].changes) << "different firmware is not marked";
  EXPECT_TRUE(rows[2].changes) << "different gateware is not marked";
}

// An update that would install exactly what is already there is worth
// showing without emphasis: it is a reinstall, not a change.
TEST(UpdateTextTest, ItDoesNotMarkAPartTheUpdateWouldNotChange) {
  const capture::UpdateManifest manifest = MakeManifest("0123abcd", "0123abcd");
  const std::vector<UpdateVersionRow> rows =
      UpdateVersionRows(QStringLiteral("1.5.0"), MakeDevice(), true, &manifest);

  EXPECT_FALSE(rows[0].changes);
  EXPECT_FALSE(rows[1].changes);
  EXPECT_FALSE(rows[2].changes);
}

// Two commits that name the same build read as the same version, even when
// one is seven characters and the other eight — the firmware asks git for
// eight and a Nix build passes seven.
TEST(UpdateTextTest, CommitsOfDifferentLengthsAreTheSameBuild) {
  const capture::UpdateManifest manifest = MakeManifest("0123abc", "0123abc");
  const std::vector<UpdateVersionRow> rows =
      UpdateVersionRows(QStringLiteral("1.5.0"), MakeDevice(), true, &manifest);

  EXPECT_FALSE(rows[1].changes);
  EXPECT_FALSE(rows[2].changes);
}

TEST(UpdateTextTest, WithNoDeviceTheRowsSaySoRatherThanBeingBlank) {
  const std::vector<UpdateVersionRow> rows = UpdateVersionRows(
      QStringLiteral("1.4.0"), capture::DeviceIdentity{}, false, nullptr);

  EXPECT_FALSE(rows[1].installed.isEmpty());
  EXPECT_FALSE(rows[2].installed.isEmpty());
  EXPECT_FALSE(rows[1].changes);
}

TEST(UpdateTextTest, AGatewareThatNeverAnsweredIsReportedAsSuch) {
  capture::DeviceIdentity device = MakeDevice();
  device.gateware_present = false;
  device.gateware_commit.clear();

  const std::vector<UpdateVersionRow> rows =
      UpdateVersionRows(QStringLiteral("1.4.0"), device, true, nullptr);

  EXPECT_EQ(rows[2].installed, QStringLiteral("Not reported"));
}

TEST(UpdateTextTest, TheTableShowsEveryRow) {
  const capture::UpdateManifest manifest = MakeManifest("89abcdef", "89abcdef");
  const QString html = UpdateVersionTable(UpdateVersionRows(
      QStringLiteral("1.4.0"), MakeDevice(), true, &manifest));

  EXPECT_TRUE(html.contains(QStringLiteral("Application")));
  EXPECT_TRUE(html.contains(QStringLiteral("0123abcd")));
  EXPECT_TRUE(html.contains(QStringLiteral("89abcdef")));
}

// A figure to the second would be precise about something that is a guess.
// What a user needs is the number that decides whether to go and make tea.
TEST(UpdateTextTest, TheEstimateIsCoarseAndAlwaysReadable) {
  EXPECT_EQ(FormatUpdateEstimate(0), QStringLiteral("a few seconds"));
  EXPECT_EQ(FormatUpdateEstimate(20), QStringLiteral("about 30 seconds"));
  EXPECT_EQ(FormatUpdateEstimate(100), QStringLiteral("about 2 minutes"));
  EXPECT_EQ(FormatUpdateEstimate(600), QStringLiteral("about 10 minutes"));

  for (int seconds : {0, 1, 30, 90, 300, 3600}) {
    EXPECT_FALSE(FormatUpdateEstimate(seconds).isEmpty());
  }
}

TEST(UpdateTextTest, EveryStageHasATitleInPlainLanguage) {
  const capture::UpdateStage stages[] = {
      capture::UpdateStage::kChecking,   capture::UpdateStage::kTransferring,
      capture::UpdateStage::kWriting,    capture::UpdateStage::kVerifying,
      capture::UpdateStage::kRestarting, capture::UpdateStage::kConfirming,
      capture::UpdateStage::kComplete,   capture::UpdateStage::kFailed};

  for (capture::UpdateStage stage : stages) {
    const QString title = UpdateStageTitle(stage);
    EXPECT_FALSE(title.isEmpty());
    EXPECT_NE(title, QStringLiteral("Working"))
        << "a stage fell through to the default title";
  }
}

// The one instruction that matters, and the same words the documentation
// uses.
TEST(UpdateTextTest, TheInstructionSaysToLeaveItPluggedIn) {
  EXPECT_TRUE(UpdateHoldStillInstruction().contains(
      QStringLiteral("plugged in"), Qt::CaseInsensitive));
}

TEST(UpdateTextTest, TheBundleSummaryNamesWhatIsInIt) {
  const capture::UpdateManifest manifest = MakeManifest("89abcdef", "89abcdef");
  const QString summary = UpdateBundleSummary(manifest);

  EXPECT_TRUE(summary.contains(QStringLiteral("1.5.0")));
  EXPECT_TRUE(summary.contains(QStringLiteral("89abcdef")));
  EXPECT_TRUE(summary.contains(QStringLiteral("firmware")));
  EXPECT_TRUE(summary.contains(QStringLiteral("gateware")));
  EXPECT_TRUE(summary.contains(QStringLiteral("Faster gateware.")));
}

// A development signature proves format and never origin, and the interface
// has to say that rather than leaving it to the documentation.
TEST(UpdateTextTest, TheDevelopmentBannerSaysWhatItDoesNotProve) {
  const QString banner = DevelopmentBundleBanner();

  EXPECT_TRUE(banner.contains(QStringLiteral("development")));
  EXPECT_TRUE(banner.contains(QStringLiteral("public")));
}

TEST(UpdateTextTest, TheGateTextIsEmptyWhenThereIsNothingToSay) {
  capture::UpdateGateResult gate;
  EXPECT_TRUE(UpdateGateText(gate).isEmpty());

  gate.verdict = capture::UpdateGateVerdict::kApplicationTooOld;
  gate.reasons.push_back("Update the application first.");
  EXPECT_TRUE(UpdateGateText(gate).contains(
      QStringLiteral("Update the application first")));
}

TEST(UpdateTextTest, TheConfirmationQuotesWhatTheDeviceReports) {
  const QString text = UpdateCompleteText(MakeDevice());

  EXPECT_TRUE(text.contains(QStringLiteral("0123abcd")));
  EXPECT_TRUE(text.contains(QStringLiteral("complete")));
}

// Every failure has to answer "is my device broken", because that is the
// question the user is actually asking.
TEST(UpdateTextTest, AFailureSaysTheDeviceIsNotDamaged) {
  const QString text =
      UpdateFailureText(QStringLiteral("Something went wrong."));

  EXPECT_TRUE(text.contains(QStringLiteral("Something went wrong.")));
  EXPECT_TRUE(text.contains(QStringLiteral("not damaged")));
  EXPECT_TRUE(text.contains(QStringLiteral("recovery mode")));
}

}  // namespace
}  // namespace ddd::gui
