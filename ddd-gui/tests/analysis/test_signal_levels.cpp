/************************************************************************

    test_signal_levels.cpp

    The level a capture is supposed to sit at
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include "front_end_gain.h"
#include "signal_levels.h"

namespace ddd::analysis {
namespace {

TEST(SignalLevelsTest, TheNominalBoundsAreThreeQuartersOfEachSide) {
  // 75% of the 512 codes available either side of mid-scale.
  EXPECT_DOUBLE_EQ(NominalPeakExcursionCodes(), 384.0);
  EXPECT_DOUBLE_EQ(NominalUpperCode(), 896.0);
  EXPECT_DOUBLE_EQ(NominalLowerCode(), 128.0);
}

TEST(SignalLevelsTest, TheBoundsAreSymmetricalAboutMidScale) {
  // The property the display depends on, stated rather than left to be read out
  // of the two numbers above: the signal swings both ways about 0 V, so a bound
  // that applied to one side only would say nothing about the half of the
  // waveform that was already closer to the rail.
  EXPECT_DOUBLE_EQ(NominalUpperCode() - kAdcMidScaleCode,
                   kAdcMidScaleCode - NominalLowerCode());
}

TEST(SignalLevelsTest, TheBoundsLeaveHeadroomBeforeTheConverterClips) {
  // A nominal level, not a limit. The headroom between it and the converter is
  // what absorbs the moments a disc is worse than the moment the gain was set
  // on.
  EXPECT_GT(NominalLowerCode(), 0.0);
  EXPECT_LT(NominalUpperCode(), 1023.0);
}

TEST(SignalLevelsTest, ARangeInsideBothBoundsIsNominal) {
  EXPECT_TRUE(WithinNominalLevel(128, 896));
  EXPECT_TRUE(WithinNominalLevel(400, 600));
}

TEST(SignalLevelsTest, EitherSideAloneIsEnoughToLeaveNominal) {
  // Both ends have to be inside. A signal riding high is out of nominal even if
  // its troughs are comfortable, and the other way round.
  EXPECT_FALSE(WithinNominalLevel(400, 900));
  EXPECT_FALSE(WithinNominalLevel(100, 600));
  EXPECT_FALSE(WithinNominalLevel(0, 1023));
}

TEST(SignalLevelsTest, TheNominalInputIsThreeQuartersOfTheBoardsFullScale) {
  // Switches 2: gain 6.00, full scale 333 mV p-p, so nominal is 250.
  const FrontEndGain gain = FrontEndGain::FromSwitchPattern(0b0100);

  EXPECT_NEAR(
      NominalInputMillivoltsPeakToPeak(gain),
      gain.FullScaleInputMillivoltsPeakToPeak() * kNominalPeakProportion, 1e-9);
  EXPECT_NEAR(NominalInputMillivoltsPeakToPeak(gain), 250.0, 0.5);
}

TEST(SignalLevelsTest, WithNoDeclaredGainThereIsNoNominalVoltage) {
  // Same rule as everywhere else: no declaration, no figure in volts.
  EXPECT_DOUBLE_EQ(NominalInputMillivoltsPeakToPeak(FrontEndGain()), 0.0);
}

}  // namespace
}  // namespace ddd::analysis
