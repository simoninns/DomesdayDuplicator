/************************************************************************

    waveform_panel.cpp

    The scope: what the signal is doing right now
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "waveform_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <algorithm>

#include "capture_controller.h"
#include "sample_format.h"
#include "theme_color_tokens.h"

namespace ddd::gui {
namespace {

// Room for the code scale on the left. Not a layout margin, because the plot
// paints its own axis and a layout would put the labels outside the widget the
// trace is drawn in.
constexpr int kScaleWidthPixels = 46;
constexpr int kPlotMarginPixels = 6;

// How much of the accumulated picture survives each frame, as an alpha the old
// picture is multiplied by. Named for what it keeps rather than what it
// removes, because the first version of this was named for the fade and set to
// 40 — which keeps 16% of the picture per frame, not 16% less. Everything older
// than the current sweep was then drawn at around 2% opacity: present in the
// buffer, invisible on the screen, and indistinguishable from the option doing
// nothing.
//
// Frames arrive at the pipeline's snapshot rate, about nine a second, so 78% a
// frame leaves a tail of roughly a second — long enough to build an envelope
// out of a repeating waveform and short enough that the display still looks
// live.
constexpr int kPersistenceRetainedAlpha = 200;

}  // namespace

QString FormatWaveformSpan(size_t samples) {
  const double seconds = static_cast<double>(samples) /
                         static_cast<double>(capture::kSampleRateHz);

  // Significant figures rather than decimal places, so the round spans on offer
  // read as "500 µs" rather than "500.0 µs" without a span that is not round
  // losing its precision. The millisecond branch is not reachable from the
  // spans the panel offers — all of those are microseconds — and is kept
  // because this formats a span, not a menu entry.
  if (seconds < 1e-3) {
    return WaveformPanel::tr("%1 µs").arg(seconds * 1e6, 0, 'g', 3);
  }
  return WaveformPanel::tr("%1 ms").arg(seconds * 1e3, 0, 'g', 3);
}

QString FormatWaveformCursor(qint64 sample_index, double code,
                             const analysis::FrontEndGain& gain) {
  const double microseconds = static_cast<double>(sample_index) /
                              static_cast<double>(capture::kSampleRateHz) * 1e6;

  const QString position =
      WaveformPanel::tr("%1 µs").arg(microseconds, 0, 'f', 2);

  if (!gain.declared()) {
    // Codes and nothing else. The alternative — a millivolt figure derived from
    // a gain nobody has stated — would be a number the user could act on and
    // could not check.
    return WaveformPanel::tr("%1, code %2").arg(position).arg(code, 0, 'f', 0);
  }

  return WaveformPanel::tr("%1, code %2  (%3 mV)")
      .arg(position)
      .arg(code, 0, 'f', 0)
      .arg(gain.CodeToInputMillivolts(code), 0, 'f', 1);
}

WaveformPlot::WaveformPlot(QWidget* parent) : QWidget(parent) {
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

void WaveformPlot::SetCodes(const std::vector<uint16_t>& codes) {
  codes_ = codes;
  update();
}

void WaveformPlot::SetSampleSpan(size_t span) {
  sample_span_ = std::max<size_t>(span, 1);
  persistence_image_ = QImage();
  update();
}

void WaveformPlot::SetPersistence(bool enabled) {
  persistence_ = enabled;
  persistence_image_ = QImage();
  update();
}

void WaveformPlot::Clear() {
  codes_.clear();
  persistence_image_ = QImage();
  update();
}

void WaveformPlot::resizeEvent(QResizeEvent* event) {
  // The accumulated picture is tied to the pixels it was drawn on, so a resize
  // starts it again rather than stretching a stale one.
  persistence_image_ = QImage();
  QWidget::resizeEvent(event);
}

analysis::WaveformMapping WaveformPlot::Mapping() const {
  analysis::WaveformMapping mapping;
  mapping.width_pixels =
      std::max(0, width() - kScaleWidthPixels - kPlotMarginPixels);
  mapping.height_pixels = std::max(0, height() - (2 * kPlotMarginPixels));
  mapping.first_sample = 0;
  mapping.sample_span = std::min(sample_span_, codes_.size());
  return mapping;
}

void WaveformPlot::PaintTrace(QPainter& painter,
                              const analysis::WaveformMapping& mapping,
                              const QColor& colour) {
  analysis::DecimateToColumns(codes_.data(), codes_.size(), mapping, columns_);

  painter.setPen(QPen(colour, 1.0));

  for (size_t column = 0; column < columns_.size(); ++column) {
    const analysis::WaveformColumn& values = columns_[column];
    if (!values.populated) {
      continue;
    }

    const double x = static_cast<double>(column) + 0.5;
    const double top = mapping.CodeToY(values.maximum);
    const double bottom = mapping.CodeToY(values.minimum);

    // A column whose samples were all equal still has to draw something, so the
    // line is given a minimum length of one pixel rather than collapsing to a
    // zero-height line that some painters drop entirely.
    painter.drawLine(QPointF(x, top), QPointF(x, std::max(bottom, top + 1.0)));
  }
}

void WaveformPlot::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QPalette& colours = palette();
  const bool dark = theme_tokens::IsDarkPalette(colours);

  painter.fillRect(rect(), colours.color(QPalette::Base));

  const analysis::WaveformMapping mapping = Mapping();
  if (!mapping.Valid()) {
    return;
  }

  painter.save();
  painter.translate(kScaleWidthPixels, kPlotMarginPixels);

  // The three lines that give the trace a meaning: the clip levels it must not
  // reach and the centre it should sit on.
  const struct {
    double code;
    theme_tokens::PlotColorToken token;
    const char* label;
  } guides[] = {
      {static_cast<double>(capture::kMaximumSampleValue),
       theme_tokens::PlotColorToken::kClipMarker, "1023"},
      {static_cast<double>(capture::kSampleZeroOffset),
       theme_tokens::PlotColorToken::kZeroReference, "512"},
      {static_cast<double>(capture::kMinimumSampleValue),
       theme_tokens::PlotColorToken::kClipMarker, "0"},
  };

  for (const auto& guide : guides) {
    const double y = mapping.CodeToY(guide.code);
    painter.setPen(
        QPen(theme_tokens::PlotColor(guide.token, dark), 1.0, Qt::DashLine));
    painter.drawLine(QPointF(0.0, y),
                     QPointF(static_cast<double>(mapping.width_pixels), y));
  }

  painter.setPen(theme_tokens::GridLine(colours));
  for (int step = 1; step < 4; ++step) {
    const double x = static_cast<double>(mapping.width_pixels) * step / 4.0;
    painter.drawLine(QPointF(x, 0.0),
                     QPointF(x, static_cast<double>(mapping.height_pixels)));
  }

  if (!codes_.empty()) {
    const QColor trace = theme_tokens::PlotColor(
        theme_tokens::PlotColorToken::kSignalTrace, dark);

    if (!persistence_) {
      PaintTrace(painter, mapping, trace);
    } else {
      if (persistence_image_.size() !=
          QSize(mapping.width_pixels, mapping.height_pixels)) {
        persistence_image_ = QImage(mapping.width_pixels, mapping.height_pixels,
                                    QImage::Format_ARGB32_Premultiplied);
        persistence_image_.fill(Qt::transparent);
      }

      QPainter accumulator(&persistence_image_);
      accumulator.setCompositionMode(QPainter::CompositionMode_DestinationIn);
      accumulator.fillRect(persistence_image_.rect(),
                           QColor(0, 0, 0, kPersistenceRetainedAlpha));
      accumulator.setCompositionMode(QPainter::CompositionMode_SourceOver);

      // The newest sweep at full strength, exactly as it would be drawn without
      // persistence. Dimming it here made the whole display look washed out
      // while adding nothing: the fade above is what separates old from new.
      PaintTrace(accumulator, mapping, trace);
      accumulator.end();

      painter.drawImage(0, 0, persistence_image_);
    }
  }

  painter.restore();

  painter.setPen(theme_tokens::MutedText(colours));
  const QFontMetrics metrics(painter.font());
  for (const auto& guide : guides) {
    const double y = mapping.CodeToY(guide.code) + kPlotMarginPixels;
    painter.drawText(QRectF(0.0, y - (metrics.height() / 2.0),
                            kScaleWidthPixels - 6.0, metrics.height()),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString::fromUtf8(guide.label));
  }
}

void WaveformPlot::mouseMoveEvent(QMouseEvent* event) {
  const analysis::WaveformMapping mapping = Mapping();
  if (!mapping.Valid()) {
    QWidget::mouseMoveEvent(event);
    return;
  }

  const double x = event->position().x() - kScaleWidthPixels;
  const double y = event->position().y() - kPlotMarginPixels;

  if (x < 0.0 || x > mapping.width_pixels || y < 0.0 ||
      y > mapping.height_pixels) {
    emit CursorLeft();
    QWidget::mouseMoveEvent(event);
    return;
  }

  const size_t sample = mapping.XToSample(x);

  // The sample's own value, not the code the pointer is at: the readout is
  // there to say what the trace is doing, and a figure that changed as the
  // pointer moved up and down a vertical line would say nothing.
  const double code = sample < codes_.size()
                          ? static_cast<double>(codes_[sample])
                          : mapping.YToCode(y);

  emit CursorMoved(static_cast<qint64>(sample), code);
  QWidget::mouseMoveEvent(event);
}

void WaveformPlot::leaveEvent(QEvent* event) {
  emit CursorLeft();
  QWidget::leaveEvent(event);
}

WaveformPanel::WaveformPanel(CaptureController* controller, QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);

  plot_ = new WaveformPlot(this);
  plot_->setObjectName(QLatin1String(kPlotName));
  layout->addWidget(plot_, 1);

  auto* controls = new QHBoxLayout();

  span_ = new QComboBox(this);
  span_->setObjectName(QLatin1String(kSpanComboName));
  for (size_t index = 0; index < analysis::kWaveformSpanChoiceCount; ++index) {
    const size_t samples = analysis::kWaveformSpanChoices[index];
    span_->addItem(FormatWaveformSpan(samples),
                   static_cast<qulonglong>(samples));
  }
  span_->setCurrentIndex(2);
  connect(span_, &QComboBox::currentIndexChanged, this, [this](int) {
    plot_->SetSampleSpan(
        static_cast<size_t>(span_->currentData().toULongLong()));
  });
  controls->addWidget(new QLabel(tr("Span"), this));
  controls->addWidget(span_);

  persistence_ = new QCheckBox(tr("Persistence"), this);
  persistence_->setObjectName(QLatin1String(kPersistenceBoxName));
  persistence_->setToolTip(
      tr("Let each trace fade rather than replacing it, so a repeating "
         "waveform builds up its envelope. The way to see the shape of an FM "
         "carrier rather than one arbitrary slice of it."));
  connect(persistence_, &QCheckBox::toggled, this,
          [this](bool on) { plot_->SetPersistence(on); });
  controls->addWidget(persistence_);

  controls->addStretch();

  cursor_ = new QLabel(this);
  cursor_->setObjectName(QLatin1String(kCursorLabelName));
  cursor_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  controls->addWidget(cursor_);

  layout->addLayout(controls);

  connect(plot_, &WaveformPlot::CursorMoved, this, &WaveformPanel::ShowCursor);
  connect(plot_, &WaveformPlot::CursorLeft, this, &WaveformPanel::ClearCursor);

  ClearCursor();

  if (controller != nullptr) {
    gain_ = controller->settings().DeclaredGain();

    connect(controller->analysis(), &AnalysisWorker::WaveformReady, this,
            &WaveformPanel::OnWaveformReady);
    connect(controller, &CaptureController::MonitoringChanged, this,
            &WaveformPanel::OnMonitoringChanged);
    connect(controller, &CaptureController::SettingsChanged, this,
            [this](const CaptureSettings& settings) {
              SetFrontEndGain(settings.DeclaredGain());
            });
  }
}

void WaveformPanel::OnWaveformReady(const std::vector<uint16_t>& codes) {
  plot_->SetCodes(codes);
}

void WaveformPanel::OnMonitoringChanged(bool monitoring) {
  if (monitoring) {
    // Cleared at the start rather than at the end, so that the last trace of a
    // finished run stays on screen to be looked at.
    plot_->Clear();
    ClearCursor();
  }
}

void WaveformPanel::SetFrontEndGain(analysis::FrontEndGain gain) {
  gain_ = gain;
  // Nothing is recomputed: the trace is drawn from codes and always was. Only
  // the readout's units change, which is the whole point of keeping the
  // declaration out of the data.
  ClearCursor();
}

void WaveformPanel::ShowCursor(qint64 sample_index, double code) {
  cursor_->setText(FormatWaveformCursor(sample_index, code, gain_));
}

void WaveformPanel::ClearCursor() {
  cursor_->setText(tr("Point at the trace to read a sample"));
}

}  // namespace ddd::gui
