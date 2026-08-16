/************************************************************************

    examine_dialog.h

    Working out what is in the player, and saying so
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QDialog>
#include <QString>

#include "disc_examiner.h"
#include "disc_profile.h"
#include "player_connection.h"

// Included rather than forward declared, and it has to be: moc generates code
// for the slots below, and a queued-connection type whose Q_DECLARE_METATYPE is
// not yet visible gets the primary template instantiated instead — after which
// the real declaration is an error naming a generated file.
#include "player_metatypes.h"

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

namespace ddd::gui {

class PlayerController;

// "Examine disc".
//
// Two states in one window, and it is one window on purpose: what the user is
// waiting through and what they were waiting for belong in the same place, so
// the progress does not vanish at the moment it is replaced by an answer.
//
// **Non-modal**, like the remote and for a related reason: an examination takes
// the better part of half a minute, spinning a disc up and seeking twice, and a
// window that blocked the spectrum and the waveform for that long would be in
// the way of the very thing it is preparing.
//
// What it leaves behind is a report the user can read and copy, because the
// commonest thing to do with a disc that behaves strangely is to tell somebody
// about it — and a report that could only be described rather than pasted is a
// report that arrives as "it said something about the disc status".
//
// **Set up capture… is not here.** It belongs with the guided setup that
// consumes the profile, and a button that is present and does nothing is
// exactly what the capability gating in the remote exists to avoid.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class ExamineDialog : public QDialog {
  Q_OBJECT

 public:
  // The controller may be null, and the widget tests pass null deliberately:
  // the dialog then builds and lays out exactly as it does in the application,
  // examines nothing, and can still be driven through the slots below.
  explicit ExamineDialog(PlayerController* controller,
                         QWidget* parent = nullptr);

  // Named so the widget tests can find them without depending on layout order.
  static constexpr const char* kHeadlineLabelName = "examine_headline";
  static constexpr const char* kStageLabelName = "examine_stage";
  static constexpr const char* kProgressBarName = "examine_progress";
  static constexpr const char* kReportViewName = "examine_report";
  static constexpr const char* kStartButtonName = "examine_start";
  static constexpr const char* kCancelButtonName = "examine_cancel";
  static constexpr const char* kCopyButtonName = "examine_copy";

  // The profile the last examination produced. Empty until one has finished.
  const player::DiscProfile& profile() const { return profile_; }

  bool running() const { return running_; }

 public slots:
  // Begin. Does nothing when one is already running, or when there is no
  // player — the button is disabled in both cases, and this is the second half
  // of that rule for anything that reaches it another way.
  void Start();

  // Stop the one in progress. Takes effect at the end of the step the player is
  // in the middle of, which may be a thirty-second seek away.
  void Cancel();

  // Wired to the controller when there is one, and called directly by the
  // widget tests when there is not.
  void SetConnection(const ddd::gui::PlayerConnection& connection);
  void SetProgress(ddd::player::ExamineStage stage, int completed, int total);
  void SetResult(const ddd::player::DiscProfile& disc,
                 ddd::player::ExamineOutcome outcome);

 private:
  void CopyReport();

  // Offer exactly what can be done now: examine when there is a player and
  // nothing running, cancel when something is.
  void ApplyState();

  PlayerController* controller_ = nullptr;

  PlayerConnection connection_;
  player::DiscProfile profile_;

  bool running_ = false;

  QLabel* headline_ = nullptr;
  QLabel* stage_ = nullptr;
  QProgressBar* progress_ = nullptr;

  // The report is a text view rather than a label for the same reason the
  // remote's readouts are: it is meant to be selected and copied, it is many
  // lines long, and it wants its own scrollbar rather than to push the buttons
  // off the bottom of the screen.
  QPlainTextEdit* report_ = nullptr;

  QPushButton* start_ = nullptr;
  QPushButton* cancel_ = nullptr;
  QPushButton* copy_ = nullptr;
};

}  // namespace ddd::gui
