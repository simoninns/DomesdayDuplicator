/************************************************************************

    sample_metrics.cpp

    What the signal looked like, accumulated as it goes past
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "sample_metrics.h"

#include <algorithm>
#include <cmath>

namespace ddd::capture {
namespace {

double RootMeanSquare(uint64_t sum_of_squares, uint64_t sample_count) {
  if (sample_count == 0) {
    return 0.0;
  }
  return std::sqrt(static_cast<double>(sum_of_squares) /
                   static_cast<double>(sample_count));
}

}  // namespace

void SampleMetrics::Accumulate(const BufferTally& tally) {
  if (tally.sample_count == 0) {
    return;
  }

  sample_count_ += tally.sample_count;
  minimum_value_ = std::min(minimum_value_, tally.minimum_value);
  maximum_value_ = std::max(maximum_value_, tally.maximum_value);
  clipped_low_count_ += tally.clipped_low_count;
  clipped_high_count_ += tally.clipped_high_count;
  sum_of_squares_ += tally.sum_of_squares;

  recent_ = tally;
}

SampleMetricsSnapshot SampleMetrics::Snapshot() const {
  SampleMetricsSnapshot snapshot;
  snapshot.sample_count = sample_count_;

  // Before any samples have arrived the minimum is still its seed, and
  // reporting 65535 as the quietest sample seen would be nonsense. Zero for
  // both is the honest "nothing measured yet".
  snapshot.minimum_value = (sample_count_ == 0) ? 0 : minimum_value_;
  snapshot.maximum_value = maximum_value_;
  snapshot.clipped_low_count = clipped_low_count_;
  snapshot.clipped_high_count = clipped_high_count_;
  snapshot.rms = RootMeanSquare(sum_of_squares_, sample_count_);

  snapshot.recent_minimum_value =
      (recent_.sample_count == 0) ? 0 : recent_.minimum_value;
  snapshot.recent_maximum_value = recent_.maximum_value;
  snapshot.recent_clipped_low_count = recent_.clipped_low_count;
  snapshot.recent_clipped_high_count = recent_.clipped_high_count;
  snapshot.recent_rms =
      RootMeanSquare(recent_.sum_of_squares, recent_.sample_count);

  return snapshot;
}

void SampleMetrics::Reset() {
  sample_count_ = 0;
  minimum_value_ = UINT16_MAX;
  maximum_value_ = 0;
  clipped_low_count_ = 0;
  clipped_high_count_ = 0;
  sum_of_squares_ = 0;
  recent_ = BufferTally{};
}

}  // namespace ddd::capture
