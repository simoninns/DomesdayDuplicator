/************************************************************************

    firmware_dialog.cpp

    The Firmware box: which build each half of the device is running
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "firmware_dialog.h"

#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QScrollArea>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

#include "version.h"

namespace ddd::gui {

FirmwareDialog::FirmwareDialog(const FirmwareVersions& versions,
                               QWidget* parent)
    : QDialog(parent) {
  Build(versions, std::nullopt);
}

FirmwareDialog::FirmwareDialog(const FirmwareVersions& versions,
                               UpdatePage::Device device, QWidget* parent)
    : QDialog(parent) {
  Build(versions, std::move(device));
}

void FirmwareDialog::Build(const FirmwareVersions& versions,
                           std::optional<UpdatePage::Device> device) {
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

  if (device.has_value()) {
    // Two pages rather than one longer one: "what is it running" and "install
    // something else" are different questions, and a user with the first
    // should not have to scroll past the second.
    tabs_ = new QTabWidget(this);
    tabs_->setObjectName(QLatin1String(kTabsName));
    tabs_->addTab(scroll, tr("Versions"));

    const std::string_view application = capture::Version();
    update_ = new UpdatePage(
        QString::fromUtf8(application.data(),
                          static_cast<qsizetype>(application.size())),
        std::move(*device), this);
    update_->setObjectName(QLatin1String(kUpdatePageName));

    auto* update_container = new QScrollArea(this);
    update_container->setWidgetResizable(true);
    update_container->setFrameShape(QFrame::NoFrame);
    update_container->setWidget(update_);

    tabs_->addTab(update_container, tr("Update"));
    layout->addWidget(tabs_, 1);
  } else {
    layout->addWidget(scroll, 1);
  }

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
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

bool FirmwareDialog::CanClose() {
  if (update_ == nullptr || !update_->busy()) {
    return true;
  }

  // Explained rather than merely refused, and it says when it will be safe.
  // Closing this window is the obvious way to try to stop an update, and it
  // is the one way to leave a device half-written.
  QMessageBox::information(
      this, tr("Update in progress"),
      tr("The device is being updated. Closing this window now would leave "
         "the update unfinished.\n\nLeave the device plugged in. You can "
         "close this window as soon as the update has finished, or press "
         "Stop to end it safely."));
  return false;
}

void FirmwareDialog::closeEvent(QCloseEvent* event) {
  if (!CanClose()) {
    event->ignore();
    return;
  }
  QDialog::closeEvent(event);
}

void FirmwareDialog::reject() {
  if (!CanClose()) {
    return;
  }
  QDialog::reject();
}

}  // namespace ddd::gui
