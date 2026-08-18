/************************************************************************

    front_end_gain.cpp

    The board's RF gain switch, as declared by the user
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "front_end_gain.h"

namespace ddd::analysis {

FrontEndGain FrontEndGain::FromSwitchPattern(uint8_t pattern) {
  FrontEndGain gain;
  if (pattern <= kMaximumSwitchPattern) {
    gain.pattern_ = pattern;
  }
  return gain;
}

double FrontEndGain::FeedbackResistanceOhms() const {
  if (!declared()) {
    return 0.0;
  }

  // Conductances add, which is the whole reason closing a second switch lowers
  // the gain rather than raising it — a result that looks wrong until the
  // resistors are seen to be in parallel rather than in series.
  double conductance = 0.0;
  for (size_t index = 0; index < kSwitchCount; ++index) {
    const unsigned bit = 1U << (kSwitchCount - 1 - index);
    if ((pattern_ & bit) != 0) {
      conductance += 1.0 / kSwitchResistanceOhms[index];
    }
  }

  return 1.0 / conductance;
}

double FrontEndGain::Gain() const {
  if (!declared()) {
    return 0.0;
  }
  return 1.0 + (FeedbackResistanceOhms() / kGainResistanceOhms);
}

double FrontEndGain::MillivoltsPerCode() const {
  const double gain = Gain();
  if (gain <= 0.0) {
    return 0.0;
  }
  return kAdcFullScaleMillivoltsPeakToPeak /
         static_cast<double>(kAdcCodeCount) / gain;
}

double FrontEndGain::CodeToInputMillivolts(double code) const {
  return (code - kAdcMidScaleCode) * MillivoltsPerCode();
}

double FrontEndGain::CodeSpanToInputMillivolts(double code_span) const {
  return code_span * MillivoltsPerCode();
}

double FrontEndGain::FullScaleInputMillivoltsPeakToPeak() const {
  const double gain = Gain();
  if (gain <= 0.0) {
    return 0.0;
  }
  return kAdcFullScaleMillivoltsPeakToPeak / gain;
}

std::string DescribeSwitchPattern(uint8_t pattern) {
  if (pattern == kUndeclaredSwitchPattern || pattern > kMaximumSwitchPattern) {
    return std::string();
  }

  std::string description;
  description.reserve(kSwitchCount);

  for (size_t index = 0; index < kSwitchCount; ++index) {
    const unsigned bit = 1U << (kSwitchCount - 1 - index);
    description += ((pattern & bit) != 0) ? '1' : '0';
  }

  return description;
}

}  // namespace ddd::analysis
