/************************************************************************

    test_analysis_worker.cpp

    T1 tests for snapshot analysis and the thread it happens on
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <cmath>
#include <numbers>
#include <vector>

#include "analysis_worker.h"
#include "front_end_gain.h"
#include "monitor_tap.h"
#include "sample_format.h"
#include "spectrum_analyser.h"

namespace ddd::gui {
namespace {

constexpr size_t kSnapshotSamples = 8192;

// Wire data with a known sample value in every word and a sequence counter in
// the top bits, which is what the device actually sends. The counter is
// deliberately non-zero: a decoder that forgot to mask it would produce values
// thousands of codes too large, and a snapshot of all-zero counters would hide
// that.
std::vector<uint8_t> MakeWire(const std::vector<uint16_t>& values,
                              uint8_t counter = 37) {
  std::vector<uint8_t> wire(values.size() * capture::kBytesPerSample);
  for (size_t index = 0; index < values.size(); ++index) {
    const uint16_t word = capture::MakeWireWord(values[index], counter);
    wire[index * 2] = static_cast<uint8_t>(word & 0xFF);
    wire[(index * 2) + 1] = static_cast<uint8_t>(word >> 8);
  }
  return wire;
}

std::vector<uint16_t> Tone(size_t bin, size_t count) {
  std::vector<uint16_t> values(count);
  for (size_t index = 0; index < count; ++index) {
    const double phase = 2.0 * std::numbers::pi * static_cast<double>(bin) *
                         static_cast<double>(index) /
                         static_cast<double>(analysis::kDefaultTransformSize);
    values[index] = static_cast<uint16_t>(
        std::lround(analysis::kAdcMidScaleCode +
                    (analysis::kAdcMidScaleCode / 2.0) * std::cos(phase)));
  }
  return values;
}

TEST(SnapshotAnalyserTest, WithNoSourceNothingIsProduced) {
  SnapshotAnalyser analyser;
  const QSignalSpy waveform(&analyser, &SnapshotAnalyser::WaveformReady);

  analyser.Poll();

  EXPECT_EQ(waveform.count(), 0);
}

TEST(SnapshotAnalyserTest, TheSequenceCounterIsStrippedFromEverySample) {
  capture::SnapshotPublisher publisher(kSnapshotSamples *
                                       capture::kBytesPerSample);

  std::vector<uint16_t> values(kSnapshotSamples);
  for (size_t index = 0; index < values.size(); ++index) {
    values[index] = static_cast<uint16_t>(index % 1024);
  }
  const std::vector<uint8_t> wire = MakeWire(values);
  publisher.Publish(wire.data(), wire.size());

  SnapshotAnalyser analyser;
  analyser.SetSource(&publisher);

  QSignalSpy waveform(&analyser, &SnapshotAnalyser::WaveformReady);
  analyser.Poll();

  ASSERT_EQ(waveform.count(), 1);
  const auto codes = waveform.at(0).at(0).value<std::vector<uint16_t>>();
  ASSERT_EQ(codes.size(), values.size());
  EXPECT_EQ(codes, values);
}

TEST(SnapshotAnalyserTest, NothingNewMeansNothingEmitted) {
  // The ordinary case: the pipeline publishes about nine snapshots a second and
  // this is polled thirty times. A poll that found nothing must be free and
  // silent, not a repeat of the last frame.
  capture::SnapshotPublisher publisher(kSnapshotSamples *
                                       capture::kBytesPerSample);
  const std::vector<uint8_t> wire =
      MakeWire(std::vector<uint16_t>(kSnapshotSamples, 512));
  publisher.Publish(wire.data(), wire.size());

  SnapshotAnalyser analyser;
  analyser.SetSource(&publisher);

  QSignalSpy waveform(&analyser, &SnapshotAnalyser::WaveformReady);
  analyser.Poll();
  analyser.Poll();
  analyser.Poll();

  EXPECT_EQ(waveform.count(), 1);
}

TEST(SnapshotAnalyserTest, DetachingStopsTheFramesWithoutStoppingTheObject) {
  capture::SnapshotPublisher publisher(kSnapshotSamples *
                                       capture::kBytesPerSample);
  const std::vector<uint8_t> wire =
      MakeWire(std::vector<uint16_t>(kSnapshotSamples, 512));

  SnapshotAnalyser analyser;
  analyser.SetSource(&publisher);

  QSignalSpy waveform(&analyser, &SnapshotAnalyser::WaveformReady);

  publisher.Publish(wire.data(), wire.size());
  analyser.Poll();
  ASSERT_EQ(waveform.count(), 1);

  analyser.SetSource(nullptr);
  publisher.Publish(wire.data(), wire.size());
  analyser.Poll();

  EXPECT_EQ(waveform.count(), 1);
}

TEST(SnapshotAnalyserTest, ASnapshotShorterThanATransformStillDrawsAWaveform) {
  // The scope can show anything; the spectrum cannot be computed from fewer
  // samples than its window. One must not take the other down with it.
  const size_t samples = analysis::kDefaultTransformSize / 2;
  capture::SnapshotPublisher publisher(samples * capture::kBytesPerSample);

  const std::vector<uint8_t> wire =
      MakeWire(std::vector<uint16_t>(samples, 700));
  publisher.Publish(wire.data(), wire.size());

  SnapshotAnalyser analyser;
  analyser.SetSource(&publisher);

  QSignalSpy waveform(&analyser, &SnapshotAnalyser::WaveformReady);
  QSignalSpy spectrum(&analyser, &SnapshotAnalyser::SpectrumReady);
  analyser.Poll();

  EXPECT_EQ(waveform.count(), 1);
  EXPECT_EQ(spectrum.count(), 0);
}

TEST(SnapshotAnalyserTest, AToneInTheSnapshotReachesTheRightSpectrumBin) {
  capture::SnapshotPublisher publisher(kSnapshotSamples *
                                       capture::kBytesPerSample);

  constexpr size_t kBin = 800;
  const std::vector<uint8_t> wire = MakeWire(Tone(kBin, kSnapshotSamples));
  publisher.Publish(wire.data(), wire.size());

  SnapshotAnalyser analyser;
  analyser.SetSource(&publisher);

  QSignalSpy spectrum(&analyser, &SnapshotAnalyser::SpectrumReady);
  analyser.Poll();

  ASSERT_EQ(spectrum.count(), 1);
  const auto levels = spectrum.at(0).at(0).value<std::vector<double>>();
  ASSERT_EQ(levels.size(), (analysis::kDefaultTransformSize / 2) + 1);

  size_t peak = 0;
  for (size_t bin = 0; bin < levels.size(); ++bin) {
    if (levels[bin] > levels[peak]) {
      peak = bin;
    }
  }
  EXPECT_EQ(peak, kBin);
  EXPECT_NEAR(levels[kBin], -6.02, 0.1);
}

TEST(SnapshotAnalyserTest, AttachingANewSourceForgetsThePreviousSignal) {
  // A run is a run. Carrying an averaged spectrum or a peak hold across would
  // show the last device's carrier for seconds after this one was attached.
  capture::SnapshotPublisher publisher(kSnapshotSamples *
                                       capture::kBytesPerSample);

  SnapshotAnalyser analyser;
  analyser.SetSource(&publisher);

  const std::vector<uint8_t> tone = MakeWire(Tone(800, kSnapshotSamples));
  publisher.Publish(tone.data(), tone.size());
  analyser.Poll();

  analyser.SetSource(&publisher);

  const std::vector<uint8_t> silence = MakeWire(std::vector<uint16_t>(
      kSnapshotSamples, static_cast<uint16_t>(analysis::kAdcMidScaleCode)));
  publisher.Publish(silence.data(), silence.size());

  QSignalSpy spectrum(&analyser, &SnapshotAnalyser::SpectrumReady);
  analyser.Poll();

  ASSERT_EQ(spectrum.count(), 1);
  const auto peak_hold = spectrum.at(0).at(1).value<std::vector<double>>();
  EXPECT_DOUBLE_EQ(peak_hold[800], analysis::SpectrumAnalyser::kFloorDecibels);
}

TEST(SnapshotAnalyserTest, PeakHoldCanBeResetWithoutLosingTheLiveTrace) {
  capture::SnapshotPublisher publisher(kSnapshotSamples *
                                       capture::kBytesPerSample);

  SnapshotAnalyser analyser;
  analyser.SetSource(&publisher);

  const std::vector<uint8_t> tone = MakeWire(Tone(800, kSnapshotSamples));
  publisher.Publish(tone.data(), tone.size());
  analyser.Poll();

  analyser.RequestPeakHoldReset();

  const std::vector<uint8_t> silence = MakeWire(std::vector<uint16_t>(
      kSnapshotSamples, static_cast<uint16_t>(analysis::kAdcMidScaleCode)));
  publisher.Publish(silence.data(), silence.size());

  QSignalSpy spectrum(&analyser, &SnapshotAnalyser::SpectrumReady);
  analyser.Poll();

  ASSERT_EQ(spectrum.count(), 1);
  const auto levels = spectrum.at(0).at(0).value<std::vector<double>>();
  const auto peak_hold = spectrum.at(0).at(1).value<std::vector<double>>();

  // The peak hold is back to tracking the live trace rather than remembering
  // the tone. Not at the floor, and deliberately not asserted to be: the
  // averaging that makes the display readable is still carrying the tone's
  // power out over several frames, which is what averaging is for.
  EXPECT_DOUBLE_EQ(peak_hold[800], levels[800]);
  EXPECT_LT(peak_hold[800], -6.02);
}

// --- The thread ----------------------------------------------------------

TEST(AnalysisWorkerTest, AWorkerThatWasNeverStartedIsHarmlessToDrive) {
  AnalysisWorker worker;

  EXPECT_FALSE(worker.running());

  // No thread to carry any of these, and a caller should not have to check.
  worker.SetSource(nullptr);
  worker.SetSpectrumAveraging(0.5);
  worker.ResetPeakHold();
  worker.Stop();

  EXPECT_FALSE(worker.running());
}

TEST(AnalysisWorkerTest, FramesArriveOnTheThreadThatConnectedToThem) {
  capture::SnapshotPublisher publisher(kSnapshotSamples *
                                       capture::kBytesPerSample);

  AnalysisWorker worker;
  QSignalSpy waveform(&worker, &AnalysisWorker::WaveformReady);

  worker.Start();
  ASSERT_TRUE(worker.running());
  worker.SetSource(&publisher);

  const std::vector<uint8_t> wire =
      MakeWire(std::vector<uint16_t>(kSnapshotSamples, 600));
  publisher.Publish(wire.data(), wire.size());

  // The poll runs on the worker thread and the delivery is queued back to this
  // one, so this both proves the hop works and pumps the event loop that makes
  // it happen.
  ASSERT_TRUE(waveform.wait(2000));

  const auto codes = waveform.at(0).at(0).value<std::vector<uint16_t>>();
  ASSERT_FALSE(codes.empty());
  EXPECT_EQ(codes.front(), 600);

  worker.Stop();
  EXPECT_FALSE(worker.running());
}

TEST(AnalysisWorkerTest, StoppingIsSafeWhileSnapshotsAreStillBeingPublished) {
  // The failure this guards against: the pipeline builds a new snapshot
  // publisher for every run, so a worker still reading the old one during a
  // restart would be reading freed memory. Stop() has to detach before the
  // thread goes, and it has to wait for a poll already under way.
  auto publisher = std::make_unique<capture::SnapshotPublisher>(
      kSnapshotSamples * capture::kBytesPerSample);

  AnalysisWorker worker;
  worker.Start();
  worker.SetSource(publisher.get());

  const std::vector<uint8_t> wire =
      MakeWire(std::vector<uint16_t>(kSnapshotSamples, 512));
  for (int index = 0; index < 50; ++index) {
    publisher->Publish(wire.data(), wire.size());
  }

  worker.Stop();

  // Safe by construction once Stop() has returned, which is the whole claim.
  publisher.reset();

  EXPECT_FALSE(worker.running());
}

TEST(AnalysisWorkerTest, ItCanBeStartedAndStoppedRepeatedly) {
  AnalysisWorker worker;

  for (int run = 0; run < 3; ++run) {
    worker.Start();
    EXPECT_TRUE(worker.running());
    worker.Start();  // idempotent
    worker.Stop();
    EXPECT_FALSE(worker.running());
    worker.Stop();  // idempotent
  }
}

}  // namespace
}  // namespace ddd::gui
