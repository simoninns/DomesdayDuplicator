/************************************************************************

    test_svf_player.cpp

    T1 unit test for the SVF player and its TAP state machine
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

#include "fake_jtag_cable.h"
#include "svf_fixtures.h"
#include "svf_player.h"

namespace ddd::capture {
namespace {

std::string AsBits(const std::vector<bool>& values) {
  std::string text;
  text.reserve(values.size());
  for (bool value : values) {
    text.push_back(value ? '1' : '0');
  }
  return text;
}

// Every run opens the same way: five cycles with TMS high, which reaches
// Test-Logic-Reset from any state the last run left the TAP in.
constexpr char kSync[] = "11111";

class SvfPlayerTest : public ::testing::Test {
 protected:
  SvfPlayResult Play(const std::string& text) {
    SvfPlayer player(cable_);
    return player.Play(text);
  }

  FakeJtagCable cable_;
};

// --- Getting to a known state ----------------------------------------------

TEST_F(SvfPlayerTest, ARunBeginsByForcingTheTapToReset) {
  const SvfPlayResult result = Play("STATE IDLE;");

  ASSERT_TRUE(result.succeeded) << result.problem;
  EXPECT_EQ(AsBits(cable_.tms()), std::string(kSync) + "0");
}

// --- Scans -----------------------------------------------------------------

// The whole of a shift, asserted as the cycles it is: walk to the shift
// state, clock the bits with TMS low, raise TMS on the last one to leave as
// it goes, and walk to the state the file said scans end in.
TEST_F(SvfPlayerTest, AnInstructionScanWalksThereShiftsAndComesBack) {
  const SvfPlayResult result = Play("STATE IDLE; SIR 10 TDI (006);");

  ASSERT_TRUE(result.succeeded) << result.problem;
  EXPECT_EQ(AsBits(cable_.tms()),
            std::string(kSync) + "0" + "1100" + "0000000001" + "10");
  EXPECT_EQ(AsBits(cable_.tdi()),
            std::string("00000") + "0" + "0000" + "0110000000" + "00");
  EXPECT_EQ(result.shifted_bits, 10u);
}

TEST_F(SvfPlayerTest, ADataScanTakesTheOtherBranchOfTheStateMachine) {
  const SvfPlayResult result = Play("STATE IDLE; SDR 4 TDI (5);");

  ASSERT_TRUE(result.succeeded) << result.problem;
  EXPECT_EQ(AsBits(cable_.tms()),
            std::string(kSync) + "0" + "100" + "0001" + "10");
  EXPECT_EQ(AsBits(cable_.tdi()),
            std::string("00000") + "0" + "000" + "1010" + "00");
}

TEST_F(SvfPlayerTest, ScansEndInTheStateTheFileAsksFor) {
  const SvfPlayResult result =
      Play("ENDDR DRPAUSE; STATE IDLE; SDR 4 TDI (0);");

  ASSERT_TRUE(result.succeeded) << result.problem;

  // Exit1-DR to Pause-DR is one cycle with TMS low, where ending in IDLE
  // would have been "10".
  EXPECT_EQ(AsBits(cable_.tms()),
            std::string(kSync) + "0" + "100" + "0001" + "0");
}

TEST_F(SvfPlayerTest, AnEndStateAScanCannotRestInIsRefused) {
  const SvfPlayResult result = Play("ENDDR DRSHIFT;");

  EXPECT_FALSE(result.succeeded);
  EXPECT_NE(result.problem.find("rest in"), std::string::npos)
      << result.problem;
}

// --- What comes back -------------------------------------------------------

TEST_F(SvfPlayerTest, AnAnswerThatMatchesPasses) {
  cable_.AnswerWithHex("A", 4);

  const SvfPlayResult result = Play("SDR 4 TDI (0) TDO (A);");
  EXPECT_TRUE(result.succeeded) << result.problem;
}

// The one failure that means the hardware disagreed rather than the file
// being wrong, so it says where in the file it was and what it saw.
TEST_F(SvfPlayerTest, AnAnswerThatDiffersStopsTheRunAndSaysWhere) {
  cable_.AnswerWithHex("B", 4);

  const SvfPlayResult result = Play("\n\nSDR 4 TDI (0) TDO (A);");

  EXPECT_FALSE(result.succeeded);
  EXPECT_EQ(result.line, 3u);
  EXPECT_NE(result.problem.find("did not answer"), std::string::npos)
      << result.problem;
  EXPECT_NE(result.problem.find('B'), std::string::npos) << result.problem;
}

// The failure message has to show the bit it names.
//
// This is a regression: the message used to print the most significant 32
// digits of both values and stop, so a wide scan reported a window that could
// not contain the bit that disagreed. On the bench (2026-08-19) that produced
// "at bit 286" beside 128 bits taken from the far end of a 732-bit scan —
// true, and impossible to do anything with.
TEST_F(SvfPlayerTest, AWideScanShowsTheBitsAroundTheOneThatDisagreed) {
  std::vector<bool> answers(732, false);
  answers[286] = true;
  cable_.AnswerWith(answers);

  const SvfPlayResult result = Play("SDR 732 TDI (0) TDO (0);");

  ASSERT_FALSE(result.succeeded);
  EXPECT_NE(result.problem.find("at bit 286 of a 732-bit scan"),
            std::string::npos)
      << result.problem;

  // The window is named, so the two values can be lined up against the bit
  // number rather than guessed at.
  EXPECT_NE(result.problem.find("across bits 347 to 220"), std::string::npos)
      << result.problem;

  // And the bit is in it: bit 286 is the middle bit of nibble 71, which is
  // sixteenth from the top of the window, with the elisions either side
  // saying that there is more of the value than is shown.
  EXPECT_NE(result.problem.find("…00000000000000040000000000000000…"),
            std::string::npos)
      << result.problem;
}

// A value that fits in the window is shown whole, with nothing elided.
TEST_F(SvfPlayerTest, ANarrowScanIsShownEndToEnd) {
  cable_.AnswerWithHex("B", 4);

  const SvfPlayResult result = Play("SDR 4 TDI (0) TDO (A);");

  ASSERT_FALSE(result.succeeded);
  EXPECT_EQ(result.problem.find("…"), std::string::npos) << result.problem;
  EXPECT_NE(result.problem.find("it said B where A was expected"),
            std::string::npos)
      << result.problem;
}

// Which statement it was in, so that a report of a failure can be looked up
// in the file that produced it without counting lines by hand.
TEST_F(SvfPlayerTest, AFailureNamesTheKindOfStatementItWasIn) {
  cable_.AnswerWithHex("B", 4);

  const SvfPlayResult result = Play("SDR 4 TDI (0) TDO (A);");

  ASSERT_FALSE(result.succeeded);
  EXPECT_EQ(result.statement_keyword, "SDR");
}

// And not the last statement that ran, when the failure is in reading the
// next one rather than in playing it.
TEST_F(SvfPlayerTest, AFailureBetweenStatementsNamesNoStatement) {
  const SvfPlayResult result = Play("STATE IDLE; SDR 4 TDI (0)");

  ASSERT_FALSE(result.succeeded);
  EXPECT_TRUE(result.statement_keyword.empty()) << result.statement_keyword;
}

TEST_F(SvfPlayerTest, MaskedBitsAreNotCompared) {
  cable_.AnswerWithHex("B", 4);

  // 0xB against 0xA differs in one bit, and the mask says not to look at it.
  const SvfPlayResult result = Play("SDR 4 TDI (0) TDO (A) MASK (E);");
  EXPECT_TRUE(result.succeeded) << result.problem;
}

TEST_F(SvfPlayerTest, AScanWithNothingExpectedReadsNothingBack) {
  const SvfPlayResult result = Play("SDR 8 TDI (00);");

  ASSERT_TRUE(result.succeeded) << result.problem;
  for (bool read : cable_.read()) {
    EXPECT_FALSE(read) << "the cable was made to wait for an answer nobody "
                          "asked for";
  }
}

// --- What a statement remembers --------------------------------------------

// TDI, MASK and SMASK carry over to the next scan of the same kind and TDO
// does not, which is the format's own rule and the one place a reader can
// quietly do the wrong thing: a scan with no TDO compares nothing rather
// than repeating the last comparison.
TEST_F(SvfPlayerTest, WhatIsShiftedCarriesOverToTheNextScanOfTheSameLength) {
  const SvfPlayResult result = Play("SDR 4 TDI (5); SDR 4;");

  ASSERT_TRUE(result.succeeded) << result.problem;

  const std::string tdi = AsBits(cable_.tdi());
  EXPECT_EQ(tdi.substr(tdi.size() - 6, 4), "1010")
      << "the second scan shifted something other than what the first did";
}

TEST_F(SvfPlayerTest, AnExpectedAnswerDoesNotCarryOver) {
  cable_.AnswerWithHex("A", 4);

  // The second scan would fail if TDO were remembered: nothing answers it.
  const SvfPlayResult result = Play("SDR 4 TDI (0) TDO (A); SDR 4 TDI (0);");
  EXPECT_TRUE(result.succeeded) << result.problem;
}

TEST_F(SvfPlayerTest, AScanThatChangesLengthHasToSayWhatItShifts) {
  const SvfPlayResult result = Play("SDR 4 TDI (5); SDR 8;");

  EXPECT_FALSE(result.succeeded);
  EXPECT_NE(result.problem.find("changes length"), std::string::npos)
      << result.problem;
}

// --- Waiting ---------------------------------------------------------------

TEST_F(SvfPlayerTest, AWaitClocksWhereItWasToldTo) {
  const SvfPlayResult result = Play("STATE IDLE; RUNTEST IDLE 8 TCK;");

  ASSERT_TRUE(result.succeeded) << result.problem;
  EXPECT_EQ(result.run_clocks, 8u);
  EXPECT_EQ(cable_.run_clocks(), 8u);
  EXPECT_EQ(AsBits(cable_.tms()), std::string(kSync) + "0" + "00000000");
}

TEST_F(SvfPlayerTest, AWaitLeavesTheTapWhereItsEndStateSays) {
  const SvfPlayResult result =
      Play("STATE IDLE; RUNTEST IDLE 4 TCK ENDSTATE DRPAUSE; SDR 4 TDI (0);");

  ASSERT_TRUE(result.succeeded) << result.problem;

  // Four idle cycles, then the walk the end state asked for — IDLE to
  // Pause-DR is "1010" — and then the scan starts from there: Pause-DR
  // reaches Shift-DR through Exit2-DR as "10", where the same scan from IDLE
  // would have started "100".
  EXPECT_EQ(AsBits(cable_.tms()),
            std::string(kSync) + "0" + "0000" + "1010" + "10" + "0001" + "10");
}

// A wait's cycle count is a duration in disguise: Quartus works it out from
// the clock rate the file declares, so the same file emitted at 6 MHz has
// every count a third larger and the same erase in it. A cable clocking
// faster than the file declares would therefore cut every wait short — a
// flash erase among them — so the wait is held open on this side instead.
TEST_F(SvfPlayerTest, AWaitTakesAtLeastAsLongAsTheFileMeantItTo) {
  const auto started = std::chrono::steady_clock::now();

  // Twenty milliseconds at the declared rate, against a cable that clocks in
  // no time at all.
  const SvfPlayResult result =
      Play("FREQUENCY 1.00E+04 HZ; STATE IDLE; RUNTEST 200 TCK;");

  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
          .count();

  ASSERT_TRUE(result.succeeded) << result.problem;
  EXPECT_GE(seconds, 0.018) << "the wait was cut short";
}

// And with no rate declared there is nothing to work a duration out from, so
// the cycles are all there is and the run is not slowed down by guesswork.
TEST_F(SvfPlayerTest, AWaitWithNoDeclaredRateIsJustItsCycles) {
  const auto started = std::chrono::steady_clock::now();

  ASSERT_TRUE(Play("STATE IDLE; RUNTEST 4500000 TCK;").succeeded);

  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
          .count();
  EXPECT_LT(seconds, 1.0);
}

TEST_F(SvfPlayerTest, AWaitOnAClockThisCableDoesNotHaveIsRefused) {
  const SvfPlayResult result = Play("RUNTEST 8 SCK;");

  EXPECT_FALSE(result.succeeded);
  EXPECT_NE(result.problem.find("system clock"), std::string::npos)
      << result.problem;
}

// --- Files this player will not play ---------------------------------------

// A non-empty header or trailer means other devices on the chain, whose
// registers this file expects to shift through. Guessing at a chain this
// project does not have would mean shifting a flash image into whatever was
// actually there.
TEST_F(SvfPlayerTest, AFileForALongerChainIsRefused) {
  const SvfPlayResult result = Play("HDR 8 TDI (00);");

  EXPECT_FALSE(result.succeeded);
  EXPECT_NE(result.problem.find("more than one device"), std::string::npos)
      << result.problem;
}

TEST_F(SvfPlayerTest, AnEmptyHeaderIsFine) {
  EXPECT_TRUE(Play("HDR 0; HIR 0; TDR 0; TIR 0;").succeeded);
}

TEST_F(SvfPlayerTest, AFileThatDrivesAResetLineIsRefused) {
  const SvfPlayResult result = Play("TRST ON;");

  EXPECT_FALSE(result.succeeded);
  EXPECT_NE(result.problem.find("reset line"), std::string::npos)
      << result.problem;
}

TEST_F(SvfPlayerTest, AFileWithNoResetLineIsFine) {
  EXPECT_TRUE(Play("TRST ABSENT;").succeeded);
  EXPECT_TRUE(Play("TRST OFF;").succeeded);
}

TEST_F(SvfPlayerTest, AStatementThisPlayerDoesNotKnowIsRefused) {
  const SvfPlayResult result = Play("PIOMAP (IN A OUT B);");

  EXPECT_FALSE(result.succeeded);
  EXPECT_NE(result.problem.find("PIOMAP"), std::string::npos) << result.problem;
}

// Truncating it instead would shift something nobody described, and the two
// readings of such a file differ by exactly the bits that were dropped.
TEST_F(SvfPlayerTest, AValueWiderThanItsScanIsRefused) {
  const SvfPlayResult result = Play("SDR 4 TDI (FF);");

  EXPECT_FALSE(result.succeeded);
  EXPECT_NE(result.problem.find("more bits"), std::string::npos)
      << result.problem;
}

TEST_F(SvfPlayerTest, AFileThatStopsMidStatementIsRefused) {
  EXPECT_FALSE(Play("SDR 4 TDI (5)").succeeded);
  EXPECT_FALSE(Play("SDR 4 TDI (5").succeeded);
}

TEST_F(SvfPlayerTest, ARubbishValueIsRefused) {
  EXPECT_FALSE(Play("SDR 4 TDI (XZ);").succeeded);
}

// --- The shape of a real file ----------------------------------------------

TEST_F(SvfPlayerTest, CommentsAndValuesSplitAcrossLinesAreRead) {
  const SvfPlayResult result = Play(
      "! a comment\n"
      "// another\n"
      "SDR 16 TDI (00\n"
      "\tFF);\n");

  ASSERT_TRUE(result.succeeded) << result.problem;

  const std::string tdi = AsBits(cable_.tdi());
  EXPECT_EQ(tdi.substr(tdi.size() - 18, 16), "1111111100000000");
}

TEST_F(SvfPlayerTest, TheClockRateTheFileAsksForIsReported) {
  const SvfPlayResult result = Play(kGrammarSvf);

  ASSERT_TRUE(result.succeeded) << result.problem;
  EXPECT_DOUBLE_EQ(result.frequency_hz, 1.0e7);
  EXPECT_EQ(result.statements, 9u);
}

// A file Quartus wrote, rather than one written to be parsed. What it adds
// is the shape the tool actually emits: values wrapped and indented,
// comments between statements, the same instruction loaded over and over,
// and waits counted in millions of cycles.
TEST_F(SvfPlayerTest, AQuartusFilePlaysAgainstADeviceThatAgreesWithIt) {
  cable_.AnswerWith(QuartusOpeningAnswers());

  const SvfPlayResult result = Play(kQuartusOpeningSvf);

  ASSERT_TRUE(result.succeeded)
      << "line " << result.line << ": " << result.problem;
  EXPECT_DOUBLE_EQ(result.frequency_hz, 4.5e6);
  EXPECT_EQ(result.run_clocks, 4500000u + 8u * 6u);
}

TEST_F(SvfPlayerTest, AQuartusFileStopsAgainstADeviceThatDoesNot) {
  std::vector<bool> answers = QuartusOpeningAnswers();
  answers[8] = !answers[8];
  cable_.AnswerWith(answers);

  const SvfPlayResult result = Play(kQuartusOpeningSvf);

  EXPECT_FALSE(result.succeeded);
  EXPECT_GT(result.line, 0u);
}

// --- Being driven ----------------------------------------------------------

TEST_F(SvfPlayerTest, ProgressEndsAtTheEndOfTheFile) {
  size_t done = 0;
  size_t total = 0;

  SvfPlayer player(cable_);
  player.SetProgressCallback([&done, &total](size_t at, size_t size) {
    done = at;
    total = size;
  });

  const std::string text = kGrammarSvf;
  ASSERT_TRUE(player.Play(text).succeeded);
  EXPECT_EQ(done, text.size());
  EXPECT_EQ(total, text.size());
}

// Stopping is not failing: a half-written flash is safe, and the same file
// played again finishes the job. The two are reported separately so that
// what a user is told is what happened.
TEST_F(SvfPlayerTest, ARunCanBeStoppedBetweenStatements) {
  SvfPlayer player(cable_);
  player.SetStopCallback([] { return true; });

  const SvfPlayResult result = player.Play(kQuartusOpeningSvf);

  EXPECT_FALSE(result.succeeded);
  EXPECT_TRUE(result.stopped);
  EXPECT_EQ(result.statements, 0u);
}

TEST_F(SvfPlayerTest, ACableThatStopsAnsweringStopsTheRun) {
  cable_.FailAfterShifts(1);

  const SvfPlayResult result = Play("STATE IDLE; SDR 4 TDI (0);");

  EXPECT_FALSE(result.succeeded);
  EXPECT_FALSE(result.stopped);
  EXPECT_NE(result.problem.find("cable"), std::string::npos) << result.problem;
}

// --- Without a cable at all ------------------------------------------------

// What the shell tool's dry run is: everything this application contributes,
// exercised against a cable that goes nowhere, so that a file can be checked
// on a machine with no hardware attached to it.
TEST(SvfPlayerNullCableTest, AFileCanBeCheckedWithNoCableAtAll) {
  NullJtagCable nothing;
  SvfPlayer player(nothing);

  const SvfPlayResult result = player.Play("STATE IDLE; RUNTEST 4096 TCK;");

  ASSERT_TRUE(result.succeeded) << result.problem;
  EXPECT_EQ(nothing.run_clocks(), 4096u);
  EXPECT_GT(nothing.shifted_bits(), 0u);
}

}  // namespace
}  // namespace ddd::capture
