/************************************************************************

    fill_history.h

    How full a buffer got, accumulated a reading at a time
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <string>

namespace ddd::capture {

// A peak alone does not say whether a capture was in trouble. A run that
// touched three quarters of the ring once, in the second the encoder opened a
// file, and a run that sat there for twenty minutes report the same peak and
// are not the same capture at all. This keeps enough beside the peak to tell
// them apart: how full it was on average, and how many readings were at or
// above each of three watch levels.
//
// Deliberately not a histogram. Three levels answer the question a developer
// reading a log actually asks — was it ever busy, was it ever close, did it
// nearly fail — and a distribution would be more numbers on a line nobody
// reads to the end.
//
// The unit is a percentage throughout, so the same class covers the host's ring
// of buffers and the device's own FIFO, which are measured in slots and in
// words and are only comparable as fractions of themselves.
//
// Thread-safety: none. Each instance belongs to one thread — in the pipeline,
// the processing thread that takes the readings — and is read after that thread
// has been joined.
class FillHistory {
 public:
  // The three levels, as percentages. A quarter is "busy", a half is "closer
  // than it should be" and three quarters is "nearly out of room".
  static constexpr int kQuarter = 25;
  static constexpr int kHalf = 50;
  static constexpr int kThreeQuarters = 75;

  // Values outside 0..100 are clamped rather than refused: a reading is a
  // measurement of hardware and a percentage that comes back as 101 is worth
  // recording as full, not worth losing.
  void AddPercent(int percent);

  // The same, worked out from a level against a capacity. A capacity of zero is
  // ignored — there is nothing a fraction of it could mean.
  void Add(uint64_t level, uint64_t capacity);

  void Reset();

  uint64_t readings() const { return readings_; }
  int peak_percent() const { return peak_percent_; }

  // Zero when nothing has been added, which reads correctly: a buffer nobody
  // measured was never full.
  double mean_percent() const;

  // Readings at or above each level.
  uint64_t readings_at_or_above_quarter() const { return at_or_above_quarter_; }
  uint64_t readings_at_or_above_half() const { return at_or_above_half_; }
  uint64_t readings_at_or_above_three_quarters() const {
    return at_or_above_three_quarters_;
  }

  // One line for a log, e.g.
  //
  //   mean 2.4%, peak 41% (4096 readings), over a quarter for 118, over half
  //   for 4, never over three quarters
  //
  // A run that never rose above a quarter says so in three words rather than
  // listing three zeroes.
  std::string Describe() const;

 private:
  uint64_t readings_ = 0;
  uint64_t percent_sum_ = 0;
  int peak_percent_ = 0;

  uint64_t at_or_above_quarter_ = 0;
  uint64_t at_or_above_half_ = 0;
  uint64_t at_or_above_three_quarters_ = 0;
};

}  // namespace ddd::capture
