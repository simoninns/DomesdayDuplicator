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

// How a trace should be drawn, which depends entirely on how many samples are
// crowded into a pixel.
//
// One rule cannot serve the whole range. At 500 µs there are thirty-three
// samples to a column and the only honest thing to draw is what they span; at
// 1 µs there are fifteen columns to a sample and the only honest thing is the
// waveform they determine. Getting this wrong in either direction produces a
// picture that looks plausible and is not the signal.
enum class WaveformDrawStyle {
  // More than a couple of samples per column: the extremes each column covers,
  // drawn as a vertical bar. Anything else aliases.
  kEnvelope,

  // About one sample per column: the sample points, joined. There is nothing to
  // reconstruct between points a pixel apart and nothing to summarise either.
  kPolyline,

  // Fewer samples than columns: the band-limited waveform the samples
  // determine, evaluated at each pixel. See sinc_interpolation.h.
  kReconstructed,
};

// Where the styles change over. Two samples a column is where an envelope stops
// having anything to summarise; one is where straight lines start visibly
// cutting the corners off a carrier.
inline constexpr double kEnvelopeSamplesPerPixel = 2.0;
inline constexpr double kPolylineSamplesPerPixel = 1.0;

// And how far apart the samples have to be before they are worth marking
// individually. Eight pixels: close enough to see they are a series, far enough
// that the dots do not merge into the line through them.
inline constexpr double kSampleMarkerPixelSpacing = 8.0;

// The span of a plot, in samples and pixels.
struct WaveformMapping {
  // The plot area, excluding whatever axes and margins the panel reserves.
  int width_pixels = 0;
  int height_pixels = 0;

  // The window of the snapshot on display.
  size_t first_sample = 0;
  size_t sample_span = 0;

  // How far past first_sample the window really begins, as a fraction of one
  // sample.
  //
  // A trigger lands between two samples, and at five samples to a cycle the
  // difference between rounding that to a sample and honouring it is a fifth of
  // a cycle of jitter — most of the shimmer the trigger exists to remove. Zero
  // for an untriggered sweep, where the window starts on a sample by
  // definition.
  double sub_sample_offset = 0.0;

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
  double SampleToX(double sample_index) const;

  // Horizontal position to sample index, clamped to the displayed window. The
  // inverse of SampleToX to within one column, which is as exact as a pixel
  // covering many samples can be.
  size_t XToSample(double x) const;

  // Horizontal position to a fractional sample position, which is what a
  // reconstructed trace needs: at a span narrower than the plot is wide, every
  // pixel falls between samples and rounding to one of them is the aliasing
  // this exists to avoid.
  double XToSamplePosition(double x) const;

  // Converter code to vertical position. Y grows downward, as in every
  // coordinate system a painter uses, so the largest code is at the top.
  double CodeToY(double code) const;

  // Vertical position back to a converter code, for the cursor readout.
  double YToCode(double y) const;

  // Seconds from the start of the displayed window to a sample in it.
  double SampleToSeconds(size_t sample_index, uint32_t sample_rate_hz) const;

  // How many samples one pixel column covers. The figure the drawing style
  // below is chosen from.
  double SamplesPerPixel() const;

  // How the trace should be drawn at this span, and whether the samples
  // themselves are far enough apart to be worth marking.
  WaveformDrawStyle DrawStyle() const;
  bool ShouldMarkSamples() const;
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
// 40 Msps, so these are 0.5, 1, 2, 5, 10, 50, 100, 200 and 500 microseconds.
//
// The short end of this ladder is the point of it. An 8 MHz carrier has a
// period of 125 ns, so a single cycle is five samples and 1 µs is about eight
// cycles — the classic few-cycles-on-screen a scope is set to, and the only
// range at which the shape of the carrier can be seen at all. The ladder used
// to start at 10 µs, which is eighty cycles: an unresolvable band of fuzz, and
// the comment here claimed it was two.
//
// The top of the range is bounded by honesty rather than by taste: a snapshot
// is 32,768 samples, so a span asking for more than that is silently clamped
// and shows less time than its own label claims. 500 µs is 20,000 samples,
// which leaves the longest span still showing all of what it says.
inline constexpr size_t kWaveformSpanChoices[] = {
    20, 40, 80, 200, 400, 2'000, 4'000, 8'000, 20'000};

inline constexpr size_t kWaveformSpanChoiceCount =
    sizeof(kWaveformSpanChoices) / sizeof(kWaveformSpanChoices[0]);

// 1 µs, about eight cycles of the carrier this instrument exists to capture.
inline constexpr size_t kDefaultWaveformSpanIndex = 1;

// Where the trigger point sits across the plot.
//
// A tenth of the way in rather than at the left edge, so that what happened
// just before the edge that started the sweep is on screen too. Every scope
// offers this and most default to somewhere near it.
inline constexpr double kPreTriggerFraction = 0.1;

}  // namespace ddd::analysis
