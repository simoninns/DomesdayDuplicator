/************************************************************************

    test_flacroundtrip.cpp

    Domesday Duplicator - GUI tests

    T1 and T2 coverage for the .ldf capture output (P7-21) and the capture reader (P7-23).

    What this proves: samples handed to the writer come back out of the reader unchanged,
    the file is a real Ogg FLAC stream with the header fields an .ldf must carry, and the
    same samples read identically whether they arrive as .ldf, .lds or .raw.

    What it deliberately does not prove: that ld-decode can read the result. No unit test
    can — that needs ld-decode. The scripted interop check in TESTING.md covers it by
    round-tripping through ld-compress and running a decode, and the two together are the
    evidence P7-23 asks for.

    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include "capturereader.h"
#include "captureformat.h"
#include "flacwriter.h"
#include "samplecodec.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace
{
// A temporary file that removes itself, so a failing assertion cannot leave litter behind
class ScopedTempFile
{
public:
    explicit ScopedTempFile(const std::string &suffix)
    {
        // random_device rather than the process id, which needs a different header on
        // Windows than it does here and this suite has to build on both.
        static const unsigned int runId = std::random_device{}();
        path = std::filesystem::temp_directory_path() /
               ("ddd-test-" + std::to_string(runId) + "-" + std::to_string(counter++) + suffix);
    }

    ~ScopedTempFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }

    const std::filesystem::path &Path() const { return path; }

private:
    std::filesystem::path path;
    static inline int counter = 0;
};

// The device's own buffer layout: 16-bit little-endian words each holding a 10-bit value
std::vector<uint8_t> toDeviceBuffer(const std::vector<uint16_t> &tenBitSamples)
{
    std::vector<uint8_t> buffer;
    buffer.reserve(tenBitSamples.size() * 2);
    for (const uint16_t sample : tenBitSamples)
    {
        buffer.push_back(static_cast<uint8_t>(sample & 0x00FF));
        buffer.push_back(static_cast<uint8_t>((sample >> 8) & 0x00FF));
    }
    return buffer;
}

// Something with structure rather than uniform noise, so the encoder has to do real work:
// a ramp is what the test pattern produces, and a pseudo-random component is closer to RF.
std::vector<uint16_t> makeSampleData(size_t count)
{
    std::vector<uint16_t> samples;
    samples.reserve(count);
    std::mt19937 generator(12345);
    std::uniform_int_distribution<int> jitter(-40, 40);
    for (size_t i = 0; i < count; ++i)
    {
        const int base = 512 + static_cast<int>(400.0 * std::sin(static_cast<double>(i) / 37.0));
        samples.push_back(static_cast<uint16_t>(std::clamp(base + jitter(generator), 0, 1023)));
    }
    return samples;
}

std::vector<uint16_t> readAll(CaptureReader &reader)
{
    std::vector<uint16_t> all;
    std::vector<uint16_t> chunk;
    bool endOfFile = false;
    while (!endOfFile)
    {
        EXPECT_TRUE(reader.Read(chunk, 4096, endOfFile));
        all.insert(all.end(), chunk.begin(), chunk.end());
        if (chunk.empty() && endOfFile) break;
    }
    return all;
}
} // namespace

// The core promise: what goes into the encoder comes out of the decoder, sample for
// sample. FLAC is lossless, so anything less than exact equality here is a defect.
TEST(FlacRoundTrip, SamplesSurviveIntact)
{
    const std::vector<uint16_t> original = makeSampleData(50000);
    const std::vector<uint8_t> deviceBuffer = toDeviceBuffer(original);

    ScopedTempFile file(".ldf");

    {
        FlacWriter writer;
        FlacWriter::Options options;
        options.compressionLevel = 1;
        options.sampleRateLabel = CaptureFormats::flacSampleRateLabel;
        std::string error;
        ASSERT_TRUE(writer.Open(file.Path(), options, error)) << error;
        ASSERT_TRUE(writer.WriteRawDeviceSamples(deviceBuffer.data(), original.size()));
        ASSERT_TRUE(writer.Finish()) << writer.GetLastError();
    }

    CaptureReader reader;
    std::string error;
    ASSERT_TRUE(reader.Open(file.Path(), CaptureReader::Format::FlacOgg, error)) << error;

    const std::vector<uint16_t> decoded = readAll(reader);
    ASSERT_EQ(decoded.size(), original.size());
    EXPECT_EQ(decoded, original);
}

// The file must be an Ogg stream, not native FLAC: ld-decode's .ldf is Ogg-encapsulated,
// and a native FLAC file with the right samples in it is still the wrong format.
TEST(FlacRoundTrip, OutputIsOggEncapsulated)
{
    const std::vector<uint16_t> original = makeSampleData(8192);
    const std::vector<uint8_t> deviceBuffer = toDeviceBuffer(original);

    ScopedTempFile file(".ldf");
    {
        FlacWriter writer;
        FlacWriter::Options options;
        std::string error;
        ASSERT_TRUE(writer.Open(file.Path(), options, error)) << error;
        ASSERT_TRUE(writer.WriteRawDeviceSamples(deviceBuffer.data(), original.size()));
        ASSERT_TRUE(writer.Finish());
    }

    std::ifstream stream(file.Path(), std::ios::binary);
    char magic[4] = {};
    stream.read(magic, 4);
    EXPECT_EQ(std::string(magic, 4), "OggS");
}

// Compression has to actually happen. A level that silently fell back to storing raw
// samples would still round-trip perfectly and would still be a defect, because the whole
// point of the format change is the file being smaller than what it replaced.
TEST(FlacRoundTrip, OutputIsSmallerThanPackedTenBit)
{
    const std::vector<uint16_t> original = makeSampleData(200000);
    const std::vector<uint8_t> deviceBuffer = toDeviceBuffer(original);

    ScopedTempFile file(".ldf");
    {
        FlacWriter writer;
        FlacWriter::Options options;
        options.compressionLevel = 1;
        std::string error;
        ASSERT_TRUE(writer.Open(file.Path(), options, error)) << error;
        ASSERT_TRUE(writer.WriteRawDeviceSamples(deviceBuffer.data(), original.size()));
        ASSERT_TRUE(writer.Finish());
    }

    const uintmax_t flacSize = std::filesystem::file_size(file.Path());
    const uintmax_t packedSize = (original.size() / SampleCodec::samplesPerGroup) * SampleCodec::bytesPerGroup;
    EXPECT_LT(flacSize, packedSize);
}

// Decimation is a stride through the same writer, not a second format (P7-22), so every
// fourth sample must be what lands in the file.
TEST(FlacRoundTrip, DecimationKeepsEveryFourthSample)
{
    const std::vector<uint16_t> original = makeSampleData(40000);
    const std::vector<uint8_t> deviceBuffer = toDeviceBuffer(original);

    ScopedTempFile file(".ldf");
    {
        FlacWriter writer;
        FlacWriter::Options options;
        options.sampleRateLabel = CaptureFormats::flacSampleRateLabelDecimated;
        std::string error;
        ASSERT_TRUE(writer.Open(file.Path(), options, error)) << error;
        ASSERT_TRUE(writer.WriteRawDeviceSamples(deviceBuffer.data(), original.size(), 4));
        ASSERT_TRUE(writer.Finish());
    }

    CaptureReader reader;
    std::string error;
    ASSERT_TRUE(reader.Open(file.Path(), CaptureReader::Format::FlacOgg, error)) << error;
    const std::vector<uint16_t> decoded = readAll(reader);

    ASSERT_EQ(decoded.size(), original.size() / 4);
    for (size_t i = 0; i < decoded.size(); ++i)
    {
        EXPECT_EQ(decoded[i], original[i * 4]) << "at decimated sample " << i;
    }
}

// The reader is the piece that has to agree with years of existing captures. Packed 10-bit
// data must read back as the same values the FLAC path produces, or the analysis reports
// different verdicts for the same capture depending on which format it was saved in.
TEST(CaptureReaderFormats, PackedTenBitReadsBackIdentically)
{
    const std::vector<uint16_t> original = makeSampleData(4000);

    ScopedTempFile file(".lds");
    {
        std::ofstream stream(file.Path(), std::ios::binary);
        for (size_t group = 0; group < original.size() / SampleCodec::samplesPerGroup; ++group)
        {
            int16_t sixteenBit[SampleCodec::samplesPerGroup];
            for (int i = 0; i < SampleCodec::samplesPerGroup; ++i)
            {
                sixteenBit[i] = SampleCodec::toSixteenBit(original[(group * SampleCodec::samplesPerGroup) + i]);
            }
            uint8_t packed[SampleCodec::bytesPerGroup];
            SampleCodec::packGroup(sixteenBit, packed);
            stream.write(reinterpret_cast<const char *>(packed), SampleCodec::bytesPerGroup);
        }
    }

    CaptureReader reader;
    std::string error;
    ASSERT_TRUE(reader.Open(file.Path(), CaptureReader::Format::Packed10Bit, error)) << error;
    const std::vector<uint16_t> decoded = readAll(reader);

    ASSERT_EQ(decoded.size(), original.size());
    EXPECT_EQ(decoded, original);
}

// The uncompressed fallback format, through the same reader
TEST(CaptureReaderFormats, SignedSixteenBitReadsBackIdentically)
{
    const std::vector<uint16_t> original = makeSampleData(4000);

    ScopedTempFile file(".raw");
    {
        std::ofstream stream(file.Path(), std::ios::binary);
        for (const uint16_t sample : original)
        {
            const int16_t value = SampleCodec::toSixteenBit(sample);
            const uint8_t bytes[2] = { static_cast<uint8_t>(static_cast<uint16_t>(value) & 0x00FF),
                                       static_cast<uint8_t>((static_cast<uint16_t>(value) >> 8) & 0x00FF) };
            stream.write(reinterpret_cast<const char *>(bytes), 2);
        }
    }

    CaptureReader reader;
    std::string error;
    ASSERT_TRUE(reader.Open(file.Path(), CaptureReader::Format::Signed16Bit, error)) << error;
    const std::vector<uint16_t> decoded = readAll(reader);

    ASSERT_EQ(decoded.size(), original.size());
    EXPECT_EQ(decoded, original);
}

// Extension detection decides how a file is interpreted, and getting it wrong would report
// perfectly good data as corrupt. An unknown extension must say so rather than guess.
TEST(CaptureReaderFormats, FormatIsDetectedFromExtension)
{
    EXPECT_EQ(CaptureReader::FormatFromExtension("capture.ldf"), CaptureReader::Format::FlacOgg);
    EXPECT_EQ(CaptureReader::FormatFromExtension("capture.lds"), CaptureReader::Format::Packed10Bit);
    EXPECT_EQ(CaptureReader::FormatFromExtension("capture.raw"), CaptureReader::Format::Signed16Bit);
    EXPECT_EQ(CaptureReader::FormatFromExtension("capture.LDF"), CaptureReader::Format::FlacOgg);
    EXPECT_FALSE(CaptureReader::FormatFromExtension("capture.wav").has_value());
    EXPECT_FALSE(CaptureReader::FormatFromExtension("capture").has_value());
}

// Provenance tags have to survive into the file, since the point of them is being readable
// after the capture has been separated from everything else it was written with (P7-25).
TEST(FlacRoundTrip, ProvenanceTagsAreWritten)
{
    const std::vector<uint16_t> original = makeSampleData(8192);
    const std::vector<uint8_t> deviceBuffer = toDeviceBuffer(original);

    ScopedTempFile file(".ldf");
    {
        FlacWriter writer;
        FlacWriter::Options options;
        options.tags = { { "DDD_VERSION", "deadbeef" }, { "DDD_DECIMATION", "1" } };
        std::string error;
        ASSERT_TRUE(writer.Open(file.Path(), options, error)) << error;
        ASSERT_TRUE(writer.WriteRawDeviceSamples(deviceBuffer.data(), original.size()));
        ASSERT_TRUE(writer.Finish());
    }

    // Read the head of the file and look for the comment text. Crude, but it checks the
    // bytes reached the disk rather than checking that the library was called.
    std::ifstream stream(file.Path(), std::ios::binary);
    std::string head(8192, '\0');
    stream.read(head.data(), static_cast<std::streamsize>(head.size()));
    head.resize(static_cast<size_t>(stream.gcount()));

    EXPECT_NE(head.find("DDD_VERSION=deadbeef"), std::string::npos);
    EXPECT_NE(head.find("DDD_DECIMATION=1"), std::string::npos);
}
