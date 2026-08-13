/************************************************************************

    amplitude_history.h

    Signal level over minutes rather than microseconds
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "sample_metrics.h"

namespace ddd::analysis {

// The long view of the signal, which is the one that catches a fault.
//
// The waveform panel shows a millisecond and the statistics show an instant.
// Neither would show a player whose RF output sagged for two seconds forty
// minutes into a side, and that is exactly the fault this exists to make
// visible. So it keeps minutes of level history, and it keeps the envelope
// rather than the average alone: a signal whose peaks are clipping while its
// RMS looks healthy is a signal about to be captured badly.
//
// Everything here is in converter codes. Nothing is stored in millivolts even
// when the front-end gain has been declared, because the declaration can be
// corrected — and when it is, a history stored in codes re-labels itself while
// a history stored in volts would be permanently wrong.

// One interval's worth of level.
struct AmplitudePoint {
  // Seconds since the run started, at the end of the interval.
  double seconds = 0.0;

  // RMS about mid-scale, in codes, averaged over the interval.
  double rms_codes = 0.0;

  // The extremes reached anywhere in the interval.
  uint16_t minimum_code = 0;
  uint16_t maximum_code = 0;

  // Samples clipped during this interval alone, not since the run started. A
  // running total draws a staircase that never comes down; a per-interval count
  // draws a tick where the clipping happened, which is what a user is looking
  // for.
  uint64_t clipped_count = 0;
};

// A fixed-length ring of points, oldest first.
//
// Fixed length rather than growing: this is fed for as long as a capture runs,
// and a capture runs for hours. A vector that grew at ten points a second would
// be 360,000 points into an eight-hour side, all but the last few hundred of
// them off the left-hand edge of a display that cannot show them.
class AmplitudeHistory {
 public:
  // Ten points a second for five minutes. Five minutes is long enough to show
  // a whole disc side's worth of trend at a glance when the panel is zoomed
  // out, and short enough that the ring is 3,000 small structs.
  static constexpr size_t kDefaultCapacity = 3'000;

  explicit AmplitudeHistory(size_t capacity = kDefaultCapacity);

  void Append(const AmplitudePoint& point);

  void Clear();

  size_t size() const { return size_; }
  size_t capacity() const { return points_.size(); }
  bool empty() const { return size_ == 0; }

  // Index 0 is the oldest point still held.
  const AmplitudePoint& At(size_t index) const;

  // The most recent point, or a default-constructed one when empty.
  AmplitudePoint Newest() const;

  // Seconds between the oldest and newest points held.
  double SpanSeconds() const;

  // Extremes across everything still held, which is not the same as the
  // capture's extremes: these fall off the back as the ring wraps, and that is
  // the point — they answer "how is it doing now" rather than "what is the
  // worst it has ever been".
  uint16_t PeakCode() const;
  uint16_t TroughCode() const;
  uint64_t TotalClipped() const;

 private:
  std::vector<AmplitudePoint> points_;
  size_t first_ = 0;
  size_t size_ = 0;
};

// Turns the stream of statistics updates into history points.
//
// The statistics arrive at 20 Hz and each carries the most recent buffer's
// figures, where a buffer is about 26 ms. Neither rate is the one the history
// wants, so this aggregates: extremes are taken across every update in the
// interval, the RMS is their mean, and the clip count is the difference in the
// capture's running totals across it. That last one is why the sampler holds
// state rather than being a function — a per-interval count cannot be computed
// from a single cumulative reading.
class AmplitudeSampler {
 public:
  // Ten points a second. Fast enough to see a dropout, slow enough that five
  // minutes of it fits in the ring above.
  static constexpr double kDefaultIntervalSeconds = 0.1;

  explicit AmplitudeSampler(double interval_seconds = kDefaultIntervalSeconds);

  // Feed one statistics update. Returns a point when an interval has closed,
  // and nothing on every other call.
  std::optional<AmplitudePoint> Observe(
      double elapsed_seconds, const capture::SampleMetricsSnapshot& metrics);

  // Forget everything, for the start of a new run.
  void Reset();

 private:
  double interval_seconds_;

  bool started_ = false;
  double interval_end_seconds_ = 0.0;

  // Accumulated across the current interval
  double rms_total_ = 0.0;
  size_t observation_count_ = 0;
  uint16_t minimum_code_ = 0;
  uint16_t maximum_code_ = 0;
  bool have_extremes_ = false;

  uint64_t last_clipped_total_ = 0;
  bool have_clipped_total_ = false;
};

}  // namespace ddd::analysis
