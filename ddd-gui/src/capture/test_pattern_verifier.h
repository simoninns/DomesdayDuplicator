/************************************************************************

    test_pattern_verifier.h

    Test-pattern integrity check
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace ddd::capture {

// With the FPGA's test-pattern generator running, every sample the device
// produces is the previous one plus one, wrapping at the end of the sequence.
// Any break in that ramp is a sample the capture path lost or corrupted, which
// makes this the one check that covers the whole chain — ADC, gateware, FX3,
// USB, buffering and the file writer — with a pass or fail a bench session can
// act on. It is step 4 of the capture-integrity procedure in TESTING.md.
//
// A pure function of a stream of 10-bit sample values: no file handling, so the
// same code serves the live capture path, the offline analysis of a written
// file, and the unit tests.
//
// Sequence length is discovered rather than assumed. Older gateware ramps
// 0..1023 and newer ramps 0..1020, and a check that hard-coded either would
// report the other as corrupt.
//
// Thread-safety: none, and none is wanted. The live path feeds one of these
// from the processing thread only.
class TestPatternVerifier {
 public:
  struct Result {
    // False once a break has been seen. Everything below describes that break.
    bool passed = true;

    // Samples consumed before the break, which is the offset of the bad sample
    uint64_t samples_checked = 0;
    uint16_t expected_value = 0;
    uint16_t actual_value = 0;

    // The ramp length that was detected, or nothing if the stream ended before
    // it wrapped. A capture too short to wrap is not a failure, but it is worth
    // reporting: a pass over 900 samples proves much less than a pass over a
    // disc.
    std::optional<uint16_t> sequence_length;
  };

  // Feed the next block of 10-bit sample values. Returns false once the ramp
  // has broken; further calls are ignored, so a caller can stop at its own
  // convenience.
  bool Feed(const uint16_t* samples, size_t count);

  // Feed a block still in the device's wire layout — 16-bit little-endian words
  // — extracting the sample value from each. The capture path has the data in
  // this form already and copying it out first would be a memcpy of 80 MB/s for
  // nothing.
  //
  // The words must already have had their sequence markers stripped, or must
  // never have carried any: this masks to the low 10 bits, so a marker left in
  // place is simply ignored rather than mistaken for signal.
  bool FeedWireBytes(const uint8_t* wire_data, size_t byte_count);

  bool HasFailed() const { return !result_.passed; }
  const Result& GetResult() const { return result_; }

 private:
  // Advance the state machine by one sample. Returns false on a break.
  bool FeedOne(uint16_t sample_value);

  Result result_;
  bool have_first_sample_ = false;
  uint16_t current_value_ = 0;

  // Nothing until the first wrap reveals it
  std::optional<uint16_t> detected_sequence_length_;
};

}  // namespace ddd::capture
