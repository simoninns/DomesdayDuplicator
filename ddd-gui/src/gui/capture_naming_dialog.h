/************************************************************************

    capture_naming_dialog.h

    What the disc is, for the file name and for the sidecar
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QDialog>

#include "capture_naming_form.h"

namespace ddd::gui {

class CaptureController;

// The manual path's way in to the naming fields.
//
// A dialog rather than more rows in the Capture panel, and that is the whole
// reason it exists. These are eight fields that are set once for a disc and
// then left alone, and the panel they would otherwise sit in shares a dock
// column with the signal displays — a form long enough to need scrolling would
// push the scope and the spectrum into a strip. The panel keeps one button.
//
// The fields themselves are CaptureNamingForm, which the automatic capture
// shows on its own first page. Everything about what a field means, when it
// applies and what the name comes out as is there; this is a frame around it
// with a way to close.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class CaptureNamingDialog : public QDialog {
  Q_OBJECT

 public:
  explicit CaptureNamingDialog(CaptureController* controller,
                               QWidget* parent = nullptr);

  // The form's own names, carried here so that anything holding this dialog —
  // the widget tests especially — need not know which of the two objects a
  // given control was built by.
  static constexpr const char* kTitleCheckName =
      CaptureNamingForm::kTitleCheckName;
  static constexpr const char* kTitleEditName =
      CaptureNamingForm::kTitleEditName;
  static constexpr const char* kDiscTypeCheckName =
      CaptureNamingForm::kDiscTypeCheckName;
  static constexpr const char* kDiscTypeComboName =
      CaptureNamingForm::kDiscTypeComboName;
  static constexpr const char* kStandardCheckName =
      CaptureNamingForm::kStandardCheckName;
  static constexpr const char* kStandardComboName =
      CaptureNamingForm::kStandardComboName;
  static constexpr const char* kAudioCheckName =
      CaptureNamingForm::kAudioCheckName;
  static constexpr const char* kAudioComboName =
      CaptureNamingForm::kAudioComboName;
  static constexpr const char* kSideCheckName =
      CaptureNamingForm::kSideCheckName;
  static constexpr const char* kSideSpinName = CaptureNamingForm::kSideSpinName;
  static constexpr const char* kNotesCheckName =
      CaptureNamingForm::kNotesCheckName;
  static constexpr const char* kNotesEditName =
      CaptureNamingForm::kNotesEditName;
  static constexpr const char* kMintCheckName =
      CaptureNamingForm::kMintCheckName;
  static constexpr const char* kMintEditName = CaptureNamingForm::kMintEditName;
  static constexpr const char* kMetadataNotesEditName =
      CaptureNamingForm::kMetadataNotesEditName;
  static constexpr const char* kMetadataInNameCheckName =
      CaptureNamingForm::kMetadataInNameCheckName;
  static constexpr const char* kAppendDurationCheckName =
      CaptureNamingForm::kAppendDurationCheckName;
  static constexpr const char* kPerSideNotesCheckName =
      CaptureNamingForm::kPerSideNotesCheckName;
  static constexpr const char* kPerSideMintCheckName =
      CaptureNamingForm::kPerSideMintCheckName;
  static constexpr const char* kPreviewLabelName =
      CaptureNamingForm::kPreviewLabelName;

  // This dialog's own, since the button is in its button box rather than in the
  // form: what it clears belongs to the form, where to put it does not.
  static constexpr const char* kClearButtonName = "naming_clear_button";

  static constexpr int kMaximumDiscSide = CaptureNamingForm::kMaximumDiscSide;

 private:
  CaptureNamingForm* form_ = nullptr;
};

}  // namespace ddd::gui
