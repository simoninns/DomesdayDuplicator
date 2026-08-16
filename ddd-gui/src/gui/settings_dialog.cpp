/************************************************************************

    settings_dialog.cpp

    The application's settings, grouped by what they are about
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "settings_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QSet>
#include <QTabWidget>
#include <QVBoxLayout>

#include "disk_buffer_ring.h"
#include "front_end_gain.h"
#include "gain_choices.h"
#include "player_definition.h"
#include "player_registry.h"
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

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// How a port is described in the list, falling back to its path when the
// system says nothing useful about it — which is most built-in ports.
QString DescribePort(const SerialPortCandidate& port) {
  if (port.description.isEmpty()) {
    return port.path;
  }
  return QStringLiteral("%1 — %2").arg(port.path, port.description);
}

}  // namespace

SettingsDialog::SettingsDialog(
    const CaptureSettings& capture,
    const std::vector<ddd::capture::DeviceInfo>& devices,
    const PlayerSettings& player, const std::vector<SerialPortCandidate>& ports,
    Tab initial_tab, QWidget* parent)
    : QDialog(parent), capture_(capture), player_(player) {
  setWindowTitle(tr("Settings"));

  auto* layout = new QVBoxLayout(this);

  tabs_ = new QTabWidget(this);
  tabs_->setObjectName(QLatin1String(kTabsName));
  tabs_->addTab(BuildCapturePage(devices), tr("&Capture"));
  tabs_->addTab(BuildPlayerPage(ports), tr("&Player"));
  tabs_->setCurrentIndex(initial_tab == Tab::kPlayer ? 1 : 0);
  layout->addWidget(tabs_);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
}

QWidget* SettingsDialog::BuildCapturePage(
    const std::vector<ddd::capture::DeviceInfo>& devices) {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  auto* form = new QFormLayout();

  queue_size_ = new QComboBox(page);
  queue_size_->setObjectName(QLatin1String(kQueueSizeComboName));
  for (const QueueChoice& choice : kQueueChoices) {
    queue_size_->addItem(tr(choice.label),
                         static_cast<qulonglong>(choice.bytes));
    if (choice.bytes == capture_.queue_size_bytes) {
      queue_size_->setCurrentIndex(queue_size_->count() - 1);
    }
  }
  form->addRow(tr("Buffer queue"), queue_size_);

  transfer_mode_ = new QComboBox(page);
  transfer_mode_->setObjectName(QLatin1String(kTransferModeComboName));
  transfer_mode_->addItem(tr("Many small transfers (recommended)"), true);
  transfer_mode_->addItem(tr("One transfer per buffer"), false);
  transfer_mode_->setCurrentIndex(capture_.small_transfers ? 0 : 1);
  transfer_mode_->setToolTip(tr(
      "Small transfers keep several reads outstanding at once, so the "
      "device always has somewhere to put the next packet. One transfer per "
      "buffer is simpler and slightly cheaper, but leaves a gap between each "
      "transfer completing and the next being submitted."));
  form->addRow(tr("USB transfers"), transfer_mode_);

  device_ = new QComboBox(page);
  device_->setObjectName(QLatin1String(kDeviceComboName));
  device_->addItem(tr("Whichever is attached"), QString());
  for (const ddd::capture::DeviceInfo& info : devices) {
    const QString path = QString::fromStdString(info.path);

    // Named for what it is, as in the Capture panel's list. A device in
    // recovery mode can still be preferred — it is the same physical port,
    // and it will be a capture device again once it has been programmed.
    device_->addItem(path + DeviceListPersonalitySuffix(info.personality),
                     path);
    if (path == capture_.preferred_device_path) {
      device_->setCurrentIndex(device_->count() - 1);
    }
  }
  form->addRow(tr("Preferred device"), device_);

  front_end_gain_ = new QComboBox(page);
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
    if (choice.switch_pattern == capture_.front_end_gain_switches) {
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
      page);
  note->setWordWrap(true);
  note->setForegroundRole(QPalette::PlaceholderText);
  layout->addWidget(note);

  layout->addStretch(1);
  return page;
}

QWidget* SettingsDialog::BuildPlayerPage(
    const std::vector<SerialPortCandidate>& ports) {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  auto* form = new QFormLayout();

  player_enabled_ = new QCheckBox(tr("Look for a LaserDisc player"), page);
  player_enabled_->setObjectName(QLatin1String(kPlayerEnabledCheckName));
  player_enabled_->setChecked(player_.enabled);
  player_enabled_->setToolTip(
      tr("While this is off, no serial port on this machine is opened or "
         "written to."));
  form->addRow(QString(), player_enabled_);

  player_model_ = new QComboBox(page);
  player_model_->setObjectName(QLatin1String(kPlayerModelComboName));
  player_model_->addItem(tr("Whichever answers (recommended)"), QString());
  for (const player::PlayerDefinition* definition :
       player::RegisteredPlayers()) {
    const QString id_code = ToQString(definition->id_code);
    player_model_->addItem(ToQString(definition->name), id_code);
    if (id_code == player_.model_id_code) {
      player_model_->setCurrentIndex(player_model_->count() - 1);
    }
  }
  player_model_->setToolTip(
      tr("The player says which model it is, so this is a check rather than a "
         "requirement: if you say LD-V4300D and an LD-V8000 answers, the "
         "application will point that out."));
  form->addRow(tr("Player model"), player_model_);

  player_port_ = new QComboBox(page);
  player_port_->setObjectName(QLatin1String(kPlayerPortComboName));
  player_port_->addItem(tr("Find it automatically (recommended)"), QString());

  bool fixed_port_listed = false;
  for (const SerialPortCandidate& candidate : ports) {
    player_port_->addItem(DescribePort(candidate), candidate.path);
    if (candidate.path == player_.port_path) {
      player_port_->setCurrentIndex(player_port_->count() - 1);
      fixed_port_listed = true;
    }
  }

  // A port that was chosen and is not there now stays chosen. Silently
  // resetting it to "find it automatically" because the adapter is unplugged
  // would change the user's configuration behind their back, and they would
  // find out by watching the application probe every other port on the machine.
  if (!player_.port_path.isEmpty() && !fixed_port_listed) {
    player_port_->addItem(tr("%1 — not connected").arg(player_.port_path),
                          player_.port_path);
    player_port_->setCurrentIndex(player_port_->count() - 1);
  }

  player_port_->setToolTip(
      tr("A port chosen here is the only one that will ever be opened. If the "
         "player is not on it, the application says so rather than looking "
         "elsewhere."));
  form->addRow(tr("Serial port"), player_port_);

  player_baud_ = new QComboBox(page);
  player_baud_->setObjectName(QLatin1String(kPlayerBaudComboName));
  player_baud_->addItem(tr("Work it out (recommended)"), 0U);

  // The rates come from the registry rather than a list here, so a player
  // family added later with different rates needs no change in this file.
  QSet<uint32_t> offered;
  for (const player::ProbeSpec* probe : player::RegisteredProbes()) {
    for (const uint32_t rate : probe->baud_rates) {
      if (offered.contains(rate)) {
        continue;
      }
      offered.insert(rate);
      player_baud_->addItem(tr("%1 baud").arg(rate), rate);
      if (rate == player_.baud_rate) {
        player_baud_->setCurrentIndex(player_baud_->count() - 1);
      }
    }
  }
  form->addRow(tr("Speed"), player_baud_);

  layout->addLayout(form);

  auto* excluded_note = new QLabel(
      tr("Ports never to open. Searching for a player means writing a few "
         "bytes to a port to see what answers, so anything on this machine "
         "that should not be interrupted belongs here."),
      page);
  excluded_note->setWordWrap(true);
  layout->addWidget(excluded_note);

  player_excluded_ = new QListWidget(page);
  player_excluded_->setObjectName(QLatin1String(kPlayerExcludedListName));

  // Every port that exists, plus any already excluded that does not — an
  // exclusion for an adapter that is currently unplugged must survive the
  // dialog being opened while it is out.
  QStringList listed;
  for (const SerialPortCandidate& candidate : ports) {
    listed.append(candidate.path);
  }
  for (const QString& path : player_.excluded_ports) {
    if (!listed.contains(path)) {
      listed.append(path);
    }
  }

  for (const QString& path : listed) {
    auto* item = new QListWidgetItem(path, player_excluded_);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(player_.excluded_ports.contains(path) ? Qt::Checked
                                                              : Qt::Unchecked);
  }

  layout->addWidget(player_excluded_);
  return page;
}

CaptureSettings SettingsDialog::Settings() const {
  CaptureSettings result = capture_;
  result.queue_size_bytes =
      static_cast<size_t>(queue_size_->currentData().toULongLong());
  result.small_transfers = transfer_mode_->currentData().toBool();
  result.preferred_device_path = device_->currentData().toString();
  result.front_end_gain_switches =
      static_cast<uint8_t>(front_end_gain_->currentData().toUInt() & 0xFFU);
  return result;
}

PlayerSettings SettingsDialog::Player() const {
  PlayerSettings result = player_;

  result.enabled = player_enabled_->isChecked();
  result.model_id_code = player_model_->currentData().toString();
  result.port_path = player_port_->currentData().toString();
  result.baud_rate = player_baud_->currentData().toUInt();

  result.excluded_ports.clear();
  for (int row = 0; row < player_excluded_->count(); ++row) {
    const QListWidgetItem* const item = player_excluded_->item(row);
    if (item->checkState() == Qt::Checked) {
      result.excluded_ports.append(item->text());
    }
  }

  // A remembered port the user has just excluded, or just overridden with a
  // fixed one, is a remembered port that would be tried first and rejected on
  // every search from now on.
  if (result.excluded_ports.contains(result.remembered_port) ||
      (!result.port_path.isEmpty() &&
       result.port_path != result.remembered_port)) {
    result.remembered_port.clear();
    result.remembered_baud = 0;
  }

  return result;
}

}  // namespace ddd::gui
