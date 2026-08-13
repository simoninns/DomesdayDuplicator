/************************************************************************

    statistics_panel.h

    What the capture is doing, second by second
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QWidget>

#include "monitor_tap.h"

class QLabel;
class QProgressBar;

namespace ddd::gui {

class CaptureController;

// The panel that answers "is this working?".
//
// Every figure here comes from the wait-free tap, so reading them cannot slow
// the capture down — which is what makes it reasonable to show this much of it
// at 20 Hz. The choice of what to show is deliberate: throughput and buffer
// fill say whether the machine is keeping up, the sequence state says whether
// the data is intact, and the sample extremes say whether the RF gain is right.
// Those are the three questions a user actually has, and the old application
// could only answer the first.
class StatisticsPanel : public QWidget {
  Q_OBJECT

 public:
  explicit StatisticsPanel(CaptureController* controller,
                           QWidget* parent = nullptr);

  static constexpr const char* kThroughputLabelName = "statistics_throughput";
  static constexpr const char* kSequenceLabelName = "statistics_sequence";
  static constexpr const char* kBufferBarName = "statistics_buffer_fill";
  static constexpr const char* kAmplitudeLabelName = "statistics_amplitude";
  static constexpr const char* kClippingLabelName = "statistics_clipping";
  static constexpr const char* kTransfersLabelName = "statistics_transfers";
  static constexpr const char* kElapsedLabelName = "statistics_elapsed";

 public slots:
  void OnStatsUpdated(const ddd::capture::CaptureStats& stats);
  void OnMonitoringChanged(bool monitoring);

 private:
  void Clear();

  QLabel* throughput_ = nullptr;
  QLabel* sequence_ = nullptr;
  QProgressBar* buffer_fill_ = nullptr;
  QLabel* amplitude_ = nullptr;
  QLabel* clipping_ = nullptr;
  QLabel* transfers_ = nullptr;
  QLabel* elapsed_ = nullptr;
};

// How a throughput figure is put to a user: megabytes per second alongside the
// sample rate it corresponds to.
//
// Both, because they answer different questions. MB/s is what a disk is
// specified in and what tells someone whether their storage can keep up; Msps
// is what the device is specified in and what tells them whether they are
// getting all of the signal. Showing only one leaves the other to be worked out
// with a calculator.
QString FormatThroughput(double bytes_per_second);

// A sample range as a proportion of the 10-bit scale, which is the form the
// number is actually used in: a user adjusting RF gain wants to know how much
// of the range is being used, not what the raw counts are.
QString FormatAmplitude(const ddd::capture::SampleMetricsSnapshot& metrics);

}  // namespace ddd::gui
