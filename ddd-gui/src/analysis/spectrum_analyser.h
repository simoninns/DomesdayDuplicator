/************************************************************************

    spectrum_analyser.h

    Windowed power spectrum, averaged and peak-held
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ddd::analysis {

// What the signal is made of, which is the question the scope cannot answer.
//
// A LaserDisc RF signal is an FM carrier around 8 MHz with sidebands, plus the
// audio carriers below it. On the scope that is a fuzzy band; here it is a peak
// with a shape, and a player that has drifted or an interfering source that has
// appeared is visible immediately.
//
// Scaled so that 0 dB is a full-scale sine wave. That reference is chosen
// because it is the one a user can act on: a carrier at -6 dB is using half the
// converter's range, and the number says so without anybody having to know how
// many codes wide the input was or how the window was normalised.

// Named at namespace scope rather than left as member initialisers alone,
// because a nested type's defaults cannot be reached from a default argument of
// the enclosing class without tripping over how the two are parsed.
inline constexpr size_t kDefaultTransformSize = 4096;
inline constexpr double kDefaultAveraging = 0.6;

// The top of the displayed frequency range, as offered to the user.
//
// The converter reaches 20 MHz and the board's anti-aliasing filter rolls off
// at 13.2 MHz, so everything above that is the filter's skirt and the noise
// under it. Showing all 20 MHz by default would spend a third of the display on
// it; 14 MHz puts the filter's corner just inside the right-hand edge, where it
// can be seen to be working without crowding out the 8 MHz carrier that
// matters. The wider ranges are kept because "is the filter doing what I think"
// is a real question, and a display that cannot show past the corner cannot
// answer it.
inline constexpr double kLowPassCornerHz = 13'200'000.0;

inline constexpr double kMaximumFrequencyChoicesHz[] = {
    14'000'000.0, 16'000'000.0, 18'000'000.0, 20'000'000.0};

inline constexpr size_t kMaximumFrequencyChoiceCount =
    sizeof(kMaximumFrequencyChoicesHz) / sizeof(kMaximumFrequencyChoicesHz[0]);

inline constexpr double kDefaultMaximumFrequencyHz =
    kMaximumFrequencyChoicesHz[0];

class SpectrumAnalyser {
 public:
  struct Options {
    // 4,096 points at 40 Msps is a 9.8 kHz bin, and 2,048 bins across the
    // 20 MHz the converter can represent — more than any panel has pixels for,
    // so the display decimates rather than interpolating.
    size_t transform_size = kDefaultTransformSize;

    // Exponential averaging, 0 to just under 1: 0 shows each transform on its
    // own, higher values hold the display still enough to read. Averaging is
    // done on power rather than on decibels, so a peak that appears in one
    // frame out of ten reads as a tenth of its power and not as a tenth of its
    // level.
    double averaging = kDefaultAveraging;
  };

  // The floor everything is clamped to. A bin with no signal in it is
  // mathematically minus infinity, and a display asked to draw that has no
  // bottom.
  static constexpr double kFloorDecibels = -120.0;

  SpectrumAnalyser();
  explicit SpectrumAnalyser(const Options& options);

  // Feed converter codes. Uses the first transform_size of them and ignores the
  // rest; returns false without changing anything if there are fewer than that,
  // which is how a short snapshot is refused rather than zero-padded into a
  // spectrum that was never measured.
  bool Analyse(const uint16_t* codes, size_t count);

  // Levels in dB relative to a full-scale sine, one per bin from DC upwards.
  const std::vector<double>& magnitudes_db() const { return magnitudes_db_; }

  // The highest level each bin has reached since the peak hold was last reset.
  const std::vector<double>& peak_hold_db() const { return peak_hold_db_; }

  // transform_size / 2 + 1 — the bins a real input produces, DC to Nyquist.
  size_t bin_count() const { return magnitudes_db_.size(); }

  size_t transform_size() const { return options_.transform_size; }

  void ResetPeakHold();

  // Forget the average and the peak hold, for the start of a new run.
  void Reset();

  // The frequency a bin is centred on.
  static double BinFrequencyHz(size_t bin, size_t transform_size,
                               uint32_t sample_rate_hz);

  // The bin nearest a frequency, for a cursor readout.
  static size_t FrequencyToBin(double frequency_hz, size_t transform_size,
                               uint32_t sample_rate_hz);

 private:
  void BuildWindow();

  Options options_;

  // Hann, and its sum. The sum is the coherent gain the window costs, and
  // dividing by it is what makes a full-scale sine read 0 dB whichever window
  // is in use.
  std::vector<double> window_;
  double window_sum_ = 0.0;

  std::vector<double> real_;
  std::vector<double> imaginary_;

  // Averaged power per bin, in linear units. Kept alongside the decibel figures
  // rather than derived from them for the reason in Options::averaging.
  std::vector<double> average_power_;
  bool have_average_ = false;

  std::vector<double> magnitudes_db_;
  std::vector<double> peak_hold_db_;
};

}  // namespace ddd::analysis
