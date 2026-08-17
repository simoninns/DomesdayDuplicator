/************************************************************************

    auto_capture_plan.cpp

    What a guided capture setup produces, and what makes one impossible
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "auto_capture_plan.h"

namespace ddd::player {

PlanProblem ValidateAutoCapturePlan(const AutoCapturePlan& plan,
                                    const DiscProfile& disc) {
  // Only a disc the examination positively found to be absent. An unexamined
  // field is not a finding, and refusing on it would refuse every disc in a
  // player that will not answer the disc-status query but seeks perfectly well.
  if (disc.disc_present.known() && !disc.disc_present.value) {
    return PlanProblem::kNoDisc;
  }

  if (!disc.disc_type.known()) {
    return PlanProblem::kUnknownDiscType;
  }

  if (plan.addressing != AddressModeFor(disc.disc_type.value)) {
    return PlanProblem::kAddressingMismatch;
  }

  // Required for every shape, not only the ones that run to the end. It is what
  // bounds the entry fields, and a range that cannot be checked against the
  // disc is a range that can be typed past the end of it — which is the failure
  // the whole examine step exists to make impossible.
  if (!disc.programme_end.known()) {
    return PlanProblem::kUnknownLength;
  }

  if (plan.start_address < 0 || plan.end_address < 0) {
    return PlanProblem::kMalformedAddress;
  }

  if (plan.end_address <= plan.start_address) {
    return PlanProblem::kEndBeforeStart;
  }

  // Both addressing schemes order numerically — a time code is fixed-width and
  // zero-padded field by field, so comparing two of them as integers gives the
  // same answer as comparing them as times.
  if (disc.programme_start.known() &&
      plan.start_address < disc.programme_start.value) {
    return PlanProblem::kStartBeforeProgramme;
  }

  if (plan.end_address > disc.programme_end.value) {
    return PlanProblem::kEndBeyondProgramme;
  }

  // Nothing here asks whether the lead-in can be reached, and nothing should:
  // no command puts a player there. The two shapes that hold it get it by
  // starting the capture before the disc, which works on any player that can be
  // stopped and started — and the examination's lead_in_reachable, which is
  // about a *seek* to the start of the programme, says nothing about that.
  return PlanProblem::kNone;
}

std::optional<std::chrono::seconds> PlannedDuration(const AutoCapturePlan& plan,
                                                    const DiscProfile& disc) {
  if (!disc.disc_type.known()) {
    return std::nullopt;
  }

  // The programme span only. The spin-up that two of the shapes also capture,
  // and the spin-down that one of them does, are deliberately not added: they
  // are ten seconds on one player and rather more on another, and a figure
  // invented here would be impossible to check. The estimate is therefore
  // slightly under for those shapes, which is the direction that does not tell
  // somebody a capture will fit when it will not.
  return AddressSpanDuration(plan.start_address, plan.end_address,
                             disc.disc_type.value, disc.video_standard.value);
}

AutoCapturePlan DefaultPlanFor(const DiscProfile& disc) {
  AutoCapturePlan plan;
  plan.shape = CaptureShape::kWholeSide;

  if (disc.disc_type.known()) {
    plan.addressing = AddressModeFor(disc.disc_type.value);
  } else if (disc.addressing.known()) {
    plan.addressing = disc.addressing.value;
  }

  // The measured ends, or the first address there is for a disc whose start was
  // never established — which is where a disc begins, and is what the
  // examination would have measured had it got that far.
  plan.start_address =
      disc.programme_start.known() ? disc.programme_start.value : 0;
  plan.end_address = disc.programme_end.known() ? disc.programme_end.value : 0;

  return plan;
}

}  // namespace ddd::player
