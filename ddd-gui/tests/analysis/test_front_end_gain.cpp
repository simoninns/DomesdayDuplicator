/************************************************************************

    test_front_end_gain.cpp

    The board's RF gain switch, as declared by the user
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include "front_end_gain.h"

namespace ddd::analysis {
namespace {

// The fifteen switch patterns and what the board does at each, taken from
// hardware/doc/DdD Gain and filter calculations.xlsx, sheet "Gain Setting".
//
// Transcribed here and computed in the module, which is the whole value of the
// table: the implementation derives its answer from the four resistor values,
// so agreeing with these numbers means the resistors, the topology and the
// arithmetic are all right. A table copied into the implementation and then
// compared against itself would prove nothing.
struct GainCase {
  uint8_t pattern;
  const char* switches;
  double gain;
  double full_scale_millivolts;
};

constexpr GainCase kGainCases[] = {
    {0b1000, "1000", 8.50, 235.3}, {0b0100, "0100", 6.00, 333.3},
    {0b0010, "0010", 4.40, 454.5}, {0b1100, "1100", 4.00, 500.0},
    {0b0001, "0001", 3.80, 526.3}, {0b1010, "1010", 3.34, 598.9},
    {0b1001, "1001", 3.04, 658.1}, {0b0110, "0110", 3.02, 661.4},
    {0b0101, "0101", 2.79, 715.6}, {0b1110, "1110", 2.59, 771.1},
    {0b0011, "0011", 2.54, 788.8}, {0b1101, "1101", 2.45, 816.9},
    {0b1011, "1011", 2.27, 879.3}, {0b0111, "0111", 2.17, 919.7},
    {0b1111, "1111", 2.02, 992.2},
};

TEST(FrontEndGainTest, EverySwitchPatternMatchesTheBoardCalculations) {
  for (const GainCase& expected : kGainCases) {
    const FrontEndGain gain = FrontEndGain::FromSwitchPattern(expected.pattern);
    ASSERT_TRUE(gain.declared()) << "pattern " << int{expected.pattern};

    EXPECT_NEAR(gain.Gain(), expected.gain, 0.005)
        << "switches " << expected.switches;
    EXPECT_NEAR(gain.FullScaleInputMillivoltsPeakToPeak(),
                expected.full_scale_millivolts, 0.1)
        << "switches " << expected.switches;
  }
}

TEST(FrontEndGainTest, TheSwitchesAreNamedAsTheyAreOnTheBoard) {
  for (const GainCase& expected : kGainCases) {
    EXPECT_EQ(DescribeSwitchPattern(expected.pattern), expected.switches);
  }
}

TEST(FrontEndGainTest, EverySwitchGetsADigitWhetherItIsClosedOrNot) {
  // A DIP switch is read by looking at it: four positions, some up. Naming only
  // the closed ones makes a user count along the block to check, which is the
  // error this representation exists to prevent.
  for (const GainCase& expected : kGainCases) {
    EXPECT_EQ(DescribeSwitchPattern(expected.pattern).size(), kSwitchCount);
  }
}

TEST(FrontEndGainTest, ClosingASecondSwitchLowersTheGain) {
  // The result that looks like a bug until the resistors are seen to be in
  // parallel. Worth a test of its own: an implementation that added the
  // resistances instead would pass nothing above, but it would also be an easy
  // thing to "fix" later by somebody who expected more switches to mean more
  // gain.
  const FrontEndGain one = FrontEndGain::FromSwitchPattern(0b1000);
  const FrontEndGain one_and_two = FrontEndGain::FromSwitchPattern(0b1100);

  EXPECT_LT(one_and_two.Gain(), one.Gain());
}

TEST(FrontEndGainTest, AllSwitchesOpenIsNotADeclaration) {
  // Not an arbitrary choice of sentinel: with every switch open the amplifier
  // has no feedback path at all, so there is no gain for the value to mean.
  const FrontEndGain gain = FrontEndGain::FromSwitchPattern(0);

  EXPECT_FALSE(gain.declared());
  EXPECT_EQ(gain.switch_pattern(), kUndeclaredSwitchPattern);
  EXPECT_EQ(DescribeSwitchPattern(0), "");
}

TEST(FrontEndGainTest, ADefaultConstructedGainIsUndeclared) {
  const FrontEndGain gain;

  EXPECT_FALSE(gain.declared());
  EXPECT_DOUBLE_EQ(gain.Gain(), 0.0);
}

TEST(FrontEndGainTest, AnUndeclaredGainConvertsNothing) {
  // The rule that keeps a wrong voltage off the screen. Zero rather than a
  // pass-through of the code, so a caller that forgot to check declared()
  // produces an obviously broken figure rather than a plausible one.
  const FrontEndGain gain;

  EXPECT_DOUBLE_EQ(gain.MillivoltsPerCode(), 0.0);
  EXPECT_DOUBLE_EQ(gain.CodeToInputMillivolts(1023), 0.0);
  EXPECT_DOUBLE_EQ(gain.CodeSpanToInputMillivolts(512), 0.0);
  EXPECT_DOUBLE_EQ(gain.FullScaleInputMillivoltsPeakToPeak(), 0.0);
}

TEST(FrontEndGainTest, APatternOutsideTheRangeIsTreatedAsUndeclared) {
  // Settings files get edited by hand and written by other versions. The safe
  // reading of a value that cannot mean anything is that nothing was declared,
  // not that some part of it can be salvaged.
  EXPECT_FALSE(FrontEndGain::FromSwitchPattern(16).declared());
  EXPECT_FALSE(FrontEndGain::FromSwitchPattern(255).declared());
}

TEST(FrontEndGainTest, MidScaleIsZeroVoltsAndTheExtremesAreSymmetrical) {
  const FrontEndGain gain = FrontEndGain::FromSwitchPattern(0b0100);  // 6.00

  EXPECT_DOUBLE_EQ(gain.CodeToInputMillivolts(kAdcMidScaleCode), 0.0);

  const double low = gain.CodeToInputMillivolts(0);
  const double high = gain.CodeToInputMillivolts(1023);

  EXPECT_LT(low, 0.0);
  EXPECT_GT(high, 0.0);

  // Not exactly symmetrical, and deliberately not fudged to be: mid-scale is
  // code 512 of 0..1023, so there is one more code below it than above.
  EXPECT_NEAR(-low, high + gain.MillivoltsPerCode(), 1e-9);
}

TEST(FrontEndGainTest, AFullScaleSpanIsTheBoardsFullScaleInput) {
  for (const GainCase& expected : kGainCases) {
    const FrontEndGain gain = FrontEndGain::FromSwitchPattern(expected.pattern);

    // 1024 codes of span is the whole converter, so it has to come back as the
    // whole input range the board can take. The two are computed by different
    // routes in the module, and this is what ties them together.
    EXPECT_NEAR(gain.CodeSpanToInputMillivolts(kAdcCodeCount),
                gain.FullScaleInputMillivoltsPeakToPeak(), 1e-9)
        << "switches " << expected.switches;
  }
}

TEST(FrontEndGainTest, TheHighestGainTakesTheSmallestInput) {
  // The relationship a user relies on when choosing a setting, stated as a
  // property rather than left implicit in the table.
  const FrontEndGain highest = FrontEndGain::FromSwitchPattern(0b1000);
  const FrontEndGain lowest = FrontEndGain::FromSwitchPattern(0b1111);

  EXPECT_GT(highest.Gain(), lowest.Gain());
  EXPECT_LT(highest.FullScaleInputMillivoltsPeakToPeak(),
            lowest.FullScaleInputMillivoltsPeakToPeak());
}

TEST(FrontEndGainTest, TwoDeclarationsOfTheSameSwitchesAreEqual) {
  EXPECT_EQ(FrontEndGain::FromSwitchPattern(0b1010),
            FrontEndGain::FromSwitchPattern(0b1010));
  EXPECT_NE(FrontEndGain::FromSwitchPattern(0b1010),
            FrontEndGain::FromSwitchPattern(0b0101));
  EXPECT_NE(FrontEndGain::FromSwitchPattern(0b1010), FrontEndGain());
}

}  // namespace
}  // namespace ddd::analysis
