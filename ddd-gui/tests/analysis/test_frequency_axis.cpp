/************************************************************************

    test_frequency_axis.cpp

    Where a frequency lands on the axis, and what a position on it means
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "frequency_axis.h"
#include "sample_format.h"
#include "spectrum_analyser.h"

namespace ddd::analysis {
namespace {

constexpr double kDefaultTopHz = 14'000'000.0;

FrequencyAxis LogAxis(double maximum_hz = kDefaultTopHz) {
  return FrequencyAxis(FrequencyScale::kLogarithmic, maximum_hz);
}

FrequencyAxis LinearAxis(double maximum_hz = kDefaultTopHz) {
  return FrequencyAxis(FrequencyScale::kLinear, maximum_hz);
}

TEST(FrequencyAxisTest, TheEndsOfTheAxisAreTheEndsOfTheRange) {
  const FrequencyAxis logarithmic = LogAxis();

  EXPECT_DOUBLE_EQ(logarithmic.FrequencyAt(0.0),
                   FrequencyAxis::kDefaultMinimumHz);
  EXPECT_DOUBLE_EQ(logarithmic.FrequencyAt(1.0), kDefaultTopHz);

  // A linear axis starts at DC whatever minimum it was handed: there is nothing
  // wrong with drawing zero on it, and starting anywhere else would hide the DC
  // offset that a display of a centred signal exists partly to show.
  const FrequencyAxis linear(FrequencyScale::kLinear, kDefaultTopHz,
                             FrequencyAxis::kDefaultMinimumHz);
  EXPECT_DOUBLE_EQ(linear.minimum_hz(), 0.0);
  EXPECT_DOUBLE_EQ(linear.FrequencyAt(0.0), 0.0);
  EXPECT_DOUBLE_EQ(linear.FrequencyAt(1.0), kDefaultTopHz);
}

TEST(FrequencyAxisTest, APositionAndAFrequencyAgreeInBothDirections) {
  // The property every cursor readout rests on. A mapping that was not its own
  // inverse would put the pointer's frequency a little away from the trace's,
  // and on a log axis the discrepancy varies across the width — so it would
  // never look like a bug, only like a measurement.
  for (const FrequencyAxis& axis : {LogAxis(), LinearAxis()}) {
    for (int step = 0; step <= 100; ++step) {
      const double proportion = static_cast<double>(step) / 100.0;
      const double frequency = axis.FrequencyAt(proportion);
      EXPECT_NEAR(axis.ProportionOf(frequency), proportion, 1e-12)
          << "at " << proportion;
    }
  }
}

TEST(FrequencyAxisTest, ADecadeIsTheSameWidthWhereverItFalls) {
  // What choosing a logarithmic axis buys, stated as the property that makes it
  // worth choosing: the EFM band gets as much room as the octave above the FM
  // carrier, instead of a seventh of it.
  const FrequencyAxis axis = LogAxis();

  const double low =
      axis.ProportionOf(1'000'000.0) - axis.ProportionOf(100'000.0);
  const double high =
      axis.ProportionOf(10'000'000.0) - axis.ProportionOf(1'000'000.0);

  EXPECT_NEAR(low, high, 1e-12);
}

TEST(FrequencyAxisTest, TheAudioCarriersAreSeparatedRatherThanCrowded) {
  // The concrete complaint against the linear axis. The PAL analogue audio
  // carriers sit at 683.6 and 1066.4 kHz; on a linear axis to 14 MHz they are
  // 2.7% of the width apart, which at any usable panel size is a handful of
  // pixels and reads as one feature.
  const double lower = 683'594.0;
  const double upper = 1'066'400.0;

  const double linear =
      LinearAxis().ProportionOf(upper) - LinearAxis().ProportionOf(lower);
  const double logarithmic =
      LogAxis().ProportionOf(upper) - LogAxis().ProportionOf(lower);

  EXPECT_LT(linear, 0.03);
  EXPECT_GT(logarithmic, 0.08);
}

TEST(FrequencyAxisTest, NothingIsDrawnOutsideThePlot) {
  const FrequencyAxis axis = LogAxis();

  // Below the bottom of a log axis, above the top of either, and the zero and
  // negative frequencies a cursor can produce at the very edge of a widget.
  EXPECT_DOUBLE_EQ(axis.ProportionOf(1'000.0), 0.0);
  EXPECT_DOUBLE_EQ(axis.ProportionOf(0.0), 0.0);
  EXPECT_DOUBLE_EQ(axis.ProportionOf(-5.0), 0.0);
  EXPECT_DOUBLE_EQ(axis.ProportionOf(100'000'000.0), 1.0);

  EXPECT_DOUBLE_EQ(LinearAxis().ProportionOf(-5.0), 0.0);
  EXPECT_DOUBLE_EQ(LinearAxis().ProportionOf(100'000'000.0), 1.0);

  // And a position off either end of the widget maps back into the range.
  EXPECT_DOUBLE_EQ(axis.FrequencyAt(-0.5), FrequencyAxis::kDefaultMinimumHz);
  EXPECT_DOUBLE_EQ(axis.FrequencyAt(1.5), kDefaultTopHz);
}

TEST(FrequencyAxisTest, TheLogarithmicTicksAreTheOnesAPersonWouldChoose) {
  const std::vector<double> ticks = LogAxis().Ticks();

  const std::vector<double> expected = {100'000.0,   200'000.0,   500'000.0,
                                        1'000'000.0, 2'000'000.0, 5'000'000.0,
                                        10'000'000.0};
  ASSERT_EQ(ticks.size(), expected.size());
  for (size_t index = 0; index < ticks.size(); ++index) {
    EXPECT_DOUBLE_EQ(ticks[index], expected[index]) << "tick " << index;
  }
}

TEST(FrequencyAxisTest, TheLinearTicksAreTheGridTheDisplayAlwaysHad) {
  const std::vector<double> ticks = LinearAxis().Ticks();

  ASSERT_EQ(ticks.size(), 8U);
  EXPECT_DOUBLE_EQ(ticks.front(), 0.0);
  EXPECT_DOUBLE_EQ(ticks.back(), kDefaultTopHz);
  for (size_t index = 1; index < ticks.size(); ++index) {
    EXPECT_DOUBLE_EQ(ticks[index] - ticks[index - 1],
                     FrequencyAxis::kLinearTickStepHz);
  }
}

TEST(FrequencyAxisTest, EveryTickIsInsideTheAxisItLabels) {
  // A tick drawn outside the plot is a label pointing at nothing, and the
  // wider ranges are where a ladder is most likely to run past its end.
  for (const double top : kMaximumFrequencyChoicesHz) {
    for (const FrequencyAxis& axis : {LogAxis(top), LinearAxis(top)}) {
      const std::vector<double> ticks = axis.Ticks();
      ASSERT_FALSE(ticks.empty());

      for (const double tick : ticks) {
        EXPECT_GE(tick, axis.minimum_hz()) << "top " << top;
        EXPECT_LE(tick, axis.maximum_hz()) << "top " << top;
      }

      // And in order, which is what lets a painter walk them once.
      for (size_t index = 1; index < ticks.size(); ++index) {
        EXPECT_GT(ticks[index], ticks[index - 1]);
      }
    }
  }
}

TEST(FrequencyAxisTest, AnAxisWithNoRangeIsCorrectedRatherThanLeftToDivide) {
  // These come from a widget mid-resize and from a settings file somebody has
  // edited. None of them should reach a painter as an infinity.
  const FrequencyAxis inverted(FrequencyScale::kLogarithmic, 1'000'000.0,
                               5'000'000.0);
  EXPECT_LT(inverted.minimum_hz(), inverted.maximum_hz());
  EXPECT_TRUE(std::isfinite(inverted.ProportionOf(2'000'000.0)));

  const FrequencyAxis zeroed(FrequencyScale::kLogarithmic, 0.0, 0.0);
  EXPECT_GT(zeroed.minimum_hz(), 0.0);
  EXPECT_GT(zeroed.maximum_hz(), zeroed.minimum_hz());
  EXPECT_TRUE(std::isfinite(zeroed.FrequencyAt(0.5)));

  const FrequencyAxis negative(FrequencyScale::kLinear, -1.0);
  EXPECT_GT(negative.maximum_hz(), 0.0);
  EXPECT_TRUE(std::isfinite(negative.ProportionOf(1.0)));
}

TEST(FrequencyAxisTest, TheDefaultAxisIsOneThatCanBeDrawn) {
  // A default-constructed plot maps before anybody has chosen a range.
  const FrequencyAxis axis;

  EXPECT_DOUBLE_EQ(axis.FrequencyAt(0.0), 0.0);
  EXPECT_GT(axis.maximum_hz(), 0.0);
  EXPECT_FALSE(axis.Ticks().empty());
}

TEST(FrequencyAxisTest, TheCarrierSitsWhereTheArithmeticSaysItDoes) {
  // A worked example, so that a change to the mapping has to be deliberate.
  // 8 MHz on a log axis from 100 kHz to 14 MHz: log(80) / log(140).
  const double expected = std::log(80.0) / std::log(140.0);

  EXPECT_NEAR(LogAxis().ProportionOf(8'000'000.0), expected, 1e-12);
  EXPECT_NEAR(LinearAxis().ProportionOf(8'000'000.0), 8.0 / 14.0, 1e-12);
}

}  // namespace
}  // namespace ddd::analysis
