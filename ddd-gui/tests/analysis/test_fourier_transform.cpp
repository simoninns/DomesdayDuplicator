/************************************************************************

    test_fourier_transform.cpp

    The FFT, checked against a directly evaluated DFT
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <vector>

#include "fourier_transform.h"

namespace ddd::analysis {
namespace {

// The transform evaluated straight from the definition: O(N²), obviously
// correct, and sharing no code with the implementation under test. That
// independence is the point — a fast transform checked against itself, or
// against properties it happens to satisfy, can be wrong in exactly the way
// that is hardest to see on a display.
void DirectTransform(const std::vector<double>& real_in,
                     const std::vector<double>& imaginary_in,
                     std::vector<double>& real_out,
                     std::vector<double>& imaginary_out) {
  const size_t count = real_in.size();
  real_out.assign(count, 0.0);
  imaginary_out.assign(count, 0.0);

  for (size_t k = 0; k < count; ++k) {
    for (size_t n = 0; n < count; ++n) {
      const double angle = -2.0 * std::numbers::pi * static_cast<double>(k) *
                           static_cast<double>(n) / static_cast<double>(count);
      const double cosine = std::cos(angle);
      const double sine = std::sin(angle);
      real_out[k] += real_in[n] * cosine - imaginary_in[n] * sine;
      imaginary_out[k] += real_in[n] * sine + imaginary_in[n] * cosine;
    }
  }
}

TEST(FourierTransformTest, PowersOfTwoAreRecognised) {
  EXPECT_TRUE(IsPowerOfTwo(1));
  EXPECT_TRUE(IsPowerOfTwo(2));
  EXPECT_TRUE(IsPowerOfTwo(4096));
  EXPECT_FALSE(IsPowerOfTwo(0));
  EXPECT_FALSE(IsPowerOfTwo(3));
  EXPECT_FALSE(IsPowerOfTwo(1000));
}

TEST(FourierTransformTest, ASizeThatIsNotAPowerOfTwoIsRefused) {
  std::vector<double> real(6, 1.0);
  std::vector<double> imaginary(6, 0.0);

  EXPECT_FALSE(ForwardTransform(real, imaginary));

  // Refused means untouched, not partly transformed.
  for (const double value : real) {
    EXPECT_DOUBLE_EQ(value, 1.0);
  }
}

TEST(FourierTransformTest, MismatchedLengthsAreRefused) {
  std::vector<double> real(8, 1.0);
  std::vector<double> imaginary(4, 0.0);

  EXPECT_FALSE(ForwardTransform(real, imaginary));
}

TEST(FourierTransformTest, ItAgreesWithADirectlyEvaluatedTransform) {
  constexpr size_t kSize = 256;

  std::vector<double> real(kSize);
  std::vector<double> imaginary(kSize);

  // A deliberately unmusical input: three tones at unrelated frequencies, a DC
  // offset and a ramp, so no symmetry of the input can hide an error.
  for (size_t index = 0; index < kSize; ++index) {
    const double t = static_cast<double>(index);
    real[index] = 3.0 + 0.01 * t +
                  2.0 * std::sin(2.0 * std::numbers::pi * 7.0 * t / kSize) +
                  0.7 * std::cos(2.0 * std::numbers::pi * 31.5 * t / kSize) +
                  0.3 * std::sin(2.0 * std::numbers::pi * 100.0 * t / kSize);
    imaginary[index] = 0.2 * std::cos(2.0 * std::numbers::pi * 3.0 * t / kSize);
  }

  std::vector<double> expected_real;
  std::vector<double> expected_imaginary;
  DirectTransform(real, imaginary, expected_real, expected_imaginary);

  ASSERT_TRUE(ForwardTransform(real, imaginary));

  for (size_t bin = 0; bin < kSize; ++bin) {
    EXPECT_NEAR(real[bin], expected_real[bin], 1e-9) << "bin " << bin;
    EXPECT_NEAR(imaginary[bin], expected_imaginary[bin], 1e-9) << "bin " << bin;
  }
}

TEST(FourierTransformTest, AnImpulseTransformsToAFlatSpectrum) {
  constexpr size_t kSize = 64;

  std::vector<double> real(kSize, 0.0);
  std::vector<double> imaginary(kSize, 0.0);
  real[0] = 1.0;

  ASSERT_TRUE(ForwardTransform(real, imaginary));

  for (size_t bin = 0; bin < kSize; ++bin) {
    EXPECT_NEAR(real[bin], 1.0, 1e-12) << "bin " << bin;
    EXPECT_NEAR(imaginary[bin], 0.0, 1e-12) << "bin " << bin;
  }
}

TEST(FourierTransformTest, ATonePutsItsEnergyInOneBin) {
  constexpr size_t kSize = 1024;
  constexpr size_t kBin = 137;

  std::vector<double> real(kSize);
  std::vector<double> imaginary(kSize, 0.0);
  for (size_t index = 0; index < kSize; ++index) {
    real[index] =
        std::cos(2.0 * std::numbers::pi * static_cast<double>(kBin) *
                 static_cast<double>(index) / static_cast<double>(kSize));
  }

  ASSERT_TRUE(ForwardTransform(real, imaginary));

  // Exactly on a bin centre, so there is no leakage to tolerate: everything but
  // the tone's bin and its mirror should be numerically zero.
  for (size_t bin = 0; bin < kSize; ++bin) {
    const double magnitude = std::hypot(real[bin], imaginary[bin]);
    if (bin == kBin || bin == kSize - kBin) {
      EXPECT_NEAR(magnitude, static_cast<double>(kSize) / 2.0, 1e-8);
    } else {
      EXPECT_NEAR(magnitude, 0.0, 1e-8) << "bin " << bin;
    }
  }
}

TEST(FourierTransformTest, EnergyIsConservedAtTheSizeTheApplicationUses) {
  // Parseval, at 4,096 points — the size the spectrum panel actually runs, so
  // the twiddle-factor drift the implementation accepts is measured where it
  // accumulates most rather than on a toy transform.
  constexpr size_t kSize = 4096;

  std::vector<double> real(kSize);
  std::vector<double> imaginary(kSize, 0.0);

  double input_energy = 0.0;
  for (size_t index = 0; index < kSize; ++index) {
    const double t = static_cast<double>(index);
    real[index] = std::sin(t * 0.37) + 0.5 * std::cos(t * 1.13);
    input_energy += real[index] * real[index];
  }

  ASSERT_TRUE(ForwardTransform(real, imaginary));

  double output_energy = 0.0;
  for (size_t bin = 0; bin < kSize; ++bin) {
    output_energy += real[bin] * real[bin] + imaginary[bin] * imaginary[bin];
  }

  EXPECT_NEAR(output_energy / static_cast<double>(kSize), input_energy,
              input_energy * 1e-10);
}

TEST(FourierTransformTest, ASingleSampleTransformsToItself) {
  std::vector<double> real{5.0};
  std::vector<double> imaginary{-2.0};

  ASSERT_TRUE(ForwardTransform(real, imaginary));

  EXPECT_DOUBLE_EQ(real[0], 5.0);
  EXPECT_DOUBLE_EQ(imaginary[0], -2.0);
}

}  // namespace
}  // namespace ddd::analysis
