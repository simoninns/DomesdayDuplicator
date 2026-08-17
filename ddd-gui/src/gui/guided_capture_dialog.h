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
#include "disc_profile.h"

// Included rather than forward declared, and it has to be: moc generates code
// for the slots below, and a queued-connection type whose Q_DECLARE_METATYPE is
// not yet visible gets the primary template instantiated instead.
#include "player_metatypes.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QSpinBox;

namespace ddd::gui {

class AutoCaptureController;

// "Set up capture", built from what the examination found.
//
// **The one behavioural departure from the old application, and it is
// deliberate.** The old Automatic Capture dialog asked for the disc type before
// it had looked at the disc, offered CAV frame fields and CLV time fields side
// by side with both live, and failed several seconds later with "The disc in
// the player does not match the selected capture option" if the answer had been
// wrong. Every fact that dialog asked for is one the player can be asked, so
// this window is built from a profile rather than from a form: a CAV disc is
// offered frames and no time codes, a CLV disc time codes and no frames, and
// every bound comes from the measured length, so a range that cannot exist
// cannot be typed.
//
// What it cannot fill in from the profile it asks for, once, saying that it is
// asking — which for a fully examined disc is nothing at all.
//
// It also shows the run. The setting-up and the watching belong in one window
// for the same reason the examination's progress and report do: what somebody
// is waiting through and what they were waiting for are the same subject, and
// a window that vanished at the moment it started would leave the disc spinning
// with nothing on screen to say why.
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
  static constexpr const char* kWholeSideRadioName = "guided_whole_side";
  static constexpr const char* kRangeRadioName = "guided_range";
  static constexpr const char* kFromSpinUpRadioName = "guided_from_spin_up";

  // CAV only. A CLV profile does not build these, which is the honest form of
  // "frame entry is not offered for a disc that has no frame numbers".
  static constexpr const char* kStartFrameSpinName = "guided_start_frame";
  static constexpr const char* kEndFrameSpinName = "guided_end_frame";

  // CLV only, on the same terms.
  static constexpr const char* kStartTimeEditName = "guided_start_time";
  static constexpr const char* kEndTimeEditName = "guided_end_time";

  // Built only where the examination could not establish the standard.
  static constexpr const char* kStandardComboName = "guided_standard";

  static constexpr const char* kNameEditName = "guided_name";
  static constexpr const char* kNameTakenLabelName = "guided_name_taken";
  static constexpr const char* kKeyLockCheckName = "guided_key_lock";
  static constexpr const char* kStopCaptureCheckName = "guided_stop_capture";

  static constexpr const char* kEstimateLabelName = "guided_estimate";
  static constexpr const char* kProblemLabelName = "guided_problem";
  static constexpr const char* kStatusLabelName = "guided_status";
  static constexpr const char* kProgressBarName = "guided_progress";
  static constexpr const char* kStartButtonName = "guided_start";
  static constexpr const char* kCancelButtonName = "guided_cancel";

  // What the controls currently describe. Not necessarily runnable — see
  // problem().
  player::AutoCapturePlan Plan() const;

  // The profile, with anything the user has since declared folded in.
  const player::DiscProfile& disc() const { return disc_; }

  // Why the plan cannot be run, or kNone.
  player::PlanProblem problem() const;

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
  void BuildAddressControls(QWidget* page, QFormLayout* form);

  // Read the fields into the plan, work out what is wrong with it, and set the
  // wording and the button states to match. Called on every change, because a
  // window whose start button is only correct when it opens is a window that
  // offers a capture it will refuse.
  void Refresh();

  // Which controls make sense for the shape now chosen.
  void ApplyShape();

  // What a capture of the plan would cost, against what the destination volume
  // has left.
  QString EstimateText() const;

  // Where a capture named by the field would actually be written.
  capture::CaptureDestination Destination() const;

  // Say, as the name is typed, what the file will really be called.
  void RefreshNameNote();

  // An address field's value, in whichever way this disc is addressed.
  int32_t StartAddress() const;
  int32_t EndAddress() const;

  AutoCaptureController* controller_ = nullptr;
  player::DiscProfile disc_;

  bool running_ = false;
  bool clv_ = false;

  QLabel* headline_ = nullptr;

  QRadioButton* whole_side_ = nullptr;
  QRadioButton* range_ = nullptr;
  QRadioButton* from_spin_up_ = nullptr;

  QSpinBox* start_frame_ = nullptr;
  QSpinBox* end_frame_ = nullptr;
  QLineEdit* start_time_ = nullptr;
  QLineEdit* end_time_ = nullptr;

  QComboBox* standard_ = nullptr;

  QLineEdit* name_ = nullptr;

  // Shown only while the typed name is one a capture already has.
  QLabel* name_taken_ = nullptr;
  QCheckBox* key_lock_ = nullptr;
  QCheckBox* stop_capture_ = nullptr;

  QLabel* estimate_ = nullptr;
  QLabel* problem_ = nullptr;
  QLabel* status_ = nullptr;
  QProgressBar* progress_ = nullptr;

  QPushButton* start_ = nullptr;
  QPushButton* cancel_ = nullptr;
};

}  // namespace ddd::gui
