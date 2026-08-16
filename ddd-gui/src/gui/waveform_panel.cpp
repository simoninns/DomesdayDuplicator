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
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

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

// Room for the time axis below the trace. A scope without one is a picture of a
// waveform rather than a measurement of it.
constexpr int kAxisHeightPixels = 18;

// How many sweeps are taken from one snapshot.
//
// A snapshot is 819 µs and holds several thousand crossings of the trigger
// level, so this is a choice about the picture rather than about what is
// available. Thirty-two sweeps nine times a second is an effective sweep rate
// near three hundred a second — enough, accumulated under the persistence fade,
// to draw the envelope of an FM carrier's deviation rather than one instant of
// it. That is the whole difference between this and the same trace drawn nine
// times a second, and it costs nothing extra to acquire: the samples were
// already there.
constexpr size_t kMaximumSweeps = 32;

// The radius of a sample marker, in pixels.
constexpr double kSampleMarkerRadius = 1.5;

// The interval to assume for the very first fade, before two snapshots have
// arrived and the real one is known. The pipeline publishes about nine a
// second.
constexpr double kNominalSnapshotSeconds = 1.0 / 9.0;

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

QString FormatPersistence(double seconds) {
  if (seconds <= 0.0) {
    return WaveformPanel::tr("off");
  }
  // Two significant figures: the settings are quarters of a second, so "0.25"
  // and "1.5" both need to read exactly rather than round to each other.
  return WaveformPanel::tr("%1 s").arg(seconds, 0, 'g', 2);
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
  sweeps_valid_ = false;

  // A new snapshot is what the fade is measured in. See the alpha above.
  persistence_pending_ = true;

  update();
}

void WaveformPlot::SetSampleSpan(size_t span) {
  sample_span_ = std::max<size_t>(span, 1);
  sweeps_valid_ = false;
  persistence_image_ = QImage();
  update();
}

int WaveformPlot::RetainedAlpha(double seconds, double elapsed_seconds) {
  if (seconds <= 0.0 || elapsed_seconds < 0.0) {
    return 0;
  }

  // Clamped below one so that a run of frames arriving in the same millisecond
  // cannot leave the picture never fading at all.
  const double retained =
      std::clamp(std::exp(-elapsed_seconds / seconds), 0.0, 0.996);
  return static_cast<int>(std::lround(retained * 255.0));
}

void WaveformPlot::SetPersistenceSeconds(double seconds) {
  const double wanted = std::max(0.0, seconds);

  // Turning the mode on or off starts the picture again; moving the tail
  // between two non-zero lengths keeps what is already accumulated, because
  // there is nothing wrong with it — only the rate it will fade at changes.
  if ((wanted > 0.0) != (persistence_seconds_ > 0.0)) {
    persistence_image_ = QImage();
    fade_clock_.invalidate();
  }

  persistence_seconds_ = wanted;
  update();
}

void WaveformPlot::SetTriggered(bool enabled) {
  triggered_ = enabled;
  sweeps_valid_ = false;
  // The accumulated picture was built from sweeps aligned one way and would be
  // meaningless under the other.
  persistence_image_ = QImage();
  update();
}

void WaveformPlot::Clear() {
  codes_.clear();
  sweeps_.clear();
  sweeps_valid_ = false;
  persistence_pending_ = false;
  persistence_image_ = QImage();
  fade_clock_.invalidate();
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
  mapping.height_pixels =
      std::max(0, height() - kPlotMarginPixels - kAxisHeightPixels);
  mapping.first_sample = 0;
  mapping.sample_span = std::min(sample_span_, codes_.size());
  return mapping;
}

analysis::WaveformMapping WaveformPlot::MappingAt(double origin) const {
  analysis::WaveformMapping mapping = Mapping();

  const double whole = std::floor(std::max(0.0, origin));
  mapping.first_sample = static_cast<size_t>(whole);
  mapping.sub_sample_offset = std::max(0.0, origin) - whole;
  return mapping;
}

void WaveformPlot::FindSweeps() {
  if (sweeps_valid_) {
    return;
  }
  sweeps_valid_ = true;
  sweeps_.clear();
  armed_ = false;

  const size_t span = std::min(sample_span_, codes_.size());
  if (codes_.empty() || span == 0) {
    return;
  }

  if (!triggered_) {
    sweeps_.push_back(0.0);
    return;
  }

  analysis::TriggerOptions options;

  // Spread across the snapshot rather than taken from the first few
  // microseconds of it: a carrier crosses the level every sixty nanoseconds, so
  // without this every sweep would come from the same instant and the
  // accumulated picture would say nothing the single sweep did not.
  options.minimum_separation =
      std::max<size_t>(span, codes_.size() / kMaximumSweeps);

  analysis::FindTriggers(codes_.data(), codes_.size(), options, kMaximumSweeps,
                         triggers_);

  const double pre = analysis::kPreTriggerFraction * static_cast<double>(span);

  for (const double trigger : triggers_) {
    const double origin = trigger - pre;
    if (origin < 0.0) {
      continue;
    }
    if (origin + static_cast<double>(span) >
        static_cast<double>(codes_.size())) {
      break;
    }
    sweeps_.push_back(origin);
  }

  // Nothing crossed the level — a flat input, or one that never came back down
  // far enough to arm. Free-running is what a scope does here, and it is much
  // better than freezing: an unmodulated or absent signal is exactly when
  // somebody needs to see that the trace is flat.
  //
  // Recorded rather than silently substituted, because the picture that comes
  // out is a picture the trigger had no part in and the display says so.
  if (sweeps_.empty()) {
    sweeps_.push_back(0.0);
    return;
  }

  armed_ = true;
}

void WaveformPlot::PaintSweep(QPainter& painter, double origin,
                              const QColor& colour, bool mark_samples) {
  const analysis::WaveformMapping mapping = MappingAt(origin);
  if (!mapping.Valid()) {
    return;
  }

  painter.setPen(QPen(colour, 1.0));

  const analysis::WaveformDrawStyle style = mapping.DrawStyle();

  // Straight edges for the envelope's vertical bars, smooth ones for a curve.
  // A reconstructed carrier drawn without antialiasing is a staircase, which is
  // exactly the artefact the reconstruction is there to remove.
  painter.setRenderHint(QPainter::Antialiasing,
                        style != analysis::WaveformDrawStyle::kEnvelope);

  if (style == analysis::WaveformDrawStyle::kEnvelope) {
    analysis::DecimateToColumns(codes_.data(), codes_.size(), mapping,
                                columns_);

    for (size_t column = 0; column < columns_.size(); ++column) {
      const analysis::WaveformColumn& values = columns_[column];
      if (!values.populated) {
        continue;
      }

      const double x = static_cast<double>(column) + 0.5;
      const double top = mapping.CodeToY(values.maximum);
      const double bottom = mapping.CodeToY(values.minimum);

      // A column whose samples were all equal still has to draw something, so
      // the line is given a minimum length of one pixel rather than collapsing
      // to a zero-height line that some painters drop entirely.
      painter.drawLine(QPointF(x, top),
                       QPointF(x, std::max(bottom, top + 1.0)));
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
    return;
  }

  points_.clear();

  if (style == analysis::WaveformDrawStyle::kPolyline) {
    const size_t last =
        std::min(mapping.first_sample + mapping.sample_span, codes_.size());
    for (size_t index = mapping.first_sample; index < last; ++index) {
      points_.append(QPointF(mapping.SampleToX(static_cast<double>(index)),
                             mapping.CodeToY(codes_[index])));
    }
  } else {
    // One point per pixel of the band-limited waveform the samples determine.
    // Joining the samples themselves here would cut the corners off every
    // crest — at five samples to a cycle, by as much as a fifth of the
    // amplitude.
    for (int x = 0; x <= mapping.width_pixels; ++x) {
      const double position = mapping.XToSamplePosition(x);
      points_.append(QPointF(x, mapping.CodeToY(kernel_.Evaluate(
                                    codes_.data(), codes_.size(), position))));
    }
  }

  painter.drawPolyline(points_);

  // The measured points, so that what was sampled stays distinguishable from
  // what was reconstructed between the samples.
  if (mark_samples && mapping.ShouldMarkSamples()) {
    painter.setBrush(colour);
    const size_t last =
        std::min(mapping.first_sample + mapping.sample_span + 1, codes_.size());
    for (size_t index = mapping.first_sample; index < last; ++index) {
      painter.drawEllipse(QPointF(mapping.SampleToX(static_cast<double>(index)),
                                  mapping.CodeToY(codes_[index])),
                          kSampleMarkerRadius, kSampleMarkerRadius);
    }
    painter.setBrush(Qt::NoBrush);
  }

  painter.setRenderHint(QPainter::Antialiasing, false);
}

void WaveformPlot::PaintTrace(QPainter& painter, const QColor& colour) {
  FindSweeps();
  if (sweeps_.empty()) {
    return;
  }

  if (persistence_seconds_ <= 0.0) {
    // One sweep. Thirty drawn over each other at full strength with no fade
    // between them would be a filled band rather than a waveform — the fade is
    // what separates them, and without persistence there is none.
    PaintSweep(painter, sweeps_.front(), colour, true);
    return;
  }

  for (const double origin : sweeps_) {
    PaintSweep(painter, origin, colour, false);
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

  // What the sweeps are, before they are drawn: with a snapshot in hand this
  // is what says whether the trigger found anything to align to.
  FindSweeps();

  // Where every sweep was started from. Worth drawing because a reader
  // measuring a feature needs to know which edge the picture is anchored to,
  // and because a trace that is standing still for no visible reason is
  // unnerving.
  //
  // Only when something actually armed it. A marker over a free-running trace
  // would be pointing at a sample that has no more claim to be the start of a
  // cycle than any other.
  if (triggered_ && armed_) {
    const double x = analysis::kPreTriggerFraction *
                     static_cast<double>(mapping.width_pixels);
    painter.setPen(QPen(theme_tokens::PlotColor(
                            theme_tokens::PlotColorToken::kTriggerMarker, dark),
                        1.0, Qt::DashLine));
    painter.drawLine(QPointF(x, 0.0),
                     QPointF(x, static_cast<double>(mapping.height_pixels)));
  }

  if (!codes_.empty()) {
    const QColor trace = theme_tokens::PlotColor(
        theme_tokens::PlotColorToken::kSignalTrace, dark);

    if (persistence_seconds_ <= 0.0) {
      PaintTrace(painter, trace);
    } else {
      if (persistence_image_.size() !=
          QSize(mapping.width_pixels, mapping.height_pixels)) {
        persistence_image_ = QImage(mapping.width_pixels, mapping.height_pixels,
                                    QImage::Format_ARGB32_Premultiplied);
        persistence_image_.fill(Qt::transparent);
        persistence_pending_ = true;
      }

      // Only when a snapshot has arrived. A paint on its own adds nothing to
      // accumulate and must not age what is already there.
      if (persistence_pending_) {
        const double elapsed =
            fade_clock_.isValid()
                ? static_cast<double>(fade_clock_.restart()) / 1000.0
                : kNominalSnapshotSeconds;
        if (!fade_clock_.isValid()) {
          fade_clock_.start();
        }

        QPainter accumulator(&persistence_image_);
        accumulator.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        accumulator.fillRect(
            persistence_image_.rect(),
            QColor(0, 0, 0, RetainedAlpha(persistence_seconds_, elapsed)));
        accumulator.setCompositionMode(QPainter::CompositionMode_SourceOver);

        // Every sweep at full strength, exactly as one would be drawn without
        // persistence. Dimming them here made the whole display look washed out
        // while adding nothing: the fade above is what separates old from new.
        PaintTrace(accumulator, trace);
        accumulator.end();

        persistence_pending_ = false;
      }

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

  // And when the trigger found nothing, the fact that it did not.
  //
  // In the corner of the plot rather than in the control row, because it is a
  // property of the picture and not a setting: the box stays ticked, since the
  // trigger is still on and still looking. What has changed is what is on
  // screen, and that is where it is said.
  if (free_running() && !codes_.empty()) {
    painter.drawText(
        QRectF(kScaleWidthPixels + 4.0, kPlotMarginPixels + 2.0,
               static_cast<double>(mapping.width_pixels), metrics.height()),
        Qt::AlignLeft | Qt::AlignTop, tr("free running"));
  }

  // The time axis, on the gridlines already drawn. Measured from the left-hand
  // edge of the sweep rather than from the trigger, so the figures mean the
  // same thing whether the trigger is on or off; the marker above says where
  // the trigger itself is.
  const double axis_top = kPlotMarginPixels + mapping.height_pixels + 2.0;
  constexpr int kAxisMarks = 4;

  for (int step = 0; step <= kAxisMarks; ++step) {
    const double fraction = static_cast<double>(step) / kAxisMarks;
    const double x = kScaleWidthPixels +
                     (fraction * static_cast<double>(mapping.width_pixels));
    const double microseconds =
        fraction * static_cast<double>(mapping.sample_span) /
        static_cast<double>(capture::kSampleRateHz) * 1e6;

    // The end labels are pulled inside the plot rather than centred on its
    // edges, where half of each would be off the widget: a time axis whose last
    // mark reads "1 µ" is worse than one that does not quite reach the end.
    constexpr double kLabelWidth = 60.0;
    QRectF box(x - (kLabelWidth / 2.0), axis_top, kLabelWidth,
               kAxisHeightPixels - 2.0);
    Qt::Alignment alignment = Qt::AlignHCenter | Qt::AlignTop;

    if (step == 0) {
      box.moveLeft(x);
      alignment = Qt::AlignLeft | Qt::AlignTop;
    } else if (step == kAxisMarks) {
      box.moveLeft(x - kLabelWidth);
      alignment = Qt::AlignRight | Qt::AlignTop;
    }

    painter.drawText(box, alignment, tr("%1 µs").arg(microseconds, 0, 'g', 3));
  }
}

void WaveformPlot::mouseMoveEvent(QMouseEvent* event) {
  FindSweeps();

  // The sweep the pointer is over is the one drawn without persistence — the
  // first. With persistence there are thirty on screen and no way to say which
  // one the pointer is on, so the readout stays with the one that is definitely
  // there.
  const analysis::WaveformMapping mapping =
      sweeps_.empty() ? Mapping() : MappingAt(sweeps_.front());

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

  // Reported as an offset into the sweep rather than into the snapshot. The
  // snapshot's own index is an accident of where the transfer started and means
  // nothing to a reader; time into the sweep is what the axis below is marked
  // in and what a measurement is taken against.
  const size_t into = sample - std::min(sample, mapping.first_sample);

  emit CursorMoved(static_cast<qint64>(into), code);
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
  span_->setCurrentIndex(static_cast<int>(analysis::kDefaultWaveformSpanIndex));
  connect(span_, &QComboBox::currentIndexChanged, this, [this](int) {
    plot_->SetSampleSpan(
        static_cast<size_t>(span_->currentData().toULongLong()));
  });
  controls->addWidget(new QLabel(tr("Span"), this));
  controls->addWidget(span_);

  trigger_ = new QCheckBox(tr("Trigger"), this);
  trigger_->setObjectName(QLatin1String(kTriggerBoxName));
  trigger_->setChecked(true);
  trigger_->setToolTip(
      tr("Start every sweep at the same point on the waveform — a rising "
         "crossing of mid-scale. Snapshots arrive from the device at whatever "
         "point in the signal the transfer happened to begin, so without this "
         "a carrier is a different slice of a cycle every frame and reads as a "
         "band of fuzz. Turn it off to see the snapshot exactly as it "
         "arrived."));
  connect(trigger_, &QCheckBox::toggled, this,
          [this](bool on) { plot_->SetTriggered(on); });
  controls->addWidget(trigger_);

  controls->addWidget(new QLabel(tr("Persistence"), this));

  persistence_ = new QSlider(Qt::Horizontal, this);
  persistence_->setObjectName(QLatin1String(kPersistenceSliderName));
  persistence_->setRange(0, kPersistenceSliderSteps);
  persistence_->setValue(0);
  persistence_->setTickPosition(QSlider::TicksBelow);
  persistence_->setTickInterval(2);

  // Wide enough to aim at and no wider: this shares a row with four other
  // controls and a readout, and a slider given its head takes all the space
  // going.
  persistence_->setFixedWidth(110);

  persistence_->setToolTip(
      tr("How long each sweep lingers before fading, from off to two seconds. "
         "A fading trace is how a repeating waveform builds up its envelope — "
         "with the trigger on, every snapshot contributes up to thirty-two "
         "sweeps rather than one, so the deviation of an FM carrier shows as a "
         "widening of the trace rather than as one arbitrary slice of it. Off "
         "replaces the trace each time, which is the plain scope."));
  connect(persistence_, &QSlider::valueChanged, this,
          [this](int) { ApplyPersistence(); });
  controls->addWidget(persistence_);

  persistence_label_ = new QLabel(this);
  persistence_label_->setObjectName(QLatin1String(kPersistenceLabelName));

  // Fixed width so that the row does not shuffle sideways as the figure grows
  // and shrinks under the slider the user is dragging.
  persistence_label_->setFixedWidth(34);
  controls->addWidget(persistence_label_);

  controls->addStretch();

  cursor_ = new QLabel(this);
  cursor_->setObjectName(QLatin1String(kCursorLabelName));
  cursor_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  controls->addWidget(cursor_);

  layout->addLayout(controls);

  connect(plot_, &WaveformPlot::CursorMoved, this, &WaveformPanel::ShowCursor);
  connect(plot_, &WaveformPlot::CursorLeft, this, &WaveformPanel::ClearCursor);

  // Puts the label in step with the slider before either has been touched.
  ApplyPersistence();
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

void WaveformPanel::ApplyPersistence() {
  const double seconds = persistence_->value() * kPersistenceSecondsPerStep;

  plot_->SetPersistenceSeconds(seconds);
  persistence_label_->setText(FormatPersistence(seconds));
}

void WaveformPanel::ShowCursor(qint64 sample_index, double code) {
  cursor_->setText(FormatWaveformCursor(sample_index, code, gain_));
}

void WaveformPanel::ClearCursor() {
  cursor_->setText(tr("Point at the trace to read a sample"));
}

}  // namespace ddd::gui
