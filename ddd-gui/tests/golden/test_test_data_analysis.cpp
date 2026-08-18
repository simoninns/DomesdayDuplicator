/************************************************************************

    test_test_data_analysis.cpp

    T1/T2 tests for checking a finished test-mode capture
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "capture_format.h"
#include "flac_writer.h"
#include "sample_format.h"
#include "synthetic_source.h"
#include "test_data_analysis.h"

namespace ddd::capture {
namespace {

// A file that removes itself, so a failing test leaves no litter and a passing
// one does not depend on the order the tests ran in.
class TemporaryFile {
 public:
  explicit TemporaryFile(const std::string& suffix) {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    path_ = std::filesystem::temp_directory_path() /
            (std::string("ddd-analysis-") +
             (info != nullptr ? info->name() : "unknown") + suffix);
    std::filesystem::remove(path_);
  }

  ~TemporaryFile() {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
  }

  TemporaryFile(const TemporaryFile&) = delete;
  TemporaryFile& operator=(const TemporaryFile&) = delete;
  TemporaryFile(TemporaryFile&&) = delete;
  TemporaryFile& operator=(TemporaryFile&&) = delete;

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

// The gateware's ramp, as the device produces it: 0, 1, 2, ... wrapping at the
// sequence length. break_at is the sample index whose value is corrupted, which
// is what a lost or mangled sample looks like on the way back off the disk.
std::vector<uint16_t> Ramp(size_t count, size_t break_at = 0) {
  std::vector<uint16_t> values;
  values.reserve(count);

  uint16_t value = 0;
  for (size_t index = 0; index < count; ++index) {
    values.push_back(value);
    value = static_cast<uint16_t>((value + 1) % SyntheticSource::kRampLength);
  }

  if (break_at > 0 && break_at < values.size()) {
    values[break_at] =
        static_cast<uint16_t>((values[break_at] + 7) % kMaximumSampleValue);
  }
  return values;
}

// Signed 16-bit, the uncompressed format this application writes.
void WriteSigned16Bit(const std::filesystem::path& path,
                      const std::vector<uint16_t>& values) {
  std::ofstream file(path, std::ios::binary);
  for (const uint16_t value : values) {
    const int16_t sample = ToSigned16Bit(static_cast<int32_t>(value));
    const auto low = static_cast<uint8_t>(static_cast<uint16_t>(sample) & 0xFF);
    const auto high =
        static_cast<uint8_t>((static_cast<uint16_t>(sample) >> 8) & 0xFF);
    file.put(static_cast<char>(low));
    file.put(static_cast<char>(high));
  }
}

void WriteFlac(const std::filesystem::path& path,
               const std::vector<uint16_t>& values) {
  std::vector<uint8_t> wire;
  wire.reserve(values.size() * kBytesPerSample);
  for (const uint16_t value : values) {
    wire.push_back(static_cast<uint8_t>(value & 0xFF));
    wire.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  }

  FlacWriter writer;
  FlacWriter::Options options;
  options.compression_level = 0;

  std::string error;
  ASSERT_TRUE(writer.Open(path, options, error)) << error;
  ASSERT_TRUE(writer.WriteRawDeviceSamples(wire.data(), values.size()));
  ASSERT_TRUE(writer.Finish());
}

// Long enough for the ramp to wrap several times, so a pass is worth something.
constexpr size_t kLongEnoughToWrap = size_t{SyntheticSource::kRampLength} * 5;

// clang-tidy cannot see through a gtest assertion, so the optionals are read
// through a fallback that cannot be the right answer. A test that passed
// because the value was absent would read 0 here and fail on the comparison.
uint16_t RecordedLength(const std::optional<uint16_t>& length) {
  return length.value_or(0);
}

uint64_t RecordedTotal(const std::optional<uint64_t>& total) {
  return total.value_or(0);
}

// --- The three verdicts --------------------------------------------------

TEST(TestDataAnalysisTest, AnIntactRampPasses) {
  const TemporaryFile file(".s16");
  WriteSigned16Bit(file.path(), Ramp(kLongEnoughToWrap));

  const TestDataAnalysis analysis = AnalyseTestData(file.path());

  EXPECT_EQ(analysis.outcome, TestDataAnalysis::Outcome::kPassed);
  EXPECT_EQ(analysis.ExitCode(), 0);
  EXPECT_EQ(analysis.samples_checked, kLongEnoughToWrap);

  // The ramp length is discovered rather than assumed: older gateware ramps
  // 0..1023 and newer 0..1020, and a check that hard-coded either would report
  // the other as corrupt.
  ASSERT_TRUE(analysis.sequence_length.has_value());
  EXPECT_EQ(RecordedLength(analysis.sequence_length),
            SyntheticSource::kRampLength);

  EXPECT_NE(analysis.message.find("PASSED"), std::string::npos)
      << analysis.message;
}

TEST(TestDataAnalysisTest, ABrokenRampFailsAndSaysWhere) {
  constexpr size_t kBreakAt = 5000;

  const TemporaryFile file(".s16");
  WriteSigned16Bit(file.path(), Ramp(kLongEnoughToWrap, kBreakAt));

  const TestDataAnalysis analysis = AnalyseTestData(file.path());

  EXPECT_EQ(analysis.outcome, TestDataAnalysis::Outcome::kFailed);
  EXPECT_EQ(analysis.ExitCode(), 1);

  // The offset is what a bench session acts on, so it has to be the offset of
  // the bad sample and not of the chunk it was found in.
  EXPECT_EQ(analysis.samples_checked, kBreakAt);
  EXPECT_NE(analysis.expected_value, analysis.actual_value);
  EXPECT_NE(analysis.message.find("FAILED"), std::string::npos)
      << analysis.message;
}

// A pass over 900 samples proves much less than a pass over a disc, and a
// verdict that did not distinguish the two would be quoted as though it had.
TEST(TestDataAnalysisTest, ACaptureTooShortToWrapPassesButSaysSoWeakly) {
  const TemporaryFile file(".s16");
  WriteSigned16Bit(file.path(), Ramp(500));

  const TestDataAnalysis analysis = AnalyseTestData(file.path());

  EXPECT_EQ(analysis.outcome, TestDataAnalysis::Outcome::kPassed);
  EXPECT_EQ(analysis.ExitCode(), 0);
  EXPECT_FALSE(analysis.sequence_length.has_value());
  EXPECT_NE(analysis.message.find("too short"), std::string::npos)
      << analysis.message;
  EXPECT_NE(analysis.message.find("weak evidence"), std::string::npos)
      << analysis.message;
}

// --- Files that cannot be analysed at all --------------------------------

TEST(TestDataAnalysisTest, AFileThatIsNotThereIsNoVerdict) {
  const TestDataAnalysis analysis = AnalyseTestData(
      std::filesystem::temp_directory_path() / "ddd-no-such-capture.s16");

  EXPECT_EQ(analysis.outcome, TestDataAnalysis::Outcome::kUnreadable);
  EXPECT_EQ(analysis.ExitCode(), 2);
  EXPECT_FALSE(analysis.message.empty());
}

// Exit code 2, not 1. "I cannot read this" and "this capture is bad" are
// different answers, and a script driving the T5 gate has to tell them apart —
// one means fix the command line, the other means the hardware is faulty.
TEST(TestDataAnalysisTest, AnExtensionThisApplicationDoesNotReadIsNoVerdict) {
  const TemporaryFile file(".lds");
  {
    std::ofstream stream(file.path(), std::ios::binary);
    stream << "not a capture";
  }

  const TestDataAnalysis analysis = AnalyseTestData(file.path());

  EXPECT_EQ(analysis.outcome, TestDataAnalysis::Outcome::kUnreadable);
  EXPECT_EQ(analysis.ExitCode(), 2);
  EXPECT_NE(analysis.message.find(".flac"), std::string::npos)
      << analysis.message;
}

// --- Through the real capture format -------------------------------------

TEST(TestDataAnalysisTest, TheSameVerdictComesBackThroughAWrittenFlacFile) {
  const TemporaryFile file(kCaptureFileSuffix);
  ASSERT_NO_FATAL_FAILURE(WriteFlac(file.path(), Ramp(kLongEnoughToWrap)));

  const TestDataAnalysis analysis = AnalyseTestData(file.path());

  EXPECT_EQ(analysis.outcome, TestDataAnalysis::Outcome::kPassed);
  EXPECT_EQ(analysis.samples_checked, kLongEnoughToWrap);
  ASSERT_TRUE(analysis.sequence_length.has_value());
  EXPECT_EQ(RecordedLength(analysis.sequence_length),
            SyntheticSource::kRampLength);
}

// The check that makes this worth doing offline at all. The live verifier sees
// the samples before the encoder does; this sees them after a full write and
// read, so it covers the encoder, the filesystem and the drive.
TEST(TestDataAnalysisTest, ABreakSurvivesTheEncoderAndIsFoundOnTheWayBack) {
  constexpr size_t kBreakAt = 3000;

  const TemporaryFile file(kCaptureFileSuffix);
  ASSERT_NO_FATAL_FAILURE(
      WriteFlac(file.path(), Ramp(kLongEnoughToWrap, kBreakAt)));

  const TestDataAnalysis analysis = AnalyseTestData(file.path());

  EXPECT_EQ(analysis.outcome, TestDataAnalysis::Outcome::kFailed);
  EXPECT_EQ(analysis.samples_checked, kBreakAt);
}

// --- Progress and cancel -------------------------------------------------

TEST(TestDataAnalysisTest, ProgressIsReportedAgainstTheFilesOwnLength) {
  const TemporaryFile file(kCaptureFileSuffix);
  ASSERT_NO_FATAL_FAILURE(WriteFlac(file.path(), Ramp(kLongEnoughToWrap)));

  uint64_t last_checked = 0;
  std::optional<uint64_t> last_total;
  int calls = 0;

  const TestDataAnalysis analysis = AnalyseTestData(
      file.path(), [&](uint64_t checked, std::optional<uint64_t> total) {
        ++calls;
        last_checked = checked;
        last_total = total;
      });

  EXPECT_EQ(analysis.outcome, TestDataAnalysis::Outcome::kPassed);
  EXPECT_GT(calls, 0);
  EXPECT_EQ(last_checked, kLongEnoughToWrap);

  // A finished FLAC has its header patched, so it knows its own length and the
  // dialog can show a percentage rather than a busy indicator.
  ASSERT_TRUE(last_total.has_value());
  EXPECT_EQ(RecordedTotal(last_total), kLongEnoughToWrap);
}

TEST(TestDataAnalysisTest, CancellingIsNotAPass) {
  const TemporaryFile file(".s16");
  WriteSigned16Bit(file.path(), Ramp(kLongEnoughToWrap));

  const TestDataAnalysis analysis =
      AnalyseTestData(file.path(), {}, [] { return true; });

  EXPECT_EQ(analysis.outcome, TestDataAnalysis::Outcome::kCancelled);

  // Code 2 rather than 0. An analysis that was stopped has not established
  // anything, and reporting it as a pass would put a green verdict on a capture
  // nobody finished checking.
  EXPECT_EQ(analysis.ExitCode(), 2);
  EXPECT_FALSE(analysis.passed());
  EXPECT_NE(analysis.message.find("Cancelled"), std::string::npos)
      << analysis.message;
}

// A break already found is a verdict, and stopping afterwards does not undo it.
TEST(TestDataAnalysisTest, ABreakFoundBeforeACancelIsStillReported) {
  const TemporaryFile file(".s16");
  WriteSigned16Bit(file.path(), Ramp(kAnalysisChunkSamples + 1000, 100));

  bool first_chunk_done = false;
  const TestDataAnalysis analysis =
      AnalyseTestData(file.path(), {}, [&] { return first_chunk_done; });

  // The break is inside the first chunk, so it is found before the cancel is
  // ever consulted a second time.
  first_chunk_done = true;

  EXPECT_EQ(analysis.outcome, TestDataAnalysis::Outcome::kFailed);
  EXPECT_EQ(analysis.ExitCode(), 1);
}

}  // namespace
}  // namespace ddd::capture
