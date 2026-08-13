/************************************************************************

    test_capture_failure_presenter.cpp

    T1 tests for turning a failure code into something a user can act on
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QSet>
#include <QString>
#include <vector>

#include "capture_failure_presenter.h"
#include "transfer_result.h"

namespace ddd::gui {
namespace {

// Every value of the enumeration, written out rather than iterated over a
// range. A switch in the presenter gets a compiler warning when a new code is
// added; a list here does not, so this is the thing that has to be kept
// current — and the tests below are what make forgetting it visible.
std::vector<capture::TransferResult> EveryFailure() {
  return {
      capture::TransferResult::kFileCreationError,
      capture::TransferResult::kFileWriteError,
      capture::TransferResult::kBufferOverflow,
      capture::TransferResult::kConnectionFailure,
      capture::TransferResult::kUsbTransferFailure,
      capture::TransferResult::kHostUnderflow,
      capture::TransferResult::kUsbMemoryLimit,
      capture::TransferResult::kSequenceMismatch,
      capture::TransferResult::kVerificationError,
      capture::TransferResult::kSourceStalled,
      capture::TransferResult::kProgramError,
      capture::TransferResult::kForcedAbort,
  };
}

CaptureFailureView Present(capture::TransferResult result) {
  return PresentCaptureFailure(result, QString(), QString());
}

// The whole point of the taxonomy. A user who has just lost forty minutes of a
// disc needs to know whether to free disk space, change a USB cable, raise a
// kernel limit or close the application competing for the CPU — and those are
// four different answers, not one.
TEST(CaptureFailurePresenterTest, NoTwoFailuresShareASummary) {
  QSet<QString> seen;
  for (const capture::TransferResult result : EveryFailure()) {
    const QString summary = Present(result).summary;
    EXPECT_FALSE(seen.contains(summary))
        << "two failures give the same summary: " << summary.toStdString();
    seen.insert(summary);
  }
}

TEST(CaptureFailurePresenterTest, NoTwoFailuresShareARemedy) {
  QSet<QString> seen;
  for (const capture::TransferResult result : EveryFailure()) {
    const QString remedy = Present(result).remedy;
    EXPECT_FALSE(seen.contains(remedy))
        << "two failures give the same remedy: " << remedy.toStdString();
    seen.insert(remedy);
  }
}

// A message with no remedy is the generic message the presenter exists to
// prevent. "The capture failed" tells somebody nothing they did not already
// know from the capture having stopped.
TEST(CaptureFailurePresenterTest, EveryFailureNamesSomethingToDo) {
  for (const capture::TransferResult result : EveryFailure()) {
    const CaptureFailureView view = Present(result);

    EXPECT_FALSE(view.summary.isEmpty()) << capture::TransferResultName(result);
    EXPECT_FALSE(view.remedy.isEmpty()) << capture::TransferResultName(result);

    // Long enough to be an instruction rather than a shrug. Every remedy in the
    // presenter names an action; a short one would mean a branch had been added
    // with a placeholder.
    EXPECT_GT(view.remedy.length(), 40) << capture::TransferResultName(result)
                                        << ": " << view.remedy.toStdString();
  }
}

// The title carries the code so a user has something to search for and a
// maintainer something to act on. A title of "Error" is worth nothing in a bug
// report.
TEST(CaptureFailurePresenterTest, TheTitleNamesTheFailureCode) {
  for (const capture::TransferResult result : EveryFailure()) {
    EXPECT_TRUE(Present(result).title.contains(
        QString::fromUtf8(capture::TransferResultName(result))))
        << Present(result).title.toStdString();
  }
}

// The one remedy that is a command rather than an action. A user hitting this
// is going to copy the line, so it has to be the line — a paraphrase does not
// paste.
TEST(CaptureFailurePresenterTest, TheUsbfsRemedyCarriesTheCommandToRun) {
  const QString remedy =
      Present(capture::TransferResult::kUsbMemoryLimit).remedy;

  EXPECT_TRUE(remedy.contains(
      QStringLiteral("/sys/module/usbcore/parameters/usbfs_memory_mb")))
      << remedy.toStdString();

  // And the way out that does not need administrator rights, because on a
  // machine somebody does not own that is the only one available to them.
  EXPECT_TRUE(
      remedy.contains(QStringLiteral("queue size"), Qt::CaseInsensitive))
      << remedy.toStdString();
}

TEST(CaptureFailurePresenterTest, ThePipelinesOwnAccountIsCarriedThrough) {
  // The detail is what no enumeration can hold — which sample the sequence
  // broke at, what libFLAC said — and it is what turns a report into a
  // diagnosis.
  const CaptureFailureView view =
      PresentCaptureFailure(capture::TransferResult::kSequenceMismatch,
                            QStringLiteral("expected 17, got 19"), QString());

  EXPECT_TRUE(view.summary.contains(QStringLiteral("expected 17, got 19")))
      << view.summary.toStdString();
  EXPECT_TRUE(view.ToMessage().contains(QStringLiteral("expected 17, got 19")));
}

// The first thing anyone wants to know after losing a capture is whether any of
// it survived. The pipeline finalises the sink however the run ended, so it
// did — and saying so is the difference between a lost afternoon and a partial
// recording somebody can still use.
TEST(CaptureFailurePresenterTest, AFailureDuringACaptureSaysWhereTheFileIs) {
  const CaptureFailureView view = PresentCaptureFailure(
      capture::TransferResult::kUsbTransferFailure, QString(),
      QStringLiteral("/captures/RF-Sample_2026-08-13.ddd.flac"));

  EXPECT_FALSE(view.file_note.isEmpty());
  EXPECT_TRUE(view.file_note.contains(
      QStringLiteral("/captures/RF-Sample_2026-08-13.ddd.flac")));
  EXPECT_TRUE(view.file_note.contains(QStringLiteral("readable")));
  EXPECT_TRUE(view.ToMessage().contains(view.file_note));
}

TEST(CaptureFailurePresenterTest, AMonitorFailureMentionsNoFile) {
  const CaptureFailureView view = PresentCaptureFailure(
      capture::TransferResult::kSourceStalled, QString(), QString());

  EXPECT_TRUE(view.file_note.isEmpty());
}

TEST(CaptureFailurePresenterTest, TheMessageIsSummaryThenRemedy) {
  const CaptureFailureView view =
      Present(capture::TransferResult::kBufferOverflow);
  const QString message = view.ToMessage();

  ASSERT_TRUE(message.contains(view.summary));
  ASSERT_TRUE(message.contains(view.remedy));
  EXPECT_LT(message.indexOf(view.summary), message.indexOf(view.remedy));
}

}  // namespace
}  // namespace ddd::gui
