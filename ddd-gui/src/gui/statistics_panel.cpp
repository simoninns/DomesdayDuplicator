/************************************************************************

    statistics_panel.cpp

    What the capture is doing, second by second
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "statistics_panel.h"

#include <QFormLayout>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

#include "capture_controller.h"
#include "sample_format.h"

namespace ddd::gui {
namespace {

const char* kNoValue = "—";

QString DescribeSequenceState(const ddd::capture::CaptureStats& stats) {
  switch (stats.sequence_state) {
    case ddd::capture::SequenceState::kSynchronising:
      return StatisticsPanel::tr("Synchronising");
    case ddd::capture::SequenceState::kRunning:
      return StatisticsPanel::tr("Verified — no samples lost");
    case ddd::capture::SequenceState::kDisabled:
      return StatisticsPanel::tr(
          "Not available — this gateware does not send sequence markers");
    case ddd::capture::SequenceState::kFailed:
      return StatisticsPanel::tr("Broken — samples have been lost");
  }
  return StatisticsPanel::tr("Unknown");
}

}  // namespace

QString FormatThroughput(double bytes_per_second) {
  if (bytes_per_second <= 0.0) {
    return QString::fromUtf8(kNoValue);
  }

  const double megabytes = bytes_per_second / (1024.0 * 1024.0);
  const double megasamples =
      bytes_per_second / static_cast<double>(ddd::capture::kBytesPerSample) /
      1'000'000.0;

  return StatisticsPanel::tr("%1 MB/s  (%2 Msps)")
      .arg(megabytes, 0, 'f', 1)
      .arg(megasamples, 0, 'f', 2);
}

QString FormatAmplitude(const ddd::capture::SampleMetricsSnapshot& metrics) {
  if (metrics.sample_count == 0) {
    return QString::fromUtf8(kNoValue);
  }

  const double span = static_cast<double>(metrics.recent_maximum_value) -
                      static_cast<double>(metrics.recent_minimum_value);
  const double proportion =
      span / static_cast<double>(ddd::capture::kMaximumSampleValue) * 100.0;

  return StatisticsPanel::tr("%1 to %2 of 1023  (%3% of range)")
      .arg(metrics.recent_minimum_value)
      .arg(metrics.recent_maximum_value)
      .arg(proportion, 0, 'f', 1);
}

StatisticsPanel::StatisticsPanel(CaptureController* controller, QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);

  auto* form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignLeft);

  const auto add_row = [&](const QString& label, const char* object_name) {
    auto* value = new QLabel(QString::fromUtf8(kNoValue), this);
    value->setObjectName(QLatin1String(object_name));
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(label, value);
    return value;
  };

  throughput_ = add_row(tr("Throughput"), kThroughputLabelName);
  sequence_ = add_row(tr("Integrity"), kSequenceLabelName);
  sequence_->setWordWrap(true);

  buffer_fill_ = new QProgressBar(this);
  buffer_fill_->setObjectName(QLatin1String(kBufferBarName));
  buffer_fill_->setRange(0, 100);
  buffer_fill_->setValue(0);
  buffer_fill_->setFormat(QString::fromUtf8(kNoValue));
  buffer_fill_->setToolTip(
      tr("How much of the buffer queue is waiting to be processed. A healthy "
         "capture sits near zero; a figure that climbs and stays up means this "
         "machine is not keeping up."));
  form->addRow(tr("Buffer queue"), buffer_fill_);

  amplitude_ = add_row(tr("Signal level"), kAmplitudeLabelName);
  clipping_ = add_row(tr("Clipping"), kClippingLabelName);
  transfers_ = add_row(tr("Transfers"), kTransfersLabelName);
  elapsed_ = add_row(tr("Elapsed"), kElapsedLabelName);

  layout->addLayout(form);
  layout->addStretch();

  if (controller != nullptr) {
    connect(controller, &CaptureController::StatsUpdated, this,
            &StatisticsPanel::OnStatsUpdated);
    connect(controller, &CaptureController::MonitoringChanged, this,
            &StatisticsPanel::OnMonitoringChanged);
  }
}

void StatisticsPanel::OnStatsUpdated(const ddd::capture::CaptureStats& stats) {
  throughput_->setText(FormatThroughput(stats.throughput_bytes_per_second));
  sequence_->setText(DescribeSequenceState(stats));

  if (stats.slot_count > 0) {
    const int percent =
        static_cast<int>(stats.slots_in_use * 100 / stats.slot_count);
    buffer_fill_->setValue(percent);
    // The peak is in the text rather than a second widget because it is the
    // figure that matters after the fact: a capture that was fine except for
    // one stall thirty minutes in reads as perfect from the live value alone.
    buffer_fill_->setFormat(tr("%1 of %2 buffers  (peak %3)")
                                .arg(stats.slots_in_use)
                                .arg(stats.slot_count)
                                .arg(stats.peak_slots_in_use));
  }

  amplitude_->setText(FormatAmplitude(stats.metrics));

  if (stats.metrics.sample_count == 0) {
    clipping_->setText(QString::fromUtf8(kNoValue));
  } else {
    clipping_->setText(tr("%1 low, %2 high")
                           .arg(stats.metrics.clipped_low_count)
                           .arg(stats.metrics.clipped_high_count));
  }

  transfers_->setText(tr("%1 transfers, %2 buffers")
                          .arg(stats.transfers_completed)
                          .arg(stats.buffers_processed));

  elapsed_->setText(tr("%1 s").arg(stats.elapsed_seconds, 0, 'f', 1));
}

void StatisticsPanel::OnMonitoringChanged(bool monitoring) {
  if (monitoring) {
    Clear();
  }
}

void StatisticsPanel::Clear() {
  const QString none = QString::fromUtf8(kNoValue);
  throughput_->setText(none);
  sequence_->setText(none);
  amplitude_->setText(none);
  clipping_->setText(none);
  transfers_->setText(none);
  elapsed_->setText(none);
  buffer_fill_->setValue(0);
  buffer_fill_->setFormat(none);
}

}  // namespace ddd::gui
