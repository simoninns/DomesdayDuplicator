/************************************************************************

    test_sinc_interpolation.cpp

    What the signal did between the samples
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <vector>

#include "front_end_gain.h"
#include "sinc_interpolation.h"

namespace ddd::analysis {
namespace {

// An 8 MHz carrier at 40 Msps is five samples a cycle.
constexpr double kSamplesPerCycle = 5.0;
constexpr double kAmplitude = 400.0;

double CarrierAt(double position, double samples_per_cycle = kSamplesPerCycle) {
  return kAdcMidScaleCode +
         (kAmplitude *
          std::sin(2.0 * std::numbers::pi * position / samples_per_cycle));
}

std::vector<uint16_t> MakeCarrier(size_t count,
                                  double samples_per_cycle = kSamplesPerCycle) {
  std::vector<uint16_t> codes(count);
  for (size_t index = 0; index < count; ++index) {
    codes[index] = static_cast<uint16_t>(
        std::lround(CarrierAt(static_cast<double>(index), samples_per_cycle)));
  }
  return codes;
}

TEST(SincInterpolationTest, TheSamplesThemselvesComeBackUnchanged) {
  // A reconstruction that moved the measured points would be inventing a
  // different signal, not filling in this one.
  const ReconstructionKernel kernel;
  const std::vector<uint16_t> codes = MakeCarrier(200);

  for (size_t index = 20; index < 180; ++index) {
    EXPECT_NEAR(
        kernel.Evaluate(codes.data(), codes.size(), static_cast<double>(index)),
        static_cast<double>(codes[index]), 0.6)
        << "sample " << index;
  }
}

TEST(SincInterpolationTest, ACarrierIsRecoveredBetweenItsSamples) {
  // The measurement that justifies the whole file. At five samples a cycle the
  // true waveform is known exactly, so the reconstruction can be checked
  // against it rather than against a picture of it.
  const ReconstructionKernel kernel;
  const std::vector<uint16_t> codes = MakeCarrier(400);

  double worst = 0.0;
  for (int step = 0; step <= 1000; ++step) {
    const double position =
        100.0 + (static_cast<double>(step) / 1000.0 * 200.0);
    const double reconstructed =
        kernel.Evaluate(codes.data(), codes.size(), position);
    worst = std::max(worst, std::abs(reconstructed - CarrierAt(position)));
  }

  // Within 1% of the amplitude, which is a fraction of a pixel on any plot this
  // will be drawn on.
  EXPECT_LT(worst, kAmplitude * 0.01) << "worst error " << worst << " codes";
}

TEST(SincInterpolationTest, TheCrestsAreFoundWhereTheSamplesMissThem) {
  // The concrete failure of joining the samples with straight lines: at five
  // samples a cycle the samples mostly miss the peak, so a polyline reads low.
  // This is that comparison, as a number.
  const ReconstructionKernel kernel;
  const std::vector<uint16_t> codes = MakeCarrier(400);

  // The sample nearest the crest of the cycle starting at 100.
  double highest_sample = 0.0;
  for (size_t index = 100; index < 105; ++index) {
    highest_sample =
        std::max(highest_sample, static_cast<double>(codes[index]));
  }

  double highest_reconstructed = 0.0;
  for (int step = 0; step <= 500; ++step) {
    const double position = 100.0 + (static_cast<double>(step) / 100.0);
    highest_reconstructed =
        std::max(highest_reconstructed,
                 kernel.Evaluate(codes.data(), codes.size(), position));
  }

  const double truth = kAdcMidScaleCode + kAmplitude;

  // The reconstruction finds the real crest; the samples alone are short of it
  // by enough to matter to anybody reading an amplitude off the screen.
  EXPECT_NEAR(highest_reconstructed, truth, kAmplitude * 0.01);
  EXPECT_LT(highest_sample, truth - (kAmplitude * 0.02))
      << "the samples happened to land on the crest, so this test is not "
         "measuring what it thinks";
}

TEST(SincInterpolationTest, AConstantSignalStaysExactlyConstant) {
  // Un-normalised weights sum to a hair under one and vary with the fraction,
  // which would draw a flat signal as a faint ripple at the pixel pitch — an
  // artefact that reads as noise on the signal and is not.
  const ReconstructionKernel kernel;
  const std::vector<uint16_t> flat(200, 700);

  for (int step = 0; step <= 200; ++step) {
    const double position = 50.0 + (static_cast<double>(step) / 100.0);
    EXPECT_NEAR(kernel.Evaluate(flat.data(), flat.size(), position), 700.0,
                1e-9)
        << "at " << position;
  }
}

TEST(SincInterpolationTest, TheEndsOfTheBufferAreHeldRatherThanInvented) {
  const ReconstructionKernel kernel;
  const std::vector<uint16_t> codes = MakeCarrier(200);

  // Off either end, the answer is the end sample rather than an extrapolation
  // or a read past the buffer.
  EXPECT_NEAR(kernel.Evaluate(codes.data(), codes.size(), -50.0),
              static_cast<double>(codes.front()), 60.0);
  EXPECT_NEAR(kernel.Evaluate(codes.data(), codes.size(), 5000.0),
              static_cast<double>(codes.back()), 60.0);

  EXPECT_DOUBLE_EQ(kernel.Evaluate(nullptr, 10, 1.0), 0.0);

  const std::vector<uint16_t> one(1, 512);
  EXPECT_DOUBLE_EQ(kernel.Evaluate(one.data(), one.size(), 0.5), 512.0);
}

TEST(SincInterpolationTest, TheKernelIsTheSizeItWasAskedFor) {
  const ReconstructionKernel kernel(4);
  EXPECT_EQ(kernel.half_taps(), 4);
  EXPECT_EQ(kernel.taps(), 8);

  // Still recognisably the same waveform with half the taps, which is what
  // makes the default a comfort rather than a necessity.
  const std::vector<uint16_t> codes = MakeCarrier(400);
  EXPECT_NEAR(kernel.Evaluate(codes.data(), codes.size(), 100.5),
              CarrierAt(100.5), kAmplitude * 0.05);
}

}  // namespace
}  // namespace ddd::analysis
