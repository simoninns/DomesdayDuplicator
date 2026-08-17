/************************************************************************

    auto_capture_plan.h

    What a guided capture setup produces, and what makes one impossible
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "disc_profile.h"
#include "player_state.h"

namespace ddd::player {

// The three shapes a capture comes in.
//
// **There is no command that puts a player on the lead-in**, and that is what
// decides this list. A player can be sent to an address and it can be started
// from a stop; the lead-in is not an address, so the only way it reaches a file
// is for the capture to be running while the disc spins up. It is therefore
// never something the user asks for — it is something two of these three shapes
// get by construction, and the third cannot have at any price.
//
// The same holds at the other end. The run-out is not an address either: it is
// what the disc does while it is being stopped, so it reaches a file only if
// the capture is still running when the player is spun down.
enum class CaptureShape : uint8_t {
  // Start the capture, spin the disc up, run to the end of the side, and spin
  // it
  // down again **before** the capture is stopped.
  //
  // What almost every capture is, and the only shape whose file holds the whole
  // of the side: the spin-up at one end and the spin-down at the other.
  kWholeSide,

  // Seek to an address, start the capture, play to another address.
  //
  // The disc is already turning throughout, so this is the one shape with
  // neither a spin-up nor a spin-down in the file.
  kRange,

  // Start the capture, spin the disc up, and stop at an address.
  //
  // The front of a whole-side capture. What somebody takes when they want the
  // disc's own identifying data and a sample of the picture without waiting out
  // a side.
  kFromSpinUp,
};

// Does this shape begin with the disc stopped?
//
// The one piece of ordering in the whole automatic capture that cannot be
// worked out by reading the code that does it, which is why it is named here
// rather than left as a condition inside the sequence: a capture that is to
// hold the spin-up has to be running before the player is.
constexpr bool BeginsWithSpinUp(CaptureShape shape) {
  return shape == CaptureShape::kWholeSide ||
         shape == CaptureShape::kFromSpinUp;
}

// Does this shape end by spinning the disc down while still writing?
//
// The mirror of the above, and just as easy to get backwards. A whole-side
// capture that stopped writing and then stopped the player would end a few
// seconds early, missing exactly the part of the disc nothing else can reach.
constexpr bool EndsWithSpinDown(CaptureShape shape) {
  return shape == CaptureShape::kWholeSide;
}

// What the guided setup decided.
//
// A plain value with no Qt in it, so the rules below are the same rules the
// dialog enables its OK button with and the sequence checks its preconditions
// against — rather than two copies of them that drift.
struct AutoCapturePlan {
  CaptureShape shape = CaptureShape::kWholeSide;

  // How the two addresses below are written. It stands for the disc type as
  // well: frames are a CAV disc and time codes are a CLV one, and a plan whose
  // addressing disagrees with the disc in the player is refused rather than
  // guessed at.
  AddressMode addressing = AddressMode::kFrame;

  // Where the programme is to be captured from.
  //
  // Only kRange seeks to it. For the two shapes that begin with a spin-up the
  // player is started from a stop and arrives here on its own, and this is
  // carried so the estimate of what the capture will cost has both ends of the
  // span.
  int32_t start_address = 0;

  // The address at which the capture stops. Reaching it, or running into the
  // lead-out, is what ends the run.
  //
  // For kWholeSide this is the measured end of the side, and reaching it starts
  // the spin-down rather than stopping the capture.
  int32_t end_address = 0;

  // Lock the player's front panel for the duration, so a hand on the machine
  // cannot pause a capture halfway through a side.
  //
  // Not defaulted on: it leaves the player unresponsive to its own buttons, and
  // an application that did that without being asked would be one somebody has
  // to work out how to undo.
  bool key_lock = false;

  bool operator==(const AutoCapturePlan&) const = default;
};

// Why a plan cannot be run.
//
// Each of these is a different sentence to the user and, in the dialog, a
// different control to point at — which is why they are enumerated rather than
// collapsed into a bool. The old application had one message for all of them:
// "The disc in the player does not match the selected capture option", arriving
// several seconds after the disc had started spinning.
enum class PlanProblem : uint8_t {
  kNone,

  // The examination found no disc, so there is nothing to plan a capture of.
  kNoDisc,

  // The disc type was never established, so nothing below can be decided: the
  // frame and time-code seeks are different commands with differently sized
  // addresses, and guessing which is the one mistake this library is arranged
  // to avoid.
  kUnknownDiscType,

  // A frame plan against a CLV disc, or a time-code plan against a CAV one.
  kAddressingMismatch,

  // The end of the side was never measured, so no bound can be checked and a
  // whole-side capture has nothing to stop at.
  kUnknownLength,

  // A negative address, which no disc has.
  kMalformedAddress,

  // The capture would contain nothing. Covers the zero-length lead-in capture
  // as well as the range typed backwards, because they are the same fault: the
  // address the capture stops at is not after the one it starts at.
  kEndBeforeStart,

  kStartBeforeProgramme,
  kEndBeyondProgramme,
};

// Can this plan be run against this disc?
//
// Pure, and that is what it is for: the dialog calls it on every keystroke to
// decide whether its start button is live, and the sequence calls it once
// before it sends anything. One set of rules, one place, and no way for the
// interface to offer a capture the sequence would refuse.
PlanProblem ValidateAutoCapturePlan(const AutoCapturePlan& plan,
                                    const DiscProfile& disc);

inline bool IsRunnable(const AutoCapturePlan& plan, const DiscProfile& disc) {
  return ValidateAutoCapturePlan(plan, disc) == PlanProblem::kNone;
}

// How long the captured span runs for, where that can be worked out.
//
// The same arithmetic ProgrammeDuration uses, applied to the plan's two ends
// rather than the disc's. Nothing for a CAV disc whose video standard was never
// established: a frame count is only a duration once the frame rate is known,
// and assuming thirty frames a second on a PAL disc is twenty per cent out — in
// the direction that fills the volume a capture was estimated to fit on.
std::optional<std::chrono::seconds> PlannedDuration(const AutoCapturePlan& plan,
                                                    const DiscProfile& disc);

// The plan a disc suggests when nothing has been asked for yet: the whole of
// the side, between the two addresses the examination measured.
//
// Where the examination could not establish the type or the end, the result is
// a plan that ValidateAutoCapturePlan refuses — which is the honest answer, and
// the dialog says which field is missing rather than offering a capture built
// on a default nobody chose.
AutoCapturePlan DefaultPlanFor(const DiscProfile& disc);

}  // namespace ddd::player
