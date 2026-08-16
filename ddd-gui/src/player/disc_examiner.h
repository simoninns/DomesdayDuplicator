/************************************************************************

    disc_examiner.h

    Working out what is in the player, one step at a time
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "disc_profile.h"
#include "player_command.h"
#include "player_controls.h"
#include "player_definition.h"
#include "response_parser.h"

namespace ddd::player {

// What the examination is doing, in the order it does it.
//
// Named individually rather than counted, because these are what the progress
// line says and a user watching a player seek for five seconds is owed a
// sentence about why. The wording lives in the Qt layer; the sequence lives
// here.
enum class ExamineStage : uint8_t {
  kIdle,

  // "?P" — is there a player, and what is it doing?
  kCheckingPlayer,

  // Play, because almost nothing below can be answered by a stopped player.
  kSpinningUp,

  // "?D" — CAV or CLV, and the disc's size, side and chapters.
  kReadingDiscStatus,

  // "?S" — NTSC or PAL. Cheap and read-only, like the one above.
  kReadingTvSystem,

  // "?U". Expensive, and read here or not at all: it searches to the lead-in,
  // so it has to happen before anything has positioned the disc.
  kReadingPioneerUserCode,

  // "$Y" — cheap, and does not move the player.
  kReadingStandardUserCode,

  // A search for chapter one — the fallback for a model whose disc-status
  // reply does not carry the chapter field. Every Pioneer definition does carry
  // it, so this step is normally not in the plan at all: the disc has already
  // said, and asking again would move it.
  kCheckingChapters,

  // Seek to an address the disc cannot have, then ask where that landed. The
  // two halves of the length measurement.
  kFindingEnd,
  kReadingEnd,

  // The same trick at the other end, which also answers whether the lead-in can
  // be reached.
  kFindingStart,
  kReadingStart,

  // Leave the player still rather than playing. An examination that ended with
  // the disc running would be one the user has to go and stop.
  kSettling,

  kFinished,
};

// How the examination ended.
enum class ExamineOutcome : uint8_t {
  kInProgress,

  // Every step that could be run, was. It does not follow that every field is
  // known: a refused query leaves its field unknown and the examination carries
  // on, which is the whole design.
  kCompleted,

  // The tray is open. Nothing else can be asked.
  kTrayOpen,

  // The tray is shut and there is nothing in it, or nothing that will spin up.
  kNoDisc,

  // The link failed underneath the sequence. Distinct from the two above: those
  // are findings about the disc and this is a fault in the cable.
  kLinkFailed,

  kCancelled,
};

// One thing to do next.
struct ExamineStep {
  ExamineStage stage = ExamineStage::kIdle;
  PlayerCommand command = PlayerCommand::kQueryActiveMode;
  std::optional<int32_t> argument;

  bool operator==(const ExamineStep&) const = default;
};

// The examine sequence, as a value rather than as control flow.
//
// Nothing here opens a port, blocks, or knows what a thread is. The caller asks
// for the next step, sends it however it sends things, and hands back what the
// player said; the examiner decides what that meant and what to do next. That
// is what makes every branch of it — a refused disc-status query, an open tray,
// a link that dies halfway through, a cancel between any two steps — a test
// that runs in microseconds with nothing plugged in.
//
// The old application's equivalent was a run of blocking calls inside a thread
// body, and its failure branches could only be reached by arranging for real
// hardware to misbehave. In practice that meant they were never reached at all
// until a user found one.
//
// **Failure is partial.** A step that is refused, unanswered or unsupported
// leaves its field unknown and the sequence goes on to the next one. Only three
// things stop it early: an open tray, a disc that will not spin, and a link
// failure — and the first two are findings rather than faults.
//
// Thread-safety: none, deliberately. One caller drives one examiner.
class DiscExaminer {
 public:
  // The definition and the firmware are what the step plan is built from: a
  // model with no chapter search is not asked about chapters, and the plan is
  // shorter by exactly that step rather than carrying one that will be refused.
  DiscExaminer(const PlayerDefinition& definition, std::string_view firmware);

  // The step to send now, or nothing when the examination is over.
  //
  // Stable until Apply(): asking twice yields the same step, so a caller that
  // has to re-read it after a retry or a redraw is not moved on by looking.
  std::optional<ExamineStep> Next();

  // What the player said to the step Next() handed out. Ignored when the
  // examination has already finished, which is what makes a reply arriving
  // after a cancel harmless.
  void Apply(const Reply& reply);

  // Stop, keeping what has been found so far.
  //
  // The profile is left intact deliberately: a user who cancels after the disc
  // type and the length have been established has still learnt both, and
  // throwing them away would make cancelling cost more than waiting.
  void Cancel();

  // Begin again from the start, with an empty profile. What the dialog does
  // when the user examines a second disc.
  void Restart();

  bool finished() const { return outcome_ != ExamineOutcome::kInProgress; }
  ExamineOutcome outcome() const { return outcome_; }
  const DiscProfile& profile() const { return profile_; }

  // What the current step is for, for the progress line.
  ExamineStage stage() const;

  // How far along. Steps that turn out not to apply — the spin-up on a player
  // that is already playing — count as done, so the count only ever moves
  // forwards.
  size_t steps_completed() const { return completed_; }
  size_t steps_planned() const { return plan_.size(); }

 private:
  // Work out which steps this model can be asked at all. Called once.
  void BuildPlan();

  // Does this step still apply, given what has been learnt?
  bool Applies(ExamineStage stage) const;

  // Move past the steps that no longer apply, and notice when the plan has run
  // out. Called after every reply, so that the last one finishes the
  // examination rather than leaving it finished-but-not-saying-so until
  // somebody asks for another step.
  void Advance();

  // Fill in the step for a stage — which for the two seeks depends on how the
  // disc is addressed, and so cannot be decided when the plan is built.
  ExamineStep StepFor(ExamineStage stage) const;

  void ApplyCheckingPlayer(const Reply& reply);
  void ApplySpinningUp(const Reply& reply);
  void ApplyDiscStatus(const Reply& reply);
  void ApplyTvSystem(const Reply& reply);
  void ApplyUserCode(const Reply& reply, UserCodeReading& into);
  void ApplyChapters(const Reply& reply);
  void ApplyEndAddress(const Reply& reply);
  void ApplyStartAddress(const Reply& reply);

  // How this disc is addressed, defaulting to frames while that is not known —
  // which is what the address parser is strictest about and so the safest thing
  // to be wrong about.
  AddressMode addressing() const;

  const PlayerDefinition* definition_ = nullptr;
  PlayerControls controls_;

  std::vector<ExamineStage> plan_;
  size_t index_ = 0;
  size_t completed_ = 0;

  // The step handed out and not yet answered.
  std::optional<ExamineStep> pending_;

  ExamineOutcome outcome_ = ExamineOutcome::kInProgress;
  DiscProfile profile_;

  // Set when the player is already spinning, so the spin-up step is skipped.
  bool spinning_ = false;

  // The seek past the end went out. Its answer is ignored — a refusal is the
  // expected one — so this records only that the player was asked to go there.
  bool end_seek_sent_ = false;

  // The seek back to the start was accepted. Unlike the one above, a refusal
  // here means the player did not move, and the address afterwards would be
  // somebody else's.
  bool start_seek_ok_ = false;
};

// The address a disc cannot have, per addressing scheme.
//
// The old application's numbers unchanged — "FR60000SE" for a frame seek and
// "FR1595900SE" for a time-code one — because they are what years of captures
// measured disc lengths with. Both are past the end of any disc, so the player
// runs to the end of the side and refuses, and where it stopped is the answer.
inline constexpr int32_t kImpossibleFrame = 60000;
inline constexpr int32_t kImpossibleTimeCode = 1595900;

// The address every disc has: the first one.
inline constexpr int32_t kFirstAddress = 1;

}  // namespace ddd::player
