/************************************************************************

    test_statistics_panel.cpp

    T1 tests for the statistics panel's formatting and updates
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QLabel>
#include <QProgressBar>
#include <QSet>
#include <QString>
#include <QStringList>

#include "monitor_tap.h"
#include "sample_format.h"
#include "statistics_panel.h"

namespace ddd::gui {
namespace {

QLabel* LabelNamed(const StatisticsPanel& panel, const char* name) {
  return panel.findChild<QLabel*>(QLatin1String(name));
}

// --- Formatting ----------------------------------------------------------

// Both units, because they answer different questions. MB/s is what a disk is
// specified in and says whether the storage can keep up; Msps is what the
// device is specified in and says whether all of the signal is arriving.
TEST(StatisticsFormatTest, ThroughputIsGivenInBothUnitsAUserCaresAbout) {
  const QString text = FormatThroughput(ddd::capture::kWireBytesPerSecond);

  EXPECT_TRUE(text.contains(QStringLiteral("MB/s"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("Msps"))) << text.toStdString();

  // 80,000,000 bytes per second is 76.3 MiB/s and exactly 40 Msps.
  EXPECT_TRUE(text.contains(QStringLiteral("76.3"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("40.00"))) << text.toStdString();
}

TEST(StatisticsFormatTest, NoThroughputYetIsBlankRatherThanZero) {
  // A hard zero reads as "it is running and delivering nothing", which is a
  // different and much more alarming statement than "it has not started".
  EXPECT_EQ(FormatThroughput(0.0), QString::fromUtf8("—"));
}

TEST(StatisticsFormatTest, TheAmplitudeIsGivenAsAProportionOfTheRange) {
  ddd::capture::SampleMetricsSnapshot metrics;
  metrics.sample_count = 1000;
  metrics.recent_minimum_value = 0;
  metrics.recent_maximum_value = ddd::capture::kMaximumSampleValue;

  const QString text = FormatAmplitude(metrics);
  EXPECT_TRUE(text.contains(QStringLiteral("100.0"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("1023"))) << text.toStdString();
}

TEST(StatisticsFormatTest, HalfTheRangeReadsAsHalfTheRange) {
  ddd::capture::SampleMetricsSnapshot metrics;
  metrics.sample_count = 1000;
  metrics.recent_minimum_value = 256;
  metrics.recent_maximum_value = 767;

  EXPECT_TRUE(FormatAmplitude(metrics).contains(QStringLiteral("50.0")))
      << FormatAmplitude(metrics).toStdString();
}

TEST(StatisticsFormatTest, NoSamplesYetIsBlank) {
  const ddd::capture::SampleMetricsSnapshot metrics;
  EXPECT_EQ(FormatAmplitude(metrics), QString::fromUtf8("—"));
}

// --- The panel -----------------------------------------------------------

TEST(StatisticsPanelTest, AFreshPanelShowsNothingRatherThanZeroes) {
  StatisticsPanel panel(nullptr);

  EXPECT_EQ(LabelNamed(panel, StatisticsPanel::kThroughputLabelName)->text(),
            QString::fromUtf8("—"));
  EXPECT_EQ(LabelNamed(panel, StatisticsPanel::kAmplitudeLabelName)->text(),
            QString::fromUtf8("—"));
}

TEST(StatisticsPanelTest, PublishedStatisticsReachEveryField) {
  StatisticsPanel panel(nullptr);

  ddd::capture::CaptureStats stats;
  stats.throughput_bytes_per_second = ddd::capture::kWireBytesPerSecond;
  stats.transfers_completed = 1234;
  stats.buffers_processed = 56;
  stats.slots_in_use = 2;
  stats.peak_slots_in_use = 9;
  stats.slot_count = 128;
  stats.elapsed_seconds = 12.5;
  stats.sequence_state = ddd::capture::SequenceState::kRunning;
  stats.metrics.sample_count = 1'000'000;
  stats.metrics.recent_minimum_value = 100;
  stats.metrics.recent_maximum_value = 900;
  stats.metrics.clipped_low_count = 3;
  stats.metrics.clipped_high_count = 4;

  panel.OnStatsUpdated(stats);

  EXPECT_TRUE(LabelNamed(panel, StatisticsPanel::kThroughputLabelName)
                  ->text()
                  .contains(QStringLiteral("MB/s")));
  EXPECT_TRUE(LabelNamed(panel, StatisticsPanel::kTransfersLabelName)
                  ->text()
                  .contains(QStringLiteral("1234")));
  EXPECT_TRUE(LabelNamed(panel, StatisticsPanel::kClippingLabelName)
                  ->text()
                  .contains(QStringLiteral("3")));
  EXPECT_TRUE(LabelNamed(panel, StatisticsPanel::kElapsedLabelName)
                  ->text()
                  .contains(QStringLiteral("12.5")));

  auto* bar = panel.findChild<QProgressBar*>(
      QLatin1String(StatisticsPanel::kBufferBarName));
  ASSERT_NE(bar, nullptr);
  EXPECT_EQ(bar->value(), 1);
  // The peak is what matters after the fact: a capture that was fine except for
  // one stall thirty minutes in reads as perfect from the live value alone.
  EXPECT_TRUE(bar->format().contains(QStringLiteral("9")))
      << bar->format().toStdString();
}

// The four sequence states have to read differently to a user, because they
// mean four entirely different things — and one of them ("this gateware does
// not send markers") is not a fault at all.
TEST(StatisticsPanelTest, EachIntegrityStateReadsDifferently) {
  StatisticsPanel panel(nullptr);
  QLabel* const integrity =
      LabelNamed(panel, StatisticsPanel::kSequenceLabelName);

  const ddd::capture::SequenceState states[] = {
      ddd::capture::SequenceState::kSynchronising,
      ddd::capture::SequenceState::kRunning,
      ddd::capture::SequenceState::kDisabled,
      ddd::capture::SequenceState::kFailed};

  QStringList seen;
  for (ddd::capture::SequenceState state : states) {
    ddd::capture::CaptureStats stats;
    stats.sequence_state = state;
    panel.OnStatsUpdated(stats);
    seen.append(integrity->text());
  }

  EXPECT_EQ(QSet<QString>(seen.begin(), seen.end()).size(), seen.size())
      << "two integrity states read the same to a user";

  // A broken capture has to say so in words, not in a status code.
  EXPECT_TRUE(seen.at(3).contains(QStringLiteral("lost")))
      << seen.at(3).toStdString();
}

// Starting a new run must not leave the previous one's figures on screen, or a
// device that fails to deliver anything looks like it is working.
TEST(StatisticsPanelTest, StartingAgainClearsTheOldFigures) {
  StatisticsPanel panel(nullptr);

  ddd::capture::CaptureStats stats;
  stats.throughput_bytes_per_second = ddd::capture::kWireBytesPerSecond;
  stats.transfers_completed = 99;
  panel.OnStatsUpdated(stats);
  ASSERT_NE(LabelNamed(panel, StatisticsPanel::kThroughputLabelName)->text(),
            QString::fromUtf8("—"));

  panel.OnMonitoringChanged(true);

  EXPECT_EQ(LabelNamed(panel, StatisticsPanel::kThroughputLabelName)->text(),
            QString::fromUtf8("—"));
  EXPECT_EQ(LabelNamed(panel, StatisticsPanel::kTransfersLabelName)->text(),
            QString::fromUtf8("—"));
}

// Stopping leaves them up. What a capture achieved is the thing a user wants to
// read after it has finished.
TEST(StatisticsPanelTest, StoppingLeavesTheFinalFiguresOnScreen) {
  StatisticsPanel panel(nullptr);

  ddd::capture::CaptureStats stats;
  stats.transfers_completed = 4321;
  panel.OnStatsUpdated(stats);

  panel.OnMonitoringChanged(false);

  EXPECT_TRUE(LabelNamed(panel, StatisticsPanel::kTransfersLabelName)
                  ->text()
                  .contains(QStringLiteral("4321")));
}

}  // namespace
}  // namespace ddd::gui
