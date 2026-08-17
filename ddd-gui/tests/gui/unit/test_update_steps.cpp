/************************************************************************

    test_update_steps.cpp

    T1 unit test for the update's step plan and the one bar over it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "update_steps.h"
#include "wire_protocol.h"

namespace ddd::gui {
namespace {

using capture::UpdateStage;
using capture::UpdateTarget;

// A bundle carrying both halves. The lengths are the realistic ones — the
// gateware is both larger and written to a much slower medium — because the
// weighting is the whole point of the plan and equal lengths would hide
// whether it happens at all.
capture::UpdateManifest BothHalves() {
  capture::UpdateManifest manifest;
  manifest.manifest_version = capture::kUpdateManifestVersion;
  manifest.version = "1.5.0";

  capture::UpdateComponent firmware;
  firmware.length = uint64_t{64} * 1024;
  manifest.firmware = firmware;

  capture::UpdateComponent gateware;
  gateware.length = uint64_t{368} * 1024;
  manifest.gateware = gateware;

  return manifest;
}

capture::UpdateManifest FirmwareOnly() {
  capture::UpdateManifest manifest = BothHalves();
  manifest.gateware.reset();
  return manifest;
}

std::vector<UpdateStepKind> Kinds(const std::vector<UpdateStep>& steps) {
  std::vector<UpdateStepKind> kinds;
  kinds.reserve(steps.size());
  for (const UpdateStep& step : steps) {
    kinds.push_back(step.kind);
  }
  return kinds;
}

// --- The plan --------------------------------------------------------------

TEST(UpdateStepsTest, ThePlanIsTheStepsThisBundleWillActuallyTake) {
  const std::vector<UpdateStep> steps = PlanUpdateSteps(BothHalves(), false);

  EXPECT_EQ(Kinds(steps),
            (std::vector<UpdateStepKind>{
                UpdateStepKind::kCheck, UpdateStepKind::kFirmware,
                UpdateStepKind::kGateware, UpdateStepKind::kRestart,
                UpdateStepKind::kConfirm}));
}

// A step that will not run is not in the list. Showing one and skipping it
// would be a list a user counted themselves through and got the wrong answer
// from.
TEST(UpdateStepsTest, ABundleWithNoGatewareHasNoGatewareStep) {
  const std::vector<UpdateStep> steps = PlanUpdateSteps(FirmwareOnly(), false);

  EXPECT_EQ(Kinds(steps),
            (std::vector<UpdateStepKind>{
                UpdateStepKind::kCheck, UpdateStepKind::kFirmware,
                UpdateStepKind::kRestart, UpdateStepKind::kConfirm}));
}

// A device with no firmware has one more thing to get through before the
// update proper, and it is the one a user is most likely to be anxious about.
TEST(UpdateStepsTest, ADeviceInRecoveryGainsTheStepThatWakesIt) {
  const std::vector<UpdateStep> steps = PlanUpdateSteps(BothHalves(), true);

  ASSERT_GE(steps.size(), 2u);
  EXPECT_EQ(steps[1].kind, UpdateStepKind::kPrepare);
}

TEST(UpdateStepsTest, EveryStepIsNamedInPlainLanguage) {
  for (const UpdateStep& step : PlanUpdateSteps(BothHalves(), true)) {
    EXPECT_FALSE(step.title.isEmpty());
  }
}

TEST(UpdateStepsTest, TheSharesAddUpToTheWholeBar) {
  double total = 0.0;
  for (const UpdateStep& step : PlanUpdateSteps(BothHalves(), true)) {
    EXPECT_GT(step.share, 0.0) << "a step worth none of the bar is a step the "
                                  "bar would skip straight past";
    total += step.share;
  }

  EXPECT_NEAR(total, 1.0, 1e-9);
}

// The reason the shares are weighted rather than split evenly: these two
// steps differ by two orders of magnitude in how long they take, and an
// evenly divided bar would sprint to four fifths and then sit still.
TEST(UpdateStepsTest, TheSlowStepIsWorthMoreOfTheBarThanTheFastOne) {
  const std::vector<UpdateStep> steps = PlanUpdateSteps(BothHalves(), false);

  const auto firmware =
      std::find_if(steps.begin(), steps.end(), [](const UpdateStep& step) {
        return step.kind == UpdateStepKind::kFirmware;
      });
  const auto gateware =
      std::find_if(steps.begin(), steps.end(), [](const UpdateStep& step) {
        return step.kind == UpdateStepKind::kGateware;
      });

  ASSERT_NE(firmware, steps.end());
  ASSERT_NE(gateware, steps.end());
  EXPECT_GT(gateware->share, firmware->share * 5.0)
      << "the gateware is six times the size and written to a medium six "
         "times slower, and the bar does not reflect it";
}

// --- Which stage belongs to which step -------------------------------------

// The three stages a bundle carrying both halves visits twice belong to
// different steps on each visit, and the target is what says which.
TEST(UpdateStepsTest, TheTargetDecidesWhichInstallStepAStageBelongsTo) {
  EXPECT_EQ(StepForStage(UpdateStage::kTransferring, UpdateTarget::kFirmware),
            UpdateStepKind::kFirmware);
  EXPECT_EQ(StepForStage(UpdateStage::kVerifying, UpdateTarget::kGateware),
            UpdateStepKind::kGateware);
  EXPECT_EQ(StepForStage(UpdateStage::kWriting, UpdateTarget::kGateware),
            UpdateStepKind::kGateware);
}

TEST(UpdateStepsTest, TheTerminalStagesAreNotSteps) {
  EXPECT_FALSE(StepForStage(UpdateStage::kComplete, UpdateTarget::kFirmware)
                   .has_value());
  EXPECT_FALSE(
      StepForStage(UpdateStage::kFailed, UpdateTarget::kFirmware).has_value());
}

// Inside one install step the three sub-phases run in order, so the
// proportion they hand back has to as well — otherwise the bar would jump
// back at each phase boundary.
TEST(UpdateStepsTest, TheSubPhasesOfAnInstallStepAreOrdered) {
  const double transferred =
      InstallStepProportion(UpdateStage::kTransferring, 100, 100);
  const double written = InstallStepProportion(UpdateStage::kWriting, 0, 100);
  const double verified =
      InstallStepProportion(UpdateStage::kVerifying, 0, 100);
  const double finished =
      InstallStepProportion(UpdateStage::kVerifying, 100, 100);

  EXPECT_LE(transferred, written);
  EXPECT_LE(written, verified);
  EXPECT_DOUBLE_EQ(finished, 1.0);
}

TEST(UpdateStepsTest, AStageWithNoTotalDoesNotDivideByZero) {
  EXPECT_DOUBLE_EQ(InstallStepProportion(UpdateStage::kTransferring, 0, 0),
                   0.0);
}

// --- The tracker -----------------------------------------------------------

TEST(UpdateProgressTrackerTest, ItStartsBeforeTheFirstStep) {
  const UpdateProgressTracker tracker(PlanUpdateSteps(BothHalves(), false));

  EXPECT_EQ(tracker.position().step, -1);
  EXPECT_EQ(tracker.position().percent, 0);
}

TEST(UpdateProgressTrackerTest, ItFollowsTheUpdateFromStepToStep) {
  UpdateProgressTracker tracker(PlanUpdateSteps(BothHalves(), false));

  EXPECT_EQ(
      tracker.Fold(UpdateStage::kChecking, UpdateTarget::kFirmware, 0, 0).step,
      0);
  EXPECT_EQ(
      tracker.Fold(UpdateStage::kTransferring, UpdateTarget::kFirmware, 0, 1024)
          .step,
      1);
  EXPECT_EQ(
      tracker.Fold(UpdateStage::kTransferring, UpdateTarget::kGateware, 0, 4096)
          .step,
      2);
  EXPECT_EQ(
      tracker.Fold(UpdateStage::kRestarting, UpdateTarget::kFirmware, 0, 0)
          .step,
      3);
  EXPECT_EQ(
      tracker.Fold(UpdateStage::kConfirming, UpdateTarget::kFirmware, 0, 0)
          .step,
      4);
}

// The one property the whole arrangement rests on. The engine's byte counts
// restart from zero when it moves on to the second component, and a bar that
// slipped back at that moment would be read as an update going wrong.
TEST(UpdateProgressTrackerTest, NeitherTheBarNorTheHighlightEverGoesBackwards) {
  UpdateProgressTracker tracker(PlanUpdateSteps(BothHalves(), false));

  int percent = 0;
  int step = -1;

  const auto advance = [&](UpdateStage stage, UpdateTarget target,
                           uint64_t done, uint64_t total) {
    const UpdateProgressTracker::Position position =
        tracker.Fold(stage, target, done, total);
    EXPECT_GE(position.percent, percent);
    EXPECT_GE(position.step, step);
    percent = position.percent;
    step = position.step;
  };

  advance(UpdateStage::kChecking, UpdateTarget::kFirmware, 0, 0);

  for (uint64_t done = 0; done <= 65536; done += 4096) {
    advance(UpdateStage::kTransferring, UpdateTarget::kFirmware, done, 65536);
  }
  advance(UpdateStage::kWriting, UpdateTarget::kFirmware, 65536, 65536);
  advance(UpdateStage::kVerifying, UpdateTarget::kFirmware, 65536, 65536);

  // And here is the moment that matters: a new component, counting from zero
  // again.
  for (uint64_t done = 0; done <= 376832; done += 32768) {
    advance(UpdateStage::kTransferring, UpdateTarget::kGateware, done, 376832);
  }
  advance(UpdateStage::kVerifying, UpdateTarget::kGateware, 376832, 376832);
  advance(UpdateStage::kRestarting, UpdateTarget::kFirmware, 0, 0);
  advance(UpdateStage::kConfirming, UpdateTarget::kFirmware, 0, 0);

  EXPECT_GT(percent, 90) << "the bar did not reach the end of the last step";
}

// A finished update leaves the bar full and the whole list behind it, which
// is what makes the list a record of what was done.
TEST(UpdateProgressTrackerTest, CompletionFillsTheBarAndPassesTheLastStep) {
  UpdateProgressTracker tracker(PlanUpdateSteps(BothHalves(), false));
  tracker.Fold(UpdateStage::kChecking, UpdateTarget::kFirmware, 0, 0);

  const UpdateProgressTracker::Position position = tracker.Complete();

  EXPECT_EQ(position.percent, 100);
  EXPECT_EQ(position.step, static_cast<int>(tracker.steps().size()));
}

// "Try again" runs the same plan from the beginning, so the tracker has to
// start again with it.
TEST(UpdateProgressTrackerTest, ResetKeepsThePlanAndForgetsTheProgress) {
  UpdateProgressTracker tracker(PlanUpdateSteps(BothHalves(), false));
  tracker.Fold(UpdateStage::kTransferring, UpdateTarget::kGateware, 4096, 8192);

  const size_t planned = tracker.steps().size();
  tracker.Reset();

  EXPECT_EQ(tracker.steps().size(), planned);
  EXPECT_EQ(tracker.position().step, -1);
  EXPECT_EQ(tracker.position().percent, 0);
}

// A report for a step this plan does not have — a device that turned out to
// need a path the manifest did not describe — moves nothing rather than
// moving the bar somewhere it cannot justify.
TEST(UpdateProgressTrackerTest, AStageOutsideThePlanIsIgnored) {
  UpdateProgressTracker tracker(PlanUpdateSteps(FirmwareOnly(), false));
  tracker.Fold(UpdateStage::kChecking, UpdateTarget::kFirmware, 0, 0);

  const UpdateProgressTracker::Position position = tracker.Fold(
      UpdateStage::kTransferring, UpdateTarget::kGateware, 0, 4096);

  EXPECT_EQ(position.step, 0);
}

}  // namespace
}  // namespace ddd::gui
