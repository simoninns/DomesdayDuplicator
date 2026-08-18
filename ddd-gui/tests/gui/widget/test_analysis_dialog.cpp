/************************************************************************

    test_analysis_dialog.cpp

    T1 tests for the test-data analysis dialog
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QApplication>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "analysis_dialog.h"
#include "sample_format.h"
#include "synthetic_source.h"

namespace ddd::gui {
namespace {

using namespace std::chrono_literals;

template <typename Predicate>
bool PumpUntil(Predicate predicate, std::chrono::milliseconds limit = 10000ms) {
  const auto deadline = std::chrono::steady_clock::now() + limit;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    QApplication::processEvents();
    std::this_thread::sleep_for(1ms);
  }
  return predicate();
}

// The gateware's ramp, written as the uncompressed .s16 this application
// writes. break_at corrupts one sample, which is what a lost sample looks
// like coming back off the disk.
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

class AnalysisDialogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    directory_ = std::filesystem::temp_directory_path() /
                 (std::string("ddd-analysis-dialog-") + info->name());
    std::filesystem::remove_all(directory_);
    std::filesystem::create_directories(directory_);
  }

  void TearDown() override { std::filesystem::remove_all(directory_); }

  QString FileWith(size_t samples, size_t break_at = 0) {
    const std::filesystem::path path = directory_ / "capture.s16";
    WriteRamp(path, samples, break_at);
    return QString::fromStdString(path.string());
  }

  static QLabel* Result(AnalysisDialog& dialog) {
    return dialog.findChild<QLabel*>(
        QLatin1String(AnalysisDialog::kResultLabelName));
  }
  static QProgressBar* Progress(AnalysisDialog& dialog) {
    return dialog.findChild<QProgressBar*>(
        QLatin1String(AnalysisDialog::kProgressBarName));
  }
  static QPushButton* Cancel(AnalysisDialog& dialog) {
    return dialog.findChild<QPushButton*>(
        QLatin1String(AnalysisDialog::kCancelButtonName));
  }

  std::filesystem::path directory_;
};

// Long enough for the ramp to wrap several times, so a pass means something.
constexpr size_t kLongEnoughToWrap =
    size_t{capture::SyntheticSource::kRampLength} * 5;

TEST_F(AnalysisDialogTest, EveryControlIsPresentAndFindable) {
  AnalysisDialog dialog;
  EXPECT_NE(Result(dialog), nullptr);
  EXPECT_NE(Progress(dialog), nullptr);
  EXPECT_NE(Cancel(dialog), nullptr);
}

TEST_F(AnalysisDialogTest, AnIntactRampReportsAPass) {
  AnalysisDialog dialog;
  dialog.Analyse(FileWith(kLongEnoughToWrap));

  ASSERT_TRUE(PumpUntil([&] {
    return dialog.outcome() == capture::TestDataAnalysis::Outcome::kPassed;
  }));

  EXPECT_TRUE(Result(dialog)->text().contains(QStringLiteral("PASSED")))
      << Result(dialog)->text().toStdString();
}

TEST_F(AnalysisDialogTest, ABrokenRampReportsAFailure) {
  AnalysisDialog dialog;
  dialog.Analyse(FileWith(kLongEnoughToWrap, 5000));

  ASSERT_TRUE(PumpUntil([&] {
    return dialog.outcome() == capture::TestDataAnalysis::Outcome::kFailed;
  }));

  EXPECT_TRUE(Result(dialog)->text().contains(QStringLiteral("FAILED")))
      << Result(dialog)->text().toStdString();

  // The sample offset is what a bench session acts on, so it has to be in the
  // message rather than only in the return value.
  EXPECT_TRUE(Result(dialog)->text().contains(QStringLiteral("5,000")))
      << Result(dialog)->text().toStdString();
}

// The verdict is read across a bench, so it is coloured. Through the theme
// tokens rather than a literal green, and the check here is that a colour was
// applied at all and that pass and fail do not get the same one.
TEST_F(AnalysisDialogTest, PassAndFailAreColouredDifferently) {
  AnalysisDialog passing;
  passing.Analyse(FileWith(kLongEnoughToWrap));
  ASSERT_TRUE(PumpUntil([&] {
    return passing.outcome() == capture::TestDataAnalysis::Outcome::kPassed;
  }));
  const QString pass_style = Result(passing)->styleSheet();

  AnalysisDialog failing;
  failing.Analyse(FileWith(kLongEnoughToWrap, 5000));
  ASSERT_TRUE(PumpUntil([&] {
    return failing.outcome() == capture::TestDataAnalysis::Outcome::kFailed;
  }));
  const QString fail_style = Result(failing)->styleSheet();

  EXPECT_FALSE(pass_style.isEmpty());
  EXPECT_FALSE(fail_style.isEmpty());
  EXPECT_NE(pass_style, fail_style);
}

TEST_F(AnalysisDialogTest, AFileThatCannotBeReadSaysSoRatherThanFailing) {
  AnalysisDialog dialog;
  dialog.Analyse(
      QString::fromStdString((directory_ / "no-such-capture.s16").string()));

  ASSERT_TRUE(PumpUntil([&] {
    return dialog.outcome() == capture::TestDataAnalysis::Outcome::kUnreadable;
  }));

  // Not "FAILED". "I cannot read this" and "this capture is bad" are different
  // answers, and the second would send somebody looking at their hardware.
  EXPECT_FALSE(Result(dialog)->text().contains(QStringLiteral("FAILED")))
      << Result(dialog)->text().toStdString();
}

// The button is the cancel while it is running and the close afterwards, so
// there is only ever one thing to press and it always does the obvious thing.
TEST_F(AnalysisDialogTest, TheCancelButtonBecomesTheCloseButton) {
  AnalysisDialog dialog;
  EXPECT_TRUE(Cancel(dialog)->text().contains(QStringLiteral("Cancel")));

  dialog.Analyse(FileWith(kLongEnoughToWrap));
  ASSERT_TRUE(PumpUntil([&] {
    return dialog.outcome() == capture::TestDataAnalysis::Outcome::kPassed;
  }));

  EXPECT_TRUE(Cancel(dialog)->text().contains(QStringLiteral("Close")))
      << Cancel(dialog)->text().toStdString();
}

// A dialog destroyed while its worker is still reading has to join it first —
// the worker holds a pointer back through its connections, and a thread still
// running into a destroyed object is a crash that only happens on the machines
// where the file is big.
TEST_F(AnalysisDialogTest, ClosingDuringAnAnalysisDoesNotLeaveAThreadRunning) {
  auto dialog = std::make_unique<AnalysisDialog>();
  dialog->Analyse(FileWith(kLongEnoughToWrap * 200));

  // Destroyed without waiting for the verdict. If the destructor did not cancel
  // and join, this is where it would go wrong.
  dialog.reset();
  SUCCEED();
}

}  // namespace
}  // namespace ddd::gui
