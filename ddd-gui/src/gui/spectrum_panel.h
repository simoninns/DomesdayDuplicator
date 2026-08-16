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
#include "frequency_axis.h"
#include "spectrogram_history.h"
#include "spectrum_analyser.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

namespace ddd::gui {

class CaptureController;

// The waterfall's colour scale, as the two figures a user sets.
//
// A full-scale reference and a hundred decibels of range is the whole of what
// the converter can represent, and it is the right default because it makes no
// assumption about the signal. It is often not what you want to look at: the
// texture that distinguishes a healthy capture from a marginal one lives in a
// thirty-decibel slice somewhere in the middle, and spread over a hundred that
// slice is three shades of one colour.
inline constexpr double kDefaultSpectrogramReferenceDb = 0.0;
inline constexpr double kDefaultSpectrogramRangeDb = 100.0;

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

  // The trace draws the averaged levels and their peak hold; the spectrogram
  // records snapshot_db, which is this transform alone. Passing one vector for
  // both would put the trace's averaging into the waterfall's time axis.
  void SetSpectrum(const std::vector<double>& magnitudes_db,
                   const std::vector<double>& peak_hold_db,
                   const std::vector<double>& snapshot_db);
  void SetPeakHoldVisible(bool visible);
  void SetView(SpectrumView view);

  // The top of the spectrogram's colour scale, and how far below it the scale
  // reaches. History is held as levels, so moving either re-colours every row
  // already on screen rather than only the rows drawn after the change.
  void SetSpectrogramReference(double reference_db);
  void SetSpectrogramRange(double range_db);

  // The top of the displayed range. History is kept across the whole span the
  // converter reaches, so narrowing this re-draws what is already held at
  // higher resolution rather than discarding it.
  void SetMaximumFrequency(double frequency_hz);

  // Whether frequency runs linearly or logarithmically. Applies to both views
  // at once: they are two pictures of the same measurement and a panel whose
  // trace and waterfall disagreed about where 1 MHz was would be worse than
  // either on its own.
  void SetFrequencyScale(analysis::FrequencyScale scale);

  void Clear();

  bool peak_hold_visible() const { return peak_hold_visible_; }
  SpectrumView view() const { return view_; }
  double maximum_frequency_hz() const { return maximum_frequency_hz_; }
  analysis::FrequencyScale frequency_scale() const { return scale_; }
  double spectrogram_reference_db() const { return reference_db_; }
  double spectrogram_range_db() const { return range_db_; }

  // The mapping the trace, the waterfall, the grid and both cursors all share.
  analysis::FrequencyAxis Axis() const;

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

  // A theme change re-colours every row already held. Without this the
  // waterfall keeps the old palette until the next frame arrives, which during
  // a run is a tenth of a second and after one is for ever.
  void changeEvent(QEvent* event) override;

 private:
  // The plot area, excluding the axis labels this widget draws itself.
  QRectF PlotArea() const;

  void PaintTrace(QPainter& painter, const QRectF& area,
                  const std::vector<double>& levels, const QColor& colour);
  void PaintSpectrogram(QPainter& painter, const QRectF& area, bool dark);
  void PaintGrid(QPainter& painter, const QRectF& area);

  // The level drawn in one pixel column: the highest bin that column covers.
  //
  // Shared by the trace and the cursor rather than written out twice, so the
  // reading under the pointer is by construction the reading on the screen. The
  // two used to be computed separately and agreed only because the axis was
  // linear; on a log axis they would have drifted apart across the width, and a
  // cursor that is wrong by a varying amount reads as a measurement rather than
  // as a fault.
  static double ColumnLevel(const std::vector<double>& levels,
                            const analysis::FrequencyAxis& axis, int column,
                            int columns);

  // Where a level sits on the waterfall's colour scale, 0 at the bottom of the
  // displayed range and 1 at the reference.
  double SpectrogramProportion(double level_db) const;

  // The band range each pixel row of the waterfall covers. Worked out once per
  // rebuild rather than per column, because it depends only on the height and
  // the axis and both are fixed for the life of the picture.
  struct BandRange {
    size_t first = 0;
    size_t last = 0;
  };

  void MapSpectrogramRows(int height);

  // Colour one held frame into one pixel column.
  void RenderSpectrogramColumn(int column, size_t frame, bool dark);

  // Re-colour every frame the history holds, into the right-hand end of the
  // picture. For a resize, a scale change, a contrast change or a theme change.
  void RebuildSpectrogram(int height, bool dark);

  // Shift the picture left and colour the newest frames in at the right. What
  // an ordinary frame costs: one column rather than all three hundred.
  void ScrollSpectrogram(int columns, bool dark);

  std::vector<double> magnitudes_db_;
  std::vector<double> peak_hold_db_;
  bool peak_hold_visible_ = true;

  SpectrumView view_ = SpectrumView::kTrace;
  double maximum_frequency_hz_ = analysis::kDefaultMaximumFrequencyHz;
  analysis::FrequencyScale scale_ = analysis::FrequencyScale::kLogarithmic;

  double reference_db_ = kDefaultSpectrogramReferenceDb;
  double range_db_ = kDefaultSpectrogramRangeDb;

  analysis::SpectrogramHistory history_;

  // Started at the first frame of a run and read for each one after, which is
  // what lets the time axis be labelled in seconds rather than in a direction.
  // Measured rather than assumed: nothing tells this panel how often the
  // pipeline publishes a snapshot, and the answer depends on the ring geometry
  // the run was started with.
  QElapsedTimer clock_;

  // The waterfall, one pixel column per frame and one pixel row per row of the
  // plot, held at the history's full capacity and drawn from its right-hand
  // end. Frames arrive one at a time and the picture is scrolled to match, so
  // the ordinary case costs one column of colour; the whole thing is re-made
  // only when something changes what a level looks like or where a frequency
  // sits, and the history is kept as levels precisely so that it can be.
  QImage spectrogram_;
  std::vector<BandRange> spectrogram_rows_;
  bool spectrogram_valid_ = false;

  // Frames appended since the picture was last brought up to date. Counted
  // rather than applied on arrival because the colour scheme and the plot's
  // height are painting concerns and are only known to be current at paint
  // time — and because a widget nobody is looking at should not be colouring
  // pixels at nine frames a second.
  int pending_frames_ = 0;
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
  static constexpr const char* kLogFrequencyBoxName =
      "spectrum_log_frequency_box";
  static constexpr const char* kReferenceComboName = "spectrum_reference_combo";
  static constexpr const char* kRangeComboName = "spectrum_range_combo";

 signals:
  // How much time the spectrogram is showing, once it has measured its own
  // frame rate. Emitted for whoever wants to keep pace with it — the Amplitude
  // panel does — and not for anything inside this panel.
  void TimeWindowChanged(double seconds);

 public slots:
  void OnSpectrumReady(const std::vector<double>& magnitudes_db,
                       const std::vector<double>& peak_hold_db,
                       const std::vector<double>& snapshot_db);
  void OnMonitoringChanged(bool monitoring);

 private:
  void ApplyAveraging();
  void ApplyResolution();
  void ApplyFrequencyScale(bool logarithmic);
  void ApplyContrast();
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
  QCheckBox* log_frequency_ = nullptr;
  QComboBox* reference_ = nullptr;
  QComboBox* range_ = nullptr;
  QCheckBox* peak_hold_ = nullptr;
  QPushButton* reset_ = nullptr;

  // The labels beside the two contrast combos, kept so that they can be shown
  // and hidden with the controls they name.
  QLabel* reference_label_ = nullptr;
  QLabel* range_label_ = nullptr;
  QLabel* cursor_ = nullptr;
};

// A frequency put to a user in the units the number is comfortable in: kHz
// below a megahertz, MHz above it.
QString FormatFrequency(double frequency_hz);

// A gridline's label. Megahertz throughout and without a unit, which is how
// this axis has always been marked: "0.5", "1", "10".
QString FormatAxisTick(double frequency_hz);

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
