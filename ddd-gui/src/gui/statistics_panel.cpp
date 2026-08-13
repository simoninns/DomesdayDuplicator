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
#include <QScrollArea>
#include <QVBoxLayout>

#include "capture_controller.h"

namespace ddd::gui {

StatisticsPanel::StatisticsPanel(CaptureController* controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
  // Inside a scroll area, because this panel is a list that grows: it went from
  // seven rows to twelve with Phase 4 and will gain three more when there is a
  // writer to describe. Without one its minimum height is the height of every
  // row, which it then demands from the dock column it shares — and the
  // separators above and below it stop moving.
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);

  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  outer->addWidget(scroll);

  auto* contents = new QWidget(scroll);
  scroll->setWidget(contents);

  auto* layout = new QVBoxLayout(contents);
  layout->setContentsMargins(12, 12, 12, 12);

  auto* form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignLeft);

  // Word wrap is opt-in rather than the default. A wrapped label's minimum
  // height grows as the panel narrows, so wrapping every row makes the whole
  // panel demand more vertical space the narrower it gets — which is the
  // opposite of what a dock that can be dragged narrow needs. Only the rows
  // that carry a sentence get it.
  const auto add_row = [&](const QString& label, const char* object_name,
                           bool wrap = false) {
    auto* value = new QLabel(contents);
    value->setObjectName(QLatin1String(object_name));
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setWordWrap(wrap);
    form->addRow(label, value);
    return value;
  };

  throughput_ = add_row(tr("Throughput"), kThroughputLabelName);
  sequence_ = add_row(tr("Integrity"), kSequenceLabelName, true);

  buffer_fill_ = new QProgressBar(contents);
  buffer_fill_->setObjectName(QLatin1String(kBufferBarName));
  buffer_fill_->setRange(0, 100);
  buffer_fill_->setValue(0);
  buffer_fill_->setToolTip(
      tr("How much of the buffer queue is waiting to be processed. A healthy "
         "capture sits near zero; a figure that climbs and stays up means this "
         "machine is not keeping up."));
  form->addRow(tr("Buffer queue"), buffer_fill_);

  amplitude_ = add_row(tr("Signal level"), kAmplitudeLabelName);
  extremes_ = add_row(tr("Extremes"), kExtremesLabelName);
  clipping_ = add_row(tr("Clipping"), kClippingLabelName);
  transfers_ = add_row(tr("Transfers"), kTransfersLabelName);
  samples_ = add_row(tr("Samples"), kSamplesLabelName);
  elapsed_ = add_row(tr("Elapsed"), kElapsedLabelName);
  written_ = add_row(tr("Written"), kWrittenLabelName);
  link_speed_ = add_row(tr("Link speed"), kLinkSpeedLabelName);
  gain_ = add_row(tr("Front-end gain"), kGainLabelName, true);

  layout->addLayout(form);
  layout->addStretch();

  if (controller_ != nullptr) {
    declared_gain_ = controller_->settings().DeclaredGain();

    connect(controller_, &CaptureController::StatsUpdated, this,
            &StatisticsPanel::OnStatsUpdated);
    connect(controller_, &CaptureController::MonitoringChanged, this,
            &StatisticsPanel::OnMonitoringChanged);
    connect(controller_, &CaptureController::DevicesChanged, this,
            &StatisticsPanel::OnDevicesChanged);
    connect(controller_, &CaptureController::SettingsChanged, this,
            [this](const CaptureSettings& settings) {
              declared_gain_ = settings.DeclaredGain();
              // Only the labels change. Nothing is re-measured, and a run in
              // progress does not notice.
              ShowIdle();
            });
  }

  ShowIdle();
}

void StatisticsPanel::OnStatsUpdated(const ddd::capture::CaptureStats& stats) {
  Apply(PresentStatistics(stats, declared_gain_, link_));
}

void StatisticsPanel::OnMonitoringChanged(bool monitoring) {
  if (monitoring) {
    ShowIdle();
  }
}

void StatisticsPanel::OnDevicesChanged(
    const std::vector<ddd::capture::DeviceInfo>& devices) {
  const QString preferred = controller_ != nullptr
                                ? controller_->settings().preferred_device_path
                                : QString();

  const capture::DeviceInfo* const selected =
      capture::SelectDevice(devices, preferred.toStdString());

  link_ =
      selected != nullptr ? selected->speed : capture::DeviceSpeed::kUnknown;
  gain_->setText(PresentIdleStatistics(declared_gain_, link_).front_end_gain);
  link_speed_->setText(PresentIdleStatistics(declared_gain_, link_).link_speed);
}

void StatisticsPanel::ShowIdle() {
  Apply(PresentIdleStatistics(declared_gain_, link_));
}

void StatisticsPanel::Apply(const StatisticsView& view) {
  throughput_->setText(view.throughput);
  sequence_->setText(view.integrity);
  amplitude_->setText(view.signal_level);
  extremes_->setText(view.extremes);
  clipping_->setText(view.clipping);
  transfers_->setText(view.transfers);
  samples_->setText(view.samples);
  elapsed_->setText(view.elapsed);
  written_->setText(view.bytes_written);
  link_speed_->setText(view.link_speed);
  gain_->setText(view.front_end_gain);

  buffer_fill_->setValue(view.buffer_percent);
  buffer_fill_->setFormat(view.buffer);
}

}  // namespace ddd::gui
