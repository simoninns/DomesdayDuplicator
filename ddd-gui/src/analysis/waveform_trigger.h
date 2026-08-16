/************************************************************************

    waveform_trigger.h

    Holding a repeating waveform still, which is what makes it a scope
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "front_end_gain.h"

namespace ddd::analysis {

// Why a plot of some samples is not a scope.
//
// Snapshots arrive about nine times a second and each one starts wherever the
// USB transfer happened to start, which bears no relation to the signal in it.
// Drawn from its first sample, a 8 MHz carrier is a different slice of a cycle
// every frame: the trace shimmers at nine hertz and reads as a band of fuzz.
// Every oscilloscope since the 1940s solves this the same way — start each
// sweep at the same point on the waveform — and this is that.
//
// The crossing is located between samples rather than at one. At 40 Msps an
// 8 MHz carrier is five samples a cycle, so triggering to the nearest sample
// would still leave the picture jittering by up to a fifth of a cycle, which is
// most of the shimmer it was supposed to remove. Interpolating the crossing
// costs one divide and is the difference between a trace that stands still and
// one that nearly does.

// How far below the level the signal must go before another rising crossing
// counts.
//
// Without it, noise sitting on the trigger level fires a trigger per sample and
// the sweeps are aligned to the noise rather than to the waveform. Sixteen
// codes is comfortably above a 10-bit converter's own noise and far below any
// signal worth looking at, which is the window this needs to sit in.
inline constexpr double kDefaultTriggerHysteresisCodes = 16.0;

struct TriggerOptions {
  // Mid-scale by default, which for a signal that swings about 0 V is the point
  // it crosses most steeply — and the steeper the crossing the less a given
  // amount of noise moves it.
  double level_codes = kAdcMidScaleCode;

  double hysteresis_codes = kDefaultTriggerHysteresisCodes;

  // Samples to pass before another trigger may be taken.
  //
  // A snapshot holds 819 µs and a carrier crosses the level every 60 ns, so a
  // display wanting thirty sweeps would otherwise take them all from the first
  // two microseconds. Spreading them across the whole snapshot is what makes
  // the accumulated picture show the FM deviation over that window rather than
  // one arbitrary instant of it.
  size_t minimum_separation = 1;
};

// Rising crossings of the trigger level, as fractional sample positions.
//
// Stops once `maximum` have been found, which is what keeps this from scanning
// 32,768 samples when the display wants thirty sweeps. Positions are appended
// in order; the vector is cleared first.
void FindTriggers(const uint16_t* codes, size_t count,
                  const TriggerOptions& options, size_t maximum,
                  std::vector<double>& positions);

}  // namespace ddd::analysis
