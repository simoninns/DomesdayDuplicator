/************************************************************************

    amplitude_history.cpp

    Signal level over minutes rather than microseconds
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "amplitude_history.h"

#include <algorithm>

namespace ddd::analysis {
namespace {

const AmplitudePoint kEmptyPoint;

}  // namespace

AmplitudeHistory::AmplitudeHistory(size_t capacity)
    : points_(std::max<size_t>(capacity, 1)) {}

void AmplitudeHistory::Append(const AmplitudePoint& point) {
  const size_t capacity = points_.size();

  if (size_ < capacity) {
    points_[(first_ + size_) % capacity] = point;
    ++size_;
    return;
  }

  // Full: the oldest point is the one overwritten, and the window slides.
  points_[first_] = point;
  first_ = (first_ + 1) % capacity;
}

void AmplitudeHistory::Clear() {
  first_ = 0;
  size_ = 0;
}

const AmplitudePoint& AmplitudeHistory::At(size_t index) const {
  if (index >= size_) {
    return kEmptyPoint;
  }
  return points_[(first_ + index) % points_.size()];
}

AmplitudePoint AmplitudeHistory::Newest() const {
  if (size_ == 0) {
    return AmplitudePoint{};
  }
  return At(size_ - 1);
}

double AmplitudeHistory::SpanSeconds() const {
  if (size_ < 2) {
    return 0.0;
  }
  return At(size_ - 1).seconds - At(0).seconds;
}

uint16_t AmplitudeHistory::PeakCode() const {
  uint16_t peak = 0;
  for (size_t index = 0; index < size_; ++index) {
    peak = std::max(peak, At(index).maximum_code);
  }
  return peak;
}

uint16_t AmplitudeHistory::TroughCode() const {
  if (size_ == 0) {
    return 0;
  }

  uint16_t trough = UINT16_MAX;
  for (size_t index = 0; index < size_; ++index) {
    trough = std::min(trough, At(index).minimum_code);
  }
  return trough;
}

uint64_t AmplitudeHistory::TotalClipped() const {
  uint64_t total = 0;
  for (size_t index = 0; index < size_; ++index) {
    total += At(index).clipped_count;
  }
  return total;
}

AmplitudeSampler::AmplitudeSampler(double interval_seconds)
    : interval_seconds_(interval_seconds > 0.0 ? interval_seconds
                                               : kDefaultIntervalSeconds) {}

std::optional<AmplitudePoint> AmplitudeSampler::Observe(
    double elapsed_seconds, const capture::SampleMetricsSnapshot& metrics) {
  // Nothing has been measured yet: a run that has opened the device but not
  // processed a buffer would otherwise contribute a point of zeroes, drawn as
  // a signal that momentarily vanished.
  if (metrics.sample_count == 0) {
    return std::nullopt;
  }

  // Time going backwards means a new run: the elapsed figure is measured from
  // the moment the pipeline started, and that clock begins again at zero every
  // time. Treated as one continuous stream instead, the next run is measured
  // against a deadline left over from the last — so a twenty-second run is
  // followed by twenty seconds in which every point is suppressed and the panel
  // reads "Nothing recorded yet" over a perfectly healthy stream, then fills in
  // at once when the clock finally catches up.
  //
  // Noticed here rather than relied upon from outside. Reset() is called when a
  // run starts and remains the right thing to do, but a sampler that keys off
  // an absolute clock has to cope with that clock restarting: this is the one
  // signal that cannot be missed, and it costs a comparison per update.
  const bool restarted = started_ && elapsed_seconds < last_elapsed_seconds_;
  last_elapsed_seconds_ = elapsed_seconds;

  if (restarted) {
    // The interval in progress belongs to the run that has ended. Carrying it
    // over would put the previous disc's extremes into the first point of the
    // next one, and its clip total would make the first count a negative jump.
    rms_total_ = 0.0;
    observation_count_ = 0;
    have_extremes_ = false;
    last_clipped_total_ = 0;
    have_clipped_total_ = false;
    started_ = false;
  }

  if (!started_) {
    started_ = true;
    interval_end_seconds_ = elapsed_seconds + interval_seconds_;
  }

  rms_total_ += metrics.recent_rms;
  ++observation_count_;

  if (!have_extremes_) {
    have_extremes_ = true;
    minimum_code_ = metrics.recent_minimum_value;
    maximum_code_ = metrics.recent_maximum_value;
  } else {
    minimum_code_ = std::min(minimum_code_, metrics.recent_minimum_value);
    maximum_code_ = std::max(maximum_code_, metrics.recent_maximum_value);
  }

  const uint64_t clipped_total =
      metrics.clipped_low_count + metrics.clipped_high_count;

  if (elapsed_seconds < interval_end_seconds_) {
    return std::nullopt;
  }

  AmplitudePoint point;
  point.seconds = elapsed_seconds;
  point.rms_codes = observation_count_ > 0
                        ? rms_total_ / static_cast<double>(observation_count_)
                        : 0.0;
  point.minimum_code = minimum_code_;
  point.maximum_code = maximum_code_;
  point.clipped_count =
      have_clipped_total_ && clipped_total >= last_clipped_total_
          ? clipped_total - last_clipped_total_
          : 0;

  last_clipped_total_ = clipped_total;
  have_clipped_total_ = true;

  rms_total_ = 0.0;
  observation_count_ = 0;
  have_extremes_ = false;

  // Advanced by whole intervals rather than set from the current time, so a
  // late update does not push every subsequent point late with it. A run that
  // stalled long enough to miss several intervals catches up in one step
  // instead of drawing a burst of points to make up for them.
  interval_end_seconds_ += interval_seconds_;
  if (interval_end_seconds_ <= elapsed_seconds) {
    interval_end_seconds_ = elapsed_seconds + interval_seconds_;
  }

  return point;
}

void AmplitudeSampler::Reset() {
  started_ = false;
  interval_end_seconds_ = 0.0;
  last_elapsed_seconds_ = 0.0;
  rms_total_ = 0.0;
  observation_count_ = 0;
  minimum_code_ = 0;
  maximum_code_ = 0;
  have_extremes_ = false;
  last_clipped_total_ = 0;
  have_clipped_total_ = false;
}

}  // namespace ddd::analysis
