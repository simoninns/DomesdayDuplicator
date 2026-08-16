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
#include <QTimer>
#include <QVBoxLayout>

#include "capture_controller.h"
#include "free_space.h"

namespace ddd::gui {
namespace {

// What the bar means before there is a reading to describe. Once one arrives,
// the tooltip becomes the figures behind it.
QString BackPressureTip() {
  return StatisticsPanel::tr(
      "How full the FPGA's capture buffer got. A packet is offered to the FX3 "
      "once half the buffer is queued and taken immediately, so a working "
      "capture fills it to about half on every packet — that is the buffer "
      "working, not a warning. Above half is the host having been late, and "
      "the top of the scale is the buffer full, which means samples were "
      "lost.");
}

}  // namespace

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

  // The device's buffer, then this machine's, in the order the samples travel
  // through them. Read together they say which end of the chain is struggling:
  // the device's buffer fills when the host cannot take packets fast enough,
  // and the host's queue fills when the disk or the encoder cannot keep up.
  back_pressure_ = new QProgressBar(contents);
  back_pressure_->setObjectName(QLatin1String(kBackPressureBarName));
  back_pressure_->setRange(0, 100);
  back_pressure_->setValue(0);
  back_pressure_->setToolTip(BackPressureTip());
  form->addRow(tr("Back pressure"), back_pressure_);

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

  backlog_ = add_row(tr("Encoder backlog"), kBacklogLabelName);
  backlog_->setToolTip(
      tr("Samples the FLAC encoder has taken but not yet written out. A steady "
         "figure is the encoder keeping pace; one that climbs is the encoder "
         "falling behind, which is a lower compression level rather than a "
         "faster drive."));

  space_ = add_row(tr("Space left"), kSpaceLabelName, true);
  space_->setToolTip(
      tr("How much longer the destination volume will hold a capture, at the "
         "estimate of 40 MB/s a FLAC capture uses."));

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

  space_timer_ = new QTimer(this);
  space_timer_->setInterval(kFreeSpaceIntervalMilliseconds);
  connect(space_timer_, &QTimer::timeout, this,
          &StatisticsPanel::RefreshFreeSpace);
  space_timer_->start();
  RefreshFreeSpace();

  ShowIdle();
}

void StatisticsPanel::OnStatsUpdated(const ddd::capture::CaptureStats& stats) {
  Apply(PresentStatistics(stats, declared_gain_, link_, destination_space_));
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
  const StatisticsView idle =
      PresentIdleStatistics(declared_gain_, link_, destination_space_);
  gain_->setText(idle.front_end_gain);
  link_speed_->setText(idle.link_speed);
}

void StatisticsPanel::ShowIdle() {
  // Nothing is draining the device when no capture is running, so its buffer
  // sits full and any figure would be a lie. The hold is cleared with it, or
  // the last run's worst moment would linger over the next one.
  back_pressure_hold_.Reset();
  Apply(PresentIdleStatistics(declared_gain_, link_, destination_space_));
}

void StatisticsPanel::RefreshFreeSpace() {
  if (controller_ == nullptr) {
    return;
  }

  destination_space_ = capture::AvailableSpace(
      controller_->settings().ResolvedCaptureDirectory().toStdString());

  // Only the one row, and only when nothing is running. During a run the next
  // statistics tick redraws everything within fifty milliseconds anyway, and
  // rebuilding the whole view here as well would be the same work twice.
  space_->setText(FormatSpaceRemaining(destination_space_));
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
  backlog_->setText(view.encoder_backlog);
  space_->setText(view.space_remaining);
  link_speed_->setText(view.link_speed);
  gain_->setText(view.front_end_gain);

  buffer_fill_->setValue(view.buffer_percent);
  buffer_fill_->setFormat(view.buffer);

  // Held rather than shown directly: a reading covers a quarter of a second and
  // reports the worst moment in it, so a single bad interval would otherwise be
  // a flash too brief to see.
  back_pressure_->setValue(
      back_pressure_hold_.Apply(view.back_pressure_percent));
  back_pressure_->setFormat(view.back_pressure);

  // The tooltip carries the figures the caption has no room for, and it changes
  // with them: a user who has just seen the bar move wants the detail of that
  // reading, not a paragraph written before the capture started.
  back_pressure_->setToolTip(view.back_pressure_detail.isEmpty()
                                 ? BackPressureTip()
                                 : view.back_pressure_detail);
}

}  // namespace ddd::gui
