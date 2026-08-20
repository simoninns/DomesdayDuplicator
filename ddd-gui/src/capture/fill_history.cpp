/************************************************************************

    fill_history.cpp

    How full a buffer got, accumulated a reading at a time
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "fill_history.h"

#include <algorithm>

#include "log_format.h"

namespace ddd::capture {

void FillHistory::AddPercent(int percent) {
  percent = std::clamp(percent, 0, 100);

  ++readings_;
  percent_sum_ += static_cast<uint64_t>(percent);
  peak_percent_ = std::max(peak_percent_, percent);

  if (percent >= kQuarter) {
    ++at_or_above_quarter_;
  }
  if (percent >= kHalf) {
    ++at_or_above_half_;
  }
  if (percent >= kThreeQuarters) {
    ++at_or_above_three_quarters_;
  }
}

void FillHistory::Add(uint64_t level, uint64_t capacity) {
  if (capacity == 0) {
    return;
  }

  // Rounded to nearest rather than truncated. One slot of a six-slot ring is
  // 16.7%, and a truncating conversion would report a ring that spent a whole
  // run one slot behind as having averaged 16% while a nearest one says 17% —
  // the difference is nothing, but the rounding is the same rounding the peak
  // gets, and two figures on one line should be arrived at the same way.
  const uint64_t scaled = (level * 200 + capacity) / (capacity * 2);
  AddPercent(static_cast<int>(std::min<uint64_t>(scaled, 100)));
}

void FillHistory::Reset() { *this = FillHistory(); }

double FillHistory::mean_percent() const {
  if (readings_ == 0) {
    return 0.0;
  }
  return static_cast<double>(percent_sum_) / static_cast<double>(readings_);
}

std::string FillHistory::Describe() const {
  if (readings_ == 0) {
    return "no readings";
  }

  std::string text = "mean " + FormatDecimal(mean_percent(), 1) + "%, peak " +
                     std::to_string(peak_percent_) + "% (" +
                     std::to_string(readings_) + " readings)";

  if (at_or_above_quarter_ == 0) {
    return text + ", never over a quarter full";
  }

  text += ", over a quarter for " + std::to_string(at_or_above_quarter_);

  if (at_or_above_half_ == 0) {
    return text + ", never over half";
  }

  text += ", over half for " + std::to_string(at_or_above_half_);

  if (at_or_above_three_quarters_ == 0) {
    return text + ", never over three quarters";
  }

  return text + ", over three quarters for " +
         std::to_string(at_or_above_three_quarters_);
}

}  // namespace ddd::capture
