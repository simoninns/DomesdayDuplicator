/************************************************************************

    test_capture_plan_form.cpp

    T1 tests for the controls that turn an examined disc into a plan
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
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QString>
#include <filesystem>
#include <memory>

#include "capture_plan_form.h"
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

// The form on its own, which is how both the manual and the automatic paths
// build it: it is handed a profile and describes what can be taken off it.
class CapturePlanFormTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-plan-%1").arg(QLatin1String(info->name())));
    QSettings().clear();

    directory_ = std::filesystem::temp_directory_path() /
                 (std::string("ddd-plan-test-") + info->name());
    std::filesystem::remove_all(directory_);
    std::filesystem::create_directories(directory_);

    CaptureSettings settings;
    settings.capture_directory = QString::fromStdString(directory_.string());
    SaveCaptureSettings(settings);
  }

  void TearDown() override {
    form_.reset();
    std::filesystem::remove_all(directory_);
    QSettings().clear();
  }

  void Build(const player::DiscProfile& disc) {
    form_ = std::make_unique<CapturePlanForm>(disc);
  }

  template <typename T>
  T* Find(const char* name) const {
    return form_->findChild<T*>(QLatin1String(name));
  }

  std::filesystem::path directory_;
  std::unique_ptr<CapturePlanForm> form_;
};

// --- What the profile decides is on offer ----------------------------------

// The plan's acceptance criterion: a CAV profile offers no time-code controls
// and a CLV profile offers no frame controls. Absent rather than disabled — a
// greyed-out field for a thing this disc does not have invites somebody to look
// for the setting that would turn it on.
TEST_F(CapturePlanFormTest, ACavDiscIsOfferedFramesAndNoTimeCodes) {
  Build(CavDisc());

  EXPECT_NE(Find<QSpinBox>(CapturePlanForm::kStartFrameSpinName), nullptr);
  EXPECT_NE(Find<QSpinBox>(CapturePlanForm::kEndFrameSpinName), nullptr);
  EXPECT_EQ(Find<QWidget>(CapturePlanForm::kStartTimeRowName), nullptr);
  EXPECT_EQ(Find<QWidget>(CapturePlanForm::kEndTimeRowName), nullptr);
}

TEST_F(CapturePlanFormTest, AClvDiscIsOfferedTimeCodesAndNoFrames) {
  Build(ClvDisc());

  EXPECT_NE(Find<QWidget>(CapturePlanForm::kStartTimeRowName), nullptr);
  EXPECT_NE(Find<QWidget>(CapturePlanForm::kEndTimeRowName), nullptr);
  EXPECT_EQ(Find<QSpinBox>(CapturePlanForm::kStartFrameSpinName), nullptr);
  EXPECT_EQ(Find<QSpinBox>(CapturePlanForm::kEndFrameSpinName), nullptr);
}

// A field per unit, each saying which unit it is, rather than one box wanting
// "0:00:00". A format somebody has to get right before the application will
// accept it is a format that will be got wrong.
TEST_F(CapturePlanFormTest, ATimeIsEnteredAsItsOwnFields) {
  Build(ClvDisc());

  auto* const minutes = Find<QSpinBox>(CapturePlanForm::kStartMinutesSpinName);
  auto* const seconds = Find<QSpinBox>(CapturePlanForm::kStartSecondsSpinName);
  ASSERT_NE(minutes, nullptr);
  ASSERT_NE(seconds, nullptr);

  EXPECT_FALSE(minutes->suffix().trimmed().isEmpty()) << "no unit on the box";
  EXPECT_FALSE(seconds->suffix().trimmed().isEmpty()) << "no unit on the box";

  // Seconds are seconds wherever they are. Minutes are bounded by the measured
  // length, on the same rule the frame boxes follow: a range that cannot exist
  // should not be typeable. This side is 50:45.
  EXPECT_EQ(seconds->maximum(), 59);
  EXPECT_EQ(minutes->maximum(), 50);
}

// Almost no LaserDisc side runs past the hour, and a box whose only legal value
// is zero is one somebody has to read, understand and then decide to ignore.
TEST_F(CapturePlanFormTest, AnHoursBoxAppearsOnlyForASideThatNeedsOne) {
  Build(ClvDisc());
  EXPECT_EQ(Find<QSpinBox>(CapturePlanForm::kStartHoursSpinName), nullptr);
  EXPECT_EQ(Find<QSpinBox>(CapturePlanForm::kEndHoursSpinName), nullptr);

  player::DiscProfile long_side = ClvDisc();
  long_side.programme_end.Record(1023000, player::Provenance::kMeasured);
  Build(long_side);

  auto* const hours = Find<QSpinBox>(CapturePlanForm::kStartHoursSpinName);
  ASSERT_NE(hours, nullptr) << "a 1:02:30 side has an hour to enter";
  EXPECT_EQ(hours->maximum(), 1);

  // And with an hours box beside it, the minutes box goes back to a full hour:
  // 1:15:00 is past the end, but 0:59:00 is not, and the two share a box.
  EXPECT_EQ(Find<QSpinBox>(CapturePlanForm::kStartMinutesSpinName)->maximum(),
            59);
}

TEST_F(CapturePlanFormTest, TheBoundsComeFromTheMeasuredLength) {
  Build(CavDisc());

  auto* const start = Find<QSpinBox>(CapturePlanForm::kStartFrameSpinName);
  auto* const end = Find<QSpinBox>(CapturePlanForm::kEndFrameSpinName);
  ASSERT_NE(start, nullptr);
  ASSERT_NE(end, nullptr);

  // A range that cannot exist cannot be typed, rather than being refused
  // several seconds after the disc has started spinning.
  EXPECT_EQ(start->minimum(), 1);
  EXPECT_EQ(start->maximum(), 54000);
  EXPECT_EQ(end->minimum(), 1);
  EXPECT_EQ(end->maximum(), 54000);
}

TEST_F(CapturePlanFormTest, TheDefaultIsTheWholeSideBetweenTheMeasuredEnds) {
  Build(CavDisc());

  const player::AutoCapturePlan plan = form_->Plan();
  EXPECT_EQ(plan.shape, player::CaptureShape::kWholeSide);
  EXPECT_EQ(plan.addressing, player::AddressMode::kFrame);
  EXPECT_EQ(plan.start_address, 1);
  EXPECT_EQ(plan.end_address, 54000);
  EXPECT_EQ(form_->problem(), player::PlanProblem::kNone);
}

TEST_F(CapturePlanFormTest, TheStartFieldIsOnlyLiveForARange) {
  Build(CavDisc());

  auto* const start = Find<QSpinBox>(CapturePlanForm::kStartFrameSpinName);
  auto* const end = Find<QSpinBox>(CapturePlanForm::kEndFrameSpinName);
  ASSERT_NE(start, nullptr);

  // The two spin-up shapes start where the programme does — the player is
  // started from a stop and arrives there by itself — so a start address the
  // user typed would describe nothing.
  EXPECT_FALSE(start->isEnabled());
  EXPECT_FALSE(end->isEnabled());

  Find<QRadioButton>(CapturePlanForm::kFromSpinUpRadioName)->setChecked(true);
  EXPECT_FALSE(start->isEnabled());
  EXPECT_TRUE(end->isEnabled());

  Find<QRadioButton>(CapturePlanForm::kRangeRadioName)->setChecked(true);
  EXPECT_TRUE(start->isEnabled());
  EXPECT_TRUE(end->isEnabled());
}

TEST_F(CapturePlanFormTest, ARangeTypedBackwardsIsSaidToBeWrongAtOnce) {
  Build(CavDisc());

  Find<QRadioButton>(CapturePlanForm::kRangeRadioName)->setChecked(true);
  Find<QSpinBox>(CapturePlanForm::kStartFrameSpinName)->setValue(30000);
  Find<QSpinBox>(CapturePlanForm::kEndFrameSpinName)->setValue(10000);

  EXPECT_EQ(form_->problem(), player::PlanProblem::kEndBeforeStart);

  auto* const problem = Find<QLabel>(CapturePlanForm::kProblemLabelName);
  ASSERT_NE(problem, nullptr);
  EXPECT_EQ(problem->text(),
            PlanProblemText(player::PlanProblem::kEndBeforeStart));
}

TEST_F(CapturePlanFormTest, AClvRangeIsReadAsTimeCodes) {
  Build(ClvDisc());

  Find<QRadioButton>(CapturePlanForm::kRangeRadioName)->setChecked(true);
  Find<QSpinBox>(CapturePlanForm::kStartMinutesSpinName)->setValue(10);
  Find<QSpinBox>(CapturePlanForm::kStartSecondsSpinName)->setValue(0);
  Find<QSpinBox>(CapturePlanForm::kEndMinutesSpinName)->setValue(20);
  Find<QSpinBox>(CapturePlanForm::kEndSecondsSpinName)->setValue(30);

  const player::AutoCapturePlan plan = form_->Plan();
  EXPECT_EQ(plan.addressing, player::AddressMode::kTimeCode);
  EXPECT_EQ(plan.start_address, 100000);
  EXPECT_EQ(plan.end_address, 203000);
  EXPECT_EQ(form_->problem(), player::PlanProblem::kNone);
}

// There is no longer such a thing as a malformed time here, and that is the
// point of the change: "1:99" was a thing somebody could type into one box and
// be told off for. A minute is a box that stops at 59, so the sentence about a
// malformed address has nothing left to describe on a CLV disc.
TEST_F(CapturePlanFormTest, ATimeThatIsNotATimeCannotBeEnteredAtAll) {
  Build(ClvDisc());

  Find<QRadioButton>(CapturePlanForm::kRangeRadioName)->setChecked(true);

  auto* const seconds = Find<QSpinBox>(CapturePlanForm::kEndSecondsSpinName);
  ASSERT_NE(seconds, nullptr);
  seconds->setValue(99);

  EXPECT_EQ(seconds->value(), 59) << "a second past 59 is not a second";
  EXPECT_NE(form_->problem(), player::PlanProblem::kMalformedAddress);
}

// --- What the examination could not say ------------------------------------

TEST_F(CapturePlanFormTest, AnUnknownStandardIsAskedForAndNothingElseIs) {
  player::DiscProfile disc = CavDisc();
  disc.video_standard = player::Fact<player::VideoStandard>{};
  Build(disc);

  auto* const standard = Find<QComboBox>(CapturePlanForm::kStandardComboName);
  ASSERT_NE(standard, nullptr);

  // A frame count is only a duration once the frame rate is known, so an
  // unexamined standard means no estimate at all — said rather than left blank.
  auto* const estimate = Find<QLabel>(CapturePlanForm::kEstimateLabelName);
  ASSERT_NE(estimate, nullptr);
  EXPECT_TRUE(estimate->text().contains(QStringLiteral("not known")));

  // Choosing one records it as declared rather than as something the player
  // said, and the estimate follows.
  standard->setCurrentIndex(
      standard->findData(static_cast<int>(player::VideoStandard::kPal)));

  EXPECT_EQ(form_->disc().video_standard.value, player::VideoStandard::kPal);
  EXPECT_EQ(form_->disc().video_standard.provenance,
            player::Provenance::kDeclared);
  EXPECT_FALSE(estimate->text().contains(QStringLiteral("not known")));
}

TEST_F(CapturePlanFormTest, AKnownStandardIsNotAskedAbout) {
  Build(CavDisc());

  // A fully examined disc is asked nothing at all, which is the whole
  // difference from the old application's form.
  EXPECT_EQ(Find<QComboBox>(CapturePlanForm::kStandardComboName), nullptr);
}

TEST_F(CapturePlanFormTest, AllThreeShapesAreAlwaysOnOffer) {
  player::DiscProfile disc = CavDisc();

  // Even where the examination could not seek to the start of the programme. No
  // command puts a player on the lead-in, so that reading says nothing about
  // whether the spin-up can be captured — it is captured by starting the file
  // before the disc, which needs only a player that can be stopped and started.
  disc.lead_in_reachable.Record(false, player::Provenance::kMeasured);
  Build(disc);

  for (const char* const name :
       {CapturePlanForm::kWholeSideRadioName, CapturePlanForm::kRangeRadioName,
        CapturePlanForm::kFromSpinUpRadioName}) {
    EXPECT_TRUE(Find<QRadioButton>(name)->isEnabled()) << name;
  }

  EXPECT_TRUE(
      Find<QRadioButton>(CapturePlanForm::kWholeSideRadioName)->isChecked());
  EXPECT_EQ(form_->problem(), player::PlanProblem::kNone);
}

TEST_F(CapturePlanFormTest, TheKeyLockIsOffUntilItIsAskedFor) {
  Build(CavDisc());

  auto* const key_lock = Find<QCheckBox>(CapturePlanForm::kKeyLockCheckName);
  ASSERT_NE(key_lock, nullptr);
  EXPECT_FALSE(key_lock->isChecked());
  EXPECT_FALSE(form_->Plan().key_lock);

  key_lock->setChecked(true);
  EXPECT_TRUE(form_->Plan().key_lock);
}

TEST_F(CapturePlanFormTest, TheCouplingPreferenceIsHereWithItsDefault) {
  Build(CavDisc());

  // Off, and a considered default: a player that stumbles partway through a
  // side would otherwise truncate a capture that was going perfectly well.
  EXPECT_FALSE(form_->stop_capture_with_player());
  EXPECT_FALSE(
      Find<QCheckBox>(CapturePlanForm::kStopCaptureCheckName)->isChecked());

  form_->SetStopCaptureWithPlayer(true);
  EXPECT_TRUE(form_->stop_capture_with_player());
}

// --- A profile that cannot be captured from --------------------------------

TEST_F(CapturePlanFormTest, AnUnmeasuredSideSaysSoRatherThanOfferingACapture) {
  player::DiscProfile disc = CavDisc();
  disc.programme_end = player::Fact<int32_t>{};
  Build(disc);

  EXPECT_EQ(form_->problem(), player::PlanProblem::kUnknownLength);
  EXPECT_EQ(Find<QLabel>(CapturePlanForm::kProblemLabelName)->text(),
            PlanProblemText(player::PlanProblem::kUnknownLength));
}

// --- Locking for a run -----------------------------------------------------

// Every control's state is set from scratch each time rather than being turned
// off when a capture starts and left to somebody to turn back on. A form whose
// fields stay dead after a run because two places disagreed about who
// re-enables them is the ordinary way this goes wrong.
TEST_F(CapturePlanFormTest, ARunLocksEveryControlAndReleasesThemAgain) {
  Build(CavDisc());
  Find<QRadioButton>(CapturePlanForm::kRangeRadioName)->setChecked(true);

  form_->SetEditable(false);

  for (const char* const name :
       {CapturePlanForm::kWholeSideRadioName, CapturePlanForm::kRangeRadioName,
        CapturePlanForm::kFromSpinUpRadioName}) {
    EXPECT_FALSE(Find<QRadioButton>(name)->isEnabled()) << name;
  }
  EXPECT_FALSE(
      Find<QSpinBox>(CapturePlanForm::kStartFrameSpinName)->isEnabled());
  EXPECT_FALSE(
      Find<QCheckBox>(CapturePlanForm::kKeyLockCheckName)->isEnabled());

  form_->SetEditable(true);

  EXPECT_TRUE(
      Find<QRadioButton>(CapturePlanForm::kWholeSideRadioName)->isEnabled());
  EXPECT_TRUE(Find<QCheckBox>(CapturePlanForm::kKeyLockCheckName)->isEnabled());

  // And the shape it was left on still decides which address fields are live,
  // rather than everything coming back enabled.
  EXPECT_TRUE(
      Find<QSpinBox>(CapturePlanForm::kStartFrameSpinName)->isEnabled());
}

}  // namespace
}  // namespace ddd::gui
