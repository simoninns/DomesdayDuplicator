/************************************************************************

    firmware_dialog.cpp

    The Firmware box: which build each half of the device is running
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "firmware_dialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>
#include <algorithm>

namespace ddd::gui {

FirmwareDialog::FirmwareDialog(const FirmwareVersions& versions,
                               QWidget* parent)
    : QDialog(parent) {
  setWindowTitle(tr("Firmware"));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(20, 20, 20, 16);
  layout->setSpacing(12);

  text_ = new QLabel(this);
  text_->setObjectName(QLatin1String(kTextLabelName));
  text_->setTextFormat(Qt::RichText);
  text_->setText(FirmwareText(versions));
  text_->setWordWrap(true);

  // Selectable, because the first useful thing to do with a mismatched pair of
  // commits is paste them into a bug report.
  text_->setTextInteractionFlags(Qt::TextSelectableByMouse);

  text_->setMinimumWidth(kTextWidthPixels);

  // Inside a scroll area for the same reason the About dialog is: a
  // word-wrapped label reports its height as a function of its width, a layout
  // is not obliged to grant it, and the last paragraph then goes off the bottom
  // with no way to reach it. Here that paragraph is the one that says whether
  // the versions match, which is the whole point of the dialog.
  auto* scroll = new QScrollArea(this);
  scroll->setObjectName(QLatin1String(kScrollAreaName));
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setWidget(text_);

  const int scrollbar = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
  scroll->setMinimumWidth(kTextWidthPixels + scrollbar);

  constexpr int kSmallestScrollHeight = 120;
  scroll->setMinimumHeight(kSmallestScrollHeight);

  layout->addWidget(scroll, 1);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  layout->addWidget(buttons);

  // Opened at the height the text actually needs, capped so that a large system
  // font cannot produce a dialog taller than the screen. Past the cap the
  // scrollbar appears and does its job.
  constexpr int kTallestWithoutScrolling = 420;
  const int wanted = std::min(
      kTallestWithoutScrolling,
      std::max(text_->heightForWidth(kTextWidthPixels), kSmallestScrollHeight));
  resize(sizeHint().width(),
         sizeHint().height() + (wanted - kSmallestScrollHeight));
}

}  // namespace ddd::gui
