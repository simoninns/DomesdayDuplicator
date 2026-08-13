/************************************************************************

    spectrum_analyser.cpp

    Windowed power spectrum, averaged and peak-held
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "spectrum_analyser.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "fourier_transform.h"
#include "front_end_gain.h"

namespace ddd::analysis {
namespace {

// The largest amplitude a 10-bit converter can represent about mid-scale, and
// so the amplitude of the sine that reads 0 dB.
constexpr double kFullScaleAmplitudeCodes = kAdcMidScaleCode;

constexpr double kMinimumTransformSize = 8.0;

double PowerToDecibels(double power) {
  if (power <= 0.0) {
    return SpectrumAnalyser::kFloorDecibels;
  }
  return std::max(SpectrumAnalyser::kFloorDecibels, 10.0 * std::log10(power));
}

}  // namespace

SpectrumAnalyser::SpectrumAnalyser() : SpectrumAnalyser(Options()) {}

SpectrumAnalyser::SpectrumAnalyser(const Options& options) : options_(options) {
  if (!IsPowerOfTwo(options_.transform_size) ||
      static_cast<double>(options_.transform_size) < kMinimumTransformSize) {
    options_.transform_size = kDefaultTransformSize;
  }
  options_.averaging = std::clamp(options_.averaging, 0.0, 0.99);

  BuildWindow();

  const size_t bins = (options_.transform_size / 2) + 1;
  real_.assign(options_.transform_size, 0.0);
  imaginary_.assign(options_.transform_size, 0.0);
  average_power_.assign(bins, 0.0);
  magnitudes_db_.assign(bins, kFloorDecibels);
  peak_hold_db_.assign(bins, kFloorDecibels);
}

void SpectrumAnalyser::BuildWindow() {
  window_.assign(options_.transform_size, 0.0);
  window_sum_ = 0.0;

  const double denominator = static_cast<double>(options_.transform_size);
  for (size_t index = 0; index < options_.transform_size; ++index) {
    // Hann, in its periodic form — divided by N rather than by N - 1.
    //
    // Chosen over a rectangular window because the interesting thing on this
    // display is a carrier next to a noise floor, and rectangular leakage
    // buries the second in the skirts of the first. Periodic rather than
    // symmetric because this window is only ever used inside a DFT, and the
    // periodic form is the one whose transform is exactly zero away from the
    // three bins around a tone. The symmetric form is a length N - 1 window
    // with a zero on the end, and the resulting off-grid sampling puts a
    // -54 dB skirt under everything — visible on a display whose whole purpose
    // is finding small signals beside large ones.
    const double value =
        0.5 * (1.0 - std::cos(2.0 * std::numbers::pi *
                              static_cast<double>(index) / denominator));
    window_[index] = value;
    window_sum_ += value;
  }
}

bool SpectrumAnalyser::Analyse(const uint16_t* codes, size_t count) {
  if (codes == nullptr || count < options_.transform_size ||
      window_sum_ <= 0.0) {
    return false;
  }

  for (size_t index = 0; index < options_.transform_size; ++index) {
    const double centred = static_cast<double>(codes[index]) - kAdcMidScaleCode;
    real_[index] = centred * window_[index];
    imaginary_[index] = 0.0;
  }

  if (!ForwardTransform(real_, imaginary_)) {
    return false;
  }

  const size_t bins = average_power_.size();
  const double scale = window_sum_ * kFullScaleAmplitudeCodes;

  for (size_t bin = 0; bin < bins; ++bin) {
    // Every bin but DC and Nyquist has a mirror image in the half of the
    // transform not being shown, and the signal's energy is split between the
    // two. Doubling here is what puts it back, and skipping those two is what
    // keeps them from being reported 6 dB high.
    const double mirror =
        (bin == 0 || bin == options_.transform_size / 2) ? 1.0 : 2.0;

    const double magnitude =
        std::hypot(real_[bin], imaginary_[bin]) * mirror / scale;
    const double power = magnitude * magnitude;

    if (!have_average_ || options_.averaging <= 0.0) {
      average_power_[bin] = power;
    } else {
      average_power_[bin] = options_.averaging * average_power_[bin] +
                            (1.0 - options_.averaging) * power;
    }

    magnitudes_db_[bin] = PowerToDecibels(average_power_[bin]);
    peak_hold_db_[bin] = std::max(peak_hold_db_[bin], magnitudes_db_[bin]);
  }

  have_average_ = true;
  return true;
}

void SpectrumAnalyser::ResetPeakHold() {
  std::fill(peak_hold_db_.begin(), peak_hold_db_.end(), kFloorDecibels);
}

void SpectrumAnalyser::Reset() {
  have_average_ = false;
  std::fill(average_power_.begin(), average_power_.end(), 0.0);
  std::fill(magnitudes_db_.begin(), magnitudes_db_.end(), kFloorDecibels);
  ResetPeakHold();
}

double SpectrumAnalyser::BinFrequencyHz(size_t bin, size_t transform_size,
                                        uint32_t sample_rate_hz) {
  if (transform_size == 0) {
    return 0.0;
  }
  return static_cast<double>(bin) * static_cast<double>(sample_rate_hz) /
         static_cast<double>(transform_size);
}

size_t SpectrumAnalyser::FrequencyToBin(double frequency_hz,
                                        size_t transform_size,
                                        uint32_t sample_rate_hz) {
  if (transform_size == 0 || sample_rate_hz == 0 || frequency_hz <= 0.0) {
    return 0;
  }

  const double bin = frequency_hz * static_cast<double>(transform_size) /
                     static_cast<double>(sample_rate_hz);
  const size_t rounded = static_cast<size_t>(std::lround(bin));
  return std::min(rounded, transform_size / 2);
}

}  // namespace ddd::analysis
