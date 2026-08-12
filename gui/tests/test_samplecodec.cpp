/************************************************************************

    test_samplecodec.cpp

    Domesday Duplicator - GUI tests

    T1 (unit) and T2 (golden) coverage for the 10-bit/16-bit sample codec used by dddconv.

    This is the highest-consequence pure logic in the project. A defect here does not
    crash and does not produce an error message — it silently corrupts every capture that
    is ever converted, and the corruption is only visible by comparing against the original
    file, which by then may not exist.

    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include "samplecodec.h"

#include <array>
#include <cstdint>
#include <vector>

using namespace SampleCodec;

namespace
{

// --- Scalar conversions -----------------------------------------------------------------

TEST(SampleCodec, ZeroMapsToMidScale)
{
    // Signed-16-bit zero is mid-scale in the unsigned 10-bit representation
    EXPECT_EQ(toTenBit(0), 512);
    EXPECT_EQ(toSixteenBit(512), 0);
}

TEST(SampleCodec, TenBitRangeEndpoints)
{
    // The ADC produces 10 bits, so the meaningful range is 0..1023
    EXPECT_EQ(toSixteenBit(0), -32768);
    EXPECT_EQ(toSixteenBit(1023), 32704);

    // ...and back
    EXPECT_EQ(toTenBit(-32768), 0);
    EXPECT_EQ(toTenBit(32704), 1023);
}

TEST(SampleCodec, ScalarRoundTripIsExactForEveryTenBitValue)
{
    // Every one of the 1024 representable sample values must survive a round trip exactly.
    // This is the property that makes the packed format lossless.
    for (int32_t tenBit = 0; tenBit < 1024; ++tenBit)
    {
        EXPECT_EQ(toTenBit(toSixteenBit(tenBit)), tenBit) << "at ten-bit value " << tenBit;
    }
}

// --- Group packing ----------------------------------------------------------------------

TEST(SampleCodec, GroupSizesAreFourSamplesInFiveBytes)
{
    // The buffer arithmetic in dataconversion.cpp depends on this ratio
    EXPECT_EQ(samplesPerGroup, 4);
    EXPECT_EQ(bytesPerGroup, 5);
}

TEST(SampleCodec, AllZeroSamplesPackToMidScalePattern)
{
    const std::array<int16_t, 4> input = { 0, 0, 0, 0 };
    std::array<uint8_t, 5> packed{};

    packGroup(input.data(), packed.data());

    // Four samples of 512 (0b10'0000'0000) packed MSB-first
    EXPECT_EQ(packed[0], 0x80);
    EXPECT_EQ(packed[1], 0x20);
    EXPECT_EQ(packed[2], 0x08);
    EXPECT_EQ(packed[3], 0x02);
    EXPECT_EQ(packed[4], 0x00);
}

TEST(SampleCodec, EachSampleOccupiesItsOwnBitPositions)
{
    // Set one sample to all-ones and the rest to zero, and check only that sample's bits
    // move. This catches a shift or mask applied to the wrong sample — the classic way to
    // corrupt one channel of a packed format while the others look fine.
    for (int slot = 0; slot < 4; ++slot)
    {
        std::array<int16_t, 4> input = { 0, 0, 0, 0 };
        input[slot] = toSixteenBit(1023);

        std::array<uint8_t, 5> packed{};
        packGroup(input.data(), packed.data());

        std::array<int16_t, 4> unpacked{};
        unpackGroup(packed.data(), unpacked.data());

        for (int i = 0; i < 4; ++i)
        {
            const int32_t expected = (i == slot) ? 1023 : 512;
            EXPECT_EQ(toTenBit(unpacked[i]), expected)
                << "slot " << slot << " leaked into sample " << i;
        }
    }
}

TEST(SampleCodec, GroupRoundTripIsExhaustivelyExactPerSlot)
{
    // Sweep every representable value through every slot position
    for (int slot = 0; slot < 4; ++slot)
    {
        for (int32_t tenBit = 0; tenBit < 1024; ++tenBit)
        {
            std::array<int16_t, 4> input = { 0, 0, 0, 0 };
            input[slot] = toSixteenBit(tenBit);

            std::array<uint8_t, 5> packed{};
            packGroup(input.data(), packed.data());

            std::array<int16_t, 4> unpacked{};
            unpackGroup(packed.data(), unpacked.data());

            EXPECT_EQ(unpacked[slot], input[slot])
                << "slot " << slot << ", ten-bit value " << tenBit;
        }
    }
}

TEST(SampleCodec, PackedBytesAreFullyDetermined)
{
    // Every bit of all five output bytes must come from the input — no byte may be left
    // uninitialised. Pack all-ones and confirm every byte is saturated.
    const int16_t maxSample = toSixteenBit(1023);
    const std::array<int16_t, 4> input = { maxSample, maxSample, maxSample, maxSample };

    std::array<uint8_t, 5> packed{};
    packGroup(input.data(), packed.data());

    EXPECT_EQ(packed[0], 0xFF);
    EXPECT_EQ(packed[1], 0xFF);
    EXPECT_EQ(packed[2], 0xFF);
    EXPECT_EQ(packed[3], 0xFF);
    EXPECT_EQ(packed[4], 0xFF);
}

// --- Golden vectors (T2) ------------------------------------------------------------------
//
// Committed reference data. These pin the on-disk byte layout: if a future change alters
// the packing, files written by older versions stop reading correctly, and these fail.

struct GoldenVector
{
    const char *name;
    std::array<int32_t, 4> tenBit;   // input samples, 10-bit unsigned
    std::array<uint8_t, 5> packed;   // expected bytes on disk
};

const GoldenVector goldenVectors[] = {
    { "all-zero",     { 0, 0, 0, 0 },             { 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { "all-max",      { 1023, 1023, 1023, 1023 }, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } },
    { "mid-scale",    { 512, 512, 512, 512 },     { 0x80, 0x20, 0x08, 0x02, 0x00 } },
    { "ascending",    { 0, 1, 2, 3 },             { 0x00, 0x00, 0x10, 0x08, 0x03 } },
    { "single-lsb-0", { 1, 0, 0, 0 },             { 0x00, 0x40, 0x00, 0x00, 0x00 } },
    { "single-msb-3", { 0, 0, 0, 512 },           { 0x00, 0x00, 0x00, 0x02, 0x00 } },
    { "ramp-1020",    { 1020, 1021, 1022, 1023 }, { 0xFF, 0x3F, 0xDF, 0xFB, 0xFF } },
};

TEST(SampleCodecGolden, PackMatchesReferenceBytes)
{
    for (const auto &v : goldenVectors)
    {
        std::array<int16_t, 4> input{};
        for (int i = 0; i < 4; ++i)
        {
            input[i] = toSixteenBit(v.tenBit[i]);
        }

        std::array<uint8_t, 5> packed{};
        packGroup(input.data(), packed.data());

        for (int i = 0; i < 5; ++i)
        {
            EXPECT_EQ(packed[i], v.packed[i]) << v.name << ", byte " << i;
        }
    }
}

TEST(SampleCodecGolden, UnpackMatchesReferenceSamples)
{
    for (const auto &v : goldenVectors)
    {
        std::array<int16_t, 4> unpacked{};
        unpackGroup(v.packed.data(), unpacked.data());

        for (int i = 0; i < 4; ++i)
        {
            EXPECT_EQ(toTenBit(unpacked[i]), v.tenBit[i]) << v.name << ", sample " << i;
        }
    }
}

// --- The test-pattern ramp ----------------------------------------------------------------

TEST(SampleCodecGolden, TestPatternRampSurvivesConversion)
{
    // dataGenerator.v emits a 0..1020 counter ramp in test mode, and dddutil's analyser
    // checks that ramp is unbroken in a captured file. That whole procedure depends on
    // the ramp surviving pack/unpack without a discontinuity, so check it directly.
    constexpr int32_t rampLength = 1021;

    std::vector<int16_t> samples;
    samples.reserve(rampLength + 3);
    for (int32_t i = 0; i < rampLength; ++i)
    {
        samples.push_back(toSixteenBit(i));
    }
    // Pad to a whole number of groups
    while (samples.size() % samplesPerGroup != 0)
    {
        samples.push_back(toSixteenBit(0));
    }

    std::vector<uint8_t> packed(samples.size() / samplesPerGroup * bytesPerGroup);
    for (size_t g = 0; g < samples.size() / samplesPerGroup; ++g)
    {
        packGroup(&samples[g * samplesPerGroup], &packed[g * bytesPerGroup]);
    }

    std::vector<int16_t> recovered(samples.size());
    for (size_t g = 0; g < samples.size() / samplesPerGroup; ++g)
    {
        unpackGroup(&packed[g * bytesPerGroup], &recovered[g * samplesPerGroup]);
    }

    for (int32_t i = 0; i < rampLength; ++i)
    {
        ASSERT_EQ(toTenBit(recovered[i]), i) << "ramp broken at position " << i;
    }
}

} // namespace
