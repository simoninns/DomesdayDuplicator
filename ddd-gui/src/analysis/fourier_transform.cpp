/************************************************************************

    fourier_transform.cpp

    A radix-2 FFT, written here rather than vendored
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "fourier_transform.h"

#include <cmath>
#include <numbers>
#include <utility>

namespace ddd::analysis {

bool IsPowerOfTwo(size_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

bool ForwardTransform(std::vector<double>& real,
                      std::vector<double>& imaginary) {
  const size_t count = real.size();
  if (count != imaginary.size() || !IsPowerOfTwo(count)) {
    return false;
  }
  if (count == 1) {
    return true;
  }

  // Bit-reversal permutation, computed by incrementing a reversed counter
  // rather than reversing each index: the carry propagates from the top bit
  // down, which is the same addition done backwards.
  for (size_t i = 1, j = 0; i < count; ++i) {
    size_t bit = count >> 1;
    for (; (j & bit) != 0; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;

    if (i < j) {
      std::swap(real[i], real[j]);
      std::swap(imaginary[i], imaginary[j]);
    }
  }

  for (size_t length = 2; length <= count; length <<= 1) {
    const double angle = -2.0 * std::numbers::pi / static_cast<double>(length);
    const double step_real = std::cos(angle);
    const double step_imaginary = std::sin(angle);

    for (size_t start = 0; start < count; start += length) {
      // The twiddle factor is advanced by repeated multiplication rather than
      // recomputed with cos/sin per butterfly. At 4,096 points the drift is
      // around 1e-13, far below the noise floor of a 10-bit converter, and it
      // is the difference between two transcendental calls per butterfly and
      // none.
      double twiddle_real = 1.0;
      double twiddle_imaginary = 0.0;

      for (size_t offset = 0; offset < length / 2; ++offset) {
        const size_t even = start + offset;
        const size_t odd = even + length / 2;

        const double odd_real =
            real[odd] * twiddle_real - imaginary[odd] * twiddle_imaginary;
        const double odd_imaginary =
            real[odd] * twiddle_imaginary + imaginary[odd] * twiddle_real;

        real[odd] = real[even] - odd_real;
        imaginary[odd] = imaginary[even] - odd_imaginary;
        real[even] += odd_real;
        imaginary[even] += odd_imaginary;

        const double next_real =
            twiddle_real * step_real - twiddle_imaginary * step_imaginary;
        twiddle_imaginary =
            twiddle_real * step_imaginary + twiddle_imaginary * step_real;
        twiddle_real = next_real;
      }
    }
  }

  return true;
}

}  // namespace ddd::analysis
