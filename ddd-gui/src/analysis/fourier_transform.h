/************************************************************************

    fourier_transform.h

    A radix-2 FFT, written here rather than vendored
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <vector>

namespace ddd::analysis {

// Why this is not KissFFT.
//
// The plan proposed vendoring an FFT and required the licence position to be
// confirmed before doing so (AGENTS.md §10). Confirming it was not the problem
// — KissFFT and pocketfft are both BSD-3-Clause and both fine in a GPLv3
// application. What settled it was the cost on the other side: this build runs
// clang-format and clang-tidy over every file under src/, as errors, and
// vendored code fails both immediately. Carrying it would mean building an
// exemption mechanism into the quality gates and then maintaining it, for a
// transform whose entire cost here is invisible.
//
// The size of that cost is worth stating, because it is what makes writing it
// out reasonable. One 4,096-point transform is about 50,000 butterflies. The
// spectrum panel asks for at most thirty a second, against a machine already
// moving 80 MB/s off a USB device — three or four milliseconds a second of one
// core, on the analysis thread, where nothing waits for it. A faster
// implementation would save nothing anybody could measure.
//
// What is here is the textbook Cooley-Tukey decimation-in-time: bit-reversal
// permutation, then log2(N) stages of butterflies. It is checked in the tests
// against a directly evaluated DFT, which is the comparison that matters —
// slow, obviously correct, and completely independent of the fast one.

bool IsPowerOfTwo(size_t value);

// In-place forward transform. real and imaginary must be the same length and
// that length must be a power of two; returns false and changes nothing
// otherwise.
//
// No scaling is applied. The caller knows what its input represents and what
// normalisation the answer needs, and a transform that silently divided by N
// would be wrong for half of them.
bool ForwardTransform(std::vector<double>& real,
                      std::vector<double>& imaginary);

}  // namespace ddd::analysis
