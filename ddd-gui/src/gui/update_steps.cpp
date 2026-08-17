/************************************************************************

    update_steps.cpp

    The list of steps an update will take, and where in them it has got to
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "update_steps.h"

#include <QCoreApplication>
#include <algorithm>
#include <utility>

namespace ddd::gui {
namespace {

QString Translate(const char* text) {
  return QCoreApplication::translate("UpdateSteps", text);
}

// What each of the short steps is worth, in the same seconds the component
// estimates are in, so that one weighting covers all of them.
//
// The two ends of an update take no measurable time at all — checking a
// signature is milliseconds and reading an identity back is one control
// transfer — but a step worth nothing would be a step the bar skipped straight
// past, and a user watching for their step to light up would never see it. A
// second each is the smallest share that is still visibly a share.
constexpr double kCheckSeconds = 1.0;
constexpr double kConfirmSeconds = 1.0;

// Waking a device that has no firmware: a few tens of kilobytes into RAM over
// the boot ROM's own protocol, then waiting for it to re-enumerate.
constexpr double kPrepareSeconds = 6.0;

// How an install step's share is divided between the three stages the engine
// reports inside it.
//
// The transfer has nearly all of it because on both media the device writes
// each chunk as it arrives — there is no separate write to wait for, and the
// stage the engine calls kWriting is only the tail of the last chunk. The
// read-back is the visible remainder, and on the gateware it is a real
// fraction of the time rather than a formality.
constexpr double kTransferBand = 0.70;
constexpr double kWriteBand = 0.80;

// Proportion of a total, clamped, and safe when the total is zero.
double Proportion(uint64_t done, uint64_t total) {
  if (total == 0) {
    return 0.0;
  }
  return std::clamp(static_cast<double>(done) / static_cast<double>(total), 0.0,
                    1.0);
}

QString StepTitle(UpdateStepKind kind) {
  switch (kind) {
    case UpdateStepKind::kCheck:
      return Translate("Check the update file");
    case UpdateStepKind::kPrepare:
      return Translate("Start the device up");
    case UpdateStepKind::kFirmware:
      return Translate("Install the firmware");
    case UpdateStepKind::kGateware:
      return Translate("Install the gateware");
    case UpdateStepKind::kRestart:
      return Translate("Restart the device");
    case UpdateStepKind::kConfirm:
      return Translate("Confirm the new version");
  }
  return Translate("Working");
}

}  // namespace

std::optional<UpdateStepKind> StepForStage(capture::UpdateStage stage,
                                           capture::UpdateTarget target) {
  const bool gateware = target == capture::UpdateTarget::kGateware;

  switch (stage) {
    case capture::UpdateStage::kChecking:
      return UpdateStepKind::kCheck;
    case capture::UpdateStage::kPreparing:
      return UpdateStepKind::kPrepare;

    // The three stages a bundle carrying both halves visits twice. Which step
    // they belong to is the target, which is exactly why the engine reports
    // one.
    case capture::UpdateStage::kTransferring:
    case capture::UpdateStage::kWriting:
    case capture::UpdateStage::kVerifying:
      return gateware ? UpdateStepKind::kGateware : UpdateStepKind::kFirmware;

    case capture::UpdateStage::kRestarting:
      return UpdateStepKind::kRestart;
    case capture::UpdateStage::kConfirming:
      return UpdateStepKind::kConfirm;

    // Not steps. They are what the list looks like once the steps are over.
    case capture::UpdateStage::kComplete:
    case capture::UpdateStage::kFailed:
      return std::nullopt;
  }
  return std::nullopt;
}

double InstallStepProportion(capture::UpdateStage stage, uint64_t done,
                             uint64_t total) {
  switch (stage) {
    case capture::UpdateStage::kTransferring:
      return kTransferBand * Proportion(done, total);

    case capture::UpdateStage::kWriting:
      return kTransferBand +
             ((kWriteBand - kTransferBand) * Proportion(done, total));

    case capture::UpdateStage::kVerifying:
      return kWriteBand + ((1.0 - kWriteBand) * Proportion(done, total));

    default:
      return 0.0;
  }
}

std::vector<UpdateStep> PlanUpdateSteps(const capture::UpdateManifest& manifest,
                                        bool from_recovery) {
  struct Planned {
    UpdateStepKind kind;
    double seconds;
  };

  std::vector<Planned> planned;
  planned.push_back({UpdateStepKind::kCheck, kCheckSeconds});

  if (from_recovery) {
    planned.push_back({UpdateStepKind::kPrepare, kPrepareSeconds});
  }

  if (manifest.firmware.has_value()) {
    planned.push_back(
        {UpdateStepKind::kFirmware,
         capture::EstimateComponentSeconds(capture::UpdateTarget::kFirmware,
                                           *manifest.firmware)});
  }
  if (manifest.gateware.has_value()) {
    planned.push_back(
        {UpdateStepKind::kGateware,
         capture::EstimateComponentSeconds(capture::UpdateTarget::kGateware,
                                           *manifest.gateware)});
  }

  planned.push_back({UpdateStepKind::kRestart,
                     static_cast<double>(capture::kUpdateRestartSeconds)});
  planned.push_back({UpdateStepKind::kConfirm, kConfirmSeconds});

  double total = 0.0;
  for (const Planned& step : planned) {
    // A component of zero length is not a step worth no time; it is a step
    // that will still be announced, so it keeps the floor every short step
    // gets. Without this a manifest with an empty payload would divide by a
    // total of zero below.
    total += std::max(step.seconds, kCheckSeconds);
  }

  std::vector<UpdateStep> steps;
  steps.reserve(planned.size());
  for (const Planned& step : planned) {
    UpdateStep out;
    out.kind = step.kind;
    out.title = StepTitle(step.kind);
    out.share = std::max(step.seconds, kCheckSeconds) / total;
    steps.push_back(std::move(out));
  }

  return steps;
}

UpdateProgressTracker::UpdateProgressTracker(std::vector<UpdateStep> steps)
    : steps_(std::move(steps)) {}

void UpdateProgressTracker::Reset() {
  current_ = -1;
  percent_ = 0;
}

double UpdateProgressTracker::ShareBefore(int index) const {
  double before = 0.0;
  for (int step = 0; step < index && step < static_cast<int>(steps_.size());
       ++step) {
    before += steps_[step].share;
  }
  return before;
}

UpdateProgressTracker::Position UpdateProgressTracker::Fold(
    capture::UpdateStage stage, capture::UpdateTarget target, uint64_t done,
    uint64_t total) {
  const std::optional<UpdateStepKind> kind = StepForStage(stage, target);
  if (!kind.has_value()) {
    return position();
  }

  const auto found = std::find_if(
      steps_.begin(), steps_.end(),
      [kind](const UpdateStep& step) { return step.kind == *kind; });
  if (found == steps_.end()) {
    // A stage for a step the plan does not have. Possible where the device
    // turned out to need a path the manifest did not describe, and not a
    // reason to move the bar to somewhere it cannot justify.
    return position();
  }

  const auto index = static_cast<int>(std::distance(steps_.begin(), found));

  // Forwards only. The engine's byte counts restart at zero for the second
  // component, and a step index that could go back would put the highlight on
  // a step that has already finished.
  current_ = std::max(current_, index);

  // Where the bar would be if this report were the whole truth. Steps with no
  // meaningful proportion — checking a signature, waiting for a device to
  // re-enumerate — sit at the start of their own share and stay there until
  // the next step reports. That is deliberate: nothing here invents motion it
  // cannot measure, and the step list is what says the update is still alive
  // while the bar is honest about having nothing new to say.
  const double within = InstallStepProportion(stage, done, total);
  const double overall = ShareBefore(index) + (steps_[index].share * within);

  percent_ =
      std::max(percent_, std::clamp(static_cast<int>(overall * 100.0), 0, 100));

  return position();
}

UpdateProgressTracker::Position UpdateProgressTracker::Complete() {
  current_ = static_cast<int>(steps_.size());
  percent_ = 100;
  return position();
}

}  // namespace ddd::gui
