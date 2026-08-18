/************************************************************************

    auto_capture_wizard.cpp

    Taking a capture off a disc, from finding out what it is to what was written
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "auto_capture_wizard.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <ctime>
#include <filesystem>

#include "auto_capture_controller.h"
#include "capture_controller.h"
#include "capture_failure_presenter.h"
#include "capture_format.h"
#include "capture_settings.h"
#include "player_controller.h"
#include "player_text.h"

namespace ddd::gui {
namespace {

// The size this window opens at.
//
// Chosen rather than derived, and it has to be: each page is a QScrollArea, and
// a scroll area reports a small fixed size hint whatever it contains, so the
// natural size of this window is a couple of hundred pixels square regardless
// of the four pages inside it. The pages are scroll areas because page 1 is
// genuinely long — an examination report, a capture name and the whole naming
// form — and a window that could not scroll would be worse on a short screen
// than one that opens too small on a tall one.
//
// Wide enough for the naming form's labelled fields and the plan's address
// boxes without the wrapped explanatory sentences becoming a column of five
// words, and tall enough to hold page 1 down to the naming form's first group
// box, which is where somebody's eye goes after reading what the disc is.
constexpr int kWantedWidthPixels = 760;
constexpr int kWantedHeightPixels = 820;

// Never more than this much of the screen, in either direction.
//
// A dialog that opens taller than the desktop puts its own Next button below
// the bottom edge, where on several window managers it cannot be reached or
// resized back into view — which would turn "the window is too big" into "the
// window is unusable". The margin is what leaves the title bar and a taskbar
// somewhere to be.
constexpr double kMostOfTheScreen = 0.9;

// A page's heading, so somebody landing on one knows which of the four steps
// they are in without counting tabs.
QLabel* MakeHeading(QWidget* parent, const QString& text) {
  auto* label = new QLabel(text, parent);
  QFont font = label->font();
  font.setBold(true);
  label->setFont(font);
  label->setWordWrap(true);
  return label;
}

}  // namespace

AutoCaptureWizard::AutoCaptureWizard(AutoCaptureController* controller,
                                     QWidget* parent)
    : QDialog(parent),
      controller_(controller),
      capture_(controller != nullptr ? controller->capture() : nullptr),
      player_(controller != nullptr ? controller->player() : nullptr) {
  setWindowTitle(tr("Automatic capture"));

  // Non-modal, like every other player window: a whole-side capture is the
  // better part of an hour, and the spectrum and waveform panels are what
  // somebody wants to be watching while it runs.
  setWindowModality(Qt::NonModal);
  setSizeGripEnabled(true);

  const QScreen* const display = screen();
  const QSize available = display != nullptr
                              ? display->availableGeometry().size()
                              : QSize(kWantedWidthPixels, kWantedHeightPixels);
  resize(std::min(kWantedWidthPixels,
                  static_cast<int>(available.width() * kMostOfTheScreen)),
         std::min(kWantedHeightPixels,
                  static_cast<int>(available.height() * kMostOfTheScreen)));

  auto* layout = new QVBoxLayout(this);

  pages_ = new QStackedWidget(this);
  pages_->setObjectName(QLatin1String(kPagesName));
  pages_->addWidget(BuildDiscPage());
  pages_->addWidget(BuildSettingsPage());
  pages_->addWidget(BuildCapturePage());
  pages_->addWidget(BuildSummaryPage());
  layout->addWidget(pages_, 1);

  // Laid out by hand rather than through a QDialogButtonBox, and the layout is
  // the whole reason: **Close goes on the left, with the width of the window
  // between it and Next.**
  //
  // A button box puts Close beside Next, where the button that leaves and the
  // button that carries on are a few pixels apart and differ only in their
  // label. On the last page of a long capture that is a genuinely expensive
  // slip. Two ends of the row is the arrangement that cannot be misread —
  // navigation on the right where it belongs, and the way out somewhere it is
  // not reached for by accident.
  auto* button_row = new QWidget(this);
  auto* button_layout = new QHBoxLayout(button_row);
  button_layout->setContentsMargins(0, 0, 0, 0);

  QPushButton* const close = new QPushButton(tr("Close"), button_row);
  close->setObjectName(QLatin1String(kCloseButtonName));
  button_layout->addWidget(close);

  button_layout->addStretch(1);

  previous_button_ = new QPushButton(tr("‹ Previous"), button_row);
  previous_button_->setObjectName(QLatin1String(kPreviousButtonName));
  button_layout->addWidget(previous_button_);

  next_button_ = new QPushButton(tr("Next ›"), button_row);
  next_button_->setObjectName(QLatin1String(kNextButtonName));
  button_layout->addWidget(next_button_);

  connect(previous_button_, &QPushButton::clicked, this,
          &AutoCaptureWizard::GoPrevious);
  connect(next_button_, &QPushButton::clicked, this,
          &AutoCaptureWizard::GoNext);
  connect(close, &QPushButton::clicked, this, &QWidget::close);
  layout->addWidget(button_row);

  if (player_ != nullptr) {
    connect(player_, &PlayerController::ExamineProgress, this,
            &AutoCaptureWizard::SetExamineProgress);
    connect(player_, &PlayerController::ExamineFinished, this,
            &AutoCaptureWizard::SetExamineResult);
    connect(player_, &PlayerController::ConnectionChanged, this,
            [this](const PlayerConnection&) { Refresh(); });
  }

  if (controller_ != nullptr) {
    connect(controller_, &AutoCaptureController::Progress, this,
            &AutoCaptureWizard::SetRunProgress);
    connect(controller_, &AutoCaptureController::Finished, this,
            &AutoCaptureWizard::SetRunResult);
    running_ = controller_->running();
  }

  if (capture_ != nullptr) {
    // Kept for the summary. The engine resolves the path before it opens
    // anything, so this is the name the file actually got rather than the one
    // that was asked for.
    connect(capture_, &CaptureController::CaptureFinished, this,
            [this](const QString& path, quint64 bytes) {
              written_path_ = path;
              written_bytes_ = bytes;
            });
  }

  ForceFullSampleRate();
  ClearDurationLimit();
  ShowCaptureSettings();
  RebuildPlanForm();
  ShowPage(Page::kDisc);

  // Straight into the examination. Somebody who chose "Automatic capture" has
  // already said what they want to happen first, and the whole page is about
  // waiting for it — an extra press to begin would be a step that exists only
  // to be got past. There is a Stop beside it for anybody who opened this by
  // accident.
  if (player_ != nullptr && player_->connection().live()) {
    Examine();
  }
}

// --- The pages --------------------------------------------------------------

QWidget* AutoCaptureWizard::BuildDiscPage() {
  // In a scroll area, because this page carries the naming form as well as the
  // examination: it is the tallest of the four and would otherwise set the
  // height of the window on every other one.
  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto* page = new QWidget(scroll);
  scroll->setWidget(page);

  auto* layout = new QVBoxLayout(page);

  layout->addWidget(MakeHeading(page, tr("1. What is in the player")));

  disc_headline_ = new QLabel(page);
  disc_headline_->setObjectName(QLatin1String(kDiscHeadlineName));
  disc_headline_->setWordWrap(true);
  layout->addWidget(disc_headline_);

  examine_stage_ = new QLabel(page);
  examine_stage_->setObjectName(QLatin1String(kExamineStageName));
  examine_stage_->setWordWrap(true);
  layout->addWidget(examine_stage_);

  examine_progress_ = new QProgressBar(page);
  examine_progress_->setObjectName(QLatin1String(kExamineProgressName));
  examine_progress_->setRange(0, 1);
  examine_progress_->setValue(0);
  examine_progress_->setTextVisible(false);
  layout->addWidget(examine_progress_);

  auto* examine_row = new QHBoxLayout();

  examine_button_ = new QPushButton(tr("Examine again"), page);
  examine_button_->setObjectName(QLatin1String(kExamineButtonName));
  examine_button_->setToolTip(
      tr("Spin the disc up, read what it says about itself, and measure the "
         "side by seeking to both ends of it. About a minute, and it puts the "
         "player back the way it found it."));
  examine_row->addWidget(examine_button_);

  examine_stop_button_ = new QPushButton(tr("Stop"), page);
  examine_stop_button_->setObjectName(QLatin1String(kExamineStopButtonName));
  examine_row->addWidget(examine_stop_button_);
  examine_row->addStretch(1);
  layout->addLayout(examine_row);

  connect(examine_button_, &QPushButton::clicked, this,
          &AutoCaptureWizard::Examine);
  connect(examine_stop_button_, &QPushButton::clicked, this,
          &AutoCaptureWizard::StopExamining);

  // --- What it will be called ---------------------------------------------

  // In a box of its own, and it has to be. The naming form below opens with a
  // sentence about the disc's fields, and with the name field loose above it
  // that sentence lands directly under "Capture name" and reads as a
  // description of it. Two blocks, each with its own heading, is the fix: the
  // name is one thing and what the disc is another, even though the second can
  // build the first.
  auto* name_box = new QGroupBox(tr("What it will be called"), page);
  auto* name_form = new QFormLayout(name_box);

  name_edit_ = new QLineEdit(name_box);
  name_edit_->setObjectName(QLatin1String(kNameEditName));
  name_edit_->setPlaceholderText(tr("RF-Sample_<timestamp>"));
  name_edit_->setToolTip(
      tr("A prefill from what the disc turned out to be, not a scheme — type "
         "over it. Leaving it empty gives the usual timestamped name."));
  name_form->addRow(tr("Capture name"), name_edit_);

  name_taken_ = new QLabel(name_box);
  name_taken_->setObjectName(QLatin1String(kNameTakenLabelName));
  name_taken_->setWordWrap(true);
  name_taken_->hide();
  name_form->addRow(QString(), name_taken_);

  layout->addWidget(name_box);

  connect(name_edit_, &QLineEdit::textChanged, this,
          [this](const QString&) { RefreshNameNote(); });

  // The same fields the manual path's Naming… button opens, so there is one set
  // of naming rules in the application. No player is passed: this page examines
  // the disc itself, so an "Ask the player" button here would be a second way
  // to do what the page is already doing.
  naming_form_ = new CaptureNamingForm(capture_, nullptr, page);
  layout->addWidget(naming_form_);

  layout->addStretch(1);
  return scroll;
}

QWidget* AutoCaptureWizard::BuildSettingsPage() {
  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto* page = new QWidget(scroll);
  scroll->setWidget(page);

  settings_layout_ = new QVBoxLayout(page);

  settings_layout_->addWidget(
      MakeHeading(page, tr("2. What to capture, and where to put it")));

  // The plan form goes in here, at index 1, whenever a disc arrives. It cannot
  // be built now because what it offers depends on the disc — see
  // RebuildPlanForm.

  // --- Where it goes -------------------------------------------------------

  auto* destination = new QGroupBox(tr("Where it goes"), page);
  auto* destination_form = new QFormLayout(destination);

  auto* directory_row = new QWidget(destination);
  auto* directory_layout = new QHBoxLayout(directory_row);
  directory_layout->setContentsMargins(0, 0, 0, 0);

  directory_edit_ = new QLineEdit(directory_row);
  directory_edit_->setObjectName(QLatin1String(kDirectoryEditName));
  directory_layout->addWidget(directory_edit_, 1);

  browse_button_ = new QPushButton(tr("Browse…"), directory_row);
  browse_button_->setObjectName(QLatin1String(kBrowseButtonName));
  directory_layout->addWidget(browse_button_);

  destination_form->addRow(tr("Folder"), directory_row);

  format_combo_ = new QComboBox(destination);
  format_combo_->setObjectName(QLatin1String(kFormatComboName));
  format_combo_->addItem(
      tr("FLAC — %1").arg(QLatin1String(capture::kCaptureFileSuffix)),
      static_cast<int>(capture::CaptureOutputFormat::kFlac));
  format_combo_->addItem(
      tr("Uncompressed — %1")
          .arg(QLatin1String(capture::kSigned16BitCaptureFileSuffix)),
      static_cast<int>(capture::CaptureOutputFormat::kSigned16Bit));
  destination_form->addRow(tr("Format"), format_combo_);

  // Stated rather than offered, and the half rate is not among the choices
  // because there are no choices. 20 MSPS exists for tape, whose RF is a
  // fraction of a LaserDisc's bandwidth; this window drives a LaserDisc player,
  // so a decimated capture here would fold everything above 10 MHz down on top
  // of the signal and nothing downstream could tell the alias from the disc. A
  // drop-down with one entry in it would only invite somebody to look for the
  // setting that adds the other one.
  sample_rate_label_ =
      new QLabel(tr("40 MSPS — a LaserDisc's full rate"), destination);
  sample_rate_label_->setObjectName(QLatin1String(kSampleRateLabelName));
  sample_rate_label_->setToolTip(
      tr("The 20 MSPS setting is for VHS and other tape formats, and is not "
         "offered here: an automatic capture drives a LaserDisc player. If the "
         "Capture panel is set to 20 MSPS, this capture puts it back to 40."));
  destination_form->addRow(tr("Sample rate"), sample_rate_label_);

  sample_rate_warning_ = new QLabel(destination);
  sample_rate_warning_->setObjectName(QLatin1String(kSampleRateWarningName));
  sample_rate_warning_->setWordWrap(true);
  sample_rate_warning_->hide();
  destination_form->addRow(QString(), sample_rate_warning_);

  settings_layout_->addWidget(destination);
  settings_layout_->addStretch(1);

  // These three are the Capture panel's own settings, written through the same
  // controller. Nothing here keeps a copy: the panel, this page and the
  // settings file are one value, so a folder chosen here is the folder the
  // panel shows.
  connect(browse_button_, &QPushButton::clicked, this, [this] {
    const QString chosen = QFileDialog::getExistingDirectory(
        this, tr("Where captures are written"), directory_edit_->text());
    if (!chosen.isEmpty()) {
      directory_edit_->setText(chosen);
      ApplyCaptureSettings();
    }
  });
  connect(directory_edit_, &QLineEdit::editingFinished, this,
          &AutoCaptureWizard::ApplyCaptureSettings);
  connect(format_combo_, &QComboBox::currentIndexChanged, this,
          [this](int) { ApplyCaptureSettings(); });

  return scroll;
}

QWidget* AutoCaptureWizard::BuildCapturePage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  layout->addWidget(MakeHeading(page, tr("3. The capture")));

  run_status_ = new QLabel(page);
  run_status_->setObjectName(QLatin1String(kRunStatusLabelName));
  run_status_->setWordWrap(true);
  layout->addWidget(run_status_);

  run_progress_ = new QProgressBar(page);
  run_progress_->setObjectName(QLatin1String(kRunProgressName));
  run_progress_->setRange(0, 1);
  run_progress_->setValue(0);
  run_progress_->setTextVisible(false);
  layout->addWidget(run_progress_);

  auto* row = new QHBoxLayout();

  start_button_ = new QPushButton(tr("Start capture"), page);
  start_button_->setObjectName(QLatin1String(kStartButtonName));
  row->addWidget(start_button_);

  stop_button_ = new QPushButton(tr("Stop"), page);
  stop_button_->setObjectName(QLatin1String(kStopButtonName));
  row->addWidget(stop_button_);
  row->addStretch(1);
  layout->addLayout(row);

  connect(start_button_, &QPushButton::clicked, this,
          &AutoCaptureWizard::Start);
  connect(stop_button_, &QPushButton::clicked, this, &AutoCaptureWizard::Stop);

  layout->addStretch(1);
  return page;
}

QWidget* AutoCaptureWizard::BuildSummaryPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  layout->addWidget(MakeHeading(page, tr("4. What happened")));

  summary_ = new QLabel(page);
  summary_->setObjectName(QLatin1String(kSummaryLabelName));
  summary_->setWordWrap(true);
  summary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(summary_);

  another_side_button_ = new QPushButton(tr("Capture another side"), page);
  another_side_button_->setObjectName(QLatin1String(kAnotherSideButtonName));
  another_side_button_->setToolTip(
      tr("Turn the disc over, then press this: it moves the side number on, "
         "examines what is now loaded, and comes back here. The two files "
         "somebody makes in a row are the two sides of one disc, and this is "
         "that done in one press rather than eight."));
  layout->addWidget(another_side_button_);

  connect(another_side_button_, &QPushButton::clicked, this,
          &AutoCaptureWizard::CaptureAnotherSide);

  layout->addStretch(1);
  return page;
}

// --- Moving about -----------------------------------------------------------

AutoCaptureWizard::Page AutoCaptureWizard::page() const {
  return static_cast<Page>(pages_->currentIndex());
}

void AutoCaptureWizard::ShowPage(Page page) {
  pages_->setCurrentIndex(static_cast<int>(page));
  Refresh();
}

void AutoCaptureWizard::GoNext() {
  switch (page()) {
    case Page::kDisc:
      if (!DiscIsUsable()) {
        return;
      }
      ShowPage(Page::kSettings);
      return;
    case Page::kSettings:
      if (problem() != player::PlanProblem::kNone) {
        return;
      }
      ShowPage(Page::kCapture);
      return;
    case Page::kCapture:
      // Only once there is something to report. The summary is written by the
      // run finishing, and an empty one reached by pressing Next would say a
      // capture had happened when none had.
      if (running_ || written_path_.isEmpty()) {
        return;
      }
      ShowPage(Page::kSummary);
      return;
    case Page::kSummary:
      return;
  }
}

void AutoCaptureWizard::GoPrevious() {
  // Never while a run is going. The page behind is the one that describes the
  // capture now being taken, and letting it be edited underneath would make the
  // window disagree with the file being written.
  if (running_) {
    return;
  }

  switch (page()) {
    case Page::kDisc:
      return;
    case Page::kSettings:
      ShowPage(Page::kDisc);
      return;
    case Page::kCapture:
      ShowPage(Page::kSettings);
      return;
    case Page::kSummary:
      ShowPage(Page::kCapture);
      return;
  }
}

void AutoCaptureWizard::StartFromProfile(const player::DiscProfile& disc) {
  SetExamineResult(disc, player::ExamineOutcome::kCompleted);
  if (DiscIsUsable()) {
    ShowPage(Page::kSettings);
  }
}

// --- The examination --------------------------------------------------------

void AutoCaptureWizard::Examine() {
  if (examining_ || running_ || player_ == nullptr ||
      !player_->connection().live()) {
    return;
  }

  examining_ = true;

  // Cleared rather than left showing the last disc's answers. A summary that
  // stayed on screen while a new disc was being examined would be read as this
  // disc's, and the two discs a user examines in a row are the two sides of the
  // same one.
  disc_ = player::DiscProfile{};
  examine_progress_->setRange(0, 1);
  examine_progress_->setValue(0);
  disc_headline_->setText(
      ExamineSummary(disc_, player::ExamineOutcome::kInProgress));
  examine_stage_->setText(
      ExamineStageName(player::ExamineStage::kCheckingPlayer));

  Refresh();
  player_->Examine();
}

void AutoCaptureWizard::StopExamining() {
  if (!examining_ || player_ == nullptr) {
    return;
  }

  // Said as soon as it is asked for, because it is not granted immediately: the
  // player may be halfway through a seek, and a window that looked unchanged
  // for the next twenty seconds would be read as one that had ignored the
  // button.
  examine_stage_->setText(
      tr("Stopping — waiting for the step the player is in the middle of."));
  examine_stop_button_->setEnabled(false);
  player_->CancelExamine();
}

void AutoCaptureWizard::SetExamineProgress(player::ExamineStage stage,
                                           int completed, int total) {
  if (!examining_) {
    return;
  }

  examine_progress_->setRange(0, total > 0 ? total : 1);
  examine_progress_->setValue(completed);
  examine_stage_->setText(ExamineStageName(stage));
}

void AutoCaptureWizard::SetExamineResult(const player::DiscProfile& disc,
                                         player::ExamineOutcome outcome) {
  examining_ = false;
  disc_ = disc;

  examine_progress_->setRange(0, 1);
  examine_progress_->setValue(1);
  disc_headline_->setText(ExamineSummary(disc_, outcome));
  examine_stage_->setText(
      tr("Examination %1.").arg(ExamineOutcomeText(outcome)));

  // The naming fields take what the examination established — the type, the
  // standard and the side — and leave everything a person has typed alone. The
  // rules are the form's, so this page and the manual path's dialog cannot
  // disagree about them.
  if (naming_form_ != nullptr) {
    naming_form_->FillFromProfile(disc_);
  }

  // The plan controls are rebuilt rather than updated: a CAV disc is offered
  // frame boxes and a CLV one time-code fields, and which exist at all is
  // decided when the form is built.
  RebuildPlanForm();
  SuggestName();
  Refresh();
}

bool AutoCaptureWizard::DiscIsUsable() const {
  // The same gate the Examine window applies to its own "Automatic capture…"
  // button: a capture needs the disc type and the measured end of the side.
  // Without them the next page would offer everything and refuse everything,
  // which is a worse way to learn that the examination did not finish.
  return !examining_ && disc_.disc_type.known() && disc_.programme_end.known();
}

// --- The plan and the destination -------------------------------------------

void AutoCaptureWizard::RebuildPlanForm() {
  if (settings_layout_ == nullptr) {
    return;
  }

  const bool stop_with_player =
      plan_form_ != nullptr ? plan_form_->stop_capture_with_player()
      : controller_ != nullptr
          ? controller_->settings().stop_capture_with_player
          : false;

  if (plan_form_ != nullptr) {
    settings_layout_->removeWidget(plan_form_);

    // Deleted outright rather than with deleteLater(), and it matters. A form
    // awaiting deletion is still a child of this window: it still answers
    // findChild, so anything looking a control up by name would get the
    // previous disc's copy of it, and it is still connected to Refresh(). The
    // two would then disagree about what is being captured until the event loop
    // next ran.
    //
    // Safe here because nothing from the form is on the stack: this is only
    // ever reached from an examination finishing or from the constructor,
    // never from the form's own Changed signal.
    delete plan_form_;
    plan_form_ = nullptr;
  }

  plan_form_ = new CapturePlanForm(disc_, settings_layout_->parentWidget());
  plan_form_->SetStopCaptureWithPlayer(stop_with_player);

  // Directly under the heading, above the destination group.
  settings_layout_->insertWidget(1, plan_form_);

  connect(plan_form_, &CapturePlanForm::Changed, this,
          &AutoCaptureWizard::Refresh);
}

player::AutoCapturePlan AutoCaptureWizard::Plan() const {
  return plan_form_ != nullptr ? plan_form_->Plan() : player::AutoCapturePlan{};
}

player::PlanProblem AutoCaptureWizard::problem() const {
  return plan_form_ != nullptr ? plan_form_->problem()
                               : player::PlanProblem::kUnknownLength;
}

void AutoCaptureWizard::ShowCaptureSettings() {
  if (capture_ == nullptr) {
    return;
  }

  loading_ = true;
  const CaptureSettings& settings = capture_->settings();
  directory_edit_->setText(settings.ResolvedCaptureDirectory());
  format_combo_->setCurrentIndex(
      format_combo_->findData(static_cast<int>(settings.output_format)));
  loading_ = false;

  ShowSampleRateWarning();
}

void AutoCaptureWizard::ForceFullSampleRate() {
  if (capture_ == nullptr) {
    return;
  }

  // Settled when the window opens rather than when somebody changes something,
  // because there is nothing here to change: an automatic capture is a
  // LaserDisc capture, and 20 MSPS is for tape. A Capture panel left decimated
  // from some tape work would otherwise carry that silently into a LaserDisc
  // capture, which is the whole failure this prevents.
  //
  // Not while a stream is open. The rate is written to the device before the
  // stream is opened and cannot be changed under a running one, so writing it
  // here would change what the settings say without changing what the device
  // is doing — and a file labelled 40 MSPS carrying half-rate samples is worse
  // than a warning. ShowSampleRateWarning raises that warning instead.
  if (capture_->monitoring()) {
    return;
  }

  CaptureSettings settings = capture_->settings();
  if (settings.decimation_factor == capture::kUndecimatedFactor) {
    return;
  }
  settings.decimation_factor = capture::kUndecimatedFactor;
  capture_->SetSettings(settings);
}

void AutoCaptureWizard::ShowSampleRateWarning() {
  if (capture_ == nullptr || sample_rate_warning_ == nullptr) {
    return;
  }

  // The rate is written to the device before the stream is opened, so it is
  // fixed from the moment monitoring starts — the Capture panel locks its own
  // control for the same reason. Forcing the setting here would change what the
  // panel says without changing what the device is doing, which is the one
  // outcome worse than saying nothing: a file labelled 40 MSPS carrying half
  // rate samples.
  const bool decimated =
      capture_->settings().decimation_factor != capture::kUndecimatedFactor;
  const bool stuck = decimated && capture_->monitoring();

  sample_rate_warning_->setVisible(stuck);
  if (stuck) {
    sample_rate_warning_->setText(
        tr("Monitoring is running at 20 MSPS, and the rate cannot be changed "
           "while a stream is open. Stop monitoring first — otherwise this "
           "capture is taken at half a LaserDisc's rate."));
  }
}

void AutoCaptureWizard::ClearDurationLimit() {
  if (capture_ == nullptr) {
    return;
  }

  // Settled when the window opens, for the same reason as the sample rate above
  // and against the same failure: a Capture panel setting left over from other
  // work carrying silently into an automatic capture. A limit set for a manual
  // capture — half a side, taken by hand and stopped after a couple of minutes
  // — would otherwise cut a whole-side run short in the middle of the disc.
  //
  // Every shape in AutoCapturePlan ends at an address: kWholeSide at the end of
  // the side, kRange and kFromSpinUp at one that was asked for. None of them
  // ends on a clock, so there is no shape this needs to be kept for and no
  // decision here to leave to the user.
  //
  // Worse than early, it ends quietly. CheckDurationLimit calls StopCapture,
  // and AutoCaptureController::OnCapturingChanged does not act on a capture
  // stopping — so the sequence plays the rest of the side, reaches its own
  // kStopCapture step, finds the capture already stopped, and reports the run
  // as having succeeded. The file is short and nothing says so.
  //
  // Cleared outright rather than warned about, and unlike the sample rate this
  // can always be done: the limit is a number compared against a counter on the
  // statistics tick, re-read every time, so a running stream is no obstacle to
  // changing it. There is no case here that needs the warning label the rate
  // has. Not put back when the window closes either — SetSettings persists it,
  // so the Capture panel shows "No limit" afterwards and says what it will
  // actually do, which is the point.
  CaptureSettings settings = capture_->settings();
  if (settings.duration_limit_seconds == 0) {
    return;
  }
  settings.duration_limit_seconds = 0;
  capture_->SetSettings(settings);
}

void AutoCaptureWizard::ApplyCaptureSettings() {
  if (capture_ == nullptr || loading_) {
    return;
  }

  CaptureSettings settings = capture_->settings();
  settings.capture_directory = directory_edit_->text();
  settings.output_format = static_cast<capture::CaptureOutputFormat>(
      format_combo_->currentData().toInt());
  // The rate and the duration limit are not among these. Neither is a control
  // on this page and neither is a decision — see ForceFullSampleRate and
  // ClearDurationLimit, which settle both when the window opens. Carried
  // through unchanged from the settings this copied.

  if (settings != capture_->settings()) {
    capture_->SetSettings(settings);
  }

  // Both of these change what a capture costs on disk, and the plan form's
  // estimate is a time against a volume — so it has to be worked out again
  // rather than left saying what the previous format would have cost.
  if (plan_form_ != nullptr) {
    plan_form_->SetEditable(!running_);
  }
  RefreshNameNote();
  Refresh();
}

// --- The name ---------------------------------------------------------------

void AutoCaptureWizard::SuggestName() {
  const QString suggested = SuggestedCaptureName(disc_);
  if (suggested.isEmpty()) {
    return;
  }

  // **Resolved against the destination before it is offered.** It is built from
  // what the disc is — "CLV_PAL_Side2" — and so is the same every time that
  // side is captured, unlike the generated name, which carries a timestamp. A
  // prefill that was already taken would put a name in the field that is not
  // the name of the file, which is precisely what a suggestion must never do.
  const CaptureSettings settings =
      capture_ != nullptr ? capture_->settings() : LoadCaptureSettings();

  const capture::CaptureDestination free_name =
      capture::ResolveCaptureDestination(
          std::filesystem::path(
              settings.ResolvedCaptureDirectory().toStdString()),
          suggested.toStdString(), settings.test_mode, std::time(nullptr),
          settings.output_format);

  name_edit_->setText(QString::fromStdString(free_name.stem));
}

void AutoCaptureWizard::RefreshNameNote() {
  // Nothing to say about the generated name: it carries a timestamp, so it is
  // free by construction.
  if (name_edit_->text().trimmed().isEmpty()) {
    name_taken_->hide();
    return;
  }

  const CaptureSettings settings =
      capture_ != nullptr ? capture_->settings() : LoadCaptureSettings();

  const capture::CaptureDestination destination =
      capture::ResolveCaptureDestination(
          std::filesystem::path(
              settings.ResolvedCaptureDirectory().toStdString()),
          name_edit_->text().trimmed().toStdString(), settings.test_mode,
          std::time(nullptr), settings.output_format);

  if (destination.as_requested) {
    name_taken_->hide();
    return;
  }

  name_taken_->setText(
      CaptureNameTakenNote(QString::fromStdString(destination.stem)));
  name_taken_->show();
}

// --- The run ----------------------------------------------------------------

void AutoCaptureWizard::Start() {
  if (running_ || controller_ == nullptr ||
      problem() != player::PlanProblem::kNone) {
    return;
  }

  // The name and the coupling preference are applied to the settings they
  // belong to rather than carried in the plan: they outlive this capture, and
  // the settings dialog shows the same checkbox.
  if (capture_ != nullptr) {
    CaptureSettings settings = capture_->settings();
    settings.capture_name = name_edit_->text().trimmed();
    capture_->SetSettings(settings);
  }

  if (player_ != nullptr && plan_form_ != nullptr) {
    PlayerSettings settings = player_->settings();
    settings.stop_capture_with_player = plan_form_->stop_capture_with_player();
    player_->SetSettings(settings);
  }

  written_path_.clear();
  written_bytes_ = 0;

  running_ = true;
  run_progress_->setRange(0, 0);
  run_status_->setText(
      AutoCaptureStageName(player::AutoCaptureStage::kLockingFrontPanel));
  Refresh();

  controller_->Start(Plan(), disc_);
}

void AutoCaptureWizard::Stop() {
  if (!running_ || controller_ == nullptr) {
    return;
  }

  run_status_->setText(tr("Stopping — the file is being finished properly."));
  stop_button_->setEnabled(false);
  controller_->Cancel();
}

void AutoCaptureWizard::SetRunProgress(player::AutoCaptureStage stage,
                                       int address) {
  if (!running_) {
    return;
  }

  run_status_->setText(AutoCaptureStageName(stage));

  const player::AutoCapturePlan plan = Plan();
  const player::AddressMode mode = plan.addressing;

  if (address < 0 || plan.end_address <= plan.start_address) {
    // Nothing to measure against yet. A busy bar rather than an empty one,
    // because the setting-up is the part that takes seconds with nothing to
    // show for it.
    run_progress_->setRange(0, 0);
    return;
  }

  // The address is the only honest measure of how far a capture has got — the
  // step count is meaningless when one step is repeated for forty minutes.
  run_progress_->setRange(0, plan.end_address - plan.start_address);
  run_progress_->setValue(
      qBound(0, address - plan.start_address, run_progress_->maximum()));

  QString line = tr("%1 — %2").arg(AutoCaptureStageName(stage),
                                   FormatDiscAddress(address, mode));

  // Only while the disc is actually being watched. The address persists through
  // the tail so the bar does not jump back, but a "time left" beside "stopping
  // the player" would be counting down to something that has already happened.
  if (stage == player::AutoCaptureStage::kWatching) {
    const QString remaining = AutoCaptureRemainingText(plan, disc_, address);
    if (!remaining.isEmpty()) {
      line = tr("%1, %2").arg(line, remaining);
    }
  }

  run_status_->setText(line);
}

void AutoCaptureWizard::SetRunResult(player::AutoCaptureOutcome outcome) {
  running_ = false;
  run_progress_->setRange(0, 1);
  run_progress_->setValue(
      outcome == player::AutoCaptureOutcome::kCompleted ? 1 : 0);
  run_status_->setText(AutoCaptureSummary(outcome));

  QString text = AutoCaptureSummary(outcome);
  if (!written_path_.isEmpty()) {
    text = tr("%1\n\nWritten to %2 (%3 MB).")
               .arg(text, written_path_)
               .arg(static_cast<double>(written_bytes_) / (1024.0 * 1024.0), 0,
                    'f', 1);
  }
  summary_->setText(text);

  // Straight to the summary. It is the page that says what happened, and
  // somebody who has left a forty-minute side running wants to find the answer
  // on the screen rather than a Next button to press for it.
  ShowPage(Page::kSummary);
}

void AutoCaptureWizard::CaptureAnotherSide() {
  if (running_ || examining_) {
    return;
  }

  // The side moves on first, so that a disc whose side the player cannot report
  // still gets a different number from the one just captured. An examination
  // that *can* report it overwrites this a moment later, which is the right way
  // round: the disc's own answer wins where there is one.
  if (naming_form_ != nullptr) {
    naming_form_->AdvanceToNextSide();
  }

  written_path_.clear();
  written_bytes_ = 0;
  run_status_->clear();
  run_progress_->setRange(0, 1);
  run_progress_->setValue(0);

  ShowPage(Page::kDisc);
  Examine();
}

// --- Everything the state decides -------------------------------------------

void AutoCaptureWizard::Refresh() {
  const bool live = player_ != nullptr && player_->connection().live();
  const Page current = page();

  // Navigation is locked outright while a run is going. The pages behind
  // describe the capture being taken, and the page ahead reports one that has
  // not happened.
  previous_button_->setEnabled(!running_ && current != Page::kDisc);

  switch (current) {
    case Page::kDisc:
      next_button_->setEnabled(DiscIsUsable());
      next_button_->setToolTip(
          DiscIsUsable()
              ? QString()
              : tr("A capture needs the disc's type and the measured end of "
                   "the side. Examine the disc to establish them."));
      break;
    case Page::kSettings:
      next_button_->setEnabled(problem() == player::PlanProblem::kNone);
      next_button_->setToolTip(problem() == player::PlanProblem::kNone
                                   ? QString()
                                   : PlanProblemText(problem()));
      break;
    case Page::kCapture:
      next_button_->setEnabled(!running_ && !written_path_.isEmpty());
      next_button_->setToolTip(QString());
      break;
    case Page::kSummary:
      next_button_->setEnabled(false);
      next_button_->setToolTip(QString());
      break;
  }

  examine_button_->setEnabled(live && !examining_ && !running_);
  examine_stop_button_->setEnabled(examining_);
  name_edit_->setEnabled(!running_);
  if (naming_form_ != nullptr) {
    naming_form_->setEnabled(!running_);
  }

  if (plan_form_ != nullptr) {
    plan_form_->SetEditable(!running_);
  }
  directory_edit_->setEnabled(!running_);
  browse_button_->setEnabled(!running_);
  format_combo_->setEnabled(!running_);

  start_button_->setEnabled(!running_ && controller_ != nullptr &&
                            problem() == player::PlanProblem::kNone);
  stop_button_->setEnabled(running_);

  another_side_button_->setEnabled(!running_ && !examining_ && live);
}

bool AutoCaptureWizard::RefuseWhileRunning() {
  // This window is the only thing reporting the run: leaving would leave a disc
  // spinning and a file being written with nothing on screen to say so, and no
  // way back to the Stop button.
  if (!running_) {
    return false;
  }

  QMessageBox::information(
      this, tr("Capture in progress"),
      tr("The capture is still running. Stop it first — the file is "
         "finished properly and the player put back, which takes a few "
         "seconds."));
  return true;
}

void AutoCaptureWizard::reject() {
  // Escape, which is its own way out: QDialog::reject() hides the window
  // without raising a close event, so the guard in closeEvent would never see
  // it. One keystroke would otherwise abandon a running capture.
  //
  // Guarded here rather than forwarded to close(). Forwarding looks tidier and
  // is a loop: QDialog::closeEvent calls reject(), so a reject() that called
  // close() would come straight back to itself — and the close would end up
  // refused, which is what a Close button that stopped working looks like.
  if (RefuseWhileRunning()) {
    return;
  }

  QDialog::reject();
}

void AutoCaptureWizard::closeEvent(QCloseEvent* event) {
  if (RefuseWhileRunning()) {
    event->ignore();
    return;
  }

  QDialog::closeEvent(event);
}

}  // namespace ddd::gui
