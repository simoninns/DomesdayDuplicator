/************************************************************************

    test_testdataanalyser.cpp

    Domesday Duplicator - GUI tests

    T1 coverage for the test-pattern ramp check (P7-19).

    This is the logic behind step 4 of the capture-integrity procedure, which is a hardware
    gate. That is exactly why it is worth unit testing: the bench session cannot tell a
    genuine pass from an analyser that says "pass" to everything, and a false pass there
    would sign off a capture path that is dropping samples.

    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include "testdataanalyser.h"

#include <cstdint>
#include <vector>

namespace
{
// Build a clean ramp of the given sequence length, starting at startValue
std::vector<uint16_t> makeRamp(size_t count, uint16_t sequenceLength, uint16_t startValue = 0)
{
    std::vector<uint16_t> samples;
    samples.reserve(count);
    uint16_t value = startValue;
    for (size_t i = 0; i < count; ++i)
    {
        samples.push_back(value);
        value = static_cast<uint16_t>((value + 1) % sequenceLength);
    }
    return samples;
}
} // namespace

// An unbroken ramp of the current gateware's 1021-long sequence passes, and the length is
// reported back rather than assumed.
TEST(TestDataAnalyser, AcceptsCleanRamp1021)
{
    const std::vector<uint16_t> samples = makeRamp(10000, 1021);

    TestDataAnalyser analyser;
    EXPECT_TRUE(analyser.Feed(samples.data(), samples.size()));
    EXPECT_TRUE(analyser.GetResult().passed);
    EXPECT_EQ(analyser.GetResult().sequenceLength, 1021);
    EXPECT_EQ(analyser.GetResult().samplesChecked, samples.size());
}

// The older gateware ramps 0..1023. A check that hard-coded either length would report the
// other as corrupt, which is the failure this test exists to prevent.
TEST(TestDataAnalyser, AcceptsCleanRamp1024)
{
    const std::vector<uint16_t> samples = makeRamp(10000, 1024);

    TestDataAnalyser analyser;
    EXPECT_TRUE(analyser.Feed(samples.data(), samples.size()));
    EXPECT_TRUE(analyser.GetResult().passed);
    EXPECT_EQ(analyser.GetResult().sequenceLength, 1024);
}

// The first sample seeds the expectation, so a capture that starts mid-sequence — which
// every real capture does — is not itself a failure.
TEST(TestDataAnalyser, AcceptsRampStartingMidSequence)
{
    const std::vector<uint16_t> samples = makeRamp(5000, 1021, 777);

    TestDataAnalyser analyser;
    EXPECT_TRUE(analyser.Feed(samples.data(), samples.size()));
    EXPECT_TRUE(analyser.GetResult().passed);
}

// A dropped sample is the thing this exists to catch, and the offset it reports is what
// makes a failure actionable at the bench.
TEST(TestDataAnalyser, DetectsDroppedSample)
{
    std::vector<uint16_t> samples = makeRamp(1000, 1021);
    samples.erase(samples.begin() + 500);

    TestDataAnalyser analyser;
    EXPECT_FALSE(analyser.Feed(samples.data(), samples.size()));
    EXPECT_FALSE(analyser.GetResult().passed);
    EXPECT_EQ(analyser.GetResult().samplesChecked, 500u);
    EXPECT_EQ(analyser.GetResult().expectedValue, 500);
    EXPECT_EQ(analyser.GetResult().actualValue, 501);
}

// A corrupted value in the middle of an otherwise clean ramp
TEST(TestDataAnalyser, DetectsCorruptedSample)
{
    std::vector<uint16_t> samples = makeRamp(1000, 1021);
    samples[250] = 999;

    TestDataAnalyser analyser;
    EXPECT_FALSE(analyser.Feed(samples.data(), samples.size()));
    EXPECT_EQ(analyser.GetResult().expectedValue, 250);
    EXPECT_EQ(analyser.GetResult().actualValue, 999);
}

// The analyser is fed in blocks as the file is read, so a break that falls on a block
// boundary must be caught in exactly the same way as one in the middle of a block.
TEST(TestDataAnalyser, DetectsBreakAcrossBlockBoundary)
{
    const std::vector<uint16_t> first = makeRamp(100, 1021);
    const std::vector<uint16_t> second = makeRamp(100, 1021, 150);

    TestDataAnalyser analyser;
    EXPECT_TRUE(analyser.Feed(first.data(), first.size()));
    EXPECT_FALSE(analyser.Feed(second.data(), second.size()));
    EXPECT_EQ(analyser.GetResult().expectedValue, 100);
    EXPECT_EQ(analyser.GetResult().actualValue, 150);
}

// A capture too short to wrap cannot report a sequence length. That is not a failure, but
// it is reported, because a pass over 900 samples is much weaker evidence than a pass over
// a whole disc and the operator should be able to tell the difference.
TEST(TestDataAnalyser, ShortCapturePassesWithoutSequenceLength)
{
    const std::vector<uint16_t> samples = makeRamp(900, 1021);

    TestDataAnalyser analyser;
    EXPECT_TRUE(analyser.Feed(samples.data(), samples.size()));
    EXPECT_TRUE(analyser.GetResult().passed);
    EXPECT_EQ(analyser.GetResult().sequenceLength, 0);
}

// Once it has failed it stays failed, and does not overwrite the first break with a later
// one — the first is the one worth diagnosing.
TEST(TestDataAnalyser, FailureIsSticky)
{
    std::vector<uint16_t> samples = makeRamp(1000, 1021);
    samples[100] = 0;
    samples[200] = 0;

    TestDataAnalyser analyser;
    EXPECT_FALSE(analyser.Feed(samples.data(), samples.size()));
    const uint64_t firstFailureOffset = analyser.GetResult().samplesChecked;

    EXPECT_FALSE(analyser.Feed(samples.data(), samples.size()));
    EXPECT_EQ(analyser.GetResult().samplesChecked, firstFailureOffset);
}
