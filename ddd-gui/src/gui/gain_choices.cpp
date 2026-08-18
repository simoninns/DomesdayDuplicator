/************************************************************************

    gain_choices.cpp

    The front-end gain settings a user can pick from
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "gain_choices.h"

#include <QCoreApplication>
#include <algorithm>

#include "front_end_gain.h"

namespace ddd::gui {
namespace {

QString SwitchesText(uint8_t pattern) {
  return QString::fromStdString(analysis::DescribeSwitchPattern(pattern));
}

}  // namespace

std::vector<GainChoice> FrontEndGainChoices() {
  std::vector<GainChoice> choices;
  choices.reserve(analysis::kMaximumSwitchPattern);

  for (uint8_t pattern = 1; pattern <= analysis::kMaximumSwitchPattern;
       ++pattern) {
    const analysis::FrontEndGain gain =
        analysis::FrontEndGain::FromSwitchPattern(pattern);

    GainChoice choice;
    choice.switch_pattern = pattern;
    choice.label =
        QCoreApplication::translate("GainChoices",
                                    "Switches %1  —  ×%2, up to %3 mV p-p")
            .arg(SwitchesText(pattern))
            .arg(gain.Gain(), 0, 'f', 2)
            .arg(gain.FullScaleInputMillivoltsPeakToPeak(), 0, 'f', 0);
    choices.push_back(choice);
  }

  std::sort(
      choices.begin(), choices.end(),
      [](const GainChoice& first, const GainChoice& second) {
        return analysis::FrontEndGain::FromSwitchPattern(first.switch_pattern)
                   .Gain() >
               analysis::FrontEndGain::FromSwitchPattern(second.switch_pattern)
                   .Gain();
      });

  return choices;
}

QString DescribeFrontEndGain(uint8_t switch_pattern) {
  const analysis::FrontEndGain gain =
      analysis::FrontEndGain::FromSwitchPattern(switch_pattern);

  if (!gain.declared()) {
    // Said as a sentence rather than shown as a dash, because a dash reads as
    // "nothing measured yet" and this is "nobody has told me". The difference
    // is the whole reason no voltage is on screen, so it is worth the words.
    return QCoreApplication::translate("GainChoices",
                                       "Not declared — levels shown in "
                                       "converter codes");
  }

  return QCoreApplication::translate("GainChoices",
                                     "Switches %1  (×%2, %3 mV p-p full scale)")
      .arg(SwitchesText(switch_pattern))
      .arg(gain.Gain(), 0, 'f', 2)
      .arg(gain.FullScaleInputMillivoltsPeakToPeak(), 0, 'f', 0);
}

}  // namespace ddd::gui
