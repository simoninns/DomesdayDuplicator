/************************************************************************

    update_steps.h

    The list of steps an update will take, and where in them it has got to
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <cstdint>
#include <optional>
#include <vector>

#include "device_updater.h"
#include "update_manifest.h"
#include "update_orchestrator.h"

namespace ddd::gui {

// An update is a procedure with a known shape, so the interface shows the
// whole of it before it starts: every step listed and greyed, the one in hand
// picked out, and a single bar that fills once across all of them.
//
// That is a different arrangement from a bar per stage, and it is the right
// one for the question a user is actually asking. "How far through the whole
// thing am I" cannot be read off a bar that restarts at every stage boundary,
// and the stage names on their own do not say how many are left. A greyed list
// answers both at a glance: what will happen, what is happening, what is done.
//
// The arithmetic that decides how much of the bar each step is worth is here
// rather than in the widget, so that it can be checked without a window — and
// so that it is derived from the same per-component estimate the dialog prints
// before the update starts, rather than from a second guess that would
// disagree with it.

// The steps, as a user sees them named.
//
// Coarser than capture::UpdateStage deliberately. Sending a payload, the
// device writing it, and the device reading it back are three stages to the
// engine and one *step* to somebody watching: "install the firmware". Listing
// all three would make an ordinary update look like nine things to get
// through, and the two extra names buy nothing — they are visible in the
// detail line under the bar and in the rolling log, which is where somebody
// who wants that level of detail will look for it.
enum class UpdateStepKind {
  // Reading the file, checking its signature and every digest.
  kCheck,

  // Waking a device that has no working firmware. Only in the plan when the
  // device is in recovery.
  kPrepare,

  kFirmware,
  kGateware,

  kRestart,
  kConfirm,
};

// One step in the list.
struct UpdateStep {
  UpdateStepKind kind = UpdateStepKind::kCheck;

  // What the step is called, in the same plain words the documentation uses.
  QString title;

  // This step's share of the one bar, as a proportion in (0, 1]. The shares
  // across a plan sum to 1.
  //
  // Weighted by how long each step is expected to take rather than split
  // evenly, because they differ by two orders of magnitude: an update carrying
  // gateware spends minutes on that one step and milliseconds on the check
  // either side of it. An evenly split bar would sprint to four fifths and
  // then sit still, which is the specific failure a single bar has to avoid.
  double share = 0.0;
};

// The steps this update will take, in the order they will happen.
//
// Derived from what the bundle actually carries and what state the device is
// in, so the list is the truth about this update rather than a generic
// procedure with steps in it that will not run. A bundle with no gateware in
// it does not show a gateware step to be skipped.
std::vector<UpdateStep> PlanUpdateSteps(const capture::UpdateManifest& manifest,
                                        bool from_recovery);

// Which step an engine stage belongs to, if any. Empty for the two terminal
// stages, which are not steps: they are what the list looks like afterwards.
std::optional<UpdateStepKind> StepForStage(capture::UpdateStage stage,
                                           capture::UpdateTarget target);

// Folds the engine's per-stage reports into the two numbers the screen needs:
// which step is in hand, and how far along the single bar is.
//
// Monotonic by construction. Neither number ever goes backwards, whatever
// order reports arrive in — a bar that slipped back would be read as an update
// going wrong, and the engine legitimately reports a stage's byte counts
// restarting from zero when it moves on to the second component.
class UpdateProgressTracker {
 public:
  struct Position {
    // Index into the plan, or -1 before anything has been reported and for a
    // stage the plan has no step for.
    int step = -1;

    // Across the whole update, 0 to 100.
    int percent = 0;
  };

  UpdateProgressTracker() = default;
  explicit UpdateProgressTracker(std::vector<UpdateStep> steps);

  // Back to the beginning, keeping the plan. What "Try again" needs.
  void Reset();

  const std::vector<UpdateStep>& steps() const { return steps_; }

  // Take one report from the engine and say where the update now is.
  Position Fold(capture::UpdateStage stage, capture::UpdateTarget target,
                uint64_t done, uint64_t total);

  // Where it is, without a report to fold.
  Position position() const { return {current_, percent_}; }

  // Everything done, bar full. What a successful finish leaves behind.
  Position Complete();

 private:
  // Where in the plan a step's share begins, as a proportion of the whole.
  double ShareBefore(int index) const;

  std::vector<UpdateStep> steps_;
  int current_ = -1;
  int percent_ = 0;
};

// How far through *one* install step a stage and its byte count place the
// update, as a proportion in [0, 1].
//
// Exposed for the tests: the three sub-phases of an install step get fixed
// bands of that step's share, and it is worth being able to check that they
// are ordered and that none of them can hand back a proportion that would move
// the bar backwards.
double InstallStepProportion(capture::UpdateStage stage, uint64_t done,
                             uint64_t total);

}  // namespace ddd::gui
