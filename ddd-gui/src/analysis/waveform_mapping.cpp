/************************************************************************

    waveform_mapping.cpp

    Where a sample lands on the screen, and what a screen position means
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "waveform_mapping.h"

#include <algorithm>

namespace ddd::analysis {

double WaveformMapping::SampleToX(size_t sample_index) const {
  if (!Valid()) {
    return 0.0;
  }

  const double offset =
      static_cast<double>(sample_index) - static_cast<double>(first_sample);
  return offset * static_cast<double>(width_pixels) /
         static_cast<double>(sample_span);
}

size_t WaveformMapping::XToSample(double x) const {
  if (!Valid()) {
    return first_sample;
  }

  const double offset =
      x * static_cast<double>(sample_span) / static_cast<double>(width_pixels);
  if (offset <= 0.0) {
    return first_sample;
  }

  const size_t index = first_sample + static_cast<size_t>(offset);
  return std::min(index, first_sample + sample_span - 1);
}

double WaveformMapping::CodeToY(double code) const {
  if (!Valid()) {
    return 0.0;
  }

  const double proportion =
      (code - minimum_code) / (maximum_code - minimum_code);
  return (1.0 - proportion) * static_cast<double>(height_pixels);
}

double WaveformMapping::YToCode(double y) const {
  if (!Valid()) {
    return minimum_code;
  }

  const double proportion = 1.0 - (y / static_cast<double>(height_pixels));
  return minimum_code + proportion * (maximum_code - minimum_code);
}

double WaveformMapping::SampleToSeconds(size_t sample_index,
                                        uint32_t sample_rate_hz) const {
  if (sample_rate_hz == 0 || sample_index < first_sample) {
    return 0.0;
  }
  return static_cast<double>(sample_index - first_sample) /
         static_cast<double>(sample_rate_hz);
}

void DecimateToColumns(const uint16_t* codes, size_t code_count,
                       const WaveformMapping& mapping,
                       std::vector<WaveformColumn>& columns) {
  columns.assign(
      mapping.Valid() ? static_cast<size_t>(mapping.width_pixels) : size_t{0},
      WaveformColumn{});

  if (codes == nullptr || code_count == 0 || columns.empty()) {
    return;
  }

  const size_t last =
      std::min(mapping.first_sample + mapping.sample_span, code_count);

  for (size_t index = mapping.first_sample; index < last; ++index) {
    // Computed from the sample rather than the sample from the column, so a
    // span shorter than the plot is wide leaves the columns between samples
    // empty instead of repeating the nearest one.
    const double x = mapping.SampleToX(index);
    if (x < 0.0) {
      continue;
    }

    const size_t column = static_cast<size_t>(x);
    if (column >= columns.size()) {
      continue;
    }

    const uint16_t code = codes[index];
    WaveformColumn& target = columns[column];
    if (!target.populated) {
      target.populated = true;
      target.minimum = code;
      target.maximum = code;
      continue;
    }

    target.minimum = std::min(target.minimum, code);
    target.maximum = std::max(target.maximum, code);
  }
}

}  // namespace ddd::analysis
