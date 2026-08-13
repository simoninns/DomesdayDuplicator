/************************************************************************

    waveform_panel.h

    The scope: what the signal is doing right now
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QImage>
#include <QWidget>
#include <cstdint>
#include <vector>

#include "capture_metatypes.h"
#include "front_end_gain.h"
#include "waveform_mapping.h"

class QCheckBox;
class QComboBox;
class QLabel;

namespace ddd::gui {

class CaptureController;

// The time-domain trace, painted with QPainter and nothing else.
//
// Deliberately not a charting library. What this has to do is draw a min/max
// column per pixel, thirty times a second, in the window's own colours — and
// every charting library that could do it brings a licence to check, a
// dependency to package on three platforms and an abstraction that would have
// to be fought to get the persistence mode. The whole of it is one paintEvent.
class WaveformPlot : public QWidget {
  Q_OBJECT

 public:
  explicit WaveformPlot(QWidget* parent = nullptr);

  void SetCodes(const std::vector<uint16_t>& codes);
  void SetSampleSpan(size_t span);
  void SetPersistence(bool enabled);
  void Clear();

  size_t sample_span() const { return sample_span_; }
  bool persistence() const { return persistence_; }

 signals:
  // The sample and code under the pointer. Emitted with the sample index into
  // the snapshot, which is what the panel turns into a time offset.
  void CursorMoved(qint64 sample_index, double code);
  void CursorLeft();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

 private:
  analysis::WaveformMapping Mapping() const;
  void PaintTrace(QPainter& painter, const analysis::WaveformMapping& mapping,
                  const QColor& colour);

  std::vector<uint16_t> codes_;
  std::vector<analysis::WaveformColumn> columns_;

  size_t sample_span_ = analysis::kWaveformSpanChoices[2];
  bool persistence_ = false;

  // The accumulated picture, kept only while persistence is on. Every frame
  // fades what is there and draws over it, which is how a repeating waveform
  // builds up a bright envelope and a transient shows as a faint one — the
  // same thing an analogue scope's phosphor did, and the reason it is worth
  // having at all.
  QImage persistence_image_;
};

// The scope panel: the plot, the span control, and the cursor readout.
class WaveformPanel : public QWidget {
  Q_OBJECT

 public:
  explicit WaveformPanel(CaptureController* controller,
                         QWidget* parent = nullptr);

  static constexpr const char* kPlotName = "waveform_plot";
  static constexpr const char* kSpanComboName = "waveform_span_combo";
  static constexpr const char* kPersistenceBoxName = "waveform_persistence_box";
  static constexpr const char* kCursorLabelName = "waveform_cursor_label";

 public slots:
  void OnWaveformReady(const std::vector<uint16_t>& codes);
  void OnMonitoringChanged(bool monitoring);
  void SetFrontEndGain(analysis::FrontEndGain gain);

 private:
  void ShowCursor(qint64 sample_index, double code);
  void ClearCursor();

  WaveformPlot* plot_ = nullptr;
  QComboBox* span_ = nullptr;
  QCheckBox* persistence_ = nullptr;
  QLabel* cursor_ = nullptr;

  analysis::FrontEndGain gain_;
};

// A span in samples, put to a user as the time it covers.
QString FormatWaveformSpan(size_t samples);

// The cursor readout: where in the snapshot, and what the sample was — in
// converter codes always, and in millivolts as well when the gain has been
// declared.
QString FormatWaveformCursor(qint64 sample_index, double code,
                             const analysis::FrontEndGain& gain);

}  // namespace ddd::gui
