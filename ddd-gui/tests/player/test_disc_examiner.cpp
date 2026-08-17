/************************************************************************

    test_disc_examiner.cpp

    T1 tests for the examine sequence, with no player attached
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

#include "disc_examiner.h"
#include "player_registry.h"
#include "players/pioneer_level_iii.h"

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

Reply Silent() {
  Reply reply;
  reply.status = ReplyStatus::kNoAnswer;
  return reply;
}

Reply LinkFailed() {
  Reply reply;
  reply.status = ReplyStatus::kLinkFailed;
  return reply;
}

// What a disc answers, stated per step rather than per command — the address
// query is asked twice and the two answers are the whole point of asking.
using Script = std::map<ExamineStage, Reply>;

// A CAV disc of 54,000 frames, in a player that is parked.
Script CavScript() {
  return {
      {ExamineStage::kCheckingPlayer, Answered("P01")},
      {ExamineStage::kSpinningUp, Answered("R")},
      {ExamineStage::kReadingDiscStatus, Answered("10001")},
      {ExamineStage::kReadingTvSystem, Answered("110")},
      {ExamineStage::kReadingPioneerUserCode, Answered("#59-014 CASPER")},
      {ExamineStage::kReadingStandardUserCode, Answered("Y1000")},
      {ExamineStage::kFindingEnd, Refused()},
      {ExamineStage::kReadingEnd, Answered("054000")},
      {ExamineStage::kFindingStart, Answered("R")},
      {ExamineStage::kReadingStart, Answered("<00001")},
      {ExamineStage::kSettling, Answered("R")},
      {ExamineStage::kSpinningDown, Answered("R")},
  };
}

// A CLV disc running to 0:50:45, in a player that is already playing.
Script ClvScript() {
  return {
      {ExamineStage::kCheckingPlayer, Answered("P04")},
      {ExamineStage::kReadingDiscStatus, Answered("11010")},
      {ExamineStage::kReadingTvSystem, Answered("220")},
      {ExamineStage::kReadingPioneerUserCode, Answered("E04")},
      {ExamineStage::kReadingStandardUserCode, Answered("Y0000")},
      {ExamineStage::kFindingEnd, Refused()},
      {ExamineStage::kReadingEnd, Answered("0504500")},
      {ExamineStage::kFindingStart, Answered("R")},
      {ExamineStage::kReadingStart, Answered("<0000000")},
      {ExamineStage::kSettling, Answered("R")},
  };
}

// Drive an examination to its end, returning what was sent.
std::vector<ExamineStep> Drive(DiscExaminer& examiner, const Script& script) {
  std::vector<ExamineStep> sent;

  while (const std::optional<ExamineStep> step = examiner.Next()) {
    sent.push_back(*step);
    const auto found = script.find(step->stage);

    // The "is the disc turning?" step defaults to a playing player. It is the
    // ordinary case, and a test about a disc that has stopped underneath the
    // examination says so by scripting the stage itself.
    const Reply reply = found != script.end() ? found->second
                        : step->stage == ExamineStage::kCheckingTransport
                            ? Answered("P04")
                            : Answered("R");
    examiner.Apply(reply);

    // A step machine that handed out the same step forever would hang the
    // suite rather than fail a test, so the loop is bounded here too.
    if (sent.size() >= 64) {
      ADD_FAILURE() << "the examination did not terminate";
      break;
    }
  }

  return sent;
}

std::vector<ExamineStage> Stages(const std::vector<ExamineStep>& steps) {
  std::vector<ExamineStage> stages;
  stages.reserve(steps.size());
  for (const ExamineStep& step : steps) {
    stages.push_back(step.stage);
  }
  return stages;
}

bool Sent(const std::vector<ExamineStep>& steps, ExamineStage stage) {
  return std::any_of(steps.begin(), steps.end(), [stage](const ExamineStep& s) {
    return s.stage == stage;
  });
}

const ExamineStep* Find(const std::vector<ExamineStep>& steps,
                        ExamineStage stage) {
  const auto found =
      std::find_if(steps.begin(), steps.end(),
                   [stage](const ExamineStep& s) { return s.stage == stage; });
  return found == steps.end() ? nullptr : &*found;
}

// --- The sequence itself ---------------------------------------------------

TEST(DiscExaminerTest, AParkedCavDiscIsExaminedInDependencyOrder) {
  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, CavScript());

  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted);
  EXPECT_EQ(
      Stages(sent),
      (std::vector<ExamineStage>{
          ExamineStage::kCheckingPlayer, ExamineStage::kSpinningUp,
          ExamineStage::kReadingDiscStatus, ExamineStage::kReadingTvSystem,
          ExamineStage::kReadingPioneerUserCode,
          ExamineStage::kReadingStandardUserCode, ExamineStage::kFindingEnd,
          ExamineStage::kReadingEnd, ExamineStage::kFindingStart,
          ExamineStage::kReadingStart, ExamineStage::kSettling,
          ExamineStage::kCheckingTransport, ExamineStage::kSpinningDown}));
}

// The disc was parked when the examination started, so it is parked when it
// ends. A player left spinning after being asked a question is one that has
// been altered by being asked, and whoever comes back to it has to work out
// whether it was like that already.
TEST(DiscExaminerTest, ADiscThatWasParkedIsParkedAgainAfterwards) {
  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, CavScript());

  ASSERT_FALSE(sent.empty());
  EXPECT_EQ(sent.back().stage, ExamineStage::kSpinningDown);
  EXPECT_EQ(sent.back().command, PlayerCommand::kStop);

  // Exactly one, and after the disc has been held still. On a Pioneer player
  // stop is Reject, and a Reject arriving at a transport that is already
  // spinning down opens the tray.
  int stops = 0;
  for (const ExamineStep& step : sent) {
    if (step.command == PlayerCommand::kStop) {
      ++stops;
    }
  }
  EXPECT_EQ(stops, 1);

  // And asked about immediately before, always: the settle may have been
  // refused, and somebody can stop the disc from the player's own front panel
  // while an examination is running.
  ASSERT_GE(sent.size(), 3u);
  EXPECT_EQ(sent[sent.size() - 2].stage, ExamineStage::kCheckingTransport);
  EXPECT_EQ(sent[sent.size() - 3].stage, ExamineStage::kSettling);
}

// Somebody stopped the disc from the player's own front panel while the
// examination was running, so by the time it comes to tidy up there is nothing
// to tidy. Sending the stop anyway would open the tray.
TEST(DiscExaminerTest, ADiscStoppedByHandMidExaminationIsNotStoppedAgain) {
  Script script = CavScript();
  script[ExamineStage::kCheckingTransport] = Answered("P01");

  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, script);

  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted);
  EXPECT_TRUE(Sent(sent, ExamineStage::kCheckingTransport));
  EXPECT_FALSE(Sent(sent, ExamineStage::kSpinningDown))
      << "a stop was sent to a disc that was not turning";
}

// Not knowing is not a licence to send it either. The findings are already in
// hand, and a disc left spinning is something somebody can stop themselves.
TEST(DiscExaminerTest, ATransportCheckThatIsRefusedSendsNoStop) {
  Script script = CavScript();
  script[ExamineStage::kCheckingTransport] = Refused();

  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, script);

  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted);
  EXPECT_FALSE(Sent(sent, ExamineStage::kSpinningDown));
}

// Put back means put back, not stopped. This player was playing when the
// examination began, so stopping it at the end would be as much of a change as
// leaving a parked one spinning.
TEST(DiscExaminerTest, ADiscThatWasAlreadyPlayingIsNotStopped) {
  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, ClvScript());

  ASSERT_FALSE(Sent(sent, ExamineStage::kSpinningUp));
  EXPECT_FALSE(Sent(sent, ExamineStage::kSpinningDown));
  EXPECT_EQ(sent.back().stage, ExamineStage::kSettling);
}

TEST(DiscExaminerTest, ACavDiscIsSeekedByFrameWithTheOldApplicationsAddresses) {
  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, CavScript());

  const ExamineStep* const end = Find(sent, ExamineStage::kFindingEnd);
  ASSERT_NE(end, nullptr);
  EXPECT_EQ(end->command, PlayerCommand::kSeekFrame);
  EXPECT_EQ(end->argument.value_or(-1), 60000);

  const ExamineStep* const start = Find(sent, ExamineStage::kFindingStart);
  ASSERT_NE(start, nullptr);
  EXPECT_EQ(start->command, PlayerCommand::kSeekFrame);
  EXPECT_EQ(start->argument.value_or(-1), 1);
}

TEST(DiscExaminerTest, AClvDiscIsSeekedByTimeCodeInstead) {
  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, ClvScript());

  const ExamineStep* const end = Find(sent, ExamineStage::kFindingEnd);
  ASSERT_NE(end, nullptr);
  EXPECT_EQ(end->command, PlayerCommand::kSeekTimeCode);
  EXPECT_EQ(end->argument.value_or(-1), 1595900);
}

TEST(DiscExaminerTest, APlayerAlreadyPlayingIsNotSpunUpAgain) {
  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, ClvScript());

  EXPECT_FALSE(Sent(sent, ExamineStage::kSpinningUp));

  // And the disc is still known to be there — the player said it was spinning
  // one, which is better evidence than a play command being accepted.
  EXPECT_TRUE(examiner.profile().disc_present.value);
  EXPECT_EQ(examiner.profile().disc_present.provenance, Provenance::kReported);
}

TEST(DiscExaminerTest, TheStepCountEndsWhereItStartedSaying) {
  DiscExaminer examiner(LevelIIIModel(), "A1");

  size_t last = 0;
  while (const std::optional<ExamineStep> step = examiner.Next()) {
    EXPECT_GE(examiner.steps_completed(), last);
    last = examiner.steps_completed();
    EXPECT_LE(examiner.steps_completed(), examiner.steps_planned());
    EXPECT_EQ(examiner.stage(), step->stage);
    examiner.Apply(Answered("R"));
  }

  EXPECT_EQ(examiner.steps_completed(), examiner.steps_planned());
  EXPECT_EQ(examiner.stage(), ExamineStage::kFinished);
}

TEST(DiscExaminerTest, AskingForTheStepTwiceDoesNotAdvanceIt) {
  DiscExaminer examiner(LevelIIIModel(), "A1");

  const std::optional<ExamineStep> first = examiner.Next();
  const std::optional<ExamineStep> again = examiner.Next();

  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first, again);
  EXPECT_EQ(examiner.steps_completed(), size_t{0});
}

// --- What it establishes ---------------------------------------------------

TEST(DiscExaminerTest, ACavDiscYieldsItsTypeItsAddressingAndItsLength) {
  DiscExaminer examiner(LevelIIIModel(), "A1");
  Drive(examiner, CavScript());

  const DiscProfile& disc = examiner.profile();

  EXPECT_EQ(disc.disc_type.value, DiscType::kCav);
  EXPECT_EQ(disc.disc_type.provenance, Provenance::kReported);

  EXPECT_EQ(disc.addressing.value, AddressMode::kFrame);
  EXPECT_EQ(disc.addressing.provenance, Provenance::kInferred);

  // The length is measured, never claimed: this is the frame the player
  // actually stopped at when it was asked for one the disc cannot have.
  EXPECT_EQ(disc.programme_end.value, 54000);
  EXPECT_EQ(disc.programme_end.provenance, Provenance::kMeasured);

  EXPECT_EQ(disc.programme_start.value, 1);
  EXPECT_TRUE(disc.lead_in_reachable.value);
  EXPECT_TRUE(disc.chapters.value);

  EXPECT_EQ(disc.tray.value, TrayState::kClosed);
  EXPECT_TRUE(disc.disc_present.value);
}

TEST(DiscExaminerTest, AClvDiscsAddressesAreReadAsTimeCodes) {
  DiscExaminer examiner(LevelIIIModel(), "A1");
  Drive(examiner, ClvScript());

  const DiscProfile& disc = examiner.profile();

  EXPECT_EQ(disc.disc_type.value, DiscType::kClv);
  EXPECT_EQ(disc.addressing.value, AddressMode::kTimeCode);

  // 0:50:45, and six significant digits — which the frame parser would have
  // refused outright rather than reporting as a frame number.
  EXPECT_EQ(disc.programme_end.value, 504500);
  EXPECT_EQ(disc.programme_end.provenance, Provenance::kMeasured);
}

TEST(DiscExaminerTest, TheWholeDiscStatusReplyIsKeptAndNotJustTheDecodedDigit) {
  // Only the disc-type digit is decoded, and the rest of the reply is the
  // evidence anybody decoding the others would have to start from.
  DiscExaminer examiner(LevelIIIModel(), "A1");
  Drive(examiner, CavScript());

  EXPECT_EQ(examiner.profile().disc_status_reply, "10001");
}

TEST(DiscExaminerTest, TheDiscSaysWhetherItHasChaptersAndIsNotAskedTwice) {
  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, ClvScript());

  EXPECT_TRUE(examiner.profile().chapters.known());
  EXPECT_FALSE(examiner.profile().chapters.value);
  EXPECT_EQ(examiner.profile().chapters.provenance, Provenance::kReported);

  // The disc has already answered, so probing for chapter one would be a
  // search command sent to find out something known — and it would move the
  // disc to do it.
  EXPECT_FALSE(Sent(sent, ExamineStage::kCheckingChapters));
}

TEST(DiscExaminerTest, TheSizeAndTheSideComeFromTheDiscsOwnProgrammeStatus) {
  // "11011" is the reading this project's Casper disc gives on its second
  // side: loaded, CLV, 12-inch, side 2, chapters. The side is the field the
  // old application never had and the one a capture most needs — two sides of
  // one disc are two files.
  Script script = ClvScript();
  script[ExamineStage::kReadingDiscStatus] = Answered("11011");

  DiscExaminer examiner(LevelIIIModel(), "A1");
  Drive(examiner, script);

  const DiscProfile& disc = examiner.profile();

  EXPECT_EQ(disc.disc_size.value, DiscSize::k30cm);
  EXPECT_EQ(disc.disc_size.provenance, Provenance::kReported);

  EXPECT_EQ(disc.disc_side.value, 2);
  EXPECT_EQ(disc.disc_side.provenance, Provenance::kReported);

  EXPECT_TRUE(disc.chapters.value);
  EXPECT_EQ(disc.disc_type.value, DiscType::kClv);
}

TEST(DiscExaminerTest, AnEightInchDiscIsReadAsOneRatherThanAsTheDefault) {
  Script script = CavScript();
  script[ExamineStage::kReadingDiscStatus] = Answered("10110");

  DiscExaminer examiner(LevelIIIModel(), "A1");
  Drive(examiner, script);

  EXPECT_EQ(examiner.profile().disc_size.value, DiscSize::k20cm);
  EXPECT_EQ(examiner.profile().disc_side.value, 2);
  EXPECT_FALSE(examiner.profile().chapters.value);
}

TEST(DiscExaminerTest, AStatusDigitCannotUndoADiscThatDemonstrablyPlayed) {
  // The player accepted a play command and then said "not loaded". Accepting
  // that would abandon a measurement that was going to work, on the word of a
  // digit against the evidence of the transport.
  Script script = CavScript();
  script[ExamineStage::kReadingDiscStatus] = Answered("00001");

  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, script);

  EXPECT_TRUE(examiner.profile().disc_present.value);
  EXPECT_TRUE(Sent(sent, ExamineStage::kFindingEnd));
  EXPECT_EQ(examiner.profile().programme_end.value, 54000);
}

TEST(DiscExaminerTest, AFieldThePlayerCouldNotDetermineIsNotReadAsNo) {
  // Both manuals document 'X' as a third value for the last three fields.
  // Reading it as '0' would turn "I could not tell which side this is" into
  // "side 1", which is the sort of confident wrong answer this whole design is
  // arranged against.
  Script script = CavScript();
  script[ExamineStage::kReadingDiscStatus] = Answered("10XXX");

  DiscExaminer examiner(LevelIIIModel(), "A1");
  Drive(examiner, script);

  const DiscProfile& disc = examiner.profile();

  EXPECT_FALSE(disc.disc_size.known());
  EXPECT_FALSE(disc.disc_side.known());
  EXPECT_FALSE(disc.chapters.known());

  // The two fields it could determine are still there.
  EXPECT_TRUE(disc.disc_present.value);
  EXPECT_EQ(disc.disc_type.value, DiscType::kCav);
}

TEST(DiscExaminerTest, TheVideoStandardIsAskedForRatherThanGuessedOrDeclared) {
  DiscExaminer ntsc(LevelIIIModel(), "A1");
  Drive(ntsc, CavScript());
  EXPECT_EQ(ntsc.profile().video_standard.value, VideoStandard::kNtsc);
  EXPECT_EQ(ntsc.profile().video_standard.provenance, Provenance::kReported);

  // "220" is this project's own bench reading from a PAL disc, and the same
  // player answers "110" for an NTSC one — which is why the standard could
  // never have come from the model.
  DiscExaminer pal(LevelIIIModel(), "A1");
  Drive(pal, ClvScript());
  EXPECT_EQ(pal.profile().video_standard.value, VideoStandard::kPal);
}

TEST(DiscExaminerTest, TheStandardIsTheDiscsAndNotWhateverIsBeingOutput) {
  // A player converting a PAL disc to an NTSC output. Taking the output field
  // would label the capture with the standard of the cable rather than of the
  // disc.
  Script script = CavScript();
  script[ExamineStage::kReadingTvSystem] = Answered("120");

  DiscExaminer examiner(LevelIIIModel(), "A1");
  Drive(examiner, script);

  EXPECT_EQ(examiner.profile().video_standard.value, VideoStandard::kPal);
}

TEST(DiscExaminerTest, APlayerThatWillNotSayTheStandardLeavesItUnknown) {
  Script script = CavScript();
  script[ExamineStage::kReadingTvSystem] = Silent();

  DiscExaminer examiner(LevelIIIModel(), "A1");
  Drive(examiner, script);

  EXPECT_FALSE(examiner.profile().video_standard.known());
  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted);
}

TEST(DiscExaminerTest, AModelWithNoTvSystemQueryIsNotAskedForOne) {
  PlayerDefinition definition = pioneer::LevelIII();
  definition.name = "Test player";
  definition.capabilities.tv_system = false;
  definition.commands[Index(PlayerCommand::kQueryTvSystem)] = CommandSpec{};

  DiscExaminer examiner(definition, "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, CavScript());

  EXPECT_FALSE(Sent(sent, ExamineStage::kReadingTvSystem));
  EXPECT_FALSE(examiner.profile().video_standard.known());
  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted);
}

// --- Partial failure -------------------------------------------------------

TEST(DiscExaminerTest, ARefusedQueryLeavesOneFieldUnknownAndFinishesTheRest) {
  Script script = CavScript();
  script[ExamineStage::kReadingEnd] = Silent();

  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, script);

  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted);
  EXPECT_FALSE(examiner.profile().programme_end.known());

  // Everything after it still ran, which is the whole difference from the old
  // application: it gave up at the first refusal.
  EXPECT_TRUE(Sent(sent, ExamineStage::kFindingStart));
  EXPECT_TRUE(Sent(sent, ExamineStage::kSettling));
  EXPECT_TRUE(examiner.profile().programme_start.known());
}

TEST(DiscExaminerTest, ADiscWhoseTypeIsUnknownIsNotSeekedOnAGuess) {
  // The cascade is real and is the right one. Frame and time-code seeks are
  // different commands with differently sized addresses, so with no disc type
  // there is no seek to send — and a length guessed with the wrong one would be
  // wrong by a factor of a hundred.
  Script script = CavScript();
  script[ExamineStage::kReadingDiscStatus] = Silent();

  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, script);

  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted);
  EXPECT_FALSE(examiner.profile().disc_type.known());
  EXPECT_FALSE(examiner.profile().programme_end.known());

  EXPECT_FALSE(Sent(sent, ExamineStage::kFindingEnd));
  EXPECT_FALSE(Sent(sent, ExamineStage::kReadingEnd));

  // And the steps that do not depend on it still happened.
  EXPECT_TRUE(Sent(sent, ExamineStage::kReadingStandardUserCode));
  EXPECT_TRUE(Sent(sent, ExamineStage::kSettling));
}

TEST(DiscExaminerTest, ARefusedSeekToTheStartMeansTheAddressIsNotTheStart) {
  // Not symmetrical with the seek past the end, and deliberately so: that one
  // is expected to be refused, this one has no excuse, and a refusal means the
  // player never moved. Reading the address anyway would report the end of the
  // disc as its beginning.
  Script script = CavScript();
  script[ExamineStage::kFindingStart] = Refused();

  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, script);

  EXPECT_FALSE(Sent(sent, ExamineStage::kReadingStart));
  EXPECT_FALSE(examiner.profile().programme_start.known());
  EXPECT_FALSE(examiner.profile().lead_in_reachable.known());

  // The measurement taken before it survives.
  EXPECT_EQ(examiner.profile().programme_end.value, 54000);
}

TEST(DiscExaminerTest, TheSeekPastTheEndIsRefusedAndThatIsTheTechniqueWorking) {
  DiscExaminer examiner(LevelIIIModel(), "A1");
  Script script = CavScript();
  ASSERT_EQ(script[ExamineStage::kFindingEnd].status, ReplyStatus::kRefused);

  Drive(examiner, script);

  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted);
  EXPECT_EQ(examiner.profile().programme_end.value, 54000);
}

TEST(DiscExaminerTest, AnAddressReplyThatSaidNothingClaimsNoLeadIn) {
  Script script = CavScript();
  script[ExamineStage::kReadingStart] = Answered("???");

  DiscExaminer examiner(LevelIIIModel(), "A1");
  Drive(examiner, script);

  // Neither an address nor a lead-in marker, so nothing is recorded either way.
  EXPECT_FALSE(examiner.profile().lead_in_reachable.known());
  EXPECT_FALSE(examiner.profile().programme_start.known());
}

TEST(DiscExaminerTest, ADiscWhoseStartIsNotTheLeadInSaysThatPlainly) {
  Script script = CavScript();
  script[ExamineStage::kReadingStart] = Answered("00001");

  DiscExaminer examiner(LevelIIIModel(), "A1");
  Drive(examiner, script);

  EXPECT_TRUE(examiner.profile().lead_in_reachable.known());
  EXPECT_FALSE(examiner.profile().lead_in_reachable.value);
  EXPECT_EQ(examiner.profile().programme_start.value, 1);
}

// --- Stopping early --------------------------------------------------------

TEST(DiscExaminerTest, AnOpenTrayIsAFindingRatherThanAFailure) {
  Script script;
  script[ExamineStage::kCheckingPlayer] = Answered("P00");

  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, script);

  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kTrayOpen);
  EXPECT_EQ(examiner.profile().tray.value, TrayState::kOpen);
  EXPECT_EQ(sent.size(), size_t{1});

  // Nothing was claimed about a disc nobody could see.
  EXPECT_FALSE(examiner.profile().disc_present.known());
}

TEST(DiscExaminerTest, APlayerThatWillNotPlayReportsNoDiscAndStopsAsking) {
  Script script = CavScript();
  script[ExamineStage::kSpinningUp] = Refused();

  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, script);

  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kNoDisc);
  EXPECT_TRUE(examiner.profile().disc_present.known());
  EXPECT_FALSE(examiner.profile().disc_present.value);

  // Every remaining step would have been a refusal costing its own timeout.
  EXPECT_EQ(Stages(sent),
            (std::vector<ExamineStage>{ExamineStage::kCheckingPlayer,
                                       ExamineStage::kSpinningUp}));
}

TEST(DiscExaminerTest, ALinkFailureStopsTheSequenceAndKeepsWhatWasFound) {
  Script script = CavScript();
  script[ExamineStage::kReadingStandardUserCode] = LinkFailed();

  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, script);

  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kLinkFailed);
  EXPECT_FALSE(Sent(sent, ExamineStage::kFindingEnd));

  // A cable pulled out halfway through does not undo what was already read.
  EXPECT_EQ(examiner.profile().disc_type.value, DiscType::kCav);
}

TEST(DiscExaminerTest, ASessionThatIsNotConnectedIsALinkFailureAndNotAFinding) {
  Script script = CavScript();
  Reply not_connected;
  not_connected.status = ReplyStatus::kNotConnected;
  script[ExamineStage::kCheckingPlayer] = not_connected;

  DiscExaminer examiner(LevelIIIModel(), "A1");
  Drive(examiner, script);

  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kLinkFailed);
}

TEST(DiscExaminerTest, CancellingKeepsTheProfileAndCanBeStartedAgain) {
  DiscExaminer examiner(LevelIIIModel(), "A1");

  // Far enough in to have learnt the disc type.
  ASSERT_TRUE(examiner.Next().has_value());
  examiner.Apply(Answered("P01"));
  ASSERT_TRUE(examiner.Next().has_value());
  examiner.Apply(Answered("R"));
  ASSERT_TRUE(examiner.Next().has_value());
  examiner.Apply(Answered("10001"));

  examiner.Cancel();

  EXPECT_TRUE(examiner.finished());
  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCancelled);
  EXPECT_FALSE(examiner.Next().has_value());

  // What was learnt before the cancel is still there: cancelling should not
  // cost more than waiting would have.
  EXPECT_EQ(examiner.profile().disc_type.value, DiscType::kCav);

  examiner.Restart();

  EXPECT_FALSE(examiner.finished());
  EXPECT_EQ(examiner.steps_completed(), size_t{0});
  EXPECT_FALSE(examiner.profile().disc_type.known());

  const std::vector<ExamineStep> sent = Drive(examiner, CavScript());
  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted);
  EXPECT_EQ(sent.size(), size_t{13});
}

TEST(DiscExaminerTest, ACancelIsPossibleBetweenAnyTwoSteps) {
  const Script script = CavScript();

  // Every position in the sequence, one run each.
  for (size_t stop_after = 0; stop_after < 11; ++stop_after) {
    DiscExaminer examiner(LevelIIIModel(), "A1");

    for (size_t sent = 0; sent < stop_after; ++sent) {
      const std::optional<ExamineStep> step = examiner.Next();
      if (!step.has_value()) {
        ADD_FAILURE() << "the plan ran out at stop_after=" << stop_after;
        break;
      }
      const auto found = script.find(step->stage);
      examiner.Apply(found == script.end() ? Answered("R") : found->second);
    }

    examiner.Cancel();
    EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCancelled);

    examiner.Restart();
    Drive(examiner, script);
    EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted)
        << "could not restart after stopping at step " << stop_after;
  }
}

TEST(DiscExaminerTest, AReplyArrivingAfterACancelIsIgnored) {
  DiscExaminer examiner(LevelIIIModel(), "A1");

  ASSERT_TRUE(examiner.Next().has_value());
  examiner.Cancel();

  // The command was already in flight when the user pressed cancel, which is
  // the ordinary case rather than a rare one — every step takes seconds.
  examiner.Apply(Answered("P01"));

  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCancelled);
  EXPECT_FALSE(examiner.profile().tray.known());
}

// --- The user codes --------------------------------------------------------

TEST(DiscExaminerTest, BothUserCodesAreReadWithoutBeingAskedFor) {
  // They are the disc's own identifying records, and an examination that left
  // them unread would be one somebody had to follow with two more trips to the
  // remote — the second of which moves the disc.
  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, CavScript());

  EXPECT_TRUE(Sent(sent, ExamineStage::kReadingPioneerUserCode));
  EXPECT_TRUE(Sent(sent, ExamineStage::kReadingStandardUserCode));

  EXPECT_TRUE(examiner.profile().pioneer_user_code.read());
  EXPECT_TRUE(examiner.profile().standard_user_code.read());
}

TEST(DiscExaminerTest,
     ThePioneerUserCodeIsReadBeforeAnythingHasBeenPositioned) {
  // It searches to the lead-in to answer, so anywhere later in the sequence it
  // would undo a measurement that had just been taken. That is also why it can
  // be read unconditionally: here it costs a seek that was going to happen.
  DiscExaminer examiner(LevelIIIModel(), "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, CavScript());
  const std::vector<ExamineStage> stages = Stages(sent);

  const auto user_code = std::find(stages.begin(), stages.end(),
                                   ExamineStage::kReadingPioneerUserCode);
  const auto end =
      std::find(stages.begin(), stages.end(), ExamineStage::kFindingEnd);

  ASSERT_NE(user_code, stages.end());
  EXPECT_LT(user_code, end);

  EXPECT_TRUE(examiner.profile().pioneer_user_code.read());
  EXPECT_EQ(examiner.profile().pioneer_user_code.text, "#59-014 CASPER");

  // And it changed nothing else. The length is still the measured one.
  EXPECT_EQ(examiner.profile().programme_end.value, 54000);
}

TEST(DiscExaminerTest, AnErrorCodeIsRecordedAsNoUserCodeRatherThanAsOne) {
  // Two of the five discs on this project's bench answer "E04" here. Shown as
  // the disc's user code it would be a disc claiming to be called E04.
  DiscExaminer examiner(LevelIIIModel(), "A1");
  Script script = CavScript();
  script[ExamineStage::kReadingPioneerUserCode] = Answered("E04");
  Drive(examiner, script);

  const UserCodeReading& code = examiner.profile().pioneer_user_code;
  EXPECT_EQ(code.outcome, UserCodeReading::Outcome::kNotEncoded);
  EXPECT_FALSE(code.read());
  EXPECT_EQ(code.text, "E04");
}

TEST(DiscExaminerTest, AUserCodeTheDiscHasIsRecordedEvenWhereItCouldNotBeRead) {
  // A run of "`" is the player saying it could not read those characters off
  // the disc. That is a fact about the disc worth recording, and it is not the
  // same as the disc having no user code.
  DiscExaminer examiner(LevelIIIModel(), "A1");
  Script script = CavScript();
  script[ExamineStage::kReadingPioneerUserCode] = Answered("#59-014``````");
  Drive(examiner, script);

  EXPECT_TRUE(examiner.profile().pioneer_user_code.read());
  EXPECT_EQ(examiner.profile().pioneer_user_code.text, "#59-014``````");
}

TEST(DiscExaminerTest, AUserCodeQueryThatWentUnansweredIsNotADiscWithoutOne) {
  Script script = CavScript();
  script[ExamineStage::kReadingStandardUserCode] = Silent();

  DiscExaminer examiner(LevelIIIModel(), "A1");
  Drive(examiner, script);

  EXPECT_EQ(examiner.profile().standard_user_code.outcome,
            UserCodeReading::Outcome::kRefused);
}

TEST(DiscExaminerTest, TheStandardUserCodeIsReadWhereTheModelOffersIt) {
  DiscExaminer examiner(LevelIIIModel(), "A1");
  Drive(examiner, CavScript());

  EXPECT_TRUE(examiner.profile().standard_user_code.read());
  EXPECT_EQ(examiner.profile().standard_user_code.text, "Y1000");
}

// --- What the model can be asked ------------------------------------------

TEST(DiscExaminerTest, AModelThatCannotReportChaptersProbesForThemInstead) {
  // The fallback, and the only case in which the disc is moved to answer a
  // question the disc status normally answers for free.
  PlayerDefinition definition = pioneer::LevelIII();
  definition.name = "Test player";
  definition.disc_status.chapters = ReplyField{};

  DiscExaminer examiner(definition, "A1");
  Script script = CavScript();
  script[ExamineStage::kCheckingChapters] = Answered("R");

  const std::vector<ExamineStep> sent = Drive(examiner, script);

  EXPECT_TRUE(Sent(sent, ExamineStage::kCheckingChapters));
  EXPECT_TRUE(examiner.profile().chapters.value);
  EXPECT_EQ(examiner.profile().chapters.provenance, Provenance::kMeasured);
  EXPECT_EQ(examiner.steps_planned(), size_t{14});
}

TEST(DiscExaminerTest, AModelWithNeitherWayOfKnowingLeavesChaptersUnknown) {
  PlayerDefinition definition = pioneer::LevelIII();
  definition.name = "Test player";
  definition.disc_status.chapters = ReplyField{};
  definition.capabilities.chapter_search = false;
  definition.commands[Index(PlayerCommand::kSeekChapter)] = CommandSpec{};

  DiscExaminer examiner(definition, "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, CavScript());

  EXPECT_FALSE(Sent(sent, ExamineStage::kCheckingChapters));
  EXPECT_FALSE(examiner.profile().chapters.known());

  // The plan is shorter by exactly that step rather than carrying one that
  // would only ever be refused — so the progress bar is honest too.
  EXPECT_EQ(examiner.steps_planned(), size_t{13});
  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted);
}

TEST(DiscExaminerTest, AModelWithNoUserCodeQueriesIsNotAskedForThem) {
  PlayerDefinition definition = pioneer::LevelIII();
  definition.name = "Test player";
  definition.capabilities.standard_user_code = false;
  definition.capabilities.pioneer_user_code = false;
  definition.commands[Index(PlayerCommand::kQueryStandardUserCode)] =
      CommandSpec{};
  definition.commands[Index(PlayerCommand::kQueryPioneerUserCode)] =
      CommandSpec{};

  DiscExaminer examiner(definition, "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, CavScript());

  EXPECT_FALSE(Sent(sent, ExamineStage::kReadingStandardUserCode));
  EXPECT_FALSE(Sent(sent, ExamineStage::kReadingPioneerUserCode));
  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted);
}

TEST(DiscExaminerTest, AModelThatCannotSeekIsStillExaminedForWhatItCanSay) {
  PlayerDefinition definition = pioneer::LevelIII();
  definition.name = "Test player";
  definition.capabilities.frame_search = false;
  definition.capabilities.time_code_search = false;
  definition.commands[Index(PlayerCommand::kSeekFrame)] = CommandSpec{};
  definition.commands[Index(PlayerCommand::kSeekTimeCode)] = CommandSpec{};

  DiscExaminer examiner(definition, "A1");
  const std::vector<ExamineStep> sent = Drive(examiner, CavScript());

  EXPECT_FALSE(Sent(sent, ExamineStage::kFindingEnd));
  EXPECT_FALSE(examiner.profile().programme_end.known());

  // The disc type is still established, which is most of what the capture setup
  // needs to ask the right questions.
  EXPECT_EQ(examiner.profile().disc_type.value, DiscType::kCav);
  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted);
}

// --- The identifying scope -------------------------------------------------
//
// What the naming fields want is what the disc says about itself, which the
// player answers off the lead-in it has already read. Measuring the side means
// seeking to both ends of it, which takes about a minute and leaves the disc
// somewhere else — a cost nobody filling in a file name should be made to pay.

TEST(DiscExaminerTest, AnIdentifyingPassNeverMovesTheDisc) {
  DiscExaminer examiner(LevelIIIModel(), "A1", ExamineScope::kIdentify);
  const std::vector<ExamineStep> sent = Drive(examiner, CavScript());

  // Every step that would move the optical assembly, absent. The Pioneer user
  // code is in this list and is the easy one to forget: it is a query, but the
  // player searches to the lead-in to answer it.
  for (const ExamineStage stage :
       {ExamineStage::kFindingEnd, ExamineStage::kReadingEnd,
        ExamineStage::kFindingStart, ExamineStage::kReadingStart,
        ExamineStage::kCheckingChapters, ExamineStage::kReadingPioneerUserCode,
        ExamineStage::kReadingStandardUserCode}) {
    EXPECT_FALSE(Sent(sent, stage))
        << "an identifying pass sent stage " << static_cast<int>(stage);
  }

  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted);
}

TEST(DiscExaminerTest, AnIdentifyingPassAsksWhatTheNamingFieldsWant) {
  DiscExaminer examiner(LevelIIIModel(), "A1", ExamineScope::kIdentify);
  const std::vector<ExamineStep> sent = Drive(examiner, CavScript());

  EXPECT_EQ(
      Stages(sent),
      (std::vector<ExamineStage>{
          ExamineStage::kCheckingPlayer, ExamineStage::kSpinningUp,
          ExamineStage::kReadingDiscStatus, ExamineStage::kReadingTvSystem,
          ExamineStage::kSettling, ExamineStage::kCheckingTransport,
          ExamineStage::kSpinningDown}));

  // And the three facts those steps establish are the three the naming fields
  // are filled from.
  const DiscProfile& disc = examiner.profile();
  EXPECT_EQ(disc.disc_type.value, DiscType::kCav);
  EXPECT_EQ(disc.video_standard.value, VideoStandard::kNtsc);
  EXPECT_TRUE(disc.disc_side.known());
}

TEST(DiscExaminerTest, AnIdentifyingPassPutsTheDiscBackAsItFoundIt) {
  // The disc has to be turning for anything below the first query to be
  // answered, so the pass spins it up — and a button in a naming dialog that
  // quietly leaves a player running is worse than one that takes a moment
  // longer. This player was parked, so it is parked again.
  DiscExaminer examiner(LevelIIIModel(), "A1", ExamineScope::kIdentify);
  const std::vector<ExamineStep> sent = Drive(examiner, CavScript());

  ASSERT_GE(sent.size(), 3u);
  EXPECT_EQ(sent[sent.size() - 3].stage, ExamineStage::kSettling);
  EXPECT_EQ(sent[sent.size() - 3].command, PlayerCommand::kPause);

  // Asked before it is stopped, always: a stop sent to a disc that is not
  // turning opens the tray on a Pioneer player.
  EXPECT_EQ(sent[sent.size() - 2].stage, ExamineStage::kCheckingTransport);
  EXPECT_EQ(sent[sent.size() - 2].command, PlayerCommand::kQueryActiveMode);
  EXPECT_EQ(sent.back().stage, ExamineStage::kSpinningDown);
  EXPECT_EQ(sent.back().command, PlayerCommand::kStop);
}

TEST(DiscExaminerTest, AnIdentifyingPassCountsOnlyTheStepsItWillTake) {
  // The progress line is built from these, so a plan carrying steps it has no
  // intention of running would be a bar that jumped rather than moved.
  DiscExaminer full(LevelIIIModel(), "A1", ExamineScope::kFull);
  DiscExaminer identify(LevelIIIModel(), "A1", ExamineScope::kIdentify);

  EXPECT_LT(identify.steps_planned(), full.steps_planned());
  EXPECT_EQ(identify.steps_planned(), 7u);
}

TEST(DiscExaminerTest, AnIdentifyingPassStillReportsWhatItDidLearn) {
  // The same partial-failure rule as a full examination: a refused query leaves
  // its field unknown and the pass carries on, because a naming dialog that got
  // the side but not the standard is still better off than one that got
  // nothing.
  Script script = CavScript();
  script[ExamineStage::kReadingTvSystem] = Refused();

  DiscExaminer examiner(LevelIIIModel(), "A1", ExamineScope::kIdentify);
  Drive(examiner, script);

  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kCompleted);
  EXPECT_FALSE(examiner.profile().video_standard.known());
  EXPECT_EQ(examiner.profile().disc_type.value, DiscType::kCav);
  EXPECT_TRUE(examiner.profile().disc_side.known());
}

TEST(DiscExaminerTest, AnIdentifyingPassStopsForAnOpenTrayLikeAnyOther) {
  Script script = CavScript();
  script[ExamineStage::kCheckingPlayer] = Answered("P00");

  DiscExaminer examiner(LevelIIIModel(), "A1", ExamineScope::kIdentify);
  const std::vector<ExamineStep> sent = Drive(examiner, script);

  EXPECT_EQ(examiner.outcome(), ExamineOutcome::kTrayOpen);
  EXPECT_EQ(examiner.profile().tray.value, TrayState::kOpen);
  EXPECT_EQ(sent.size(), 1u);
}

TEST(DiscExaminerTest, TheDefaultScopeIsStillEverything) {
  // Every existing caller passes no scope and must get the full examination,
  // including both length measurements.
  DiscExaminer examiner(LevelIIIModel(), "A1");
  EXPECT_EQ(examiner.scope(), ExamineScope::kFull);

  const std::vector<ExamineStep> sent = Drive(examiner, CavScript());
  EXPECT_TRUE(Sent(sent, ExamineStage::kFindingEnd));
  EXPECT_TRUE(Sent(sent, ExamineStage::kReadingStart));
}

}  // namespace
}  // namespace ddd::player
