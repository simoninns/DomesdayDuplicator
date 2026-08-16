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

// The formatting these labels display is tested in
// tests/gui/unit/test_statistics_presenter.cpp, against a pipeline running on
// synthetic data. What is left here is what only a widget can answer: that the
// text reaches the right label, and that starting and stopping do the right
// thing to what is on screen.

namespace ddd::gui {
namespace {

QLabel* LabelNamed(const StatisticsPanel& panel, const char* name) {
  return panel.findChild<QLabel*>(QLatin1String(name));
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

TEST(StatisticsPanelTest, TheBackPressureBarShowsWhatTheDeviceReported) {
  StatisticsPanel panel(nullptr);

  // What a working capture reports: filled to the packet threshold and taken
  // away again. The bar has to show that as use rather than as nothing, which
  // is the whole reason it is scaled on the buffer and not on the trouble.
  ddd::capture::CaptureStats stats;
  stats.device_buffer.present = true;
  stats.device_buffer.peak = 8194;
  stats.device_buffer.used_now = 3120;
  stats.device_buffer.packets_read = 1221;
  stats.device_buffer.depth_words = 16384;
  stats.device_buffer.packet_words = 8192;
  stats.device_buffer.near_full_words = 12288;

  panel.OnStatsUpdated(stats);

  auto* bar = panel.findChild<QProgressBar*>(
      QLatin1String(StatisticsPanel::kBackPressureBarName));
  ASSERT_NE(bar, nullptr);

  EXPECT_EQ(bar->value(), 50);
  EXPECT_TRUE(bar->format().contains(QStringLiteral("8194")))
      << bar->format().toStdString();

  // The tooltip becomes the figures behind the bar once there are some
  EXPECT_TRUE(bar->toolTip().contains(QStringLiteral("1221")))
      << bar->toolTip().toStdString();
}

TEST(StatisticsPanelTest, TheBackPressureBarSaysNothingWhenTheDeviceCannot) {
  // A device that cannot report its buffer must not look like one whose buffer
  // is untroubled — a fresh panel showing a confident zero would be the display
  // inventing a measurement.
  StatisticsPanel panel(nullptr);

  auto* bar = panel.findChild<QProgressBar*>(
      QLatin1String(StatisticsPanel::kBackPressureBarName));
  ASSERT_NE(bar, nullptr);

  EXPECT_EQ(bar->value(), 0);
  EXPECT_EQ(bar->format(), QString::fromUtf8("—"));
}

// The three capture-only rows. Blank while monitoring, because none of them
// describes anything that is happening: there is no encoder, and nothing has
// been written.
TEST(StatisticsPanelTest, TheCaptureOnlyRowsAreBlankWhileMonitoring) {
  StatisticsPanel panel(nullptr);

  ddd::capture::CaptureStats stats;
  stats.metrics.sample_count = 1'000'000;
  stats.writing = false;
  stats.bytes_written = 0;
  stats.samples_pending = 0;

  panel.OnStatsUpdated(stats);

  EXPECT_EQ(LabelNamed(panel, StatisticsPanel::kWrittenLabelName)->text(),
            QString::fromUtf8("—"));
  EXPECT_EQ(LabelNamed(panel, StatisticsPanel::kBacklogLabelName)->text(),
            QString::fromUtf8("—"));
}

TEST(StatisticsPanelTest, TheCaptureOnlyRowsFillInOnceAWriterIsAttached) {
  StatisticsPanel panel(nullptr);

  ddd::capture::CaptureStats stats;
  stats.metrics.sample_count = 1'000'000;
  stats.writing = true;
  stats.bytes_written = 50U << 20;
  stats.samples_pending = ddd::capture::kSampleRateHz / 20;

  panel.OnStatsUpdated(stats);

  EXPECT_TRUE(LabelNamed(panel, StatisticsPanel::kWrittenLabelName)
                  ->text()
                  .contains(QStringLiteral("50.0 MB")))
      << LabelNamed(panel, StatisticsPanel::kWrittenLabelName)
             ->text()
             .toStdString();
  EXPECT_TRUE(LabelNamed(panel, StatisticsPanel::kBacklogLabelName)
                  ->text()
                  .contains(QStringLiteral("50.0 ms")))
      << LabelNamed(panel, StatisticsPanel::kBacklogLabelName)
             ->text()
             .toStdString();
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
