/************************************************************************

    amplitude_panel.h

    Signal level over minutes, which is where a fault shows up
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QWidget>

#include "amplitude_history.h"
#include "capture_metatypes.h"
#include "front_end_gain.h"
#include "monitor_tap.h"

class QComboBox;
class QLabel;
class QPushButton;

namespace ddd::gui {

class CaptureController;

// The history strip: min/max envelope, RMS through the middle, clip ticks.
class AmplitudePlot : public QWidget {
  Q_OBJECT

 public:
  explicit AmplitudePlot(QWidget* parent = nullptr);

  // Borrowed, not copied. The history is appended to at ten points a second and
  // holds three thousand of them; copying it into the plot on every append
  // would be the most expensive thing this panel did, for a picture that is
  // redrawn from it anyway.
  void SetHistory(const analysis::AmplitudeHistory* history);

  void SetFrontEndGain(analysis::FrontEndGain gain);

  // How much of the past to draw, in seconds. Zero or less means everything
  // held, which is the ring's whole five minutes once it has filled.
  void SetWindowSeconds(double seconds);

  double window_seconds() const { return window_seconds_; }

  void Refresh() { update(); }

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  const analysis::AmplitudeHistory* history_ = nullptr;
  analysis::FrontEndGain gain_;
  double window_seconds_ = 0.0;
};

// The amplitude history panel.
class AmplitudePanel : public QWidget {
  Q_OBJECT

 public:
  explicit AmplitudePanel(CaptureController* controller,
                          QWidget* parent = nullptr);

  static constexpr const char* kPlotName = "amplitude_plot";
  static constexpr const char* kSummaryLabelName = "amplitude_summary_label";
  static constexpr const char* kGainLabelName = "amplitude_gain_label";
  static constexpr const char* kClearButtonName = "amplitude_clear_button";
  static constexpr const char* kSpanComboName = "amplitude_span_combo";

  // For tests: what the panel has accumulated.
  const analysis::AmplitudeHistory& history() const { return history_; }

 public slots:
  void OnStatsUpdated(const ddd::capture::CaptureStats& stats);
  void OnMonitoringChanged(bool monitoring);
  void SetFrontEndGain(analysis::FrontEndGain gain);

  // Start the history again from now, leaving the run alone.
  void ClearHistory();

  // The span the Spectrum panel's spectrogram is showing, so this panel can be
  // asked to match it. Supplied from outside rather than measured here: the two
  // panels do not know about each other, and the window that owns both is where
  // they are related.
  void SetMatchedWindowSeconds(double seconds);

 private:
  void UpdateSummary();
  void ApplySpan();

  analysis::AmplitudeHistory history_;
  double matched_window_seconds_ = 0.0;
  analysis::AmplitudeSampler sampler_;
  analysis::FrontEndGain gain_;

  AmplitudePlot* plot_ = nullptr;
  QComboBox* span_ = nullptr;
  QLabel* summary_ = nullptr;
  QLabel* gain_label_ = nullptr;
  QPushButton* clear_ = nullptr;
};

// The level summary: the range the window covers, in codes and — when the gain
// has been declared — in millivolts, with the clip count.
QString FormatAmplitudeSummary(const analysis::AmplitudeHistory& history,
                               const analysis::FrontEndGain& gain);

}  // namespace ddd::gui
