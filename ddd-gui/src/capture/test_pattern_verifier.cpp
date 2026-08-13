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

// The two ramp lengths the gateware has used. 1024 is the original 10-bit
// counter; 1021 is what the current test-pattern generator produces.
constexpr uint16_t kLegacySequenceLength = 1024;
constexpr uint16_t kCurrentSequenceLength = 1021;

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
  if (detected_sequence_length_.has_value() &&
      current_value_ == *detected_sequence_length_) {
    current_value_ = 0;
  }

  if (sample_value != current_value_) {
    // The first disagreement may be the sequence wrapping rather than a fault:
    // if the stream restarts at 0 exactly where one of the known ramp lengths
    // would end, that is the length being revealed, not a lost sample.
    if (!detected_sequence_length_.has_value() && sample_value == 0 &&
        (current_value_ == kCurrentSequenceLength ||
         current_value_ == kLegacySequenceLength)) {
      detected_sequence_length_ = current_value_;
      result_.sequence_length = current_value_;
      current_value_ = 0;
      ++result_.samples_checked;
      return true;
    }

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
