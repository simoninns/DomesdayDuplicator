/************************************************************************

    test_guided_capture_dialog.cpp

    Widget tests for the guided capture setup, with no player attached
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

#include "auto_capture_controller.h"
#include "capture_format.h"
#include "capture_settings.h"
#include "guided_capture_dialog.h"
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
// builds it, driving nothing.
class GuidedCaptureDialogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-guided-%1").arg(QLatin1String(info->name())));
    QSettings().clear();

    // A directory of this test's own, so that what is already in it is what the
    // test put there.
    directory_ = std::filesystem::temp_directory_path() /
                 (std::string("ddd-guided-test-") + info->name());
    std::filesystem::remove_all(directory_);
    std::filesystem::create_directories(directory_);

    CaptureSettings settings;
    settings.capture_directory = QString::fromStdString(directory_.string());
    SaveCaptureSettings(settings);
  }

  void TearDown() override {
    dialog_.reset();
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

  std::filesystem::path directory_;

  void Build(const player::DiscProfile& disc) {
    dialog_ = std::make_unique<GuidedCaptureDialog>(nullptr, disc);
  }

  // With a controller that has neither a player nor a capture engine behind it.
  // Start() then takes the window into its running state and drives nothing,
  // which is exactly what is needed to test what the window shows during a run.
  void BuildRunning(const player::DiscProfile& disc) {
    controller_ = std::make_unique<AutoCaptureController>(nullptr, nullptr);
    dialog_ = std::make_unique<GuidedCaptureDialog>(controller_.get(), disc);
    dialog_->Start();
  }

  template <typename T>
  T* Find(const char* name) const {
    return dialog_->findChild<T*>(QLatin1String(name));
  }

  std::unique_ptr<AutoCaptureController> controller_;
  std::unique_ptr<GuidedCaptureDialog> dialog_;
};

// --- Never suggesting a name that is already taken --------------------------

// The defect this exists for: the suggestion is built from what the disc *is*,
// so it is the same every time that side is captured — unlike the generated
// name, which carries a timestamp. A prefill that was already taken would put a
// name in the field that is not the name of the file.
TEST_F(GuidedCaptureDialogTest, ASuggestedNameIsResolvedBeforeItIsOffered) {
  ExistingCapture(QStringLiteral("CLV_PAL_Side2"));
  Build(ClvDisc());

  auto* const name = Find<QLineEdit>(GuidedCaptureDialog::kNameEditName);
  ASSERT_NE(name, nullptr);

  // What is on screen is what will be written, not what would have been.
  EXPECT_EQ(name->text(), QStringLiteral("CLV_PAL_Side2_2"));
}

TEST_F(GuidedCaptureDialogTest, AFreeSuggestionIsOfferedAsItIs) {
  Build(ClvDisc());

  EXPECT_EQ(Find<QLineEdit>(GuidedCaptureDialog::kNameEditName)->text(),
            QStringLiteral("CLV_PAL_Side2"));

  // isHidden() rather than isVisible(): the dialog itself is never shown in a
  // widget test, so isVisible() is false for every child and would pass here
  // however wrong the code was.
  EXPECT_TRUE(
      Find<QLabel>(GuidedCaptureDialog::kNameTakenLabelName)->isHidden());
}

TEST_F(GuidedCaptureDialogTest, ATypedNameThatIsTakenSaysWhatWillBeWritten) {
  ExistingCapture(QStringLiteral("Casper side 1"));
  Build(CavDisc());

  auto* const note = Find<QLabel>(GuidedCaptureDialog::kNameTakenLabelName);
  ASSERT_NE(note, nullptr);

  Find<QLineEdit>(GuidedCaptureDialog::kNameEditName)
      ->setText(QStringLiteral("Casper side 1"));

  // Said before the disc starts spinning, and said as the name is typed rather
  // than after the file has been written under another one.
  EXPECT_FALSE(note->text().isEmpty());
  EXPECT_TRUE(note->text().contains(QStringLiteral("Casper side 1_2")));

  // And nothing is overwritten, which is what the wording has to say — the
  // engine resolves the path before it opens anything.
  EXPECT_TRUE(note->text().contains(QStringLiteral("overwritten")));

  EXPECT_FALSE(note->isHidden());

  // And it goes away again for a name that is free.
  Find<QLineEdit>(GuidedCaptureDialog::kNameEditName)
      ->setText(QStringLiteral("Casper side 2"));
  EXPECT_TRUE(note->isHidden());
}

TEST_F(GuidedCaptureDialogTest, AnEmptyNameIsNeverReportedAsTaken) {
  Build(CavDisc());

  Find<QLineEdit>(GuidedCaptureDialog::kNameEditName)->setText(QString());

  // The generated name carries a timestamp and is free by construction, so a
  // note here would be one that never went away.
  EXPECT_TRUE(
      Find<QLabel>(GuidedCaptureDialog::kNameTakenLabelName)->isHidden());
}

// --- What the profile decides is on offer ----------------------------------

// The plan's acceptance criterion: a CAV profile offers no time-code controls
// and a CLV profile offers no frame controls. Absent rather than disabled — a
// greyed-out field for a thing this disc does not have invites somebody to look
// for the setting that would turn it on.
TEST_F(GuidedCaptureDialogTest, ACavDiscIsOfferedFramesAndNoTimeCodes) {
  Build(CavDisc());

  EXPECT_NE(Find<QSpinBox>(GuidedCaptureDialog::kStartFrameSpinName), nullptr);
  EXPECT_NE(Find<QSpinBox>(GuidedCaptureDialog::kEndFrameSpinName), nullptr);
  EXPECT_EQ(Find<QLineEdit>(GuidedCaptureDialog::kStartTimeEditName), nullptr);
  EXPECT_EQ(Find<QLineEdit>(GuidedCaptureDialog::kEndTimeEditName), nullptr);
}

TEST_F(GuidedCaptureDialogTest, AClvDiscIsOfferedTimeCodesAndNoFrames) {
  Build(ClvDisc());

  EXPECT_NE(Find<QLineEdit>(GuidedCaptureDialog::kStartTimeEditName), nullptr);
  EXPECT_NE(Find<QLineEdit>(GuidedCaptureDialog::kEndTimeEditName), nullptr);
  EXPECT_EQ(Find<QSpinBox>(GuidedCaptureDialog::kStartFrameSpinName), nullptr);
  EXPECT_EQ(Find<QSpinBox>(GuidedCaptureDialog::kEndFrameSpinName), nullptr);
}

TEST_F(GuidedCaptureDialogTest, TheBoundsComeFromTheMeasuredLength) {
  Build(CavDisc());

  auto* const start = Find<QSpinBox>(GuidedCaptureDialog::kStartFrameSpinName);
  auto* const end = Find<QSpinBox>(GuidedCaptureDialog::kEndFrameSpinName);
  ASSERT_NE(start, nullptr);
  ASSERT_NE(end, nullptr);

  // A range that cannot exist cannot be typed, rather than being refused
  // several seconds after the disc has started spinning.
  EXPECT_EQ(start->minimum(), 1);
  EXPECT_EQ(start->maximum(), 54000);
  EXPECT_EQ(end->minimum(), 1);
  EXPECT_EQ(end->maximum(), 54000);
}

TEST_F(GuidedCaptureDialogTest,
       TheDefaultIsTheWholeSideBetweenTheMeasuredEnds) {
  Build(CavDisc());

  const player::AutoCapturePlan plan = dialog_->Plan();
  EXPECT_EQ(plan.shape, player::CaptureShape::kWholeSide);
  EXPECT_EQ(plan.addressing, player::AddressMode::kFrame);
  EXPECT_EQ(plan.start_address, 1);
  EXPECT_EQ(plan.end_address, 54000);
  EXPECT_EQ(dialog_->problem(), player::PlanProblem::kNone);
}

TEST_F(GuidedCaptureDialogTest, TheStartFieldIsOnlyLiveForARange) {
  Build(CavDisc());

  auto* const start = Find<QSpinBox>(GuidedCaptureDialog::kStartFrameSpinName);
  auto* const end = Find<QSpinBox>(GuidedCaptureDialog::kEndFrameSpinName);
  ASSERT_NE(start, nullptr);

  // The two spin-up shapes start where the programme does — the player is
  // started from a stop and arrives there by itself — so a start address the
  // user typed would describe nothing.
  EXPECT_FALSE(start->isEnabled());
  EXPECT_FALSE(end->isEnabled());

  Find<QRadioButton>(GuidedCaptureDialog::kFromSpinUpRadioName)
      ->setChecked(true);
  EXPECT_FALSE(start->isEnabled());
  EXPECT_TRUE(end->isEnabled());

  Find<QRadioButton>(GuidedCaptureDialog::kRangeRadioName)->setChecked(true);
  EXPECT_TRUE(start->isEnabled());
  EXPECT_TRUE(end->isEnabled());
}

TEST_F(GuidedCaptureDialogTest, ARangeTypedBackwardsIsSaidToBeWrongAtOnce) {
  Build(CavDisc());

  Find<QRadioButton>(GuidedCaptureDialog::kRangeRadioName)->setChecked(true);
  Find<QSpinBox>(GuidedCaptureDialog::kStartFrameSpinName)->setValue(30000);
  Find<QSpinBox>(GuidedCaptureDialog::kEndFrameSpinName)->setValue(10000);

  EXPECT_EQ(dialog_->problem(), player::PlanProblem::kEndBeforeStart);

  auto* const problem = Find<QLabel>(GuidedCaptureDialog::kProblemLabelName);
  ASSERT_NE(problem, nullptr);
  EXPECT_EQ(problem->text(),
            PlanProblemText(player::PlanProblem::kEndBeforeStart));

  // And there is nothing to press, rather than a button that starts a capture
  // the sequence would refuse.
  EXPECT_FALSE(
      Find<QPushButton>(GuidedCaptureDialog::kStartButtonName)->isEnabled());
}

TEST_F(GuidedCaptureDialogTest, AClvRangeIsReadAsTimeCodes) {
  Build(ClvDisc());

  Find<QRadioButton>(GuidedCaptureDialog::kRangeRadioName)->setChecked(true);
  Find<QLineEdit>(GuidedCaptureDialog::kStartTimeEditName)->setText("10:00");
  Find<QLineEdit>(GuidedCaptureDialog::kEndTimeEditName)->setText("20:30");

  const player::AutoCapturePlan plan = dialog_->Plan();
  EXPECT_EQ(plan.addressing, player::AddressMode::kTimeCode);
  EXPECT_EQ(plan.start_address, 100000);
  EXPECT_EQ(plan.end_address, 203000);
  EXPECT_EQ(dialog_->problem(), player::PlanProblem::kNone);
}

TEST_F(GuidedCaptureDialogTest, ATimeThatIsNotATimeIsRefusedRatherThanGuessed) {
  Build(ClvDisc());

  Find<QRadioButton>(GuidedCaptureDialog::kRangeRadioName)->setChecked(true);
  Find<QLineEdit>(GuidedCaptureDialog::kEndTimeEditName)->setText("1:99");

  // "1:99" is not a time, and guessing what it meant would be worse than saying
  // so. It reaches the plan as an address no disc has.
  EXPECT_EQ(dialog_->problem(), player::PlanProblem::kMalformedAddress);
}

// --- What the examination could not say ------------------------------------

TEST_F(GuidedCaptureDialogTest, AnUnknownStandardIsAskedForAndNothingElseIs) {
  player::DiscProfile disc = CavDisc();
  disc.video_standard = player::Fact<player::VideoStandard>{};
  Build(disc);

  auto* const standard =
      Find<QComboBox>(GuidedCaptureDialog::kStandardComboName);
  ASSERT_NE(standard, nullptr);

  // A frame count is only a duration once the frame rate is known, so an
  // unexamined standard means no estimate at all — said rather than left blank.
  auto* const estimate = Find<QLabel>(GuidedCaptureDialog::kEstimateLabelName);
  ASSERT_NE(estimate, nullptr);
  EXPECT_TRUE(estimate->text().contains(QStringLiteral("not known")));

  // Choosing one records it as declared rather than as something the player
  // said, and the estimate follows.
  standard->setCurrentIndex(
      standard->findData(static_cast<int>(player::VideoStandard::kPal)));

  EXPECT_EQ(dialog_->disc().video_standard.value, player::VideoStandard::kPal);
  EXPECT_EQ(dialog_->disc().video_standard.provenance,
            player::Provenance::kDeclared);
  EXPECT_FALSE(estimate->text().contains(QStringLiteral("not known")));
}

TEST_F(GuidedCaptureDialogTest, AKnownStandardIsNotAskedAbout) {
  Build(CavDisc());

  // A fully examined disc is asked nothing at all, which is the whole
  // difference from the old application's form.
  EXPECT_EQ(Find<QComboBox>(GuidedCaptureDialog::kStandardComboName), nullptr);
}

TEST_F(GuidedCaptureDialogTest, AllThreeShapesAreAlwaysOnOffer) {
  player::DiscProfile disc = CavDisc();

  // Even where the examination could not seek to the start of the programme. No
  // command puts a player on the lead-in, so that reading says nothing about
  // whether the spin-up can be captured — it is captured by starting the file
  // before the disc, which needs only a player that can be stopped and started.
  disc.lead_in_reachable.Record(false, player::Provenance::kMeasured);
  Build(disc);

  for (const char* const name : {GuidedCaptureDialog::kWholeSideRadioName,
                                 GuidedCaptureDialog::kRangeRadioName,
                                 GuidedCaptureDialog::kFromSpinUpRadioName}) {
    EXPECT_TRUE(Find<QRadioButton>(name)->isEnabled()) << name;
  }

  EXPECT_TRUE(Find<QRadioButton>(GuidedCaptureDialog::kWholeSideRadioName)
                  ->isChecked());
  EXPECT_EQ(dialog_->problem(), player::PlanProblem::kNone);
}

TEST_F(GuidedCaptureDialogTest,
       TheNameIsPrefilledFromWhatTheDiscTurnedOutToBe) {
  Build(ClvDisc());

  auto* const name = Find<QLineEdit>(GuidedCaptureDialog::kNameEditName);
  ASSERT_NE(name, nullptr);

  // The side is the part that earns its place: two files made in a row are the
  // two sides of one disc.
  EXPECT_TRUE(name->text().contains(QStringLiteral("Side2")));
  EXPECT_TRUE(name->text().contains(QStringLiteral("CLV")));
}

TEST_F(GuidedCaptureDialogTest, TheKeyLockIsOffUntilItIsAskedFor) {
  Build(CavDisc());

  auto* const key_lock =
      Find<QCheckBox>(GuidedCaptureDialog::kKeyLockCheckName);
  ASSERT_NE(key_lock, nullptr);
  EXPECT_FALSE(key_lock->isChecked());
  EXPECT_FALSE(dialog_->Plan().key_lock);

  key_lock->setChecked(true);
  EXPECT_TRUE(dialog_->Plan().key_lock);
}

TEST_F(GuidedCaptureDialogTest, TheCouplingPreferenceIsHereWithItsDefault) {
  Build(CavDisc());

  // Off, and a considered default: a player that stumbles partway through a
  // side would otherwise truncate a capture that was going perfectly well.
  EXPECT_FALSE(
      Find<QCheckBox>(GuidedCaptureDialog::kStopCaptureCheckName)->isChecked());
}

// --- A profile that cannot be captured from --------------------------------

TEST_F(GuidedCaptureDialogTest,
       AnUnmeasuredSideSaysSoRatherThanOfferingACapture) {
  player::DiscProfile disc = CavDisc();
  disc.programme_end = player::Fact<int32_t>{};
  Build(disc);

  EXPECT_EQ(dialog_->problem(), player::PlanProblem::kUnknownLength);
  EXPECT_EQ(Find<QLabel>(GuidedCaptureDialog::kProblemLabelName)->text(),
            PlanProblemText(player::PlanProblem::kUnknownLength));
  EXPECT_FALSE(
      Find<QPushButton>(GuidedCaptureDialog::kStartButtonName)->isEnabled());
}

TEST_F(GuidedCaptureDialogTest, WithNoControllerNothingCanBeStarted) {
  Build(CavDisc());

  // A runnable plan and still nothing to press, because there is no player to
  // run it. The window builds and validates all the same, which is what makes
  // the whole of it testable with nothing attached.
  ASSERT_EQ(dialog_->problem(), player::PlanProblem::kNone);
  EXPECT_FALSE(
      Find<QPushButton>(GuidedCaptureDialog::kStartButtonName)->isEnabled());

  dialog_->Start();
  EXPECT_FALSE(dialog_->running());
}

// --- Showing the run -------------------------------------------------------

TEST_F(GuidedCaptureDialogTest,
       ProgressIsMeasuredByTheAddressRatherThanTheSteps) {
  Build(CavDisc());

  // Driven directly, as the controller would. The step count is meaningless
  // here — one step is repeated for the length of a side — so the address is
  // the only honest measure.
  dialog_->SetProgress(player::AutoCaptureStage::kWatching, 27000);

  auto* const progress =
      Find<QProgressBar>(GuidedCaptureDialog::kProgressBarName);
  ASSERT_NE(progress, nullptr);

  // Nothing moves until a run is under way: this window does not pretend a
  // capture is happening because somebody called a slot.
  EXPECT_FALSE(dialog_->running());

  auto* const status = Find<QLabel>(GuidedCaptureDialog::kStatusLabelName);
  ASSERT_NE(status, nullptr);
  EXPECT_TRUE(status->text().isEmpty());
}

TEST_F(GuidedCaptureDialogTest, TheRunSaysHowMuchLongerItHasToGo) {
  BuildRunning(CavDisc());
  ASSERT_TRUE(dialog_->running());

  dialog_->SetProgress(player::AutoCaptureStage::kWatching, 54000 - 1800);

  auto* const status = Find<QLabel>(GuidedCaptureDialog::kStatusLabelName);
  ASSERT_NE(status, nullptr);

  // Where the player is, and how long there is left to wait for it. The second
  // is what somebody actually wants from a window they are going to leave
  // running for half an hour.
  EXPECT_TRUE(status->text().contains(QStringLiteral("52200")));
  EXPECT_TRUE(status->text().contains(QStringLiteral("1:00")));
  EXPECT_TRUE(status->text().contains(QStringLiteral("left")));
}

TEST_F(GuidedCaptureDialogTest, TheTimeLeftIsNotShownOnceTheDiscIsNotPlaying) {
  BuildRunning(CavDisc());
  ASSERT_TRUE(dialog_->running());

  // The address persists into the tail so the bar does not jump backwards, but
  // a countdown beside "spinning the disc down" would be counting towards
  // something that has already happened.
  dialog_->SetProgress(player::AutoCaptureStage::kStoppingPlayer, 54000 - 1800);

  const QString text =
      Find<QLabel>(GuidedCaptureDialog::kStatusLabelName)->text();
  EXPECT_TRUE(text.contains(QStringLiteral("52200")));
  EXPECT_FALSE(text.contains(QStringLiteral("left")));
}

TEST_F(GuidedCaptureDialogTest, ARunWithNoTimeLeftToShowStillSaysWhereItIs) {
  player::DiscProfile disc = CavDisc();
  disc.video_standard = player::Fact<player::VideoStandard>{};
  BuildRunning(disc);
  ASSERT_TRUE(dialog_->running());

  dialog_->SetProgress(player::AutoCaptureStage::kWatching, 27000);

  // A frame count is only a duration once the frame rate is known, and the
  // window says nothing rather than a figure that would be a fifth out.
  const QString text =
      Find<QLabel>(GuidedCaptureDialog::kStatusLabelName)->text();
  EXPECT_TRUE(text.contains(QStringLiteral("27000")));
  EXPECT_FALSE(text.contains(QStringLiteral("left")));
}

TEST_F(GuidedCaptureDialogTest, EveryOutcomeGetsItsOwnSentence) {
  Build(CavDisc());

  auto* const status = Find<QLabel>(GuidedCaptureDialog::kStatusLabelName);
  ASSERT_NE(status, nullptr);

  for (const player::AutoCaptureOutcome outcome :
       {player::AutoCaptureOutcome::kCompleted,
        player::AutoCaptureOutcome::kStalled,
        player::AutoCaptureOutcome::kPlayerStopped,
        player::AutoCaptureOutcome::kDiscChanged,
        player::AutoCaptureOutcome::kLinkFailed,
        player::AutoCaptureOutcome::kCancelled}) {
    dialog_->SetResult(outcome);
    EXPECT_EQ(status->text(), AutoCaptureSummary(outcome));
    EXPECT_FALSE(status->text().isEmpty());
  }
}

TEST_F(GuidedCaptureDialogTest, ALinkFailureSaysTheCaptureIsStillRunning) {
  Build(CavDisc());

  dialog_->SetResult(player::AutoCaptureOutcome::kLinkFailed);

  // The one branch that ends with a capture still writing, and the window is
  // where the user finds that out.
  EXPECT_TRUE(Find<QLabel>(GuidedCaptureDialog::kStatusLabelName)
                  ->text()
                  .contains(QStringLiteral("still running")));
}

}  // namespace
}  // namespace ddd::gui
