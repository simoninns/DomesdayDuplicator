/************************************************************************

    test_bringup_text.cpp

    T1 unit test for what the bring-up wizard says
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QString>
#include <vector>

#include "bringup_text.h"
#include "wire_protocol.h"

namespace ddd::gui {
namespace {

using capture::DevicePersonality;
using capture::UsbPresence;

// --- the pages ------------------------------------------------------------

TEST(BringUpText, EveryPageIsNumberedAndTitled) {
  for (BringUpPage page :
       {BringUpPage::kOverview, BringUpPage::kConnect, BringUpPage::kImage,
        BringUpPage::kJumper, BringUpPage::kFirmware,
        BringUpPage::kRemoveJumper, BringUpPage::kGateware,
        BringUpPage::kPowerCycle, BringUpPage::kVerify}) {
    EXPECT_FALSE(BringUpPageTitle(page).isEmpty());
    EXPECT_FALSE(BringUpPageHeading(page).isEmpty());
  }

  // Numbered out of nine even on a run that visits seven of them, so that two
  // runs of one procedure can be talked about in the same words.
  EXPECT_TRUE(BringUpPageTitle(BringUpPage::kVerify).contains("9 of 9"));
}

// The page that decides whether somebody does the physical job once or three
// times. Everything it has to ask for has to be on it.
TEST(BringUpText, TheOverviewNamesEveryPhysicalThingInAdvance) {
  const QString text = BringUpOverviewText();

  EXPECT_TRUE(text.contains("enclosure", Qt::CaseInsensitive));
  EXPECT_TRUE(text.contains("both cables", Qt::CaseInsensitive));
  EXPECT_TRUE(text.contains("jumper", Qt::CaseInsensitive));

  // And the end state, honestly: a working device that cannot capture yet is
  // a surprise unless it was promised.
  EXPECT_TRUE(text.contains("recovery gateware", Qt::CaseInsensitive));
}

// --- the sentence that matters most ---------------------------------------

// Either cable alone keeps the assembled unit powered, so pulling one changes
// nothing while the board stays lit. Every page that asks for a power cycle
// says *both*, in those words — this is the single likeliest way for somebody
// to get stuck, and it is invisible when it happens.
TEST(BringUpText, EveryPowerCycleAsksForBothCables) {
  for (const QString& text :
       {BringUpFitJumperText(), BringUpPowerCycleText()}) {
    EXPECT_TRUE(text.contains("both", Qt::CaseInsensitive))
        << text.toStdString();
    EXPECT_TRUE(text.contains("mini-USB", Qt::CaseInsensitive));
  }
}

TEST(BringUpText, TheTimeoutLeadsWithThePartialPowerCycle) {
  const QString text = BringUpPowerCycleTimeoutText();

  // First diagnosis, not third: it is far and away the commonest, and the one
  // whose symptom is that everything looks right.
  EXPECT_LT(text.indexOf("both cables", 0, Qt::CaseInsensitive),
            text.indexOf("jumper", 0, Qt::CaseInsensitive));
}

// One vocabulary for one jumper. The documentation and the firmware README
// both say fitted and removed, so the wizard does too.
TEST(BringUpText, TheJumperIsFittedAndRemovedRatherThanClosedAndOpen) {
  EXPECT_TRUE(BringUpFitJumperText().contains("Fit jumper J4"));
  EXPECT_TRUE(BringUpRemoveJumperText().contains("Remove jumper J4"));

  for (const QString& caption :
       {BringUpPhotographCaption(BringUpPage::kJumper),
        BringUpPhotographCaption(BringUpPage::kRemoveJumper)}) {
    EXPECT_FALSE(caption.contains("closed", Qt::CaseInsensitive));
    EXPECT_FALSE(caption.contains("open", Qt::CaseInsensitive));
  }
  EXPECT_TRUE(
      BringUpPhotographCaption(BringUpPage::kJumper).contains("fitted"));
  EXPECT_TRUE(
      BringUpPhotographCaption(BringUpPage::kRemoveJumper).contains("removed"));
}

TEST(BringUpText, EveryPhotographPageHasAPictureAndACaption) {
  for (BringUpPage page : {BringUpPage::kOverview, BringUpPage::kJumper,
                           BringUpPage::kRemoveJumper}) {
    EXPECT_TRUE(BringUpPhotographPath(page).startsWith(":/photographs/"));
    EXPECT_FALSE(BringUpPhotographCaption(page).isEmpty());
  }

  // And no others claim one.
  EXPECT_TRUE(BringUpPhotographPath(BringUpPage::kGateware).isEmpty());
}

// --- the connectivity rows ------------------------------------------------

TEST(BringUpTextFx3Row, ABoardInItsBootRomIsReadyAndNeedsNoJumper) {
  const BringUpStatusRow row =
      BringUpFx3Row(true, DevicePersonality::kRecovery, UsbPresence::kAbsent);

  EXPECT_EQ(row.state, BringUpRowState::kReady);
  EXPECT_TRUE(row.usable());
  EXPECT_TRUE(row.detail.contains("boot ROM"));
}

// The board this wizard exists for. It is not a fault and is not reported as
// one — it is waiting for the one thing this flow is about to do.
TEST(BringUpTextFx3Row, ALegacyBoardIsWaitingRatherThanBroken) {
  const BringUpStatusRow row =
      BringUpFx3Row(true, DevicePersonality::kLegacy, UsbPresence::kAbsent);

  EXPECT_EQ(row.state, BringUpRowState::kWaiting);
  EXPECT_TRUE(row.usable());
  EXPECT_TRUE(row.detail.contains("original", Qt::CaseInsensitive));
}

TEST(BringUpTextFx3Row, ACurrentBoardWillBeSentToTheJumper) {
  const BringUpStatusRow row = BringUpFx3Row(
      true, DevicePersonality::kApplication, UsbPresence::kAbsent);

  EXPECT_EQ(row.state, BringUpRowState::kWaiting);
  EXPECT_TRUE(row.detail.contains("jumper"));
}

// The one diagnosis nothing else in the application can make: the kit's debug
// serial port is powered whenever the board is, so seeing it while seeing no
// FX3 says the board has power and the USB 3.0 link does not work.
TEST(BringUpTextFx3Row, TheDebugBridgeSeparatesUnpoweredFromUnanswering) {
  const BringUpStatusRow powered = BringUpFx3Row(
      false, DevicePersonality::kApplication, UsbPresence::kPresent);
  EXPECT_FALSE(powered.usable());
  EXPECT_TRUE(powered.detail.contains("power", Qt::CaseInsensitive));
  EXPECT_TRUE(powered.detail.contains("USB 3.0"));

  const BringUpStatusRow nothing = BringUpFx3Row(
      false, DevicePersonality::kApplication, UsbPresence::kAbsent);
  EXPECT_FALSE(nothing.usable());
  EXPECT_NE(nothing.detail, powered.detail)
      << "a powered board and an absent one were given the same advice";
}

TEST(BringUpTextFpgaRow, AnOpenedCableIsReady) {
  const BringUpStatusRow row =
      BringUpFpgaRow(true, UsbPresence::kPresent, QString());

  EXPECT_EQ(row.state, BringUpRowState::kReady);
  EXPECT_TRUE(row.usable());
}

// The charge-only cable first, and deliberately: it is the documented failure
// of this exact step, and in an assembled unit the board is lit from the other
// cable no matter what the mini-USB is doing.
TEST(BringUpTextFpgaRow, ANotFoundCableNamesTheChargeOnlyCableFirst) {
  const BringUpStatusRow row =
      BringUpFpgaRow(false, UsbPresence::kAbsent, QString());

  EXPECT_FALSE(row.usable());
  EXPECT_TRUE(row.detail.contains("charge-only", Qt::CaseInsensitive));
  EXPECT_LT(row.detail.indexOf("charge-only", 0, Qt::CaseInsensitive),
            row.detail.indexOf("check the connector", 0, Qt::CaseInsensitive));
}

// Attached and un-openable is a different problem with a different remedy,
// and the whole reason the presence check exists.
TEST(BringUpTextFpgaRow, AnAttachedCableThatWillNotOpenSaysWhy) {
  const BringUpStatusRow row =
      BringUpFpgaRow(false, UsbPresence::kPresent, QString());

  EXPECT_FALSE(row.usable());
  EXPECT_FALSE(row.detail.contains("charge-only", Qt::CaseInsensitive))
      << "a cable that is plainly attached was blamed on the cable";
  EXPECT_TRUE(row.detail.contains("udev", Qt::CaseInsensitive));
  EXPECT_TRUE(row.detail.contains("jtagd", Qt::CaseInsensitive));
}

TEST(BringUpTextFpgaRow, TheCableDriversOwnSentenceIsCarriedThrough) {
  const QString reason =
      QStringLiteral("The cable attached is a USB-Blaster II.");
  EXPECT_EQ(BringUpFpgaRow(false, UsbPresence::kPresent, reason).detail,
            reason);
}

// --- the image page -------------------------------------------------------

capture::UpdateManifest MakeSet(bool firmware, bool provisioning) {
  capture::UpdateManifest manifest;
  manifest.manifest_version = capture::kUpdateManifestVersion;
  manifest.version = "1.4.0";
  manifest.commit = "0123abcd";

  capture::UpdateComponent component;
  component.length = 1024;
  component.identity = "0123abcd";

  if (firmware) {
    manifest.firmware = component;
  }
  if (provisioning) {
    manifest.provisioning = component;
  }
  return manifest;
}

TEST(BringUpTextImage, ACompleteSetHasNoProblemAndSaysWhatItCarries) {
  const capture::UpdateManifest manifest = MakeSet(true, true);

  EXPECT_TRUE(BringUpImageProblem(manifest).isEmpty());

  const QString summary = BringUpImageSummary(manifest);
  EXPECT_TRUE(summary.contains("1.4.0"));
  EXPECT_TRUE(summary.contains("firmware", Qt::CaseInsensitive));
  EXPECT_TRUE(summary.contains("provisioning", Qt::CaseInsensitive));
}

TEST(BringUpTextImage, AnOrdinaryUpdateFileIsRefusedAndNamed) {
  const QString problem = BringUpImageProblem(MakeSet(true, false));
  EXPECT_FALSE(problem.isEmpty());
  EXPECT_TRUE(problem.contains("provisioning", Qt::CaseInsensitive));
}

// A set with no firmware is refused for a reason worth stating: the FX3 has to
// become modern first, so there is no order in which such a set could be used.
TEST(BringUpTextImage, ASetWithNoFirmwareIsRefusedForTheOrderingReason) {
  const QString problem = BringUpImageProblem(MakeSet(false, true));
  EXPECT_FALSE(problem.isEmpty());
  EXPECT_TRUE(problem.contains("FX3 first", Qt::CaseInsensitive));
}

TEST(BringUpTextGateware, TheEstimateIsShownInMinutes) {
  EXPECT_TRUE(BringUpGatewareText(300).contains("5 minutes"));
  EXPECT_TRUE(BringUpGatewareText(300).contains("factory image"));
}

// --- the verification -----------------------------------------------------

capture::DeviceIdentity BroughtUpDevice() {
  capture::DeviceIdentity identity;
  identity.product_string = "Domesday Duplicator (0123abcd)";
  identity.protocol_version = 1;
  identity.gateware_present = true;
  identity.register_map_version = capture::kRegisterMapVersionWithImageRole;
  identity.image_role = capture::kImageRoleFactory;
  return identity;
}

TEST(BringUpVerify, EveryCheckPassesForAFinishedBoard) {
  const std::vector<BringUpCheck> checks =
      BringUpVerification(true, DevicePersonality::kApplication,
                          BroughtUpDevice(), QStringLiteral("0123abcd"));

  ASSERT_EQ(checks.size(), 4u);
  for (const BringUpCheck& check : checks) {
    EXPECT_TRUE(check.passed) << check.description.toStdString();
  }
}

// The state bring-up actually leaves a board in, and the one a reader has to
// be told is correct: the factory image is what the wizard writes.
TEST(BringUpVerify, TheApplicationImageIsNotWhatBringUpLeavesBehind) {
  capture::DeviceIdentity identity = BroughtUpDevice();
  identity.image_role = capture::kImageRoleApplication;

  const std::vector<BringUpCheck> checks = BringUpVerification(
      true, DevicePersonality::kApplication, identity, QString());

  ASSERT_FALSE(checks.empty());
  EXPECT_FALSE(checks.back().passed);
}

// One failure, described once. A device that is not answering cannot report a
// protocol version, a commit or a register map, and listing three more
// failures caused by the first would describe one problem four times.
TEST(BringUpVerify, ADeviceThatIsNotThereProducesOneFailureRatherThanFour) {
  const std::vector<BringUpCheck> checks = BringUpVerification(
      false, DevicePersonality::kApplication, capture::DeviceIdentity{},
      QStringLiteral("0123abcd"));

  ASSERT_EQ(checks.size(), 1u);
  EXPECT_FALSE(checks.front().passed);
}

// A check that cannot be made is not reported as one that passed.
TEST(BringUpVerify, ASetThatNamesNoCommitIsNotCheckedAgainstOne) {
  const std::vector<BringUpCheck> checks = BringUpVerification(
      true, DevicePersonality::kApplication, BroughtUpDevice(), QString());

  EXPECT_EQ(checks.size(), 3u);
  for (const BringUpCheck& check : checks) {
    EXPECT_FALSE(check.description.contains("build this set carries"));
  }
}

// --- the endings ----------------------------------------------------------

TEST(BringUpText, TheHandOverNamesTheNextStepAndTheCase) {
  const QString text = BringUpCompleteText();

  EXPECT_TRUE(text.contains("Update firmware"));
  EXPECT_TRUE(text.contains("case", Qt::CaseInsensitive));
}

// Nothing in this flow can be broken by stopping in the middle of it, and the
// failure page says so rather than leaving somebody to wonder.
TEST(BringUpText, AFailureSaysNothingIsBroken) {
  const QString text = BringUpFailureText(QStringLiteral("The cable went."));

  EXPECT_TRUE(text.contains("The cable went."));
  EXPECT_TRUE(text.contains("J4"));
  EXPECT_TRUE(text.contains("again", Qt::CaseInsensitive));
}

TEST(BringUpText, AStoppedRunIsNotDescribedAsAFailure) {
  const QString text = BringUpStoppedText();

  EXPECT_TRUE(text.contains("Stopped"));
  EXPECT_FALSE(text.contains("failed", Qt::CaseInsensitive));
}

}  // namespace
}  // namespace ddd::gui
