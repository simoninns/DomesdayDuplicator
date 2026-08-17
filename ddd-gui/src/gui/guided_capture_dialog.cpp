/************************************************************************

    guided_capture_dialog.cpp

    Setting up a capture from what the disc turned out to be
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "guided_capture_dialog.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <ctime>
#include <filesystem>

#include "auto_capture_controller.h"
#include "capture_controller.h"
#include "capture_failure_presenter.h"
#include "capture_settings.h"
#include "player_controller.h"
#include "player_text.h"

namespace ddd::gui {

GuidedCaptureDialog::GuidedCaptureDialog(AutoCaptureController* controller,
                                         const player::DiscProfile& disc,
                                         QWidget* parent)
    : QDialog(parent), controller_(controller) {
  setWindowTitle(tr("Set up capture"));

  // Non-modal, like the examine window and the remote: a whole-side capture is
  // the better part of an hour, and the spectrum and waveform panels are what
  // somebody wants to be watching while it runs.
  setWindowModality(Qt::NonModal);
  setSizeGripEnabled(true);

  auto* layout = new QVBoxLayout(this);

  headline_ = new QLabel(this);
  headline_->setObjectName(QLatin1String(kHeadlineLabelName));
  headline_->setWordWrap(true);
  QFont headline_font = headline_->font();
  headline_font.setBold(true);
  headline_->setFont(headline_font);
  headline_->setText(ExamineSummary(disc, player::ExamineOutcome::kCompleted));
  layout->addWidget(headline_);

  // --- What to call it -----------------------------------------------------

  auto* name_form = new QFormLayout();

  name_ = new QLineEdit(this);
  name_->setObjectName(QLatin1String(kNameEditName));
  name_->setPlaceholderText(tr("RF-Sample_<timestamp>"));
  name_->setToolTip(
      tr("A prefill from what the disc turned out to be, not a scheme — type "
         "over it. Leaving it empty gives the usual timestamped name."));
  name_form->addRow(tr("Capture name"), name_);

  name_taken_ = new QLabel(this);
  name_taken_->setObjectName(QLatin1String(kNameTakenLabelName));
  name_taken_->setWordWrap(true);
  name_taken_->hide();
  name_form->addRow(QString(), name_taken_);

  layout->addLayout(name_form);

  // **The suggestion is resolved against the destination before it is
  // offered.** It is built from what the disc is — "CLV_PAL_Side2" — and so is
  // the same every time that side is captured, unlike the generated name, which
  // carries a timestamp. A prefill that was already taken would put a name in
  // the field that is not the name of the file, which is precisely what a
  // suggestion must never do.
  const QString suggested = SuggestedCaptureName(disc);
  if (!suggested.isEmpty()) {
    const CaptureSettings settings = LoadCaptureSettings();
    const capture::CaptureDestination free_name =
        capture::ResolveCaptureDestination(
            std::filesystem::path(
                settings.ResolvedCaptureDirectory().toStdString()),
            suggested.toStdString(), settings.test_mode, std::time(nullptr),
            settings.output_format);
    name_->setText(QString::fromStdString(free_name.stem));
  }

  // --- What to capture, and what it will cost ------------------------------

  form_ = new CapturePlanForm(disc, this);
  layout->addWidget(form_);

  if (controller_ != nullptr) {
    form_->SetStopCaptureWithPlayer(
        controller_->settings().stop_capture_with_player);
  }

  // --- The run -------------------------------------------------------------

  status_ = new QLabel(this);
  status_->setObjectName(QLatin1String(kStatusLabelName));
  status_->setWordWrap(true);
  layout->addWidget(status_);

  progress_ = new QProgressBar(this);
  progress_->setObjectName(QLatin1String(kProgressBarName));
  progress_->setRange(0, 1);
  progress_->setValue(0);
  progress_->setTextVisible(false);
  layout->addWidget(progress_);

  auto* buttons = new QDialogButtonBox(this);
  start_ =
      buttons->addButton(tr("Start capture"), QDialogButtonBox::ActionRole);
  start_->setObjectName(QLatin1String(kStartButtonName));
  cancel_ = buttons->addButton(tr("Stop"), QDialogButtonBox::ActionRole);
  cancel_->setObjectName(QLatin1String(kCancelButtonName));
  buttons->addButton(QDialogButtonBox::Close);

  connect(start_, &QPushButton::clicked, this, &GuidedCaptureDialog::Start);
  connect(cancel_, &QPushButton::clicked, this, &GuidedCaptureDialog::Cancel);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
  layout->addWidget(buttons);

  connect(name_, &QLineEdit::textChanged, this,
          [this](const QString&) { RefreshNameNote(); });

  // The form works out what is wrong with the plan and says so itself; what is
  // left for this window is the button that would run it.
  connect(form_, &CapturePlanForm::Changed, this,
          &GuidedCaptureDialog::Refresh);

  if (controller_ != nullptr) {
    connect(controller_, &AutoCaptureController::Progress, this,
            &GuidedCaptureDialog::SetProgress);
    connect(controller_, &AutoCaptureController::Finished, this,
            &GuidedCaptureDialog::SetResult);
    running_ = controller_->running();
  }

  Refresh();
  RefreshNameNote();
}

capture::CaptureDestination GuidedCaptureDialog::Destination() const {
  const CaptureSettings settings = LoadCaptureSettings();

  return capture::ResolveCaptureDestination(
      std::filesystem::path(settings.ResolvedCaptureDirectory().toStdString()),
      name_->text().trimmed().toStdString(), settings.test_mode,
      std::time(nullptr), settings.output_format);
}

void GuidedCaptureDialog::RefreshNameNote() {
  // Nothing to say about the generated name: it carries a timestamp, so it is
  // free by construction.
  if (name_->text().trimmed().isEmpty()) {
    name_taken_->hide();
    return;
  }

  const capture::CaptureDestination destination = Destination();
  if (destination.as_requested) {
    name_taken_->hide();
    return;
  }

  name_taken_->setText(
      CaptureNameTakenNote(QString::fromStdString(destination.stem)));
  name_taken_->show();
}

void GuidedCaptureDialog::Refresh() {
  // The name is fixed for the duration of a run — the file is already open —
  // and the form locks its own controls on the same terms.
  form_->SetEditable(!running_);
  name_->setEnabled(!running_);

  start_->setEnabled(!running_ &&
                     form_->problem() == player::PlanProblem::kNone &&
                     controller_ != nullptr);
  cancel_->setEnabled(running_);
}

void GuidedCaptureDialog::Start() {
  if (running_ || controller_ == nullptr ||
      form_->problem() != player::PlanProblem::kNone) {
    return;
  }

  // The name and the coupling preference are applied to the settings they
  // belong to rather than carried in the plan: they outlive this capture, and
  // the settings dialog shows the same checkbox.
  if (CaptureController* const capture = controller_->capture();
      capture != nullptr) {
    CaptureSettings settings = capture->settings();
    settings.capture_name = name_->text().trimmed();
    capture->SetSettings(settings);
  }

  if (PlayerController* const player = controller_->player();
      player != nullptr) {
    PlayerSettings settings = player->settings();
    settings.stop_capture_with_player = form_->stop_capture_with_player();
    player->SetSettings(settings);
  }

  running_ = true;
  progress_->setRange(0, 0);
  status_->setText(
      AutoCaptureStageName(player::AutoCaptureStage::kLockingFrontPanel));
  Refresh();

  controller_->Start(form_->Plan(), form_->disc());
}

void GuidedCaptureDialog::Cancel() {
  if (!running_ || controller_ == nullptr) {
    return;
  }

  // Said as soon as it is asked for. It is not granted immediately — the player
  // may be halfway through a seek — and a window that looked unchanged for the
  // next twenty seconds would be read as one that had ignored the button.
  status_->setText(tr("Stopping — the file is being finished properly."));
  cancel_->setEnabled(false);
  controller_->Cancel();
}

void GuidedCaptureDialog::SetProgress(player::AutoCaptureStage stage,
                                      int address) {
  if (!running_) {
    return;
  }

  status_->setText(AutoCaptureStageName(stage));

  const player::AutoCapturePlan plan = form_->Plan();
  const player::AddressMode mode = plan.addressing;

  if (address < 0 || plan.end_address <= plan.start_address) {
    // Nothing to measure against yet. A busy bar rather than an empty one,
    // because the setting-up is the part that takes seconds with nothing to
    // show for it.
    progress_->setRange(0, 0);
    return;
  }

  // The address is the only honest measure of how far a capture has got — the
  // step count is meaningless when one step is repeated for forty minutes.
  progress_->setRange(0, plan.end_address - plan.start_address);
  progress_->setValue(
      qBound(0, address - plan.start_address, progress_->maximum()));

  QString line = tr("%1 — %2").arg(AutoCaptureStageName(stage),
                                   FormatDiscAddress(address, mode));

  // Only while the disc is actually being watched. The address persists through
  // the tail so the bar does not jump back, but a "time left" beside "stopping
  // the player" would be counting down to something that has already happened.
  if (stage == player::AutoCaptureStage::kWatching) {
    const QString remaining =
        AutoCaptureRemainingText(plan, form_->disc(), address);
    if (!remaining.isEmpty()) {
      line = tr("%1, %2").arg(line, remaining);
    }
  }

  status_->setText(line);
}

void GuidedCaptureDialog::SetResult(player::AutoCaptureOutcome outcome) {
  running_ = false;
  progress_->setRange(0, 1);
  progress_->setValue(outcome == player::AutoCaptureOutcome::kCompleted ? 1
                                                                        : 0);
  status_->setText(AutoCaptureSummary(outcome));
  Refresh();
}

}  // namespace ddd::gui
