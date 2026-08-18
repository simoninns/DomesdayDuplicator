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
      capture::UpdateStage::kChecking,     capture::UpdateStage::kPreparing,
      capture::UpdateStage::kTransferring, capture::UpdateStage::kWriting,
      capture::UpdateStage::kVerifying,    capture::UpdateStage::kRestarting,
      capture::UpdateStage::kConfirming,   capture::UpdateStage::kComplete,
      capture::UpdateStage::kFailed};

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

// --- A device with no firmware ---------------------------------------------

// Two cases, indistinguishable on the wire, that read very differently to the
// person in front of them. The text says what is true of both rather than
// guessing at one.
TEST(UpdateTextTest, RecoveryModeNamesBothWaysADeviceGetsThere) {
  const QString text =
      DevicePersonalityText(capture::DevicePersonality::kRecovery);

  EXPECT_TRUE(text.contains(QStringLiteral("recovery mode")));
  EXPECT_TRUE(text.contains(QStringLiteral("never been programmed")))
      << "somebody with a newly built board is not told this is normal";
  EXPECT_TRUE(text.contains(QStringLiteral("did not finish")))
      << "somebody whose update was interrupted is not told what happened";
  EXPECT_TRUE(text.contains(QStringLiteral("not damaged")));
}

TEST(UpdateTextTest, AWorkingDeviceHasNothingToExplain) {
  EXPECT_TRUE(DevicePersonalityText(capture::DevicePersonality::kApplication)
                  .isEmpty());
  EXPECT_TRUE(
      DeviceListPersonalitySuffix(capture::DevicePersonality::kApplication)
          .isEmpty());
}

// --- A device running the legacy firmware ----------------------------------

// Old, not broken. The device works; what it predates is this mechanism, and
// a user told their working board is faulty has been given the wrong problem
// to solve.
TEST(UpdateTextTest, TheLegacyStateNamesTheDeviceAsOldRatherThanBroken) {
  const QString text =
      DevicePersonalityText(capture::DevicePersonality::kLegacy);

  EXPECT_TRUE(text.contains(QStringLiteral("original Duplicator firmware")));
  EXPECT_TRUE(text.contains(QStringLiteral("It works")))
      << "a working board was described as a fault";
  EXPECT_TRUE(text.contains(QStringLiteral("cannot program it")))
      << "the one thing this window cannot do about it is not said";

  EXPECT_FALSE(DeviceListPersonalitySuffix(capture::DevicePersonality::kLegacy)
                   .isEmpty());
}

// The install button can never act on one, so it stops offering. A disabled
// button reading "Program this device" beside a paragraph saying this window
// cannot program it reads as a fault in the application.
TEST(UpdateTextTest, TheActionOffersNothingOnALegacyDevice) {
  const QString label = InstallActionLabel(capture::DevicePersonality::kLegacy);

  EXPECT_FALSE(label.contains(QStringLiteral("Program this device")));
  EXPECT_TRUE(label.contains(QStringLiteral("Cannot be programmed")));
}

TEST(UpdateTextTest, TheFlashProgrammerStateSaysHowToLeaveIt) {
  const QString text =
      DevicePersonalityText(capture::DevicePersonality::kFlashProgrammer);

  EXPECT_TRUE(text.contains(QStringLiteral("Unplug it")));
}

// "Program", not "repair": somebody holding a board they have just built has
// not broken anything, and the application cannot tell the two cases apart.
TEST(UpdateTextTest, TheActionIsProgrammingRatherThanRepairing) {
  EXPECT_EQ(InstallActionLabel(capture::DevicePersonality::kApplication),
            QStringLiteral("Update"));

  const QString recovery =
      InstallActionLabel(capture::DevicePersonality::kRecovery);
  EXPECT_TRUE(recovery.contains(QStringLiteral("Program")));
  EXPECT_FALSE(recovery.contains(QStringLiteral("epair")))
      << "a device that may never have been programmed is not 'repaired'";
}

TEST(UpdateTextTest, ARecoveryDeviceReportsNoVersionsRatherThanUnknownOnes) {
  const std::vector<UpdateVersionRow> rows =
      UpdateVersionRows(QStringLiteral("1.4.0"), capture::DeviceIdentity{},
                        true, nullptr, capture::DevicePersonality::kRecovery);

  ASSERT_EQ(rows.size(), 3u);
  EXPECT_EQ(rows[1].installed, QStringLiteral("None installed"));

  // Not "none" for the gateware: the FPGA is a separate part with its own
  // memory, and what is true is that nothing can ask it.
  EXPECT_EQ(rows[2].installed, QStringLiteral("Cannot be read"));
}

// Every row a bundle carries is a change on a device that has nothing
// installed, which is what makes the table show at a glance what the install
// is going to do.
TEST(UpdateTextTest, EverythingInTheBundleChangesOnADeviceWithNoFirmware) {
  const capture::UpdateManifest manifest = MakeManifest("89abcdef", "89abcdef");

  const std::vector<UpdateVersionRow> rows =
      UpdateVersionRows(QStringLiteral("1.4.0"), capture::DeviceIdentity{},
                        true, &manifest, capture::DevicePersonality::kRecovery);

  ASSERT_EQ(rows.size(), 3u);
  EXPECT_TRUE(rows[1].changes) << "the firmware row is not marked as changing";
}

// --- A unit running its recovery gateware ----------------------------------

capture::DeviceIdentity MakeGatewareRecoveryDevice() {
  capture::DeviceIdentity device = MakeDevice();
  device.register_map_version = capture::kRegisterMapVersionMaximum;
  device.image_role = capture::kImageRoleFactory;
  return device;
}

// The stages a bundle with both halves visits twice each say which half they
// are about. Without the name the screen shows the same three titles twice
// over with the bar restarting in the middle, which reads as an update that
// went wrong rather than one that is half done.
TEST(UpdateTextTest, TheStagesWithAComponentSayWhichOneTheyAreAbout) {
  const capture::UpdateStage staged[] = {capture::UpdateStage::kTransferring,
                                         capture::UpdateStage::kWriting,
                                         capture::UpdateStage::kVerifying};

  for (capture::UpdateStage stage : staged) {
    const QString firmware =
        UpdateStageTitle(stage, capture::UpdateTarget::kFirmware);
    const QString gateware =
        UpdateStageTitle(stage, capture::UpdateTarget::kGateware);

    EXPECT_TRUE(firmware.contains(QStringLiteral("firmware")));
    EXPECT_TRUE(gateware.contains(QStringLiteral("gateware")));
    EXPECT_NE(firmware, gateware);
  }
}

// The rest happen once for the whole update rather than once per component,
// so naming one would be naming the wrong thing.
TEST(UpdateTextTest, TheStagesWithoutAComponentDoNotNameOne) {
  const capture::UpdateStage whole[] = {
      capture::UpdateStage::kChecking, capture::UpdateStage::kRestarting,
      capture::UpdateStage::kConfirming, capture::UpdateStage::kComplete,
      capture::UpdateStage::kFailed};

  for (capture::UpdateStage stage : whole) {
    EXPECT_EQ(UpdateStageTitle(stage, capture::UpdateTarget::kGateware),
              UpdateStageTitle(stage));
  }
}

// A unit in gateware recovery is working, enumerated and answering, and it
// cannot capture. Nothing else about the device would say so — the version
// row would show a gateware commit, and it would be the wrong one to be
// reassured by.
TEST(UpdateTextTest, TheRecoveryGatewareStateIsExplained) {
  const QString text = GatewareRecoveryText(MakeGatewareRecoveryDevice());

  EXPECT_FALSE(text.isEmpty());
  EXPECT_TRUE(
      text.contains(QStringLiteral("recovery gateware"), Qt::CaseInsensitive));

  // The same reassurance every other failure state carries, because it is
  // true here too and it is the question a user is actually asking.
  EXPECT_TRUE(text.contains(QStringLiteral("not damaged")));
}

TEST(UpdateTextTest, AWorkingGatewareHasNothingToExplain) {
  EXPECT_TRUE(GatewareRecoveryText(MakeDevice()).isEmpty());

  // And neither has an FPGA that did not answer: the role byte read from one
  // is not a role byte, and there is no recovery state to report from it.
  capture::DeviceIdentity silent = MakeDevice();
  silent.gateware_present = false;
  silent.image_role = capture::kImageRoleFactory;
  EXPECT_TRUE(GatewareRecoveryText(silent).isEmpty());
}

// "Reinstall", not "repair" and not "program": the firmware is fine, and
// what is wanted is the half that did not finish arriving.
TEST(UpdateTextTest, TheActionForGatewareRecoveryIsAReinstall) {
  const QString label =
      InstallActionLabel(capture::DevicePersonality::kApplication, true);

  EXPECT_TRUE(label.contains(QStringLiteral("Reinstall")));
  EXPECT_TRUE(label.contains(QStringLiteral("gateware")));
}

// The row says what is installed, and on a unit in recovery the honest
// answer is not the resident image's commit — which would read as a
// perfectly ordinary gateware version.
TEST(UpdateTextTest, TheGatewareRowNamesTheRecoveryImageRatherThanItsCommit) {
  const capture::UpdateManifest manifest = MakeManifest("0123abcd", "0123abcd");

  const std::vector<UpdateVersionRow> rows = UpdateVersionRows(
      QStringLiteral("1.4.0"), MakeGatewareRecoveryDevice(), true, &manifest);

  ASSERT_EQ(rows.size(), 3u);
  EXPECT_EQ(rows[2].installed, QStringLiteral("Recovery gateware"));

  // And it is marked as a change even though the bundle's commit matches
  // what the factory image reports: it is not the commit being replaced, it
  // is the absence of a working one.
  EXPECT_TRUE(rows[2].changes);
}

}  // namespace
}  // namespace ddd::gui
