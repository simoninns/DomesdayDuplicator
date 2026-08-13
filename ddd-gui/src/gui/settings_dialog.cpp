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
    device_->addItem(path, path);
    if (path == settings_.preferred_device_path) {
      device_->setCurrentIndex(device_->count() - 1);
    }
  }
  form->addRow(tr("Preferred device"), device_);

  layout->addLayout(form);

  auto* note = new QLabel(
      tr("Changes apply to the next capture. Nothing here can be changed while "
         "one is running."),
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
  return result;
}

}  // namespace ddd::gui
