/************************************************************************

    sinc_interpolation.h

    What the signal did between the samples
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ddd::analysis {

// Why joining the sample points with straight lines is wrong here.
//
// A LaserDisc FM carrier at 8 MHz sampled at 40 Msps is five samples a cycle.
// Drawn as a polyline that is a jagged pentagon whose corners sit wherever the
// sampling happened to land, and whose peaks are up to 20% low — the samples
// mostly miss the actual crest. A viewer reading amplitude off it reads it
// wrong, and a viewer judging the shape sees an artefact of the sampling grid.
//
// The samples do not merely suggest the waveform, they determine it: the signal
// is band-limited by the board's filter at 13.2 MHz, well under the 20 MHz
// Nyquist limit, so exactly one band-limited waveform passes through them. This
// reconstructs it, which is what every digital oscilloscope does below about
// ten samples a cycle and for the same reason.
//
// Windowed rather than a pure sinc because a pure one runs to infinity and has
// to be cut off somewhere; cutting it off abruptly rings. Blackman over sixteen
// taps is the usual compromise and is far more accuracy than a display needs.

inline constexpr int kDefaultReconstructionHalfTaps = 8;

// The number of sub-sample positions the weights are precomputed at.
//
// A table rather than evaluating the window and the sinc per point, because the
// display asks for this once per pixel per sweep — thirty sweeps across six
// hundred pixels, nine times a second — and three transcendental calls per tap
// at that rate is real time on the thread that has to stay responsive.
// Quantising the position to a 256th of a sample is a timing error of 0.1 ns at
// 40 Msps, which is four orders of magnitude below one pixel.
inline constexpr size_t kReconstructionPhases = 256;

// A table of windowed-sinc weights, one row per sub-sample position.
//
// Built once and used for every point of every sweep. Cheap to hold — sixteen
// taps at 256 phases is 32 KB — and it turns each reconstructed point into
// sixteen multiply-adds.
class ReconstructionKernel {
 public:
  explicit ReconstructionKernel(int half_taps = kDefaultReconstructionHalfTaps);

  // The band-limited signal's value at a fractional sample position, in
  // converter codes. Positions outside the buffer are clamped to its ends
  // rather than extrapolated, and taps that would reach past either end take
  // the end sample — a display asked to draw the very edge of a snapshot should
  // show something flat rather than something invented.
  double Evaluate(const uint16_t* codes, size_t count, double position) const;

  int half_taps() const { return half_taps_; }
  int taps() const { return half_taps_ * 2; }

 private:
  int half_taps_;

  // kReconstructionPhases rows of taps() weights, each row summing to one.
  //
  // Normalised so that a constant input reconstructs to exactly that constant.
  // Un-normalised the weights sum to a hair under one and vary with the
  // fraction, which would draw a flat signal as a faint ripple at the pixel
  // pitch — an artefact that looks like noise on the signal and is not.
  std::vector<double> weights_;
};

}  // namespace ddd::analysis
