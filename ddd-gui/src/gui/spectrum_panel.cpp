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

#include "capture_controller.h"
#include "sample_format.h"
#include "spectrum_analyser.h"
#include "theme_color_tokens.h"

namespace ddd::gui {
namespace {

constexpr int kScaleWidthPixels = 46;
constexpr int kAxisHeightPixels = 20;
constexpr int kPlotMarginPixels = 6;

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

// 2 MHz gridlines put the LaserDisc FM carrier — around 8 MHz — between two of
// them rather than on one, which is what makes a drift visible, and keep the
// axis readable across every displayed range rather than only the widest.
constexpr double kGridFrequencyStepHz = 2'000'000.0;

constexpr double kNyquistHz = static_cast<double>(capture::kSampleRateHz) / 2.0;

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
                               const std::vector<double>& peak_hold_db) {
  magnitudes_db_ = magnitudes_db;
  peak_hold_db_ = peak_hold_db;

  if (!clock_.isValid()) {
    clock_.start();
  }

  // Recorded whichever view is showing. The spectrogram is a record of the run
  // rather than of the time the user spent looking at it, so switching to it
  // shows what has happened rather than starting again from the moment it was
  // asked for.
  history_.Append(magnitudes_db_,
                  static_cast<double>(clock_.elapsed()) / 1000.0);
  spectrogram_valid_ = false;

  update();
}

void SpectrumPlot::SetView(SpectrumView view) {
  view_ = view;
  update();
}

void SpectrumPlot::SetMaximumFrequency(double frequency_hz) {
  maximum_frequency_hz_ = std::clamp(frequency_hz, 1.0, kNyquistHz);
  spectrogram_valid_ = false;
  update();
}

double SpectrumPlot::DisplayedProportion() const {
  return std::clamp(maximum_frequency_hz_ / kNyquistHz, 0.0, 1.0);
}

void SpectrumPlot::SetPeakHoldVisible(bool visible) {
  peak_hold_visible_ = visible;
  update();
}

void SpectrumPlot::Clear() {
  magnitudes_db_.clear();
  peak_hold_db_.clear();
  history_.Clear();
  clock_.invalidate();
  spectrogram_valid_ = false;
  update();
}

QRectF SpectrumPlot::PlotArea() const {
  const double left = kScaleWidthPixels;
  const double top = kPlotMarginPixels;
  const double plot_width = std::max(0.0, width() - left - kPlotMarginPixels);
  const double plot_height = std::max(0.0, height() - top - kAxisHeightPixels);
  return QRectF(left, top, plot_width, plot_height);
}

void SpectrumPlot::PaintTrace(QPainter& painter, const QRectF& area,
                              const std::vector<double>& levels,
                              const QColor& colour) {
  if (levels.size() < 2 || area.width() < 1.0) {
    return;
  }

  // Only the part of the spectrum on display. The analyser always produces the
  // whole span to Nyquist; narrowing the range spreads a subset of its bins
  // across the same width rather than throwing any away.
  const size_t usable = std::max<size_t>(
      2, static_cast<size_t>(static_cast<double>(levels.size()) *
                             DisplayedProportion()));

  const int columns = static_cast<int>(area.width());
  QPolygonF trace;
  trace.reserve(columns);

  for (int column = 0; column < columns; ++column) {
    // The highest bin in the column, not the first or the mean. There are more
    // bins than pixels, and a narrow carrier that fell between two sampled bins
    // would simply not be drawn — which on a display whose job is finding
    // carriers is the one failure that matters.
    const size_t first = static_cast<size_t>(static_cast<double>(column) *
                                             static_cast<double>(usable) /
                                             static_cast<double>(columns));
    const size_t last =
        std::min(usable, static_cast<size_t>(static_cast<double>(column + 1) *
                                             static_cast<double>(usable) /
                                             static_cast<double>(columns)) +
                             1);

    double peak = kBottomDecibels;
    for (size_t bin = first; bin < last; ++bin) {
      peak = std::max(peak, levels[bin]);
    }

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
  const size_t bands = std::max<size_t>(
      1, static_cast<size_t>(static_cast<double>(history_.columns()) *
                             DisplayedProportion()));

  // Built at the height it will be drawn at, so nothing scales it vertically.
  //
  // That is not an optimisation. There are a thousand frequency bands and a
  // couple of hundred pixels to put them in, and letting the painter do the
  // reduction means it averages them — which is exactly how a one-band carrier
  // disappears into the noise either side of it. Reducing here by taking the
  // highest band in each pixel row keeps it, the same rule and the same reason
  // as the spectrum trace's own decimation.
  const int height = std::max(1, static_cast<int>(area.height()));

  if (!spectrogram_valid_ ||
      spectrogram_.size() != QSize(static_cast<int>(frames), height)) {
    // Time across and frequency up, which is the way round the amplitude panel
    // already runs and the way round a spectrogram is read: a carrier is a
    // horizontal line at its own frequency, and a drift is that line sloping.
    spectrogram_ =
        QImage(static_cast<int>(frames), height, QImage::Format_RGB32);

    for (int y = 0; y < height; ++y) {
      // Row zero is the top of the picture and the top of the frequency range,
      // as on every frequency axis.
      const double upper = 1.0 - (static_cast<double>(y) / height);
      const double lower = 1.0 - (static_cast<double>(y + 1) / height);

      const size_t first = static_cast<size_t>(std::max(0.0, lower) *
                                               static_cast<double>(bands));
      const size_t last = std::min(
          bands,
          std::max(first + 1, static_cast<size_t>(std::min(1.0, upper) *
                                                  static_cast<double>(bands))));

      auto* const scanline = reinterpret_cast<QRgb*>(spectrogram_.scanLine(y));
      for (size_t frame = 0; frame < frames; ++frame) {
        double peak = kBottomDecibels;
        for (size_t band = first; band < last; ++band) {
          peak = std::max(peak, history_.At(frame, band));
        }

        const double proportion =
            (peak - kBottomDecibels) / (kTopDecibels - kBottomDecibels);
        scanline[frame] =
            theme_tokens::SpectrogramColor(proportion, dark).rgb();
      }
    }

    spectrogram_valid_ = true;
  }

  // Anchored to the right and only as wide as there is history for, so a run
  // starts as a sliver at the right-hand edge and grows leftwards until it
  // fills — and then scrolls. This is a fixed window of time, unlike the
  // amplitude panel, which stretches what it holds across its whole width until
  // it is full: stretching would mean the time scale silently changed under the
  // reader for the first half-minute of every run.
  const double used = area.width() * static_cast<double>(frames) /
                      static_cast<double>(history_.rows());
  const QRectF target(area.right() - used, area.top(), used,
                      static_cast<double>(height));

  // Smoothing now only ever stretches time, where averaging between adjacent
  // frames is what is wanted: it turns a drifting carrier into a line rather
  // than a staircase.
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter.drawImage(target, spectrogram_);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
}

void SpectrumPlot::PaintGrid(QPainter& painter, const QRectF& area) {
  const QPalette& colours = palette();
  const QFontMetrics metrics(painter.font());

  const bool spectrogram = view_ == SpectrumView::kSpectrogram;

  painter.setPen(theme_tokens::GridLine(colours));

  if (spectrogram) {
    // Frequency runs up the side here, so its gridlines are horizontal.
    for (int line = 1; line * kGridFrequencyStepHz < maximum_frequency_hz_;
         ++line) {
      const double frequency = line * kGridFrequencyStepHz;
      const double y =
          area.bottom() - (frequency / maximum_frequency_hz_ * area.height());
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

    for (int line = 1; line * kGridFrequencyStepHz < maximum_frequency_hz_;
         ++line) {
      const double frequency = line * kGridFrequencyStepHz;
      const double x =
          area.left() + (frequency / maximum_frequency_hz_ * area.width());
      painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    }
  }

  painter.setPen(theme_tokens::MutedText(colours));

  if (spectrogram) {
    for (int label = 0; label * kGridFrequencyStepHz <= maximum_frequency_hz_;
         ++label) {
      const double frequency = label * kGridFrequencyStepHz;
      const double y =
          area.bottom() - (frequency / maximum_frequency_hz_ * area.height());
      painter.drawText(QRectF(0.0, y - (metrics.height() / 2.0),
                              kScaleWidthPixels - 6.0, metrics.height()),
                       Qt::AlignRight | Qt::AlignVCenter,
                       tr("%1").arg(frequency / 1'000'000.0, 0, 'f', 0));
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
    painter.drawText(QRectF(0.0, y - (metrics.height() / 2.0),
                            kScaleWidthPixels - 6.0, metrics.height()),
                     Qt::AlignRight | Qt::AlignVCenter,
                     tr("%1 dB").arg(level, 0, 'f', 0));
  }

  for (int label = 0; label * kGridFrequencyStepHz <= maximum_frequency_hz_;
       ++label) {
    const double frequency = label * kGridFrequencyStepHz;
    const double x =
        area.left() + (frequency / maximum_frequency_hz_ * area.width());
    painter.drawText(
        QRectF(x - 30.0, area.bottom() + 2.0, 60.0, kAxisHeightPixels - 2.0),
        Qt::AlignHCenter | Qt::AlignTop,
        tr("%1").arg(frequency / 1'000'000.0, 0, 'f', 0));
  }
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
    return;
  }

  PaintGrid(painter, area);

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

  if (view_ == SpectrumView::kSpectrogram) {
    // Frequency up the side, time across.
    const double up = std::clamp(
        (area.bottom() - event->position().y()) / area.height(), 0.0, 1.0);
    const double frequency = up * maximum_frequency_hz_;

    const size_t bands = std::max<size_t>(
        1, static_cast<size_t>(static_cast<double>(history_.columns()) *
                               DisplayedProportion()));
    const size_t band = std::min(
        bands - 1, static_cast<size_t>(up * static_cast<double>(bands - 1)));

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
  const double frequency = across * maximum_frequency_hz_;

  const size_t usable = std::max<size_t>(
      2, static_cast<size_t>(static_cast<double>(magnitudes_db_.size()) *
                             DisplayedProportion()));
  const size_t bin =
      std::min(usable - 1,
               static_cast<size_t>(across * static_cast<double>(usable - 1)));

  emit CursorMoved(frequency, magnitudes_db_[bin], -1.0);
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

  maximum_frequency_ = new QComboBox(this);
  maximum_frequency_->setObjectName(QLatin1String(kMaximumFrequencyComboName));
  for (size_t index = 0; index < analysis::kMaximumFrequencyChoiceCount;
       ++index) {
    const double hertz = analysis::kMaximumFrequencyChoicesHz[index];
    maximum_frequency_->addItem(
        tr("to %1 MHz").arg(hertz / 1'000'000.0, 0, 'f', 0), hertz);
    if (hertz == analysis::kDefaultMaximumFrequencyHz) {
      maximum_frequency_->setCurrentIndex(maximum_frequency_->count() - 1);
    }
  }
  maximum_frequency_->setToolTip(
      tr("The top of the displayed range. The board's anti-aliasing filter "
         "rolls off at 13.2 MHz, so 14 MHz puts its corner just inside the "
         "edge of the display; the wider ranges are for looking at what the "
         "filter is doing above it."));
  connect(maximum_frequency_, &QComboBox::currentIndexChanged, this,
          [this](int) {
            plot_->SetMaximumFrequency(
                maximum_frequency_->currentData().toDouble());
          });
  controls->addWidget(maximum_frequency_);

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

  // Peak hold is a property of the trace: the spectrogram already shows every
  // frame it would be summarising, so the controls are disabled rather than
  // left to do nothing when pressed.
  const bool trace = chosen == SpectrumView::kTrace;
  peak_hold_->setEnabled(trace);
  reset_->setEnabled(trace);
}

void SpectrumPanel::ApplyAveraging() {
  if (controller_ != nullptr) {
    controller_->analysis()->SetSpectrumAveraging(
        averaging_->currentData().toDouble());
  }
}

void SpectrumPanel::OnSpectrumReady(const std::vector<double>& magnitudes_db,
                                    const std::vector<double>& peak_hold_db) {
  plot_->SetSpectrum(magnitudes_db, peak_hold_db);

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

  // The worker is built fresh for each run, so the averaging it was told about
  // last time went with the old one. Re-applied here rather than remembered
  // there, because this control is the thing that knows what the user chose.
  ApplyAveraging();
}

void SpectrumPanel::ShowCursor(double frequency_hz, double level_db,
                               double seconds_ago) {
  cursor_->setText(FormatSpectrumCursor(frequency_hz, level_db, seconds_ago));
}

void SpectrumPanel::ClearCursor() {
  cursor_->setText(tr("Point at the trace to read a frequency"));
}

}  // namespace ddd::gui
