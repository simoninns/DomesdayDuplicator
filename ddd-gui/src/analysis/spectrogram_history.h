/************************************************************************

    spectrogram_history.h

    The spectrum over time, which is where a drift shows up
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <vector>

namespace ddd::analysis {

// The spectrum answers "what is there now"; this answers "what has been there".
//
// A carrier that wanders, an interfering source that comes and goes, a player
// whose output changes as it warms up — none of those are visible in a live
// trace, because the trace only ever shows the moment it was taken. Stacked as
// rows with time down the display they are obvious, which is the entire reason
// this exists alongside the spectrum rather than instead of it.
//
// Held as levels rather than as a picture. A rendered image would have the
// colour scheme baked into it, so a theme change would leave every row already
// on screen in the old palette, and a change to the displayed frequency range
// would have nothing to re-draw from. Keeping the numbers means the display is
// a function of the history rather than a copy of it.
class SpectrogramHistory {
 public:
  // Columns span DC to Nyquist, always — not the range currently displayed.
  // The panel shows a slice of them, so narrowing the display re-draws existing
  // history at higher resolution instead of discarding it.
  //
  // 1,024 columns is half the analyser's bins, which is already more than any
  // panel has pixels for, and 300 rows is a little over half a minute at the
  // rate snapshots arrive — long enough to see a drift, short enough that the
  // whole thing is a couple of megabytes.
  static constexpr size_t kDefaultColumns = 1024;
  static constexpr size_t kDefaultRows = 300;

  explicit SpectrogramHistory(size_t columns = kDefaultColumns,
                              size_t rows = kDefaultRows);

  // Add one spectrum, taken at `seconds` since the run started. Bins are
  // reduced to columns by taking the highest level in each, for the same reason
  // the spectrum trace does: there are more bins than columns, and a narrow
  // carrier that fell between two sampled bins would simply not be recorded.
  //
  // The timestamp is supplied rather than taken here because this is Qt-free
  // and clock-free by rule, and because a caller replaying recorded frames
  // wants their own times rather than the times it replayed them at.
  void Append(const std::vector<double>& magnitudes_db, double seconds);

  void Clear();

  size_t columns() const { return columns_; }
  size_t rows() const { return rows_; }

  // Rows held so far, which is less than rows() until the history has filled.
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  // When each row was taken, on the same clock the caller supplied.
  double SecondsAt(size_t row) const;

  // Seconds between the oldest and newest rows held. Zero until there are two
  // of them, because one row establishes no interval.
  double SpanSeconds() const;

  // The average interval between rows, which is the rate frames arrived at.
  // Zero until it is known.
  double IntervalSeconds() const;

  // How much time a full ring covers, at the rate frames have been arriving.
  // This is the width of the display's window — the part not yet filled
  // included — and it is zero until the interval is known.
  double WindowSeconds() const;

  // Row 0 is the oldest still held. Out-of-range asks give the floor rather
  // than reading past the end, so a display racing an append draws a blank cell
  // rather than crashing.
  double At(size_t row, size_t column) const;

 private:
  size_t columns_;
  size_t rows_;

  // Row-major, oldest-first once size_ reaches rows_ and first_ starts moving.
  std::vector<double> cells_;
  std::vector<double> seconds_;
  size_t first_ = 0;
  size_t size_ = 0;
};

}  // namespace ddd::analysis
