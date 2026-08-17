/************************************************************************

    capture_plan_form.h

    What to capture off a disc that has been examined
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <QWidget>

#include "auto_capture_plan.h"
#include "disc_profile.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QRadioButton;
class QSpinBox;

namespace ddd::gui {

// The controls that turn an examined disc into a plan, and what that plan
// would cost.
//
// **Built from a profile rather than from a form, and that is the one
// behavioural departure from the old application.** The old Automatic Capture
// dialog asked for the disc type before it had looked at the disc, offered CAV
// frame fields and CLV time fields side by side with both live, and failed
// several seconds later with "The disc in the player does not match the
// selected capture option" if the answer had been wrong. Every fact that dialog
// asked for is one the player can be asked, so a CAV disc is offered frames and
// no time codes, a CLV disc time codes and no frames, and every bound comes
// from the measured length — a range that cannot exist cannot be typed.
//
// What it cannot fill in from the profile it asks for, once, saying that it is
// asking — which for a fully examined disc is nothing at all.
//
// A widget rather than a dialog because the setting-up is one step of a longer
// business: it is the second page of the automatic capture's wizard, and a
// widget is what can be embedded rather than opened. It stays a widget rather
// than being folded back into that page because a plan is a thing with its own
// validity — Plan() and problem() are the page's whole interest in it.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class CapturePlanForm : public QWidget {
  Q_OBJECT

 public:
  explicit CapturePlanForm(const player::DiscProfile& disc,
                           QWidget* parent = nullptr);

  // Named so the widget tests can find them without depending on layout order.
  static constexpr const char* kWholeSideRadioName = "plan_whole_side";
  static constexpr const char* kRangeRadioName = "plan_range";
  static constexpr const char* kFromSpinUpRadioName = "plan_from_spin_up";

  // CAV only. A CLV profile does not build these, which is the honest form of
  // "frame entry is not offered for a disc that has no frame numbers".
  static constexpr const char* kStartFrameSpinName = "plan_start_frame";
  static constexpr const char* kEndFrameSpinName = "plan_end_frame";

  // CLV only, on the same terms.
  static constexpr const char* kStartTimeEditName = "plan_start_time";
  static constexpr const char* kEndTimeEditName = "plan_end_time";

  // Built only where the examination could not establish the standard.
  static constexpr const char* kStandardComboName = "plan_standard";

  static constexpr const char* kKeyLockCheckName = "plan_key_lock";
  static constexpr const char* kStopCaptureCheckName = "plan_stop_capture";

  static constexpr const char* kEstimateLabelName = "plan_estimate";
  static constexpr const char* kProblemLabelName = "plan_problem";

  // What the controls currently describe. Not necessarily runnable — see
  // problem().
  player::AutoCapturePlan Plan() const;

  // The profile, with anything the user has since declared folded in.
  const player::DiscProfile& disc() const { return disc_; }

  // Why the plan cannot be run, or kNone.
  player::PlanProblem problem() const;

  // Whether the capture should end when the player does. A preference rather
  // than part of the plan — it outlives the capture, and the settings dialog
  // shows the same box — so it is read out rather than carried in Plan().
  bool stop_capture_with_player() const;
  void SetStopCaptureWithPlayer(bool on);

  // Lock the controls, for a run that is under way.
  //
  // Every control's state is set from scratch each time rather than being
  // turned off when a capture starts and left to somebody to turn back on. A
  // window whose fields stay dead after a run because two places disagreed
  // about who re-enables them is the ordinary way this goes wrong.
  void SetEditable(bool editable);

 signals:
  // The plan, the problem or the estimate has changed. The labels in this form
  // are already up to date by the time it is emitted; it exists for whatever is
  // showing the form and owns the button that starts the capture.
  void Changed();

 private:
  void BuildAddressControls(QWidget* page, QFormLayout* form);

  // Work out what is wrong with the plan and set the wording to match, then say
  // so. Called on every change, because a form whose estimate is only correct
  // when it opens is one that describes a capture nobody asked for.
  void Refresh();

  // Which controls make sense for the shape now chosen.
  void ApplyShape();

  // What a capture of the plan would cost, against what the destination volume
  // has left.
  QString EstimateText() const;

  // An address field's value, in whichever way this disc is addressed.
  int32_t StartAddress() const;
  int32_t EndAddress() const;

  player::DiscProfile disc_;

  bool clv_ = false;
  bool editable_ = true;

  QRadioButton* whole_side_ = nullptr;
  QRadioButton* range_ = nullptr;
  QRadioButton* from_spin_up_ = nullptr;

  QSpinBox* start_frame_ = nullptr;
  QSpinBox* end_frame_ = nullptr;
  QLineEdit* start_time_ = nullptr;
  QLineEdit* end_time_ = nullptr;

  QComboBox* standard_ = nullptr;

  QCheckBox* key_lock_ = nullptr;
  QCheckBox* stop_capture_ = nullptr;

  QLabel* estimate_ = nullptr;
  QLabel* problem_ = nullptr;
};

}  // namespace ddd::gui
