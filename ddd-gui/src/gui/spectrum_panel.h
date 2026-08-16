/************************************************************************

    spectrum_panel.h

    What the signal is made of
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QWidget>
#include <vector>

#include "capture_metatypes.h"
#include "spectrogram_history.h"
#include "spectrum_analyser.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

namespace ddd::gui {

class CaptureController;

// How the frequency content is drawn.
enum class SpectrumView {
  // Log magnitude against frequency: what is there now.
  kTrace,

  // Magnitude as colour, frequency across and time down: what has been there.
  kSpectrogram,
};

// The spectrum display, in either of its two forms.
class SpectrumPlot : public QWidget {
  Q_OBJECT

 public:
  explicit SpectrumPlot(QWidget* parent = nullptr);

  void SetSpectrum(const std::vector<double>& magnitudes_db,
                   const std::vector<double>& peak_hold_db);
  void SetPeakHoldVisible(bool visible);
  void SetView(SpectrumView view);

  // The top of the displayed range. History is kept across the whole span the
  // converter reaches, so narrowing this re-draws what is already held at
  // higher resolution rather than discarding it.
  void SetMaximumFrequency(double frequency_hz);

  void Clear();

  bool peak_hold_visible() const { return peak_hold_visible_; }
  SpectrumView view() const { return view_; }
  double maximum_frequency_hz() const { return maximum_frequency_hz_; }

  const analysis::SpectrogramHistory& history() const { return history_; }

 signals:
  // seconds_ago is negative in the trace view, where the reading is of now and
  // there is no age to give.
  void CursorMoved(double frequency_hz, double level_db, double seconds_ago);
  void CursorLeft();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  // The plot area, excluding the axis labels this widget draws itself.
  QRectF PlotArea() const;

  void PaintTrace(QPainter& painter, const QRectF& area,
                  const std::vector<double>& levels, const QColor& colour);
  void PaintSpectrogram(QPainter& painter, const QRectF& area, bool dark);
  void PaintGrid(QPainter& painter, const QRectF& area);

  // The proportion of the whole DC-to-Nyquist span currently displayed.
  double DisplayedProportion() const;

  std::vector<double> magnitudes_db_;
  std::vector<double> peak_hold_db_;
  bool peak_hold_visible_ = true;

  SpectrumView view_ = SpectrumView::kTrace;
  double maximum_frequency_hz_ = analysis::kDefaultMaximumFrequencyHz;

  analysis::SpectrogramHistory history_;

  // Started at the first frame of a run and read for each one after, which is
  // what lets the time axis be labelled in seconds rather than in a direction.
  // Measured rather than assumed: nothing tells this panel how often the
  // pipeline publishes a snapshot, and the answer depends on the ring geometry
  // the run was started with.
  QElapsedTimer clock_;

  // The history rendered at one pixel per cell, scaled to the plot when drawn.
  // Rebuilt rather than scrolled, because a theme change and a change of
  // frequency range both re-colour every row that is already held — and at
  // 1,024 by 300 cells nine times a second that is a rounding error against the
  // transform that produced them.
  QImage spectrogram_;
  bool spectrogram_valid_ = false;
};

// The spectrum panel: the plot, the averaging control, peak hold and a
// frequency readout.
class SpectrumPanel : public QWidget {
  Q_OBJECT

 public:
  explicit SpectrumPanel(CaptureController* controller,
                         QWidget* parent = nullptr);

  static constexpr const char* kPlotName = "spectrum_plot";
  static constexpr const char* kAveragingComboName = "spectrum_averaging_combo";
  static constexpr const char* kResolutionComboName =
      "spectrum_resolution_combo";
  static constexpr const char* kPeakHoldBoxName = "spectrum_peak_hold_box";
  static constexpr const char* kResetButtonName = "spectrum_reset_button";
  static constexpr const char* kCursorLabelName = "spectrum_cursor_label";
  static constexpr const char* kViewComboName = "spectrum_view_combo";
  static constexpr const char* kMaximumFrequencyComboName =
      "spectrum_maximum_frequency_combo";

 signals:
  // How much time the spectrogram is showing, once it has measured its own
  // frame rate. Emitted for whoever wants to keep pace with it — the Amplitude
  // panel does — and not for anything inside this panel.
  void TimeWindowChanged(double seconds);

 public slots:
  void OnSpectrumReady(const std::vector<double>& magnitudes_db,
                       const std::vector<double>& peak_hold_db);
  void OnMonitoringChanged(bool monitoring);

 private:
  void ApplyAveraging();
  void ApplyResolution();
  void ShowCursor(double frequency_hz, double level_db, double seconds_ago);
  void ClearCursor();

  void ApplyView();

  CaptureController* controller_ = nullptr;

  // The last window announced, so a frame that did not move it materially does
  // not announce anything: this would otherwise fire at the snapshot rate and
  // redraw another panel every time.
  double announced_window_seconds_ = 0.0;

  SpectrumPlot* plot_ = nullptr;
  QComboBox* view_ = nullptr;
  QComboBox* maximum_frequency_ = nullptr;
  QComboBox* resolution_ = nullptr;
  QComboBox* averaging_ = nullptr;
  QCheckBox* peak_hold_ = nullptr;
  QPushButton* reset_ = nullptr;
  QLabel* cursor_ = nullptr;
};

// A frequency put to a user in the units the number is comfortable in: kHz
// below a megahertz, MHz above it.
QString FormatFrequency(double frequency_hz);

// The cursor readout: where in the spectrum, how much is there, and — in the
// spectrogram, where a reading is of a moment rather than of now — how long
// ago. A negative age leaves the time out.
QString FormatSpectrumCursor(double frequency_hz, double level_db,
                             double seconds_ago = -1.0);

// A duration in the past, as the time axis and the cursor give it: "12.34 s"
// at most two decimal places, and no more precision than the figure deserves.
QString FormatSecondsAgo(double seconds);

// A transform size as the resolution it buys, which is the half of the trade a
// user can act on: "9.8 kHz bins" rather than "4096 points".
QString FormatSpectrumResolution(size_t transform_size);

}  // namespace ddd::gui
