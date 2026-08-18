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

#include "capture_format.h"
#include "capture_pipeline.h"
#include "front_end_gain.h"
#include "logger.h"
#include "sample_format.h"
#include "sample_sink.h"
#include "statistics_presenter.h"
#include "synthetic_source.h"

namespace ddd::gui {
namespace {

// A function rather than a namespace-scope constant: constructing a QString
// before main() runs is an allocation whose failure has nowhere to go, and
// clang-tidy is right to say so. Constructed on each call, which costs nothing
// a test can measure.
QString None() { return QString::fromUtf8("—"); }

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

// A device reading, as the gateware's instrument produces one. The geometry is
// the real one: a 16384-word buffer offered in 8192-word packets, so the
// headroom every percentage is measured against is the 8192 words above a
// packet.
capture::FpgaTelemetry Reading(uint16_t peak, uint16_t overflows = 0) {
  capture::FpgaTelemetry reading;
  reading.present = true;
  reading.format = capture::kTelemetryFormat;
  reading.used_now = 4000;
  reading.peak = peak;
  reading.peak_since_open = peak;
  reading.overflow_events = overflows;
  reading.depth_words = 16384;
  reading.packet_words = 8192;
  reading.near_full_words = 12288;
  return reading;
}

// --- The device's buffer ---------------------------------------------------
//
// The bar is how full the buffer got, and the reason it is that rather than how
// much trouble the device was in is a bench observation: on a working capture
// the trouble figure is zero for hours, and a bar that never leaves zero cannot
// be told from one that is broken. The verdict lives in the caption instead.

TEST(StatisticsPresenterTest, AWorkingCaptureShowsTheBufferBeingUsed) {
  // What the hardware actually reports: the buffer fills to the packet
  // threshold, the FX3 takes the packet, and it never goes higher. Half the
  // buffer, every packet, for the whole run — the instrument's proof that it is
  // reading a device rather than reading nothing.
  capture::CaptureStats stats = RunningStats();
  stats.device_buffer = Reading(8194);
  stats.device_buffer.used_now = 3120;

  const StatisticsView view =
      PresentStatistics(stats, Undeclared(), capture::DeviceSpeed::kSuper);

  EXPECT_EQ(view.back_pressure_percent, 50);
  EXPECT_TRUE(view.back_pressure.contains(QStringLiteral("8194")))
      << view.back_pressure.toStdString();
  EXPECT_TRUE(view.back_pressure.contains(QStringLiteral("16384")))
      << view.back_pressure.toStdString();

  // The occupancy at the instant of the reading leads, because it is the only
  // figure here that changes from one reading to the next: the peak of a
  // capture that is keeping up is the packet threshold plus a word or two,
  // every time, and a caption that led with it looked frozen.
  EXPECT_TRUE(view.back_pressure.startsWith(QStringLiteral("now 3120")))
      << view.back_pressure.toStdString();

  // And it is not reported as trouble, because it is not
  EXPECT_EQ(stats.device_buffer.BackPressurePercent(), 0);
}

TEST(StatisticsPresenterTest, AStretchedDeviceSaysSoInWords) {
  capture::CaptureStats stats = RunningStats();
  stats.device_buffer = Reading(10240);

  const StatisticsView view =
      PresentStatistics(stats, Undeclared(), capture::DeviceSpeed::kSuper);

  // 10240 of 16384 words is 62% of the buffer...
  EXPECT_EQ(view.back_pressure_percent, 62);

  // ...and 2048 words into the 8192 of reserve above a packet, which is the
  // figure that says how much trouble that is
  EXPECT_EQ(stats.device_buffer.BackPressurePercent(), 25);
  EXPECT_TRUE(view.back_pressure.contains(QStringLiteral("reserve")))
      << view.back_pressure.toStdString();
}

TEST(StatisticsPresenterTest, TheTooltipCarriesWhatTheCaptionCannot) {
  capture::CaptureStats stats = RunningStats();
  stats.device_buffer = Reading(8194);
  stats.device_buffer.packets_read = 1221;
  stats.device_buffer.peak_since_open = 9000;

  const StatisticsView view =
      PresentStatistics(stats, Undeclared(), capture::DeviceSpeed::kSuper);

  // The packets taken are the plainest evidence that the device is draining
  EXPECT_TRUE(view.back_pressure_detail.contains(QStringLiteral("1221")))
      << view.back_pressure_detail.toStdString();
  EXPECT_TRUE(view.back_pressure_detail.contains(QStringLiteral("9000")))
      << view.back_pressure_detail.toStdString();
}

TEST(StatisticsPresenterTest, ADeviceThatIsNotMovingSaysIdle) {
  // Nothing filling and nothing draining. Distinct from a device that cannot
  // report, and distinct from one whose buffer is simply empty at this instant.
  capture::CaptureStats stats = RunningStats();
  stats.device_buffer = Reading(0);
  stats.device_buffer.used_now = 0;

  const StatisticsView view =
      PresentStatistics(stats, Undeclared(), capture::DeviceSpeed::kSuper);

  EXPECT_EQ(view.back_pressure_percent, 0);
  EXPECT_TRUE(view.back_pressure.contains(QStringLiteral("Idle")))
      << view.back_pressure.toStdString();
}

TEST(StatisticsPresenterTest, LostSamplesReplaceThePercentageWithTheDamage) {
  // Once samples have been lost the percentages have stopped being the
  // interesting numbers.
  capture::CaptureStats stats = RunningStats();
  stats.device_buffer = Reading(16384, 2);
  stats.device_overflow_events = 3;
  stats.device_dropped_words = 4200;

  const StatisticsView view =
      PresentStatistics(stats, Undeclared(), capture::DeviceSpeed::kSuper);

  // A full buffer is the top of the bar, and is what overflow means
  EXPECT_EQ(view.back_pressure_percent, 100);
  EXPECT_EQ(stats.device_buffer.BackPressurePercent(), 100);
  EXPECT_TRUE(view.back_pressure.contains(QStringLiteral("4200")))
      << view.back_pressure.toStdString();
}

TEST(StatisticsPresenterTest, GatewareWithoutTheInstrumentSaysSo) {
  // The case that must not read as a healthy device: gateware that predates the
  // instrument captures perfectly well and simply cannot answer the question.
  const StatisticsView view = PresentStatistics(RunningStats(), Undeclared(),
                                                capture::DeviceSpeed::kSuper);

  EXPECT_EQ(view.back_pressure_percent, 0);
  EXPECT_TRUE(view.back_pressure.contains(QStringLiteral("Not reported")))
      << view.back_pressure.toStdString();
}

TEST(StatisticsPresenterTest, ASpikeStaysVisibleForAboutASecond) {
  // A reading covers a quarter of a second and reports the worst moment in it,
  // so a single bad interval would otherwise be a flash nobody sees. Four
  // readings later it is down to a quarter of its height, and it is gone
  // before it can be mistaken for a device still in trouble.
  BackPressureHold hold;

  EXPECT_EQ(hold.Apply(100), 100);
  EXPECT_EQ(hold.Apply(0), 70);
  EXPECT_EQ(hold.Apply(0), 49);
  EXPECT_EQ(hold.Apply(0), 34);
  EXPECT_EQ(hold.Apply(0), 23);

  // A higher reading always wins immediately: the hold may only delay a fall,
  // never a rise.
  EXPECT_EQ(hold.Apply(80), 80);

  hold.Reset();
  EXPECT_EQ(hold.displayed(), 0);
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
  EXPECT_EQ(FormatThroughput(0.0), None());
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
  EXPECT_EQ(FormatAmplitude(metrics, Undeclared()), None());
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

  EXPECT_NE(view.front_end_gain, None());
  EXPECT_TRUE(view.front_end_gain.contains(QStringLiteral("Not declared")))
      << view.front_end_gain.toStdString();
}

// --- Idle ----------------------------------------------------------------

TEST(StatisticsPresenterTest, IdleShowsNothingMeasuredAndWhatIsStillKnown) {
  const StatisticsView view =
      PresentIdleStatistics(Declared(), capture::DeviceSpeed::kSuper);

  EXPECT_EQ(view.throughput, None());
  EXPECT_EQ(view.signal_level, None());
  EXPECT_EQ(view.samples, None());
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

  EXPECT_EQ(view.bytes_written, None());
}

// --- The three capture-only figures --------------------------------------

// Blank while monitoring, because there is no encoder. Zero pending is a
// meaningful measurement during a capture and a meaningless one outside it, and
// a row reading "0.0 ms" when nothing is being written would look like a
// healthy encoder rather than like an absent one.
TEST(StatisticsPresenterTest, TheEncoderBacklogIsBlankWhileMerelyMonitoring) {
  capture::CaptureStats stats = RunningStats();
  stats.writing = false;
  stats.samples_pending = 0;

  const StatisticsView view =
      PresentStatistics(stats, Undeclared(), capture::DeviceSpeed::kSuper);

  EXPECT_EQ(view.encoder_backlog, None());
}

TEST(StatisticsPresenterTest, TheEncoderBacklogIsShownAsTimeWhileCapturing) {
  capture::CaptureStats stats = RunningStats();
  stats.writing = true;

  // A tenth of a second of signal held inside the encoder.
  stats.samples_pending = capture::kSampleRateHz / 10;

  const StatisticsView view =
      PresentStatistics(stats, Undeclared(), capture::DeviceSpeed::kSuper);

  // Time rather than a bare sample count, because the number that matters is
  // how it compares with the ring's own depth.
  EXPECT_TRUE(view.encoder_backlog.contains(QStringLiteral("100.0 ms")))
      << view.encoder_backlog.toStdString();
}

TEST(StatisticsPresenterTest, TheBacklogIsMeasuredAtTheRateTheStreamIsRunning) {
  // The comparison this figure exists for is against the ring's own depth, in
  // time. A decimated stream puts half as many samples into a second of signal,
  // so the same count of pending samples is twice the backlog — and a figure
  // fixed at the converter's rate would report a stalling encoder as coping.
  capture::CaptureStats stats = RunningStats();
  stats.writing = true;
  stats.samples_pending = capture::kSampleRateHz / 10;

  const StatisticsView view = PresentStatistics(
      stats, Undeclared(), capture::DeviceSpeed::kSuper, {},
      capture::kEstimatedCaptureBytesPerSecond,
      capture::SampleRateHzFor(capture::kTapeDecimationFactor));

  EXPECT_TRUE(view.encoder_backlog.contains(QStringLiteral("200.0 ms")))
      << view.encoder_backlog.toStdString();
}

TEST(StatisticsPresenterTest,
     AnEncoderKeepingUpReportsNoBacklogRatherThanNone) {
  capture::CaptureStats stats = RunningStats();
  stats.writing = true;
  stats.samples_pending = 0;

  const StatisticsView view =
      PresentStatistics(stats, Undeclared(), capture::DeviceSpeed::kSuper);

  EXPECT_NE(view.encoder_backlog, None());
  EXPECT_TRUE(view.encoder_backlog.contains(QStringLiteral("0.0 ms")))
      << view.encoder_backlog.toStdString();
}

// A time first, because "412 GB free" does not answer the question a user has,
// which is whether this will last the side they are about to play.
TEST(StatisticsPresenterTest, FreeSpaceIsHowMuchCaptureItHolds) {
  capture::FreeSpace space;
  space.known = true;
  space.bytes_available = capture::CaptureBytesForSeconds(3600.0);

  const StatisticsView view = PresentStatistics(
      RunningStats(), Undeclared(), capture::DeviceSpeed::kSuper, space);

  EXPECT_TRUE(view.space_remaining.startsWith(QStringLiteral("1:00:0")))
      << view.space_remaining.toStdString();
  EXPECT_TRUE(view.space_remaining.contains(QStringLiteral("GB")))
      << view.space_remaining.toStdString();
}

// Unknown, and specifically not zero. Zero reads as "the disk is full" and
// would stop somebody capturing to a folder they were about to create.
TEST(StatisticsPresenterTest, AVolumeThatCannotBeReadIsUnknownRatherThanFull) {
  const StatisticsView view =
      PresentStatistics(RunningStats(), Undeclared(),
                        capture::DeviceSpeed::kSuper, capture::FreeSpace{});

  EXPECT_EQ(view.space_remaining, None());
}

// The two panels that show free space share this formatter, so they cannot end
// up saying different things about the same volume.
TEST(StatisticsPresenterTest, ByteSizesUseTheUnitsADriveIsSoldIn) {
  EXPECT_EQ(FormatByteSize(2'000'000'000), QStringLiteral("2.0 GB"));
  EXPECT_EQ(FormatByteSize(512'000'000), QStringLiteral("512 MB"));
}

// The rate is not a constant: an uncompressed capture costs twice what a FLAC
// one does, so the same volume holds half as much of it. A readout that assumed
// FLAC would promise a recording the disk cannot hold.
TEST(StatisticsPresenterTest, TheSpaceReadoutIsRelativeToWhatWillBeWritten) {
  capture::FreeSpace space;
  space.known = true;
  space.bytes_available = 40'000'000'000;

  const QString compressed = FormatSpaceRemaining(space);
  const QString uncompressed = FormatSpaceRemaining(
      space, static_cast<double>(capture::kWireBytesPerSecond));

  EXPECT_NE(compressed, uncompressed);

  // Half the time, exactly: 1000 seconds against 500. Both are shown as h:mm:ss
  // past a minute, which is what makes them comparable at a glance.
  EXPECT_TRUE(compressed.contains(QStringLiteral("0:16:40")))
      << compressed.toStdString();
  EXPECT_TRUE(uncompressed.contains(QStringLiteral("0:08:20")))
      << uncompressed.toStdString();
}

// --- Counts a person can read --------------------------------------------

// A capture running for a side of a disc reaches ninety thousand million
// samples, and "90,113,472,000" is a figure nobody reads — they count the digit
// groups, get it wrong, and look away.
TEST(StatisticsPresenterTest, LargeCountsAreScaledToAUnit) {
  EXPECT_EQ(FormatCount(1'500), QStringLiteral("1.50 k"));
  EXPECT_EQ(FormatCount(12'340), QStringLiteral("12.3 k"));
  EXPECT_EQ(FormatCount(123'400), QStringLiteral("123 k"));
  EXPECT_EQ(FormatCount(40'000'000), QStringLiteral("40.0 M"));
  EXPECT_EQ(FormatCount(90'113'472'000), QStringLiteral("90.1 G"));
  EXPECT_EQ(FormatCount(2'500'000'000'000), QStringLiteral("2.50 T"));
}

// Below a thousand every digit is information: "3 transfers" is a fact about
// the run and "3.00" is not.
TEST(StatisticsPresenterTest, SmallCountsAreGivenExactly) {
  EXPECT_EQ(FormatCount(0), QStringLiteral("0"));
  EXPECT_EQ(FormatCount(7), QStringLiteral("7"));
  EXPECT_EQ(FormatCount(999), QStringLiteral("999"));

  // And the first scaled figure is the one immediately above it, rather than
  // there being a gap where neither rule applies.
  EXPECT_EQ(FormatCount(1'000), QStringLiteral("1.00 k"));
}

// Powers of a thousand, not of 1024. These are counts of events and samples
// rather than quantities of memory, and a user reading a sample count against a
// device specified at 40 million a second expects the two to line up.
TEST(StatisticsPresenterTest, CountsScaleByThousandsAndNotByKibibytes) {
  EXPECT_EQ(FormatCount(1'024), QStringLiteral("1.02 k"));
}

// Both rows go through it, which is the whole point: they are the two figures
// that grow without bound during a capture.
TEST(StatisticsPresenterTest, TheSampleAndTransferRowsAreScaled) {
  capture::CaptureStats stats = RunningStats();
  stats.transfers_completed = 1'234'000;
  stats.buffers_processed = 56'000;
  stats.metrics.sample_count = 40'000'000'000;

  const StatisticsView view =
      PresentStatistics(stats, Undeclared(), capture::DeviceSpeed::kSuper);

  EXPECT_EQ(view.samples, QStringLiteral("40.0 G"));
  EXPECT_TRUE(view.transfers.contains(QStringLiteral("1.23 M")))
      << view.transfers.toStdString();
  EXPECT_TRUE(view.transfers.contains(QStringLiteral("56.0 k")))
      << view.transfers.toStdString();
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

  // Both counts are scaled for reading, so they are compared against the
  // formatter rather than against the raw number: what matters is that the
  // figure shown is the pipeline's and not a recomputed one.
  EXPECT_TRUE(view.transfers.contains(FormatCount(stats.buffers_processed)))
      << view.transfers.toStdString();
  EXPECT_TRUE(view.buffer.contains(QString::number(stats.slot_count)))
      << view.buffer.toStdString();
  EXPECT_TRUE(view.buffer.contains(QString::number(stats.peak_slots_in_use)))
      << view.buffer.toStdString();

  EXPECT_EQ(view.samples, FormatCount(stats.metrics.sample_count));

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
