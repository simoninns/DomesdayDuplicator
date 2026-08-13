/************************************************************************

    test_test_pattern_verifier.cpp

    T1 tests for the test-pattern ramp check
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <optional>
#include <vector>

#include "test_pattern_verifier.h"
#include "wire_data.h"

namespace ddd::capture {
namespace {

std::vector<uint16_t> Ramp(uint16_t first, size_t count, uint16_t length) {
  std::vector<uint16_t> samples;
  samples.reserve(count);
  uint16_t value = first;
  for (size_t index = 0; index < count; ++index) {
    samples.push_back(value);
    ++value;
    if (value >= length) {
      value = 0;
    }
  }
  return samples;
}

TEST(TestPatternVerifierTest, AnIntactRampPasses) {
  const std::vector<uint16_t> samples = Ramp(0, 5000, 1021);

  TestPatternVerifier verifier;
  EXPECT_TRUE(verifier.Feed(samples.data(), samples.size()));
  EXPECT_FALSE(verifier.HasFailed());
  EXPECT_EQ(verifier.GetResult().samples_checked, samples.size());
}

TEST(TestPatternVerifierTest, TheCaptureMayStartAnywhereInTheRamp) {
  // The device is already counting when the host opens it, so the first sample
  // seeds the expectation rather than being checked against zero.
  const std::vector<uint16_t> samples = Ramp(700, 5000, 1021);

  TestPatternVerifier verifier;
  EXPECT_TRUE(verifier.Feed(samples.data(), samples.size()));
  EXPECT_FALSE(verifier.HasFailed());
}

TEST(TestPatternVerifierTest, TheCurrentGatewareRampLengthIsDiscovered) {
  const std::vector<uint16_t> samples = Ramp(0, 3000, 1021);

  TestPatternVerifier verifier;
  ASSERT_TRUE(verifier.Feed(samples.data(), samples.size()));
  // Compared as an optional rather than dereferenced: the assertion then covers
  // "a length was found" and "it was the right one" in one statement.
  EXPECT_EQ(verifier.GetResult().sequence_length,
            std::optional<uint16_t>(1021));
}

TEST(TestPatternVerifierTest, TheLegacyGatewareRampLengthIsAlsoDiscovered) {
  // Older gateware ramps 0..1023. A check that assumed either length would
  // report the other as corrupt, which is why the length is found rather than
  // assumed.
  const std::vector<uint16_t> samples = Ramp(0, 3000, 1024);

  TestPatternVerifier verifier;
  ASSERT_TRUE(verifier.Feed(samples.data(), samples.size()));
  EXPECT_EQ(verifier.GetResult().sequence_length,
            std::optional<uint16_t>(1024));
}

TEST(TestPatternVerifierTest, ACaptureTooShortToWrapReportsNoLength) {
  // Not a failure, but worth reporting: a pass over 900 samples proves much
  // less than a pass over a disc.
  const std::vector<uint16_t> samples = Ramp(0, 900, 1021);

  TestPatternVerifier verifier;
  ASSERT_TRUE(verifier.Feed(samples.data(), samples.size()));
  EXPECT_FALSE(verifier.HasFailed());
  EXPECT_FALSE(verifier.GetResult().sequence_length.has_value());
}

TEST(TestPatternVerifierTest, ABreakIsReportedWithItsExactOffset) {
  std::vector<uint16_t> samples = Ramp(0, 5000, 1021);
  samples[3000] = 999;

  TestPatternVerifier verifier;
  EXPECT_FALSE(verifier.Feed(samples.data(), samples.size()));
  EXPECT_TRUE(verifier.HasFailed());

  const TestPatternVerifier::Result& result = verifier.GetResult();
  EXPECT_EQ(result.samples_checked, 3000U);
  EXPECT_EQ(result.actual_value, 999);
  EXPECT_NE(result.expected_value, 999);
}

TEST(TestPatternVerifierTest, ADroppedSampleIsCaughtRatherThanAbsorbed) {
  // The failure that actually happens: a lost transfer means the ramp jumps
  // forward, and the values on either side are individually plausible.
  std::vector<uint16_t> samples = Ramp(0, 4000, 1021);
  samples.erase(samples.begin() + 2000);

  TestPatternVerifier verifier;
  EXPECT_FALSE(verifier.Feed(samples.data(), samples.size()));
  EXPECT_EQ(verifier.GetResult().samples_checked, 2000U);
}

TEST(TestPatternVerifierTest, FeedingIsIgnoredOnceItHasFailed) {
  std::vector<uint16_t> samples = Ramp(0, 100, 1021);
  samples[50] = 4;

  TestPatternVerifier verifier;
  ASSERT_FALSE(verifier.Feed(samples.data(), samples.size()));
  const uint64_t checked = verifier.GetResult().samples_checked;

  const std::vector<uint16_t> more = Ramp(0, 100, 1021);
  EXPECT_FALSE(verifier.Feed(more.data(), more.size()));
  EXPECT_EQ(verifier.GetResult().samples_checked, checked);
}

TEST(TestPatternVerifierTest, ARampSplitAcrossCallsIsStillContinuous) {
  // Buffers arrive one at a time on the capture path, and the state has to
  // survive the boundary or every buffer would look like a break.
  const std::vector<uint16_t> samples = Ramp(0, 4000, 1021);

  TestPatternVerifier verifier;
  ASSERT_TRUE(verifier.Feed(samples.data(), 1500));
  ASSERT_TRUE(verifier.Feed(samples.data() + 1500, 1500));
  ASSERT_TRUE(verifier.Feed(samples.data() + 3000, 1000));
  EXPECT_FALSE(verifier.HasFailed());
  EXPECT_EQ(verifier.GetResult().samples_checked, samples.size());
}

TEST(TestPatternVerifierTest, WireBytesAreReadWithoutCopyingThemFirst) {
  // The capture path has the data in the device's own layout already. This is
  // the entry point it uses, and it must agree with the sample-value one.
  test::WireStreamBuilder builder;
  builder.AppendRamp(4000);

  TestPatternVerifier from_wire;
  EXPECT_TRUE(
      from_wire.FeedWireBytes(builder.bytes().data(), builder.bytes().size()));

  const std::vector<uint16_t> samples = Ramp(0, 4000, 1021);
  TestPatternVerifier from_samples;
  EXPECT_TRUE(from_samples.Feed(samples.data(), samples.size()));

  EXPECT_EQ(from_wire.GetResult().samples_checked,
            from_samples.GetResult().samples_checked);
  EXPECT_EQ(from_wire.GetResult().sequence_length,
            from_samples.GetResult().sequence_length);
}

TEST(TestPatternVerifierTest, SequenceMarkersDoNotLookLikeSignal) {
  // Wire words carry a counter in their top six bits. Feeding them without
  // masking would make every counter change look like a ramp break.
  test::WireStreamBuilder builder(17);
  builder.AppendRamp(2000);

  TestPatternVerifier verifier;
  EXPECT_TRUE(
      verifier.FeedWireBytes(builder.bytes().data(), builder.bytes().size()));
  EXPECT_FALSE(verifier.HasFailed());
}

}  // namespace
}  // namespace ddd::capture
