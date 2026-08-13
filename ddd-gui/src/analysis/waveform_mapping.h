/************************************************************************

    waveform_mapping.h

    Where a sample lands on the screen, and what a screen position means
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ddd::analysis {

// The scope's arithmetic, with no painting anywhere near it.
//
// Split out because it is the part that can be wrong in ways nobody notices: a
// trace drawn one pixel out still looks like a waveform, and a cursor readout
// that disagrees with the trace by half a division is only found by measuring
// something known. Kept here it can be asked directly whether the sample under
// the cursor is the sample being drawn there, which is the only question that
// matters and one no screenshot answers.

// The span of a plot, in samples and pixels.
struct WaveformMapping {
  // The plot area, excluding whatever axes and margins the panel reserves.
  int width_pixels = 0;
  int height_pixels = 0;

  // The window of the snapshot on display.
  size_t first_sample = 0;
  size_t sample_span = 0;

  // The vertical extent, in converter codes. Fixed at the full 10-bit range by
  // the panel, and a parameter here so a test can check a partial range without
  // the panel having to offer one.
  double minimum_code = 0.0;
  double maximum_code = 1023.0;

  bool Valid() const {
    return width_pixels > 0 && height_pixels > 0 && sample_span > 0 &&
           maximum_code > minimum_code;
  }

  // Sample index to horizontal position. The left edge of the plot is the first
  // sample and the right edge is one past the last, so a span of N samples
  // divides the width into N equal columns rather than N - 1.
  double SampleToX(size_t sample_index) const;

  // Horizontal position to sample index, clamped to the displayed window. The
  // inverse of SampleToX to within one column, which is as exact as a pixel
  // covering many samples can be.
  size_t XToSample(double x) const;

  // Converter code to vertical position. Y grows downward, as in every
  // coordinate system a painter uses, so the largest code is at the top.
  double CodeToY(double code) const;

  // Vertical position back to a converter code, for the cursor readout.
  double YToCode(double y) const;

  // Seconds from the start of the displayed window to a sample in it.
  double SampleToSeconds(size_t sample_index, uint32_t sample_rate_hz) const;
};

// One pixel column's worth of the trace.
//
// A column covers many samples at any useful span — 32,768 samples across a
// 600-pixel plot is 54 of them — so drawing one point per column would alias a
// 5 MHz carrier into whatever pattern the sampling happened to hit. Keeping the
// extremes and drawing a vertical line between them is what makes an envelope
// look like an envelope.
struct WaveformColumn {
  uint16_t minimum = 0;
  uint16_t maximum = 0;
  bool populated = false;
};

// Reduce a run of codes to one column per pixel.
//
// Columns with no samples are left unpopulated rather than interpolated: at a
// span narrower than the plot is wide there genuinely is nothing to draw
// between the samples, and inventing it would draw a signal that was never
// measured.
void DecimateToColumns(const uint16_t* codes, size_t code_count,
                       const WaveformMapping& mapping,
                       std::vector<WaveformColumn>& columns);

// The time spans the panel offers, as sample counts at the device's rate.
//
// 40 Msps, so these are 10, 50, 100, 200 and 500 microseconds. 10 µs is about
// two cycles of a LaserDisc FM carrier — the shortest span with anything to see
// in it — and the top of the range is bounded by honesty rather than by taste:
// a snapshot is 32,768 samples, so a span asking for more than that is silently
// clamped and shows less time than its own label claims. 500 µs is 20,000
// samples, which leaves the longest span still showing all of what it says.
inline constexpr size_t kWaveformSpanChoices[] = {400, 2'000, 4'000, 8'000,
                                                  20'000};

inline constexpr size_t kWaveformSpanChoiceCount =
    sizeof(kWaveformSpanChoices) / sizeof(kWaveformSpanChoices[0]);

}  // namespace ddd::analysis
