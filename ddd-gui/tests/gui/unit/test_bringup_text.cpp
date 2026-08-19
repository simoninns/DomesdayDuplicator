/************************************************************************

    test_bringup_text.cpp

    T1 unit test for what the bring-up wizard says
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QString>
#include <optional>
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
        BringUpPage::kJumper, BringUpPage::kConfigure, BringUpPage::kProgram,
        BringUpPage::kRemoveJumper, BringUpPage::kPowerCycle,
        BringUpPage::kVerify}) {
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

  // And the end state: a finished board, with nothing to follow.
  EXPECT_TRUE(text.contains("ready to capture", Qt::CaseInsensitive));
}

// The fact that lets somebody with an unknown board start. The jumper reaches
// the FX3's boot ROM from any state, so this flow never diagnoses a board —
// and that has to be said on the page where somebody decides whether they are
// in the right place at all.
TEST(BringUpText, TheOverviewSaysTheBoardsCurrentStateDoesNotMatter) {
  const QString text = BringUpOverviewText();

  EXPECT_TRUE(text.contains("does not matter what the board is running",
                            Qt::CaseInsensitive));
  EXPECT_TRUE(text.contains("twice", Qt::CaseInsensitive));
}

// --- what every page says about its own state ------------------------------
//
// Two lines, used on every page that has anything to wait for. They are the
// answer to the one question somebody has after doing something physical or
// pressing a button: did that work, and what now?

// A finished step must not bury the good news. Success used to be a clause in
// the middle of the paragraph that explained the step — the one part of a page
// nobody re-reads after pressing a button.
TEST(BringUpText, AFinishedStepLeadsWithAllDoneAndNamesTheButtonToPressNext) {
  const QString done =
      BringUpStepDoneText(QStringLiteral("The FPGA is running the gateware."));

  EXPECT_LT(done.indexOf("All done"), done.indexOf("gateware"));
  EXPECT_TRUE(done.contains("Next"));

  // In the same green the rows tick in, from the same place.
  EXPECT_TRUE(done.contains(BringUpMark(BringUpRowState::kReady)));
  EXPECT_TRUE(done.contains(BringUpMarkColour(BringUpRowState::kReady)));
}

// And a step that has not finished says what it is waiting for. "Waiting for
// you to press Load the gateware" and "waiting for the board to come back"
// are different situations that look identical from the outside: in both,
// nothing is happening.
TEST(BringUpText, AnUnfinishedStepSaysWhatItIsWaitingFor) {
  const QString waiting =
      BringUpWaitingText(QStringLiteral("you to press <b>Load it</b>."));

  EXPECT_TRUE(waiting.contains("Waiting for", Qt::CaseInsensitive));
  EXPECT_TRUE(waiting.contains("you to press"));
  EXPECT_TRUE(waiting.contains(BringUpMark(BringUpRowState::kWaiting)));

  // And cannot be mistaken for the finished line at a glance, which is what
  // the mark in front of it is for.
  EXPECT_FALSE(waiting.contains("All done"));
  EXPECT_NE(BringUpMark(BringUpRowState::kWaiting),
            BringUpMark(BringUpRowState::kReady));
}

// --- instructions before explanation ---------------------------------------

// What to do is the first thing on the page, not the conclusion of three
// paragraphs about why. Somebody reading these has a board in one hand.
TEST(BringUpText, TheWorkingPagesOpenByNamingTheButtonToPress) {
  const QString configure = BringUpConfigureText(3);
  EXPECT_LT(configure.indexOf("Load the gateware"),
            configure.indexOf("Nothing is written"));

  const QString program = BringUpProgramText(240);
  EXPECT_LT(program.indexOf("Program the board"), program.indexOf("EEPROM"));
}

// The two pages that ask for something physical do it as a numbered list. Both
// are sequences where doing step one without steps two and three achieves
// nothing at all while looking exactly as though it worked.
TEST(BringUpText, ThePhysicalPagesAreNumberedInstructions) {
  for (const QString& text :
       {BringUpFitJumperText(), BringUpPowerCycleText()}) {
    for (const char* step : {"1.", "2.", "3."}) {
      EXPECT_TRUE(text.contains(QLatin1String(step))) << text.toStdString();
    }
  }
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
  EXPECT_TRUE(BringUpPhotographPath(BringUpPage::kConfigure).isEmpty());
  EXPECT_TRUE(BringUpPhotographPath(BringUpPage::kProgram).isEmpty());
}

// --- the connectivity rows ------------------------------------------------

// A device as the wizard receives it: what it enumerated as, which protocol it
// speaks, and what it says it is running.
capture::DeviceInfo Board(DevicePersonality personality, int protocol_version,
                          const char* product_string = "") {
  capture::DeviceInfo info;
  info.path = "bus-1-port-2";
  info.personality = personality;
  info.protocol_version = protocol_version;
  info.product_string = product_string;
  return info;
}

// A board in its boot ROM is usable as it is and still has something asked of
// it, which is exactly what amber means. It is the one row where a green tick
// would mislead: an FX3 with an empty EEPROM is in its boot ROM with or
// without a jumper, so "already there" is not "already arranged".
TEST(BringUpTextFx3Row, ABoardInItsBootRomIsUsableAndStillNeedsTheJumper) {
  const BringUpStatusRow row = BringUpFx3Row(
      Board(DevicePersonality::kRecovery, 0), UsbPresence::kAbsent);

  EXPECT_EQ(row.state, BringUpRowState::kWaiting);
  EXPECT_TRUE(row.usable());
  EXPECT_TRUE(row.detail.contains("boot ROM"));
  EXPECT_TRUE(row.detail.contains("jumper"));
}

// The board this wizard exists for. It is not a fault and is not reported as
// one — it is waiting for the one thing this flow is about to do.
TEST(BringUpTextFx3Row, ALegacyBoardIsWaitingRatherThanBroken) {
  const BringUpStatusRow row =
      BringUpFx3Row(Board(DevicePersonality::kLegacy, 0), UsbPresence::kAbsent);

  EXPECT_EQ(row.state, BringUpRowState::kWaiting);
  EXPECT_TRUE(row.usable());
  EXPECT_TRUE(row.detail.contains("original", Qt::CaseInsensitive));

  // Named by the identity it enumerates under, so that "the original firmware"
  // is a thing somebody can check rather than a claim they have to take.
  EXPECT_TRUE(row.detail.contains("1d50:603b"));
}

// The row that started this: "running the Duplicator's firmware" said nothing
// about *which* firmware, and an amber mark beside it read as a fault. Three
// boards enumerate under the current identifiers or close to it, and each of
// them wants a different sentence.
TEST(BringUpTextFx3Row,
     ACurrentBoardIsNamedAsCurrentAndPointedAtTheUpdatePath) {
  const BringUpStatusRow row =
      BringUpFx3Row(Board(DevicePersonality::kApplication, 1,
                          "Domesday Duplicator (0123abcd)"),
                    UsbPresence::kAbsent);

  EXPECT_EQ(row.state, BringUpRowState::kWaiting);

  // Which firmware, and which build of it.
  EXPECT_TRUE(row.detail.contains("this application's own firmware"));
  EXPECT_TRUE(row.detail.contains("0123abcd"));

  // Somebody who only wanted to update a working board is in the wrong window,
  // and the row says so rather than letting them find out at the jumper page.
  EXPECT_TRUE(row.detail.contains("Update firmware"));
  EXPECT_TRUE(row.detail.contains("does not need bringing up"));
  EXPECT_TRUE(row.detail.contains("jumper"));
}

// A protocol this build does not know, which is indistinguishable from a
// working board by VID/PID alone and is the opposite thing to tell somebody.
//
// The reachable direction is **newer**: an application older than the firmware
// in front of it is an ordinary thing to have. Firmware older than the update
// agent under these identifiers was never released, so the row does not claim
// to have met one.
TEST(BringUpTextFx3Row, AnUnknownProtocolIsDistinguishedFromCurrent) {
  const BringUpStatusRow unknown = BringUpFx3Row(
      Board(DevicePersonality::kApplication, 0), UsbPresence::kAbsent);

  EXPECT_EQ(unknown.state, BringUpRowState::kWaiting);
  EXPECT_TRUE(unknown.detail.contains("does not know", Qt::CaseInsensitive));
  EXPECT_FALSE(unknown.detail.contains("predates", Qt::CaseInsensitive))
      << "the row named a generation of firmware that was never released";

  const BringUpStatusRow current = BringUpFx3Row(
      Board(DevicePersonality::kApplication, 1), UsbPresence::kAbsent);
  EXPECT_NE(unknown.detail, current.detail)
      << "a protocol this build knows and one it does not were described the "
         "same way";
}

TEST(BringUpTextFx3Row, ABoardThatNamesNoCommitIsNotGivenAnEmptyOne) {
  const BringUpStatusRow row = BringUpFx3Row(
      Board(DevicePersonality::kApplication, 1), UsbPresence::kAbsent);

  EXPECT_FALSE(row.detail.contains("()"));
}

// The one diagnosis nothing else in the application can make: the kit's debug
// serial port is powered whenever the board is, so seeing it while seeing no
// FX3 says the board has power and the USB 3.0 link does not work.
TEST(BringUpTextFx3Row, TheDebugBridgeSeparatesUnpoweredFromUnanswering) {
  const BringUpStatusRow powered =
      BringUpFx3Row(std::nullopt, UsbPresence::kPresent);
  EXPECT_FALSE(powered.usable());
  EXPECT_TRUE(powered.detail.contains("power", Qt::CaseInsensitive));
  EXPECT_TRUE(powered.detail.contains("USB 3.0"));

  const BringUpStatusRow nothing =
      BringUpFx3Row(std::nullopt, UsbPresence::kAbsent);
  EXPECT_FALSE(nothing.usable());
  EXPECT_NE(nothing.detail, powered.detail)
      << "a powered board and an absent one were given the same advice";
}

// The marks, and the sentence that says what they mean. Amber is the one worth
// explaining: it is the state a user is most likely to read as a fault.
TEST(BringUpText, TheLegendExplainsTheMarksTheRowsActuallyUse) {
  const QString legend = BringUpConnectLegend();

  for (BringUpRowState state :
       {BringUpRowState::kReady, BringUpRowState::kWaiting,
        BringUpRowState::kProblem}) {
    EXPECT_TRUE(legend.contains(BringUpMark(state)))
        << "the legend does not show the mark a row uses";
    EXPECT_TRUE(legend.contains(BringUpMarkColour(state)))
        << "the legend shows a mark in a colour the rows do not use";
  }

  EXPECT_TRUE(legend.contains("not", Qt::CaseInsensitive));
  EXPECT_TRUE(legend.contains("wrong", Qt::CaseInsensitive));
}

TEST(BringUpText, TheThreeMarksAreDifferentFromEachOther) {
  EXPECT_NE(BringUpMark(BringUpRowState::kReady),
            BringUpMark(BringUpRowState::kWaiting));
  EXPECT_NE(BringUpMark(BringUpRowState::kReady),
            BringUpMark(BringUpRowState::kProblem));
  EXPECT_NE(BringUpMarkColour(BringUpRowState::kReady),
            BringUpMarkColour(BringUpRowState::kProblem));
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

// A bundle carrying whichever payloads a test is about.
capture::UpdateManifest MakeFile(bool firmware, bool provisioning, bool factory,
                                 bool gateware) {
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
  if (factory) {
    manifest.factory_gateware = component;
  }
  if (gateware) {
    manifest.gateware = component;
  }
  return manifest;
}

TEST(BringUpTextImage, ACompleteFileHasNoProblemAndSaysWhatItCarries) {
  const capture::UpdateManifest manifest = MakeFile(true, true, true, true);

  EXPECT_TRUE(BringUpImageProblem(manifest).isEmpty());

  const QString summary = BringUpImageSummary(manifest);
  EXPECT_TRUE(summary.contains("1.4.0"));
  EXPECT_TRUE(summary.contains("firmware", Qt::CaseInsensitive));
  EXPECT_TRUE(summary.contains("vectors", Qt::CaseInsensitive));
  EXPECT_TRUE(summary.contains("factory image", Qt::CaseInsensitive));
  EXPECT_TRUE(summary.contains("application gateware", Qt::CaseInsensitive));
}

// The likeliest wrong file, and the one that has to be named rather than
// dismissed: a real release bundle from before the bring-up payloads existed.
// It updates a working device perfectly well and cannot bring one up.
TEST(BringUpTextImage, AnUpdateOnlyFileNamesExactlyWhatItIsMissing) {
  const QString problem =
      BringUpImageProblem(MakeFile(true, false, false, true));
  EXPECT_FALSE(problem.isEmpty());
  EXPECT_TRUE(problem.contains("vectors", Qt::CaseInsensitive));
  EXPECT_TRUE(problem.contains("factory image", Qt::CaseInsensitive));
}

TEST(BringUpTextImage, EachMissingPayloadIsNamedOnItsOwn) {
  EXPECT_TRUE(BringUpImageProblem(MakeFile(false, true, true, true))
                  .contains("firmware", Qt::CaseInsensitive));
  EXPECT_TRUE(BringUpImageProblem(MakeFile(true, true, true, false))
                  .contains("application gateware", Qt::CaseInsensitive));
  EXPECT_TRUE(BringUpImageProblem(MakeFile(true, true, false, true))
                  .contains("factory image", Qt::CaseInsensitive));
}

// Seconds rather than minutes, and that is a measurement rather than a
// wording preference: the step used to play an 18.4 MB flash-writing file that
// could not in fact be played at all, and what it does now is a 2.6-second
// configuration (TESTING.md B-V1).
TEST(BringUpTextConfigure, TheEstimateIsShownInSeconds) {
  EXPECT_TRUE(BringUpConfigureText(3).contains("3 seconds"));
}

// The one step in this application that talks to a board through a second
// cable. Somebody watching a bar labelled with an FPGA has every reason to
// assume it is being programmed, so the page says otherwise in as many words.
TEST(BringUpTextConfigure, TheTextSaysTheLoadWritesNothing) {
  const QString text = BringUpConfigureText(3);
  EXPECT_TRUE(text.contains("USB-Blaster"));
  EXPECT_TRUE(text.contains("Nothing is written", Qt::CaseInsensitive));

  // And why it happens before the firmware rather than after it, which is the
  // hardware-safety property this page carries.
  EXPECT_TRUE(text.contains("boot ROM", Qt::CaseInsensitive));
}

// The page that writes names all three images and the order they go in, since
// the order is what makes every interruption recoverable.
TEST(BringUpTextProgram, TheTextNamesAllThreeWritesAndTheEstimate) {
  const QString text = BringUpProgramText(240);

  EXPECT_TRUE(text.contains("240 seconds"));
  EXPECT_TRUE(text.contains("EEPROM"));
  EXPECT_TRUE(text.contains("factory image", Qt::CaseInsensitive));
  EXPECT_TRUE(text.contains("application image", Qt::CaseInsensitive));

  // And that nothing restarts here, because a user watching the device stay
  // put would otherwise wonder whether the step finished.
  EXPECT_TRUE(text.contains("power cycle", Qt::CaseInsensitive));
}

// --- the verification -----------------------------------------------------

capture::DeviceIdentity BroughtUpDevice() {
  capture::DeviceIdentity identity;
  identity.product_string = "Domesday Duplicator (0123abcd)";
  identity.protocol_version = 1;
  identity.gateware_present = true;
  identity.register_map_version = capture::kRegisterMapVersionMaximum;
  identity.image_role = capture::kImageRoleApplication;
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

// A board that comes back on its factory image is a board whose application
// image did not take: everything works and nothing is damaged, but it cannot
// capture. That is the difference between brought up and *most* of the way up,
// so it is a failed check rather than a pass.
TEST(BringUpVerify, ABoardLeftOnItsFactoryImageIsNotFinished) {
  capture::DeviceIdentity identity = BroughtUpDevice();
  identity.image_role = capture::kImageRoleFactory;

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
    EXPECT_FALSE(check.description.contains("build this file carries"));
  }
}

// --- where the file came from ---------------------------------------------
//
// A packaged build carries an update file, because a board being brought up
// cannot be updated over USB and the machine beside it may have no network at
// all. What these three sentences must never let happen is "it came with the
// application" reading as "so it was not checked".

TEST(BringUpText, ABundledFileSaysItWasCheckedAnyway) {
  const QString text = BringUpBundledFileText();

  EXPECT_TRUE(text.contains("signature", Qt::CaseInsensitive));
  EXPECT_TRUE(text.contains("digest", Qt::CaseInsensitive));

  // And that a different file can still be chosen, since a release newer than
  // the application is an ordinary thing to have.
  EXPECT_TRUE(text.contains("Choose", Qt::CaseInsensitive));
}

TEST(BringUpText, ABuildWithNoFileNamesTheFileToDownload) {
  const QString text = BringUpNoBundledFileText();

  EXPECT_TRUE(text.contains("domesday-duplicator-update"));

  // The one page of this procedure that needs another machine, said as such:
  // somebody in front of a computer with no network needs to know that a file
  // fetched elsewhere is the whole of what is missing.
  EXPECT_TRUE(text.contains("network", Qt::CaseInsensitive));
}

TEST(BringUpText, AnUnusableBundledFilePointsSomewhereElse) {
  const QString text = BringUpBundledFileUnusableText();

  // Nothing here can repair it, and the page does not pretend otherwise.
  EXPECT_TRUE(text.contains("could not be used", Qt::CaseInsensitive));
  EXPECT_TRUE(text.contains("domesday-duplicator-update"));
}

TEST(BringUpText, TheThreeSourceSentencesAreDifferentFromEachOther) {
  // They are shown in the same place, one at a time, so two that read alike
  // would be a page that changed and said nothing.
  EXPECT_NE(BringUpBundledFileText(), BringUpChosenFileText());
  EXPECT_NE(BringUpBundledFileText(), BringUpNoBundledFileText());
  EXPECT_NE(BringUpChosenFileText(), BringUpNoBundledFileText());
  EXPECT_TRUE(BringUpChosenFileText().contains("bundled", Qt::CaseInsensitive));
}

// --- the endings ----------------------------------------------------------

// Nothing follows a bring-up, and the last page has to say so: the flow used
// to hand over to the update dialog, and a page that still pointed at one
// would send somebody to repeat what they had just done.
TEST(BringUpText, TheEndSaysThereIsNothingLeftToDoAndTheCaseCanGoOn) {
  const QString text = BringUpCompleteText();

  EXPECT_TRUE(text.contains("nothing else to do", Qt::CaseInsensitive));
  EXPECT_TRUE(text.contains("ready to capture", Qt::CaseInsensitive));
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
