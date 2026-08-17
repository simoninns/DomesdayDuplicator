/************************************************************************

    capture_naming_dialog.cpp

    What the disc is, for the file name and for the sidecar
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_naming_dialog.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace ddd::gui {

CaptureNamingDialog::CaptureNamingDialog(CaptureController* controller,
                                         PlayerController* player,
                                         QWidget* parent)
    : QDialog(parent) {
  setWindowTitle(tr("Capture naming"));
  setSizeGripEnabled(true);

  auto* layout = new QVBoxLayout(this);

  form_ = new CaptureNamingForm(controller, player, this);
  layout->addWidget(form_);

  auto* buttons = new QDialogButtonBox(this);
  QPushButton* const clear =
      buttons->addButton(tr("Clear all fields"), QDialogButtonBox::ResetRole);
  clear->setObjectName(QLatin1String(kClearButtonName));
  clear->setToolTip(
      tr("Empty every field and untick every box, ready for the next disc."));
  buttons->addButton(QDialogButtonBox::Close);

  // accept() rather than reject(), because there is nothing to reject: every
  // field applied as it was typed, so closing is only closing.
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
  connect(clear, &QPushButton::clicked, form_,
          &CaptureNamingForm::ClearAllFields);

  layout->addWidget(buttons);
}

}  // namespace ddd::gui
