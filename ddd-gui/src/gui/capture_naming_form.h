/************************************************************************

    capture_naming_form.h

    What the disc is, for the file name and for the sidecar
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QHash>
#include <QString>
#include <QWidget>

// Included rather than forward declared, and it has to be: moc generates code
// for the slot below, and a queued-connection type whose Q_DECLARE_METATYPE is
// not yet visible gets the primary template instantiated instead.
#include "player_metatypes.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

namespace ddd::gui {

class CaptureController;
class PlayerController;

// Everything the user knows about the disc, collected in one place.
//
// A widget rather than a dialog, because there are two places that ask for
// these fields and they must not be two forms. CaptureNamingDialog is a shell
// around this one — the manual path, reached from the Capture panel's Naming…
// button — and the automatic capture asks the same questions on its first page,
// prefilled from what the examination found. Two forms would be two sets of
// rules about which field reaches a file name, and they would drift.
//
// What is collected here reaches two places: the capture's sidecar, which takes
// all of it, and the capture's file name, which takes the parts that make sense
// in one. See capture_naming.h for the rules, which are in the engine because a
// command-line capture tool will name its files by the same ones.
//
// **Changes are applied as they are made** rather than on an OK button. There
// is no draft state to lose and nothing here is destructive, so a form that
// could be cancelled would only be a second copy of the settings waiting to
// disagree with the first — and the file-name preview at the bottom has to
// track the fields to be worth showing at all. It also means an embedding page
// has nothing to collect: by the time it moves on, the settings already say
// what the form says.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class CaptureNamingForm : public QWidget {
  Q_OBJECT

 public:
  // Either controller may be null, and the widget tests pass null
  // deliberately: every field then builds and lays out as it does in the
  // application and writes nowhere.
  //
  // With no player there is no "Ask the player" button — absent rather than
  // disabled, because a build with no player support has nothing to ask and a
  // greyed-out button would invite somebody to look for the setting that turns
  // it on. Whether a player is *connected* is a different question, and that
  // one does grey the button out.
  explicit CaptureNamingForm(CaptureController* controller,
                             PlayerController* player = nullptr,
                             QWidget* parent = nullptr);

  // Named so the widget tests can find them without depending on layout order.
  static constexpr const char* kTitleCheckName = "naming_title_check";
  static constexpr const char* kTitleEditName = "naming_title_edit";
  static constexpr const char* kDiscTypeCheckName = "naming_disc_type_check";
  static constexpr const char* kDiscTypeComboName = "naming_disc_type_combo";
  static constexpr const char* kStandardCheckName = "naming_standard_check";
  static constexpr const char* kStandardComboName = "naming_standard_combo";
  static constexpr const char* kAudioCheckName = "naming_audio_check";
  static constexpr const char* kAudioComboName = "naming_audio_combo";
  static constexpr const char* kSideCheckName = "naming_side_check";
  static constexpr const char* kSideSpinName = "naming_side_spin";
  static constexpr const char* kNotesCheckName = "naming_notes_check";
  static constexpr const char* kNotesEditName = "naming_notes_edit";
  static constexpr const char* kMintCheckName = "naming_mint_check";
  static constexpr const char* kMintEditName = "naming_mint_edit";
  static constexpr const char* kMetadataNotesEditName =
      "naming_metadata_notes_edit";
  static constexpr const char* kMetadataInNameCheckName =
      "naming_metadata_in_name_check";
  static constexpr const char* kAppendDurationCheckName =
      "naming_append_duration_check";
  static constexpr const char* kPerSideNotesCheckName =
      "naming_per_side_notes_check";
  static constexpr const char* kPerSideMintCheckName =
      "naming_per_side_mint_check";
  static constexpr const char* kPreviewLabelName = "naming_preview_label";
  static constexpr const char* kAskButtonName = "naming_ask_button";
  static constexpr const char* kAskStatusLabelName = "naming_ask_status_label";

  // The largest side number the spin box offers. A disc has two sides and a
  // boxed set has a few dozen; a hundred is generous without being absurd, and
  // matches what the old application's per-side holdings were sized for.
  static constexpr int kMaximumDiscSide = 100;

 public slots:
  // Empty every field and untick every box, for the next disc.
  //
  // Public because the button that does it belongs to whatever is showing the
  // form — a dialog puts it in its button box, a wizard page beside its own
  // navigation — while what it clears belongs here.
  void ClearAllFields();

  // Fill in what an examination established, and tick those fields.
  //
  // **Only the fields the profile actually knows, and nothing else.** A fact
  // the player could not report leaves its field exactly as it was, and no
  // amount of examining ever touches the title, the notes or the mint marks:
  // those are things only a person can know, and overwriting what somebody has
  // typed because a disc was spun up would be the worst thing this button could
  // do.
  //
  // Public because two places have a profile to fill from — this form's own
  // "Ask the player" button, and the automatic capture, which has examined the
  // disc in full before it ever shows these fields.
  void FillFromProfile(const ddd::player::DiscProfile& disc);

  // Move on to the next side of the same disc.
  //
  // Only where a side is being recorded at all — somebody who has not ticked
  // the box is not filing by side, and turning the option on for them would be
  // deciding how they file. It also ticks nothing new: what it is for is the
  // gap between capturing side 1 and capturing side 2, where the number is
  // already in use.
  void AdvanceToNextSide();

 private slots:
  // Read every widget into the settings, and redraw the preview. Called on any
  // change: there is no apply button, so this is the apply.
  void Apply();

  // Stash the notes and mint marks under the side they were typed for, and
  // fetch whatever was typed for the side just selected.
  void ChangeSide(int side);

 private:
  void BuildWidgets();
  void ShowSettings();

  // Start an identifying examination, and say that it is running.
  void AskThePlayer();

  // Whether the button can be pressed: a player has to be connected, and one
  // question at a time.
  void UpdateAskState();

  CaptureController* controller_ = nullptr;
  PlayerController* player_ = nullptr;

  // True between pressing Ask and the answer arriving. The controller's examine
  // signals are a broadcast — the examine window may be listening to the same
  // ones — so without this the form would fill itself in from an examination
  // somebody else started.
  bool asking_ = false;

  // Grey out the value beside a box that is not ticked, so an unticked field
  // reads as absent rather than as empty.
  void UpdateEnabledState();

  // What the next capture will be called, given everything in this form.
  void UpdatePreview();

  QPushButton* ask_button_ = nullptr;
  QLabel* ask_status_ = nullptr;

  QCheckBox* title_check_ = nullptr;
  QLineEdit* title_edit_ = nullptr;
  QCheckBox* disc_type_check_ = nullptr;
  QComboBox* disc_type_combo_ = nullptr;
  QCheckBox* standard_check_ = nullptr;
  QComboBox* standard_combo_ = nullptr;
  QCheckBox* audio_check_ = nullptr;
  QComboBox* audio_combo_ = nullptr;
  QCheckBox* side_check_ = nullptr;
  QSpinBox* side_spin_ = nullptr;
  QCheckBox* notes_check_ = nullptr;
  QLineEdit* notes_edit_ = nullptr;
  QCheckBox* mint_check_ = nullptr;
  QLineEdit* mint_edit_ = nullptr;
  QPlainTextEdit* metadata_notes_edit_ = nullptr;

  QCheckBox* metadata_in_name_check_ = nullptr;
  QCheckBox* append_duration_check_ = nullptr;
  QCheckBox* per_side_notes_check_ = nullptr;
  QCheckBox* per_side_mint_check_ = nullptr;

  QLabel* preview_ = nullptr;

  // What was typed for each side, when the per-side options are on. Held for
  // the life of the form rather than persisted: it is a convenience for
  // somebody working through the sides of one disc in one sitting, and text
  // restored a week later against a different disc would be worse than none.
  QHash<int, QString> notes_by_side_;
  QHash<int, QString> mint_by_side_;

  // Which side the two boxes above currently hold the text for.
  int current_side_ = 1;

  // True while the widgets are being filled from the settings, so the change
  // signals they emit do not write those same settings straight back.
  bool loading_ = false;
};

}  // namespace ddd::gui
