/************************************************************************

    test_pattern_verifier.cpp

    Test-pattern integrity check
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "test_pattern_verifier.h"

#include "sample_format.h"

namespace ddd::capture {
namespace {

// The ramp the gateware's test-pattern generator produces: 0 to 1020, wrapping
// at 1021. A second copy of the constant in fpga/application/dataGenerator.v,
// and the two must agree — a host that expected a different length would
// report every good capture as corrupt.
constexpr uint16_t kSequenceLength = 1021;

}  // namespace

bool TestPatternVerifier::FeedOne(uint16_t sample_value) {
  // The first sample of the capture is wherever in the ramp the device happened
  // to be, so it seeds the expectation rather than being checked against one.
  if (!have_first_sample_) {
    current_value_ = sample_value;
    have_first_sample_ = true;
    ++result_.samples_checked;
    return true;
  }

  ++current_value_;

  // Wrapped rather than discovered. `>=` and not `==` because the seed above
  // is whatever the first sample happened to be, and a stream that is not the
  // test pattern at all can start outside the ramp — from which `==` would
  // never come back round, and the run would fail on a mismatch several
  // samples later than the one that was actually wrong.
  if (current_value_ >= kSequenceLength) {
    current_value_ = 0;

    // Recorded because a capture that has been all the way round proves more
    // than one that has not, and the caller has no other way to tell.
    result_.sequence_length = kSequenceLength;
  }

  if (sample_value != current_value_) {
    result_.passed = false;
    result_.expected_value = current_value_;
    result_.actual_value = sample_value;
    return false;
  }

  ++result_.samples_checked;
  return true;
}

bool TestPatternVerifier::Feed(const uint16_t* samples, size_t count) {
  if (!result_.passed) {
    return false;
  }

  for (size_t index = 0; index < count; ++index) {
    if (!FeedOne(samples[index])) {
      return false;
    }
  }

  return true;
}

bool TestPatternVerifier::FeedWireBytes(const uint8_t* wire_data,
                                        size_t byte_count) {
  if (!result_.passed) {
    return false;
  }

  for (size_t offset = 0; (offset + kBytesPerSample) <= byte_count;
       offset += kBytesPerSample) {
    const uint16_t word = static_cast<uint16_t>(
        static_cast<uint16_t>(wire_data[offset]) |
        static_cast<uint16_t>(static_cast<uint16_t>(wire_data[offset + 1])
                              << 8));
    if (!FeedOne(SampleValueFromWord(word))) {
      return false;
    }
  }

  return true;
}

}  // namespace ddd::capture
