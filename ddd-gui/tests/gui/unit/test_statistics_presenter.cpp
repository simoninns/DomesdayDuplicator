/************************************************************************

    test_statistics_presenter.cpp

    T1 tests for the figures the Statistics panel shows
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QRegularExpression>
#include <QString>
#include <chrono>
#include <memory>
#include <thread>

#include "capture_pipeline.h"
#include "front_end_gain.h"
#include "logger.h"
#include "sample_format.h"
#include "sample_sink.h"
#include "statistics_presenter.h"
#include "synthetic_source.h"

namespace ddd::gui {
namespace {

const QString kNone = QString::fromUtf8("—");

analysis::FrontEndGain Undeclared() { return analysis::FrontEndGain(); }

// Switches 2 alone: gain 6.00, so a full-scale span is 333 mV p-p. Picked
// because the arithmetic comes out in round numbers and a wrong factor shows up
// immediately.
analysis::FrontEndGain Declared() {
  return analysis::FrontEndGain::FromSwitchPattern(0b0100);
}

capture::CaptureStats RunningStats() {
  capture::CaptureStats stats;
  stats.throughput_bytes_per_second = capture::kWireBytesPerSecond;
  stats.transfers_completed = 1234;
  stats.buffers_processed = 56;
  stats.slots_in_use = 2;
  stats.peak_slots_in_use = 9;
  stats.slot_count = 128;
  stats.elapsed_seconds = 12.5;
  stats.sequence_state = capture::SequenceState::kRunning;
  stats.metrics.sample_count = 1'000'000;
  stats.metrics.minimum_value = 12;
  stats.metrics.maximum_value = 1000;
  stats.metrics.recent_minimum_value = 256;
  stats.metrics.recent_maximum_value = 767;
  stats.metrics.clipped_low_count = 3;
  stats.metrics.clipped_high_count = 4;
  stats.metrics.recent_clipped_low_count = 1;
  stats.metrics.recent_clipped_high_count = 2;
  return stats;
}

// --- Units ---------------------------------------------------------------

// Both units, because they answer different questions. MB/s is what a disk is
// specified in and says whether the storage can keep up; Msps is what the
// device is specified in and says whether all of the signal is arriving.
TEST(StatisticsPresenterTest, ThroughputIsGivenInBothUnitsAUserCaresAbout) {
  const QString text = FormatThroughput(capture::kWireBytesPerSecond);

  EXPECT_TRUE(text.contains(QStringLiteral("MB/s"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("Msps"))) << text.toStdString();

  // 80,000,000 bytes per second is 76.3 MiB/s and exactly 40 Msps.
  EXPECT_TRUE(text.contains(QStringLiteral("76.3"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("40.00"))) << text.toStdString();
}

TEST(StatisticsPresenterTest, NoThroughputYetIsBlankRatherThanZero) {
  // A hard zero reads as "it is running and delivering nothing", which is a
  // different and much more alarming statement than "it has not started".
  EXPECT_EQ(FormatThroughput(0.0), kNone);
}

TEST(StatisticsPresenterTest, TheAmplitudeIsGivenAsAProportionOfTheRange) {
  capture::SampleMetricsSnapshot metrics;
  metrics.sample_count = 1000;
  metrics.recent_minimum_value = 0;
  metrics.recent_maximum_value = capture::kMaximumSampleValue;

  const QString text = FormatAmplitude(metrics, Undeclared());
  EXPECT_TRUE(text.contains(QStringLiteral("100.0"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("1023"))) << text.toStdString();
}

TEST(StatisticsPresenterTest, HalfTheRangeReadsAsHalfTheRange) {
  capture::SampleMetricsSnapshot metrics;
  metrics.sample_count = 1000;
  metrics.recent_minimum_value = 256;
  metrics.recent_maximum_value = 767;

  EXPECT_TRUE(
      FormatAmplitude(metrics, Undeclared()).contains(QStringLiteral("50.0")));
}

TEST(StatisticsPresenterTest, NoSamplesYetIsBlank) {
  const capture::SampleMetricsSnapshot metrics;
  EXPECT_EQ(FormatAmplitude(metrics, Undeclared()), kNone);
}

TEST(StatisticsPresenterTest, ShortElapsedTimesAreSecondsAndLongOnesAreClocks) {
  // "5,412.3 s" is not a length of time anybody can picture, and a capture runs
  // for a side of a disc.
  EXPECT_EQ(FormatElapsed(12.5), QStringLiteral("12.5 s"));
  EXPECT_EQ(FormatElapsed(3661.0), QStringLiteral("1:01:01"));
}

// --- The gain declaration ------------------------------------------------

TEST(StatisticsPresenterTest, WithNoDeclarationNoFigureIsInVolts) {
  // The rule the whole gain declaration exists to keep. Checked across every
  // field at once rather than one at a time, because the failure being guarded
  // against is a single field that was forgotten.
  const StatisticsView view = PresentStatistics(RunningStats(), Undeclared(),
                                                capture::DeviceSpeed::kSuper);

  const QString fields =
      view.throughput + view.integrity + view.buffer + view.signal_level +
      view.extremes + view.clipping + view.transfers + view.samples +
      view.elapsed + view.bytes_written + view.link_speed + view.front_end_gain;

  EXPECT_FALSE(fields.contains(QStringLiteral("mV"))) << fields.toStdString();
}

TEST(StatisticsPresenterTest, WithADeclarationTheLevelsCarryVoltsAsWell) {
  const StatisticsView view = PresentStatistics(RunningStats(), Declared(),
                                                capture::DeviceSpeed::kSuper);

  // Codes first and volts beside them, never volts instead: the code figure is
  // what was measured and the voltage is derived from something the user said.
  EXPECT_TRUE(view.signal_level.contains(QStringLiteral("1023")));
  EXPECT_TRUE(view.signal_level.contains(QStringLiteral("mV")))
      << view.signal_level.toStdString();
  EXPECT_TRUE(view.extremes.contains(QStringLiteral("mV")))
      << view.extremes.toStdString();
}

TEST(StatisticsPresenterTest, TheDeclaredLevelIsTheOneTheBoardWouldProduce) {
  capture::SampleMetricsSnapshot metrics;
  metrics.sample_count = 1000;
  metrics.recent_minimum_value = 0;
  metrics.recent_maximum_value = capture::kMaximumSampleValue;

  // A full-scale span at a gain of 6 is 2000 / 6 = 333 mV p-p, which is the
  // figure on the board's own calculations sheet.
  EXPECT_TRUE(
      FormatAmplitude(metrics, Declared()).contains(QStringLiteral("333")))
      << FormatAmplitude(metrics, Declared()).toStdString();
}

TEST(StatisticsPresenterTest, ClippingIsIdenticalWhateverTheDeclarationSays) {
  // Clipping is a code reaching 0 or 1023, which is a property of the
  // converter. A wrong declaration, or none, must not change it — this is what
  // keeps the application useful to somebody who never opened the settings.
  const capture::CaptureStats stats = RunningStats();

  const QString undeclared =
      PresentStatistics(stats, Undeclared(), capture::DeviceSpeed::kSuper)
          .clipping;
  const QString correct =
      PresentStatistics(stats, Declared(), capture::DeviceSpeed::kSuper)
          .clipping;
  const QString wrong =
      PresentStatistics(stats,
                        analysis::FrontEndGain::FromSwitchPattern(0b1111),
                        capture::DeviceSpeed::kSuper)
          .clipping;

  EXPECT_EQ(undeclared, correct);
  EXPECT_EQ(correct, wrong);
}

TEST(StatisticsPresenterTest, AnUndeclaredGainSaysSoRatherThanShowingADash) {
  const StatisticsView view =
      PresentIdleStatistics(Undeclared(), capture::DeviceSpeed::kUnknown);

  EXPECT_NE(view.front_end_gain, kNone);
  EXPECT_TRUE(view.front_end_gain.contains(QStringLiteral("Not declared")))
      << view.front_end_gain.toStdString();
}

// --- Idle ----------------------------------------------------------------

TEST(StatisticsPresenterTest, IdleShowsNothingMeasuredAndWhatIsStillKnown) {
  const StatisticsView view =
      PresentIdleStatistics(Declared(), capture::DeviceSpeed::kSuper);

  EXPECT_EQ(view.throughput, kNone);
  EXPECT_EQ(view.signal_level, kNone);
  EXPECT_EQ(view.samples, kNone);
  EXPECT_EQ(view.buffer_percent, 0);

  // The link speed and the declared gain are facts about the setup, not
  // measurements, so they stay on screen when nothing is running.
  EXPECT_TRUE(view.link_speed.contains(QStringLiteral("Super")))
      << view.link_speed.toStdString();
  EXPECT_TRUE(view.front_end_gain.contains(QStringLiteral("6.00")))
      << view.front_end_gain.toStdString();
}

TEST(StatisticsPresenterTest, NoWriterMeansNothingWritten) {
  // Monitor mode. Zero bytes written is correct and is not a figure worth
  // showing, because nothing was meant to be written.
  const StatisticsView view = PresentStatistics(RunningStats(), Undeclared(),
                                                capture::DeviceSpeed::kSuper);

  EXPECT_EQ(view.bytes_written, kNone);
}

// --- Against a running pipeline ------------------------------------------

// The acceptance criterion for this panel: every figure comes from the stats
// block, and the stats block is the one a real pipeline published. Anything
// that reads pipeline state directly, or invents a figure, disagrees here.
TEST(StatisticsPresenterTest, TheFiguresAreTheOnesASyntheticRunProduced) {
  capture::CallbackLogger logger([](capture::LogLevel, const std::string&) {},
                                 capture::LogLevel::kError);

  capture::SyntheticSource::Options source_options;
  source_options.pattern = capture::SyntheticSource::Pattern::kSine;
  source_options.slot_size_bytes = size_t{64} << 10;
  source_options.slot_count = 8;
  source_options.slot_limit = 24;
  capture::SyntheticSource source(source_options);

  capture::CapturePipeline::Options pipeline_options;
  pipeline_options.lock_memory = false;
  pipeline_options.elevate_priority = false;
  pipeline_options.queue_size_bytes = size_t{512} << 10;

  capture::CapturePipeline pipeline(&logger);
  ASSERT_TRUE(pipeline.Start(&source, std::make_unique<capture::NullSink>(),
                             pipeline_options));
  pipeline.Wait();

  const capture::CaptureStats stats = pipeline.stats().Read();
  ASSERT_GT(stats.buffers_processed, 0u);

  const StatisticsView view =
      PresentStatistics(stats, Undeclared(), capture::DeviceSpeed::kSuper);

  EXPECT_TRUE(view.transfers.contains(QString::number(stats.buffers_processed)))
      << view.transfers.toStdString();
  EXPECT_TRUE(view.buffer.contains(QString::number(stats.slot_count)))
      << view.buffer.toStdString();
  EXPECT_TRUE(view.buffer.contains(QString::number(stats.peak_slots_in_use)))
      << view.buffer.toStdString();

  // The sample count is grouped for reading, so it is compared by digits rather
  // than by string: what matters is that it is the pipeline's number and not a
  // recomputed one.
  QString digits = view.samples;
  digits.remove(QRegularExpression(QStringLiteral("[^0-9]")));
  EXPECT_EQ(digits, QString::number(stats.metrics.sample_count));

  EXPECT_TRUE(view.signal_level.contains(
      QString::number(stats.metrics.recent_maximum_value)))
      << view.signal_level.toStdString();

  // A sine near the middle of the range does not clip, and the presenter must
  // not invent any.
  EXPECT_TRUE(view.clipping.startsWith(QStringLiteral("0 low, 0 high")))
      << view.clipping.toStdString();
}

}  // namespace
}  // namespace ddd::gui
