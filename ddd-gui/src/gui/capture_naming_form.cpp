/************************************************************************

    capture_naming_form.cpp

    What the disc is, for the file name and for the sidecar
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_naming_form.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QVBoxLayout>
#include <ctime>

#include "capture_controller.h"
#include "capture_format.h"
#include "capture_metadata.h"
#include "capture_naming.h"
#include "player_controller.h"
#include "player_text.h"

namespace ddd::gui {
namespace {

// A drop-down rather than the old application's row of radio buttons.
//
// The same choices either way, and the check box beside it already carries the
// "not stated" case — which is what the radio buttons could not express, since
// a group of them with none selected looks like a defect. Three drop-downs also
// fit a form layout that has eight rows in it, where three rows of radios do
// not.
void FillDiscTypes(QComboBox* combo) {
  combo->addItem(QObject::tr("CAV"),
                 static_cast<int>(capture::DiscTypeChoice::kCav));
  combo->addItem(QObject::tr("CLV"),
                 static_cast<int>(capture::DiscTypeChoice::kClv));
}

void FillVideoStandards(QComboBox* combo) {
  combo->addItem(QObject::tr("NTSC"),
                 static_cast<int>(capture::VideoStandardChoice::kNtsc));
  combo->addItem(QObject::tr("PAL"),
                 static_cast<int>(capture::VideoStandardChoice::kPal));
}

void FillAudioTypes(QComboBox* combo) {
  combo->addItem(QObject::tr("Default"),
                 static_cast<int>(capture::AudioTypeChoice::kDefault));
  combo->addItem(QObject::tr("Analogue"),
                 static_cast<int>(capture::AudioTypeChoice::kAnalogue));
  combo->addItem(QObject::tr("AC3"),
                 static_cast<int>(capture::AudioTypeChoice::kAc3));
  combo->addItem(QObject::tr("DTS"),
                 static_cast<int>(capture::AudioTypeChoice::kDts));
}

// Select the entry carrying `value`, leaving the box alone when nothing does.
void SelectChoice(QComboBox* combo, int value) {
  const int index = combo->findData(value);
  if (index >= 0) {
    combo->setCurrentIndex(index);
  }
}

// What to say once the player has been asked.
//
// The fields that were filled are named rather than merely counted, because
// this is a button that reaches in and changes what somebody typed: saying
// exactly what it touched is what makes it safe to press. An examination that
// established one thing has ticked one box, and that is worth being told
// plainly rather than left to be spotted.
QString AskOutcomeNote(const player::DiscProfile& disc,
                       player::ExamineOutcome outcome) {
  if (outcome != player::ExamineOutcome::kCompleted) {
    return QObject::tr("The player was asked and the examination %1.")
        .arg(ExamineOutcomeText(outcome));
  }

  QStringList filled;
  if (disc.disc_type.known()) {
    filled.append(QObject::tr("the disc type"));
  }
  if (disc.video_standard.known()) {
    filled.append(QObject::tr("the video standard"));
  }
  if (disc.disc_side.known()) {
    filled.append(QObject::tr("the side"));
  }

  if (filled.isEmpty()) {
    // A real answer rather than a failure. Some models report none of these,
    // and saying so is better than a blank line that reads as a button which
    // did nothing.
    return QObject::tr(
        "The player answered, but said nothing about this disc that these "
        "fields cover. Everything here is still yours to fill in.");
  }

  return QObject::tr(
             "Filled in %1 from the disc. Everything else is untouched.")
      .arg(filled.join(QObject::tr(", ")));
}

}  // namespace

CaptureNamingForm::CaptureNamingForm(CaptureController* controller,
                                     PlayerController* player, QWidget* parent)
    : QWidget(parent), controller_(controller), player_(player) {
  BuildWidgets();
  ShowSettings();

  if (player_ != nullptr) {
    connect(player_, &PlayerController::ExamineProgress, this,
            [this](player::ExamineStage stage, int, int) {
              if (asking_) {
                ask_status_->setText(ExamineStageName(stage));
              }
            });

    connect(player_, &PlayerController::ExamineFinished, this,
            [this](const player::DiscProfile& disc,
                   player::ExamineOutcome outcome) {
              if (!asking_) {
                return;
              }
              asking_ = false;

              if (outcome == player::ExamineOutcome::kCompleted) {
                FillFromProfile(disc);
              }

              // Said whatever happened, including when it worked: an
              // examination that found a CLV disc and nothing else has ticked
              // one box, and somebody who pressed a button deserves to be told
              // that is all there was rather than left to spot it.
              ask_status_->setText(AskOutcomeNote(disc, outcome));
              UpdateAskState();
            });

    // A player unplugged mid-question, or plugged in while the dialog is open.
    connect(player_, &PlayerController::ConnectionChanged, this,
            [this](const PlayerConnection&) { UpdateAskState(); });
  }

  UpdateAskState();
}

void CaptureNamingForm::BuildWidgets() {
  auto* layout = new QVBoxLayout(this);

  // No margins of its own: this is a form to be placed inside something, and
  // whatever places it owns the spacing around it.
  layout->setContentsMargins(0, 0, 0, 0);

  auto* intro = new QLabel(
      tr("What this disc is. All of it is recorded in the capture's metadata "
         "file; the parts that make sense in one can also be folded into the "
         "file name."),
      this);
  intro->setWordWrap(true);
  layout->addWidget(intro);

  // --- Asking the player instead of typing ---------------------------------

  // Built only where there is a player layer at all. Three of the fields below
  // are things the disc itself can be asked, and asking takes a few seconds
  // where typing takes a minute and can be got wrong.
  if (player_ != nullptr) {
    auto* ask_row = new QWidget(this);
    auto* ask_layout = new QHBoxLayout(ask_row);
    ask_layout->setContentsMargins(0, 0, 0, 0);

    ask_button_ = new QPushButton(tr("Ask the player"), ask_row);
    ask_button_->setObjectName(QLatin1String(kAskButtonName));
    ask_button_->setToolTip(
        tr("Spin the disc up and read what it says about itself: its type, its "
           "video standard and which side is loaded. It takes a few seconds, "
           "leaves the disc where it started, and fills in only what the "
           "player actually reports — nothing you have typed is touched."));
    ask_layout->addWidget(ask_button_);

    ask_status_ = new QLabel(ask_row);
    ask_status_->setObjectName(QLatin1String(kAskStatusLabelName));
    ask_status_->setWordWrap(true);
    ask_layout->addWidget(ask_status_, 1);

    layout->addWidget(ask_row);

    connect(ask_button_, &QPushButton::clicked, this,
            &CaptureNamingForm::AskThePlayer);
  }

  // --- The disc ------------------------------------------------------------

  auto* disc_box = new QGroupBox(tr("The disc"), this);
  auto* disc_form = new QFormLayout(disc_box);
  disc_form->setLabelAlignment(Qt::AlignLeft);

  // Each row is a check box carrying the field's name, then the value. The box
  // is what says whether the field was established at all: an empty title and a
  // title nobody was asked for are different facts, and a line edit alone
  // cannot tell them apart.
  title_check_ = new QCheckBox(tr("Title"), disc_box);
  title_check_->setObjectName(QLatin1String(kTitleCheckName));
  title_edit_ = new QLineEdit(disc_box);
  title_edit_->setObjectName(QLatin1String(kTitleEditName));
  title_edit_->setPlaceholderText(tr("The title on the sleeve"));
  disc_form->addRow(title_check_, title_edit_);

  disc_type_check_ = new QCheckBox(tr("Disc type"), disc_box);
  disc_type_check_->setObjectName(QLatin1String(kDiscTypeCheckName));
  disc_type_combo_ = new QComboBox(disc_box);
  disc_type_combo_->setObjectName(QLatin1String(kDiscTypeComboName));
  FillDiscTypes(disc_type_combo_);
  disc_form->addRow(disc_type_check_, disc_type_combo_);

  standard_check_ = new QCheckBox(tr("Video standard"), disc_box);
  standard_check_->setObjectName(QLatin1String(kStandardCheckName));
  standard_combo_ = new QComboBox(disc_box);
  standard_combo_->setObjectName(QLatin1String(kStandardComboName));
  FillVideoStandards(standard_combo_);
  disc_form->addRow(standard_check_, standard_combo_);

  audio_check_ = new QCheckBox(tr("Audio"), disc_box);
  audio_check_->setObjectName(QLatin1String(kAudioCheckName));
  audio_combo_ = new QComboBox(disc_box);
  audio_combo_->setObjectName(QLatin1String(kAudioComboName));
  FillAudioTypes(audio_combo_);
  audio_combo_->setToolTip(tr(
      "What the disc carries as its digital audio. Default is a real answer "
      "rather than a way of not saying — it means the disc's own default "
      "tracks — and it is the one choice that adds nothing to a file name."));
  disc_form->addRow(audio_check_, audio_combo_);

  side_check_ = new QCheckBox(tr("Side"), disc_box);
  side_check_->setObjectName(QLatin1String(kSideCheckName));
  side_spin_ = new QSpinBox(disc_box);
  side_spin_->setObjectName(QLatin1String(kSideSpinName));
  side_spin_->setRange(1, kMaximumDiscSide);
  side_spin_->setToolTip(
      tr("The side joins the file name whether or not the other details do. "
         "The two files somebody makes in a row are the two sides of one disc, "
         "and telling them apart afterwards is the whole problem."));
  disc_form->addRow(side_check_, side_spin_);

  notes_check_ = new QCheckBox(tr("Notes"), disc_box);
  notes_check_->setObjectName(QLatin1String(kNotesCheckName));
  notes_edit_ = new QLineEdit(disc_box);
  notes_edit_->setObjectName(QLatin1String(kNotesEditName));
  disc_form->addRow(notes_check_, notes_edit_);

  mint_check_ = new QCheckBox(tr("Mint marks"), disc_box);
  mint_check_->setObjectName(QLatin1String(kMintCheckName));
  mint_edit_ = new QLineEdit(disc_box);
  mint_edit_->setObjectName(QLatin1String(kMintEditName));

  // Letters, digits, underscores, hyphens and single spaces between them. The
  // old application's rule, kept because these end up in file names on three
  // platforms and this is the character set all three agree about.
  mint_edit_->setValidator(new QRegularExpressionValidator(
      QRegularExpression(QStringLiteral("^[a-zA-Z0-9_-]+( [a-zA-Z0-9_-]+)*$")),
      mint_edit_));
  mint_edit_->setToolTip(
      tr("The condition of the disc, in whatever shorthand you already use."));
  disc_form->addRow(mint_check_, mint_edit_);

  auto* notes_label = new QLabel(tr("Metadata notes"), disc_box);
  metadata_notes_edit_ = new QPlainTextEdit(disc_box);
  metadata_notes_edit_->setObjectName(QLatin1String(kMetadataNotesEditName));
  metadata_notes_edit_->setPlaceholderText(
      tr("Anything worth recording about this capture. Written to the "
         "metadata file only — never to the file name."));
  metadata_notes_edit_->setMinimumHeight(70);
  disc_form->addRow(notes_label, metadata_notes_edit_);

  layout->addWidget(disc_box);

  // --- The file name -------------------------------------------------------

  auto* name_box = new QGroupBox(tr("The file name"), this);
  auto* name_layout = new QVBoxLayout(name_box);

  metadata_in_name_check_ =
      new QCheckBox(tr("Include the disc details in the file name"), name_box);
  metadata_in_name_check_->setObjectName(
      QLatin1String(kMetadataInNameCheckName));
  metadata_in_name_check_->setToolTip(
      tr("Adds the type, the standard, the audio, the notes and the mint "
         "marks to the name. The title and the side are in it either way."));
  name_layout->addWidget(metadata_in_name_check_);

  append_duration_check_ = new QCheckBox(
      tr("Append the capture's length when it finishes"), name_box);
  append_duration_check_->setObjectName(
      QLatin1String(kAppendDurationCheckName));
  append_duration_check_->setToolTip(
      tr("The length is not known until the capture has stopped, so the file "
         "is renamed at that point — from “disc.ddd.flac” to "
         "“disc_00H41M12S.ddd.flac”. The metadata file is written "
         "beside it under the new name."));
  name_layout->addWidget(append_duration_check_);

  preview_ = new QLabel(name_box);
  preview_->setObjectName(QLatin1String(kPreviewLabelName));
  preview_->setWordWrap(true);
  preview_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  name_layout->addWidget(preview_);

  layout->addWidget(name_box);

  // --- Working through a disc ----------------------------------------------

  auto* sides_box = new QGroupBox(tr("Capturing several sides"), this);
  auto* sides_layout = new QVBoxLayout(sides_box);

  per_side_notes_check_ =
      new QCheckBox(tr("Keep separate notes for each side"), sides_box);
  per_side_notes_check_->setObjectName(QLatin1String(kPerSideNotesCheckName));
  sides_layout->addWidget(per_side_notes_check_);

  per_side_mint_check_ =
      new QCheckBox(tr("Keep separate mint marks for each side"), sides_box);
  per_side_mint_check_->setObjectName(QLatin1String(kPerSideMintCheckName));
  sides_layout->addWidget(per_side_mint_check_);

  // What they do is remember, not clear. Changing the side number puts back
  // whatever was typed for that side earlier in this session; nothing is
  // carried between sessions, because text restored a week later against a
  // different disc would be worse than none.
  const QString sides_note =
      tr("Changing the side number then swaps in what you typed for that side "
         "earlier in this session.");
  per_side_notes_check_->setToolTip(sides_note);
  per_side_mint_check_->setToolTip(sides_note);

  layout->addWidget(sides_box);

  // Every control applies as it changes — see the class comment. Listed one by
  // one rather than swept up by findChildren so that a control added later has
  // to be thought about rather than silently inheriting this.
  const auto on_check = [this](QCheckBox* box) {
    connect(box, &QCheckBox::toggled, this, [this](bool) { Apply(); });
  };
  on_check(title_check_);
  on_check(disc_type_check_);
  on_check(standard_check_);
  on_check(audio_check_);
  on_check(side_check_);
  on_check(notes_check_);
  on_check(mint_check_);
  on_check(metadata_in_name_check_);
  on_check(append_duration_check_);
  on_check(per_side_notes_check_);
  on_check(per_side_mint_check_);

  const auto on_edit = [this](QLineEdit* edit) {
    connect(edit, &QLineEdit::textChanged, this,
            [this](const QString&) { Apply(); });
  };
  on_edit(title_edit_);
  on_edit(notes_edit_);
  on_edit(mint_edit_);

  connect(metadata_notes_edit_, &QPlainTextEdit::textChanged, this,
          &CaptureNamingForm::Apply);

  const auto on_combo = [this](QComboBox* combo) {
    connect(combo, &QComboBox::currentIndexChanged, this,
            [this](int) { Apply(); });
  };
  on_combo(disc_type_combo_);
  on_combo(standard_combo_);
  on_combo(audio_combo_);

  // The side is the one control with something to do beyond applying: it moves
  // the per-side text across before the new value is read.
  connect(side_spin_, &QSpinBox::valueChanged, this,
          &CaptureNamingForm::ChangeSide);
}

void CaptureNamingForm::ShowSettings() {
  if (controller_ == nullptr) {
    UpdateEnabledState();
    UpdatePreview();
    return;
  }

  loading_ = true;
  const capture::CaptureNamingFields& fields = controller_->settings().naming;

  title_check_->setChecked(fields.title_used);
  title_edit_->setText(QString::fromStdString(fields.title));

  disc_type_check_->setChecked(fields.disc_type_used);
  SelectChoice(disc_type_combo_, static_cast<int>(fields.disc_type));

  standard_check_->setChecked(fields.video_standard_used);
  SelectChoice(standard_combo_, static_cast<int>(fields.video_standard));

  audio_check_->setChecked(fields.audio_used);
  SelectChoice(audio_combo_, static_cast<int>(fields.audio));

  side_check_->setChecked(fields.side_used);
  side_spin_->setValue(fields.side);
  current_side_ = fields.side;

  notes_check_->setChecked(fields.notes_used);
  notes_edit_->setText(QString::fromStdString(fields.notes));

  mint_check_->setChecked(fields.mint_marks_used);
  mint_edit_->setText(QString::fromStdString(fields.mint_marks));

  metadata_notes_edit_->setPlainText(
      QString::fromStdString(fields.metadata_notes));

  metadata_in_name_check_->setChecked(fields.metadata_in_name);
  append_duration_check_->setChecked(fields.append_duration);
  per_side_notes_check_->setChecked(fields.per_side_notes);
  per_side_mint_check_->setChecked(fields.per_side_mint_marks);

  loading_ = false;

  UpdateEnabledState();
  UpdatePreview();
}

void CaptureNamingForm::Apply() {
  UpdateEnabledState();

  if (controller_ == nullptr || loading_) {
    UpdatePreview();
    return;
  }

  CaptureSettings settings = controller_->settings();
  capture::CaptureNamingFields& fields = settings.naming;

  fields.title_used = title_check_->isChecked();
  fields.title = title_edit_->text().toStdString();

  // The three drop-downs always have something selected — a list with nothing
  // chosen looks like a defect — so the *value* of an unticked one is taken as
  // unset rather than as whatever happens to be showing. Without this, a field
  // nobody ticked would still record "CAV" in the settings file, which is the
  // one case the unset state exists to keep apart from an answer.
  fields.disc_type_used = disc_type_check_->isChecked();
  fields.disc_type = fields.disc_type_used
                         ? static_cast<capture::DiscTypeChoice>(
                               disc_type_combo_->currentData().toInt())
                         : capture::DiscTypeChoice::kUnset;

  fields.video_standard_used = standard_check_->isChecked();
  fields.video_standard = fields.video_standard_used
                              ? static_cast<capture::VideoStandardChoice>(
                                    standard_combo_->currentData().toInt())
                              : capture::VideoStandardChoice::kUnset;

  fields.audio_used = audio_check_->isChecked();
  fields.audio = fields.audio_used ? static_cast<capture::AudioTypeChoice>(
                                         audio_combo_->currentData().toInt())
                                   : capture::AudioTypeChoice::kUnset;

  fields.side_used = side_check_->isChecked();
  fields.side = side_spin_->value();

  fields.notes_used = notes_check_->isChecked();
  fields.notes = notes_edit_->text().toStdString();

  fields.mint_marks_used = mint_check_->isChecked();
  fields.mint_marks = mint_edit_->text().toStdString();

  fields.metadata_notes = metadata_notes_edit_->toPlainText().toStdString();
  fields.metadata_in_name = metadata_in_name_check_->isChecked();
  fields.append_duration = append_duration_check_->isChecked();
  fields.per_side_notes = per_side_notes_check_->isChecked();
  fields.per_side_mint_marks = per_side_mint_check_->isChecked();

  if (settings != controller_->settings()) {
    controller_->SetSettings(settings);
  }

  UpdatePreview();
}

void CaptureNamingForm::AskThePlayer() {
  if (player_ == nullptr || asking_ || !player_->connection().live()) {
    return;
  }

  asking_ = true;
  ask_status_->setText(ExamineStageName(player::ExamineStage::kCheckingPlayer));
  UpdateAskState();

  // The identifying scope, not the full one. What these fields want is what the
  // disc says about itself; measuring the length would take a minute, move the
  // disc, and answer nothing that is asked for here.
  player_->Examine(player::ExamineScope::kIdentify);
}

void CaptureNamingForm::UpdateAskState() {
  if (ask_button_ == nullptr) {
    return;
  }

  const bool live = player_ != nullptr && player_->connection().live();
  ask_button_->setEnabled(live && !asking_);

  if (!live && !asking_) {
    // Said rather than left to the greyed-out button to imply, because "no
    // player is connected" and "this build cannot ask" are different problems
    // with different remedies.
    ask_status_->setText(tr("No player is connected."));
  }
}

void CaptureNamingForm::FillFromProfile(const player::DiscProfile& disc) {
  // Straight through the widgets, and every one of them guarded by whether the
  // examination actually established it. A field the player could not report is
  // left exactly as it was — including left unticked, which is the honest state
  // for a fact nobody has established.
  //
  // Nothing here touches the title, the notes, the mint marks or the metadata
  // notes. Those are things only a person knows, and a button that overwrote
  // them because a disc was spun up would be unusable.
  loading_ = true;

  if (disc.disc_type.known()) {
    disc_type_check_->setChecked(true);
    SelectChoice(disc_type_combo_,
                 static_cast<int>(disc.disc_type.value == player::DiscType::kClv
                                      ? capture::DiscTypeChoice::kClv
                                      : capture::DiscTypeChoice::kCav));
  }

  if (disc.video_standard.known()) {
    standard_check_->setChecked(true);
    SelectChoice(standard_combo_,
                 static_cast<int>(disc.video_standard.value ==
                                          player::VideoStandard::kPal
                                      ? capture::VideoStandardChoice::kPal
                                      : capture::VideoStandardChoice::kNtsc));
  }

  // Bounded, because the spin box is: a player reporting a side number outside
  // what a boxed set could have is one this application should not follow into
  // a field it cannot represent.
  if (disc.disc_side.known() && disc.disc_side.value >= 1 &&
      disc.disc_side.value <= kMaximumDiscSide) {
    side_check_->setChecked(true);
    side_spin_->setValue(disc.disc_side.value);
    current_side_ = disc.disc_side.value;
  }

  loading_ = false;

  // Once, at the end, rather than per field: the settings are written whole and
  // a preview redrawn three times would be two redraws wasted.
  Apply();
}

void CaptureNamingForm::AdvanceToNextSide() {
  if (!side_check_->isChecked() || side_spin_->value() >= kMaximumDiscSide) {
    return;
  }

  // Through the spin box, so the per-side notes and mint marks move across with
  // it exactly as they do when somebody changes the number by hand.
  side_spin_->setValue(side_spin_->value() + 1);
}

void CaptureNamingForm::ClearAllFields() {
  // Straight to a default-constructed set rather than clearing widget by
  // widget: a field added later is then cleared by this without anybody having
  // to remember, which is the failure mode a "clear everything" button has.
  loading_ = true;

  const capture::CaptureNamingFields empty;

  title_check_->setChecked(empty.title_used);
  title_edit_->clear();
  disc_type_check_->setChecked(empty.disc_type_used);
  disc_type_combo_->setCurrentIndex(0);
  standard_check_->setChecked(empty.video_standard_used);
  standard_combo_->setCurrentIndex(0);
  audio_check_->setChecked(empty.audio_used);
  audio_combo_->setCurrentIndex(0);
  side_check_->setChecked(empty.side_used);
  side_spin_->setValue(empty.side);
  notes_check_->setChecked(empty.notes_used);
  notes_edit_->clear();
  mint_check_->setChecked(empty.mint_marks_used);
  mint_edit_->clear();
  metadata_notes_edit_->clear();
  metadata_in_name_check_->setChecked(empty.metadata_in_name);
  append_duration_check_->setChecked(empty.append_duration);

  // The per-side text goes with them. Keeping it would put the previous disc's
  // notes back the moment somebody selected side 2, which is exactly the thing
  // this button exists to prevent.
  notes_by_side_.clear();
  mint_by_side_.clear();
  current_side_ = empty.side;

  loading_ = false;

  // The two per-side preferences are deliberately left alone. They are a way of
  // working rather than a fact about a disc, and somebody who turned them on
  // for this session means them for the next disc too.
  Apply();
}

void CaptureNamingForm::ChangeSide(int side) {
  if (!loading_) {
    if (per_side_notes_check_->isChecked()) {
      notes_by_side_[current_side_] = notes_edit_->text();
      notes_edit_->setText(notes_by_side_.value(side));
    }
    if (per_side_mint_check_->isChecked()) {
      mint_by_side_[current_side_] = mint_edit_->text();
      mint_edit_->setText(mint_by_side_.value(side));
    }
  }

  current_side_ = side;
  Apply();
}

void CaptureNamingForm::UpdateEnabledState() {
  title_edit_->setEnabled(title_check_->isChecked());
  disc_type_combo_->setEnabled(disc_type_check_->isChecked());
  standard_combo_->setEnabled(standard_check_->isChecked());
  audio_combo_->setEnabled(audio_check_->isChecked());
  side_spin_->setEnabled(side_check_->isChecked());
  notes_edit_->setEnabled(notes_check_->isChecked());
  mint_edit_->setEnabled(mint_check_->isChecked());

  // Off unless there are notes or mint marks to keep, since without either
  // there is nothing per-side to hold.
  per_side_notes_check_->setEnabled(notes_check_->isChecked());
  per_side_mint_check_->setEnabled(mint_check_->isChecked());
}

void CaptureNamingForm::UpdatePreview() {
  if (controller_ == nullptr) {
    preview_->clear();
    return;
  }

  const CaptureSettings& settings = controller_->settings();
  const std::string stem = settings.CaptureStem(std::time(nullptr));
  const QString file_name = QString::fromStdString(
      capture::AddCaptureFileSuffix(stem, settings.output_format).string());

  // The sidecar is named beside it, so it is shown beside it. A user who has
  // just been told their capture will carry eight fields of metadata should be
  // able to see where those fields are going to end up.
  const QString sidecar = QString::fromStdString(
      capture::CaptureMetadataPath(file_name.toStdString())
          .filename()
          .string());

  if (!settings.capture_name.trimmed().isEmpty()) {
    // The typed name wins, and the fields above then reach the metadata file
    // only. Said plainly here rather than left to be discovered: a user who
    // ticks five boxes and sees none of them in the name would reasonably
    // conclude the boxes do not work.
    preview_->setText(
        tr("The next capture will be called %1, with its metadata in %2. The "
           "name comes from the Capture panel's Name field; clear that field "
           "to build the name from the details above instead.")
            .arg(file_name, sidecar));
    return;
  }

  if (settings.test_mode) {
    preview_->setText(
        tr("Test data mode is on, so the next capture will be called %1 "
           "whatever is set here — a file of ramps must never carry a disc's "
           "name. The details above still reach %2.")
            .arg(file_name, sidecar));
    return;
  }

  preview_->setText(tr("The next capture will be called %1, with its metadata "
                       "in %2.")
                        .arg(file_name, sidecar));
}

}  // namespace ddd::gui
