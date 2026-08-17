/************************************************************************

    test_auto_capture_sequence.cpp

    T1 tests for the automatic capture, with no player and no Duplicator
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "auto_capture_sequence.h"
#include "player_registry.h"

namespace ddd::player {
namespace {

const PlayerDefinition& LevelIIIModel() {
  const PlayerDefinition* const found = FindPlayerByIdCode("15");
  EXPECT_NE(found, nullptr);
  return found != nullptr ? *found : GenericPlayer();
}

Reply Answered(std::string text) {
  Reply reply;
  reply.status = ReplyStatus::kOk;
  reply.text = std::move(text);
  return reply;
}

Reply Refused(std::string code = "E04") {
  Reply reply;
  reply.status = ReplyStatus::kRefused;
  reply.text = code;
  reply.error_code = std::move(code);
  return reply;
}

Reply LinkFailed() {
  Reply reply;
  reply.status = ReplyStatus::kLinkFailed;
  return reply;
}

// An NTSC CAV side of 54,000 frames, examined in full.
DiscProfile CavDisc() {
  DiscProfile disc;
  disc.disc_present.Record(true, Provenance::kReported);
  disc.disc_type.Record(DiscType::kCav, Provenance::kReported);
  disc.addressing.Record(AddressMode::kFrame, Provenance::kInferred);
  disc.disc_side.Record(1, Provenance::kReported);
  disc.disc_size.Record(DiscSize::k30cm, Provenance::kReported);
  disc.programme_start.Record(1, Provenance::kMeasured);
  disc.programme_end.Record(54000, Provenance::kMeasured);
  disc.lead_in_reachable.Record(true, Provenance::kMeasured);
  disc.video_standard.Record(VideoStandard::kNtsc, Provenance::kReported);
  return disc;
}

DiscProfile ClvDisc() {
  DiscProfile disc = CavDisc();
  disc.disc_type.Record(DiscType::kClv, Provenance::kReported);
  disc.addressing.Record(AddressMode::kTimeCode, Provenance::kInferred);
  disc.programme_start.Record(0, Provenance::kMeasured);
  disc.programme_end.Record(504500, Provenance::kMeasured);
  return disc;
}

// What the player and the capture engine answer, stated per stage.
//
// The addresses are a list rather than one reply because the whole of the watch
// is about how one reading relates to the last: a run of identical readings is
// a stall, and a run of climbing ones is a capture in progress.
struct Script {
  std::map<AutoCaptureStage, Reply> answers;
  std::vector<std::string> addresses{"0000100", "0054000"};

  bool start_capture_ok = true;
  bool stop_capture_ok = true;

  // Injected at the nth step of the whole run, counting from zero. -1 for
  // never.
  int link_fails_at = -1;
  int cancel_at = -1;

  size_t address_index = 0;

  // What the engine actually did, as distinct from what it was asked to do. The
  // property below is about files left open, so it counts writers rather than
  // steps: a start that failed opened nothing to leave.
  size_t writers_opened = 0;
  size_t writers_closed = 0;

  StepResult Answer(const AutoCaptureStep& step) {
    switch (step.action) {
      case AutoCaptureAction::kStartCapture:
        writers_opened += start_capture_ok ? 1 : 0;
        return CaptureDone(start_capture_ok);
      case AutoCaptureAction::kStopCapture:
        ++writers_closed;
        return CaptureDone(stop_capture_ok);
      case AutoCaptureAction::kSendCommand:
        break;
    }

    if (step.stage == AutoCaptureStage::kWatching) {
      // The last reading repeats once the list runs out, which is how a stall
      // is written: give one address and nothing after it.
      const size_t index = std::min(address_index, addresses.size() - 1);
      ++address_index;
      return PlayerReplied(Answered(addresses[index]));
    }

    const auto found = answers.find(step.stage);
    return PlayerReplied(found == answers.end() ? Answered("R")
                                                : found->second);
  }
};

std::vector<AutoCaptureStep> Drive(AutoCaptureSequence& sequence,
                                   Script& script) {
  std::vector<AutoCaptureStep> steps;

  while (const std::optional<AutoCaptureStep> step = sequence.Next()) {
    steps.push_back(*step);
    const int index = static_cast<int>(steps.size()) - 1;

    // Both arrive while the step is in flight, which is the case that matters:
    // a cancel between two steps is the easy one.
    if (index == script.cancel_at) {
      sequence.Cancel();
    }

    // Only ever injected on a step that talks to the player: attaching a writer
    // does not touch the serial link, so a link failure there would be a fault
    // this test invented rather than one the sequence can meet.
    if (index == script.link_fails_at &&
        step->action == AutoCaptureAction::kSendCommand) {
      sequence.Apply(PlayerReplied(LinkFailed()));
    } else {
      sequence.Apply(script.Answer(*step));
    }

    // A step machine that handed out the same step forever would hang the suite
    // rather than fail a test, so the loop is bounded here too.
    if (steps.size() >= 256) {
      ADD_FAILURE() << "the automatic capture did not terminate";
      break;
    }
  }

  return steps;
}

std::vector<AutoCaptureStage> Stages(
    const std::vector<AutoCaptureStep>& steps) {
  std::vector<AutoCaptureStage> stages;
  stages.reserve(steps.size());
  for (const AutoCaptureStep& step : steps) {
    stages.push_back(step.stage);
  }
  return stages;
}

// The stages with the repeated ones collapsed, which is what makes an ordering
// assertion readable: the watch is one thing that happened however many times
// it was polled.
std::vector<AutoCaptureStage> Shape(const std::vector<AutoCaptureStep>& steps) {
  std::vector<AutoCaptureStage> shape;
  for (const AutoCaptureStage stage : Stages(steps)) {
    if (shape.empty() || shape.back() != stage) {
      shape.push_back(stage);
    }
  }
  return shape;
}

bool Contains(const std::vector<AutoCaptureStep>& steps,
              AutoCaptureStage stage) {
  return std::any_of(
      steps.begin(), steps.end(),
      [stage](const AutoCaptureStep& step) { return step.stage == stage; });
}

size_t CountAction(const std::vector<AutoCaptureStep>& steps,
                   AutoCaptureAction action) {
  return static_cast<size_t>(std::count_if(
      steps.begin(), steps.end(),
      [action](const AutoCaptureStep& step) { return step.action == action; }));
}

AutoCapturePlan WholeSide(const DiscProfile& disc) {
  return DefaultPlanFor(disc);
}

AutoCapturePlan Range(const DiscProfile& disc, int32_t start, int32_t end) {
  AutoCapturePlan plan = DefaultPlanFor(disc);
  plan.shape = CaptureShape::kRange;
  plan.start_address = start;
  plan.end_address = end;
  return plan;
}

// --- The three shapes ------------------------------------------------------

TEST(AutoCaptureSequence, AWholeCavSideSpinsDownBeforeItStartsWriting) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");
  script.addresses = {"<0000001", "0001000", "0054000"};

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCompleted);
  EXPECT_EQ(Shape(steps), (std::vector<AutoCaptureStage>{
                              AutoCaptureStage::kConfirmingDisc,
                              AutoCaptureStage::kSpinningDown,
                              AutoCaptureStage::kStartingCapture,
                              AutoCaptureStage::kSpinningUp,
                              AutoCaptureStage::kWatching,

                              // The player first, and the writer after it: the
                              // run-out is not an address, so it reaches the
                              // file only by being recorded while the disc is
                              // being stopped.
                              AutoCaptureStage::kStoppingPlayer,
                              AutoCaptureStage::kStoppingCapture,
                          }));

  // The ordering that makes lead-in capture work: the writer is attached while
  // the disc is stopped, and the disc is started afterwards.
  const auto stages = Stages(steps);
  const auto spin_down =
      std::find(stages.begin(), stages.end(), AutoCaptureStage::kSpinningDown);
  const auto start = std::find(stages.begin(), stages.end(),
                               AutoCaptureStage::kStartingCapture);
  const auto spin_up =
      std::find(stages.begin(), stages.end(), AutoCaptureStage::kSpinningUp);
  EXPECT_LT(spin_down, start);
  EXPECT_LT(start, spin_up);
}

TEST(AutoCaptureSequence, ACavSideIsPlayedWithItsStopCodesIgnored) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  const auto spin_up =
      std::find_if(steps.begin(), steps.end(), [](const AutoCaptureStep& step) {
        return step.stage == AutoCaptureStage::kSpinningUp;
      });
  ASSERT_NE(spin_up, steps.end());

  // A stop code partway through a CAV side would otherwise pause the player and
  // end a whole-side capture with nothing to say it had happened.
  EXPECT_EQ(spin_up->command, PlayerCommand::kPlayWithoutStopCodes);
}

TEST(AutoCaptureSequence, AClvSideIsSimplyPlayed) {
  const DiscProfile disc = ClvDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("11001");
  script.addresses = {"<0000000", "0504500"};

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCompleted);

  const auto spin_up =
      std::find_if(steps.begin(), steps.end(), [](const AutoCaptureStep& step) {
        return step.stage == AutoCaptureStage::kSpinningUp;
      });
  ASSERT_NE(spin_up, steps.end());

  // CLV discs carry no stop codes, so there is nothing to disable.
  EXPECT_EQ(spin_up->command, PlayerCommand::kPlay);

  const auto watch =
      std::find_if(steps.begin(), steps.end(), [](const AutoCaptureStep& step) {
        return step.stage == AutoCaptureStage::kWatching;
      });
  ASSERT_NE(watch, steps.end());
  EXPECT_EQ(watch->command, PlayerCommand::kQueryAddress);
}

TEST(AutoCaptureSequence, ARangeSeeksToItsStartAndNeverSpinsDown) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", Range(disc, 1000, 2000),
                               disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");
  script.addresses = {"0001000", "0001500", "0002000"};

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCompleted);
  EXPECT_EQ(Shape(steps), (std::vector<AutoCaptureStage>{
                              AutoCaptureStage::kConfirmingDisc,
                              AutoCaptureStage::kSeekingStart,
                              AutoCaptureStage::kStartingCapture,
                              AutoCaptureStage::kSpinningUp,
                              AutoCaptureStage::kWatching,
                              AutoCaptureStage::kStoppingCapture,
                              AutoCaptureStage::kStoppingPlayer,
                          }));

  const auto seek =
      std::find_if(steps.begin(), steps.end(), [](const AutoCaptureStep& step) {
        return step.stage == AutoCaptureStage::kSeekingStart;
      });
  ASSERT_NE(seek, steps.end());
  EXPECT_EQ(seek->command, PlayerCommand::kSeekFrame);
  EXPECT_EQ(seek->argument, std::optional<int32_t>{1000});
}

TEST(AutoCaptureSequence, AClvRangeSeeksByTimeCode) {
  const DiscProfile disc = ClvDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02",
                               Range(disc, 100000, 203000), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("11001");
  script.addresses = {"0100000", "0203000"};

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCompleted);

  const auto seek =
      std::find_if(steps.begin(), steps.end(), [](const AutoCaptureStep& step) {
        return step.stage == AutoCaptureStage::kSeekingStart;
      });
  ASSERT_NE(seek, steps.end());
  EXPECT_EQ(seek->command, PlayerCommand::kSeekTimeCode);
  EXPECT_EQ(seek->argument, std::optional<int32_t>{100000});
}

TEST(AutoCaptureSequence, ASpinUpCaptureStartsLikeAWholeSideAndEndsLikeARange) {
  const DiscProfile disc = CavDisc();
  AutoCapturePlan plan = WholeSide(disc);
  plan.shape = CaptureShape::kFromSpinUp;
  plan.end_address = 1000;

  AutoCaptureSequence sequence(LevelIIIModel(), "02", plan, disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");
  script.addresses = {"<0000001", "0000500", "0001000"};

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCompleted);

  // The spin-down at the front is what both spin-up shapes are for: there is no
  // command that puts a player on the lead-in, so the only way it reaches the
  // file is for the capture to be running while the disc comes up from a stop.
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kSpinningDown));
  EXPECT_FALSE(Contains(steps, AutoCaptureStage::kSeekingStart));

  // But not at the back. It stops partway through the side, where there is no
  // run-out to record and nothing to be gained from writing through the stop.
  const std::vector<AutoCaptureStage> shape = Shape(steps);
  const auto stop_capture =
      std::find(shape.begin(), shape.end(), AutoCaptureStage::kStoppingCapture);
  const auto stop_player =
      std::find(shape.begin(), shape.end(), AutoCaptureStage::kStoppingPlayer);
  EXPECT_LT(stop_capture, stop_player);

  // And it stopped where it was told to, well short of the end of the side.
  EXPECT_EQ(sequence.last_address(), std::optional<int32_t>{1000});
}

TEST(AutoCaptureSequence, AWholeSideKeepsWritingThroughTheSpinDown) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");
  script.addresses = {"0054000"};

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);
  const std::vector<AutoCaptureStage> shape = Shape(steps);

  // The whole reason the tail's order depends on the shape. A capture that
  // detached its writer and then stopped the player would end a few seconds
  // short of exactly the part of the disc nothing else can reach — the run-out
  // is not an address, so nothing can seek to it and no later capture can go
  // back for it.
  const auto stop_player =
      std::find(shape.begin(), shape.end(), AutoCaptureStage::kStoppingPlayer);
  const auto stop_capture =
      std::find(shape.begin(), shape.end(), AutoCaptureStage::kStoppingCapture);
  ASSERT_NE(stop_player, shape.end());
  ASSERT_NE(stop_capture, shape.end());
  EXPECT_LT(stop_player, stop_capture);

  // And the writer really was still attached when the stop went out.
  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCompleted);
  EXPECT_FALSE(sequence.capture_left_running());
}

TEST(AutoCaptureSequence, ARangeStopsTheWriterBeforeThePlayer) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", Range(disc, 1000, 2000),
                               disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");
  script.addresses = {"0002000"};

  const std::vector<AutoCaptureStage> shape = Shape(Drive(sequence, script));

  // Nothing at the end of a range is worth the extra seconds of a spin-down.
  const auto stop_capture =
      std::find(shape.begin(), shape.end(), AutoCaptureStage::kStoppingCapture);
  const auto stop_player =
      std::find(shape.begin(), shape.end(), AutoCaptureStage::kStoppingPlayer);
  EXPECT_LT(stop_capture, stop_player);
}

// --- The key lock ----------------------------------------------------------

TEST(AutoCaptureSequence, TheKeyLockIsTakenFirstAndReleasedLast) {
  const DiscProfile disc = CavDisc();
  AutoCapturePlan plan = WholeSide(disc);
  plan.key_lock = true;

  AutoCaptureSequence sequence(LevelIIIModel(), "02", plan, disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);
  const std::vector<AutoCaptureStage> shape = Shape(steps);

  ASSERT_FALSE(shape.empty());
  EXPECT_EQ(shape.front(), AutoCaptureStage::kLockingFrontPanel);
  EXPECT_EQ(shape.back(), AutoCaptureStage::kUnlockingFrontPanel);
}

TEST(AutoCaptureSequence, ALockThatWasRefusedIsNotReleased) {
  const DiscProfile disc = CavDisc();
  AutoCapturePlan plan = WholeSide(disc);
  plan.key_lock = true;

  AutoCaptureSequence sequence(LevelIIIModel(), "02", plan, disc);

  Script script;
  script.answers[AutoCaptureStage::kLockingFrontPanel] = Refused();
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  // The capture goes ahead — a live front panel is a smaller thing than the
  // capture somebody asked for — and nothing pretends to undo a lock that was
  // never taken.
  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCompleted);
  EXPECT_FALSE(Contains(steps, AutoCaptureStage::kUnlockingFrontPanel));
}

TEST(AutoCaptureSequence, NoLockIsAskedForWhenThePlanDidNotWantOne) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_FALSE(Contains(steps, AutoCaptureStage::kLockingFrontPanel));
  EXPECT_FALSE(Contains(steps, AutoCaptureStage::kUnlockingFrontPanel));
}

// --- Ending the watch ------------------------------------------------------

TEST(AutoCaptureSequence, RunningIntoTheLeadOutFinishesTheCapture) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");

  // The measured end is a frame or two short of where this player thinks the
  // lead-out starts, which is the ordinary case rather than a fault.
  script.addresses = {"0053000", ">0054001"};

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCompleted);
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kStoppingCapture));
}

TEST(AutoCaptureSequence, TimeSpentInTheLeadInIsNotAStall) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");

  // Rather more lead-in readings than the stall threshold. The number in a
  // lead-in reply is not a programme address, so it is not compared with
  // anything — and a whole-side capture spends its first seconds here.
  script.addresses = {"<0000001", "<0000001", "<0000001",
                      "<0000001", "<0000001", "<0000001",
                      "<0000001", "0000001",  "0054000"};

  Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCompleted);
}

TEST(AutoCaptureSequence,
     AnAddressThatStopsAdvancingIsQueriedRatherThanAssumed) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");

  // The address never moves, and the player insists it is playing.
  script.addresses = {"0001000"};
  script.answers[AutoCaptureStage::kCheckingStall] = Answered("P04");

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kStalled);
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kCheckingStall));

  // Ended rather than left running, which is the whole point: the alternative
  // is a file that grows until the volume fills.
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kStoppingCapture));
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kStoppingPlayer));
  EXPECT_FALSE(sequence.capture_left_running());
}

TEST(AutoCaptureSequence, APlayerStillGettingUpToSpeedIsNotAStall) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");

  // Six identical readings, then the disc gets going.
  script.addresses = {"0000001", "0000001", "0000001", "0000001",
                      "0000001", "0000001", "0002000", "0054000"};
  script.answers[AutoCaptureStage::kCheckingStall] = Answered("P02");

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kCheckingStall));
  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCompleted);
}

TEST(AutoCaptureSequence, APlayerThatStoppedIsReportedAsHavingStopped) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");
  script.addresses = {"0001000"};

  // Parked. Somebody pressed a button, or the disc has a defect the player
  // cannot read past — either way the capture up to here is good.
  script.answers[AutoCaptureStage::kCheckingStall] = Answered("P01");

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kPlayerStopped);
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kStoppingCapture));
}

TEST(AutoCaptureSequence,
     AnUnreadableWatchEndsTheRunRatherThanWatchingForever) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");
  script.addresses = {"nonsense"};
  script.answers[AutoCaptureStage::kCheckingStall] = Answered("nonsense");

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kStalled);
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kStoppingCapture));
}

// --- The failure branches --------------------------------------------------

TEST(AutoCaptureSequence, ASwappedDiscIsNoticedBeforeAnythingIsWritten) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  // A CLV disc, where a CAV one was examined. Every address in the plan means
  // something else on this disc.
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("11001");

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kDiscChanged);
  EXPECT_FALSE(Contains(steps, AutoCaptureStage::kStartingCapture));
  EXPECT_FALSE(Contains(steps, AutoCaptureStage::kSpinningUp));
}

TEST(AutoCaptureSequence, AFlippedDiscIsNoticedFromItsSide) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  // Same type, same size, other side — and the other side is a different
  // length. The commonest way for the disc to change between an examination and
  // a capture.
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10011");

  Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kDiscChanged);
}

TEST(AutoCaptureSequence, ADiscTakenOutIsNoticed) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("0XXXX");

  Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kDiscChanged);
}

TEST(AutoCaptureSequence, APlayerThatWillNotAnswerTheCheckIsCapturedAnyway) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Refused();

  Drive(sequence, script);

  // Partial failure is this library's rule. Refusing here would refuse discs on
  // players that seek and play perfectly well.
  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCompleted);
}

TEST(AutoCaptureSequence, ARefusedSpinDownDoesNotStopTheCapture) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");

  // Several models answer an already-stopped stop with an error, and the aim of
  // the step — a disc that is not turning — has been met either way.
  script.answers[AutoCaptureStage::kSpinningDown] = Refused();

  Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCompleted);
}

TEST(AutoCaptureSequence, ARefusedSeekStopsBeforeAnythingIsWritten) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", Range(disc, 1000, 2000),
                               disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");
  script.answers[AutoCaptureStage::kSeekingStart] = Refused();

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kPlayerRefused);
  EXPECT_FALSE(Contains(steps, AutoCaptureStage::kStartingCapture));

  // The player is put back all the same: a seek that was sent may have spun the
  // disc up before it was refused.
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kStoppingPlayer));
}

TEST(AutoCaptureSequence,
     ARefusedSpinUpFinalisesTheCaptureItHadAlreadyStarted) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");
  script.answers[AutoCaptureStage::kSpinningUp] = Refused();

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kPlayerRefused);

  // The branch the tail exists for: the writer was attached before the player
  // was started, so a player that never started leaves a file to close.
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kStartingCapture));
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kStoppingCapture));
  EXPECT_FALSE(sequence.capture_left_running());
}

TEST(AutoCaptureSequence, ACaptureThatWillNotStartLeavesThePlayerAlone) {
  const DiscProfile disc = CavDisc();
  AutoCapturePlan plan = WholeSide(disc);
  plan.key_lock = true;

  AutoCaptureSequence sequence(LevelIIIModel(), "02", plan, disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");
  script.start_capture_ok = false;

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCaptureFailed);
  EXPECT_FALSE(Contains(steps, AutoCaptureStage::kSpinningUp));
  EXPECT_FALSE(Contains(steps, AutoCaptureStage::kStoppingCapture));

  // The front panel is given back, though — the lock was this sequence's doing.
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kUnlockingFrontPanel));
}

TEST(AutoCaptureSequence, ACaptureThatWillNotFinaliseIsNotReportedAsCompleted) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");
  script.stop_capture_ok = false;

  Drive(sequence, script);

  // The disc played to the end, so the run "worked" — and the file did not
  // close, so calling it completed would be the one report nobody goes back and
  // checks.
  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCaptureFailed);
}

TEST(AutoCaptureSequence, ALinkThatDiesMidCaptureLeavesTheCaptureRunning) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");
  script.addresses = {"0001000", "0002000", "0003000"};

  // The fifth step: confirm, spin down, start capture, spin up, then the first
  // watch — which is where the cable comes out.
  script.link_fails_at = 4;

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kLinkFailed);

  // The plan's rule, and the one branch where a started capture is not stopped:
  // a player fault never destroys a capture. The player runs to the end of the
  // side by itself, and the caller is told the automation has ended.
  EXPECT_FALSE(Contains(steps, AutoCaptureStage::kStoppingCapture));
  EXPECT_TRUE(sequence.capture_left_running());

  // And nothing is sent into a dead port to wait out a timeout apiece.
  EXPECT_FALSE(Contains(steps, AutoCaptureStage::kStoppingPlayer));
}

TEST(AutoCaptureSequence,
     ALinkThatDiesBeforeTheCaptureStartsLeavesNothingOpen) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.link_fails_at = 0;  // the disc-status check

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kLinkFailed);
  EXPECT_FALSE(sequence.capture_left_running());
  EXPECT_EQ(CountAction(steps, AutoCaptureAction::kStartCapture), 0u);
}

// --- Cancelling ------------------------------------------------------------

TEST(AutoCaptureSequence, CancellingMidWatchFinishesTheCaptureProperly) {
  const DiscProfile disc = CavDisc();
  AutoCapturePlan plan = WholeSide(disc);
  plan.key_lock = true;

  AutoCaptureSequence sequence(LevelIIIModel(), "02", plan, disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");
  script.addresses = {"0001000", "0002000", "0003000", "0004000"};
  script.cancel_at =
      6;  // lock, confirm, spin down, start, spin up, watch, watch

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCancelled);

  // The difference from the old application, which abandoned the run where it
  // stood: the writer is detached and the file finalised, the player is stopped
  // and the front panel released.
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kStoppingCapture));
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kStoppingPlayer));
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kUnlockingFrontPanel));
  EXPECT_FALSE(sequence.capture_left_running());
}

TEST(AutoCaptureSequence, CancellingBeforeAnythingStartsSendsNothing) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  sequence.Cancel();

  EXPECT_TRUE(sequence.finished());
  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCancelled);
  EXPECT_FALSE(sequence.Next().has_value());
}

TEST(AutoCaptureSequence, ACancelDuringTheTailIsIgnored) {
  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(LevelIIIModel(), "02", WholeSide(disc), disc);

  Script script;
  script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");
  script.addresses = {"0054000"};

  // Confirm, spin down, start, spin up, watch, then the stop-capture step —
  // where the user presses the button a second time.
  script.cancel_at = 5;

  const std::vector<AutoCaptureStep> steps = Drive(sequence, script);

  // The run had already ended of its own accord, and a second press does not
  // rewrite why.
  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kCompleted);
  EXPECT_TRUE(Contains(steps, AutoCaptureStage::kStoppingPlayer));
}

// --- Refusing to start at all ----------------------------------------------

TEST(AutoCaptureSequence, AnInvalidPlanSendsNothingAtAll) {
  const DiscProfile disc = CavDisc();

  AutoCapturePlan plan = WholeSide(disc);
  plan.end_address = 99999;  // past the measured end of the side

  AutoCaptureSequence sequence(LevelIIIModel(), "02", plan, disc);

  EXPECT_TRUE(sequence.finished());
  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kInvalidPlan);
  EXPECT_FALSE(sequence.Next().has_value());
}

TEST(AutoCaptureSequence, APlayerWithNoTransportIsRefusedUpFront) {
  PlayerDefinition definition = GenericPlayer();
  definition.commands[Index(PlayerCommand::kPlay)] = CommandSpec{};
  definition.commands[Index(PlayerCommand::kPlayWithoutStopCodes)] =
      CommandSpec{};

  const DiscProfile disc = CavDisc();
  AutoCaptureSequence sequence(definition, "02", WholeSide(disc), disc);

  // Said now rather than by a capture that starts and immediately stops.
  EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kUnsupportedPlayer);
  EXPECT_FALSE(sequence.Next().has_value());
}

// --- The property that matters most ----------------------------------------

TEST(AutoCaptureSequence, NoBranchLeavesACaptureRunningWithoutSayingSo) {
  const DiscProfile cav = CavDisc();
  const DiscProfile clv = ClvDisc();

  struct Case {
    const char* name;
    DiscProfile disc;
    AutoCapturePlan plan;
    Script script;
  };

  Script base;
  base.answers[AutoCaptureStage::kConfirmingDisc] = Answered("10001");

  std::vector<Case> cases;

  for (const CaptureShape shape :
       {CaptureShape::kWholeSide, CaptureShape::kRange,
        CaptureShape::kFromSpinUp}) {
    for (const bool key_lock : {false, true}) {
      AutoCapturePlan plan = Range(cav, 1000, 2000);
      plan.shape = shape;
      plan.key_lock = key_lock;

      // Every way a run can go wrong, against every shape it can take.
      Script stalled = base;
      stalled.addresses = {"0001500"};
      stalled.answers[AutoCaptureStage::kCheckingStall] = Answered("P04");

      Script stopped = base;
      stopped.addresses = {"0001500"};
      stopped.answers[AutoCaptureStage::kCheckingStall] = Answered("P01");

      Script swapped = base;
      swapped.answers[AutoCaptureStage::kConfirmingDisc] = Answered("11001");

      Script refused_seek = base;
      refused_seek.answers[AutoCaptureStage::kSeekingStart] = Refused();

      Script refused_play = base;
      refused_play.answers[AutoCaptureStage::kSpinningUp] = Refused();

      Script no_capture = base;
      no_capture.start_capture_ok = false;

      Script bad_finalise = base;
      bad_finalise.stop_capture_ok = false;

      Script cancelled = base;
      cancelled.addresses = {"0001100", "0001200", "0001300", "0001400"};
      cancelled.cancel_at = 4;

      cases.push_back({"completed", cav, plan, base});
      cases.push_back({"stalled", cav, plan, stalled});
      cases.push_back({"stopped", cav, plan, stopped});
      cases.push_back({"swapped", cav, plan, swapped});
      cases.push_back({"refused seek", cav, plan, refused_seek});
      cases.push_back({"refused play", cav, plan, refused_play});
      cases.push_back({"capture would not start", cav, plan, no_capture});
      cases.push_back({"capture would not finalise", cav, plan, bad_finalise});
      cases.push_back({"cancelled", cav, plan, cancelled});

      for (int at = 0; at < 8; ++at) {
        Script dead = base;
        dead.link_fails_at = at;
        cases.push_back({"link failed", cav, plan, dead});
      }
    }
  }

  // And the same sweep on a CLV disc, whose plan is written in time codes.
  {
    AutoCapturePlan plan = Range(clv, 100000, 203000);
    Script script = base;
    script.answers[AutoCaptureStage::kConfirmingDisc] = Answered("11001");
    script.addresses = {"0100000", "0203000"};
    cases.push_back({"clv", clv, plan, script});
  }

  for (Case& item : cases) {
    AutoCaptureSequence sequence(LevelIIIModel(), "02", item.plan, item.disc);
    const std::vector<AutoCaptureStep> steps = Drive(sequence, item.script);

    SCOPED_TRACE(item.name);

    EXPECT_TRUE(sequence.finished());

    // Never more than one writer, whatever happened.
    EXPECT_LE(item.script.writers_opened, 1u);
    EXPECT_LE(item.script.writers_closed, 1u);
    EXPECT_LE(CountAction(steps, AutoCaptureAction::kStartCapture), 1u);

    if (item.script.writers_opened == 0) {
      EXPECT_EQ(item.script.writers_closed, 0u);
      EXPECT_FALSE(sequence.capture_left_running());
      continue;
    }

    // A writer that was attached is either detached, or the sequence says
    // plainly that it was left running — which only the link-failure branch
    // does, and which the caller reports rather than a file quietly outliving
    // the thing that started it.
    if (item.script.writers_closed == 1) {
      EXPECT_FALSE(sequence.capture_left_running());

      const auto stages = Stages(steps);
      const auto start_at = std::find(stages.begin(), stages.end(),
                                      AutoCaptureStage::kStartingCapture);
      const auto stop_at = std::find(stages.begin(), stages.end(),
                                     AutoCaptureStage::kStoppingCapture);
      EXPECT_LT(start_at, stop_at);

      // And the spin-down is inside the capture on exactly the shape that wants
      // it there, whatever ended the run.
      const auto stop_player = std::find(stages.begin(), stages.end(),
                                         AutoCaptureStage::kStoppingPlayer);
      if (stop_player != stages.end()) {
        if (EndsWithSpinDown(item.plan.shape)) {
          EXPECT_LT(stop_player, stop_at);
        } else {
          EXPECT_LT(stop_at, stop_player);
        }
      }
    } else {
      EXPECT_TRUE(sequence.capture_left_running());
      EXPECT_EQ(sequence.outcome(), AutoCaptureOutcome::kLinkFailed);
    }
  }
}

}  // namespace
}  // namespace ddd::player
