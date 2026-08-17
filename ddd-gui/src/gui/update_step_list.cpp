/************************************************************************

    update_step_list.cpp

    The numbered list of steps an update works through
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "update_step_list.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QPalette>

#include "theme_color_tokens.h"

namespace ddd::gui {
namespace {

// The marker glyph for each state.
//
// Chosen from the set a plain system font has everywhere rather than from an
// icon theme, because this list has to look the same on the three platforms
// this application ships to and a missing glyph is a blank column.
QString Marker(UpdateStepList::State state) {
  switch (state) {
    case UpdateStepList::State::kPending:
      return QStringLiteral("○");
    case UpdateStepList::State::kActive:
      return QStringLiteral("▶");
    case UpdateStepList::State::kDone:
      return QStringLiteral("✓");
    case UpdateStepList::State::kFailed:
      return QStringLiteral("✕");
  }
  return QStringLiteral("○");
}

// The state in words, for the row's accessible name and for anyone reading
// the list through a screen reader rather than looking at it.
QString StateWord(UpdateStepList::State state) {
  switch (state) {
    case UpdateStepList::State::kPending:
      return QCoreApplication::translate("UpdateStepList", "still to come");
    case UpdateStepList::State::kActive:
      return QCoreApplication::translate("UpdateStepList", "in progress");
    case UpdateStepList::State::kDone:
      return QCoreApplication::translate("UpdateStepList", "done");
    case UpdateStepList::State::kFailed:
      return QCoreApplication::translate("UpdateStepList", "stopped here");
  }
  return QString();
}

}  // namespace

UpdateStepList::UpdateStepList(QWidget* parent) : QWidget(parent) {
  layout_ = new QGridLayout(this);
  layout_->setContentsMargins(0, 0, 0, 0);
  layout_->setHorizontalSpacing(8);
  layout_->setVerticalSpacing(4);

  // The marker column takes only what it needs, so the titles line up down
  // the list however wide the window is.
  layout_->setColumnStretch(0, 0);
  layout_->setColumnStretch(1, 1);
}

QString UpdateStepList::RowObjectName(int index) {
  return QStringLiteral("update_step_%1").arg(index);
}

void UpdateStepList::SetSteps(const std::vector<QString>& titles) {
  for (const Row& row : rows_) {
    delete row.marker;
    delete row.title;
  }
  rows_.clear();

  for (int index = 0; index < static_cast<int>(titles.size()); ++index) {
    Row row;
    row.text = titles[static_cast<size_t>(index)];

    row.marker = new QLabel(this);
    row.marker->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    row.title = new QLabel(this);
    row.title->setObjectName(RowObjectName(index));
    row.title->setWordWrap(true);

    // Numbered, because "step 3 of 5" on the progress bar has to point at
    // something a user can find on the list without counting.
    row.title->setText(QStringLiteral("%1. %2").arg(index + 1).arg(row.text));

    layout_->addWidget(row.marker, index, 0);
    layout_->addWidget(row.title, index, 1);

    rows_.push_back(row);
    ApplyState(index, State::kPending);
  }
}

UpdateStepList::State UpdateStepList::StateAt(int index) const {
  if (index < 0 || index >= count()) {
    return State::kPending;
  }
  return rows_[static_cast<size_t>(index)].state;
}

QString UpdateStepList::TitleAt(int index) const {
  if (index < 0 || index >= count()) {
    return QString();
  }
  return rows_[static_cast<size_t>(index)].text;
}

void UpdateStepList::SetCurrent(int index) {
  if (index < 0 || index >= count()) {
    return;
  }

  for (int row = 0; row < count(); ++row) {
    if (row < index) {
      ApplyState(row, State::kDone);
    } else if (row == index) {
      ApplyState(row, State::kActive);
    } else {
      ApplyState(row, State::kPending);
    }
  }
}

void UpdateStepList::MarkComplete() {
  for (int row = 0; row < count(); ++row) {
    ApplyState(row, State::kDone);
  }
}

void UpdateStepList::MarkFailed() {
  for (int row = 0; row < count(); ++row) {
    if (rows_[static_cast<size_t>(row)].state == State::kActive) {
      ApplyState(row, State::kFailed);
    }
  }
}

void UpdateStepList::ApplyState(int index, State state) {
  if (index < 0 || index >= count()) {
    return;
  }

  Row& row = rows_[static_cast<size_t>(index)];
  row.state = state;
  row.marker->setText(Marker(state));

  // A step that has not started is greyed by being disabled rather than by
  // being given a colour: the palette's own disabled text colour is the one
  // the platform already uses to mean "not yet", and it follows a theme change
  // with nothing here to reconnect.
  const bool pending = state == State::kPending;
  row.marker->setEnabled(!pending);
  row.title->setEnabled(!pending);

  QFont font = row.title->font();
  font.setBold(state == State::kActive || state == State::kFailed);
  row.title->setFont(font);

  const QString accessible =
      QCoreApplication::translate("UpdateStepList", "Step %1 of %2, %3: %4")
          .arg(index + 1)
          .arg(count())
          .arg(StateWord(state), row.text);
  row.title->setAccessibleName(accessible);
  row.marker->setAccessibleName(StateWord(state));

  Restyle();
}

void UpdateStepList::Restyle() {
  const bool dark = theme_tokens::IsDarkPalette(palette());

  for (Row& row : rows_) {
    QPalette marker_palette = row.marker->palette();
    QColor colour = palette().color(QPalette::WindowText);

    if (row.state == State::kDone) {
      colour = theme_tokens::PlotColor(
          theme_tokens::PlotColorToken::kVerdictPass, dark);
    } else if (row.state == State::kFailed) {
      colour = theme_tokens::PlotColor(
          theme_tokens::PlotColorToken::kVerdictFail, dark);
    }

    marker_palette.setColor(QPalette::WindowText, colour);
    row.marker->setPalette(marker_palette);

    // Only the failed row's *text* is coloured. A finished step is not a
    // result to be read across the room — it is a step that is over — and a
    // list of green sentences would compete with the one line that matters,
    // which is whichever one is in progress.
    QPalette title_palette = row.title->palette();
    title_palette.setColor(
        QPalette::WindowText,
        row.state == State::kFailed
            ? theme_tokens::PlotColor(
                  theme_tokens::PlotColorToken::kVerdictFail, dark)
            : palette().color(QPalette::WindowText));
    row.title->setPalette(title_palette);
  }
}

void UpdateStepList::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);

  if (event->type() == QEvent::PaletteChange) {
    Restyle();
  }
}

}  // namespace ddd::gui
