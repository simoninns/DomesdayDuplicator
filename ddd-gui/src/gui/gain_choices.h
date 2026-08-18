/************************************************************************

    gain_choices.h

    The front-end gain settings a user can pick from
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <cstdint>
#include <vector>

namespace ddd::gui {

// One entry in the front-end gain list.
struct GainChoice {
  uint8_t switch_pattern = 0;
  QString label;
};

// Every switch pattern the board has, ordered by gain from highest to lowest,
// each labelled with the switches to close and the input it accepts.
//
// Ordered by gain rather than by the pattern's numeric value because the
// numeric order is meaningless to a user: closing switch 3 and 4 together gives
// less gain than either alone, so a list in pattern order jumps up and down.
// Ordered by gain, the list reads as a range, and the two ways of arriving at
// it — "which switches am I set to" and "how much gain do I want" — both work.
//
// The labels carry the full-scale input as well as the gain, because that is
// the figure that says whether a setting is right for a given player: a source
// putting out 500 mV p-p into a setting that accepts 235 clips, and the number
// says so without any arithmetic.
std::vector<GainChoice> FrontEndGainChoices();

// How the declared gain is put to a user in a readout — "switches 1, 3 (×3.34,
// 599 mV p-p full scale)" — or a sentence saying nothing has been declared.
QString DescribeFrontEndGain(uint8_t switch_pattern);

}  // namespace ddd::gui
