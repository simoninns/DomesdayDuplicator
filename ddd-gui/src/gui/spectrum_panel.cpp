/************************************************************************

    spectrum_panel.cpp

    What the signal is made of
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "spectrum_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <cstring>

#include "capture_controller.h"
#include "sample_format.h"
#include "spectrum_analyser.h"
#include "theme_color_tokens.h"

namespace ddd::gui {
namespace {

constexpr int kAxisHeightPixels = 20;
constexpr int kPlotMarginPixels = 6;

// The gap between the level scale's text and the plot it labels.
constexpr double kScaleTextGapPixels = 6.0;

// How far the annotations sit inside the plot's corners.
constexpr double kAnnotationInsetPixels = 4.0;

// The radius of the peak marker's dot, in pixels.
constexpr double kPeakMarkerRadius = 3.0;

// The display range. The analyser's own floor is -120 dB, which is below
// anything a 10-bit converter can show: its quantisation noise sits around
// -90 dB, so a scale running to -120 would spend a quarter of its height on
// empty space.
constexpr double kTopDecibels = 0.0;
constexpr double kBottomDecibels = -100.0;
constexpr double kDecibelsPerGridLine = 20.0;

// How many gridlines that gives, counting both ends. Every loop over the scale
// counts these rather than accumulating a double, because a float loop counter
// accumulates its own rounding error: the last line of a long scale lands a
// little off where the arithmetic says it should, and the compiler cannot warn
// about a value that is only slightly wrong. It is also a finding clang-tidy
// makes, under a name that has changed between releases — so the fix is here
// rather than in a suppression that only some versions would need.
constexpr int kDecibelGridLines =
    static_cast<int>((kTopDecibels - kBottomDecibels) / kDecibelsPerGridLine);

constexpr double kNyquistHz = static_cast<double>(capture::kSampleRateHz) / 2.0;

// Where a frequency falls in a run of `count` values spread evenly from DC to
// Nyquist — the analyser's bins, or the history's columns. Returned unrounded,
// because a caller covering a pixel needs the range either side of it rather
// than the nearest one.
double IndexAtFrequency(double frequency_hz, double count) {
  return frequency_hz / kNyquistHz * count;
}

struct ViewChoice {
  const char* label;
  SpectrumView view;
};

constexpr ViewChoice kViewChoices[] = {
    {"Spectrum", SpectrumView::kTrace},
    {"Spectrogram", SpectrumView::kSpectrogram},
};

struct AveragingChoice {
  const char* label;
  double value;
};

constexpr AveragingChoice kAveragingChoices[] = {
    {"None — every transform", 0.0},
    {"Light", 0.3},
    {"Medium (default)", 0.6},
    {"Heavy — slow and steady", 0.85},
};

struct ContrastChoice {
  const char* label;
  double decibels;
};

// The top of the waterfall's colour scale. Below full scale is the useful part
// of this list: a LaserDisc carrier rarely reaches -10 dBFS, so a reference set
// there spends none of the ramp on levels the signal never gets to.
constexpr ContrastChoice kReferenceChoices[] = {
    {"0 dBFS", 0.0},
    {"-10 dBFS", -10.0},
    {"-20 dBFS", -20.0},
    {"-30 dBFS", -30.0},
};

// And how far below the reference the scale reaches. The narrow settings are
// what turn a wash of one colour into visible texture — the difference between
// a healthy noise floor and a marginal one is a few decibels, and across a
// hundred that is two shades of the same blue.
constexpr ContrastChoice kRangeChoices[] = {
    {"100 dB", 100.0},
    {"60 dB", 60.0},
    {"40 dB", 40.0},
    {"20 dB", 20.0},
};

// A gridline interval that divides the window into a readable number of steps
// and lands on a figure a person would choose themselves. Picked from a fixed
// ladder rather than computed, because window / 5 is how you get an axis marked
// every 6.4 seconds.
double TimeAxisStepSeconds(double window_seconds) {
  constexpr double kLadder[] = {0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0};

  for (const double step : kLadder) {
    if (window_seconds / step <= 6.0) {
      return step;
    }
  }
  return kLadder[(sizeof(kLadder) / sizeof(kLadder[0])) - 1];
}

}  // namespace

QString FormatSpectrumResolution(size_t transform_size) {
  const double spacing = analysis::SpectrumAnalyser::BinSpacingHz(
      transform_size, capture::kSampleRateHz);

  // Bin spacing rather than the resolution bandwidth, which is half again
  // wider. The two are easy to confuse and only one of them is a property of
  // the choice being made here — the bandwidth follows from the window, which
  // the user is not being offered.
  return SpectrumPanel::tr("%1 kHz bins").arg(spacing / 1000.0, 0, 'f', 1);
}

QString FormatResolutionBandwidth(size_t transform_size, size_t segments) {
  const double bandwidth = analysis::SpectrumAnalyser::NoiseBandwidthHz(
      transform_size, capture::kSampleRateHz);

  // Kilohertz throughout. Every transform size on offer lands between 3 and
  // 15 kHz, so a unit that switched would be a unit that never switched.
  const QString stated =
      SpectrumPanel::tr("RBW %1 kHz").arg(bandwidth / 1000.0, 0, 'f', 1);

  if (segments == 0) {
    return stated;
  }
  return SpectrumPanel::tr("%1 · %2 avg")
      .arg(stated)
      .arg(static_cast<qulonglong>(segments));
}

QString FormatLevelTick(double level_db) {
  return SpectrumPanel::tr("%1 dBFS").arg(level_db, 0, 'f', 0);
}

QString FormatAxisTick(double frequency_hz) {
  // Megahertz throughout, and bare, as this axis has always been labelled — a
  // scale that changed units partway up would need a suffix on every mark to
  // stay unambiguous, and the cursor is where an exact figure with units comes
  // from anyway.
  if (frequency_hz <= 0.0) {
    return QStringLiteral("0");
  }

  const double megahertz = frequency_hz / 1'000'000.0;
  if (megahertz < 1.0) {
    return QString::number(megahertz, 'f', 1);
  }
  return QString::number(megahertz, 'g', 3);
}

QString FormatFrequency(double frequency_hz) {
  if (frequency_hz < 1'000'000.0) {
    return SpectrumPanel::tr("%1 kHz").arg(frequency_hz / 1000.0, 0, 'f', 0);
  }
  return SpectrumPanel::tr("%1 MHz").arg(frequency_hz / 1'000'000.0, 0, 'f', 2);
}

QString FormatSecondsAgo(double seconds) {
  // Two decimal places at most, and fewer when the rest would be zeroes: a
  // spectrogram thirty seconds wide labelled "-30.00 s" is claiming a precision
  // its own frame rate does not have.
  QString text = QString::number(seconds, 'f', 2);
  if (text.contains(QLatin1Char('.'))) {
    while (text.endsWith(QLatin1Char('0'))) {
      text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
      text.chop(1);
    }
  }
  return SpectrumPanel::tr("%1 s").arg(text);
}

QString FormatSpectrumCursor(double frequency_hz, double level_db,
                             double seconds_ago) {
  const QString where = seconds_ago < 0.0
                            ? FormatFrequency(frequency_hz)
                            : SpectrumPanel::tr("%1, %2 ago")
                                  .arg(FormatFrequency(frequency_hz),
                                       FormatSecondsAgo(seconds_ago));

  if (level_db <= analysis::SpectrumAnalyser::kFloorDecibels) {
    return SpectrumPanel::tr("%1, nothing measurable").arg(where);
  }
  return SpectrumPanel::tr("%1, %2 dBFS").arg(where).arg(level_db, 0, 'f', 1);
}

SpectrumPlot::SpectrumPlot(QWidget* parent) : QWidget(parent) {
  setMinimumHeight(60);
  // Small on purpose. The size policy below is Expanding, so the plot fills
  // whatever it is given; the minimum is only what it must never be squeezed
  // below. Set to the height that looks right instead, three of these stacked
  // in one dock column demand more than the column has, and the separators
  // between them stop moving — a panel that cannot be resized because it
  // insisted on being comfortable.
  setMouseTracking(true);
  setAutoFillBackground(false);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SpectrumPlot::SetSpectrum(const std::vector<double>& magnitudes_db,
                               const std::vector<double>& peak_hold_db,
                               const std::vector<double>& snapshot_db,
                               size_t segments) {
  magnitudes_db_ = magnitudes_db;
  peak_hold_db_ = peak_hold_db;
  segments_ = segments;

  if (!clock_.isValid()) {
    clock_.start();
  }

  // This snapshot's own levels, not the averaged ones the trace draws. A row of
  // a spectrogram is a moment, and averaging belongs to the trace alone.
  //
  // Recorded whichever view is showing. The spectrogram is a record of the run
  // rather than of the time the user spent looking at it, so switching to it
  // shows what has happened rather than starting again from the moment it was
  // asked for.
  history_.Append(snapshot_db, static_cast<double>(clock_.elapsed()) / 1000.0);

  // Counted, not drawn. What the new row looks like depends on the theme and on
  // how tall the plot is, and paint time is where both are known to be current.
  if (pending_frames_ < static_cast<int>(history_.rows())) {
    ++pending_frames_;
  }

  update();
}

void SpectrumPlot::SetView(SpectrumView view) {
  view_ = view;
  update();
}

void SpectrumPlot::SetFrequencyScale(analysis::FrequencyScale scale) {
  scale_ = scale;
  // The picture has the mapping baked into its rows, so it is built again
  // rather than re-scaled: the rows are not a stretch of one another.
  spectrogram_valid_ = false;
  update();
}

analysis::FrequencyAxis SpectrumPlot::Axis() const {
  // Everything the converter can represent, always. There is no top-of-range
  // control: see kLowPassCornerHz for why one is no longer worth having.
  return analysis::FrequencyAxis(scale_, kNyquistHz);
}

void SpectrumPlot::SetSpectrogramReference(double reference_db) {
  reference_db_ = std::clamp(reference_db, kBottomDecibels, kTopDecibels);
  spectrogram_valid_ = false;
  update();
}

void SpectrumPlot::SetSpectrogramRange(double range_db) {
  // A range of nothing would put every level at one end of the scale and
  // divide by zero getting there.
  range_db_ = std::max(1.0, range_db);
  spectrogram_valid_ = false;
  update();
}

double SpectrumPlot::SpectrogramProportion(double level_db) const {
  return std::clamp((level_db - (reference_db_ - range_db_)) / range_db_, 0.0,
                    1.0);
}

void SpectrumPlot::changeEvent(QEvent* event) {
  if (event->type() == QEvent::PaletteChange) {
    spectrogram_valid_ = false;
  }
  QWidget::changeEvent(event);
}

void SpectrumPlot::SetPeakHoldVisible(bool visible) {
  peak_hold_visible_ = visible;
  update();
}

void SpectrumPlot::Clear() {
  magnitudes_db_.clear();
  peak_hold_db_.clear();
  segments_ = 0;
  history_.Clear();
  clock_.invalidate();
  spectrogram_valid_ = false;
  pending_frames_ = 0;
  update();
}

double SpectrumPlot::ScaleWidth() const {
  // The widest label the scale will ever draw, measured in the font it will be
  // drawn in. Fixed at 46 pixels this used to clip "-100 dBFS" to "-100 dB",
  // and would clip it again at the next font or display scale somebody used.
  const QFontMetrics metrics(font());
  return metrics.horizontalAdvance(FormatLevelTick(kBottomDecibels)) +
         kScaleTextGapPixels;
}

QRectF SpectrumPlot::PlotArea() const {
  const double left = ScaleWidth();
  const double top = kPlotMarginPixels;
  const double plot_width = std::max(0.0, width() - left - kPlotMarginPixels);
  const double plot_height = std::max(0.0, height() - top - kAxisHeightPixels);
  return QRectF(left, top, plot_width, plot_height);
}

double SpectrumPlot::ColumnLevel(const std::vector<double>& levels,
                                 const analysis::FrequencyAxis& axis,
                                 int column, int columns) {
  if (levels.size() < 2 || columns <= 0) {
    return kBottomDecibels;
  }

  // Bin levels.size() - 1 is Nyquist, so that is the count the bins are spread
  // across rather than the number of them.
  const double bins = static_cast<double>(levels.size() - 1);

  const double from =
      IndexAtFrequency(axis.FrequencyAt(static_cast<double>(column) /
                                        static_cast<double>(columns)),
                       bins);
  const double to =
      IndexAtFrequency(axis.FrequencyAt(static_cast<double>(column + 1) /
                                        static_cast<double>(columns)),
                       bins);

  // Outward to whole bins, so a column covers every bin it touches rather than
  // only those whose centres fall inside it. At the bottom of a logarithmic
  // axis a column is narrower than a bin and this covers one; at the top it is
  // many bins wide and this covers them all.
  size_t first = static_cast<size_t>(std::floor(std::max(0.0, from)));
  first = std::min(first, levels.size() - 1);

  size_t last = static_cast<size_t>(std::ceil(std::max(0.0, to)));
  last = std::clamp(last, first + 1, levels.size());

  // The highest bin in the column, not the first or the mean. There are more
  // bins than pixels, and a narrow carrier that fell between two sampled bins
  // would simply not be drawn — which on a display whose job is finding
  // carriers is the one failure that matters.
  double peak = kBottomDecibels;
  for (size_t bin = first; bin < last; ++bin) {
    peak = std::max(peak, levels[bin]);
  }
  return peak;
}

void SpectrumPlot::PaintTrace(QPainter& painter, const QRectF& area,
                              const std::vector<double>& levels,
                              const QColor& colour) {
  if (levels.size() < 2 || area.width() < 1.0) {
    return;
  }

  // The analyser always produces the whole span to Nyquist; the axis decides
  // which part of it each column shows, so narrowing the range or switching to
  // a logarithmic scale re-spreads the same bins rather than throwing any away.
  const analysis::FrequencyAxis axis = Axis();
  const int columns = static_cast<int>(area.width());

  QPolygonF trace;
  trace.reserve(columns);

  for (int column = 0; column < columns; ++column) {
    const double peak = ColumnLevel(levels, axis, column, columns);

    const double proportion =
        (peak - kBottomDecibels) / (kTopDecibels - kBottomDecibels);
    const double y =
        area.bottom() - (std::clamp(proportion, 0.0, 1.0) * area.height());
    trace.append(QPointF(area.left() + column, y));
  }

  painter.setPen(QPen(colour, 1.0));
  painter.drawPolyline(trace);
}

void SpectrumPlot::PaintSpectrogram(QPainter& painter, const QRectF& area,
                                    bool dark) {
  if (history_.empty()) {
    return;
  }

  const size_t frames = history_.size();

  // Built at the height it will be drawn at, so nothing scales it vertically.
  //
  // That is not an optimisation. There are a thousand frequency bands and a
  // couple of hundred pixels to put them in, and letting the painter do the
  // reduction means it averages them — which is exactly how a one-band carrier
  // disappears into the noise either side of it. Reducing here by taking the
  // highest band in each pixel row keeps it, the same rule and the same reason
  // as the spectrum trace's own decimation.
  const int height = std::max(1, static_cast<int>(area.height()));
  const int capacity = static_cast<int>(history_.rows());

  if (!spectrogram_valid_ || spectrogram_.size() != QSize(capacity, height)) {
    RebuildSpectrogram(height, dark);
  } else if (pending_frames_ > 0) {
    ScrollSpectrogram(pending_frames_, dark);
  }
  pending_frames_ = 0;

  // Anchored to the right and only as wide as there is history for, so a run
  // starts as a sliver at the right-hand edge and grows leftwards until it
  // fills — and then scrolls. This is a fixed window of time, unlike the
  // amplitude panel, which stretches what it holds across its whole width until
  // it is full: stretching would mean the time scale silently changed under the
  // reader for the first half-minute of every run.
  //
  // The newest frame is always the right-hand column of the picture, so the
  // part worth drawing is the right-hand end of it.
  const double used = area.width() * static_cast<double>(frames) /
                      static_cast<double>(capacity);
  const QRectF target(area.right() - used, area.top(), used,
                      static_cast<double>(height));
  const QRect source(capacity - static_cast<int>(frames), 0,
                     static_cast<int>(frames), height);

  // Smoothing now only ever stretches time, where averaging between adjacent
  // frames is what is wanted: it turns a drifting carrier into a line rather
  // than a staircase.
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter.drawImage(target, spectrogram_, source);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
}

void SpectrumPlot::MapSpectrogramRows(int height) {
  const analysis::FrequencyAxis axis = Axis();
  const size_t held = history_.columns();
  const double bands = static_cast<double>(held);

  spectrogram_rows_.assign(static_cast<size_t>(height), BandRange{});

  for (int y = 0; y < height; ++y) {
    // Row zero is the top of the picture and the top of the frequency range, as
    // on every frequency axis.
    const double upper = 1.0 - (static_cast<double>(y) / height);
    const double lower = 1.0 - (static_cast<double>(y + 1) / height);

    // Through the axis rather than by proportion, so the picture and the trace
    // beside it put a carrier at the same place on the same scale.
    const double from = IndexAtFrequency(axis.FrequencyAt(lower), bands);
    const double to = IndexAtFrequency(axis.FrequencyAt(upper), bands);

    BandRange& range = spectrogram_rows_[static_cast<size_t>(y)];
    range.first = std::min(static_cast<size_t>(std::floor(std::max(0.0, from))),
                           held - 1);
    range.last = std::clamp(static_cast<size_t>(std::ceil(std::max(0.0, to))),
                            range.first + 1, held);
  }
}

void SpectrumPlot::RenderSpectrogramColumn(int column, size_t frame,
                                           bool dark) {
  for (size_t y = 0; y < spectrogram_rows_.size(); ++y) {
    const BandRange& range = spectrogram_rows_[y];

    double peak = analysis::SpectrumAnalyser::kFloorDecibels;
    for (size_t band = range.first; band < range.last; ++band) {
      peak = std::max(peak, history_.At(frame, band));
    }

    auto* const scanline =
        reinterpret_cast<QRgb*>(spectrogram_.scanLine(static_cast<int>(y)));
    scanline[column] =
        theme_tokens::SpectrogramColor(SpectrogramProportion(peak), dark).rgb();
  }
}

void SpectrumPlot::RebuildSpectrogram(int height, bool dark) {
  const int capacity = static_cast<int>(history_.rows());

  // Time across and frequency up, which is the way round the amplitude panel
  // already runs and the way round a spectrogram is read: a carrier is a
  // horizontal line at its own frequency, and a drift is that line sloping.
  spectrogram_ = QImage(capacity, height, QImage::Format_RGB32);
  spectrogram_.fill(palette().color(QPalette::Base));

  MapSpectrogramRows(height);

  const int frames = static_cast<int>(history_.size());
  for (int frame = 0; frame < frames; ++frame) {
    RenderSpectrogramColumn(capacity - frames + frame,
                            static_cast<size_t>(frame), dark);
  }

  spectrogram_valid_ = true;
}

void SpectrumPlot::ScrollSpectrogram(int columns, bool dark) {
  const int width = spectrogram_.width();
  const int frames = static_cast<int>(history_.size());
  const int shift = std::min({columns, width, frames});
  if (shift <= 0) {
    return;
  }

  if (shift < width) {
    for (int y = 0; y < spectrogram_.height(); ++y) {
      auto* const scanline = reinterpret_cast<QRgb*>(spectrogram_.scanLine(y));
      // Overlapping by definition — every column moves onto one still being
      // read — so a move rather than a copy.
      std::memmove(scanline, scanline + shift,
                   static_cast<size_t>(width - shift) * sizeof(QRgb));
    }
  }

  // The newest frame ends up in the right-hand column and the ones before it
  // fill in leftwards from there.
  for (int step = 0; step < shift; ++step) {
    RenderSpectrogramColumn(width - shift + step,
                            static_cast<size_t>(frames - shift + step), dark);
  }
}

void SpectrumPlot::PaintGrid(QPainter& painter, const QRectF& area) {
  const QPalette& colours = palette();
  const QFontMetrics metrics(painter.font());

  const bool spectrogram = view_ == SpectrumView::kSpectrogram;

  const analysis::FrequencyAxis axis = Axis();
  const std::vector<double> ticks = axis.Ticks();

  painter.setPen(theme_tokens::GridLine(colours));

  if (spectrogram) {
    // Frequency runs up the side here, so its gridlines are horizontal.
    for (const double frequency : ticks) {
      const double y =
          area.bottom() - (axis.ProportionOf(frequency) * area.height());
      painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }
  } else {
    for (int line = 0; line <= kDecibelGridLines; ++line) {
      const double level = kTopDecibels - (line * kDecibelsPerGridLine);
      const double proportion =
          (level - kBottomDecibels) / (kTopDecibels - kBottomDecibels);
      const double y = area.bottom() - (proportion * area.height());
      painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }

    for (const double frequency : ticks) {
      const double x =
          area.left() + (axis.ProportionOf(frequency) * area.width());
      painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    }
  }

  painter.setPen(theme_tokens::MutedText(colours));

  if (spectrogram) {
    for (const double frequency : ticks) {
      const double y =
          area.bottom() - (axis.ProportionOf(frequency) * area.height());
      painter.drawText(
          QRectF(0.0, y - (metrics.height() / 2.0),
                 area.left() - kScaleTextGapPixels, metrics.height()),
          Qt::AlignRight | Qt::AlignVCenter, FormatAxisTick(frequency));
    }

    // The time axis, in seconds into the past. The rate frames arrive at is not
    // told to this panel and depends on the ring geometry the run was started
    // with, so it is measured from the frames themselves — which means there is
    // nothing to label until two have arrived and an interval exists.
    const double window = history_.WindowSeconds();
    painter.drawText(QRectF(area.left(), area.bottom() + 2.0, area.width(),
                            kAxisHeightPixels - 2.0),
                     Qt::AlignRight | Qt::AlignTop, tr("now"));

    if (window <= 0.0) {
      return;
    }

    const double step = TimeAxisStepSeconds(window);
    painter.setPen(theme_tokens::GridLine(colours));
    for (int mark = 1; mark * step < window; ++mark) {
      const double ago = mark * step;
      const double x = area.right() - (ago / window * area.width());
      painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    }

    painter.setPen(theme_tokens::MutedText(colours));
    for (int mark = 1; mark * step < window; ++mark) {
      const double ago = mark * step;
      const double x = area.right() - (ago / window * area.width());
      painter.drawText(
          QRectF(x - 40.0, area.bottom() + 2.0, 80.0, kAxisHeightPixels - 2.0),
          Qt::AlignHCenter | Qt::AlignTop, FormatSecondsAgo(ago));
    }
    return;
  }

  for (int line = 0; line <= kDecibelGridLines; ++line) {
    const double level = kTopDecibels - (line * kDecibelsPerGridLine);
    const double proportion =
        (level - kBottomDecibels) / (kTopDecibels - kBottomDecibels);
    const double y = area.bottom() - (proportion * area.height());
    painter.drawText(
        QRectF(0.0, y - (metrics.height() / 2.0),
               area.left() - kScaleTextGapPixels, metrics.height()),
        Qt::AlignRight | Qt::AlignVCenter, FormatLevelTick(level));
  }

  for (const double frequency : ticks) {
    const double x =
        area.left() + (axis.ProportionOf(frequency) * area.width());
    painter.drawText(
        QRectF(x - 30.0, area.bottom() + 2.0, 60.0, kAxisHeightPixels - 2.0),
        Qt::AlignHCenter | Qt::AlignTop, FormatAxisTick(frequency));
  }
}

void SpectrumPlot::PaintLabel(QPainter& painter, const QRectF& box,
                              Qt::Alignment alignment, const QString& text,
                              const QColor& ink) {
  // Not quite opaque: an annotation that blanked what was under it would hide
  // the very trace a reader is comparing it against.
  QColor backing = palette().color(QPalette::Base);
  backing.setAlpha(200);
  painter.fillRect(box, backing);

  painter.setPen(ink);
  painter.drawText(box, static_cast<int>(alignment), text);
}

void SpectrumPlot::PaintAnnotation(QPainter& painter, const QRectF& area) {
  if (magnitudes_db_.size() < 2) {
    return;
  }

  // The transform that produced these levels, read back from how many of them
  // there are rather than from the control. The control is what was asked for;
  // this is what arrived, and between a user moving it and the worker building
  // the new analyser those are briefly different things.
  const size_t transform_size = (magnitudes_db_.size() - 1) * 2;
  const QString text = FormatResolutionBandwidth(transform_size, segments_);

  const QFontMetrics metrics(painter.font());
  const QRectF box(
      area.left() + kAnnotationInsetPixels, area.top() + kAnnotationInsetPixels,
      metrics.horizontalAdvance(text) + kScaleTextGapPixels, metrics.height());

  PaintLabel(painter, box, Qt::AlignCenter, text,
             theme_tokens::MutedText(palette()));
}

void SpectrumPlot::PaintFilterCorner(QPainter& painter, const QRectF& area,
                                     bool dark) {
  const analysis::FrequencyAxis axis = Axis();
  const double proportion = axis.ProportionOf(analysis::kLowPassCornerHz);

  // Off the end of the axis is possible in principle and worth handling: a
  // marker clamped to an edge would claim the corner was somewhere it is not.
  if (proportion <= 0.0 || proportion >= 1.0) {
    return;
  }

  const QString text =
      tr("%1 MHz filter")
          .arg(analysis::kLowPassCornerHz / 1'000'000.0, 0, 'g', 3);

  const QFontMetrics metrics(painter.font());
  const double text_width =
      metrics.horizontalAdvance(text) + kScaleTextGapPixels;

  const QColor ink = theme_tokens::PlotColor(
      theme_tokens::PlotColorToken::kFilterCorner, dark);
  painter.setPen(QPen(ink, 1.0, Qt::DashLine));

  if (view_ == SpectrumView::kSpectrogram) {
    // Frequency runs up the side here, so the corner is a horizontal line.
    const double y = area.bottom() - (proportion * area.height());
    painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));

    // Labelled at the right-hand end, where the newest column is: the corner
    // sits high on the axis and the left-hand end is where the annotation in
    // the opposite corner already is.
    PaintLabel(painter,
               QRectF(area.right() - text_width - kAnnotationInsetPixels,
                      y - metrics.height() - 1.0, text_width, metrics.height()),
               Qt::AlignCenter, text, ink);
    return;
  }

  const double x = area.left() + (proportion * area.width());
  painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));

  // To the right of the line, unless that would run off the plot — which on a
  // decade axis, where the corner sits at 92% of the width, it always does.
  const bool fits_right =
      x + kAnnotationInsetPixels + text_width <= area.right();
  const double left = fits_right ? x + kAnnotationInsetPixels
                                 : x - kAnnotationInsetPixels - text_width;

  PaintLabel(painter,
             QRectF(left, area.top() + kAnnotationInsetPixels, text_width,
                    metrics.height()),
             Qt::AlignCenter, text, ink);
}

void SpectrumPlot::PaintPeakMarker(QPainter& painter, const QRectF& area) {
  if (magnitudes_db_.size() < 2 || area.width() < 1.0) {
    return;
  }

  const analysis::FrequencyAxis axis = Axis();
  const int columns = static_cast<int>(area.width());

  // The highest column of the drawn trace, found the same way the trace was
  // drawn — so the marker lands on the picture rather than near it. Taking the
  // highest bin instead would put the label at a frequency no pixel shows.
  int peak_column = 0;
  double peak_level = kBottomDecibels;
  for (int column = 0; column < columns; ++column) {
    const double level = ColumnLevel(magnitudes_db_, axis, column, columns);
    if (level > peak_level) {
      peak_level = level;
      peak_column = column;
    }
  }

  // Nothing above the bottom of the scale: an empty run, or a signal that has
  // gone away. A marker here would be pointing at the floor.
  if (peak_level <= kBottomDecibels) {
    return;
  }

  // The middle of the column, because that is the frequency the column stands
  // for; its left-hand edge is where it starts.
  const double frequency = axis.FrequencyAt(
      (static_cast<double>(peak_column) + 0.5) / static_cast<double>(columns));

  const double proportion =
      (peak_level - kBottomDecibels) / (kTopDecibels - kBottomDecibels);
  const double x = area.left() + peak_column;
  const double y =
      area.bottom() - (std::clamp(proportion, 0.0, 1.0) * area.height());

  // The window's own text colour rather than a plot colour. This is the
  // instrument speaking, not another trace, and a reader should not have to
  // work out whether the marker is part of the signal.
  const QColor ink = palette().color(QPalette::WindowText);

  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(QPen(ink, 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawEllipse(QPointF(x, y), kPeakMarkerRadius, kPeakMarkerRadius);
  painter.setRenderHint(QPainter::Antialiasing, false);

  // The same words the cursor uses for the same reading, so that pointing at
  // the peak confirms the marker rather than paraphrasing it.
  const QString text = FormatSpectrumCursor(frequency, peak_level);

  const QFontMetrics metrics(painter.font());
  const double text_width =
      metrics.horizontalAdvance(text) + kScaleTextGapPixels;

  // Above the marker where there is room, below it when the peak is near the
  // top of the scale — which for a healthy carrier at the default reference is
  // most of the time.
  const double gap = kPeakMarkerRadius + kAnnotationInsetPixels;
  const bool above = y - gap - metrics.height() >= area.top();
  const double top = above ? y - gap - metrics.height() : y + gap;

  // And shifted along when it would otherwise run off either edge. A carrier
  // near the top of the band sits at the right-hand end of the axis, which is
  // where there is least room for the label naming it.
  const double left =
      std::clamp(x - (text_width / 2.0), area.left(),
                 std::max(area.left(), area.right() - text_width));

  PaintLabel(painter, QRectF(left, top, text_width, metrics.height()),
             Qt::AlignCenter, text, ink);
}

void SpectrumPlot::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);

  QPainter painter(this);
  const QPalette& colours = palette();
  const bool dark = theme_tokens::IsDarkPalette(colours);

  painter.fillRect(rect(), colours.color(QPalette::Base));

  const QRectF area = PlotArea();
  if (area.width() < 2.0 || area.height() < 2.0) {
    return;
  }

  if (view_ == SpectrumView::kSpectrogram) {
    // Under the grid, so the frequency lines stay readable over it.
    PaintSpectrogram(painter, area, dark);
    PaintGrid(painter, area);
    PaintFilterCorner(painter, area, dark);
    PaintAnnotation(painter, area);
    return;
  }

  PaintGrid(painter, area);

  // Under both traces. These say what the instrument is doing; the traces are
  // what it measured, and the measurement is what a reader is looking at.
  PaintFilterCorner(painter, area, dark);
  PaintAnnotation(painter, area);

  if (peak_hold_visible_) {
    // Drawn first, so the live trace sits on top of its own history rather than
    // being hidden by it.
    PaintTrace(painter, area, peak_hold_db_,
               theme_tokens::PlotColor(
                   theme_tokens::PlotColorToken::kSpectrumPeakHold, dark));
  }

  PaintTrace(painter, area, magnitudes_db_,
             theme_tokens::PlotColor(
                 theme_tokens::PlotColorToken::kSpectrumTrace, dark));

  // Last, so that the one thing on the display a reader is meant to be able to
  // read off without pointing at anything is not drawn over by a trace.
  PaintPeakMarker(painter, area);
}

void SpectrumPlot::mouseMoveEvent(QMouseEvent* event) {
  const QRectF area = PlotArea();
  const bool has_trace = view_ == SpectrumView::kTrace ? !magnitudes_db_.empty()
                                                       : !history_.empty();

  if (!area.contains(event->position()) || !has_trace) {
    emit CursorLeft();
    QWidget::mouseMoveEvent(event);
    return;
  }

  const analysis::FrequencyAxis axis = Axis();

  if (view_ == SpectrumView::kSpectrogram) {
    // Frequency up the side, time across.
    const double up = std::clamp(
        (area.bottom() - event->position().y()) / area.height(), 0.0, 1.0);
    const double frequency = axis.FrequencyAt(up);

    const size_t held = history_.columns();
    const size_t band = std::min(
        held - 1,
        static_cast<size_t>(std::max(
            0.0, IndexAtFrequency(frequency, static_cast<double>(held)))));

    // The picture is anchored to the right and only as wide as there is
    // history for, so a pointer left of where it starts is over nothing.
    const double used = area.width() * static_cast<double>(history_.size()) /
                        static_cast<double>(history_.rows());
    const double into = event->position().x() - (area.right() - used);
    if (into < 0.0) {
      emit CursorLeft();
      QWidget::mouseMoveEvent(event);
      return;
    }

    const size_t frame =
        std::min(history_.size() - 1,
                 static_cast<size_t>(into / used *
                                     static_cast<double>(history_.size() - 1)));

    emit CursorMoved(
        frequency, history_.At(frame, band),
        history_.SecondsAt(history_.size() - 1) - history_.SecondsAt(frame));
    QWidget::mouseMoveEvent(event);
    return;
  }

  const double across = std::clamp(
      (event->position().x() - area.left()) / area.width(), 0.0, 1.0);
  const double frequency = axis.FrequencyAt(across);

  // The level the column under the pointer was drawn with, computed by the same
  // function that drew it, so the readout cannot disagree with the picture.
  const int columns = std::max(1, static_cast<int>(area.width()));
  const int column = std::clamp(
      static_cast<int>(event->position().x() - area.left()), 0, columns - 1);

  emit CursorMoved(frequency,
                   ColumnLevel(magnitudes_db_, axis, column, columns), -1.0);
  QWidget::mouseMoveEvent(event);
}

void SpectrumPlot::leaveEvent(QEvent* event) {
  emit CursorLeft();
  QWidget::leaveEvent(event);
}

SpectrumPanel::SpectrumPanel(CaptureController* controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);

  plot_ = new SpectrumPlot(this);
  plot_->setObjectName(QLatin1String(kPlotName));
  layout->addWidget(plot_, 1);

  auto* controls = new QHBoxLayout();

  view_ = new QComboBox(this);
  view_->setObjectName(QLatin1String(kViewComboName));
  for (const ViewChoice& choice : kViewChoices) {
    view_->addItem(tr(choice.label), static_cast<int>(choice.view));
  }
  view_->setToolTip(
      tr("The spectrum shows what is in the signal now. The spectrogram shows "
         "the same thing over time, with level as colour — which is what makes "
         "a carrier that drifts, or an interfering source that comes and goes, "
         "visible at all."));
  connect(view_, &QComboBox::currentIndexChanged, this,
          [this](int) { ApplyView(); });
  controls->addWidget(view_);

  log_frequency_ = new QCheckBox(tr("Log frequency"), this);
  log_frequency_->setObjectName(QLatin1String(kLogFrequencyBoxName));
  log_frequency_->setChecked(true);
  log_frequency_->setToolTip(tr(
      "Space the frequency axis by decade rather than evenly. The content "
      "here runs from the digital audio band at 200 kHz to the filter "
      "corner at 13.2 MHz — nearly two decades — and spread evenly, "
      "everything below 2 MHz is crushed into the left-hand seventh of the "
      "display. Turn it off to read the filter's roll-off or the symmetry "
      "of the FM sidebands, both of which are about equal spacing in hertz."));
  connect(log_frequency_, &QCheckBox::toggled, this,
          [this](bool on) { ApplyFrequencyScale(on); });
  controls->addWidget(log_frequency_);

  resolution_ = new QComboBox(this);
  resolution_->setObjectName(QLatin1String(kResolutionComboName));
  for (size_t index = 0; index < analysis::kTransformSizeChoiceCount; ++index) {
    const size_t size = analysis::kTransformSizeChoices[index];
    resolution_->addItem(FormatSpectrumResolution(size),
                         static_cast<qulonglong>(size));
    if (size == analysis::kDefaultTransformSize) {
      resolution_->setCurrentIndex(resolution_->count() - 1);
    }
  }
  resolution_->setToolTip(
      tr("How finely the spectrum is divided. Narrower bins separate carriers "
         "that sit close together — the audio carriers below 3 MHz — but a "
         "snapshot holds fewer of them to average, so the noise floor is less "
         "steady. The widest setting resolves the FM carrier comfortably and "
         "averages the most."));
  connect(resolution_, &QComboBox::currentIndexChanged, this,
          [this](int) { ApplyResolution(); });
  controls->addWidget(new QLabel(tr("Resolution"), this));
  controls->addWidget(resolution_);

  averaging_ = new QComboBox(this);
  averaging_->setObjectName(QLatin1String(kAveragingComboName));
  for (const AveragingChoice& choice : kAveragingChoices) {
    averaging_->addItem(tr(choice.label), choice.value);
  }
  averaging_->setCurrentIndex(2);
  averaging_->setToolTip(
      tr("How much of the previous display each new transform replaces. More "
         "averaging makes a weak carrier readable against the noise; less "
         "shows a transient that would otherwise be averaged away."));
  connect(averaging_, &QComboBox::currentIndexChanged, this,
          [this](int) { ApplyAveraging(); });
  controls->addWidget(new QLabel(tr("Averaging"), this));
  controls->addWidget(averaging_);

  reference_ = new QComboBox(this);
  reference_->setObjectName(QLatin1String(kReferenceComboName));
  for (const ContrastChoice& choice : kReferenceChoices) {
    reference_->addItem(tr(choice.label), choice.decibels);
    if (choice.decibels == kDefaultSpectrogramReferenceDb) {
      reference_->setCurrentIndex(reference_->count() - 1);
    }
  }
  reference_->setToolTip(
      tr("The level the top of the colour scale stands for. Anything above it "
         "is drawn the same, so setting it near the strongest thing present "
         "spends the whole ramp on levels the signal actually reaches."));
  connect(reference_, &QComboBox::currentIndexChanged, this,
          [this](int) { ApplyContrast(); });
  reference_label_ = new QLabel(tr("Reference"), this);
  controls->addWidget(reference_label_);
  controls->addWidget(reference_);

  range_ = new QComboBox(this);
  range_->setObjectName(QLatin1String(kRangeComboName));
  for (const ContrastChoice& choice : kRangeChoices) {
    range_->addItem(tr(choice.label), choice.decibels);
    if (choice.decibels == kDefaultSpectrogramRangeDb) {
      range_->setCurrentIndex(range_->count() - 1);
    }
  }
  range_->setToolTip(
      tr("How far below the reference the colour scale reaches. Narrowing it "
         "spreads the ramp over fewer decibels, which is what makes the "
         "texture of a noise floor visible instead of a wash of one colour. "
         "Everything already on screen is re-coloured — the history is kept as "
         "levels, not as a picture."));
  connect(range_, &QComboBox::currentIndexChanged, this,
          [this](int) { ApplyContrast(); });
  range_label_ = new QLabel(tr("Range"), this);
  controls->addWidget(range_label_);
  controls->addWidget(range_);

  peak_hold_ = new QCheckBox(tr("Peak hold"), this);
  peak_hold_->setObjectName(QLatin1String(kPeakHoldBoxName));
  peak_hold_->setChecked(true);
  connect(peak_hold_, &QCheckBox::toggled, this,
          [this](bool on) { plot_->SetPeakHoldVisible(on); });
  controls->addWidget(peak_hold_);

  reset_ = new QPushButton(tr("Reset peaks"), this);
  reset_->setObjectName(QLatin1String(kResetButtonName));
  connect(reset_, &QPushButton::clicked, this, [this] {
    if (controller_ != nullptr) {
      controller_->analysis()->ResetPeakHold();
    }
  });
  controls->addWidget(reset_);

  controls->addStretch();

  cursor_ = new QLabel(this);
  cursor_->setObjectName(QLatin1String(kCursorLabelName));
  cursor_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  controls->addWidget(cursor_);

  layout->addLayout(controls);

  connect(plot_, &SpectrumPlot::CursorMoved, this, &SpectrumPanel::ShowCursor);
  connect(plot_, &SpectrumPlot::CursorLeft, this, &SpectrumPanel::ClearCursor);

  ClearCursor();
  ApplyView();

  if (controller_ != nullptr) {
    connect(controller_->analysis(), &AnalysisWorker::SpectrumReady, this,
            &SpectrumPanel::OnSpectrumReady);
    connect(controller_, &CaptureController::MonitoringChanged, this,
            &SpectrumPanel::OnMonitoringChanged);
  }
}

void SpectrumPanel::ApplyView() {
  const auto chosen = static_cast<SpectrumView>(view_->currentData().toInt());
  plot_->SetView(chosen);

  // Each view keeps the controls that do something in it. Peak hold belongs to
  // the trace — the spectrogram already shows every frame it would be
  // summarising — and the colour scale belongs to the spectrogram, which is the
  // only thing drawn in colour.
  //
  // Disabled as well as hidden, rather than only hidden, because "can this be
  // pressed" is what the tests and the accessibility layer both ask, and a
  // hidden control that still answers yes is lying to both.
  const bool trace = chosen == SpectrumView::kTrace;

  peak_hold_->setEnabled(trace);
  peak_hold_->setVisible(trace);
  reset_->setEnabled(trace);
  reset_->setVisible(trace);

  reference_->setEnabled(!trace);
  reference_->setVisible(!trace);
  reference_label_->setVisible(!trace);
  range_->setEnabled(!trace);
  range_->setVisible(!trace);
  range_label_->setVisible(!trace);
}

void SpectrumPanel::ApplyAveraging() {
  if (controller_ != nullptr) {
    controller_->analysis()->SetSpectrumAveraging(
        averaging_->currentData().toDouble());
  }
}

void SpectrumPanel::ApplyFrequencyScale(bool logarithmic) {
  plot_->SetFrequencyScale(logarithmic ? analysis::FrequencyScale::kLogarithmic
                                       : analysis::FrequencyScale::kLinear);
}

void SpectrumPanel::ApplyContrast() {
  plot_->SetSpectrogramReference(reference_->currentData().toDouble());
  plot_->SetSpectrogramRange(range_->currentData().toDouble());
}

void SpectrumPanel::ApplyResolution() {
  if (controller_ != nullptr) {
    controller_->analysis()->SetSpectrumTransformSize(
        static_cast<size_t>(resolution_->currentData().toULongLong()));
  }
}

void SpectrumPanel::OnSpectrumReady(const std::vector<double>& magnitudes_db,
                                    const std::vector<double>& peak_hold_db,
                                    const std::vector<double>& snapshot_db,
                                    size_t segments) {
  plot_->SetSpectrum(magnitudes_db, peak_hold_db, snapshot_db, segments);

  // The measured window settles quickly and then drifts by fractions of a
  // percent. Announced only when it has moved enough to matter, because
  // otherwise this is a signal at the snapshot rate that redraws another panel
  // every time.
  const double window = plot_->history().WindowSeconds();
  if (window > 0.0 && std::abs(window - announced_window_seconds_) >
                          announced_window_seconds_ * 0.05 + 0.05) {
    announced_window_seconds_ = window;
    emit TimeWindowChanged(window);
  }
}

void SpectrumPanel::OnMonitoringChanged(bool monitoring) {
  if (!monitoring) {
    return;
  }

  plot_->Clear();
  announced_window_seconds_ = 0.0;
  ClearCursor();

  // The worker is built fresh for each run, so the settings it was told about
  // last time went with the old one. Re-applied here rather than remembered
  // there, because these controls are the thing that knows what the user chose.
  ApplyAveraging();
  ApplyResolution();
}

void SpectrumPanel::ShowCursor(double frequency_hz, double level_db,
                               double seconds_ago) {
  cursor_->setText(FormatSpectrumCursor(frequency_hz, level_db, seconds_ago));
}

void SpectrumPanel::ClearCursor() {
  cursor_->setText(tr("Point at the trace to read a frequency"));
}

}  // namespace ddd::gui
