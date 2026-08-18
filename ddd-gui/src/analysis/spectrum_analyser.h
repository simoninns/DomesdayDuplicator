/************************************************************************

    spectrum_analyser.h

    Windowed power spectrum, averaged and peak-held
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ddd::analysis {

// What the signal is made of, which is the question the scope cannot answer.
//
// A LaserDisc RF signal is an FM carrier around 8 MHz with sidebands, plus the
// audio carriers below it. On the scope that is a fuzzy band; here it is a peak
// with a shape, and a player that has drifted or an interfering source that has
// appeared is visible immediately.
//
// Scaled so that 0 dB is a full-scale sine wave. That reference is chosen
// because it is the one a user can act on: a carrier at -6 dB is using half the
// converter's range, and the number says so without anybody having to know how
// many codes wide the input was or how the window was normalised.
//
// Estimated by Welch's method: the snapshot is cut into half-overlapping
// segments, each is windowed and transformed, and their powers are averaged.
// One transform of one segment is a periodogram, and a periodogram is a
// famously noisy estimator — its scatter does not shrink however long the
// segment is, so a 32,768-point transform of the whole snapshot would give
// finer bins and a noise floor that boiled exactly as much as before. Averaging
// fifteen of them is what makes the floor sit still, and a floor that sits
// still is what lets a weak carrier be seen against it. The overlap is there
// because the window throws away the ends of every segment, and half-overlapped
// segments put each discarded end under the middle of its neighbour.

// Named at namespace scope rather than left as member initialisers alone,
// because a nested type's defaults cannot be reached from a default argument of
// the enclosing class without tripping over how the two are parsed.
inline constexpr size_t kDefaultTransformSize = 4096;
inline constexpr double kDefaultAveraging = 0.6;

// The transform sizes offered, and what choosing between them costs.
//
// A snapshot is 32,768 samples, so these are 15, 7 and 3 half-overlapped
// segments respectively. That is the trade the user is being handed: at 40 Msps
// the bins are 9.8, 4.9 and 2.4 kHz wide, and the finer the bin the fewer
// segments there are to average, so the steadier floor and the sharper
// resolution pull against each other. 4,096 is the default because it resolves
// the FM carrier and its sidebands comfortably while keeping the most
// averaging; the finer sizes are for looking at the audio carriers below 3 MHz,
// which sit close enough together to want them.
//
// Nothing larger is offered. 32,768 would be one segment — a periodogram, with
// no averaging at all — and anything above that could not be measured from a
// snapshot and would be refused.
inline constexpr size_t kTransformSizeChoices[] = {4096, 8192, 16384};

inline constexpr size_t kTransformSizeChoiceCount =
    sizeof(kTransformSizeChoices) / sizeof(kTransformSizeChoices[0]);

// Hann's equivalent noise bandwidth, in bins.
//
// A window spreads a tone across more than one bin, so a bin collects noise
// from wider than its own spacing. 1.5 is the figure for Hann, and it is what
// turns the bin spacing into the resolution bandwidth an analyser would state —
// the two differ by half again, which is the difference between "9.8 kHz" and
// the 14.6 kHz the instrument is actually resolving at.
inline constexpr double kHannNoiseBandwidthBins = 1.5;

// Where the board's anti-aliasing filter rolls off. Everything above this is
// the filter's skirt and the noise under it.
//
// There was once a control here offering tops of 14 to 20 MHz, because on a
// linear axis the stretch above this corner was dead space — a third of the
// display spent on the part of the spectrum the hardware has deliberately
// removed. A logarithmic axis makes the question go away: 13.2 to 20 MHz is a
// fifth of a decade, under a tenth of the width, so the display simply shows
// everything the converter can represent and there is nothing left to choose.
inline constexpr double kLowPassCornerHz = 13'200'000.0;

class SpectrumAnalyser {
 public:
  struct Options {
    // 4,096 points at 40 Msps is a 9.8 kHz bin, and 2,048 bins across the
    // 20 MHz the converter can represent — more than any panel has pixels for,
    // so the display decimates rather than interpolating.
    //
    // This is the length of one segment, not of the analysis: a snapshot holds
    // several of these and every one of them is transformed.
    size_t transform_size = kDefaultTransformSize;

    // Exponential averaging, 0 to just under 1: 0 shows each transform on its
    // own, higher values hold the display still enough to read. Averaging is
    // done on power rather than on decibels, so a peak that appears in one
    // frame out of ten reads as a tenth of its power and not as a tenth of its
    // level.
    double averaging = kDefaultAveraging;
  };

  // The floor everything is clamped to. A bin with no signal in it is
  // mathematically minus infinity, and a display asked to draw that has no
  // bottom.
  static constexpr double kFloorDecibels = -120.0;

  SpectrumAnalyser();
  explicit SpectrumAnalyser(const Options& options);

  // Feed converter codes. Every whole segment in them is measured, so a longer
  // buffer is a better estimate rather than a wasted one; returns false without
  // changing anything if there are fewer than one segment's worth, which is how
  // a short snapshot is refused rather than zero-padded into a spectrum that
  // was never measured.
  //
  // A trailing part-segment is dropped rather than padded, for the same reason.
  // At the default size that is under 2,048 samples of a 32,768-sample
  // snapshot, and every one of them was already measured by the segment before
  // it.
  bool Analyse(const uint16_t* codes, size_t count);

  // Levels in dB relative to a full-scale sine, one per bin from DC upwards.
  //
  // Averaged across snapshots as well as across segments, by the exponential
  // filter in Options::averaging. This is the trace's reading: held still
  // enough to read a weak carrier off.
  const std::vector<double>& magnitudes_db() const { return magnitudes_db_; }

  // The highest level each bin has reached since the peak hold was last reset.
  const std::vector<double>& peak_hold_db() const { return peak_hold_db_; }

  // This snapshot's own estimate, with no averaging across snapshots at all —
  // the segments of one 819 µs window and nothing else.
  //
  // What a spectrogram records. Its rows are moments, and a row that had been
  // smoothed against the rows before it would smear a transient across several
  // of them: at heavy averaging the filter's time constant is most of a second,
  // which is a third of a minute-wide waterfall. An interferer that appeared
  // once has to be one sharp row, whatever the trace beside it is set to.
  const std::vector<double>& snapshot_db() const { return snapshot_db_; }

  // transform_size / 2 + 1 — the bins a real input produces, DC to Nyquist.
  size_t bin_count() const { return magnitudes_db_.size(); }

  size_t transform_size() const { return options_.transform_size; }

  // Segments averaged by the most recent Analyse, and zero before the first
  // one. This is what a readout quotes to say how steady the estimate is, and
  // it depends on how long a buffer the caller handed over rather than on
  // anything set here.
  size_t segment_count() const { return segment_count_; }

  void ResetPeakHold();

  // Forget the average and the peak hold, for the start of a new run.
  void Reset();

  // The frequency a bin is centred on.
  static double BinFrequencyHz(size_t bin, size_t transform_size,
                               uint32_t sample_rate_hz);

  // The bin nearest a frequency, for a cursor readout.
  static size_t FrequencyToBin(double frequency_hz, size_t transform_size,
                               uint32_t sample_rate_hz);

  // The width of one bin. The same figure as the frequency of bin 1, named for
  // what a reader wants it for.
  static double BinSpacingHz(size_t transform_size, uint32_t sample_rate_hz);

  // The resolution bandwidth: how wide a band each bin is really collecting
  // from, once the window's spreading is counted. This is the figure an
  // analyser states, and it is half again the bin spacing for the Hann window
  // in use.
  static double NoiseBandwidthHz(size_t transform_size,
                                 uint32_t sample_rate_hz);

  // How many half-overlapped segments a buffer of `count` samples holds, which
  // is how many transforms Analyse would average over it. Zero when the buffer
  // is too short to hold one, which is the case Analyse refuses.
  static size_t SegmentsIn(size_t count, size_t transform_size);

 private:
  void BuildWindow();

  Options options_;

  // Hann, and its sum. The sum is the coherent gain the window costs, and
  // dividing by it is what makes a full-scale sine read 0 dB whichever window
  // is in use.
  std::vector<double> window_;
  double window_sum_ = 0.0;

  std::vector<double> real_;
  std::vector<double> imaginary_;

  // Power per bin accumulated across the segments of one snapshot, before it is
  // divided by their number and handed to the exponential average below. A
  // member rather than a local so that a display running at the snapshot rate
  // does not allocate a vector per frame.
  std::vector<double> segment_power_;
  size_t segment_count_ = 0;

  // Averaged power per bin, in linear units. Kept alongside the decibel figures
  // rather than derived from them for the reason in Options::averaging.
  std::vector<double> average_power_;
  bool have_average_ = false;

  std::vector<double> magnitudes_db_;
  std::vector<double> peak_hold_db_;
  std::vector<double> snapshot_db_;
};

}  // namespace ddd::analysis
