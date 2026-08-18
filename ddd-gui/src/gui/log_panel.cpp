/************************************************************************

    log_panel.cpp

    Log panel contents
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "log_panel.h"

#include <QCheckBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QListView>
#include <QPushButton>
#include <QVBoxLayout>

#include "log_message_model.h"

namespace ddd::gui {

LogPanel::LogPanel(LogMessageModel* model, QWidget* parent)
    : QWidget(parent), model_(model) {
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

  auto* controls = new QHBoxLayout;
  controls->setContentsMargins(0, 0, 0, 0);

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

}  // namespace ddd::gui
