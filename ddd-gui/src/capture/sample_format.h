/************************************************************************

    sample_format.h

    The device's sample layout on the wire
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

// Every fact about what arrives from the device, in one place.
//
// The device delivers one 16-bit little-endian word per sample:
//
//     bits  9..0   the 10-bit unsigned sample value, 0..1023
//     bits 15..10  a 6-bit sequence counter
//
// The sequence counter increments once every 65,536 samples and wraps at 62,
// so it repeats every 63 * 65,536 samples. It is the only evidence a capture is
// bit-perfect: a host that stalls loses whole USB packets, and the gap shows up
// as a counter that skipped. Nothing else in the stream would reveal it.
//
// These constants are deliberately a component-local copy rather than shared
// with the gateware or the firmware (AGENTS.md §2). They describe a wire
// protocol, not shared code: if the gateware ever changes them, the three
// definitions must be able to disagree for as long as it takes to migrate.
namespace ddd::capture {

// Bytes per sample on the wire
inline constexpr size_t kBytesPerSample = 2;

// The device's real sampling rate
inline constexpr uint32_t kSampleRateHz = 40'000'000;

// 80 MB/s, continuously, for as long as a capture runs. Stated as a constant
// because almost every buffer-sizing decision in the engine is derived from it.
inline constexpr uint64_t kWireBytesPerSecond =
    static_cast<uint64_t>(kSampleRateHz) * kBytesPerSample;

// Sample value limits. A sample sitting at either end is clipped, and the count
// of those is what tells a user their RF gain is wrong.
inline constexpr uint16_t kMinimumSampleValue = 0;
inline constexpr uint16_t kMaximumSampleValue = 1023;

// The 10-bit value that represents zero once the signal is centred
inline constexpr int32_t kSampleZeroOffset = 512;

// Scale between the 10-bit and 16-bit representations
inline constexpr int32_t kSampleScale = 64;

// Mask selecting the sample value out of a wire word
inline constexpr uint16_t kSampleValueMask = 0x03FF;

// The sequence counter occupies the top six bits of the word. It is expressed
// here in terms of the high byte as well, because the hot loop reads bytes: it
// never assembles the 16-bit word just to extract the counter.
inline constexpr int kSequenceCounterShift = 10;
inline constexpr uint8_t kSequenceCounterHighByteShift = 2;
inline constexpr uint8_t kSampleValueHighByteMask = 0x03;

// Sequence counter values run 0..62 inclusive — 63 distinct values, not 64.
inline constexpr uint32_t kSequenceCounterValues = 63;

// Samples carrying each sequence counter value
inline constexpr uint32_t kSamplesPerSequenceCounter = 65'536;

// Extract the 10-bit sample value from a little-endian wire word.
inline constexpr uint16_t SampleValueFromWord(uint16_t word) {
  return static_cast<uint16_t>(word & kSampleValueMask);
}

// Extract the 6-bit sequence counter from a little-endian wire word.
inline constexpr uint8_t SequenceCounterFromWord(uint16_t word) {
  return static_cast<uint8_t>(word >> kSequenceCounterShift);
}

// Build a wire word from a sample value and a sequence counter. Used by the
// synthetic source and by every test that needs to fabricate device data.
inline constexpr uint16_t MakeWireWord(uint16_t sample_value,
                                       uint8_t sequence_counter) {
  return static_cast<uint16_t>(
      (sample_value & kSampleValueMask) |
      static_cast<uint16_t>(static_cast<uint16_t>(sequence_counter)
                            << kSequenceCounterShift));
}

// Convert a 10-bit unsigned sample to the signed 16-bit representation
// ld-decode calls the DdD 16-bit format.
//
// This is not a local convention: lddecode/lds.py unpacks captures into exactly
// this before handing them to flac, which is why a FLAC file written here needs
// no format negotiation with the decode toolchain.
inline constexpr int16_t ToSigned16Bit(int32_t ten_bit_value) {
  return static_cast<int16_t>((ten_bit_value - kSampleZeroOffset) *
                              kSampleScale);
}

// The inverse, for reading a capture back into the 10-bit domain the device's
// test pattern counts in.
inline constexpr int32_t ToTenBit(int16_t signed_value) {
  return (signed_value / kSampleScale) + kSampleZeroOffset;
}

}  // namespace ddd::capture
