/************************************************************************

    capture_panel.cpp

    Device selection and the monitor control
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "capture_controller.h"

namespace ddd::gui {

CapturePanel::CapturePanel(CaptureController* controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(10);

  auto* form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignLeft);

  device_combo_ = new QComboBox(this);
  device_combo_->setObjectName(QLatin1String(kDeviceComboName));
  form->addRow(tr("Device"), device_combo_);

  test_mode_box_ = new QCheckBox(tr("Test mode"), this);
  test_mode_box_->setObjectName(QLatin1String(kTestModeBoxName));
  test_mode_box_->setToolTip(
      tr("Ask the gateware for its internal test pattern instead of the RF "
         "input, so that the whole capture path can be checked against a "
         "known signal."));
  form->addRow(QString(), test_mode_box_);

  layout->addLayout(form);

  monitor_button_ = new QPushButton(tr("Start monitoring"), this);
  monitor_button_->setObjectName(QLatin1String(kMonitorButtonName));
  layout->addWidget(monitor_button_);

  status_label_ = new QLabel(tr("No capture device attached"), this);
  status_label_->setObjectName(QLatin1String(kStatusLabelName));
  status_label_->setWordWrap(true);
  layout->addWidget(status_label_);

  layout->addStretch();

  connect(monitor_button_, &QPushButton::clicked, this,
          &CapturePanel::OnMonitorButtonPressed);
  connect(device_combo_, &QComboBox::currentIndexChanged, this,
          &CapturePanel::OnDeviceSelected);
  connect(test_mode_box_, &QCheckBox::toggled, this,
          &CapturePanel::OnTestModeToggled);

  if (controller_ != nullptr) {
    connect(controller_, &CaptureController::DevicesChanged, this,
            &CapturePanel::OnDevicesChanged);
    connect(controller_, &CaptureController::MonitoringChanged, this,
            &CapturePanel::OnMonitoringChanged);
    test_mode_box_->setChecked(controller_->settings().test_mode);
  }

  UpdateEnabledState();
}

void CapturePanel::OnDevicesChanged(
    const std::vector<ddd::capture::DeviceInfo>& devices) {
  devices_ = devices;

  const QString previous = controller_ != nullptr
                               ? controller_->settings().preferred_device_path
                               : QString();

  // Rebuilt rather than diffed. The list is at most a handful of entries and
  // changes only when someone plugs something in, so the cost of getting it
  // exactly right is nothing and the cost of a subtly wrong diff is a device
  // that cannot be selected.
  const QSignalBlocker blocker(device_combo_);
  device_combo_->clear();

  int restore_index = -1;
  for (size_t index = 0; index < devices_.size(); ++index) {
    const ddd::capture::DeviceInfo& device = devices_[index];
    const QString path = QString::fromStdString(device.path);

    QString label = path;
    if (!device.CanCarryCapture()) {
      // Said in the list rather than only when the user presses the button. A
      // device on the wrong port is the thing they need to know about before
      // they wonder why nothing works.
      label += tr(" — connected at insufficient speed (%1)")
                   .arg(QString::fromUtf8(
                       ddd::capture::DeviceSpeedName(device.speed)));
    }

    device_combo_->addItem(label, path);
    if (path == previous) {
      restore_index = static_cast<int>(index);
    }
  }

  if (restore_index >= 0) {
    device_combo_->setCurrentIndex(restore_index);
  }

  if (devices_.empty()) {
    status_label_->setText(tr("No capture device attached"));
  } else {
    const ddd::capture::DeviceInfo* const selected =
        &devices_[static_cast<size_t>(
            std::max(0, device_combo_->currentIndex()))];
    if (!selected->CanCarryCapture()) {
      status_label_->setText(
          tr("Connected at insufficient speed. This device is on a USB 2 port "
             "and cannot carry 80 MB/s — move it to a USB 3 port."));
    } else {
      status_label_->setText(tr("Ready"));
    }
  }

  UpdateEnabledState();
}

void CapturePanel::OnMonitoringChanged(bool monitoring) {
  monitoring_ = monitoring;
  monitor_button_->setText(monitoring ? tr("Stop monitoring")
                                      : tr("Start monitoring"));
  if (monitoring) {
    status_label_->setText(tr("Monitoring"));
  } else if (!devices_.empty()) {
    status_label_->setText(tr("Ready"));
  }
  UpdateEnabledState();
}

void CapturePanel::OnMonitorButtonPressed() {
  if (controller_ == nullptr) {
    return;
  }
  if (monitoring_) {
    controller_->StopMonitoring();
  } else {
    controller_->StartMonitoring();
  }
}

void CapturePanel::OnDeviceSelected(int index) {
  if (controller_ == nullptr || index < 0) {
    return;
  }
  CaptureSettings settings = controller_->settings();
  settings.preferred_device_path = device_combo_->itemData(index).toString();
  controller_->SetSettings(settings);
}

void CapturePanel::OnTestModeToggled(bool enabled) {
  if (controller_ == nullptr) {
    return;
  }
  CaptureSettings settings = controller_->settings();
  settings.test_mode = enabled;
  controller_->SetSettings(settings);
}

void CapturePanel::UpdateEnabledState() {
  const bool have_usable_device =
      !devices_.empty() &&
      devices_[static_cast<size_t>(std::max(0, device_combo_->currentIndex()))]
          .CanCarryCapture();

  monitor_button_->setEnabled(monitoring_ || have_usable_device);

  // Locked down while streaming, because neither can be changed without
  // stopping: the device is open and the mode change would land at an
  // unpredictable point in the stream.
  device_combo_->setEnabled(!monitoring_);
  test_mode_box_->setEnabled(!monitoring_);
}

}  // namespace ddd::gui
