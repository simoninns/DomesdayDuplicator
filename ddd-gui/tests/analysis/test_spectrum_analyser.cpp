/************************************************************************

    test_spectrum_analyser.cpp

    Windowed power spectrum, averaged and peak-held
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <vector>

#include "fourier_transform.h"
#include "front_end_gain.h"
#include "sample_format.h"
#include "spectrum_analyser.h"

namespace ddd::analysis {
namespace {

constexpr size_t kTransformSize = 1024;

// A tone sitting exactly on a bin centre, at a stated fraction of full scale.
// Exactly on a centre so that the expected level is a number that can be
// written down rather than one that depends on how the window smears a tone
// between two bins.
std::vector<uint16_t> MakeTone(size_t bin, double amplitude_codes,
                               size_t count = kTransformSize) {
  std::vector<uint16_t> codes(count);
  for (size_t index = 0; index < count; ++index) {
    const double phase = 2.0 * std::numbers::pi * static_cast<double>(bin) *
                         static_cast<double>(index) /
                         static_cast<double>(kTransformSize);
    const double value = kAdcMidScaleCode + amplitude_codes * std::cos(phase);
    codes[index] = static_cast<uint16_t>(std::lround(value));
  }
  return codes;
}

SpectrumAnalyser::Options InstantOptions() {
  SpectrumAnalyser::Options options;
  options.transform_size = kTransformSize;
  options.averaging = 0.0;
  return options;
}

TEST(SpectrumAnalyserTest, ThereIsABinForEveryFrequencyUpToNyquist) {
  const SpectrumAnalyser analyser(InstantOptions());

  EXPECT_EQ(analyser.transform_size(), kTransformSize);
  EXPECT_EQ(analyser.bin_count(), (kTransformSize / 2) + 1);
}

TEST(SpectrumAnalyserTest, BinsMapToTheFrequenciesTheDeviceCanRepresent) {
  EXPECT_DOUBLE_EQ(SpectrumAnalyser::BinFrequencyHz(0, kTransformSize,
                                                    capture::kSampleRateHz),
                   0.0);

  // The top bin is Nyquist: 20 MHz for a 40 Msps converter, which is the whole
  // range the spectrum panel claims to show.
  EXPECT_DOUBLE_EQ(
      SpectrumAnalyser::BinFrequencyHz(kTransformSize / 2, kTransformSize,
                                       capture::kSampleRateHz),
      20'000'000.0);
}

TEST(SpectrumAnalyserTest, AFrequencyMapsBackToItsBin) {
  for (size_t bin = 0; bin <= kTransformSize / 2; bin += 37) {
    const double frequency = SpectrumAnalyser::BinFrequencyHz(
        bin, kTransformSize, capture::kSampleRateHz);
    EXPECT_EQ(SpectrumAnalyser::FrequencyToBin(frequency, kTransformSize,
                                               capture::kSampleRateHz),
              bin);
  }
}

TEST(SpectrumAnalyserTest, AFrequencyAboveNyquistIsClampedToTheTopBin) {
  EXPECT_EQ(SpectrumAnalyser::FrequencyToBin(200'000'000.0, kTransformSize,
                                             capture::kSampleRateHz),
            kTransformSize / 2);
  EXPECT_EQ(SpectrumAnalyser::FrequencyToBin(-5.0, kTransformSize,
                                             capture::kSampleRateHz),
            0u);
}

TEST(SpectrumAnalyserTest, AToneAppearsInItsOwnBinAtItsOwnLevel) {
  // The measurement the whole panel rests on. Half of full scale is -6.02 dB,
  // and it has to come out that way whatever the window does — which is what
  // the normalisation by the window's sum is for.
  SpectrumAnalyser analyser(InstantOptions());

  constexpr size_t kToneBin = 200;
  const std::vector<uint16_t> codes =
      MakeTone(kToneBin, kAdcMidScaleCode / 2.0);

  ASSERT_TRUE(analyser.Analyse(codes.data(), codes.size()));

  const std::vector<double>& magnitudes = analyser.magnitudes_db();
  ASSERT_EQ(magnitudes.size(), (kTransformSize / 2) + 1);

  EXPECT_NEAR(magnitudes[kToneBin], -6.02, 0.05);

  // The peak is where the tone is, and not merely near it.
  size_t peak_bin = 0;
  for (size_t bin = 0; bin < magnitudes.size(); ++bin) {
    if (magnitudes[bin] > magnitudes[peak_bin]) {
      peak_bin = bin;
    }
  }
  EXPECT_EQ(peak_bin, kToneBin);
}

TEST(SpectrumAnalyserTest, AFullScaleToneReadsZeroDecibels) {
  SpectrumAnalyser analyser(InstantOptions());

  const std::vector<uint16_t> codes = MakeTone(150, kAdcMidScaleCode - 1.0);

  ASSERT_TRUE(analyser.Analyse(codes.data(), codes.size()));

  // Not exactly full scale — a tone of amplitude 512 about mid-scale would run
  // one code past the top of the converter — so a hair under 0 dB is the right
  // answer and 0 dB would be the wrong one.
  EXPECT_NEAR(analyser.magnitudes_db()[150], -0.02, 0.05);
}

TEST(SpectrumAnalyserTest, TheWindowKeepsAToneOutOfDistantBins) {
  // Without a window a tone smears across the whole spectrum and buries
  // everything else in it, which on this display would hide an interfering
  // carrier next to the FM one.
  SpectrumAnalyser analyser(InstantOptions());

  constexpr size_t kToneBin = 200;
  const std::vector<uint16_t> codes =
      MakeTone(kToneBin, kAdcMidScaleCode / 2.0);

  ASSERT_TRUE(analyser.Analyse(codes.data(), codes.size()));

  const std::vector<double>& magnitudes = analyser.magnitudes_db();
  for (size_t bin = 0; bin < magnitudes.size(); ++bin) {
    const size_t distance = bin > kToneBin ? bin - kToneBin : kToneBin - bin;
    if (distance < 20) {
      continue;
    }
    EXPECT_LT(magnitudes[bin], -50.0) << "bin " << bin;
  }
}

TEST(SpectrumAnalyserTest, ASteadyOffsetIsInTheDCBin) {
  SpectrumAnalyser analyser(InstantOptions());

  // Well away from mid-scale and nothing else: everything should be at DC.
  std::vector<uint16_t> codes(kTransformSize, 768);

  ASSERT_TRUE(analyser.Analyse(codes.data(), codes.size()));

  const std::vector<double>& magnitudes = analyser.magnitudes_db();
  EXPECT_GT(magnitudes[0], -20.0);
  for (size_t bin = 5; bin < magnitudes.size(); ++bin) {
    EXPECT_LT(magnitudes[bin], -80.0) << "bin " << bin;
  }
}

TEST(SpectrumAnalyserTest,
     ASilentInputSitsOnTheFloorRatherThanAtMinusInfinity) {
  SpectrumAnalyser analyser(InstantOptions());

  const std::vector<uint16_t> codes(kTransformSize,
                                    static_cast<uint16_t>(kAdcMidScaleCode));

  ASSERT_TRUE(analyser.Analyse(codes.data(), codes.size()));

  for (const double level : analyser.magnitudes_db()) {
    EXPECT_DOUBLE_EQ(level, SpectrumAnalyser::kFloorDecibels);
  }
}

TEST(SpectrumAnalyserTest, TooFewSamplesAreRefusedRatherThanPadded) {
  // Zero-padding a short snapshot would produce a spectrum of a signal that
  // stopped halfway through the window, which is not the signal.
  SpectrumAnalyser analyser(InstantOptions());

  const std::vector<uint16_t> codes(kTransformSize / 2, 512);

  EXPECT_FALSE(analyser.Analyse(codes.data(), codes.size()));
  EXPECT_FALSE(analyser.Analyse(nullptr, kTransformSize));

  for (const double level : analyser.magnitudes_db()) {
    EXPECT_DOUBLE_EQ(level, SpectrumAnalyser::kFloorDecibels);
  }
}

TEST(SpectrumAnalyserTest, MoreSamplesThanNeededAreIgnoredRatherThanRefused) {
  SpectrumAnalyser analyser(InstantOptions());

  const std::vector<uint16_t> codes =
      MakeTone(200, kAdcMidScaleCode / 2.0, kTransformSize * 4);

  ASSERT_TRUE(analyser.Analyse(codes.data(), codes.size()));
  EXPECT_NEAR(analyser.magnitudes_db()[200], -6.02, 0.05);
}

TEST(SpectrumAnalyserTest, PeakHoldRemembersATonePastItsDisappearance) {
  SpectrumAnalyser analyser(InstantOptions());

  const std::vector<uint16_t> tone = MakeTone(200, kAdcMidScaleCode / 2.0);
  const std::vector<uint16_t> silence(kTransformSize,
                                      static_cast<uint16_t>(kAdcMidScaleCode));

  ASSERT_TRUE(analyser.Analyse(tone.data(), tone.size()));
  ASSERT_TRUE(analyser.Analyse(silence.data(), silence.size()));

  EXPECT_DOUBLE_EQ(analyser.magnitudes_db()[200],
                   SpectrumAnalyser::kFloorDecibels);
  EXPECT_NEAR(analyser.peak_hold_db()[200], -6.02, 0.05);

  analyser.ResetPeakHold();
  EXPECT_DOUBLE_EQ(analyser.peak_hold_db()[200],
                   SpectrumAnalyser::kFloorDecibels);
}

TEST(SpectrumAnalyserTest, AveragingMovesTowardsTheNewLevelRatherThanJumping) {
  SpectrumAnalyser::Options options = InstantOptions();
  options.averaging = 0.8;
  SpectrumAnalyser analyser(options);

  const std::vector<uint16_t> loud = MakeTone(200, kAdcMidScaleCode / 2.0);
  const std::vector<uint16_t> quiet = MakeTone(200, kAdcMidScaleCode / 16.0);

  ASSERT_TRUE(analyser.Analyse(loud.data(), loud.size()));
  const double first = analyser.magnitudes_db()[200];

  // The first transform is taken whole rather than averaged against a starting
  // value of nothing, so the display is right immediately instead of climbing
  // out of the floor for the first second of every run.
  EXPECT_NEAR(first, -6.02, 0.05);

  ASSERT_TRUE(analyser.Analyse(quiet.data(), quiet.size()));
  const double second = analyser.magnitudes_db()[200];

  const double quiet_level = 20.0 * std::log10(1.0 / 16.0);
  EXPECT_LT(second, first);
  EXPECT_GT(second, quiet_level);
}

TEST(SpectrumAnalyserTest, ResettingForgetsBothTheAverageAndThePeak) {
  SpectrumAnalyser analyser(InstantOptions());

  const std::vector<uint16_t> tone = MakeTone(200, kAdcMidScaleCode / 2.0);
  ASSERT_TRUE(analyser.Analyse(tone.data(), tone.size()));

  analyser.Reset();

  for (size_t bin = 0; bin < analyser.bin_count(); ++bin) {
    EXPECT_DOUBLE_EQ(analyser.magnitudes_db()[bin],
                     SpectrumAnalyser::kFloorDecibels);
    EXPECT_DOUBLE_EQ(analyser.peak_hold_db()[bin],
                     SpectrumAnalyser::kFloorDecibels);
  }
}

TEST(SpectrumAnalyserTest, AnImpossibleTransformSizeFallsBackToTheDefault) {
  SpectrumAnalyser::Options options;
  options.transform_size = 1000;
  const SpectrumAnalyser analyser(options);

  EXPECT_EQ(analyser.transform_size(), kDefaultTransformSize);
  EXPECT_TRUE(IsPowerOfTwo(analyser.transform_size()));
}

}  // namespace
}  // namespace ddd::analysis
