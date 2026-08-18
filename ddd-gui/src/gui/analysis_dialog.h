/************************************************************************

    analysis_dialog.h

    Checking a test-mode capture, with somewhere to watch it happen
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QDialog>
#include <QString>
#include <QThread>
#include <atomic>

#include "test_data_analysis.h"

class QLabel;
class QProgressBar;
class QPushButton;

namespace ddd::gui {

// Runs the analysis off the GUI thread.
//
// Not an optimisation. A full disc side is tens of gigabytes and takes minutes
// to read and decode; doing that in the event loop would leave the application
// looking hung for the duration, and there would be no way to cancel it because
// the cancel button could not be painted.
class TestDataAnalysisWorker : public QThread {
  Q_OBJECT

 public:
  explicit TestDataAnalysisWorker(QString file_path, QObject* parent = nullptr);

  // Asks the read loop to stop at the end of its current chunk. Safe from the
  // GUI thread while the worker runs, which is the only place it is called
  // from.
  void RequestCancel();

 signals:
  // percentage is -1 when the file does not know its own length — a streamed
  // FLAC whose header was never patched, which is what a killed capture leaves
  // behind. The dialog shows a busy indicator rather than inventing a figure.
  void Progress(int percentage, qulonglong samples_checked);
  void Finished(int outcome, const QString& message);

 protected:
  void run() override;

 private:
  QString file_path_;
  std::atomic<bool> cancel_requested_{false};
};

// The GUI half of step 4 of the capture-integrity procedure (TESTING.md §5).
//
// The other half is `--analyse-test-data`, and the two share every part that
// can be wrong: the read loop, the ramp check and the wording of the verdict
// all live in ddd::capture::AnalyseTestData. What is here is the file chooser,
// the thread and the progress bar. The old application had the loop written out
// twice, once for the dialog and once for the command line, and the two could
// report different things about the same file.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class AnalysisDialog : public QDialog {
  Q_OBJECT

 public:
  explicit AnalysisDialog(QWidget* parent = nullptr);
  ~AnalysisDialog() override;

  // Ask for a file and analyse it. Returns without showing anything if the
  // chooser was dismissed.
  void ChooseFileAndAnalyse(const QString& starting_directory);

  // Analyse a named file. Separated from the chooser so a test can drive the
  // whole dialog without a modal file dialog in the way.
  void Analyse(const QString& file_path);

  // The verdict, once Finished has been seen. Held so a test can assert on the
  // outcome rather than on the text of a label.
  capture::TestDataAnalysis::Outcome outcome() const { return outcome_; }

  static constexpr const char* kProgressBarName = "analysis_progress";
  static constexpr const char* kResultLabelName = "analysis_result";
  static constexpr const char* kCancelButtonName = "analysis_cancel";

 private slots:
  void OnProgress(int percentage, qulonglong samples_checked);
  void OnFinished(int outcome, const QString& message);
  void OnCancelPressed();

 private:
  // Forward declared at global scope above, not with an elaborated specifier
  // here: inside this namespace `class QLabel*` declares ddd::gui::QLabel, a
  // different and incomplete type that happens to compile in a header and
  // fails in every file that tries to use it.
  QProgressBar* progress_ = nullptr;
  QLabel* result_ = nullptr;
  QPushButton* cancel_ = nullptr;

  TestDataAnalysisWorker* worker_ = nullptr;
  capture::TestDataAnalysis::Outcome outcome_ =
      capture::TestDataAnalysis::Outcome::kUnreadable;
};

}  // namespace ddd::gui
