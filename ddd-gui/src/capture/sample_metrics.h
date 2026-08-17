/************************************************************************

    sample_metrics.h

    What the signal looked like, accumulated as it goes past
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

namespace ddd::capture {

// One buffer's worth of tallies, produced by the single pass that also
// validates the sequence markers.
//
// Separated out as a struct so that the fusing of validation and measurement is
// visible rather than implied: the two are done together because a 2 MB buffer
// does not fit in cache and reading it twice costs more than everything else on
// the processing thread put together, not because they are conceptually one
// thing.
struct BufferTally {
  uint64_t sample_count = 0;

  // Both are seeded so that an empty tally merges harmlessly
  uint16_t minimum_value = UINT16_MAX;
  uint16_t maximum_value = 0;

  uint64_t clipped_low_count = 0;
  uint64_t clipped_high_count = 0;

  // Sum of (value - 512)^2 over the buffer, in 10-bit units.
  //
  // 64 bits is not a guess: the largest term is 512^2, so a full hour at 40
  // Msps accumulates at most 3.8e16, well inside the range. Anything narrower
  // would overflow partway through a long capture, which is the length of
  // capture this application exists for.
  uint64_t sum_of_squares = 0;
};

// What a monitoring consumer sees. A plain value, so it can be copied out of
// the published stats block without touching the accumulator.
struct SampleMetricsSnapshot {
  uint64_t sample_count = 0;

  // Over the whole capture so far
  uint16_t minimum_value = 0;
  uint16_t maximum_value = 0;
  uint64_t clipped_low_count = 0;
  uint64_t clipped_high_count = 0;
  double rms = 0.0;

  // Over the most recent buffer only. The distinction matters to a user
  // adjusting RF gain: a whole-capture maximum records the worst moment since
  // the run started and will not come back down, so it cannot show that a
  // change has helped.
  uint16_t recent_minimum_value = 0;
  uint16_t recent_maximum_value = 0;
  uint64_t recent_clipped_low_count = 0;
  uint64_t recent_clipped_high_count = 0;
  double recent_rms = 0.0;

  // Over the samples that went into the file, and nothing else.
  //
  // The three sets above all cover the whole run, which begins when monitoring
  // does and usually runs on either side of the capture. That is right for a
  // display somebody is watching and wrong for the metadata written beside a
  // file: a capture's metadata describes the recording, and a maximum that
  // includes a minute of setting up before the file was opened describes
  // something that was never recorded.
  //
  // Measured rather than derived. A count and a clipping tally could be got by
  // subtracting the figures at the start of the capture from the figures at the
  // end, but a minimum and a maximum cannot — they only move one way — so the
  // span is accumulated in its own right. It costs six scalar operations per
  // two-megabyte buffer.
  //
  // Zero throughout until a capture has run, and frozen at the moment the file
  // is closed rather than left growing: see BeginCaptureSpan and
  // EndCaptureSpan.
  uint64_t capture_sample_count = 0;
  uint16_t capture_minimum_value = 0;
  uint16_t capture_maximum_value = 0;
  uint64_t capture_clipped_low_count = 0;
  uint64_t capture_clipped_high_count = 0;
  double capture_rms = 0.0;
};

// Accumulates per-buffer tallies into the figures the monitor panels show.
//
// Thread-safety: none. Owned and driven by the processing thread; readers get a
// copy through the monitor tap, never a reference to this.
class SampleMetrics {
 public:
  void Accumulate(const BufferTally& tally);

  SampleMetricsSnapshot Snapshot() const;

  void Reset();

  // Start measuring a capture, discarding whatever the previous one measured.
  //
  // Called when a writer is attached, which happens at a buffer boundary — so
  // the span begins at exactly the sample the file begins at.
  void BeginCaptureSpan();

  // Stop measuring it, leaving the figures where they stand.
  //
  // Called when the writer is detached, for the same reason: without it the
  // span would go on growing through the tick or two between the file closing
  // and its metadata being written, and the file would be described as
  // containing samples that reached no file at all.
  void EndCaptureSpan();

 private:
  uint64_t sample_count_ = 0;
  uint16_t minimum_value_ = UINT16_MAX;
  uint16_t maximum_value_ = 0;
  uint64_t clipped_low_count_ = 0;
  uint64_t clipped_high_count_ = 0;
  uint64_t sum_of_squares_ = 0;

  BufferTally recent_;

  // The span a file's own samples fall in, and whether it is still open.
  BufferTally capture_;
  bool capturing_ = false;
};

}  // namespace ddd::capture
