/************************************************************************

    statistics_panel.h

    What the capture is doing, second by second
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QWidget>
#include <vector>

#include "capture_metatypes.h"
#include "front_end_gain.h"
#include "monitor_tap.h"
#include "statistics_presenter.h"
#include "usb_device_info.h"

class QLabel;
class QProgressBar;
class QTimer;

namespace ddd::gui {

class CaptureController;

// The panel that answers "is this working?".
//
// Every measured figure here comes from the wait-free tap, so reading them
// cannot slow the capture down — which is what makes it reasonable to show this
// much of it at 20 Hz. The choice of what to show is deliberate: throughput and
// buffer fill say whether the machine is keeping up, the sequence state says
// whether the data is intact, and the sample extremes say whether the RF gain
// is right. Those are the three questions a user actually has, and the old
// application could only answer the first.
//
// The panel itself holds no logic worth the name. Everything it displays is
// produced by PresentStatistics, which is where the figures can be tested
// without a display.
class StatisticsPanel : public QWidget {
  Q_OBJECT

 public:
  explicit StatisticsPanel(CaptureController* controller,
                           QWidget* parent = nullptr);

  static constexpr const char* kThroughputLabelName = "statistics_throughput";
  static constexpr const char* kSequenceLabelName = "statistics_sequence";
  static constexpr const char* kBufferBarName = "statistics_buffer_fill";
  static constexpr const char* kBackPressureBarName =
      "statistics_back_pressure";
  static constexpr const char* kAmplitudeLabelName = "statistics_amplitude";
  static constexpr const char* kExtremesLabelName = "statistics_extremes";
  static constexpr const char* kClippingLabelName = "statistics_clipping";
  static constexpr const char* kTransfersLabelName = "statistics_transfers";
  static constexpr const char* kSamplesLabelName = "statistics_samples";
  static constexpr const char* kElapsedLabelName = "statistics_elapsed";
  static constexpr const char* kWrittenLabelName = "statistics_written";
  static constexpr const char* kBacklogLabelName = "statistics_encoder_backlog";
  static constexpr const char* kSpaceLabelName = "statistics_space_remaining";
  static constexpr const char* kLinkSpeedLabelName = "statistics_link_speed";
  static constexpr const char* kGainLabelName = "statistics_front_end_gain";

  // See CapturePanel::kFreeSpaceIntervalMilliseconds — the same reasoning, and
  // deliberately the same figure, so the two panels never disagree about how
  // much room is left.
  static constexpr int kFreeSpaceIntervalMilliseconds = 2000;

 public slots:
  void OnStatsUpdated(const ddd::capture::CaptureStats& stats);
  void OnMonitoringChanged(bool monitoring);
  void OnDevicesChanged(const std::vector<ddd::capture::DeviceInfo>& devices);

 private:
  void Apply(const StatisticsView& view);
  void ShowIdle();
  void RefreshFreeSpace();

  // What a capture in the current settings is expected to cost on disk, which
  // is what turns free bytes into the time this panel actually shows. Falls
  // back to the FLAC estimate when there is no controller to ask.
  double EstimatedBytesPerSecond() const;

  // The rate the stream is running at, which is what turns the encoder's
  // backlog of samples into the length of signal it represents. Falls back to
  // the converter's own rate when there is no controller to ask.
  uint32_t SampleRateHz() const;

  QLabel* throughput_ = nullptr;
  QLabel* sequence_ = nullptr;
  QProgressBar* buffer_fill_ = nullptr;
  QProgressBar* back_pressure_ = nullptr;
  BackPressureHold back_pressure_hold_;
  QLabel* amplitude_ = nullptr;
  QLabel* extremes_ = nullptr;
  QLabel* clipping_ = nullptr;
  QLabel* transfers_ = nullptr;
  QLabel* samples_ = nullptr;
  QLabel* elapsed_ = nullptr;
  QLabel* written_ = nullptr;
  QLabel* backlog_ = nullptr;
  QLabel* space_ = nullptr;
  QLabel* link_speed_ = nullptr;
  QLabel* gain_ = nullptr;

  CaptureController* controller_ = nullptr;

  analysis::FrontEndGain declared_gain_;
  capture::DeviceSpeed link_ = capture::DeviceSpeed::kUnknown;

  // Refreshed on a timer of its own rather than with the statistics. The
  // statistics arrive twenty times a second and come from a wait-free tap that
  // costs nothing to read; this is a filesystem call, and nothing it reports
  // changes at that rate.
  capture::FreeSpace destination_space_;
  QTimer* space_timer_ = nullptr;
};

}  // namespace ddd::gui
