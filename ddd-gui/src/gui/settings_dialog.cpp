/************************************************************************

    settings_dialog.cpp

    Buffer queue, transfer mode and preferred device
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "settings_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "disk_buffer_ring.h"
#include "front_end_gain.h"
#include "gain_choices.h"
#include "update_text.h"

namespace ddd::gui {
namespace {

// The queue sizes offered. Not a free-form number: the useful range is narrow,
// the units are confusing to type, and the figure that matters to a user is not
// the megabytes but the seconds of write stall they buy — which is why both are
// shown.
struct QueueChoice {
  size_t bytes;
  const char* label;
};

constexpr QueueChoice kQueueChoices[] = {
    {size_t{64} << 20, "64 MB — 0.8 seconds of slack"},
    {size_t{128} << 20, "128 MB — 1.6 seconds of slack"},
    {size_t{256} << 20, "256 MB — 3.2 seconds of slack (default)"},
    {size_t{512} << 20, "512 MB — 6.5 seconds of slack"},
};

}  // namespace

SettingsDialog::SettingsDialog(
    const CaptureSettings& settings,
    const std::vector<ddd::capture::DeviceInfo>& devices, QWidget* parent)
    : QDialog(parent), settings_(settings) {
  setWindowTitle(tr("Capture settings"));

  auto* layout = new QVBoxLayout(this);
  auto* form = new QFormLayout();

  queue_size_ = new QComboBox(this);
  queue_size_->setObjectName(QLatin1String(kQueueSizeComboName));
  for (const QueueChoice& choice : kQueueChoices) {
    queue_size_->addItem(tr(choice.label),
                         static_cast<qulonglong>(choice.bytes));
    if (choice.bytes == settings_.queue_size_bytes) {
      queue_size_->setCurrentIndex(queue_size_->count() - 1);
    }
  }
  form->addRow(tr("Buffer queue"), queue_size_);

  transfer_mode_ = new QComboBox(this);
  transfer_mode_->setObjectName(QLatin1String(kTransferModeComboName));
  transfer_mode_->addItem(tr("Many small transfers (recommended)"), true);
  transfer_mode_->addItem(tr("One transfer per buffer"), false);
  transfer_mode_->setCurrentIndex(settings_.small_transfers ? 0 : 1);
  transfer_mode_->setToolTip(tr(
      "Small transfers keep several reads outstanding at once, so the "
      "device always has somewhere to put the next packet. One transfer per "
      "buffer is simpler and slightly cheaper, but leaves a gap between each "
      "transfer completing and the next being submitted."));
  form->addRow(tr("USB transfers"), transfer_mode_);

  device_ = new QComboBox(this);
  device_->setObjectName(QLatin1String(kDeviceComboName));
  device_->addItem(tr("Whichever is attached"), QString());
  for (const ddd::capture::DeviceInfo& info : devices) {
    const QString path = QString::fromStdString(info.path);

    // Named for what it is, as in the Capture panel's list. A device in
    // recovery mode can still be preferred — it is the same physical port,
    // and it will be a capture device again once it has been programmed.
    device_->addItem(path + DeviceListPersonalitySuffix(info.personality),
                     path);
    if (path == settings_.preferred_device_path) {
      device_->setCurrentIndex(device_->count() - 1);
    }
  }
  form->addRow(tr("Preferred device"), device_);

  front_end_gain_ = new QComboBox(this);
  front_end_gain_->setObjectName(QLatin1String(kFrontEndGainComboName));

  // First in the list and the default, and it is not a placeholder: an
  // application that has not been told the switch positions genuinely does not
  // know them, and every level display says so by staying in converter codes
  // until it has been told. Offering a guess here would put millivolt figures
  // on screen that could be wrong by a factor of four with nothing to reveal
  // it.
  front_end_gain_->addItem(
      tr("Not declared — show converter codes"),
      static_cast<uint>(ddd::analysis::kUndeclaredSwitchPattern));
  for (const GainChoice& choice : FrontEndGainChoices()) {
    front_end_gain_->addItem(choice.label,
                             static_cast<uint>(choice.switch_pattern));
    if (choice.switch_pattern == settings_.front_end_gain_switches) {
      front_end_gain_->setCurrentIndex(front_end_gain_->count() - 1);
    }
  }
  front_end_gain_->setToolTip(
      tr("The SW401 gain switch on the Domesday Duplicator board. It is "
         "mechanical and the application cannot read it, so signal levels are "
         "shown in converter codes until it is set here. Getting it wrong "
         "changes only what the levels are labelled — never what is captured, "
         "and never whether clipping is detected."));
  form->addRow(tr("Front-end gain"), front_end_gain_);

  layout->addLayout(form);

  // The distinction matters and is worth the sentence: the buffer and transfer
  // settings resize things a running capture is using and so cannot take effect
  // until the next one, while the gain declaration only changes what a number
  // is labelled and takes effect immediately.
  auto* note = new QLabel(
      tr("The buffer and transfer settings apply to the next capture. The "
         "front-end gain applies at once, including to a capture already "
         "running."),
      this);
  note->setWordWrap(true);
  note->setForegroundRole(QPalette::PlaceholderText);
  layout->addWidget(note);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
}

CaptureSettings SettingsDialog::Settings() const {
  CaptureSettings result = settings_;
  result.queue_size_bytes =
      static_cast<size_t>(queue_size_->currentData().toULongLong());
  result.small_transfers = transfer_mode_->currentData().toBool();
  result.preferred_device_path = device_->currentData().toString();
  result.front_end_gain_switches =
      static_cast<uint8_t>(front_end_gain_->currentData().toUInt() & 0xFFU);
  return result;
}

}  // namespace ddd::gui
