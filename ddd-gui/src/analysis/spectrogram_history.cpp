/************************************************************************

    spectrogram_history.cpp

    The spectrum over time, which is where a drift shows up
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "spectrogram_history.h"

#include <algorithm>

#include "spectrum_analyser.h"

namespace ddd::analysis {

SpectrogramHistory::SpectrogramHistory(size_t columns, size_t rows)
    : columns_(std::max<size_t>(columns, 1)),
      rows_(std::max<size_t>(rows, 1)),
      cells_(columns_ * rows_, SpectrumAnalyser::kFloorDecibels),
      seconds_(rows_, 0.0) {}

void SpectrogramHistory::Append(const std::vector<double>& magnitudes_db,
                                double seconds) {
  if (magnitudes_db.empty()) {
    return;
  }

  const size_t target = (size_ < rows_) ? ((first_ + size_) % rows_) : first_;
  double* const row = &cells_[target * columns_];
  seconds_[target] = seconds;

  for (size_t column = 0; column < columns_; ++column) {
    const size_t from = column * magnitudes_db.size() / columns_;
    const size_t to =
        std::min(magnitudes_db.size(),
                 ((column + 1) * magnitudes_db.size() / columns_) + 1);

    double peak = SpectrumAnalyser::kFloorDecibels;
    for (size_t bin = from; bin < to; ++bin) {
      peak = std::max(peak, magnitudes_db[bin]);
    }
    row[column] = peak;
  }

  if (size_ < rows_) {
    ++size_;
  } else {
    // Full: the row just written was the oldest, and the window slides past it.
    first_ = (first_ + 1) % rows_;
  }
}

void SpectrogramHistory::Clear() {
  first_ = 0;
  size_ = 0;
  std::fill(cells_.begin(), cells_.end(), SpectrumAnalyser::kFloorDecibels);
  std::fill(seconds_.begin(), seconds_.end(), 0.0);
}

double SpectrogramHistory::SecondsAt(size_t row) const {
  if (row >= size_) {
    return 0.0;
  }
  return seconds_[(first_ + row) % rows_];
}

double SpectrogramHistory::SpanSeconds() const {
  if (size_ < 2) {
    return 0.0;
  }
  return SecondsAt(size_ - 1) - SecondsAt(0);
}

double SpectrogramHistory::IntervalSeconds() const {
  if (size_ < 2) {
    return 0.0;
  }
  return SpanSeconds() / static_cast<double>(size_ - 1);
}

double SpectrogramHistory::WindowSeconds() const {
  return IntervalSeconds() * static_cast<double>(rows_);
}

double SpectrogramHistory::At(size_t row, size_t column) const {
  if (row >= size_ || column >= columns_) {
    return SpectrumAnalyser::kFloorDecibels;
  }
  return cells_[(((first_ + row) % rows_) * columns_) + column];
}

}  // namespace ddd::analysis
