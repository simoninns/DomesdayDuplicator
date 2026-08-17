/************************************************************************

    capture_panel.cpp

    Device selection, the destination, and the two controls
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_panel.h"

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
#include <filesystem>

#include "capture_controller.h"
#include "capture_failure_presenter.h"
#include "capture_format.h"
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

  // Neither the device nor the destination folder is here. Both are chosen
  // once and then left — the Duplicator does not move between USB ports and
  // captures do not move between drives — and a control that is set once does
  // not earn a row on the panel somebody works from. They are on the Capture
  // tab of File ▸ Settings…, and what stays here is what changes: the name,
  // the format, the limits, and the buttons.
  //
  // What the device is doing is still reported, on the status line at the
  // bottom — a device in recovery mode or on a USB 2 port says so there, which
  // is where somebody looks when nothing works.

  // The name and the way into everything else that goes into one. The button
  // sits beside the field rather than under it because the two are the same
  // decision approached from different ends: type a name, or say what the disc
  // is and let one be built.
  auto* name_row = new QWidget(contents);
  auto* name_layout = new QHBoxLayout(name_row);
  name_layout->setContentsMargins(0, 0, 0, 0);

  name_edit_ = new QLineEdit(name_row);
  name_edit_->setObjectName(QLatin1String(kNameEditName));
  name_edit_->setToolTip(
      tr("Leave empty to name each capture after the time it was taken, which "
         "is what keeps a folder of captures in order. Naming… builds the name "
         "from what the disc is instead."));
  name_layout->addWidget(name_edit_, 1);

  naming_button_ = new QPushButton(tr("Naming…"), name_row);
  naming_button_->setObjectName(QLatin1String(kNamingButtonName));
  // Its tooltip belongs to ApplyNamingAttention, which has two of them: one for
  // the ordinary state and one for the state where the button is asking to be
  // noticed.
  name_layout->addWidget(naming_button_);

  form->addRow(tr("Name"), name_row);

  name_taken_label_ = new QLabel(contents);
  name_taken_label_->setObjectName(QLatin1String(kNameTakenLabelName));
  name_taken_label_->setWordWrap(true);
  name_taken_label_->hide();
  form->addRow(QString(), name_taken_label_);

  format_combo_ = new QComboBox(contents);
  format_combo_->setObjectName(QLatin1String(kFormatComboName));
  format_combo_->addItem(
      tr("FLAC — %1").arg(QLatin1String(capture::kCaptureFileSuffix)),
      static_cast<int>(capture::CaptureOutputFormat::kFlac));
  format_combo_->addItem(
      tr("Uncompressed — %1")
          .arg(QLatin1String(capture::kSigned16BitCaptureFileSuffix)),
      static_cast<int>(capture::CaptureOutputFormat::kSigned16Bit));
  format_combo_->setToolTip(
      tr("FLAC roughly halves the file and carries the capture's provenance in "
         "its tags. Uncompressed is the same samples with no header and no "
         "encoder — twice the disk, nothing to keep up with, and nothing in "
         "the file to say what it is or what rate it was written at."));
  form->addRow(tr("Format"), format_combo_);

  sample_rate_combo_ = new QComboBox(contents);
  sample_rate_combo_->setObjectName(QLatin1String(kSampleRateComboName));
  // Named by what each rate is for rather than by what it does to the samples.
  // "2:1 decimated" is a fact about the implementation, and the choice being
  // made here is which format is being captured — which is the thing the user
  // actually knows when they arrive at this control.
  sample_rate_combo_->addItem(tr("40 MSPS for LaserDisc"),
                              capture::kUndecimatedFactor);
  sample_rate_combo_->addItem(tr("20 MSPS for VHS"),
                              capture::kTapeDecimationFactor);
  sample_rate_combo_->setToolTip(
      tr("The converter always runs at 40 MSPS. Decimating halves that in the "
         "FPGA, which low-passes the signal at 10 MHz first and then keeps "
         "every second sample — half the file, and enough for any tape RF, "
         "whose bandwidth is a fraction of a LaserDisc's. VHS names the common "
         "case rather than the only one: Betamax and Video8 are the same "
         "choice. Energy close to 10 MHz still folds down around it, so a "
         "signal with content up there should be captured at the full rate."));
  form->addRow(tr("Sample rate"), sample_rate_combo_);

  compression_spin_ = new QSpinBox(contents);
  compression_spin_->setObjectName(QLatin1String(kCompressionSpinName));
  compression_spin_->setRange(0, 8);
  compression_spin_->setToolTip(
      tr("FLAC compression, 0 to 8. The default of 8 gives the smallest file "
         "and is what a multithreaded libFLAC sustains at the device's full "
         "rate. Lower it if this machine cannot keep up — the buffer-queue and "
         "encoder-backlog figures in the Statistics panel are what say so."));
  form->addRow(tr("Compression"), compression_spin_);

  // The limit and the button that clears it, side by side. A limit is the one
  // setting here that is set for a single capture and then wants to be gone
  // again, and holding the down arrow from 40 minutes to "No limit" is forty
  // presses.
  auto* duration_row = new QWidget(contents);
  auto* duration_layout = new QHBoxLayout(duration_row);
  duration_layout->setContentsMargins(0, 0, 0, 0);

  duration_spin_ = new QSpinBox(duration_row);
  duration_spin_->setObjectName(QLatin1String(kDurationSpinName));
  duration_spin_->setRange(0, CaptureSettings::kMaximumDurationLimitMinutes);
  duration_spin_->setSpecialValueText(tr("No limit"));
  duration_spin_->setSuffix(tr(" min"));
  duration_spin_->setToolTip(
      tr("Stop the capture automatically after this long. The stop lands on a "
         "buffer boundary, so nothing is half-written."));
  duration_layout->addWidget(duration_spin_, 1);

  duration_reset_button_ = new QPushButton(tr("Reset"), duration_row);
  duration_reset_button_->setObjectName(
      QLatin1String(kDurationResetButtonName));
  duration_reset_button_->setToolTip(
      tr("Clear the limit, so the capture runs until it is stopped."));
  duration_layout->addWidget(duration_reset_button_);

  form->addRow(tr("Duration limit"), duration_row);

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

  // The other path, and named for what it does rather than for the window it
  // opens. It sits below the two manual controls because it replaces both: an
  // automatic capture starts and stops itself.
  automatic_button_ = new QPushButton(tr("Automatic capture…"), contents);
  automatic_button_->setObjectName(QLatin1String(kAutomaticButtonName));
  automatic_button_->setToolTip(
      tr("Examine the disc, name the capture from what was found, take the "
         "whole side, and see what was written. Needs a player connected."));
  layout->addWidget(automatic_button_);

  status_label_ = new QLabel(tr("No capture device attached"), contents);
  status_label_->setObjectName(QLatin1String(kStatusLabelName));
  status_label_->setWordWrap(true);
  layout->addWidget(status_label_);

  layout->addStretch();

  connect(monitor_button_, &QPushButton::clicked, this,
          &CapturePanel::OnMonitorButtonPressed);
  connect(capture_button_, &QPushButton::clicked, this,
          &CapturePanel::OnCaptureButtonPressed);
  connect(naming_button_, &QPushButton::clicked, this,
          &CapturePanel::OnNamingPressed);
  connect(automatic_button_, &QPushButton::clicked, this,
          &CapturePanel::AutomaticCaptureRequested);
  connect(duration_reset_button_, &QPushButton::clicked, this,
          &CapturePanel::OnDurationResetPressed);

  connect(name_edit_, &QLineEdit::editingFinished, this,
          &CapturePanel::ApplySettingsFromWidgets);

  // On every keystroke rather than on editingFinished, because the point of it
  // is to be read while the name is being decided rather than after.
  connect(name_edit_, &QLineEdit::textChanged, this,
          [this](const QString&) { RefreshNameNote(); });
  connect(format_combo_, &QComboBox::currentIndexChanged, this,
          [this](int) { ApplySettingsFromWidgets(); });
  connect(sample_rate_combo_, &QComboBox::currentIndexChanged, this,
          [this](int) { ApplySettingsFromWidgets(); });
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

  test_mode_ = settings.test_mode;
  name_edit_->setText(settings.capture_name);
  RefreshNameNote();

  // The folder is set elsewhere now, so this has to follow the settings rather
  // than a field on this panel: both the free-space figure and the
  // name-already-taken note are about a directory nothing here can see.
  RefreshFreeSpace();
  format_combo_->setCurrentIndex(
      format_combo_->findData(static_cast<int>(settings.output_format)));
  sample_rate_combo_->setCurrentIndex(
      sample_rate_combo_->findData(settings.decimation_factor));
  compression_spin_->setValue(settings.compression_level);
  // Rounded to the nearest whole minute for display. The stored value is in
  // seconds and is honoured as written; only what this box shows is coarser.
  duration_spin_->setValue((settings.duration_limit_seconds + 30) / 60);
  low_space_spin_->setValue(settings.low_space_warning_minutes);
  loading_ = false;

  UpdateNamePlaceholder();
  UpdateEnabledState();
  RefreshFreeSpace();
}

void CapturePanel::UpdateNamePlaceholder() {
  // What the capture would be called if this field were left alone, which is
  // the generated RF-Sample_<timestamp> until somebody has said what the disc
  // is and something more useful afterwards.
  const capture::CaptureNamingFields fields =
      controller_ != nullptr ? controller_->settings().naming
                             : capture::CaptureNamingFields{};

  name_edit_->setPlaceholderText(
      QString::fromStdString(capture::BuildCaptureStem(
          fields, std::string(), test_mode_, std::time(nullptr))));

  // In test mode the name is not a suggestion, so the field stops accepting
  // one. Disabled rather than silently ignored: a field that took text and
  // then did not use it would be a lie about what the application was going
  // to do.
  name_edit_->setEnabled(!test_mode_ && !capturing_);
}

void CapturePanel::ApplySettingsFromWidgets() {
  if (controller_ == nullptr || loading_) {
    return;
  }

  CaptureSettings settings = controller_->settings();
  settings.capture_name = name_edit_->text();
  settings.output_format = static_cast<capture::CaptureOutputFormat>(
      format_combo_->currentData().toInt());
  settings.decimation_factor = sample_rate_combo_->currentData().toInt();
  settings.compression_level = compression_spin_->value();
  settings.duration_limit_seconds = duration_spin_->value() * 60;
  settings.low_space_warning_minutes = low_space_spin_->value();

  if (settings != controller_->settings()) {
    controller_->SetSettings(settings);
  }

  // The format and the rate both change what a capture costs on disk, and the
  // free-space readout is a time rather than a size — so it has to be worked
  // out again here rather than waiting for the next two-second tick.
  UpdateEnabledState();
  RefreshFreeSpace();
}

void CapturePanel::OnDurationResetPressed() {
  // Straight to the special value, which the box shows as "No limit". Setting
  // it is enough: the valueChanged signal carries it into the settings by the
  // same route typing a number does.
  duration_spin_->setValue(0);
}

void CapturePanel::RefreshNameNote() {
  // Here as well as in UpdateEnabledState, because this is what runs on every
  // keystroke: without it the button stays coloured until the field loses
  // focus, which is long after the user has answered it.
  ApplyNamingAttention();

  const QString typed = name_edit_->text().trimmed();

  // Nothing to say about the generated name. It carries a timestamp, so it is
  // free by construction and a note about it would be a row that never went
  // away.
  if (typed.isEmpty() || test_mode_) {
    name_taken_label_->hide();
    return;
  }

  const QString directory = CaptureDirectory();
  const capture::CaptureOutputFormat format =
      controller_ == nullptr ? capture::CaptureOutputFormat::kFlac
                             : controller_->settings().output_format;

  const capture::CaptureDestination destination =
      capture::ResolveCaptureDestination(
          std::filesystem::path(directory.toStdString()), typed.toStdString(),
          false, std::time(nullptr), format);

  if (destination.as_requested) {
    name_taken_label_->hide();
    return;
  }

  name_taken_label_->setText(
      CaptureNameTakenNote(QString::fromStdString(destination.stem)));
  name_taken_label_->show();
}

void CapturePanel::OnNamingPressed() {
  // Asked for rather than opened here: the dialog may offer to ask the player
  // what the disc is, and only the main window holds both controllers.
  //
  // Nothing is collected on the way back. Every field applies as it is typed,
  // so by the time the dialog closes the settings already say what it said —
  // and the settings signal has already brought this panel's own view of them
  // up to date.
  emit NamingRequested();
}

QString CapturePanel::CaptureDirectory() const {
  return controller_ != nullptr
             ? controller_->settings().ResolvedCaptureDirectory()
             : QString();
}

void CapturePanel::RefreshFreeSpace() {
  const QString directory = CaptureDirectory();

  // Which volume, on the row rather than only in Settings. The figure is the
  // question people actually have, but "2:51:40 of capture" is worth nothing
  // if you cannot tell which drive it is about — and the folder is no longer
  // on this panel to be read off.
  free_space_label_->setToolTip(
      directory.isEmpty()
          ? QString()
          : tr("Where captures are written: %1\nChange it in File ▸ Settings…")
                .arg(directory));

  const capture::FreeSpace space =
      capture::AvailableSpace(directory.toStdString());

  if (!space.known) {
    // Not "0 bytes". A folder that does not exist yet is an ordinary thing to
    // have on the way to creating it, and a reading of zero would say the disk
    // was full. Named, because it cannot be seen from here.
    free_space_label_->setText(
        tr("Unknown — %1 does not exist yet").arg(directory));
    return;
  }

  // Through the statistics presenter's formatter rather than one of its own, so
  // this panel and the Statistics panel cannot end up saying different things
  // about the same volume. The rate goes with it for the same reason: an
  // uncompressed capture costs twice what a FLAC one does.
  free_space_label_->setText(FormatSpaceRemaining(
      space, controller_ != nullptr
                 ? controller_->settings().EstimatedBytesPerSecond()
                 : capture::kEstimatedCaptureBytesPerSecond));
}

void CapturePanel::OnDevicesChanged(
    const std::vector<ddd::capture::DeviceInfo>& devices) {
  devices_ = devices;

  // The device the engine would actually open, asked for the same way the
  // engine asks. This used to report on whichever entry a combo box on this
  // panel happened to be showing, which was the same device in every ordinary
  // case and not guaranteed to be — the engine prefers the first *capturable*
  // device where the list was simply in order. With the combo gone there is
  // one answer, and it is the engine's.
  const ddd::capture::DeviceInfo* const selected =
      controller_ != nullptr
          ? ddd::capture::SelectDevice(
                devices_,
                controller_->settings().preferred_device_path.toStdString())
          : nullptr;

  // Nothing capturable does not mean nothing attached. A device with no
  // firmware is reported for what it is rather than as an absence: saying "no
  // capture device attached" to somebody looking straight at one is how a user
  // decides the application is broken.
  const ddd::capture::DeviceInfo* const shown =
      selected != nullptr ? selected
                          : (devices_.empty() ? nullptr : &devices_.front());

  if (shown == nullptr) {
    status_label_->setText(tr("No capture device attached"));
  } else if (!shown->is_application()) {
    status_label_->setText(
        tr("This device has no firmware installed, so it cannot capture "
           "yet. Open Tools ▸ Firmware… to program it."));
  } else if (!shown->CanCarryCapture()) {
    status_label_->setText(
        tr("Connected at insufficient speed. This device is on a USB 2 port "
           "and cannot carry 80 MB/s — move it to a USB 3 port."));
  } else {
    status_label_->setText(tr("Ready"));
  }

  UpdateEnabledState();
}

void CapturePanel::SetAutomaticCaptureAvailable(bool available) {
  automatic_available_ = available;
  UpdateEnabledState();
}

bool CapturePanel::NamingWantsAttention() const {
  if (controller_ == nullptr) {
    return false;
  }

  // Not in test mode: the name is forced to TestData_ there and the naming
  // fields cannot change it, so pointing at the button would be pointing at a
  // control that has nothing to offer.
  if (test_mode_) {
    return false;
  }

  // Not once the capture is running. The file is open under whatever name it
  // got, and a control saying "you may have meant to name this" after the fact
  // is only a reproach.
  if (capturing_) {
    return false;
  }

  // The field rather than the setting behind it, so the colour goes as the name
  // is typed rather than when the field loses focus. The disc details come from
  // the settings because that is where the naming dialog puts them, and it
  // writes them as they are typed.
  return name_edit_->text().trimmed().isEmpty() &&
         !controller_->settings().naming.DescribesDisc();
}

void CapturePanel::ApplyNamingAttention() {
  const bool dark = theme_tokens::IsDarkPalette(palette());
  const bool wanted = NamingWantsAttention();

  naming_button_->setStyleSheet(
      wanted ? ActiveButtonStyle(
                   theme_tokens::PlotColor(
                       theme_tokens::PlotColorToken::kAttention, dark),
                   natural_button_height_)
             : QString());

  // The colour says "look here"; the tooltip says why, and what will happen if
  // it is ignored. A coloured control with no explanation is a control somebody
  // has to guess about.
  naming_button_->setToolTip(
      wanted ? tr("Nothing has been said about this disc, so the capture will "
                  "be named after the time it was taken — “%1”. That is a "
                  "perfectly good way to work; this is only here in case you "
                  "meant to fill it in.")
                   .arg(name_edit_->placeholderText())
             : tr("What the disc is: title, type, standard, side, notes. All "
                  "of it is recorded in the capture's metadata file, and some "
                  "of it can be folded into the file name."));
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
    ApplyNamingAttention();
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

void CapturePanel::UpdateEnabledState() {
  // Which device, asked the way the engine asks — and then both questions
  // about it. SelectDevice's kCaptureCapable means "running capture firmware"
  // and says nothing about the link: a device on a USB 2 port is selected by
  // it and still cannot carry 80 MB/s. Two conditions, because there are two
  // ways for an attached Duplicator not to be usable.
  const ddd::capture::DeviceInfo* const selected =
      controller_ != nullptr
          ? ddd::capture::SelectDevice(
                devices_,
                controller_->settings().preferred_device_path.toStdString())
          : nullptr;
  const bool have_usable_device = selected != nullptr &&
                                  selected->is_application() &&
                                  selected->CanCarryCapture();

  // Not while a capture is running, and this is the whole of the fix for a
  // button that used to throw away the recording.
  //
  // A capture is this stream with a file on the end of it, so stopping the
  // stream necessarily ends the capture — the pipeline finalises the sink on
  // its way out. That made "Stop monitoring" a second, unlabelled stop button
  // for the capture, sitting directly above the real one. Nothing about it said
  // so, and pressing it lost the rest of the side.
  //
  // Disabled rather than made to ask "are you sure": the capture already has a
  // stop of its own, and stopping it leaves the stream running, so there is
  // never anything a user wanted that this route was the only way to.
  monitor_button_->setEnabled(!capturing_ &&
                              (monitoring_ || have_usable_device));
  monitor_button_->setToolTip(
      capturing_
          ? tr("Stop the capture first. Monitoring is the stream the capture "
               "is being written from, so stopping it would end the capture "
               "too — and stopping the capture leaves the stream running.")
          : QString());

  // Capture can be started from idle — it starts the stream itself — so the
  // same condition governs it.
  capture_button_->setEnabled(capturing_ || have_usable_device);

  // The automatic path needs both pieces of equipment, and not a capture
  // already running: it drives the player and the engine together, and there is
  // one of each.
  automatic_button_->setEnabled(automatic_available_ && have_usable_device &&
                                !capturing_);

  format_combo_->setEnabled(!capturing_);
  name_edit_->setEnabled(!capturing_ && !test_mode_);

  // Locked from the moment monitoring starts, on the same terms as test mode
  // and for the same reason: it is written to the device's decimation register
  // before the stream is opened and there is no way to change it under a
  // running stream. Left editable while monitoring it would appear to work and
  // do nothing — and the analysis panels, which scale their axes by this, would
  // then be drawing a rate the device is not sending.
  //
  // It works in test mode as well: the gateware generates its pattern
  // downstream of the decimator, so a decimated test capture is an unbroken
  // ramp at the decimated rate.
  sample_rate_combo_->setEnabled(!monitoring_);

  // Nothing to compress in the uncompressed format, so the level stops being a
  // setting rather than becoming one that is quietly ignored.
  compression_spin_->setEnabled(
      !capturing_ && format_combo_->currentData().toInt() ==
                         static_cast<int>(capture::CaptureOutputFormat::kFlac));

  // These three are read as the capture runs rather than when it starts, so all
  // stay live: noticing halfway through that the disk is filling and wanting a
  // warning sooner is a reasonable thing to want, and so is deciding mid-side
  // that the limit should go.
  duration_spin_->setEnabled(true);
  duration_reset_button_->setEnabled(true);
  low_space_spin_->setEnabled(true);

  // Everything that changes whether the nudge applies — the settings arriving,
  // test mode going on or off, a capture starting or stopping — comes through
  // here.
  ApplyNamingAttention();
}

}  // namespace ddd::gui
