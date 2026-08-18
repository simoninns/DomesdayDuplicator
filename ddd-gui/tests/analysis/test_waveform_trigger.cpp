/************************************************************************

    test_waveform_trigger.cpp

    Holding a repeating waveform still, which is what makes it a scope
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

#include "front_end_gain.h"
#include "sample_format.h"
#include "waveform_trigger.h"

namespace ddd::analysis {
namespace {

// An 8 MHz carrier at 40 Msps: five samples a cycle, which is what a LaserDisc
// FM signal actually looks like to this converter.
constexpr double kSamplesPerCycle = 5.0;

std::vector<uint16_t> MakeCarrier(size_t count, double amplitude_codes,
                                  double phase_offset,
                                  double samples_per_cycle = kSamplesPerCycle) {
  std::vector<uint16_t> codes(count);
  for (size_t index = 0; index < count; ++index) {
    const double phase = (2.0 * std::numbers::pi * static_cast<double>(index) /
                          samples_per_cycle) +
                         phase_offset;
    codes[index] = static_cast<uint16_t>(std::lround(std::clamp(
        kAdcMidScaleCode + (amplitude_codes * std::sin(phase)), 0.0, 1023.0)));
  }
  return codes;
}

TEST(WaveformTriggerTest, ACarrierTriggersOncePerCycle) {
  const std::vector<uint16_t> codes = MakeCarrier(500, 300.0, 0.0);

  TriggerOptions options;
  std::vector<double> positions;
  FindTriggers(codes.data(), codes.size(), options, 20, positions);

  ASSERT_GE(positions.size(), 10U);

  for (size_t index = 1; index < positions.size(); ++index) {
    EXPECT_NEAR(positions[index] - positions[index - 1], kSamplesPerCycle, 0.05)
        << "trigger " << index;
  }
}

TEST(WaveformTriggerTest, TheSweepStartsAtTheSamePointWhateverThePhase) {
  // The property the whole display rests on. Snapshots begin wherever the USB
  // transfer began, so the same carrier arrives at a different phase every
  // frame; if the trigger did not undo that, the trace would shimmer at the
  // snapshot rate and read as a band of fuzz.
  //
  // Checked as the waveform's own phase at the trigger point, which is the
  // thing that has to be constant — not the sample index, which of course
  // moves.
  TriggerOptions options;
  std::vector<double> positions;

  for (int step = 0; step < 16; ++step) {
    const double offset =
        2.0 * std::numbers::pi * static_cast<double>(step) / 16.0;
    const std::vector<uint16_t> codes = MakeCarrier(500, 300.0, offset);

    FindTriggers(codes.data(), codes.size(), options, 4, positions);
    ASSERT_FALSE(positions.empty()) << "phase step " << step;

    // The signal's phase where the sweep starts, as a fraction of a cycle.
    const double phase =
        std::fmod((positions.front() / kSamplesPerCycle) +
                      (offset / (2.0 * std::numbers::pi)) + 1.0,
                  1.0);

    // A rising crossing of mid-scale is a sine's zero, so every sweep should
    // start at phase zero however the snapshot was cut.
    EXPECT_NEAR(std::min(phase, 1.0 - phase), 0.0, 0.01)
        << "phase step " << step << " started at " << phase << " of a cycle";
  }
}

TEST(WaveformTriggerTest, TheCrossingIsFoundBetweenSamplesNotAtOne) {
  // Rounding the crossing to the nearest sample would leave a fifth of a cycle
  // of jitter at five samples a cycle, which is most of the shimmer the trigger
  // is there to remove.
  //
  // A ramp from below the level to above it, crossing exactly a quarter of the
  // way between two samples.
  const std::vector<uint16_t> codes = {400, 400, 400, 500, 548, 600};

  TriggerOptions options;
  options.level_codes = 512.0;
  options.hysteresis_codes = 16.0;

  std::vector<double> positions;
  FindTriggers(codes.data(), codes.size(), options, 4, positions);

  ASSERT_EQ(positions.size(), 1U);

  // Between samples 3 (500) and 4 (548): 12 of the 48 codes, a quarter.
  EXPECT_NEAR(positions.front(), 3.25, 1e-9);
}

TEST(WaveformTriggerTest, NoiseOnTheLevelDoesNotFireATriggerPerSample) {
  // Without hysteresis a signal sitting on the trigger level triggers on every
  // upward wobble, and the sweeps end up aligned to the noise rather than to
  // the waveform.
  // A fixed seed, so a hysteresis test cannot fail one run in fifty.
  // Predictability is the property under test rather than a fault. Both the
  // cert- and bugprone- names are listed because which one exists depends on
  // the clang-tidy release.
  //
  // NOLINTNEXTLINE(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed)
  std::mt19937 generator(7);
  std::uniform_int_distribution<int> wobble(-6, 6);

  std::vector<uint16_t> codes(2000);
  for (auto& code : codes) {
    code = static_cast<uint16_t>(512 + wobble(generator));
  }

  TriggerOptions options;
  std::vector<double> positions;
  FindTriggers(codes.data(), codes.size(), options, 100, positions);

  EXPECT_TRUE(positions.empty())
      << "noise of a few codes about the level produced " << positions.size()
      << " triggers";
}

TEST(WaveformTriggerTest, SweepsAreSpreadAcrossTheSnapshotWhenAskedToBe) {
  // A carrier crosses the level every five samples, so without a separation
  // every sweep would come from the first fraction of a microsecond and the
  // accumulated picture would say nothing a single sweep did not.
  const std::vector<uint16_t> codes = MakeCarrier(32'768, 300.0, 0.0);

  TriggerOptions options;
  options.minimum_separation = 1024;

  std::vector<double> positions;
  FindTriggers(codes.data(), codes.size(), options, 32, positions);

  ASSERT_EQ(positions.size(), 32U);

  for (size_t index = 1; index < positions.size(); ++index) {
    EXPECT_GE(positions[index] - positions[index - 1], 1024.0);
  }

  // And they reach the far end of the snapshot rather than clustering.
  EXPECT_GT(positions.back(), 30'000.0);
}

TEST(WaveformTriggerTest, AFlatInputTriggersNothingRatherThanEverything) {
  const std::vector<uint16_t> flat(1000,
                                   static_cast<uint16_t>(kAdcMidScaleCode));

  TriggerOptions options;
  std::vector<double> positions;
  FindTriggers(flat.data(), flat.size(), options, 32, positions);

  EXPECT_TRUE(positions.empty());

  // And so does a signal that never comes back down far enough to re-arm.
  const std::vector<uint16_t> high(1000, 900);
  FindTriggers(high.data(), high.size(), options, 32, positions);
  EXPECT_TRUE(positions.empty());
}

TEST(WaveformTriggerTest, TheSearchStopsOnceEnoughHaveBeenFound) {
  // A snapshot holds several thousand crossings and the display wants thirty.
  const std::vector<uint16_t> codes = MakeCarrier(32'768, 300.0, 0.0);

  TriggerOptions options;
  std::vector<double> positions;
  FindTriggers(codes.data(), codes.size(), options, 8, positions);

  EXPECT_EQ(positions.size(), 8U);
  EXPECT_LT(positions.back(), 100.0);
}

TEST(WaveformTriggerTest, NothingIsAskedOfAnEmptyOrAbsentBuffer) {
  TriggerOptions options;
  std::vector<double> positions{1.0, 2.0};

  FindTriggers(nullptr, 100, options, 8, positions);
  EXPECT_TRUE(positions.empty());

  const std::vector<uint16_t> one(1, 512);
  positions.assign({1.0});
  FindTriggers(one.data(), one.size(), options, 8, positions);
  EXPECT_TRUE(positions.empty());
}

}  // namespace
}  // namespace ddd::analysis
