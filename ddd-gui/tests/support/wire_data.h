/************************************************************************

    wire_data.h

    Fabricating device data for tests
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "sample_format.h"

namespace ddd::capture::test {

// A stream of wire words built the way the device builds them, so that the
// engine's own reading of them can be checked against something independent.
//
// Deliberately a second implementation rather than a call into
// SyntheticSource: a test that generates its input with the same code it is
// testing proves only that the code agrees with itself.
class WireStreamBuilder {
 public:
  // sample_values are 10-bit; the sequence counter is applied on top.
  explicit WireStreamBuilder(
      uint8_t starting_counter = 0,
      uint32_t samples_until_increment = kSamplesPerSequenceCounter)
      : counter_(starting_counter),
        samples_until_increment_(samples_until_increment) {}

  // Append one sample.
  void Append(uint16_t sample_value) {
    const uint16_t word = MakeWireWord(sample_value, counter_);
    bytes_.push_back(static_cast<uint8_t>(word & 0xFF));
    bytes_.push_back(static_cast<uint8_t>((word >> 8) & 0xFF));

    --samples_until_increment_;
    if (samples_until_increment_ == 0) {
      Advance();
    }
  }

  // Append `count` samples all of the same value.
  void AppendConstant(uint16_t sample_value, size_t count) {
    for (size_t index = 0; index < count; ++index) {
      Append(sample_value);
    }
  }

  // Append `count` samples of the device's test ramp, continuing where the last
  // call left off.
  void AppendRamp(size_t count, uint16_t ramp_length = 1021) {
    for (size_t index = 0; index < count; ++index) {
      Append(ramp_);
      ++ramp_;
      if (ramp_ >= ramp_length) {
        ramp_ = 0;
      }
    }
  }

  // Skip a sequence counter value, as a lost transfer would.
  void SkipCounter() { Advance(); }

  const std::vector<uint8_t>& bytes() const { return bytes_; }
  std::vector<uint8_t>& bytes() { return bytes_; }
  uint8_t counter() const { return counter_; }

 private:
  void Advance() {
    ++counter_;
    if (counter_ >= kSequenceCounterValues) {
      counter_ = 0;
    }
    samples_until_increment_ = kSamplesPerSequenceCounter;
  }

  std::vector<uint8_t> bytes_;
  uint8_t counter_ = 0;
  uint32_t samples_until_increment_ = kSamplesPerSequenceCounter;
  uint16_t ramp_ = 0;
};

// Read one sample value back out of a wire buffer.
inline uint16_t SampleAt(const std::vector<uint8_t>& bytes, size_t index) {
  const uint16_t word = static_cast<uint16_t>(
      static_cast<uint16_t>(bytes[index * kBytesPerSample]) |
      static_cast<uint16_t>(
          static_cast<uint16_t>(bytes[(index * kBytesPerSample) + 1]) << 8));
  return SampleValueFromWord(word);
}

// Read the raw word, markers and all.
inline uint16_t WordAt(const std::vector<uint8_t>& bytes, size_t index) {
  return static_cast<uint16_t>(
      static_cast<uint16_t>(bytes[index * kBytesPerSample]) |
      static_cast<uint16_t>(
          static_cast<uint16_t>(bytes[(index * kBytesPerSample) + 1]) << 8));
}

}  // namespace ddd::capture::test
