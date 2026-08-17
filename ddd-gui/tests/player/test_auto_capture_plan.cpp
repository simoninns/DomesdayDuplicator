/************************************************************************

    test_auto_capture_plan.cpp

    T1 tests for what makes an automatic capture plan impossible
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include "auto_capture_plan.h"

namespace ddd::player {
namespace {

// An NTSC CAV side of 54,000 frames, examined in full.
DiscProfile CavDisc() {
  DiscProfile disc;
  disc.disc_present.Record(true, Provenance::kReported);
  disc.disc_type.Record(DiscType::kCav, Provenance::kReported);
  disc.addressing.Record(AddressMode::kFrame, Provenance::kInferred);
  disc.programme_start.Record(1, Provenance::kMeasured);
  disc.programme_end.Record(54000, Provenance::kMeasured);
  disc.lead_in_reachable.Record(true, Provenance::kMeasured);
  disc.video_standard.Record(VideoStandard::kNtsc, Provenance::kReported);
  return disc;
}

// A PAL CLV side running to 0:50:45.
DiscProfile ClvDisc() {
  DiscProfile disc;
  disc.disc_present.Record(true, Provenance::kReported);
  disc.disc_type.Record(DiscType::kClv, Provenance::kReported);
  disc.addressing.Record(AddressMode::kTimeCode, Provenance::kInferred);
  disc.programme_start.Record(0, Provenance::kMeasured);
  disc.programme_end.Record(504500, Provenance::kMeasured);
  disc.lead_in_reachable.Record(true, Provenance::kMeasured);
  disc.video_standard.Record(VideoStandard::kPal, Provenance::kReported);
  return disc;
}

TEST(AutoCapturePlan, TheDefaultForAnExaminedDiscIsTheWholeSide) {
  const DiscProfile disc = CavDisc();
  const AutoCapturePlan plan = DefaultPlanFor(disc);

  EXPECT_EQ(plan.shape, CaptureShape::kWholeSide);
  EXPECT_EQ(plan.addressing, AddressMode::kFrame);
  EXPECT_EQ(plan.start_address, 1);
  EXPECT_EQ(plan.end_address, 54000);
  EXPECT_EQ(ValidateAutoCapturePlan(plan, disc), PlanProblem::kNone);

  // Not on by default, because it leaves the player deaf to its own front
  // panel and nobody asked for that.
  EXPECT_FALSE(plan.key_lock);
}

TEST(AutoCapturePlan, EveryShapeIsRunnableOnAFullyExaminedDisc) {
  const DiscProfile disc = CavDisc();

  for (const CaptureShape shape :
       {CaptureShape::kWholeSide, CaptureShape::kRange,
        CaptureShape::kFromSpinUp}) {
    AutoCapturePlan plan = DefaultPlanFor(disc);
    plan.shape = shape;
    plan.start_address = 1;
    plan.end_address = 30000;
    EXPECT_EQ(ValidateAutoCapturePlan(plan, disc), PlanProblem::kNone)
        << "shape " << static_cast<int>(shape);
  }
}

TEST(AutoCapturePlan, ARangeTypedBackwardsIsRefused) {
  const DiscProfile disc = CavDisc();

  AutoCapturePlan plan = DefaultPlanFor(disc);
  plan.shape = CaptureShape::kRange;
  plan.start_address = 30000;
  plan.end_address = 10000;

  EXPECT_EQ(ValidateAutoCapturePlan(plan, disc), PlanProblem::kEndBeforeStart);
}

TEST(AutoCapturePlan, AZeroLengthSpinUpCaptureIsRefused) {
  const DiscProfile disc = CavDisc();

  // "From the spin-up, for no frames at all" — the same fault as the range
  // typed backwards, and reported as such rather than as its own kind of
  // nothing.
  AutoCapturePlan plan = DefaultPlanFor(disc);
  plan.shape = CaptureShape::kFromSpinUp;
  plan.start_address = 1;
  plan.end_address = 1;

  EXPECT_EQ(ValidateAutoCapturePlan(plan, disc), PlanProblem::kEndBeforeStart);
}

TEST(AutoCapturePlan, ARangeBeyondTheMeasuredLengthIsRefused) {
  const DiscProfile disc = CavDisc();

  AutoCapturePlan plan = DefaultPlanFor(disc);
  plan.shape = CaptureShape::kRange;
  plan.start_address = 50000;
  plan.end_address = 60000;

  EXPECT_EQ(ValidateAutoCapturePlan(plan, disc),
            PlanProblem::kEndBeyondProgramme);
}

TEST(AutoCapturePlan, ARangeStartingBeforeTheProgrammeIsRefused) {
  DiscProfile disc = CavDisc();
  disc.programme_start.Record(100, Provenance::kMeasured);

  AutoCapturePlan plan = DefaultPlanFor(disc);
  plan.shape = CaptureShape::kRange;
  plan.start_address = 50;
  plan.end_address = 200;

  EXPECT_EQ(ValidateAutoCapturePlan(plan, disc),
            PlanProblem::kStartBeforeProgramme);
}

TEST(AutoCapturePlan, AddressingHasToMatchTheDisc) {
  const DiscProfile cav = CavDisc();
  const DiscProfile clv = ClvDisc();

  // The plan the CLV disc produced, run against the CAV one. Both are perfectly
  // valid plans; each is nonsense on the other disc.
  AutoCapturePlan clv_plan = DefaultPlanFor(clv);
  EXPECT_EQ(ValidateAutoCapturePlan(clv_plan, cav),
            PlanProblem::kAddressingMismatch);

  AutoCapturePlan cav_plan = DefaultPlanFor(cav);
  EXPECT_EQ(ValidateAutoCapturePlan(cav_plan, clv),
            PlanProblem::kAddressingMismatch);
}

TEST(AutoCapturePlan, ADiscWhoseTypeWasNeverEstablishedCannotBePlanned) {
  DiscProfile disc = CavDisc();
  disc.disc_type = Fact<DiscType>{};

  EXPECT_EQ(ValidateAutoCapturePlan(DefaultPlanFor(CavDisc()), disc),
            PlanProblem::kUnknownDiscType);
}

TEST(AutoCapturePlan, ADiscWhoseEndWasNeverMeasuredCannotBePlanned) {
  DiscProfile disc = CavDisc();
  disc.programme_end = Fact<int32_t>{};

  AutoCapturePlan plan = DefaultPlanFor(CavDisc());
  EXPECT_EQ(ValidateAutoCapturePlan(plan, disc), PlanProblem::kUnknownLength);
}

TEST(AutoCapturePlan, AnEmptyTrayIsSaidSoRatherThanReportedAsABadRange) {
  DiscProfile disc = CavDisc();
  disc.disc_present.Record(false, Provenance::kMeasured);

  EXPECT_EQ(ValidateAutoCapturePlan(DefaultPlanFor(CavDisc()), disc),
            PlanProblem::kNoDisc);
}

TEST(AutoCapturePlan, AnUnexaminedDiscPresenceIsNotTakenAsAnEmptyTray) {
  DiscProfile disc = CavDisc();
  disc.disc_present = Fact<bool>{};

  // The player never said, which is not the same as saying no — and a disc that
  // seeks and measures perfectly well should not be refused on the strength of
  // one query nobody answered.
  EXPECT_EQ(ValidateAutoCapturePlan(DefaultPlanFor(disc), disc),
            PlanProblem::kNone);
}

TEST(AutoCapturePlan, AnUnreachableLeadInRefusesNothing) {
  DiscProfile disc = CavDisc();

  // The examination could not seek to the start of the programme. That says
  // nothing about whether the spin-up can be captured, because **no command
  // puts a player on the lead-in** — the two shapes that hold it get it by
  // starting the capture before the disc, which needs only a player that can be
  // stopped and started. Refusing them on this reading would refuse discs that
  // capture perfectly well.
  disc.lead_in_reachable.Record(false, Provenance::kMeasured);

  for (const CaptureShape shape :
       {CaptureShape::kWholeSide, CaptureShape::kRange,
        CaptureShape::kFromSpinUp}) {
    AutoCapturePlan plan = DefaultPlanFor(disc);
    plan.shape = shape;
    EXPECT_EQ(ValidateAutoCapturePlan(plan, disc), PlanProblem::kNone)
        << "shape " << static_cast<int>(shape);
  }
}

TEST(AutoCapturePlan, TheShapesSayWhereTheSpinUpAndSpinDownGo) {
  // The whole of what distinguishes the three, and both halves are easy to get
  // backwards: the spin-up reaches a file only by the capture starting first,
  // and the run-out only by the capture stopping last.
  EXPECT_TRUE(BeginsWithSpinUp(CaptureShape::kWholeSide));
  EXPECT_TRUE(BeginsWithSpinUp(CaptureShape::kFromSpinUp));
  EXPECT_FALSE(BeginsWithSpinUp(CaptureShape::kRange));

  EXPECT_TRUE(EndsWithSpinDown(CaptureShape::kWholeSide));
  EXPECT_FALSE(EndsWithSpinDown(CaptureShape::kRange));
  EXPECT_FALSE(EndsWithSpinDown(CaptureShape::kFromSpinUp));
}

TEST(AutoCapturePlan, NegativeAddressesAreRefused) {
  const DiscProfile disc = CavDisc();

  AutoCapturePlan plan = DefaultPlanFor(disc);
  plan.start_address = -1;

  EXPECT_EQ(ValidateAutoCapturePlan(plan, disc),
            PlanProblem::kMalformedAddress);
}

TEST(AutoCapturePlan, TheDurationOfACavRangeNeedsTheVideoStandard) {
  DiscProfile disc = CavDisc();

  AutoCapturePlan plan = DefaultPlanFor(disc);
  plan.shape = CaptureShape::kRange;
  plan.start_address = 1;
  plan.end_address = 1800;

  // 1800 frames at 30000/1001 is a hair over sixty seconds. Compared whole, so
  // a duration that was never worked out fails here rather than needing its own
  // assertion.
  EXPECT_EQ(PlannedDuration(plan, disc), std::chrono::seconds{60});

  // The same range on a disc nobody established the standard of yields nothing
  // rather than a figure that would be a fifth out on a PAL disc.
  disc.video_standard = Fact<VideoStandard>{};
  EXPECT_FALSE(PlannedDuration(plan, disc).has_value());
}

TEST(AutoCapturePlan, TheDurationOfAClvRangeIsReadStraightOffTheTimeCodes) {
  DiscProfile disc = ClvDisc();

  AutoCapturePlan plan = DefaultPlanFor(disc);
  plan.shape = CaptureShape::kRange;
  plan.start_address = 100000;  // 0:10:00
  plan.end_address = 203000;    // 0:20:30

  const std::optional<std::chrono::seconds> duration =
      PlannedDuration(plan, disc);
  EXPECT_EQ(duration, std::chrono::seconds{(10 * 60) + 30});

  // And it does not depend on the standard, because the addresses are already
  // times.
  disc.video_standard = Fact<VideoStandard>{};
  EXPECT_EQ(PlannedDuration(plan, disc), duration);
}

}  // namespace
}  // namespace ddd::player
