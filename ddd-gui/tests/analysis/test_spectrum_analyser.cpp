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
#include <random>
#include <vector>

#include "fourier_transform.h"
#include "front_end_gain.h"
#include "sample_format.h"
#include "spectrum_analyser.h"

namespace ddd::analysis {
namespace {

constexpr size_t kTransformSize = 1024;

// What the pipeline publishes: 64 KiB, which is 32,768 samples. Written out
// here because the segment counts below are properties of that number and of
// the transform sizes offered, and a test that computed it from the same
// constant the code uses would agree with itself rather than with the device.
constexpr size_t kSnapshotSamples = 32'768;

// A tone sitting exactly on a bin centre, at a stated fraction of full scale.
// Exactly on a centre so that the expected level is a number that can be
// written down rather than one that depends on how the window smears a tone
// between two bins.
//
// The tone runs coherently for the whole buffer, so every segment cut out of it
// sees the same tone at the same bin — which is what makes an averaged estimate
// comparable with a single one.
std::vector<uint16_t> MakeTone(size_t bin, double amplitude_codes,
                               size_t count = kTransformSize,
                               size_t transform_size = kTransformSize) {
  std::vector<uint16_t> codes(count);
  for (size_t index = 0; index < count; ++index) {
    const double phase = 2.0 * std::numbers::pi * static_cast<double>(bin) *
                         static_cast<double>(index) /
                         static_cast<double>(transform_size);
    const double value = kAdcMidScaleCode + amplitude_codes * std::cos(phase);
    codes[index] = static_cast<uint16_t>(std::lround(value));
  }
  return codes;
}

// A tone confined to the last eighth of a buffer, mid-scale before it.
//
// Sine rather than cosine so the tone starts from zero: an onset that stepped
// straight to full amplitude would splatter across the whole spectrum, and the
// test would be measuring the step instead of the tone.
std::vector<uint16_t> MakeLateTone(size_t bin, double amplitude_codes,
                                   size_t count) {
  std::vector<uint16_t> codes(count, static_cast<uint16_t>(kAdcMidScaleCode));

  const size_t from = count - (count / 8);
  for (size_t index = from; index < count; ++index) {
    const double phase = 2.0 * std::numbers::pi * static_cast<double>(bin) *
                         static_cast<double>(index - from) /
                         static_cast<double>(kTransformSize);
    const double value = kAdcMidScaleCode + amplitude_codes * std::sin(phase);
    codes[index] = static_cast<uint16_t>(std::lround(value));
  }
  return codes;
}

// Reproducible noise. A fixed seed rather than a clock, because a variance test
// that failed one run in fifty would be worse than no test at all.
std::vector<uint16_t> MakeNoise(size_t count, double amplitude_codes) {
  std::mt19937 generator(20260816);
  std::uniform_real_distribution<double> spread(-amplitude_codes,
                                                amplitude_codes);

  std::vector<uint16_t> codes(count);
  for (size_t index = 0; index < count; ++index) {
    codes[index] = static_cast<uint16_t>(
        std::lround(kAdcMidScaleCode + spread(generator)));
  }
  return codes;
}

// How much the noise floor scatters, in dB, across a band of bins away from
// both ends of the spectrum. This is the figure segment averaging exists to
// reduce, and the one a user reads as the trace boiling.
double FloorScatterDb(const std::vector<double>& magnitudes_db, size_t from,
                      size_t to) {
  double total = 0.0;
  for (size_t bin = from; bin < to; ++bin) {
    total += magnitudes_db[bin];
  }
  const double mean = total / static_cast<double>(to - from);

  double squares = 0.0;
  for (size_t bin = from; bin < to; ++bin) {
    const double deviation = magnitudes_db[bin] - mean;
    squares += deviation * deviation;
  }
  return std::sqrt(squares / static_cast<double>(to - from));
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

TEST(SpectrumAnalyserTest, ALongerBufferIsMeasuredWholeAndReadsTheSame) {
  // Averaging segments must not move the level of a steady tone: a carrier at
  // half of full scale is at -6.02 dB whether it was measured once or fifteen
  // times, and a display whose calibration depended on the resolution setting
  // would be useless for setting a gain.
  SpectrumAnalyser analyser(InstantOptions());

  const std::vector<uint16_t> codes =
      MakeTone(200, kAdcMidScaleCode / 2.0, kTransformSize * 4);

  ASSERT_TRUE(analyser.Analyse(codes.data(), codes.size()));
  EXPECT_NEAR(analyser.magnitudes_db()[200], -6.02, 0.05);
  EXPECT_EQ(analyser.segment_count(),
            SpectrumAnalyser::SegmentsIn(kTransformSize * 4, kTransformSize));
}

TEST(SpectrumAnalyserTest, SomethingInTheLastPartOfTheSnapshotIsStillMeasured) {
  // The regression this whole estimator exists to fix. Transforming only the
  // first segment of a snapshot left seven eighths of every 819 µs of signal
  // unexamined, so an interferer that appeared late in one was simply not
  // there — and the display said so with the same authority it says everything
  // else.
  SpectrumAnalyser analyser(InstantOptions());

  constexpr size_t kToneBin = 200;
  const std::vector<uint16_t> codes =
      MakeLateTone(kToneBin, kAdcMidScaleCode / 2.0, kTransformSize * 8);

  ASSERT_TRUE(analyser.Analyse(codes.data(), codes.size()));

  const std::vector<double>& magnitudes = analyser.magnitudes_db();

  // Well down on a tone that ran the whole buffer — it was only present for an
  // eighth of it — and unmistakably present, which is the point.
  EXPECT_GT(magnitudes[kToneBin], -30.0);

  size_t peak_bin = 0;
  for (size_t bin = 1; bin < magnitudes.size(); ++bin) {
    if (magnitudes[bin] > magnitudes[peak_bin]) {
      peak_bin = bin;
    }
  }
  EXPECT_EQ(peak_bin, kToneBin);
}

TEST(SpectrumAnalyserTest, AveragingSegmentsSettlesTheNoiseFloor) {
  // A periodogram's scatter does not shrink with a longer transform — only with
  // more of them averaged. This is the measurement behind that claim, and the
  // reason the estimator cuts a snapshot up rather than transforming it whole.
  const std::vector<uint16_t> noise =
      MakeNoise(kTransformSize * 8, kAdcMidScaleCode / 4.0);

  SpectrumAnalyser once(InstantOptions());
  ASSERT_TRUE(once.Analyse(noise.data(), kTransformSize));

  SpectrumAnalyser averaged(InstantOptions());
  ASSERT_TRUE(averaged.Analyse(noise.data(), noise.size()));
  ASSERT_EQ(averaged.segment_count(), 15U);

  // Away from DC and from Nyquist, where the window and the mirror rule make
  // the end bins behave differently from the rest.
  const double one_segment = FloorScatterDb(once.magnitudes_db(), 50, 450);
  const double fifteen = FloorScatterDb(averaged.magnitudes_db(), 50, 450);

  // Fifteen segments should be about a quarter of the scatter. Asserted as half
  // so that the test is measuring the effect rather than the exact arithmetic
  // of one particular noise sequence.
  EXPECT_LT(fifteen, one_segment / 2.0)
      << "one segment scattered by " << one_segment << " dB and fifteen by "
      << fifteen << " dB";
}

TEST(SpectrumAnalyserTest, ASnapshotIsCutIntoHalfOverlappingSegments) {
  // The counts the resolution control is offering, at the size the pipeline
  // actually publishes.
  EXPECT_EQ(SpectrumAnalyser::SegmentsIn(kSnapshotSamples, 4096), 15U);
  EXPECT_EQ(SpectrumAnalyser::SegmentsIn(kSnapshotSamples, 8192), 7U);
  EXPECT_EQ(SpectrumAnalyser::SegmentsIn(kSnapshotSamples, 16384), 3U);

  // Exactly one transform's worth is one segment; a sample short of it is none,
  // which is the case Analyse refuses rather than padding.
  EXPECT_EQ(SpectrumAnalyser::SegmentsIn(4096, 4096), 1U);
  EXPECT_EQ(SpectrumAnalyser::SegmentsIn(4095, 4096), 0U);

  // A trailing part-segment is dropped rather than counted.
  EXPECT_EQ(SpectrumAnalyser::SegmentsIn(6143, 4096), 1U);
  EXPECT_EQ(SpectrumAnalyser::SegmentsIn(6144, 4096), 2U);
}

TEST(SpectrumAnalyserTest, EveryOfferedResolutionIsCalibratedTheSame) {
  for (const size_t size : kTransformSizeChoices) {
    SpectrumAnalyser::Options options;
    options.transform_size = size;
    options.averaging = 0.0;
    SpectrumAnalyser analyser(options);

    ASSERT_EQ(analyser.transform_size(), size);

    const size_t bin = size / 8;
    const std::vector<uint16_t> codes =
        MakeTone(bin, kAdcMidScaleCode - 1.0, kSnapshotSamples, size);

    ASSERT_TRUE(analyser.Analyse(codes.data(), codes.size()));

    EXPECT_NEAR(analyser.magnitudes_db()[bin], -0.02, 0.05) << "size " << size;
    EXPECT_EQ(analyser.segment_count(),
              SpectrumAnalyser::SegmentsIn(kSnapshotSamples, size))
        << "size " << size;
  }
}

TEST(SpectrumAnalyserTest, TheResolutionFiguresAreTheOnesAReadoutWouldQuote) {
  // 40 Msps over 4,096 points. The bandwidth is half again wider than the
  // spacing because the Hann window collects from beyond a bin's own width, and
  // conflating the two is how an instrument comes to claim a resolution it has
  // not got.
  EXPECT_NEAR(SpectrumAnalyser::BinSpacingHz(4096, capture::kSampleRateHz),
              9765.625, 1e-6);
  EXPECT_NEAR(SpectrumAnalyser::NoiseBandwidthHz(4096, capture::kSampleRateHz),
              14648.4375, 1e-6);

  // Halving the bin width takes twice the transform.
  EXPECT_NEAR(
      SpectrumAnalyser::BinSpacingHz(8192, capture::kSampleRateHz),
      SpectrumAnalyser::BinSpacingHz(4096, capture::kSampleRateHz) / 2.0, 1e-9);

  EXPECT_DOUBLE_EQ(SpectrumAnalyser::BinSpacingHz(0, capture::kSampleRateHz),
                   0.0);
}

TEST(SpectrumAnalyserTest, NothingHasBeenMeasuredBeforeTheFirstSnapshot) {
  SpectrumAnalyser analyser(InstantOptions());
  EXPECT_EQ(analyser.segment_count(), 0U);

  const std::vector<uint16_t> tone = MakeTone(200, kAdcMidScaleCode / 2.0);
  ASSERT_TRUE(analyser.Analyse(tone.data(), tone.size()));
  EXPECT_EQ(analyser.segment_count(), 1U);

  analyser.Reset();
  EXPECT_EQ(analyser.segment_count(), 0U);
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
