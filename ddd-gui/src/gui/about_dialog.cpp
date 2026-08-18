/************************************************************************

    about_dialog.cpp

    The About box: what this is, which build, and under what terms
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "about_dialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPixmap>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>
#include <algorithm>

#include "about_text.h"

namespace ddd::gui {

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(tr("About Domesday Duplicator"));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(20, 20, 20, 16);
  layout->setSpacing(12);

  logo_ = new QLabel(this);
  logo_->setObjectName(QLatin1String(kLogoLabelName));
  logo_->setAlignment(Qt::AlignCenter);

  const QPixmap logo = AboutLogo();
  if (!logo.isNull()) {
    logo_->setPixmap(logo);
  } else {
    // A missing decoration is not a reason to withhold the version and licence
    // text underneath it, so the label simply takes no space.
    logo_->hide();
  }
  layout->addWidget(logo_);

  text_ = new QLabel(this);
  text_->setObjectName(QLatin1String(kTextLabelName));
  text_->setTextFormat(Qt::RichText);
  text_->setText(AboutText());
  text_->setWordWrap(true);

  // The licence notice carries a link to the source, which is of no use unless
  // it can be followed.
  text_->setOpenExternalLinks(true);
  text_->setTextInteractionFlags(Qt::TextBrowserInteraction);

  // The whole reason this is a dialog rather than a message box: the text gets
  // a width it can be read at, instead of whatever a message box wrapped it to.
  text_->setMinimumWidth(kTextWidthPixels);

  // Inside a scroll area, so that text which does not fit can still be read.
  //
  // A word-wrapped QLabel reports its height as a function of its width, and a
  // layout does not always give it the height it asked for — so the last
  // paragraph goes off the bottom of the dialog, and a label has no way to
  // scroll to it. That is a licence notice a user cannot read, which is worse
  // than an ugly dialog.
  auto* scroll = new QScrollArea(this);
  scroll->setObjectName(QLatin1String(kScrollAreaName));
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setWidget(text_);

  // A scroll area does not adopt its widget's minimum width, so without this
  // the dialog is free to shrink below the width the text was given — and with
  // the horizontal scrollbar turned off, the right-hand end of every line
  // simply disappears. Room for the vertical scrollbar as well, so its arrival
  // does not narrow the text under the reader.
  const int scrollbar = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
  scroll->setMinimumWidth(kTextWidthPixels + scrollbar);

  // A small floor, so the dialog can be made short and will then scroll. Sizing
  // the floor to the text instead would stop it being resized below the text at
  // all — which sounds like the same thing and is not: it means the scroll area
  // can never scroll, and a screen too short for the dialog has no way out.
  constexpr int kSmallestScrollHeight = 140;
  scroll->setMinimumHeight(kSmallestScrollHeight);

  layout->addWidget(scroll, 1);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  layout->addWidget(buttons);

  // Opened at the height its text actually needs, so there is nothing to scroll
  // and no scrollbar to see unless somebody makes the window smaller. The cap
  // keeps a large system font from producing a dialog taller than the screen;
  // past it the scrollbar appears and does its job.
  constexpr int kTallestWithoutScrolling = 460;
  const int wanted = std::min(
      kTallestWithoutScrolling,
      std::max(text_->heightForWidth(kTextWidthPixels), kSmallestScrollHeight));
  resize(sizeHint().width(),
         sizeHint().height() + (wanted - kSmallestScrollHeight));
}

}  // namespace ddd::gui
