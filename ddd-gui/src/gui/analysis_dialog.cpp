/************************************************************************

    analysis_dialog.cpp

    Checking a test-mode capture, with somewhere to watch it happen
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "analysis_dialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <filesystem>

#include "theme_color_tokens.h"

namespace ddd::gui {
namespace {

// Wide enough for a verdict sentence naming a file and a sample offset, without
// it wrapping into a paragraph.
constexpr int kDialogWidthPixels = 520;

}  // namespace

TestDataAnalysisWorker::TestDataAnalysisWorker(QString file_path,
                                               QObject* parent)
    : QThread(parent), file_path_(std::move(file_path)) {}

void TestDataAnalysisWorker::RequestCancel() { cancel_requested_ = true; }

void TestDataAnalysisWorker::run() {
  int last_percentage = -2;

  const capture::TestDataAnalysis analysis = capture::AnalyseTestData(
      std::filesystem::path(file_path_.toStdString()),
      [this, &last_percentage](uint64_t checked,
                               std::optional<uint64_t> total) {
        int percentage = -1;
        if (total.has_value() && *total > 0) {
          percentage = static_cast<int>((checked * 100) / *total);
        }

        // Only when it changes. A signal per chunk on a file that knows its
        // length would post several hundred events the GUI thread has to
        // process, all of them saying the same thing.
        if (percentage != last_percentage) {
          last_percentage = percentage;
          emit Progress(percentage, static_cast<qulonglong>(checked));
        }
      },
      [this] { return cancel_requested_.load(); });

  emit Finished(static_cast<int>(analysis.outcome),
                QString::fromStdString(analysis.message));
}

AnalysisDialog::AnalysisDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(tr("Analyse test data"));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(20, 20, 20, 16);
  layout->setSpacing(12);

  result_ = new QLabel(this);
  result_->setObjectName(QLatin1String(kResultLabelName));
  result_->setWordWrap(true);
  result_->setMinimumWidth(kDialogWidthPixels);
  result_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(result_);

  progress_ = new QProgressBar(this);
  progress_->setObjectName(QLatin1String(kProgressBarName));
  progress_->setRange(0, 100);
  progress_->setValue(0);
  layout->addWidget(progress_);

  auto* buttons = new QDialogButtonBox(this);
  cancel_ = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
  cancel_->setObjectName(QLatin1String(kCancelButtonName));
  connect(cancel_, &QPushButton::clicked, this,
          &AnalysisDialog::OnCancelPressed);
  layout->addWidget(buttons);
}

AnalysisDialog::~AnalysisDialog() {
  // The worker holds a pointer back to this object through its connections, so
  // it has to be stopped and joined before the object goes. A cancel is asked
  // for first so the join is over in one chunk rather than at the end of the
  // file.
  if (worker_ != nullptr) {
    worker_->RequestCancel();
    worker_->wait();
  }
}

void AnalysisDialog::ChooseFileAndAnalyse(const QString& starting_directory) {
  const QString file_path = QFileDialog::getOpenFileName(
      this, tr("Select a test-mode capture"), starting_directory,
      tr("Captures (*.flac *.raw);;All files (*)"));

  if (file_path.isEmpty()) {
    return;
  }

  Analyse(file_path);
  exec();
}

void AnalysisDialog::Analyse(const QString& file_path) {
  result_->setText(tr("Analysing %1…").arg(QFileInfo(file_path).fileName()));
  result_->setStyleSheet(QString());
  progress_->setRange(0, 100);
  progress_->setValue(0);
  cancel_->setText(tr("Cancel"));
  cancel_->setEnabled(true);

  worker_ = new TestDataAnalysisWorker(file_path, this);
  connect(worker_, &TestDataAnalysisWorker::Progress, this,
          &AnalysisDialog::OnProgress);
  connect(worker_, &TestDataAnalysisWorker::Finished, this,
          &AnalysisDialog::OnFinished);
  worker_->start();
}

void AnalysisDialog::OnProgress(int percentage, qulonglong samples_checked) {
  if (percentage < 0) {
    // The file does not know its own length, which is what a capture killed
    // mid-write leaves behind. A busy indicator says "working" honestly; a
    // percentage here would have to be invented.
    progress_->setRange(0, 0);
  } else {
    progress_->setRange(0, 100);
    progress_->setValue(percentage);
  }

  result_->setText(
      tr("Checked %1 samples…")
          .arg(QLocale().toString(static_cast<qulonglong>(samples_checked))));
}

void AnalysisDialog::OnFinished(int outcome, const QString& message) {
  outcome_ = static_cast<capture::TestDataAnalysis::Outcome>(outcome);

  progress_->setRange(0, 100);
  progress_->setValue(100);
  cancel_->setText(tr("Close"));
  result_->setText(message);

  // Colour rather than an icon, so the verdict is readable across a bench.
  // Through the theme tokens rather than a literal green: the same green on a
  // dark background is unreadable, and this is the one message in the
  // application somebody reads from a distance.
  const bool dark = theme_tokens::IsDarkPalette(palette());
  const QColor colour = theme_tokens::PlotColor(
      outcome_ == capture::TestDataAnalysis::Outcome::kPassed
          ? theme_tokens::PlotColorToken::kVerdictPass
          : theme_tokens::PlotColorToken::kVerdictFail,
      dark);
  result_->setStyleSheet(
      QStringLiteral("color: %1").arg(colour.name(QColor::HexRgb)));
}

void AnalysisDialog::OnCancelPressed() {
  if (worker_ != nullptr && worker_->isRunning()) {
    worker_->RequestCancel();
    return;
  }
  accept();
}

}  // namespace ddd::gui
