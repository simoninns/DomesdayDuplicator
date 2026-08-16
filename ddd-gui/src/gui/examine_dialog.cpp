/************************************************************************

    examine_dialog.cpp

    Working out what is in the player, and saying so
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "examine_dialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include "capture_settings.h"
#include "player_controller.h"
#include "player_text.h"

namespace ddd::gui {
namespace {

// What a capture of this disc would cost, at the settings the user is actually
// going to capture with.
//
// Read from the saved settings rather than passed in from a capture controller,
// which would couple this window to the capture side for one number. The
// settings file is the single source of truth for both, and the estimate is
// only ever an estimate.
double CaptureBytesPerSecond() {
  return LoadCaptureSettings().EstimatedBytesPerSecond();
}

}  // namespace

ExamineDialog::ExamineDialog(PlayerController* controller, QWidget* parent)
    : QDialog(parent), controller_(controller) {
  setWindowTitle(tr("Examine disc"));

  // Non-modal, like the remote: an examination is half a minute of a disc
  // spinning up and seeking, and the spectrum and waveform are exactly what
  // somebody wants to be watching while it happens.
  setWindowModality(Qt::NonModal);
  setSizeGripEnabled(true);

  auto* layout = new QVBoxLayout(this);

  headline_ = new QLabel(this);
  headline_->setObjectName(QLatin1String(kHeadlineLabelName));
  headline_->setWordWrap(true);
  QFont headline_font = headline_->font();
  headline_font.setBold(true);
  headline_->setFont(headline_font);
  layout->addWidget(headline_);

  stage_ = new QLabel(this);
  stage_->setObjectName(QLatin1String(kStageLabelName));
  stage_->setWordWrap(true);
  layout->addWidget(stage_);

  progress_ = new QProgressBar(this);
  progress_->setObjectName(QLatin1String(kProgressBarName));
  progress_->setRange(0, 1);
  progress_->setValue(0);
  progress_->setTextVisible(false);
  layout->addWidget(progress_);

  report_ = new QPlainTextEdit(this);
  report_->setObjectName(QLatin1String(kReportViewName));
  report_->setReadOnly(true);
  report_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  layout->addWidget(report_, 1);

  auto* buttons = new QDialogButtonBox(this);

  start_ = buttons->addButton(tr("Examine"), QDialogButtonBox::ActionRole);
  start_->setObjectName(QLatin1String(kStartButtonName));

  cancel_ = buttons->addButton(tr("Stop"), QDialogButtonBox::ActionRole);
  cancel_->setObjectName(QLatin1String(kCancelButtonName));

  copy_ = buttons->addButton(tr("Copy report"), QDialogButtonBox::ActionRole);
  copy_->setObjectName(QLatin1String(kCopyButtonName));

  buttons->addButton(QDialogButtonBox::Close);

  connect(start_, &QPushButton::clicked, this, &ExamineDialog::Start);
  connect(cancel_, &QPushButton::clicked, this, &ExamineDialog::Cancel);
  connect(copy_, &QPushButton::clicked, this, &ExamineDialog::CopyReport);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);

  layout->addWidget(buttons);

  if (controller_ != nullptr) {
    connect(controller_, &PlayerController::ConnectionChanged, this,
            &ExamineDialog::SetConnection);
    connect(controller_, &PlayerController::ExamineProgress, this,
            &ExamineDialog::SetProgress);
    connect(controller_, &PlayerController::ExamineFinished, this,
            &ExamineDialog::SetResult);

    connection_ = controller_->connection();
  }

  ApplyState();
}

void ExamineDialog::Start() {
  if (running_ || controller_ == nullptr || !connection_.live()) {
    return;
  }

  running_ = true;
  profile_ = player::DiscProfile{};

  // Cleared rather than left showing the last disc's answers. A report that
  // stayed on screen while a new disc was being examined would be read as this
  // disc's, and the two discs a user examines in a row are the two sides of the
  // same one.
  report_->clear();
  progress_->setRange(0, 1);
  progress_->setValue(0);

  headline_->setText(
      ExamineSummary(profile_, player::ExamineOutcome::kInProgress));
  stage_->setText(ExamineStageName(player::ExamineStage::kCheckingPlayer));

  ApplyState();
  controller_->Examine();
}

void ExamineDialog::Cancel() {
  if (!running_ || controller_ == nullptr) {
    return;
  }

  controller_->CancelExamine();

  // Said as soon as it is asked for, because it is not granted immediately: the
  // player may be halfway through a seek, and a window that looked unchanged
  // for the next twenty seconds would be read as one that had ignored the
  // button.
  stage_->setText(
      tr("Stopping — waiting for the step the player is in the middle of."));
  cancel_->setEnabled(false);
}

void ExamineDialog::SetConnection(const PlayerConnection& connection) {
  connection_ = connection;
  ApplyState();
}

void ExamineDialog::SetProgress(player::ExamineStage stage, int completed,
                                int total) {
  if (!running_) {
    return;
  }

  progress_->setRange(0, total > 0 ? total : 1);
  progress_->setValue(completed);
  stage_->setText(ExamineStageName(stage));
}

void ExamineDialog::SetResult(const player::DiscProfile& disc,
                              player::ExamineOutcome outcome) {
  running_ = false;
  profile_ = disc;

  progress_->setValue(progress_->maximum());
  headline_->setText(ExamineSummary(disc, outcome));
  stage_->setText(tr("Examination %1.").arg(ExamineOutcomeText(outcome)));
  report_->setPlainText(
      DiscProfileReport(disc, outcome, CaptureBytesPerSecond()));

  ApplyState();
}

void ExamineDialog::CopyReport() {
  QClipboard* const clipboard = QApplication::clipboard();
  if (clipboard != nullptr) {
    clipboard->setText(report_->toPlainText());
  }
}

void ExamineDialog::ApplyState() {
  const bool live = connection_.live();

  start_->setEnabled(live && !running_);
  cancel_->setEnabled(running_);
  copy_->setEnabled(!report_->toPlainText().isEmpty());

  if (running_) {
    return;
  }

  if (!live) {
    headline_->setText(tr("No player is connected."));
    stage_->setText(PlayerConnectionSummary(connection_));
    return;
  }

  if (report_->toPlainText().isEmpty()) {
    headline_->setText(tr("Nothing examined yet."));
    stage_->setText(
        tr("Examining spins the disc up, reads both of the disc's own "
           "identifying codes, seeks to the end of the side to measure it, and "
           "comes back to the start. It takes about a minute and leaves the "
           "disc held still at its beginning."));
  }
}

}  // namespace ddd::gui
