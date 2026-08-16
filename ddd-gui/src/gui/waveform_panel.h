/************************************************************************

    waveform_panel.h

    The scope: what the signal is doing right now
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
#include "front_end_gain.h"
#include "sinc_interpolation.h"
#include "waveform_mapping.h"
#include "waveform_trigger.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;

namespace ddd::gui {

class CaptureController;

// The persistence control's range: a quarter of a second per step, up to two.
//
// Quarter-second steps because the useful settings are all at the short end —
// the difference between a quarter of a second and a half is plainly visible,
// and one between the longest settings would not be.
//
// Two seconds is the top because past it the display stops being live. With the
// trigger on, a two-second tail has already accumulated something like five
// hundred sweeps; a longer one adds nothing to the picture and costs the thing
// a live display is for, which is that a change in the signal is apparent
// quickly. A trace lingering longer than this is showing what the signal was
// doing rather than what it is doing.
inline constexpr int kPersistenceSliderSteps = 8;
inline constexpr double kPersistenceSecondsPerStep = 0.25;

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

  // How long a sweep lingers before it has faded to nothing worth seeing, in
  // seconds. Zero replaces the trace each frame, which is the plain scope.
  //
  // Set as a duration rather than as a per-frame fade because a per-frame
  // figure means different things at different snapshot rates, and because the
  // relation between the two is exponential: the alphas that give a useful tail
  // are all crowded into the top fifth of the range, and a slider over them
  // would do nothing for four fifths of its travel and everything in the last.
  void SetPersistenceSeconds(double seconds);

  // Start each sweep at the same point on the waveform. Without it the trace is
  // drawn from wherever the USB transfer happened to begin, which at these
  // spans is a different part of a cycle every frame.
  void SetTriggered(bool enabled);

  void Clear();

  size_t sample_span() const { return sample_span_; }
  double persistence_seconds() const { return persistence_seconds_; }
  bool persistence() const { return persistence_seconds_ > 0.0; }
  bool triggered() const { return triggered_; }

  // How much of the accumulated picture survives one fade, as an alpha the old
  // picture is multiplied by, given the tail length asked for and the time that
  // has actually passed since the last one.
  //
  // Exponential decay, so `seconds` is the time constant: after it the picture
  // is at 37% and after three times it at 5%, which is faded to nothing anybody
  // would call a trace. That relation is why the control is calibrated in
  // seconds and not in this figure — the alphas worth having are 0.85 to 0.99,
  // and a slider over them would spend most of its travel doing nothing.
  //
  // Public because it is the part of the persistence that can be wrong without
  // looking wrong. A tail that is half the length it says is still a plausible
  // picture, and the only way to catch it is to ask the arithmetic directly:
  // paints in a test arrive microseconds apart, where every setting decays by
  // nothing and every setting therefore looks alike.
  static int RetainedAlpha(double seconds, double elapsed_seconds);

  // Sweeps found in the current snapshot: one when untriggered or when nothing
  // crossed the level, and as many as the snapshot yielded when triggered.
  // Persistence is what puts all of them on screen at once, which is how the
  // accumulated picture comes to show the carrier's deviation rather than one
  // instant of it. Zero until the first paint.
  size_t sweep_count() const { return sweeps_.size(); }

  // Whether the trigger is on and nothing crossed it, so the sweeps on screen
  // start wherever the transfer did.
  //
  // Worth asking, and worth saying on the display, because the two states look
  // identical for exactly the signal that produces one: a flat input drawn
  // free-running is a flat line, and a flat input drawn triggered would be the
  // same flat line. Without this the panel shows a trigger marker and a ticked
  // box over a trace that is not triggered at all, which is a display claiming
  // a property of the picture that the picture has not got.
  bool free_running() const { return triggered_ && !armed_; }

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
  // The plot geometry for a sweep starting at a given fractional position.
  analysis::WaveformMapping MappingAt(double origin) const;

  // The geometry of the plot area alone, with the window left at the start.
  analysis::WaveformMapping Mapping() const;

  // Where each sweep of the current snapshot begins. One entry at zero when
  // untriggered or when nothing crossed the trigger level.
  void FindSweeps();

  void PaintSweep(QPainter& painter, double origin, const QColor& colour,
                  bool mark_samples);
  void PaintTrace(QPainter& painter, const QColor& colour);

  std::vector<uint16_t> codes_;
  std::vector<analysis::WaveformColumn> columns_;
  std::vector<double> triggers_;
  std::vector<double> sweeps_;
  bool sweeps_valid_ = false;

  // Whether the sweeps above came from crossings of the trigger level rather
  // than from the start of the snapshot.
  bool armed_ = false;

  // Reused across sweeps rather than rebuilt per sweep, which at thirty sweeps
  // a frame is thirty allocations that need not happen.
  QPolygonF points_;

  size_t sample_span_ =
      analysis::kWaveformSpanChoices[analysis::kDefaultWaveformSpanIndex];
  double persistence_seconds_ = 0.0;
  bool triggered_ = true;

  // Built once. The table is 32 KB and its construction is a few thousand
  // transcendental calls, neither of which belongs in a paint.
  analysis::ReconstructionKernel kernel_;

  // The accumulated picture, kept only while persistence is on. Every frame
  // fades what is there and draws over it, which is how a repeating waveform
  // builds up a bright envelope and a transient shows as a faint one — the
  // same thing an analogue scope's phosphor did, and the reason it is worth
  // having at all.
  QImage persistence_image_;

  // Whether a snapshot has arrived since the picture was last faded.
  //
  // The fade belongs to the arrival of data, not to the arrival of a paint
  // event. Faded on every paint instead, a resize or an expose would wash the
  // accumulated picture out at whatever rate the window manager felt like
  // sending events.
  bool persistence_pending_ = false;

  // Time since the picture was last faded, which is what turns the tail length
  // the user asked for into a fade for this particular frame.
  //
  // Measured rather than assumed, for the same reason the spectrogram measures
  // its own frame rate: nothing tells this panel how often the pipeline
  // publishes, the answer depends on the ring geometry the run was started
  // with, and a tail calibrated against the wrong rate is the wrong length.
  // Measuring also makes a stall behave properly — a picture nobody has added
  // to for two seconds has genuinely faded by two seconds' worth.
  QElapsedTimer fade_clock_;
};

// The scope panel: the plot, the span control, and the cursor readout.
class WaveformPanel : public QWidget {
  Q_OBJECT

 public:
  explicit WaveformPanel(CaptureController* controller,
                         QWidget* parent = nullptr);

  static constexpr const char* kPlotName = "waveform_plot";
  static constexpr const char* kSpanComboName = "waveform_span_combo";
  static constexpr const char* kPersistenceSliderName =
      "waveform_persistence_slider";
  static constexpr const char* kPersistenceLabelName =
      "waveform_persistence_label";
  static constexpr const char* kTriggerBoxName = "waveform_trigger_box";
  static constexpr const char* kCursorLabelName = "waveform_cursor_label";

 public slots:
  void OnWaveformReady(const std::vector<uint16_t>& codes);
  void OnMonitoringChanged(bool monitoring);
  void SetFrontEndGain(analysis::FrontEndGain gain);

 private:
  void ApplyPersistence();
  void ShowCursor(qint64 sample_index, double code);
  void ClearCursor();

  WaveformPlot* plot_ = nullptr;
  QComboBox* span_ = nullptr;
  QCheckBox* trigger_ = nullptr;
  QSlider* persistence_ = nullptr;
  QLabel* persistence_label_ = nullptr;
  QLabel* cursor_ = nullptr;

  analysis::FrontEndGain gain_;
};

// A span in samples, put to a user as the time it covers.
QString FormatWaveformSpan(size_t samples);

// The persistence setting as the slider's label reads it: "off" at zero, and
// the tail length in seconds above that.
QString FormatPersistence(double seconds);

// The cursor readout: where in the snapshot, and what the sample was — in
// converter codes always, and in millivolts as well when the gain has been
// declared.
QString FormatWaveformCursor(qint64 sample_index, double code,
                             const analysis::FrontEndGain& gain);

}  // namespace ddd::gui
