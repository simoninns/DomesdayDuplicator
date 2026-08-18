/************************************************************************

    frequency_axis.h

    Where a frequency lands on the axis, and what a position on it means
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <vector>

namespace ddd::analysis {

// The one piece of arithmetic every frequency display shares.
//
// The spectrum trace, the spectrogram's vertical axis, both sets of gridlines
// and both cursor readouts all have to agree about where 8 MHz is. Kept in one
// place because the failure mode when they do not is silent: a cursor that
// disagrees with the trace by a few pixels still reads like a measurement, and
// on a logarithmic axis — where the error varies across the width — nobody
// notices by looking. Split out from the panel for the same reason
// waveform_mapping.h is: it can be asked directly whether the frequency under
// the pointer is the frequency drawn there, and no screenshot answers that.

enum class FrequencyScale {
  // Position proportional to frequency. The right view for the filter's corner
  // and for FM sideband symmetry, both of which are about equal spacing in Hz.
  kLinear,

  // Position proportional to the logarithm of frequency, which is the norm for
  // audio work and for monitoring a band as wide as this one.
  //
  // The content here runs from the EFM band at 200 kHz to the filter corner at
  // 13.2 MHz — nearly two decades. Spread linearly, everything below 2 MHz is
  // crushed into the leftmost seventh of the display while the octave of very
  // little between 10 and 20 MHz gets more room than the whole digital audio
  // band. Spread logarithmically, the EFM band, the analogue audio carriers and
  // the video carrier are each a legible region of the axis.
  kLogarithmic,
};

class FrequencyAxis {
 public:
  // The bottom of a logarithmic axis.
  //
  // Not zero, which has no logarithm, and not the lowest bin either. At the
  // default resolution the bins are 9.8 kHz wide, so below 100 kHz there are
  // fewer than ten of them in total — an expanse of axis with almost no
  // measurement behind it, and on a log scale it would be the widest part of
  // the display.
  static constexpr double kDefaultMinimumHz = 100'000.0;

  // The linear grid interval.
  //
  // 2 MHz gridlines put the LaserDisc FM carrier — around 8 MHz — between two
  // of them rather than on one, which is what makes a drift visible, and keep
  // the axis readable across every displayed range rather than only the widest.
  static constexpr double kLinearTickStepHz = 2'000'000.0;

  // A linear axis from DC to 20 MHz. A default that is a valid axis rather than
  // a degenerate one, so a default-constructed plot draws something sane before
  // anybody has chosen a range.
  FrequencyAxis();

  // maximum_hz is the top of the axis in both scales. minimum_hz is the bottom
  // of a logarithmic one and is ignored by a linear axis, which always starts
  // at DC — an axis that began at 100 kHz when nothing required it to would be
  // hiding the DC offset the waveform panel exists to show.
  //
  // Nonsense is corrected rather than refused: a maximum at or below the
  // minimum, or a non-positive minimum on a log axis, would otherwise produce a
  // mapping that returned infinities into a painter.
  FrequencyAxis(FrequencyScale scale, double maximum_hz,
                double minimum_hz = kDefaultMinimumHz);

  FrequencyScale scale() const { return scale_; }

  // The frequency at each end. The bottom is zero on a linear axis.
  double minimum_hz() const { return minimum_hz_; }
  double maximum_hz() const { return maximum_hz_; }

  // Where along the axis a frequency sits, from 0 at the bottom to 1 at the
  // top. Clamped, so a frequency off either end lands on that end rather than
  // being drawn outside the plot.
  double ProportionOf(double frequency_hz) const;

  // The inverse: the frequency at a position along the axis. Proportion is
  // clamped to [0, 1] for the same reason.
  //
  // Exactly the inverse of ProportionOf within the axis's own range, which is
  // the property the cursor readouts depend on and the tests check.
  double FrequencyAt(double proportion) const;

  // Where the gridlines and their labels go, bottom to top.
  //
  // Multiples of the step above on a linear axis. On a logarithmic one, the
  // 1-2-5 ladder that every log plot uses — 100 k, 200 k, 500 k, 1 M, 2 M and
  // so on — because a decade divided any other way produces labels nobody would
  // choose and gridlines that crowd at one end of each decade.
  std::vector<double> Ticks() const;

 private:
  FrequencyScale scale_ = FrequencyScale::kLinear;
  double minimum_hz_ = 0.0;
  double maximum_hz_ = 20'000'000.0;
};

}  // namespace ddd::analysis
