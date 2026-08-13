/************************************************************************

    panel_placeholder.cpp

    Stand-in contents for panels whose displays are not yet built
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "panel_placeholder.h"

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

namespace ddd::gui {

PanelPlaceholder::PanelPlaceholder(const QString& title,
                                   const QString& description, QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(6);
  layout->addStretch();

  auto* title_label = new QLabel(title, this);
  QFont title_font = title_label->font();
  title_font.setBold(true);
  title_label->setFont(title_font);
  title_label->setAlignment(Qt::AlignCenter);
  layout->addWidget(title_label);

  auto* description_label = new QLabel(description, this);
  description_label->setAlignment(Qt::AlignCenter);
  description_label->setWordWrap(true);
  layout->addWidget(description_label);

  auto* status_label = new QLabel(tr("Not yet implemented."), this);
  status_label->setAlignment(Qt::AlignCenter);
  // Palette-derived rather than a literal colour, so it stays legible when the
  // theme changes underneath it.
  status_label->setForegroundRole(QPalette::PlaceholderText);
  layout->addWidget(status_label);

  layout->addStretch();
}

}  // namespace ddd::gui
