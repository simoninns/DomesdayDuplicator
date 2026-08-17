/************************************************************************

    test_auto_capture_wizard.cpp

    T1 tests for the automatic capture workflow, with no player attached
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QString>
#include <filesystem>
#include <fstream>
#include <memory>

#include "auto_capture_controller.h"
#include "auto_capture_wizard.h"
#include "capture_format.h"
#include "capture_settings.h"
#include "player_text.h"

namespace ddd::gui {
namespace {

// An NTSC CAV side of 54,000 frames, examined in full.
player::DiscProfile CavDisc() {
  player::DiscProfile disc;
  disc.disc_present.Record(true, player::Provenance::kReported);
  disc.disc_type.Record(player::DiscType::kCav, player::Provenance::kReported);
  disc.addressing.Record(player::AddressMode::kFrame,
                         player::Provenance::kInferred);
  disc.disc_side.Record(1, player::Provenance::kReported);
  disc.disc_size.Record(player::DiscSize::k30cm, player::Provenance::kReported);
  disc.programme_start.Record(1, player::Provenance::kMeasured);
  disc.programme_end.Record(54000, player::Provenance::kMeasured);
  disc.lead_in_reachable.Record(true, player::Provenance::kMeasured);
  disc.video_standard.Record(player::VideoStandard::kNtsc,
                             player::Provenance::kReported);
  return disc;
}

// A PAL CLV side running to 0:50:45.
player::DiscProfile ClvDisc() {
  player::DiscProfile disc = CavDisc();
  disc.disc_type.Record(player::DiscType::kClv, player::Provenance::kReported);
  disc.addressing.Record(player::AddressMode::kTimeCode,
                         player::Provenance::kInferred);
  disc.disc_side.Record(2, player::Provenance::kReported);
  disc.programme_start.Record(0, player::Provenance::kMeasured);
  disc.programme_end.Record(504500, player::Provenance::kMeasured);
  disc.video_standard.Record(player::VideoStandard::kPal,
                             player::Provenance::kReported);
  return disc;
}

// The whole window, built with no controller — exactly as the application
// builds it, driving nothing. Every page lays out and validates the same way,
// which is what makes the flow testable with nothing attached.
class AutoCaptureWizardTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-wizard-%1").arg(QLatin1String(info->name())));
    QSettings().clear();

    // A directory of this test's own, so that what is already in it is what the
    // test put there.
    directory_ = std::filesystem::temp_directory_path() /
                 (std::string("ddd-wizard-test-") + info->name());
    std::filesystem::remove_all(directory_);
    std::filesystem::create_directories(directory_);

    CaptureSettings settings;
    settings.capture_directory = QString::fromStdString(directory_.string());
    SaveCaptureSettings(settings);
  }

  void TearDown() override {
    wizard_.reset();
    controller_.reset();
    std::filesystem::remove_all(directory_);
    QSettings().clear();
  }

  // Put a capture of this name in the destination, as an earlier session would
  // have left it.
  void ExistingCapture(const QString& stem) {
    std::ofstream file(directory_ /
                       (stem.toStdString() + capture::kCaptureFileSuffix));
    file << "x";
  }

  // No controller at all. The window builds, navigates and validates, and
  // captures nothing.
  void Build() { wizard_ = std::make_unique<AutoCaptureWizard>(nullptr); }

  // With a controller that has neither a player nor a capture engine behind it.
  // Start() then takes the window into its running state and drives nothing,
  // which is what is needed to test what the window does during a run.
  void BuildRunnable() {
    controller_ = std::make_unique<AutoCaptureController>(nullptr, nullptr);
    wizard_ = std::make_unique<AutoCaptureWizard>(controller_.get());
  }

  // Hand over an examined disc, as the Examine window's report does.
  void WithDisc(const player::DiscProfile& disc) {
    wizard_->StartFromProfile(disc);
  }

  template <typename T>
  T* Find(const char* name) const {
    return wizard_->findChild<T*>(QLatin1String(name));
  }

  QPushButton* Next() const {
    return Find<QPushButton>(AutoCaptureWizard::kNextButtonName);
  }
  QPushButton* Previous() const {
    return Find<QPushButton>(AutoCaptureWizard::kPreviousButtonName);
  }

  std::filesystem::path directory_;
  std::unique_ptr<AutoCaptureController> controller_;
  std::unique_ptr<AutoCaptureWizard> wizard_;
};

// --- Where it starts, and what it will not skip ----------------------------

TEST_F(AutoCaptureWizardTest, ItOpensOnTheDiscAndGoesNoFurtherWithoutOne) {
  Build();

  EXPECT_EQ(wizard_->page(), AutoCaptureWizard::Page::kDisc);

  // A capture needs the disc type and the measured end of the side. Without
  // them the next page would offer everything and refuse everything, which is a
  // worse way to learn that the examination did not finish.
  EXPECT_FALSE(Next()->isEnabled());
  EXPECT_FALSE(Previous()->isEnabled());

  wizard_->GoNext();
  EXPECT_EQ(wizard_->page(), AutoCaptureWizard::Page::kDisc);
}

TEST_F(AutoCaptureWizardTest, AnExaminedDiscOpensTheWayForward) {
  Build();
  wizard_->SetExamineResult(CavDisc(), player::ExamineOutcome::kCompleted);

  EXPECT_TRUE(Next()->isEnabled());

  wizard_->GoNext();
  EXPECT_EQ(wizard_->page(), AutoCaptureWizard::Page::kSettings);
  EXPECT_TRUE(Previous()->isEnabled());
}

TEST_F(AutoCaptureWizardTest, AnUnmeasuredSideIsNotSomethingToPlanFrom) {
  player::DiscProfile disc = CavDisc();
  disc.programme_end = player::Fact<int32_t>{};

  Build();
  wizard_->SetExamineResult(disc, player::ExamineOutcome::kCompleted);

  EXPECT_FALSE(Next()->isEnabled());
  EXPECT_FALSE(Next()->toolTip().isEmpty())
      << "nothing says why the way forward is shut";
}

TEST_F(AutoCaptureWizardTest, AReportHandedOverSkipsStraightToTheSettings) {
  // What the Examine window's "Automatic capture…" does. Examining again would
  // spend a minute rediscovering what the report already says, and leave the
  // disc somewhere else while it did.
  Build();
  WithDisc(ClvDisc());

  EXPECT_EQ(wizard_->page(), AutoCaptureWizard::Page::kSettings);
  EXPECT_EQ(wizard_->disc().disc_type.value, player::DiscType::kClv);
}

// --- The settings page -----------------------------------------------------

TEST_F(AutoCaptureWizardTest, ThePlanControlsFollowTheDiscThatWasFound) {
  Build();
  WithDisc(CavDisc());

  // The plan form's own rule, reached through the wizard: a CAV disc is offered
  // frames and no time codes.
  EXPECT_NE(Find<QSpinBox>(CapturePlanForm::kStartFrameSpinName), nullptr);
  EXPECT_EQ(Find<QLineEdit>(CapturePlanForm::kStartTimeEditName), nullptr);
  EXPECT_EQ(wizard_->problem(), player::PlanProblem::kNone);
}

TEST_F(AutoCaptureWizardTest, ADifferentDiscRebuildsTheControlsForIt) {
  // The controls cannot be updated in place: which fields exist at all is
  // decided by the disc, so a second disc is a second form.
  Build();
  WithDisc(CavDisc());
  ASSERT_NE(Find<QSpinBox>(CapturePlanForm::kStartFrameSpinName), nullptr);

  wizard_->SetExamineResult(ClvDisc(), player::ExamineOutcome::kCompleted);

  EXPECT_EQ(Find<QSpinBox>(CapturePlanForm::kStartFrameSpinName), nullptr);
  EXPECT_NE(Find<QLineEdit>(CapturePlanForm::kStartTimeEditName), nullptr);
}

TEST_F(AutoCaptureWizardTest, APlanThatCannotRunHoldsTheWayForwardShut) {
  Build();
  WithDisc(CavDisc());
  ASSERT_TRUE(Next()->isEnabled());

  Find<QRadioButton>(CapturePlanForm::kRangeRadioName)->setChecked(true);
  Find<QSpinBox>(CapturePlanForm::kStartFrameSpinName)->setValue(30000);
  Find<QSpinBox>(CapturePlanForm::kEndFrameSpinName)->setValue(10000);

  EXPECT_EQ(wizard_->problem(), player::PlanProblem::kEndBeforeStart);
  EXPECT_FALSE(Next()->isEnabled());

  // And it says which problem, rather than only that there is one.
  EXPECT_EQ(Next()->toolTip(),
            PlanProblemText(player::PlanProblem::kEndBeforeStart));

  wizard_->GoNext();
  EXPECT_EQ(wizard_->page(), AutoCaptureWizard::Page::kSettings);
}

TEST_F(AutoCaptureWizardTest, TheDestinationIsHereRatherThanOnAnotherPanel) {
  // The settings that used to force a detour to the Capture panel, which is
  // most of what made this a workflow spread across four windows.
  Build();
  WithDisc(CavDisc());

  EXPECT_NE(Find<QLineEdit>(AutoCaptureWizard::kDirectoryEditName), nullptr);
  EXPECT_NE(Find<QComboBox>(AutoCaptureWizard::kFormatComboName), nullptr);
  EXPECT_NE(Find<QComboBox>(AutoCaptureWizard::kSampleRateComboName), nullptr);
}

// --- Naming ----------------------------------------------------------------

// The defect this exists for: the suggestion is built from what the disc *is*,
// so it is the same every time that side is captured — unlike the generated
// name, which carries a timestamp. A prefill that was already taken would put a
// name in the field that is not the name of the file.
TEST_F(AutoCaptureWizardTest, ASuggestedNameIsResolvedBeforeItIsOffered) {
  ExistingCapture(QStringLiteral("CLV_PAL_Side2"));
  Build();
  WithDisc(ClvDisc());

  auto* const name = Find<QLineEdit>(AutoCaptureWizard::kNameEditName);
  ASSERT_NE(name, nullptr);

  // What is on screen is what will be written, not what would have been.
  EXPECT_EQ(name->text(), QStringLiteral("CLV_PAL_Side2_2"));
}

TEST_F(AutoCaptureWizardTest, AFreeSuggestionIsOfferedAsItIs) {
  Build();
  WithDisc(ClvDisc());

  EXPECT_EQ(Find<QLineEdit>(AutoCaptureWizard::kNameEditName)->text(),
            QStringLiteral("CLV_PAL_Side2"));

  // isHidden() rather than isVisible(): the window is never shown in a widget
  // test, so isVisible() is false for every child and would pass here however
  // wrong the code was.
  EXPECT_TRUE(Find<QLabel>(AutoCaptureWizard::kNameTakenLabelName)->isHidden());
}

TEST_F(AutoCaptureWizardTest, ATypedNameThatIsTakenSaysWhatWillBeWritten) {
  ExistingCapture(QStringLiteral("Casper side 1"));
  Build();
  WithDisc(CavDisc());

  auto* const note = Find<QLabel>(AutoCaptureWizard::kNameTakenLabelName);
  ASSERT_NE(note, nullptr);

  Find<QLineEdit>(AutoCaptureWizard::kNameEditName)
      ->setText(QStringLiteral("Casper side 1"));

  // Said before the disc starts spinning, and said as the name is typed rather
  // than after the file has been written under another one.
  EXPECT_TRUE(note->text().contains(QStringLiteral("Casper side 1_2")));
  EXPECT_TRUE(note->text().contains(QStringLiteral("overwritten")));
  EXPECT_FALSE(note->isHidden());

  // And it goes away again for a name that is free.
  Find<QLineEdit>(AutoCaptureWizard::kNameEditName)
      ->setText(QStringLiteral("Casper side 2"));
  EXPECT_TRUE(note->isHidden());
}

TEST_F(AutoCaptureWizardTest, TheNamingFieldsAreFilledFromWhatWasFound) {
  // The same form the manual path's Naming… button opens, so there is one set
  // of naming rules in the application rather than two.
  Build();
  WithDisc(ClvDisc());

  EXPECT_TRUE(
      Find<QCheckBox>(CaptureNamingForm::kDiscTypeCheckName)->isChecked());
  EXPECT_TRUE(
      Find<QCheckBox>(CaptureNamingForm::kStandardCheckName)->isChecked());
  EXPECT_TRUE(Find<QCheckBox>(CaptureNamingForm::kSideCheckName)->isChecked());
  EXPECT_EQ(Find<QSpinBox>(CaptureNamingForm::kSideSpinName)->value(), 2);
}

// --- The run ---------------------------------------------------------------

TEST_F(AutoCaptureWizardTest, WithNoControllerNothingCanBeStarted) {
  Build();
  WithDisc(CavDisc());
  wizard_->GoNext();
  ASSERT_EQ(wizard_->page(), AutoCaptureWizard::Page::kCapture);

  // A runnable plan and still nothing to press, because there is no player to
  // run it. The window builds and validates all the same, which is what makes
  // the whole of it testable with nothing attached.
  ASSERT_EQ(wizard_->problem(), player::PlanProblem::kNone);
  EXPECT_FALSE(
      Find<QPushButton>(AutoCaptureWizard::kStartButtonName)->isEnabled());

  wizard_->Start();
  EXPECT_FALSE(wizard_->running());
}

TEST_F(AutoCaptureWizardTest, ARunLocksTheWayBackAndForward) {
  BuildRunnable();
  WithDisc(CavDisc());
  wizard_->GoNext();
  ASSERT_EQ(wizard_->page(), AutoCaptureWizard::Page::kCapture);

  wizard_->Start();
  ASSERT_TRUE(wizard_->running());

  // The pages behind describe the capture now being taken, and letting them be
  // edited underneath would make the window disagree with the file.
  EXPECT_FALSE(Previous()->isEnabled());
  EXPECT_FALSE(Next()->isEnabled());

  wizard_->GoPrevious();
  EXPECT_EQ(wizard_->page(), AutoCaptureWizard::Page::kCapture);

  // And the controls that describe it go with them.
  EXPECT_FALSE(Find<QLineEdit>(AutoCaptureWizard::kNameEditName)->isEnabled());
  EXPECT_FALSE(
      Find<QRadioButton>(CapturePlanForm::kWholeSideRadioName)->isEnabled());
  EXPECT_FALSE(
      Find<QLineEdit>(AutoCaptureWizard::kDirectoryEditName)->isEnabled());
}

TEST_F(AutoCaptureWizardTest,
       ProgressIsMeasuredByTheAddressRatherThanTheSteps) {
  BuildRunnable();
  WithDisc(CavDisc());
  wizard_->GoNext();
  wizard_->Start();
  ASSERT_TRUE(wizard_->running());

  // Driven directly, as the controller would. The step count is meaningless
  // here — one step is repeated for the length of a side — so the address is
  // the only honest measure.
  wizard_->SetRunProgress(player::AutoCaptureStage::kWatching, 54000 - 1800);

  auto* const status = Find<QLabel>(AutoCaptureWizard::kRunStatusLabelName);
  ASSERT_NE(status, nullptr);

  // Where the player is, and how long there is left to wait for it. The second
  // is what somebody actually wants from a window they are going to leave
  // running for half an hour.
  EXPECT_TRUE(status->text().contains(QStringLiteral("52200")));
  EXPECT_TRUE(status->text().contains(QStringLiteral("1:00")));
  EXPECT_TRUE(status->text().contains(QStringLiteral("left")));
}

TEST_F(AutoCaptureWizardTest, TheTimeLeftIsNotShownOnceTheDiscIsNotPlaying) {
  BuildRunnable();
  WithDisc(CavDisc());
  wizard_->GoNext();
  wizard_->Start();
  ASSERT_TRUE(wizard_->running());

  // The address persists into the tail so the bar does not jump backwards, but
  // a countdown beside "spinning the disc down" would be counting towards
  // something that has already happened.
  wizard_->SetRunProgress(player::AutoCaptureStage::kStoppingPlayer,
                          54000 - 1800);

  const QString text =
      Find<QLabel>(AutoCaptureWizard::kRunStatusLabelName)->text();
  EXPECT_TRUE(text.contains(QStringLiteral("52200")));
  EXPECT_FALSE(text.contains(QStringLiteral("left")));
}

TEST_F(AutoCaptureWizardTest, NothingMovesUntilARunIsActuallyUnderWay) {
  Build();
  WithDisc(CavDisc());

  // This window does not pretend a capture is happening because somebody called
  // a slot.
  wizard_->SetRunProgress(player::AutoCaptureStage::kWatching, 27000);

  EXPECT_FALSE(wizard_->running());
  EXPECT_TRUE(
      Find<QLabel>(AutoCaptureWizard::kRunStatusLabelName)->text().isEmpty());
}

// --- The summary -----------------------------------------------------------

TEST_F(AutoCaptureWizardTest, AFinishedRunLandsOnTheSummaryByItself) {
  BuildRunnable();
  WithDisc(CavDisc());
  wizard_->GoNext();
  wizard_->Start();
  ASSERT_TRUE(wizard_->running());

  wizard_->SetRunResult(player::AutoCaptureOutcome::kCompleted);

  // Somebody who has left a forty-minute side running wants to find the answer
  // on the screen rather than a Next button to press for it.
  EXPECT_EQ(wizard_->page(), AutoCaptureWizard::Page::kSummary);
  EXPECT_FALSE(wizard_->running());
  EXPECT_EQ(Find<QLabel>(AutoCaptureWizard::kSummaryLabelName)->text(),
            AutoCaptureSummary(player::AutoCaptureOutcome::kCompleted));
}

TEST_F(AutoCaptureWizardTest, EveryOutcomeGetsItsOwnSentence) {
  BuildRunnable();
  WithDisc(CavDisc());

  auto* const summary = Find<QLabel>(AutoCaptureWizard::kSummaryLabelName);
  ASSERT_NE(summary, nullptr);

  for (const player::AutoCaptureOutcome outcome :
       {player::AutoCaptureOutcome::kCompleted,
        player::AutoCaptureOutcome::kStalled,
        player::AutoCaptureOutcome::kPlayerStopped,
        player::AutoCaptureOutcome::kDiscChanged,
        player::AutoCaptureOutcome::kLinkFailed,
        player::AutoCaptureOutcome::kCancelled}) {
    wizard_->SetRunResult(outcome);
    EXPECT_EQ(summary->text(), AutoCaptureSummary(outcome));
    EXPECT_FALSE(summary->text().isEmpty());
  }
}

TEST_F(AutoCaptureWizardTest, ALinkFailureSaysTheCaptureIsStillRunning) {
  BuildRunnable();
  WithDisc(CavDisc());

  wizard_->SetRunResult(player::AutoCaptureOutcome::kLinkFailed);

  // The one branch that ends with a capture still writing, and the window is
  // where the user finds that out.
  EXPECT_TRUE(Find<QLabel>(AutoCaptureWizard::kSummaryLabelName)
                  ->text()
                  .contains(QStringLiteral("still running")));
}

TEST_F(AutoCaptureWizardTest, TheOtherSideIsTwoPressesRatherThanEight) {
  // The loop this whole window exists to shorten: capture a side, turn the disc
  // over, capture the other.
  BuildRunnable();
  WithDisc(CavDisc());

  auto* const side = Find<QSpinBox>(CaptureNamingForm::kSideSpinName);
  ASSERT_NE(side, nullptr);
  ASSERT_EQ(side->value(), 1);

  wizard_->SetRunResult(player::AutoCaptureOutcome::kCompleted);
  ASSERT_EQ(wizard_->page(), AutoCaptureWizard::Page::kSummary);

  wizard_->CaptureAnotherSide();

  // Back to the beginning with the side moved on. There is no player here, so
  // nothing is examined — what is under test is the state it leaves behind for
  // the examination that would follow.
  EXPECT_EQ(wizard_->page(), AutoCaptureWizard::Page::kDisc);
  EXPECT_EQ(side->value(), 2);
}

TEST_F(AutoCaptureWizardTest, TheSideOnlyMovesOnWhereOneIsBeingRecorded) {
  // Somebody who has not ticked the side box is not filing by side, and turning
  // the option on for them would be deciding how they file.
  player::DiscProfile disc = CavDisc();
  disc.disc_side = player::Fact<int>{};

  BuildRunnable();
  WithDisc(disc);

  auto* const side_check = Find<QCheckBox>(CaptureNamingForm::kSideCheckName);
  ASSERT_NE(side_check, nullptr);
  ASSERT_FALSE(side_check->isChecked());

  wizard_->CaptureAnotherSide();

  EXPECT_FALSE(side_check->isChecked());
  EXPECT_EQ(Find<QSpinBox>(CaptureNamingForm::kSideSpinName)->value(), 1);
}

// --- Closing ---------------------------------------------------------------

TEST_F(AutoCaptureWizardTest, ItDoesNotBlockTheRestOfTheApplication) {
  // A whole-side capture is the better part of an hour, and the spectrum and
  // waveform panels are what somebody wants to be watching while it runs.
  Build();
  EXPECT_EQ(wizard_->windowModality(), Qt::NonModal);
}

}  // namespace
}  // namespace ddd::gui
