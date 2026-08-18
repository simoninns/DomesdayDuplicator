/************************************************************************

    update_step_list.h

    The numbered list of steps an update works through
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <QWidget>
#include <vector>

class QLabel;
class QGridLayout;

namespace ddd::gui {

// The whole procedure, on screen before it starts: every step numbered and
// greyed, the one in hand picked out, and a tick against each one as it is
// finished.
//
// The point of showing it before the first byte moves is that an update is the
// one operation in this application a user cannot interrupt safely, and the
// question they need answered before pressing the button is "how long am I
// committed for". A list of five steps answers that in a way a stage name that
// will be replaced by another stage name cannot.
//
// The state of a step is carried three ways over — the marker glyph, the
// weight of the text, and whether the row is greyed — rather than by colour
// alone, so it reads the same to somebody who cannot tell the tick's green
// from the cross's red. The accessible name of each row says the state in
// words for the same reason.
//
// Thread-safety: NOT thread-safe. Interface thread only.
class UpdateStepList : public QWidget {
  Q_OBJECT

 public:
  enum class State {
    // Not started. Greyed, because it is a promise rather than a report.
    kPending,

    kActive,
    kDone,

    // Stopped here, whether by a failure or by the Stop button.
    kFailed,
  };

  explicit UpdateStepList(QWidget* parent = nullptr);

  // Replace the list. Every step starts pending, which is what makes the plan
  // readable before the update is started.
  void SetSteps(const std::vector<QString>& titles);

  // Put the highlight on one step: everything before it is finished,
  // everything after it is still to come.
  //
  // Out-of-range indices are ignored rather than clamped. The tracker hands
  // back -1 before the first report, and clamping that to the first step would
  // claim an update had begun a step it had not.
  void SetCurrent(int index);

  // Every step finished.
  void MarkComplete();

  // The step in hand did not finish. Steps before it keep their ticks: they
  // did finish, and on this device that is the difference between "the
  // firmware is installed and the gateware is not" and "nothing happened".
  void MarkFailed();

  int count() const { return static_cast<int>(rows_.size()); }
  State StateAt(int index) const;
  QString TitleAt(int index) const;

  // The object name of one row's text label, so a widget test can reach it
  // without depending on the order things were added to the layout.
  static QString RowObjectName(int index);

 protected:
  // Re-picks the colours when the theme changes. A tick that was legible on
  // the light palette is not necessarily legible on the dark one, and this
  // widget outlives a theme change in the settings dialog.
  void changeEvent(QEvent* event) override;

 private:
  struct Row {
    QLabel* marker = nullptr;
    QLabel* title = nullptr;
    QString text;
    State state = State::kPending;
  };

  void Restyle();
  void ApplyState(int index, State state);

  QGridLayout* layout_ = nullptr;
  std::vector<Row> rows_;
};

}  // namespace ddd::gui
