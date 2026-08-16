/************************************************************************

    sinc_interpolation.cpp

    What the signal did between the samples
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "sinc_interpolation.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ddd::analysis {
namespace {

// sin(pi x) / (pi x), under a Blackman window that reaches zero at the ends of
// the tap range.
double WindowedSinc(double x, double half_taps) {
  if (std::abs(x) > half_taps) {
    return 0.0;
  }

  const double window =
      0.42 + (0.5 * std::cos(std::numbers::pi * x / half_taps)) +
      (0.08 * std::cos(2.0 * std::numbers::pi * x / half_taps));

  // The limit at zero, where the quotient is 0/0 and the function is 1.
  if (std::abs(x) < 1e-12) {
    return window;
  }

  const double argument = std::numbers::pi * x;
  return window * std::sin(argument) / argument;
}

}  // namespace

ReconstructionKernel::ReconstructionKernel(int half_taps)
    : half_taps_(std::max(1, half_taps)) {
  const double reach = static_cast<double>(half_taps_);
  weights_.assign(kReconstructionPhases * static_cast<size_t>(taps()), 0.0);

  for (size_t phase = 0; phase < kReconstructionPhases; ++phase) {
    const double fraction =
        static_cast<double>(phase) / static_cast<double>(kReconstructionPhases);

    double* const row = &weights_[phase * static_cast<size_t>(taps())];

    double total = 0.0;
    for (int tap = 0; tap < taps(); ++tap) {
      // Taps run from half_taps - 1 samples before the position's whole part to
      // half_taps after it, so the position always sits inside the run rather
      // than at its edge.
      const double offset = static_cast<double>(tap - half_taps_ + 1);
      const double weight = WindowedSinc(fraction - offset, reach);
      row[tap] = weight;
      total += weight;
    }

    if (total != 0.0) {
      for (int tap = 0; tap < taps(); ++tap) {
        row[tap] /= total;
      }
    }
  }
}

double ReconstructionKernel::Evaluate(const uint16_t* codes, size_t count,
                                      double position) const {
  if (codes == nullptr || count == 0) {
    return 0.0;
  }
  if (count == 1) {
    return static_cast<double>(codes[0]);
  }

  const double last = static_cast<double>(count - 1);
  const double clamped = std::clamp(position, 0.0, last);
  const double base = std::floor(clamped);
  const double fraction = clamped - base;

  const size_t phase =
      std::min(kReconstructionPhases - 1,
               static_cast<size_t>(fraction *
                                   static_cast<double>(kReconstructionPhases)));

  const auto first = static_cast<int64_t>(base) - half_taps_ + 1;
  const double* const row = &weights_[phase * static_cast<size_t>(taps())];

  double total = 0.0;
  for (int tap = 0; tap < taps(); ++tap) {
    const int64_t index =
        std::clamp<int64_t>(first + tap, 0, static_cast<int64_t>(count) - 1);
    total += static_cast<double>(codes[static_cast<size_t>(index)]) * row[tap];
  }
  return total;
}

}  // namespace ddd::analysis
