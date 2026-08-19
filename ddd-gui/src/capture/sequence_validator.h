/************************************************************************

    sequence_validator.h

    Proving a capture is bit-perfect, one buffer at a time
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include "sample_metrics.h"

namespace ddd::capture {

// Where the validator is in its life
enum class SequenceState {
  // Looking for the first counter change, which is what reveals the phase
  kSynchronising,

  // Locked on and checking every sample
  kRunning,

  // No sequence markers were found in the first stretch of data. Older gateware
  // does not emit them, and refusing to capture from a working device because
  // its firmware predates a diagnostic would be the wrong trade — so the
  // capture continues, unverified, and the application says so.
  kDisabled,

  // A counter did not follow its predecessor. Samples have been lost, and there
  // is no recovering them.
  kFailed,
};

const char* SequenceStateName(SequenceState state);

// Validates the 6-bit sequence counter carried in the top bits of every sample,
// strips it, and measures the signal — all in one pass over the buffer.
//
// The single pass is the whole point. A 2 MB buffer does not fit in any cache
// this will run on, so reading it twice costs two trips to main memory, and at
// 80 MB/s that is the difference between comfortable and marginal. Validation,
// stripping and measurement therefore share a loop even though they are three
// separate ideas.
//
// The stripping is destructive: the buffer is rewritten in place with the
// marker bits cleared, so what the sink writes is sample data and nothing else.
// A caller that wants the markers must look before calling.
//
// Thread-safety: none. One instance belongs to the processing thread for the
// life of a capture.
class SequenceValidator {
 public:
  struct Outcome {
    // False only for a genuine mismatch. A stream with no markers at all is not
    // a failure — see kDisabled.
    bool ok = true;

    // Always populated, mismatch or not: the samples up to the break were
    // real, and a user looking at why a capture stopped wants to see them.
    BufferTally tally;

    // Where in this buffer the mismatch was, as a sample index, and what the
    // two counters were. Meaningless when ok is true.
    uint64_t mismatch_sample_index = 0;
    uint8_t expected_counter = 0;
    uint8_t actual_counter = 0;

    // How the validator came to expect what it did. A mismatch says samples
    // were lost, but not where the loss is: the same message comes from a
    // stream with a hole in it and from a validator that locked onto the
    // wrong phase, and those have opposite causes. These say which.
    //
    // synchronised_here is true when the lock was acquired during this call,
    // in which case synchronisation_sample_index is where the first counter
    // change was seen and first_counter is what the buffer opened with. A
    // lock at sample 1 means the opening sample disagreed with its
    // neighbour, which is a corrupt or unwritten buffer head rather than a
    // counter boundary.
    bool synchronised_here = false;
    uint64_t synchronisation_sample_index = 0;
    uint8_t first_counter = 0;

    // Samples the validator still expected to carry expected_counter. The
    // shortfall against a full counter period is exactly how many samples the
    // stream is missing, which is the figure that says whether a whole
    // transfer went astray or a handful of samples did.
    uint32_t samples_expected_remaining = 0;
  };

  // Validate, strip and measure one buffer. byte_count must be even; a trailing
  // odd byte is not a sample and is left alone.
  Outcome Process(uint8_t* buffer, size_t byte_count);

  SequenceState state() const { return state_; }

  // True once the validator has decided whether this stream carries markers.
  bool synchronised() const { return state_ != SequenceState::kSynchronising; }

  void Reset();

 private:
  SequenceState state_ = SequenceState::kSynchronising;

  // The counter value every sample should currently carry, 0..62
  uint8_t counter_value_ = 0;

  // Samples still to go before the counter increments. Carried across buffer
  // boundaries, which is what lets a buffer size that is not a whole number of
  // counter periods work — the device does not know where our buffers end.
  uint32_t samples_until_increment_ = 0;
};

}  // namespace ddd::capture
