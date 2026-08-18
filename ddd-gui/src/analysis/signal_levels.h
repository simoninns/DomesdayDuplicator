/************************************************************************

    signal_levels.h

    The level a capture is supposed to sit at
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>

#include "front_end_gain.h"

namespace ddd::analysis {

// What a well-adjusted capture looks like, as a number the displays can draw.
//
// The recommendation is that the signal peaks at no more than 75% of the
// converter's range. It is a nominal level rather than a limit: the hard limit
// is the converter itself, which clips at 0 and 1023 and is reported
// separately, and the headroom between the two is what absorbs the moments a
// disc is worse than the moment the gain was set on. A capture that spends its
// time at 95% is not yet clipping and is one dropout away from it.
//
// The 75% applies to the excursion either side of mid-scale, not to the code
// range as a whole. The signal is centred: it swings both ways about 0 V, and a
// bound drawn on one side only would say nothing about the half of the waveform
// that was already closer to the rail.
inline constexpr double kNominalPeakProportion = 0.75;

// The largest excursion from mid-scale that counts as nominal, in codes. 384 of
// the 512 available.
inline constexpr double NominalPeakExcursionCodes() {
  return kNominalPeakProportion * kAdcMidScaleCode;
}

// The two codes the nominal bounds fall on: 896 and 128, symmetrical about
// mid-scale.
inline constexpr double NominalUpperCode() {
  return kAdcMidScaleCode + NominalPeakExcursionCodes();
}

inline constexpr double NominalLowerCode() {
  return kAdcMidScaleCode - NominalPeakExcursionCodes();
}

// Whether a measured range sits inside the nominal bounds. Both ends have to,
// for the reason above.
inline constexpr bool WithinNominalLevel(uint16_t minimum_code,
                                         uint16_t maximum_code) {
  return static_cast<double>(minimum_code) >= NominalLowerCode() &&
         static_cast<double>(maximum_code) <= NominalUpperCode();
}

// The nominal peak-to-peak input the board accepts at a declared gain — the
// figure to set a source against. Zero when nothing has been declared, in
// keeping with every other conversion in front_end_gain.h.
inline double NominalInputMillivoltsPeakToPeak(const FrontEndGain& gain) {
  return gain.CodeSpanToInputMillivolts(2.0 * NominalPeakExcursionCodes());
}

}  // namespace ddd::analysis
