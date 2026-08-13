/************************************************************************

    test_sample_format.cpp

    T1 tests for the device's sample layout and the capture file naming
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include "capture_format.h"
#include "sample_format.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

TEST(SampleFormatTest, TheWireRateIsEightyMegabytesPerSecond) {
  // The number the whole design is built around. If this ever changes, every
  // buffer-sizing decision in the engine has to be revisited, so it is asserted
  // rather than left as an assumption in a comment.
  EXPECT_EQ(kWireBytesPerSecond, 80'000'000U);
}

TEST(SampleFormatTest, AWordSplitsIntoASampleAndACounter) {
  const uint16_t word = MakeWireWord(0x2AB, 37);

  EXPECT_EQ(SampleValueFromWord(word), 0x2AB);
  EXPECT_EQ(SequenceCounterFromWord(word), 37);
}

TEST(SampleFormatTest, TheCounterCannotReachIntoTheSampleValue) {
  // The two fields share a 16-bit word, so the check that matters is that the
  // largest legal value of each leaves the other alone.
  const uint16_t word = MakeWireWord(kMaximumSampleValue, 62);

  EXPECT_EQ(SampleValueFromWord(word), kMaximumSampleValue);
  EXPECT_EQ(SequenceCounterFromWord(word), 62);
}

TEST(SampleFormatTest, TheHighByteConstantsAgreeWithTheWordConstants) {
  // The hot loop reads bytes rather than assembling words, so it uses a second
  // set of shifts and masks. They have to describe the same layout, and this is
  // what stops the two drifting apart.
  for (uint16_t value = 0; value <= kMaximumSampleValue; value += 7) {
    for (uint8_t counter = 0; counter < kSequenceCounterValues; counter += 5) {
      const uint16_t word = MakeWireWord(value, counter);
      const uint8_t high_byte = static_cast<uint8_t>(word >> 8);

      EXPECT_EQ(high_byte >> kSequenceCounterHighByteShift, counter);
      EXPECT_EQ(
          static_cast<uint16_t>(
              (word & 0xFF) | static_cast<uint16_t>(
                                  (high_byte & kSampleValueHighByteMask) << 8)),
          value);
    }
  }
}

TEST(SampleFormatTest, TheScalingIsTheOneLdDecodeExpects) {
  // ld-decode's lds.py calls (value - 512) * 64 "the DdD 16-bit format". These
  // three points pin it: the bottom of the range, the midpoint, and the top.
  EXPECT_EQ(ToSigned16Bit(0), -32768);
  EXPECT_EQ(ToSigned16Bit(512), 0);
  EXPECT_EQ(ToSigned16Bit(1023), 32704);
}

TEST(SampleFormatTest, ScalingRoundTripsThroughEveryTenBitValue) {
  for (int32_t value = 0; value <= kMaximumSampleValue; ++value) {
    EXPECT_EQ(ToTenBit(ToSigned16Bit(value)), value) << "value " << value;
  }
}

TEST(CaptureFormatTest, TheDefaultSuffixSaysWhereTheSamplesCameFrom) {
  EXPECT_EQ(AddCaptureFileSuffix("disc1").string(), "disc1.ddd.flac");
}

TEST(CaptureFormatTest, AddingTheSuffixTwiceDoesNothingTheSecondTime) {
  // The name arrives from a text field a user can type into, so this is a
  // condition that reaches the code rather than a hypothetical one.
  const std::filesystem::path once = AddCaptureFileSuffix("disc1");
  EXPECT_EQ(AddCaptureFileSuffix(once).string(), "disc1.ddd.flac");
}

TEST(CaptureFormatTest, ExtensionsAreComparedWithoutRegardToCase) {
  EXPECT_EQ(LowerCaseExtension("capture.FLAC"), "flac");
  EXPECT_EQ(LowerCaseExtension("capture.Raw"), "raw");
  EXPECT_EQ(LowerCaseExtension("capture"), "");
}

TEST(WireProtocolTest, TheConfigurationWordCarriesOnlyTheTestModeBit) {
  // Every other bit is reserved and must be sent as zero: that is what makes
  // adding a flag later a firmware change rather than a protocol break.
  EXPECT_EQ(MakeConfigurationFlags(false), 0x0000);
  EXPECT_EQ(MakeConfigurationFlags(true), 0x0001);
}

TEST(WireProtocolTest, TheIdentifiersAreTheAssignedOnes) {
  // pid.codes allocated these. A wrong value here means the application does
  // not find the device at all, which is worth one assertion.
  EXPECT_EQ(kVendorId, 0x1209);
  EXPECT_EQ(kProductId, 0x2347);
  EXPECT_EQ(kBulkInEndpoint, 0x81);
}

}  // namespace
}  // namespace ddd::capture
