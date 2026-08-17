/************************************************************************

    guided_capture_dialog.h

    Setting up a capture from what the disc turned out to be
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QDialog>
#include <QString>

#include "auto_capture_plan.h"
#include "capture_naming.h"
#include "capture_plan_form.h"
#include "disc_profile.h"

// Included rather than forward declared, and it has to be: moc generates code
// for the slots below, and a queued-connection type whose Q_DECLARE_METATYPE is
// not yet visible gets the primary template instantiated instead.
#include "player_metatypes.h"

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;

namespace ddd::gui {

class AutoCaptureController;

// "Set up capture", built from what the examination found.
//
// The controls that describe the capture are CapturePlanForm, which is where
// the design of them is set out. This window is what surrounds them: the disc
// it was opened on, a name for the file, and the run itself.
//
// The setting-up and the watching belong in one window for the same reason the
// examination's progress and report do: what somebody is waiting through and
// what they were waiting for are the same subject, and a window that vanished
// at the moment it started would leave the disc spinning with nothing on screen
// to say why.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class GuidedCaptureDialog : public QDialog {
  Q_OBJECT

 public:
  // The controller may be null, and the widget tests pass null deliberately:
  // the window then builds, lays out and validates exactly as it does in the
  // application, and captures nothing.
  GuidedCaptureDialog(AutoCaptureController* controller,
                      const player::DiscProfile& disc,
                      QWidget* parent = nullptr);

  // Named so the widget tests can find them without depending on layout order.
  static constexpr const char* kHeadlineLabelName = "guided_headline";

  // The plan form's own names, carried here so that anything holding this
  // window need not know which of the two objects a given control was built by.
  static constexpr const char* kWholeSideRadioName =
      CapturePlanForm::kWholeSideRadioName;
  static constexpr const char* kRangeRadioName =
      CapturePlanForm::kRangeRadioName;
  static constexpr const char* kFromSpinUpRadioName =
      CapturePlanForm::kFromSpinUpRadioName;
  static constexpr const char* kStartFrameSpinName =
      CapturePlanForm::kStartFrameSpinName;
  static constexpr const char* kEndFrameSpinName =
      CapturePlanForm::kEndFrameSpinName;
  static constexpr const char* kStartTimeEditName =
      CapturePlanForm::kStartTimeEditName;
  static constexpr const char* kEndTimeEditName =
      CapturePlanForm::kEndTimeEditName;
  static constexpr const char* kStandardComboName =
      CapturePlanForm::kStandardComboName;
  static constexpr const char* kKeyLockCheckName =
      CapturePlanForm::kKeyLockCheckName;
  static constexpr const char* kStopCaptureCheckName =
      CapturePlanForm::kStopCaptureCheckName;
  static constexpr const char* kEstimateLabelName =
      CapturePlanForm::kEstimateLabelName;
  static constexpr const char* kProblemLabelName =
      CapturePlanForm::kProblemLabelName;

  static constexpr const char* kNameEditName = "guided_name";
  static constexpr const char* kNameTakenLabelName = "guided_name_taken";

  static constexpr const char* kStatusLabelName = "guided_status";
  static constexpr const char* kProgressBarName = "guided_progress";
  static constexpr const char* kStartButtonName = "guided_start";
  static constexpr const char* kCancelButtonName = "guided_cancel";

  // What the controls currently describe. Not necessarily runnable — see
  // problem().
  player::AutoCapturePlan Plan() const { return form_->Plan(); }

  // The profile, with anything the user has since declared folded in.
  const player::DiscProfile& disc() const { return form_->disc(); }

  // Why the plan cannot be run, or kNone.
  player::PlanProblem problem() const { return form_->problem(); }

  bool running() const { return running_; }

 public slots:
  // Begin the capture. Does nothing when the plan is not runnable or one is
  // already going — the button is disabled in both cases, and this is the
  // second half of that rule for anything that reaches it another way.
  void Start();

  // Stop it, finishing the file properly.
  void Cancel();

  // Wired to the controller when there is one, and called directly by the
  // widget tests when there is not.
  void SetProgress(ddd::player::AutoCaptureStage stage, int address);
  void SetResult(ddd::player::AutoCaptureOutcome outcome);

 private:
  // Set the button states and the form's editability from the plan and from
  // whether a run is under way. Called on every change, because a window whose
  // start button is only correct when it opens is a window that offers a
  // capture it will refuse.
  void Refresh();

  // Where a capture named by the field would actually be written.
  capture::CaptureDestination Destination() const;

  // Say, as the name is typed, what the file will really be called.
  void RefreshNameNote();

  AutoCaptureController* controller_ = nullptr;

  bool running_ = false;

  QLabel* headline_ = nullptr;

  QLineEdit* name_ = nullptr;

  // Shown only while the typed name is one a capture already has.
  QLabel* name_taken_ = nullptr;

  CapturePlanForm* form_ = nullptr;

  QLabel* status_ = nullptr;
  QProgressBar* progress_ = nullptr;

  QPushButton* start_ = nullptr;
  QPushButton* cancel_ = nullptr;
};

}  // namespace ddd::gui
