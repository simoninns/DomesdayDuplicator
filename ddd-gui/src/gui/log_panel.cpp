/************************************************************************

    log_panel.cpp

    Log panel contents
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "log_panel.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QModelIndexList>
#include <QPoint>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>
#include <algorithm>

#include "application_logger.h"
#include "log_message_model.h"

namespace ddd::gui {
namespace {

// The chooser's entries, least to most severe, ending in the threshold that
// admits nothing. The same set --log-level accepts, and in the same order, so
// that the drop-down and the switch describe one thing rather than two.
struct LevelChoice {
  capture::LogLevel level;
  const char* label;
};

constexpr LevelChoice kLevelChoices[] = {
    {capture::LogLevel::kDebug,
     QT_TRANSLATE_NOOP("ddd::gui::LogPanel", "Debug")},
    {capture::LogLevel::kInfo, QT_TRANSLATE_NOOP("ddd::gui::LogPanel", "Info")},
    {capture::LogLevel::kWarning,
     QT_TRANSLATE_NOOP("ddd::gui::LogPanel", "Warning")},
    {capture::LogLevel::kError,
     QT_TRANSLATE_NOOP("ddd::gui::LogPanel", "Error")},
    {capture::LogLevel::kOff, QT_TRANSLATE_NOOP("ddd::gui::LogPanel", "Off")},
};

}  // namespace

LogPanel::LogPanel(LogMessageModel* model, ApplicationLogger* logger,
                   QWidget* parent)
    : QWidget(parent), model_(model), logger_(logger) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(4);

  view_ = new QListView(this);
  view_->setModel(model_);
  view_->setUniformItemSizes(true);
  view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  // Fixed pitch, because the records are timestamp-prefixed and columns that
  // do not line up are markedly harder to scan.
  view_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  layout->addWidget(view_, 1);

  // Copy is offered twice over: the shortcut for anyone who expects Ctrl+C to
  // work in a list, and the context menu so that it is discoverable by anyone
  // who does not. Both run the one slot.
  copy_action_ = new QAction(tr("&Copy"), this);
  copy_action_->setObjectName(QLatin1String(kCopyActionName));
  copy_action_->setShortcut(QKeySequence::Copy);
  // Scoped to the list rather than the window: the shortcut belongs to whatever
  // has the focus, and a panel-wide one would take Ctrl+C away from a text
  // field that happened to be a sibling.
  copy_action_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(copy_action_, &QAction::triggered, this, &LogPanel::CopySelection);
  view_->addAction(copy_action_);

  select_all_action_ = new QAction(tr("Select &All"), this);
  select_all_action_->setObjectName(QLatin1String(kSelectAllActionName));
  select_all_action_->setShortcut(QKeySequence::SelectAll);
  select_all_action_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(select_all_action_, &QAction::triggered, view_,
          &QAbstractItemView::selectAll);
  view_->addAction(select_all_action_);

  view_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(view_, &QWidget::customContextMenuRequested, this,
          &LogPanel::ShowContextMenu);

  auto* controls = new QHBoxLayout;
  controls->setContentsMargins(0, 0, 0, 0);

  auto* level_label = new QLabel(tr("Level:"), this);
  controls->addWidget(level_label);

  level_ = new QComboBox(this);
  level_->setObjectName(QLatin1String(kLevelComboName));
  for (const LevelChoice& choice : kLevelChoices) {
    level_->addItem(tr(choice.label), static_cast<int>(choice.level));
  }
  level_->setToolTip(tr(
      "Discard records below this level. Applies to the console and the log "
      "file as well as this panel; records already listed are not removed."));
  level_label->setBuddy(level_);
  controls->addWidget(level_);

  if (logger_ != nullptr) {
    // Started from where the logger already is, which is what --log-level or
    // --debug asked for rather than this panel's own idea of a default.
    const int index =
        level_->findData(static_cast<int>(logger_->minimum_level()));
    if (index >= 0) {
      level_->setCurrentIndex(index);
    }

    connect(level_, &QComboBox::currentIndexChanged, this, [this](int index) {
      const QVariant level = level_->itemData(index);
      if (!level.isValid()) {
        return;
      }
      logger_->SetMinimumLevel(static_cast<capture::LogLevel>(level.toInt()));
    });
  } else {
    level_->setEnabled(false);
  }

  controls->addSpacing(8);

  follow_tail_ = new QCheckBox(tr("Follow"), this);
  follow_tail_->setChecked(true);
  follow_tail_->setToolTip(tr("Scroll to the newest record as it arrives"));
  controls->addWidget(follow_tail_);

  controls->addStretch();

  auto* clear_button = new QPushButton(tr("Clear"), this);
  connect(clear_button, &QPushButton::clicked, model_, &LogMessageModel::Clear);
  controls->addWidget(clear_button);

  layout->addLayout(controls);

  connect(model_, &QAbstractItemModel::rowsInserted, this,
          [this](const QModelIndex&, int, int) { ScrollToLatest(); });
}

void LogPanel::ScrollToLatest() {
  if (!follow_tail_->isChecked()) {
    return;
  }
  view_->scrollToBottom();
}

QString LogPanel::SelectedText() const {
  QModelIndexList selected = view_->selectionModel()->selectedIndexes();
  if (selected.isEmpty()) {
    return {};
  }

  // Selection order is the order rows were picked, which for a shift-click
  // upwards is the reverse of the order they were logged in. Sorted, so that
  // what is pasted reads the way the panel does.
  std::sort(selected.begin(), selected.end(),
            [](const QModelIndex& lhs, const QModelIndex& rhs) {
              return lhs.row() < rhs.row();
            });

  QStringList lines;
  lines.reserve(selected.size());
  for (const QModelIndex& index : selected) {
    lines.append(index.data(Qt::DisplayRole).toString());
  }

  // A trailing newline, because these are whole lines: pasted into a terminal
  // or an editor the last one should end like the others rather than leave the
  // cursor mid-line.
  return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

void LogPanel::CopySelection() {
  const QString text = SelectedText();
  if (text.isEmpty()) {
    // Nothing selected. The clipboard is left alone rather than emptied —
    // clearing what a user copied from somewhere else is not what an
    // ineffective copy should do.
    return;
  }
  QApplication::clipboard()->setText(text);
}

void LogPanel::ShowContextMenu(const QPoint& position) {
  // Enabled state is decided here rather than kept in step with the selection,
  // because this is the only moment either action is visible.
  copy_action_->setEnabled(view_->selectionModel()->hasSelection());
  select_all_action_->setEnabled(model_->rowCount(QModelIndex()) > 0);

  QMenu menu(this);
  menu.addAction(copy_action_);
  menu.addAction(select_all_action_);
  menu.exec(view_->viewport()->mapToGlobal(position));
}

}  // namespace ddd::gui
