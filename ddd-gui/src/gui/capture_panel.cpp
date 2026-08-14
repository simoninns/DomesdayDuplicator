/************************************************************************

    capture_panel.cpp

    Device selection, the destination, and the two controls
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <ctime>

#include "capture_controller.h"
#include "capture_naming.h"
#include "free_space.h"
#include "statistics_presenter.h"
#include "theme_color_tokens.h"
#include "update_text.h"

namespace ddd::gui {
namespace {

// A button that is doing something, coloured so its state reads from across a
// room rather than only by reading its label.
//
// A stylesheet rather than QPalette::Button, which would be the tidier way to
// do it: the macOS style ignores the button palette entirely, so on one of the
// three platforms this ships on the colour would simply not appear. Because a
// stylesheet takes the button off the native drawing path, the rule has to put
// the border, radius and padding back — otherwise the button becomes a flat
// rectangle, which looks like a defect rather than like a state.
// natural_height is the height the button has with no stylesheet on it, and it
// has to be passed in and pinned: a stylesheet takes the button off the native
// drawing path, and the size the stylesheet path computes from border and
// padding alone is not the size the platform style chose. Without this the
// button changes height at the moment it is pressed and the whole panel shifts
// under the pointer — which is a far more distracting thing than the colour is
// a useful one.
//
// min-height in a Qt stylesheet is the content rect, so the border is
// subtracted from it.
QString ActiveButtonStyle(const QColor& colour, int natural_height) {
  const QColor text = theme_tokens::ReadableTextOn(colour);
  constexpr int kBorderWidth = 1;

  return QStringLiteral(
             "QPushButton {"
             "  background-color: %1;"
             "  color: %2;"
             "  border: %5px solid %3;"
             "  border-radius: 3px;"
             "  padding: 0px;"
             "  min-height: %6px;"
             "}"
             "QPushButton:hover { background-color: %4; }"
             "QPushButton:pressed { background-color: %3; }")
      .arg(colour.name(QColor::HexRgb), text.name(QColor::HexRgb),
           colour.darker(125).name(QColor::HexRgb),
           colour.lighter(112).name(QColor::HexRgb))
      .arg(kBorderWidth)
      .arg(std::max(0, natural_height - (2 * kBorderWidth)));
}

}  // namespace

CapturePanel::CapturePanel(CaptureController* controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
  // Inside a scroll area, for the reason the Statistics panel is: without one,
  // this panel's minimum height is the height of every row it contains, which
  // it then demands from the dock column it shares — and the separators above
  // and below it stop moving. The panel gained eight rows with the capture
  // controls, which is exactly when that starts to bite.
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);

  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  outer->addWidget(scroll);

  auto* contents = new QWidget(scroll);
  scroll->setWidget(contents);

  auto* layout = new QVBoxLayout(contents);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(10);

  auto* form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignLeft);

  device_combo_ = new QComboBox(contents);
  device_combo_->setObjectName(QLatin1String(kDeviceComboName));
  form->addRow(tr("Device"), device_combo_);

  test_mode_box_ = new QCheckBox(tr("Test mode"), contents);
  test_mode_box_->setObjectName(QLatin1String(kTestModeBoxName));
  test_mode_box_->setToolTip(
      tr("Ask the gateware for its internal test pattern instead of the RF "
         "input, so that the whole capture path can be checked against a "
         "known signal. Test captures are always named TestData_ so they "
         "cannot be mistaken for a recording."));
  form->addRow(QString(), test_mode_box_);

  // --- Where it goes -------------------------------------------------------

  auto* directory_row = new QWidget(contents);
  auto* directory_layout = new QHBoxLayout(directory_row);
  directory_layout->setContentsMargins(0, 0, 0, 0);

  directory_edit_ = new QLineEdit(directory_row);
  directory_edit_->setObjectName(QLatin1String(kDirectoryEditName));
  directory_layout->addWidget(directory_edit_, 1);

  browse_button_ = new QPushButton(tr("Browse…"), directory_row);
  browse_button_->setObjectName(QLatin1String(kBrowseButtonName));
  directory_layout->addWidget(browse_button_);

  form->addRow(tr("Folder"), directory_row);

  name_edit_ = new QLineEdit(contents);
  name_edit_->setObjectName(QLatin1String(kNameEditName));
  name_edit_->setToolTip(
      tr("Leave empty to name each capture after the time it was taken, which "
         "is what keeps a folder of captures in order."));
  form->addRow(tr("Name"), name_edit_);

  compression_spin_ = new QSpinBox(contents);
  compression_spin_->setObjectName(QLatin1String(kCompressionSpinName));
  compression_spin_->setRange(0, 8);
  compression_spin_->setToolTip(
      tr("FLAC compression, 0 to 8. The default of 8 gives the smallest file "
         "and is what a multithreaded libFLAC sustains at the device's full "
         "rate. Lower it if this machine cannot keep up — the buffer-queue and "
         "encoder-backlog figures in the Statistics panel are what say so."));
  form->addRow(tr("Compression"), compression_spin_);

  duration_spin_ = new QSpinBox(contents);
  duration_spin_->setObjectName(QLatin1String(kDurationSpinName));
  duration_spin_->setRange(0, CaptureSettings::kMaximumDurationLimitMinutes);
  duration_spin_->setSpecialValueText(tr("No limit"));
  duration_spin_->setSuffix(tr(" min"));
  duration_spin_->setToolTip(
      tr("Stop the capture automatically after this long. The stop lands on a "
         "buffer boundary, so nothing is half-written."));
  form->addRow(tr("Duration limit"), duration_spin_);

  low_space_spin_ = new QSpinBox(contents);
  low_space_spin_->setObjectName(QLatin1String(kLowSpaceSpinName));
  low_space_spin_->setRange(0, CaptureSettings::kMaximumLowSpaceWarningMinutes);
  low_space_spin_->setSpecialValueText(tr("Never"));
  low_space_spin_->setSuffix(tr(" min"));
  low_space_spin_->setToolTip(
      tr("Warn when the destination volume has less than this much capture "
         "time left. A warning only — the capture is not stopped."));
  form->addRow(tr("Warn below"), low_space_spin_);

  free_space_label_ = new QLabel(contents);
  free_space_label_->setObjectName(QLatin1String(kFreeSpaceLabelName));
  free_space_label_->setWordWrap(true);
  form->addRow(tr("Free space"), free_space_label_);

  layout->addLayout(form);

  monitor_button_ = new QPushButton(tr("Start monitoring"), contents);
  monitor_button_->setObjectName(QLatin1String(kMonitorButtonName));
  layout->addWidget(monitor_button_);

  capture_button_ = new QPushButton(tr("Start capture"), contents);
  capture_button_->setObjectName(QLatin1String(kCaptureButtonName));
  layout->addWidget(capture_button_);

  status_label_ = new QLabel(tr("No capture device attached"), contents);
  status_label_->setObjectName(QLatin1String(kStatusLabelName));
  status_label_->setWordWrap(true);
  layout->addWidget(status_label_);

  layout->addStretch();

  connect(monitor_button_, &QPushButton::clicked, this,
          &CapturePanel::OnMonitorButtonPressed);
  connect(capture_button_, &QPushButton::clicked, this,
          &CapturePanel::OnCaptureButtonPressed);
  connect(device_combo_, &QComboBox::currentIndexChanged, this,
          &CapturePanel::OnDeviceSelected);
  connect(test_mode_box_, &QCheckBox::toggled, this,
          &CapturePanel::OnTestModeToggled);
  connect(browse_button_, &QPushButton::clicked, this,
          &CapturePanel::OnBrowsePressed);

  connect(directory_edit_, &QLineEdit::editingFinished, this,
          &CapturePanel::ApplySettingsFromWidgets);
  connect(name_edit_, &QLineEdit::editingFinished, this,
          &CapturePanel::ApplySettingsFromWidgets);
  connect(compression_spin_, &QSpinBox::valueChanged, this,
          [this](int) { ApplySettingsFromWidgets(); });
  connect(duration_spin_, &QSpinBox::valueChanged, this,
          [this](int) { ApplySettingsFromWidgets(); });
  connect(low_space_spin_, &QSpinBox::valueChanged, this,
          [this](int) { ApplySettingsFromWidgets(); });

  if (controller_ != nullptr) {
    connect(controller_, &CaptureController::DevicesChanged, this,
            &CapturePanel::OnDevicesChanged);
    connect(controller_, &CaptureController::MonitoringChanged, this,
            &CapturePanel::OnMonitoringChanged);
    connect(controller_, &CaptureController::CapturingChanged, this,
            &CapturePanel::OnCapturingChanged);
    connect(controller_, &CaptureController::SettingsChanged, this,
            [this](const CaptureSettings&) { ShowSettings(); });
  }

  // Measured before anything is ever styled, and kept, so that colouring a
  // button cannot change its size. See ActiveButtonStyle.
  natural_button_height_ = monitor_button_->sizeHint().height();

  ShowSettings();

  free_space_timer_ = new QTimer(this);
  free_space_timer_->setInterval(kFreeSpaceIntervalMilliseconds);
  connect(free_space_timer_, &QTimer::timeout, this,
          &CapturePanel::RefreshFreeSpace);
  free_space_timer_->start();
  RefreshFreeSpace();

  UpdateEnabledState();
}

void CapturePanel::ShowSettings() {
  if (controller_ == nullptr) {
    return;
  }

  loading_ = true;
  const CaptureSettings& settings = controller_->settings();

  test_mode_box_->setChecked(settings.test_mode);
  directory_edit_->setText(settings.ResolvedCaptureDirectory());
  name_edit_->setText(settings.capture_name);
  compression_spin_->setValue(settings.compression_level);
  // Rounded to the nearest whole minute for display. The stored value is in
  // seconds and is honoured as written; only what this box shows is coarser.
  duration_spin_->setValue((settings.duration_limit_seconds + 30) / 60);
  low_space_spin_->setValue(settings.low_space_warning_minutes);
  loading_ = false;

  UpdateNamePlaceholder();
}

void CapturePanel::UpdateNamePlaceholder() {
  const bool test_mode = test_mode_box_->isChecked();
  name_edit_->setPlaceholderText(QString::fromStdString(
      capture::DefaultCaptureStem(test_mode, std::time(nullptr))));

  // In test mode the name is not a suggestion, so the field stops accepting
  // one. Disabled rather than silently ignored: a field that took text and
  // then did not use it would be a lie about what the application was going
  // to do.
  name_edit_->setEnabled(!test_mode && !capturing_);
}

void CapturePanel::ApplySettingsFromWidgets() {
  if (controller_ == nullptr || loading_) {
    return;
  }

  CaptureSettings settings = controller_->settings();
  settings.capture_directory = directory_edit_->text();
  settings.capture_name = name_edit_->text();
  settings.compression_level = compression_spin_->value();
  settings.duration_limit_seconds = duration_spin_->value() * 60;
  settings.low_space_warning_minutes = low_space_spin_->value();

  if (settings != controller_->settings()) {
    controller_->SetSettings(settings);
  }

  RefreshFreeSpace();
}

void CapturePanel::OnBrowsePressed() {
  const QString chosen = QFileDialog::getExistingDirectory(
      this, tr("Where captures are written"), directory_edit_->text());
  if (chosen.isEmpty()) {
    return;
  }

  directory_edit_->setText(chosen);
  ApplySettingsFromWidgets();
}

void CapturePanel::RefreshFreeSpace() {
  const QString directory = directory_edit_->text();

  const capture::FreeSpace space =
      capture::AvailableSpace(directory.toStdString());

  if (!space.known) {
    // Not "0 bytes". A folder that does not exist yet is an ordinary thing for
    // someone to have typed on the way to creating it, and a reading of zero
    // would say the disk was full.
    free_space_label_->setText(tr("Unknown — this folder does not exist yet"));
    return;
  }

  // Through the statistics presenter's formatter rather than one of its own, so
  // this panel and the Statistics panel cannot end up saying different things
  // about the same volume.
  free_space_label_->setText(FormatSpaceRemaining(space));
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
    if (!device.is_application()) {
      // A device with no firmware is listed rather than hidden, and named for
      // what it is. Hiding it would report "no device attached" to somebody
      // looking straight at one, which is the state in which they most need
      // to be told what to do next.
      label += DeviceListPersonalitySuffix(device.personality);
    } else if (!device.CanCarryCapture()) {
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
    if (!selected->is_application()) {
      status_label_->setText(
          tr("This device has no firmware installed, so it cannot capture "
             "yet. Open Help ▸ Firmware… to program it."));
    } else if (!selected->CanCarryCapture()) {
      status_label_->setText(
          tr("Connected at insufficient speed. This device is on a USB 2 port "
             "and cannot carry 80 MB/s — move it to a USB 3 port."));
    } else {
      status_label_->setText(tr("Ready"));
    }
  }

  UpdateEnabledState();
}

void CapturePanel::ApplyButtonColours() {
  // Read from this widget's own palette rather than from the theme controller,
  // so the buttons follow a theme change through the palette-change event they
  // already get, with no signal to connect and nothing to be handed.
  const bool dark = theme_tokens::IsDarkPalette(palette());

  monitor_button_->setStyleSheet(
      monitoring_
          ? ActiveButtonStyle(
                theme_tokens::PlotColor(
                    theme_tokens::PlotColorToken::kMonitoringActive, dark),
                natural_button_height_)
          : QString());

  capture_button_->setStyleSheet(
      capturing_
          ? ActiveButtonStyle(
                theme_tokens::PlotColor(
                    theme_tokens::PlotColorToken::kCapturingActive, dark),
                natural_button_height_)
          : QString());
}

void CapturePanel::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);

  // A theme switched while a capture is running has to recolour the buttons
  // that are already coloured, or the green chosen for a light window stays on
  // a dark one.
  if (event != nullptr && event->type() == QEvent::PaletteChange) {
    ApplyButtonColours();
  }
}

void CapturePanel::OnMonitoringChanged(bool monitoring) {
  monitoring_ = monitoring;
  monitor_button_->setText(monitoring ? tr("Stop monitoring")
                                      : tr("Start monitoring"));
  ApplyButtonColours();
  if (monitoring) {
    if (!capturing_) {
      status_label_->setText(tr("Monitoring"));
    }
  } else if (!devices_.empty()) {
    status_label_->setText(tr("Ready"));
  }
  UpdateEnabledState();
}

void CapturePanel::OnCapturingChanged(bool capturing,
                                      const QString& file_path) {
  capturing_ = capturing;
  capture_button_->setText(capturing ? tr("Stop capture")
                                     : tr("Start capture"));
  ApplyButtonColours();

  if (capturing) {
    // The whole path, not just the name. Somebody who has just started a
    // forty-minute capture should be able to see that it is going to the drive
    // they meant without leaving the application.
    status_label_->setText(tr("Capturing to %1").arg(file_path));
  } else if (monitoring_) {
    status_label_->setText(tr("Monitoring"));
  }

  UpdateNamePlaceholder();
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

void CapturePanel::OnCaptureButtonPressed() {
  if (controller_ == nullptr) {
    return;
  }
  if (capturing_) {
    controller_->StopCapture();
  } else {
    controller_->StartCapture();
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
  UpdateNamePlaceholder();

  if (controller_ == nullptr || loading_) {
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
          .is_application() &&
      devices_[static_cast<size_t>(std::max(0, device_combo_->currentIndex()))]
          .CanCarryCapture();

  monitor_button_->setEnabled(monitoring_ || have_usable_device);

  // Capture can be started from idle — it starts the stream itself — so the
  // same condition governs it. What it cannot do is stop monitoring, so a
  // capture in progress keeps the monitor button live and stopping the stream
  // ends the capture with it.
  capture_button_->setEnabled(capturing_ || have_usable_device);

  // Locked down while streaming, because neither can be changed without
  // stopping: the device is open and the mode change would land at an
  // unpredictable point in the stream.
  device_combo_->setEnabled(!monitoring_);
  test_mode_box_->setEnabled(!monitoring_);

  // The destination is fixed for the duration of a capture — the file is
  // already open — but stays editable while merely monitoring, which is when
  // somebody is setting up for the capture they are about to take.
  directory_edit_->setEnabled(!capturing_);
  browse_button_->setEnabled(!capturing_);
  compression_spin_->setEnabled(!capturing_);
  name_edit_->setEnabled(!capturing_ && !test_mode_box_->isChecked());

  // These two are read as the capture runs rather than when it starts, so both
  // stay live: noticing halfway through that the disk is filling and wanting a
  // warning sooner is a reasonable thing to want.
  duration_spin_->setEnabled(true);
  low_space_spin_->setEnabled(true);
}

}  // namespace ddd::gui
