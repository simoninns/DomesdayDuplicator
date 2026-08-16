/************************************************************************

    test_flac_round_trip.cpp

    T1/T2 tests for the FLAC writer and the capture reader, against each other
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "capture_format.h"
#include "capture_reader.h"
#include "flac_writer.h"
#include "raw_sink.h"
#include "sample_format.h"

namespace ddd::capture {
namespace {

// A file that removes itself, so a failing test does not leave litter behind
// and a passing one does not depend on the order tests ran in.
class TemporaryFile {
 public:
  explicit TemporaryFile(const std::string& suffix) {
    const ::testing::TestInfo* info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    path_ = std::filesystem::temp_directory_path() /
            (std::string("ddd-gui-") +
             (info != nullptr ? info->name() : "unknown") + suffix);
    std::filesystem::remove(path_);
  }

  ~TemporaryFile() {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
  }

  TemporaryFile(const TemporaryFile&) = delete;
  TemporaryFile& operator=(const TemporaryFile&) = delete;

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

// Wire-layout bytes for a run of 10-bit sample values.
std::vector<uint8_t> ToWireBytes(const std::vector<uint16_t>& values) {
  std::vector<uint8_t> bytes;
  bytes.reserve(values.size() * kBytesPerSample);
  for (uint16_t value : values) {
    bytes.push_back(static_cast<uint8_t>(value & 0xFF));
    bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  }
  return bytes;
}

// Something with structure a compressor can work on, and enough range to catch
// a scaling error at both extremes.
std::vector<uint16_t> SampleValues(size_t count) {
  std::vector<uint16_t> values;
  values.reserve(count);

  // The constant seed is the point of a golden test and not an oversight: the
  // same samples have to come out of every run on every machine, or a failure
  // means "the data was different" rather than "the codec was". clang-tidy
  // flags a predictable sequence because predictability is usually a security
  // fault; here it is the property under test. Both the cert- and bugprone-
  // names are listed because which one exists depends on the clang-tidy
  // release, and a suppression naming only one is a suppression that lapses.
  //
  // NOLINTNEXTLINE(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed)
  std::mt19937 generator(1234);
  std::uniform_int_distribution<int> noise(-8, 8);

  for (size_t index = 0; index < count; ++index) {
    if (index % 997 == 0) {
      values.push_back(kMinimumSampleValue);
    } else if (index % 991 == 0) {
      values.push_back(kMaximumSampleValue);
    } else {
      const int base =
          512 + static_cast<int>(300 * ((index % 200) < 100 ? 1 : -1));
      values.push_back(
          static_cast<uint16_t>(std::clamp(base + noise(generator), 0, 1023)));
    }
  }
  return values;
}

std::vector<uint16_t> ReadEverything(CaptureReader& reader) {
  std::vector<uint16_t> all;
  std::vector<uint16_t> chunk;
  bool end_of_file = false;

  while (!end_of_file) {
    if (!reader.Read(chunk, 4096, end_of_file)) {
      ADD_FAILURE() << "read failed: " << reader.LastError();
      break;
    }
    all.insert(all.end(), chunk.begin(), chunk.end());
  }
  return all;
}

TEST(FlacRoundTripTest, EverySampleSurvivesTheEncodeAndDecode) {
  // The property everything else rests on. A capture is an archival artefact;
  // if the compression were not lossless the whole format choice would be
  // wrong, and nothing downstream would notice.
  const std::vector<uint16_t> values = SampleValues(50'000);
  const std::vector<uint8_t> wire = ToWireBytes(values);

  TemporaryFile file(".ddd.flac");

  {
    FlacWriter writer;
    FlacWriter::Options options;
    options.sample_rate_label = kFlacSampleRateLabel;
    std::string error;
    ASSERT_TRUE(writer.Open(file.path(), options, error)) << error;
    ASSERT_TRUE(writer.WriteRawDeviceSamples(wire.data(), values.size()));
    ASSERT_TRUE(writer.Finish());
    EXPECT_EQ(writer.SamplesWritten(), values.size());
    EXPECT_GT(writer.BytesWritten(), 0U);
  }

  CaptureReader reader;
  std::string error;
  ASSERT_TRUE(reader.Open(file.path(), CaptureReader::Format::kFlac, error))
      << error;

  EXPECT_EQ(ReadEverything(reader), values);
}

TEST(FlacRoundTripTest, TheFileIsANativeFlacRatherThanAnOggOne) {
  // The one byte-level fact that distinguishes what this writes from the .ldf
  // the old application writes: a native stream starts "fLaC", an Ogg-wrapped
  // one starts "OggS". Tenacity and Audacity open the first and refuse the
  // second, which is the whole reason for the change.
  TemporaryFile file(".ddd.flac");

  const std::vector<uint16_t> values = SampleValues(4096);
  const std::vector<uint8_t> wire = ToWireBytes(values);

  FlacWriter writer;
  std::string error;
  ASSERT_TRUE(writer.Open(file.path(), FlacWriter::Options{}, error)) << error;
  ASSERT_TRUE(writer.WriteRawDeviceSamples(wire.data(), values.size()));
  ASSERT_TRUE(writer.Finish());

  std::ifstream input(file.path(), std::ios::binary);
  ASSERT_TRUE(input.is_open());
  char magic[4] = {};
  input.read(magic, sizeof(magic));

  EXPECT_EQ(std::string(magic, sizeof(magic)), "fLaC");
}

TEST(FlacRoundTripTest, TheSampleRateLabelIsTheOneLdDecodeExpects) {
  // Not a measurement: FLAC's field cannot hold 40 MHz. The value has to match
  // lddecode/compress.py's SAMPLE_RATE, or ld-decode reads the file at the
  // wrong speed.
  TemporaryFile file(".ddd.flac");

  const std::vector<uint16_t> values = SampleValues(4096);
  const std::vector<uint8_t> wire = ToWireBytes(values);

  FlacWriter writer;
  FlacWriter::Options options;
  options.sample_rate_label = kFlacSampleRateLabel;
  std::string error;
  ASSERT_TRUE(writer.Open(file.path(), options, error)) << error;
  ASSERT_TRUE(writer.WriteRawDeviceSamples(wire.data(), values.size()));
  ASSERT_TRUE(writer.Finish());

  // STREAMINFO lives at a fixed offset in a native FLAC stream: four magic
  // bytes, then a four-byte block header, then the block. The sample rate is 20
  // bits starting ten bytes into it.
  std::ifstream input(file.path(), std::ios::binary);
  ASSERT_TRUE(input.is_open());
  std::vector<uint8_t> header(30);
  input.read(reinterpret_cast<char*>(header.data()),
             static_cast<std::streamsize>(header.size()));

  const uint32_t sample_rate = (static_cast<uint32_t>(header[18]) << 12) |
                               (static_cast<uint32_t>(header[19]) << 4) |
                               (static_cast<uint32_t>(header[20]) >> 4);
  EXPECT_EQ(sample_rate, kFlacSampleRateLabel);

  const uint32_t channels =
      static_cast<uint32_t>(((header[20] >> 1) & 0x07) + 1);
  EXPECT_EQ(channels, kFlacChannels);
}

TEST(FlacRoundTripTest, ProvenanceTagsSurviveIntoTheFile) {
  // A capture that has been separated from its metadata sidecar must still be
  // able to say which build produced it.
  TemporaryFile file(".ddd.flac");

  const std::vector<uint16_t> values = SampleValues(4096);
  const std::vector<uint8_t> wire = ToWireBytes(values);

  FlacWriter::Options options;
  options.tags = {{"DDD_VERSION", "abcd1234"},
                  {"DESCRIPTION", "Domesday Duplicator capture"}};

  FlacWriter writer;
  std::string error;
  ASSERT_TRUE(writer.Open(file.path(), options, error)) << error;
  ASSERT_TRUE(writer.WriteRawDeviceSamples(wire.data(), values.size()));
  ASSERT_TRUE(writer.Finish());

  CaptureReader reader;
  ASSERT_TRUE(reader.Open(file.path(), CaptureReader::Format::kFlac, error))
      << error;

  bool found_version = false;
  for (const auto& [name, value] : reader.Tags()) {
    if (name == "DDD_VERSION") {
      found_version = true;
      EXPECT_EQ(value, "abcd1234");
    }
  }
  EXPECT_TRUE(found_version) << "the build stamp did not reach the file";
}

TEST(FlacRoundTripTest, TheDecodedLengthIsKnownBeforeReadingTheSamples) {
  TemporaryFile file(".ddd.flac");

  const std::vector<uint16_t> values = SampleValues(20'000);
  const std::vector<uint8_t> wire = ToWireBytes(values);

  FlacWriter writer;
  std::string error;
  ASSERT_TRUE(writer.Open(file.path(), FlacWriter::Options{}, error)) << error;
  ASSERT_TRUE(writer.WriteRawDeviceSamples(wire.data(), values.size()));
  ASSERT_TRUE(writer.Finish());

  CaptureReader reader;
  ASSERT_TRUE(reader.Open(file.path(), CaptureReader::Format::kFlac, error))
      << error;

  EXPECT_EQ(reader.TotalSamples(), std::optional<uint64_t>(values.size()));
}

TEST(CaptureReaderTest, TheUncompressedFormatReadsBackTheSameValues) {
  // Retained because it costs four lines and gives the test-pattern analyser a
  // format in common with the old application, so the two can be checked
  // against each other on the same file.
  TemporaryFile file(".raw");

  const std::vector<uint16_t> values = SampleValues(5000);
  {
    std::ofstream output(file.path(), std::ios::binary);
    ASSERT_TRUE(output.is_open());
    for (uint16_t value : values) {
      const int16_t scaled = ToSigned16Bit(value);
      const uint16_t bits = static_cast<uint16_t>(scaled);
      const char bytes[2] = {static_cast<char>(bits & 0xFF),
                             static_cast<char>((bits >> 8) & 0xFF)};
      output.write(bytes, sizeof(bytes));
    }
  }

  CaptureReader reader;
  std::string error;
  ASSERT_TRUE(
      reader.Open(file.path(), CaptureReader::Format::kSigned16Bit, error))
      << error;

  EXPECT_EQ(reader.TotalSamples(), std::optional<uint64_t>(values.size()));
  EXPECT_EQ(ReadEverything(reader), values);
}

TEST(CaptureReaderTest, TheFormatIsGuessedFromTheExtension) {
  EXPECT_EQ(CaptureReader::FormatFromExtension("capture.ddd.flac"),
            CaptureReader::Format::kFlac);
  EXPECT_EQ(CaptureReader::FormatFromExtension("capture.FLAC"),
            CaptureReader::Format::kFlac);
  EXPECT_EQ(CaptureReader::FormatFromExtension("capture.ddd.s16"),
            CaptureReader::Format::kSigned16Bit);
  EXPECT_EQ(CaptureReader::FormatFromExtension("capture.S16"),
            CaptureReader::Format::kSigned16Bit);

  // The old application's spelling of the same layout, kept readable so the
  // test-pattern analyser works on both.
  EXPECT_EQ(CaptureReader::FormatFromExtension("capture.raw"),
            CaptureReader::Format::kSigned16Bit);
}

// --- The uncompressed sink ------------------------------------------------

// The same samples a FLAC capture holds, with nothing wrapped round them — so
// what this writes has to read back through the same reader, value for value.
TEST(RawSinkTest, WhatItWritesIsWhatTheReaderReadsBack) {
  TemporaryFile file(".ddd.s16");

  const std::vector<uint16_t> values = SampleValues(5000);
  const std::vector<uint8_t> wire = ToWireBytes(values);

  {
    RawSink sink;
    ASSERT_TRUE(sink.Open(file.path(), RawSink::Options{})) << sink.LastError();
    ASSERT_TRUE(sink.Write(wire.data(), values.size())) << sink.LastError();
    ASSERT_TRUE(sink.Finish()) << sink.LastError();

    EXPECT_EQ(sink.SamplesWritten(), values.size());
    EXPECT_EQ(sink.BytesWritten(), values.size() * kBytesPerSample);
  }

  CaptureReader reader;
  std::string error;
  ASSERT_TRUE(
      reader.Open(file.path(), CaptureReader::Format::kSigned16Bit, error))
      << error;

  EXPECT_EQ(ReadEverything(reader), values);
}

// A factor the file cannot describe is refused rather than silently treated as
// one: a file whose rate disagrees with its contents decodes at the wrong speed
// with nothing to reveal it.
TEST(RawSinkTest, AnUnsupportedDecimationFactorIsRefused) {
  TemporaryFile file(".ddd.s16");

  RawSink sink;
  RawSink::Options options;
  options.decimation_factor = 3;

  EXPECT_FALSE(sink.Open(file.path(), options));
  EXPECT_FALSE(sink.LastError().empty());
}

// --- Decimation -----------------------------------------------------------

// Every second sample, starting with the first. Plain selection with no filter,
// which is what gui/ does for its 4:1 CD decimation.
TEST(DecimationTest, TwoToOneKeepsEverySecondSample) {
  TemporaryFile file(".ddd.s16");

  const std::vector<uint16_t> values = SampleValues(4096);
  const std::vector<uint8_t> wire = ToWireBytes(values);

  std::vector<uint16_t> expected;
  for (size_t i = 0; i < values.size(); i += kTapeDecimationFactor) {
    expected.push_back(values[i]);
  }

  {
    RawSink sink;
    RawSink::Options options;
    options.decimation_factor = kTapeDecimationFactor;
    ASSERT_TRUE(sink.Open(file.path(), options)) << sink.LastError();
    ASSERT_TRUE(sink.Write(wire.data(), values.size())) << sink.LastError();
    ASSERT_TRUE(sink.Finish()) << sink.LastError();
    EXPECT_EQ(sink.SamplesWritten(), expected.size());
  }

  CaptureReader reader;
  std::string error;
  ASSERT_TRUE(
      reader.Open(file.path(), CaptureReader::Format::kSigned16Bit, error))
      << error;

  EXPECT_EQ(ReadEverything(reader), expected);
}

// The seam. A capture arrives as a run of buffers, and a buffer holding an odd
// number of samples shifts the phase — so a writer that reset it on every call
// would keep a sample twice, or lose one, at every boundary. Written as three
// odd-length buffers, which is the shape that catches it.
TEST(DecimationTest, ThePhaseCarriesAcrossBufferBoundaries) {
  TemporaryFile flac_file(".ddd.flac");
  TemporaryFile raw_file(".ddd.s16");

  const std::vector<uint16_t> values = SampleValues(3 * 1001);
  const std::vector<uint8_t> wire = ToWireBytes(values);

  std::vector<uint16_t> expected;
  for (size_t i = 0; i < values.size(); i += kTapeDecimationFactor) {
    expected.push_back(values[i]);
  }

  constexpr size_t kBuffer = 1001;

  {
    FlacWriter writer;
    FlacWriter::Options options;
    options.compression_level = 0;
    options.sample_rate_label = FlacSampleRateLabelFor(kTapeDecimationFactor);
    std::string error;
    ASSERT_TRUE(writer.Open(flac_file.path(), options, error)) << error;

    for (size_t offset = 0; offset < values.size(); offset += kBuffer) {
      ASSERT_TRUE(
          writer.WriteRawDeviceSamples(wire.data() + (offset * kBytesPerSample),
                                       kBuffer, kTapeDecimationFactor))
          << writer.LastError();
    }
    ASSERT_TRUE(writer.Finish());
  }

  {
    RawSink sink;
    RawSink::Options options;
    options.decimation_factor = kTapeDecimationFactor;
    ASSERT_TRUE(sink.Open(raw_file.path(), options)) << sink.LastError();
    for (size_t offset = 0; offset < values.size(); offset += kBuffer) {
      ASSERT_TRUE(sink.Write(wire.data() + (offset * kBytesPerSample), kBuffer))
          << sink.LastError();
    }
    ASSERT_TRUE(sink.Finish()) << sink.LastError();
  }

  CaptureReader flac_reader;
  std::string error;
  ASSERT_TRUE(
      flac_reader.Open(flac_file.path(), CaptureReader::Format::kFlac, error))
      << error;
  EXPECT_EQ(ReadEverything(flac_reader), expected);

  CaptureReader raw_reader;
  ASSERT_TRUE(raw_reader.Open(raw_file.path(),
                              CaptureReader::Format::kSigned16Bit, error))
      << error;
  EXPECT_EQ(ReadEverything(raw_reader), expected);
}

// A 2:1 capture is a real 20 Msps stream and its header says so, on the same
// convention as the undecimated label: a reader that took the file at 40 would
// have it an octave out.
TEST(DecimationTest, TheRateLabelHalvesWithTheRate) {
  EXPECT_EQ(FlacSampleRateLabelFor(kUndecimatedFactor), kFlacSampleRateLabel);
  EXPECT_EQ(FlacSampleRateLabelFor(kTapeDecimationFactor),
            kFlacSampleRateLabel / 2);

  TemporaryFile file(".ddd.flac");
  const std::vector<uint16_t> values = SampleValues(4096);
  const std::vector<uint8_t> wire = ToWireBytes(values);

  FlacWriter writer;
  FlacWriter::Options options;
  options.sample_rate_label = FlacSampleRateLabelFor(kTapeDecimationFactor);
  std::string error;
  ASSERT_TRUE(writer.Open(file.path(), options, error)) << error;
  ASSERT_TRUE(writer.WriteRawDeviceSamples(wire.data(), values.size(),
                                           kTapeDecimationFactor));
  ASSERT_TRUE(writer.Finish());

  // STREAMINFO's sample rate lives in the 20 bits starting 18 bytes past the
  // "fLaC" marker and the metadata block header — read here rather than through
  // the decoder, because the point is what is in the file.
  std::ifstream input(file.path(), std::ios::binary);
  ASSERT_TRUE(input.is_open());
  std::vector<char> header(30);
  input.read(header.data(), static_cast<std::streamsize>(header.size()));

  const auto byte = [&header](size_t index) {
    return static_cast<uint32_t>(static_cast<uint8_t>(header[index]));
  };
  const uint32_t rate = (byte(18) << 12) | (byte(19) << 4) | (byte(20) >> 4);

  EXPECT_EQ(rate, kFlacSampleRateLabel / 2);
}

TEST(DecimationTest, AnUnsupportedFactorIsRefusedByTheWriter) {
  TemporaryFile file(".ddd.flac");

  const std::vector<uint16_t> values = SampleValues(64);
  const std::vector<uint8_t> wire = ToWireBytes(values);

  FlacWriter writer;
  std::string error;
  ASSERT_TRUE(writer.Open(file.path(), FlacWriter::Options{}, error)) << error;

  EXPECT_FALSE(writer.WriteRawDeviceSamples(wire.data(), values.size(), 3));
  EXPECT_FALSE(writer.LastError().empty());
}

TEST(CaptureReaderTest, AnUnsupportedExtensionIsDeclinedRatherThanGuessedAt) {
  // Guessing wrong here would produce nonsense that reads as data corruption,
  // which is a much worse answer than "this application does not read that".
  EXPECT_FALSE(CaptureReader::FormatFromExtension("capture.ldf").has_value());
  EXPECT_FALSE(CaptureReader::FormatFromExtension("capture.lds").has_value());
  EXPECT_FALSE(CaptureReader::FormatFromExtension("capture").has_value());
}

TEST(CaptureReaderTest, AFileThatIsNotAFlacStreamIsRefusedWithAReason) {
  TemporaryFile file(".ddd.flac");
  {
    std::ofstream output(file.path(), std::ios::binary);
    ASSERT_TRUE(output.is_open());
    output << "this is not a FLAC stream, whatever its name says";
  }

  CaptureReader reader;
  std::string error;
  EXPECT_FALSE(reader.Open(file.path(), CaptureReader::Format::kFlac, error));
  EXPECT_FALSE(error.empty());
}

}  // namespace
}  // namespace ddd::capture
