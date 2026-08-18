/************************************************************************

    frequency_axis.cpp

    Where a frequency lands on the axis, and what a position on it means
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "frequency_axis.h"

#include <algorithm>
#include <cmath>

namespace ddd::analysis {
namespace {

// The multiples that make a readable decade.
constexpr double kLadder[] = {1.0, 2.0, 5.0};

}  // namespace

FrequencyAxis::FrequencyAxis() = default;

FrequencyAxis::FrequencyAxis(FrequencyScale scale, double maximum_hz,
                             double minimum_hz)
    : scale_(scale) {
  maximum_hz_ = maximum_hz > 0.0 ? maximum_hz : 1.0;

  if (scale_ != FrequencyScale::kLogarithmic) {
    minimum_hz_ = 0.0;
    return;
  }

  minimum_hz_ = minimum_hz > 0.0 ? minimum_hz : kDefaultMinimumHz;

  // A decade of range, if the caller has asked for an axis whose ends are the
  // wrong way round or the same. Any answer here is arbitrary; this one is at
  // least an axis, which is what the painter above needs.
  if (minimum_hz_ >= maximum_hz_) {
    minimum_hz_ = maximum_hz_ / 10.0;
  }
}

double FrequencyAxis::ProportionOf(double frequency_hz) const {
  if (maximum_hz_ <= minimum_hz_) {
    return 0.0;
  }

  if (scale_ == FrequencyScale::kLogarithmic) {
    if (frequency_hz <= minimum_hz_) {
      return 0.0;
    }
    const double span = std::log(maximum_hz_ / minimum_hz_);
    return std::clamp(std::log(frequency_hz / minimum_hz_) / span, 0.0, 1.0);
  }

  return std::clamp((frequency_hz - minimum_hz_) / (maximum_hz_ - minimum_hz_),
                    0.0, 1.0);
}

double FrequencyAxis::FrequencyAt(double proportion) const {
  const double along = std::clamp(proportion, 0.0, 1.0);

  if (scale_ == FrequencyScale::kLogarithmic) {
    return minimum_hz_ * std::pow(maximum_hz_ / minimum_hz_, along);
  }

  return minimum_hz_ + (along * (maximum_hz_ - minimum_hz_));
}

std::vector<double> FrequencyAxis::Ticks() const {
  std::vector<double> ticks;

  if (scale_ != FrequencyScale::kLogarithmic) {
    // Counted rather than accumulated: a double advanced by repeated addition
    // drifts, and the last line of a long axis lands a little off where the
    // arithmetic says it should.
    for (int step = 0;
         static_cast<double>(step) * kLinearTickStepHz <= maximum_hz_; ++step) {
      ticks.push_back(static_cast<double>(step) * kLinearTickStepHz);
    }
    return ticks;
  }

  const int first = static_cast<int>(std::floor(std::log10(minimum_hz_)));
  const int last = static_cast<int>(std::ceil(std::log10(maximum_hz_)));

  for (int exponent = first; exponent <= last; ++exponent) {
    const double decade = std::pow(10.0, exponent);
    for (const double multiple : kLadder) {
      const double tick = decade * multiple;
      if (tick >= minimum_hz_ && tick <= maximum_hz_) {
        ticks.push_back(tick);
      }
    }
  }

  return ticks;
}

}  // namespace ddd::analysis
