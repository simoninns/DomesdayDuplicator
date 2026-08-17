/************************************************************************

    capture_naming_dialog.h

    What the disc is, for the file name and for the sidecar
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QDialog>
#include <QHash>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

namespace ddd::gui {

class CaptureController;

// Everything the user knows about the disc, collected in one place.
//
// A dialog rather than more rows in the Capture panel, and that is the whole
// reason it exists. These are eight fields that are set once for a disc and
// then left alone, and the panel they would otherwise sit in shares a dock
// column with the signal displays — a form long enough to need scrolling would
// push the scope and the spectrum into a strip. The panel keeps one button.
//
// What is collected here reaches two places: the capture's sidecar, which takes
// all of it, and the capture's file name, which takes the parts that make sense
// in one. See capture_naming.h for the rules, which are in the engine because a
// command-line capture tool will name its files by the same ones.
//
// **Changes are applied as they are made** rather than on an OK button. There
// is no draft state to lose and nothing here is destructive, so a dialog that
// could be cancelled would only be a second copy of the settings waiting to
// disagree with the first — and the file-name preview at the bottom has to
// track the fields to be worth showing at all.
class CaptureNamingDialog : public QDialog {
  Q_OBJECT

 public:
  explicit CaptureNamingDialog(CaptureController* controller,
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
  static constexpr const char* kClearButtonName = "naming_clear_button";

  // The largest side number the spin box offers. A disc has two sides and a
  // boxed set has a few dozen; a hundred is generous without being absurd, and
  // matches what the old application's per-side holdings were sized for.
  static constexpr int kMaximumDiscSide = 100;

 private slots:
  // Read every widget into the settings, and redraw the preview. Called on any
  // change: there is no apply button, so this is the apply.
  void Apply();

  // Empty every field and untick every box, for the next disc.
  void ClearAllFields();

  // Stash the notes and mint marks under the side they were typed for, and
  // fetch whatever was typed for the side just selected.
  void ChangeSide(int side);

 private:
  void BuildWidgets();
  void ShowSettings();

  // Grey out the value beside a box that is not ticked, so an unticked field
  // reads as absent rather than as empty.
  void UpdateEnabledState();

  // What the next capture will be called, given everything in this dialog.
  void UpdatePreview();

  CaptureController* controller_ = nullptr;

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
  QPushButton* clear_button_ = nullptr;

  // What was typed for each side, when the per-side options are on. Held for
  // the life of the dialog rather than persisted: it is a convenience for
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
