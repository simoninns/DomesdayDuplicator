/************************************************************************

    test_analysis_cli.cpp

    T1 tests for --analyse-test-data's exit codes
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QString>
#include <QTextStream>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "analysis_cli.h"
#include "sample_format.h"
#include "synthetic_source.h"

namespace ddd::gui {
namespace {

// The exit code is the whole interface. A script driving the T5 gate reads
// nothing else, so these are the assertions that matter — the message beside
// them is for a person, and the code is for the script.

void WriteRamp(const std::filesystem::path& path, size_t count,
               size_t break_at = 0) {
  std::vector<uint16_t> values;
  values.reserve(count);

  uint16_t value = 0;
  for (size_t index = 0; index < count; ++index) {
    values.push_back(value);
    value = static_cast<uint16_t>((value + 1) %
                                  capture::SyntheticSource::kRampLength);
  }
  if (break_at > 0 && break_at < values.size()) {
    values[break_at] = static_cast<uint16_t>((values[break_at] + 7) %
                                             capture::kMaximumSampleValue);
  }

  std::ofstream file(path, std::ios::binary);
  for (const uint16_t sample : values) {
    const int16_t signed_value =
        capture::ToSigned16Bit(static_cast<int32_t>(sample));
    file.put(static_cast<char>(static_cast<uint16_t>(signed_value) & 0xFF));
    file.put(
        static_cast<char>((static_cast<uint16_t>(signed_value) >> 8) & 0xFF));
  }
}

constexpr size_t kLongEnoughToWrap =
    size_t{capture::SyntheticSource::kRampLength} * 5;

class AnalysisCliTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    directory_ = std::filesystem::temp_directory_path() /
                 (std::string("ddd-analysis-cli-") + info->name());
    std::filesystem::remove_all(directory_);
    std::filesystem::create_directories(directory_);
  }

  void TearDown() override { std::filesystem::remove_all(directory_); }

  QString FileWith(size_t samples, size_t break_at = 0) {
    const std::filesystem::path path = directory_ / "capture.raw";
    WriteRamp(path, samples, break_at);
    return QString::fromStdString(path.string());
  }

  struct Run {
    int code = -1;
    QString out;
    QString error;
  };

  static Run Analyse(const QString& path) {
    Run run;
    QTextStream out(&run.out);
    QTextStream error(&run.error);
    run.code = RunTestDataAnalysis(path, out, error);
    out.flush();
    error.flush();
    return run;
  }

  std::filesystem::path directory_;
};

TEST_F(AnalysisCliTest, AnIntactRampExitsZero) {
  const Run run = Analyse(FileWith(kLongEnoughToWrap));

  EXPECT_EQ(run.code, 0) << run.out.toStdString();
  EXPECT_TRUE(run.out.contains(QStringLiteral("PASSED")))
      << run.out.toStdString();
  EXPECT_TRUE(run.error.isEmpty()) << run.error.toStdString();
}

TEST_F(AnalysisCliTest, ABreakExitsOne) {
  const Run run = Analyse(FileWith(kLongEnoughToWrap, 5000));

  EXPECT_EQ(run.code, 1) << run.out.toStdString();
  EXPECT_TRUE(run.out.contains(QStringLiteral("FAILED")))
      << run.out.toStdString();

  // A verdict about the capture, even a bad one, is a result and goes to
  // stdout — a script collecting results wants it in the collection.
  EXPECT_TRUE(run.error.isEmpty()) << run.error.toStdString();
}

// Two, not one. "I cannot read this" and "this capture is bad" are different
// answers: the first means fix the command line, the second means look at the
// hardware. A boolean exit code cannot tell them apart, which is why there are
// three.
TEST_F(AnalysisCliTest, AFileThatIsNotThereExitsTwo) {
  const Run run = Analyse(
      QString::fromStdString((directory_ / "no-such-capture.raw").string()));

  EXPECT_EQ(run.code, 2);
  EXPECT_TRUE(run.error.contains(QStringLiteral("Error:")))
      << run.error.toStdString();
}

TEST_F(AnalysisCliTest, AnExtensionThisApplicationDoesNotReadExitsTwo) {
  const std::filesystem::path path = directory_ / "capture.lds";
  {
    std::ofstream file(path, std::ios::binary);
    file << "not a capture";
  }

  const Run run = Analyse(QString::fromStdString(path.string()));

  EXPECT_EQ(run.code, 2);

  // The message names the formats that would have worked, because the most
  // likely cause is somebody pointing this at a legacy file the old
  // application reads and this one does not.
  EXPECT_TRUE(run.error.contains(QStringLiteral(".flac")))
      << run.error.toStdString();
}

// A pass over 900 samples proves much less than a pass over a disc side, and it
// still exits 0 — but the message has to say so, or the verdict gets quoted as
// though it were the strong one.
TEST_F(AnalysisCliTest, ACaptureTooShortToWrapPassesWithACaveat) {
  const Run run = Analyse(FileWith(500));

  EXPECT_EQ(run.code, 0);
  EXPECT_TRUE(run.out.contains(QStringLiteral("weak evidence")))
      << run.out.toStdString();
}

TEST_F(AnalysisCliTest, TheFileBeingAnalysedIsNamedBeforeTheWorkStarts) {
  // Printed first, so a run that takes minutes on a large capture says what it
  // is doing rather than sitting silent.
  const QString path = FileWith(kLongEnoughToWrap);
  const Run run = Analyse(path);

  EXPECT_TRUE(run.out.startsWith(QStringLiteral("Analysing ")))
      << run.out.toStdString();
  EXPECT_TRUE(run.out.contains(path)) << run.out.toStdString();
}

}  // namespace
}  // namespace ddd::gui
