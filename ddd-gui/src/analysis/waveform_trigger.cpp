/************************************************************************

    waveform_trigger.cpp

    Holding a repeating waveform still, which is what makes it a scope
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "waveform_trigger.h"

#include <algorithm>

namespace ddd::analysis {

void FindTriggers(const uint16_t* codes, size_t count,
                  const TriggerOptions& options, size_t maximum,
                  std::vector<double>& positions) {
  positions.clear();

  if (codes == nullptr || count < 2 || maximum == 0) {
    return;
  }

  const double level = options.level_codes;
  const double arm_level = level - std::max(0.0, options.hysteresis_codes);
  const size_t separation = std::max<size_t>(options.minimum_separation, 1);

  // Armed means the signal has been below the arming level since the last
  // trigger, and so the next upward crossing is a real edge rather than noise
  // rattling around the level.
  bool armed = false;
  size_t next_allowed = 0;

  for (size_t index = 1; index < count; ++index) {
    const double previous = static_cast<double>(codes[index - 1]);
    const double current = static_cast<double>(codes[index]);

    if (!armed) {
      if (index >= next_allowed && current < arm_level) {
        armed = true;
      }
      continue;
    }

    if (previous >= level || current < level) {
      continue;
    }

    // Where between the two samples the level was actually crossed. The
    // denominator cannot be zero: the tests above put one sample strictly below
    // the level and the other at or above it.
    const double fraction = (level - previous) / (current - previous);
    positions.push_back(static_cast<double>(index - 1) + fraction);

    armed = false;
    next_allowed = index + separation;

    if (positions.size() >= maximum) {
      return;
    }
  }
}

}  // namespace ddd::analysis
