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
#include <cstdint>
#include <vector>

#include "capture_metatypes.h"
#include "frequency_axis.h"
#include "sample_format.h"
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
  //
  // segments is how many were averaged, for the readout in the corner. Zero
  // means nobody said, and the readout then states the bandwidth alone rather
  // than inventing a figure for it.
  void SetSpectrum(const std::vector<double>& magnitudes_db,
                   const std::vector<double>& peak_hold_db,
                   const std::vector<double>& snapshot_db, size_t segments = 0);
  void SetPeakHoldVisible(bool visible);
  void SetView(SpectrumView view);

  // The top of the spectrogram's colour scale, and how far below it the scale
  // reaches. History is held as levels, so moving either re-colours every row
  // already on screen rather than only the rows drawn after the change.
  void SetSpectrogramReference(double reference_db);
  void SetSpectrogramRange(double range_db);

  // Whether frequency runs linearly or logarithmically. Applies to both views
  // at once: they are two pictures of the same measurement and a panel whose
  // trace and waterfall disagreed about where 1 MHz was would be worse than
  // either on its own.
  void SetFrequencyScale(analysis::FrequencyScale scale);

  // The rate the samples being plotted arrived at, which is what sets the top
  // of the frequency axis. A decimated stream is 20 Msps and its Nyquist is
  // 10 MHz, so a plot that assumed the converter's own rate would draw every
  // component at twice the frequency it is really at.
  //
  // Clears the history, because the columns already held span DC to the old
  // Nyquist: re-labelling them under a new axis would silently move every
  // carrier already on screen.
  void SetSampleRate(uint32_t sample_rate_hz);

  void Clear();

  bool peak_hold_visible() const { return peak_hold_visible_; }
  SpectrumView view() const { return view_; }
  analysis::FrequencyScale frequency_scale() const { return scale_; }
  double spectrogram_reference_db() const { return reference_db_; }
  double spectrogram_range_db() const { return range_db_; }
  uint32_t sample_rate_hz() const { return sample_rate_hz_; }

  // The top of the frequency axis.
  double NyquistHz() const {
    return static_cast<double>(sample_rate_hz_) / 2.0;
  }

  // The mapping the trace, the waterfall, the grid and both cursors all share.
  analysis::FrequencyAxis Axis() const;

  // The plot area, excluding the scale this widget draws down its left-hand
  // side and the axis it draws below.
  //
  // Public because everything that has to agree with the picture — the cursor,
  // the markers, and the tests that check where a carrier landed — needs the
  // same rectangle the painting used. The width of the scale follows from the
  // font, so a test that assumed a number here would be measuring the machine
  // it ran on.
  QRectF PlotArea() const;

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
  // Room for the level scale down the left-hand side, measured from the font
  // rather than fixed. "-100 dBFS" is wider than the 46 pixels this used to
  // reserve, and the label was silently clipped to "-100 dB" — which is not a
  // cosmetic fault on a display whose whole business is stating levels.
  double ScaleWidth() const;

  void PaintTrace(QPainter& painter, const QRectF& area,
                  const std::vector<double>& levels, const QColor& colour);
  void PaintSpectrogram(QPainter& painter, const QRectF& area, bool dark);
  void PaintGrid(QPainter& painter, const QRectF& area);

  // An annotation, with the window's own background behind it.
  //
  // Backed rather than drawn straight onto the picture because every one of
  // these sits over something: a trace, a colour ramp, or the part of a
  // waterfall that happens to be brightest. Text the same lightness as what is
  // behind it is not dimmer, it is absent.
  void PaintLabel(QPainter& painter, const QRectF& box, Qt::Alignment alignment,
                  const QString& text, const QColor& ink);

  // What the instrument is set to, in the corner of the plot: the resolution
  // bandwidth and how many segments were averaged for it. An analyser states
  // this because every level on the screen depends on it — the same noise
  // floor reads six decibels lower at half the bandwidth, and without the
  // figure beside it a level is not a measurement.
  void PaintAnnotation(QPainter& painter, const QRectF& area);

  // Where the board's anti-aliasing filter turns over. Drawn in both views,
  // because the roll-off above it is the one part of either picture that is
  // the hardware rather than the signal, and a reader who does not know that
  // is looking at a carrier that falls away and wondering what happened.
  void PaintFilterCorner(QPainter& painter, const QRectF& area, bool dark);

  // The strongest thing on the trace, named. The norm is that an analyser
  // says what the biggest peak is without being asked: the cursor answers
  // "what is here", and this answers "what is there", which is the question
  // somebody watching a player being adjusted actually has.
  void PaintPeakMarker(QPainter& painter, const QRectF& area);

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
  bool peak_hold_visible_ = false;

  // Segments behind the levels above, and zero before anything has said.
  size_t segments_ = 0;

  SpectrumView view_ = SpectrumView::kTrace;
  analysis::FrequencyScale scale_ = analysis::FrequencyScale::kLogarithmic;

  // The converter's own rate until told otherwise, which is what an undecimated
  // capture runs at and therefore the right thing to draw before any settings
  // have arrived.
  uint32_t sample_rate_hz_ = capture::kSampleRateHz;

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
  // segments defaults so that a caller with nothing to say about it — a test
  // feeding levels, or any future producer that does not average — is not made
  // to invent one. The readout leaves the figure out rather than showing zero.
  void OnSpectrumReady(const std::vector<double>& magnitudes_db,
                       const std::vector<double>& peak_hold_db,
                       const std::vector<double>& snapshot_db,
                       size_t segments = 0);
  void OnMonitoringChanged(bool monitoring);

  // Put the plot and the resolution list on the stream's real rate. The list
  // is rebuilt rather than only re-read, because every entry in it names a bin
  // width and every one of those halves with the rate.
  //
  // Public because it is how the panel is told about a setting it does not
  // own, and it is what the controller's SettingsChanged is connected to.
  void SetSampleRate(uint32_t sample_rate_hz);

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
//
// The rate is a parameter because the bins are a fraction of it: the same
// transform over a decimated stream has bins half as wide, and a label that
// assumed the converter's own rate would overstate the resolution on offer by a
// factor of two.
QString FormatSpectrumResolution(size_t transform_size,
                                 uint32_t sample_rate_hz);

// The instrument's settings as the corner of the plot states them:
// "RBW 14.6 kHz · 15 avg", or the bandwidth alone when the segment count is
// not known.
//
// The bandwidth quoted is the resolution bandwidth and not the bin spacing —
// 14.6 kHz for a 4,096-point transform, where the bins are 9.8 kHz apart. That
// is the figure an analyser states because it is the one the noise floor
// follows: the Hann window collects from half again wider than a bin, and a
// panel that quoted the spacing here would be understating its own bandwidth
// by 1.8 dB. The Resolution control still names the spacing, because that is
// what the choice being made there is about.
QString FormatResolutionBandwidth(size_t transform_size, size_t segments,
                                  uint32_t sample_rate_hz);

// A level as the scale down the side states it: "0 dBFS", "-100 dBFS".
//
// Full scale is a full-scale sine wave, which is the reference the whole panel
// is normalised to, and saying so on every mark is what stops a level being
// read as decibels of something else.
QString FormatLevelTick(double level_db);

}  // namespace ddd::gui
