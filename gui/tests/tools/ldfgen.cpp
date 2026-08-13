/************************************************************************

    ldfgen.cpp

    Domesday Duplicator - interop test fixture generator (P7-23)

    Writes a synthetic capture through the *production* FlacWriter, plus the packed .lds
    the removed 10-bit path would have written from the same samples. interop-ldf.sh then
    checks the two against ld-decode's own tools.

    This is a test fixture, not a tool: it is built only when BUILD_TESTING is on and is
    never installed. It exists because the interop claim — "what this application writes is
    an .ldf" — cannot be checked without a file that the application's own encoder produced.

    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "captureformat.h"
#include "flacwriter.h"
#include "samplecodec.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace
{
// Deterministic, and shaped rather than uniform: a sine with bounded jitter compresses
// like real RF does, so the size comparison in the script means something. Fixed seed so
// two runs produce identical files and a diff failure is never noise.
std::vector<uint16_t> makeSampleData(size_t count)
{
    std::vector<uint16_t> samples;
    samples.reserve(count);
    std::mt19937 generator(20260812);
    std::uniform_int_distribution<int> jitter(-40, 40);
    for (size_t i = 0; i < count; ++i)
    {
        const int base = 512 + static_cast<int>(400.0 * std::sin(static_cast<double>(i) / 37.0));
        int value = base + jitter(generator);
        if (value < 0) value = 0;
        if (value > 1023) value = 1023;
        samples.push_back(static_cast<uint16_t>(value));
    }
    return samples;
}

// A pure 10-bit counter ramp, which is what the FPGA test-pattern generator produces. Used
// to check --analyse-test-data end to end without any hardware.
std::vector<uint16_t> makeRampData(size_t count, uint16_t sequenceLength)
{
    std::vector<uint16_t> samples;
    samples.reserve(count);
    uint16_t value = 0;
    for (size_t i = 0; i < count; ++i)
    {
        samples.push_back(value);
        value = static_cast<uint16_t>((value + 1) % sequenceLength);
    }
    return samples;
}

std::vector<uint8_t> toDeviceBuffer(const std::vector<uint16_t> &samples)
{
    std::vector<uint8_t> buffer;
    buffer.reserve(samples.size() * 2);
    for (const uint16_t sample : samples)
    {
        buffer.push_back(static_cast<uint8_t>(sample & 0x00FF));
        buffer.push_back(static_cast<uint8_t>((sample >> 8) & 0x00FF));
    }
    return buffer;
}

// The packing the capture application used to do, kept here rather than in production code
// because this is now the only thing that needs it: the reference .lds to compare an
// uncompressed .ldf against.
bool writePackedLds(const std::string &path, const std::vector<uint16_t> &samples)
{
    std::ofstream stream(path, std::ios::binary);
    if (!stream.is_open()) return false;

    for (size_t group = 0; group < samples.size() / SampleCodec::samplesPerGroup; ++group)
    {
        int16_t sixteenBit[SampleCodec::samplesPerGroup];
        for (int i = 0; i < SampleCodec::samplesPerGroup; ++i)
        {
            sixteenBit[i] = SampleCodec::toSixteenBit(samples[(group * SampleCodec::samplesPerGroup) + i]);
        }
        uint8_t packed[SampleCodec::bytesPerGroup];
        SampleCodec::packGroup(sixteenBit, packed);
        stream.write(reinterpret_cast<const char *>(packed), SampleCodec::bytesPerGroup);
    }

    return stream.good();
}
} // namespace

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "usage: ddd-ldfgen <output-basename> <sample-count> [--ramp]\n"
                  << "\n"
                  << "Writes <basename>.ldf and <basename>.lds from the same samples.\n"
                  << "--ramp writes the FPGA test pattern instead of synthetic RF.\n";
        return 2;
    }

    const std::string basename = argv[1];
    const size_t sampleCount = static_cast<size_t>(std::strtoull(argv[2], nullptr, 10));
    const bool ramp = (argc > 3) && (std::strcmp(argv[3], "--ramp") == 0);

    // A whole number of packed groups, so the .lds has no partial group at the end and the
    // comparison is over the same samples on both sides.
    const size_t alignedCount = (sampleCount / SampleCodec::samplesPerGroup) * SampleCodec::samplesPerGroup;
    const std::vector<uint16_t> samples = ramp ? makeRampData(alignedCount, 1021) : makeSampleData(alignedCount);
    const std::vector<uint8_t> deviceBuffer = toDeviceBuffer(samples);

    FlacWriter writer;
    FlacWriter::Options options;
    options.compressionLevel = 1;
    options.sampleRateLabel = CaptureFormats::flacSampleRateLabel;
    options.tags = { { "DDD_VERSION", "interop-fixture" }, { "DDD_DECIMATION", "1" } };

    std::string error;
    if (!writer.Open(basename + ".ldf", options, error))
    {
        std::cerr << error << "\n";
        return 1;
    }
    if (!writer.WriteRawDeviceSamples(deviceBuffer.data(), samples.size()))
    {
        std::cerr << writer.GetLastError() << "\n";
        return 1;
    }
    if (!writer.Finish())
    {
        std::cerr << writer.GetLastError() << "\n";
        return 1;
    }

    if (!writePackedLds(basename + ".lds", samples))
    {
        std::cerr << "Failed to write " << basename << ".lds\n";
        return 1;
    }

    std::cout << "Wrote " << samples.size() << " samples to " << basename << ".ldf and " << basename << ".lds\n";
    return 0;
}
