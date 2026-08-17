/************************************************************************

    auto_capture_wizard.h

    Taking a capture off a disc, from finding out what it is to what was written
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QDialog>
#include <QString>

#include "capture_naming_form.h"
#include "capture_plan_form.h"
#include "disc_profile.h"

// Included rather than forward declared, and it has to be: moc generates code
// for the slots below, and a queued-connection type whose Q_DECLARE_METATYPE is
// not yet visible gets the primary template instantiated instead.
#include "player_metatypes.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

namespace ddd::gui {

class AutoCaptureController;
class CaptureController;
class PlayerController;

// An automatic capture, start to finish, in one window.
//
// **The shape of this is the point of it.** Everything here existed before and
// was spread across four windows reached from three places: the Player dock or
// menu opened Examine, Examine's report offered "Set up capture", the
// destination and format lived on the Capture panel, and the naming fields
// lived in a dialog none of those opened. A user doing the ordinary thing —
// capturing both sides of one disc — walked all of it twice and had to know the
// order. An automatic capture is a workflow, so it is presented as one: four
// pages, Previous and Next, and a summary at the end.
//
// Hand-built rather than QWizard, and deliberately. Every dialog in this
// application is built against the same conventions — an object name on each
// control, and a window that builds with null controllers so the whole of it is
// testable with nothing attached — while QWizard brings a frame, a watermark
// and a button box that would be fought at every step. The flow also needs two
// things QWizard is awkward about: it is non-modal, because the spectrum and
// waveform are what somebody watches during a forty-minute side, and it refuses
// to navigate at all while a run is going.
//
// The two forms on the way through are the same widgets the manual path uses —
// CaptureNamingForm and CapturePlanForm — so there is one set of naming rules
// and one plan validator in the application rather than two that agree by
// inspection.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class AutoCaptureWizard : public QDialog {
  Q_OBJECT

 public:
  // The controller may be null, and the widget tests pass null deliberately:
  // every page then builds, lays out and validates exactly as it does in the
  // application, and captures nothing. The capture and player controllers are
  // taken from it rather than passed separately, because it is the object that
  // already holds both.
  explicit AutoCaptureWizard(AutoCaptureController* controller,
                             QWidget* parent = nullptr);

  // The four pages, in the order they are worked through.
  enum class Page {
    // What is in the player, and what to call the file. The examination runs
    // here.
    kDisc,

    // What to take off it, and where to put it.
    kSettings,

    // The run.
    kCapture,

    // What happened, and the way round again for the other side.
    kSummary,
  };

  // Begin from a disc that has already been examined, on the settings page.
  //
  // What the Examine window's "Set up capture…" hands over. Examining again
  // would spend a minute rediscovering what is already known and leave the disc
  // somewhere else while it did.
  void StartFromProfile(const ddd::player::DiscProfile& disc);

  // Named so the widget tests can find them without depending on layout order.
  static constexpr const char* kPagesName = "wizard_pages";
  static constexpr const char* kPreviousButtonName = "wizard_previous";
  static constexpr const char* kNextButtonName = "wizard_next";
  static constexpr const char* kCloseButtonName = "wizard_close";

  static constexpr const char* kDiscHeadlineName = "wizard_disc_headline";
  static constexpr const char* kExamineStageName = "wizard_examine_stage";
  static constexpr const char* kExamineProgressName = "wizard_examine_progress";
  static constexpr const char* kExamineButtonName = "wizard_examine";
  static constexpr const char* kExamineStopButtonName = "wizard_examine_stop";
  static constexpr const char* kNameEditName = "wizard_name";
  static constexpr const char* kNameTakenLabelName = "wizard_name_taken";

  static constexpr const char* kDirectoryEditName = "wizard_directory";
  static constexpr const char* kBrowseButtonName = "wizard_browse";
  static constexpr const char* kFormatComboName = "wizard_format";
  static constexpr const char* kSampleRateComboName = "wizard_sample_rate";

  static constexpr const char* kRunStatusLabelName = "wizard_run_status";
  static constexpr const char* kRunProgressName = "wizard_run_progress";
  static constexpr const char* kStartButtonName = "wizard_start";
  static constexpr const char* kStopButtonName = "wizard_stop";

  static constexpr const char* kSummaryLabelName = "wizard_summary";
  static constexpr const char* kAnotherSideButtonName = "wizard_another_side";

  Page page() const;
  bool running() const { return running_; }

  // The plan the settings page currently describes, or a default one before
  // there is a form to ask. Exposed for the same reason the guided window
  // exposed it: what the controls mean is worth asserting directly.
  player::AutoCapturePlan Plan() const;
  player::PlanProblem problem() const;

  const player::DiscProfile& disc() const { return disc_; }

 public slots:
  // Move a page, if the page in hand allows it.
  void GoNext();
  void GoPrevious();

  // Run the examination on the disc page.
  void Examine();
  void StopExamining();

  // Begin the capture, and stop it. Do nothing when the plan is not runnable or
  // a run is already going — the buttons are disabled in both cases, and this
  // is the second half of that rule for anything reaching them another way.
  void Start();
  void Stop();

  // Back to the disc page for the other side of this disc, with the side number
  // moved on and the examination started again.
  void CaptureAnotherSide();

  // Wired to the controllers when there are any, and called directly by the
  // widget tests when there are not.
  void SetExamineProgress(ddd::player::ExamineStage stage, int completed,
                          int total);
  void SetExamineResult(const ddd::player::DiscProfile& disc,
                        ddd::player::ExamineOutcome outcome);
  void SetRunProgress(ddd::player::AutoCaptureStage stage, int address);
  void SetRunResult(ddd::player::AutoCaptureOutcome outcome);

 protected:
  // Refused while a capture is running. See the definition.
  void closeEvent(QCloseEvent* event) override;

 private:
  QWidget* BuildDiscPage();
  QWidget* BuildSettingsPage();
  QWidget* BuildCapturePage();
  QWidget* BuildSummaryPage();

  void ShowPage(Page page);

  // Build the plan controls for the disc now in hand, replacing whatever was
  // there. It has to be a rebuild rather than an update: a CAV disc is offered
  // frame boxes and a CLV one time-code fields, and which exist at all is
  // decided when the form is constructed.
  void RebuildPlanForm();

  // Set every button and every gate from the state in hand. One function,
  // called after anything changes, because a wizard whose Next button is only
  // correct on the page it was drawn on is a wizard that offers a step it will
  // refuse.
  void Refresh();

  // Whether the disc page has established enough to plan a capture from.
  bool DiscIsUsable() const;

  // Read the destination controls into the settings, and back.
  void ApplyCaptureSettings();
  void ShowCaptureSettings();

  // Where a capture named by the field would actually be written, and what to
  // say when that is not the name that was asked for.
  void RefreshNameNote();

  // Offer a name built from what the disc turned out to be, resolved against
  // the destination first so that what is on screen is what will be written.
  void SuggestName();

  AutoCaptureController* controller_ = nullptr;
  CaptureController* capture_ = nullptr;
  PlayerController* player_ = nullptr;

  player::DiscProfile disc_;

  bool examining_ = false;
  bool running_ = false;

  // True while the destination controls are being filled from the settings, so
  // the change signals they emit do not write those same settings straight
  // back.
  bool loading_ = false;

  // What the finished capture was written to, for the summary.
  QString written_path_;
  quint64 written_bytes_ = 0;

  QStackedWidget* pages_ = nullptr;

  QPushButton* previous_button_ = nullptr;
  QPushButton* next_button_ = nullptr;

  // --- The disc page --------------------------------------------------------

  QLabel* disc_headline_ = nullptr;
  QLabel* examine_stage_ = nullptr;
  QProgressBar* examine_progress_ = nullptr;
  QPushButton* examine_button_ = nullptr;
  QPushButton* examine_stop_button_ = nullptr;
  CaptureNamingForm* naming_form_ = nullptr;
  QLineEdit* name_edit_ = nullptr;
  QLabel* name_taken_ = nullptr;

  // --- The settings page ----------------------------------------------------

  QVBoxLayout* settings_layout_ = nullptr;
  CapturePlanForm* plan_form_ = nullptr;
  QLineEdit* directory_edit_ = nullptr;
  QPushButton* browse_button_ = nullptr;
  QComboBox* format_combo_ = nullptr;
  QComboBox* sample_rate_combo_ = nullptr;

  // --- The capture page -----------------------------------------------------

  QLabel* run_status_ = nullptr;
  QProgressBar* run_progress_ = nullptr;
  QPushButton* start_button_ = nullptr;
  QPushButton* stop_button_ = nullptr;

  // --- The summary page -----------------------------------------------------

  QLabel* summary_ = nullptr;
  QPushButton* another_side_button_ = nullptr;
};

}  // namespace ddd::gui
