/************************************************************************

    amplitude_panel.cpp

    Signal level over minutes, which is where a fault shows up
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "amplitude_panel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPolygonF>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

#include "capture_controller.h"
#include "gain_choices.h"
#include "sample_format.h"
#include "signal_levels.h"
#include "theme_color_tokens.h"

namespace ddd::gui {
namespace {

constexpr int kScaleWidthPixels = 60;
constexpr int kPlotMarginPixels = 6;
constexpr int kClipTickHeightPixels = 6;

constexpr double kCodeTop = 1023.0;
constexpr double kCodeBottom = 0.0;

double CodeToY(double code, const QRectF& area) {
  const double proportion = (code - kCodeBottom) / (kCodeTop - kCodeBottom);
  return area.bottom() - (std::clamp(proportion, 0.0, 1.0) * area.height());
}

}  // namespace

QString FormatAmplitudeSummary(const analysis::AmplitudeHistory& history,
                               const analysis::FrontEndGain& gain) {
  if (history.empty()) {
    return AmplitudePanel::tr("Nothing recorded yet");
  }

  const double span = static_cast<double>(history.PeakCode()) -
                      static_cast<double>(history.TroughCode());

  QString level;
  if (gain.declared()) {
    level = AmplitudePanel::tr("%1 to %2 of 1023  (%3 mV p-p)")
                .arg(history.TroughCode())
                .arg(history.PeakCode())
                .arg(gain.CodeSpanToInputMillivolts(span), 0, 'f', 0);
  } else {
    // No millivolt figure, and no apology for it: the level is real and the
    // conversion to volts is the part nobody has supplied.
    level = AmplitudePanel::tr("%1 to %2 of 1023")
                .arg(history.TroughCode())
                .arg(history.PeakCode());
  }

  const uint64_t clipped = history.TotalClipped();
  if (clipped == 0) {
    return AmplitudePanel::tr("%1, no clipping").arg(level);
  }

  return AmplitudePanel::tr("%1, %2 samples clipped").arg(level).arg(clipped);
}

AmplitudePlot::AmplitudePlot(QWidget* parent) : QWidget(parent) {
  setMinimumHeight(60);
  // Small on purpose. The size policy below is Expanding, so the plot fills
  // whatever it is given; the minimum is only what it must never be squeezed
  // below. Set to the height that looks right instead, three of these stacked
  // in one dock column demand more than the column has, and the separators
  // between them stop moving — a panel that cannot be resized because it
  // insisted on being comfortable.
  setAutoFillBackground(false);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void AmplitudePlot::SetHistory(const analysis::AmplitudeHistory* history) {
  history_ = history;
  update();
}

void AmplitudePlot::SetWindowSeconds(double seconds) {
  window_seconds_ = seconds;
  update();
}

void AmplitudePlot::SetFrontEndGain(analysis::FrontEndGain gain) {
  gain_ = gain;
  // The history is not touched. It holds codes, it always did, and the only
  // thing a new declaration changes is what the axis beside them says — which
  // is why correcting a wrong declaration re-labels history already recorded
  // instead of discarding it.
  update();
}

void AmplitudePlot::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);

  QPainter painter(this);
  const QPalette& colours = palette();
  const bool dark = theme_tokens::IsDarkPalette(colours);

  painter.fillRect(rect(), colours.color(QPalette::Base));

  const QRectF area(kScaleWidthPixels, kPlotMarginPixels,
                    std::max(0.0, width() - kScaleWidthPixels -
                                      static_cast<double>(kPlotMarginPixels)),
                    std::max(0.0, height() - (2.0 * kPlotMarginPixels)));
  if (area.width() < 2.0 || area.height() < 2.0) {
    return;
  }

  const QFontMetrics metrics(painter.font());

  const double guides[] = {kCodeTop, 768.0, 512.0, 256.0, kCodeBottom};

  painter.setPen(theme_tokens::GridLine(colours));
  for (const double code : guides) {
    const double y = CodeToY(code, area);
    painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
  }

  painter.setPen(theme_tokens::MutedText(colours));
  for (const double code : guides) {
    const double y = CodeToY(code, area);
    const QString label =
        gain_.declared()
            ? tr("%1 mV").arg(gain_.CodeToInputMillivolts(code), 0, 'f', 0)
            : tr("%1").arg(code, 0, 'f', 0);
    painter.drawText(QRectF(0.0, y - (metrics.height() / 2.0),
                            kScaleWidthPixels - 6.0, metrics.height()),
                     Qt::AlignRight | Qt::AlignVCenter, label);
  }

  // The nominal bounds, drawn on both sides of mid-scale because the signal
  // swings both ways about 0 V and a bound on one side would say nothing about
  // the half of the waveform that was already closer to the rail.
  //
  // Drawn before the early return below, so they are on screen as a scale
  // reference before there is any history to judge against them.
  const QColor nominal = theme_tokens::PlotColor(
      theme_tokens::PlotColorToken::kNominalLimit, dark);
  painter.setPen(QPen(nominal, 1.0, Qt::DashLine));

  const double bounds[] = {analysis::NominalUpperCode(),
                           analysis::NominalLowerCode()};
  for (const double code : bounds) {
    const double y = CodeToY(code, area);
    painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
  }

  painter.setPen(nominal);
  for (const double code : bounds) {
    const double y = CodeToY(code, area);
    // Against the right-hand edge and centred on its own line, so the label
    // cannot be read as belonging to the axis on the left, which is in
    // different units the moment a gain is declared.
    painter.drawText(
        QRectF(area.right() - 46.0, y - (metrics.height() / 2.0), 42.0,
               metrics.height()),
        Qt::AlignRight | Qt::AlignVCenter,
        tr("%1%").arg(analysis::kNominalPeakProportion * 100.0, 0, 'f', 0));
  }

  if (history_ == nullptr || history_->empty()) {
    return;
  }

  const size_t count = history_->size();
  const double newest = history_->Newest().seconds;

  // Time-proportional and anchored to the right, so "now" is always at the
  // right-hand edge and a point's distance from it is how long ago it was.
  // Positioning by index instead would make a gap in the history — a run that
  // stalled, or a sampler that skipped an interval — look like time passing at
  // a different rate.
  //
  // A window of zero means everything held, spread across the whole width. Any
  // other window is a fixed span, which is what lets this panel be made to keep
  // pace with the spectrogram.
  const double window = window_seconds_ > 0.0
                            ? window_seconds_
                            : std::max(history_->SpanSeconds(), 1e-6);

  const auto position = [&](size_t index) {
    const double ago = newest - history_->At(index).seconds;
    return area.right() - (ago / window * area.width());
  };

  size_t first = 0;
  while (first < count && (newest - history_->At(first).seconds) > window) {
    ++first;
  }
  const size_t visible = count - first;

  if (visible == 0) {
    return;
  }

  // The envelope as a filled band rather than two lines: what a user is looking
  // for is a moment when the band narrowed or moved, and a shape shows that
  // where a pair of lines makes it something to work out.
  QPolygonF band;
  band.reserve(static_cast<int>(visible * 2));
  for (size_t index = first; index < count; ++index) {
    band.append(QPointF(position(index),
                        CodeToY(history_->At(index).maximum_code, area)));
  }
  for (size_t index = count; index > first; --index) {
    band.append(QPointF(position(index - 1),
                        CodeToY(history_->At(index - 1).minimum_code, area)));
  }

  QColor envelope = theme_tokens::PlotColor(
      theme_tokens::PlotColorToken::kAmplitudeEnvelope, dark);
  envelope.setAlpha(110);
  painter.setPen(Qt::NoPen);
  painter.setBrush(envelope);
  painter.drawPolygon(band);
  painter.setBrush(Qt::NoBrush);

  // RMS is a magnitude, and the signal it measures swings both ways about 0 V —
  // so it is drawn as a pair of traces, at plus and minus that level either
  // side of mid-scale. Drawn on one side only it reads as though the waveform
  // were sitting above the centre line, which is exactly what it is not doing.
  QPolygonF rms_above;
  QPolygonF rms_below;
  rms_above.reserve(static_cast<int>(visible));
  rms_below.reserve(static_cast<int>(visible));

  for (size_t index = first; index < count; ++index) {
    const double x = position(index);
    const double level = history_->At(index).rms_codes;
    const double centre = static_cast<double>(capture::kSampleZeroOffset);

    rms_above.append(QPointF(x, CodeToY(centre + level, area)));
    rms_below.append(QPointF(x, CodeToY(centre - level, area)));
  }

  painter.setPen(QPen(theme_tokens::PlotColor(
                          theme_tokens::PlotColorToken::kAmplitudeTrace, dark),
                      1.5));
  painter.drawPolyline(rms_above);
  painter.drawPolyline(rms_below);

  painter.setPen(QPen(
      theme_tokens::PlotColor(theme_tokens::PlotColorToken::kClipMarker, dark),
      2.0));
  for (size_t index = first; index < count; ++index) {
    if (history_->At(index).clipped_count == 0) {
      continue;
    }
    const double x = position(index);
    painter.drawLine(QPointF(x, area.top()),
                     QPointF(x, area.top() + kClipTickHeightPixels));
  }
}

AmplitudePanel::AmplitudePanel(CaptureController* controller, QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);

  plot_ = new AmplitudePlot(this);
  plot_->setObjectName(QLatin1String(kPlotName));
  plot_->SetHistory(&history_);
  layout->addWidget(plot_, 1);

  auto* controls = new QHBoxLayout();

  span_ = new QComboBox(this);
  span_->setObjectName(QLatin1String(kSpanComboName));
  span_->addItem(tr("All"), 0.0);
  span_->addItem(tr("Match spectrogram"), -1.0);
  span_->setToolTip(
      tr("All shows everything the history holds — five minutes once it has "
         "filled. Matching the spectrogram narrows it to the same span that "
         "panel is showing, so the two scroll at the same pace and a moment on "
         "one lines up with the same moment on the other."));
  connect(span_, &QComboBox::currentIndexChanged, this,
          [this](int) { ApplySpan(); });
  controls->addWidget(span_);

  summary_ = new QLabel(this);
  summary_->setObjectName(QLatin1String(kSummaryLabelName));
  summary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  controls->addWidget(summary_);

  controls->addStretch();

  clear_ = new QPushButton(tr("Clear history"), this);
  clear_->setObjectName(QLatin1String(kClearButtonName));
  clear_->setToolTip(
      tr("Start the history again from now. The run is not affected — this is "
         "for after a cable or a gain setting has been changed, when what came "
         "before is no longer what is being measured."));
  connect(clear_, &QPushButton::clicked, this, &AmplitudePanel::ClearHistory);
  controls->addWidget(clear_);

  layout->addLayout(controls);

  gain_label_ = new QLabel(this);
  gain_label_->setObjectName(QLatin1String(kGainLabelName));
  gain_label_->setWordWrap(true);
  gain_label_->setForegroundRole(QPalette::PlaceholderText);
  layout->addWidget(gain_label_);

  if (controller != nullptr) {
    SetFrontEndGain(controller->settings().DeclaredGain());

    connect(controller, &CaptureController::StatsUpdated, this,
            &AmplitudePanel::OnStatsUpdated);
    connect(controller, &CaptureController::MonitoringChanged, this,
            &AmplitudePanel::OnMonitoringChanged);
    connect(controller, &CaptureController::SettingsChanged, this,
            [this](const CaptureSettings& settings) {
              SetFrontEndGain(settings.DeclaredGain());
            });
  } else {
    SetFrontEndGain(analysis::FrontEndGain());
  }

  UpdateSummary();
  ApplySpan();
}

void AmplitudePanel::OnStatsUpdated(const ddd::capture::CaptureStats& stats) {
  const std::optional<analysis::AmplitudePoint> point =
      sampler_.Observe(stats.elapsed_seconds, stats.metrics);
  if (!point.has_value()) {
    return;
  }

  history_.Append(point.value_or(analysis::AmplitudePoint{}));
  plot_->Refresh();
  UpdateSummary();
}

void AmplitudePanel::OnMonitoringChanged(bool monitoring) {
  if (!monitoring) {
    // A finished run's history stays on screen. It is the record of what just
    // happened, and clearing it at the end would throw away the evidence at
    // exactly the moment somebody wanted to look at it.
    return;
  }

  history_.Clear();
  sampler_.Reset();
  plot_->Refresh();
  UpdateSummary();
}

void AmplitudePanel::SetFrontEndGain(analysis::FrontEndGain gain) {
  gain_ = gain;
  plot_->SetFrontEndGain(gain);
  gain_label_->setText(tr("Front-end gain: %1")
                           .arg(DescribeFrontEndGain(gain.switch_pattern())));
  UpdateSummary();
}

void AmplitudePanel::SetMatchedWindowSeconds(double seconds) {
  matched_window_seconds_ = seconds;
  ApplySpan();
}

void AmplitudePanel::ApplySpan() {
  const double chosen = span_->currentData().toDouble();

  // A matched span of nothing means the spectrogram has not measured its own
  // window yet — it needs two frames to know the interval. Showing everything
  // until it has is better than showing nothing.
  const double window = chosen < 0.0 ? matched_window_seconds_ : 0.0;

  plot_->SetWindowSeconds(window);
}

void AmplitudePanel::ClearHistory() {
  // The sampler goes with it. Left alone it would still be holding the clip
  // total it last saw, and the first point after a clear would report the
  // difference against a run of history that no longer exists.
  history_.Clear();
  sampler_.Reset();
  plot_->Refresh();
  UpdateSummary();
}

void AmplitudePanel::UpdateSummary() {
  summary_->setText(FormatAmplitudeSummary(history_, gain_));
}

}  // namespace ddd::gui
