/************************************************************************

    player_panel.cpp

    The player: whether there is one, and what it is doing
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "player_panel.h"

#include <QCheckBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "player_controller.h"
#include "player_text.h"

namespace ddd::gui {
namespace {

QLabel* MakeReadout(QWidget* parent, const char* object_name) {
  auto* label = new QLabel(QStringLiteral("—"), parent);
  label->setObjectName(QLatin1String(object_name));
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return label;
}

}  // namespace

PlayerPanel::PlayerPanel(PlayerController* controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
  // Inside a scroll area for the reason the Capture panel is: without one the
  // panel's minimum height is every row it contains, which it then demands from
  // the dock column it shares — and the separators stop moving.
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

  enabled_check_ = new QCheckBox(tr("Player control"), contents);
  enabled_check_->setObjectName(QLatin1String(kEnabledCheckName));
  enabled_check_->setToolTip(
      tr("Look for a LaserDisc player on the serial ports, and control it from "
         "here. While this is off, no serial port on this machine is opened or "
         "written to."));
  layout->addWidget(enabled_check_);

  summary_ = new QLabel(contents);
  summary_->setObjectName(QLatin1String(kSummaryLabelName));
  summary_->setWordWrap(true);
  QFont summary_font = summary_->font();
  summary_font.setBold(true);
  summary_->setFont(summary_font);
  layout->addWidget(summary_);

  detail_ = new QLabel(contents);
  detail_->setObjectName(QLatin1String(kDetailLabelName));
  detail_->setWordWrap(true);
  detail_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(detail_);

  source_ = new QLabel(contents);
  source_->setObjectName(QLatin1String(kSourceLabelName));
  source_->setWordWrap(true);
  source_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(source_);

  verification_ = new QLabel(contents);
  verification_->setObjectName(QLatin1String(kVerificationLabelName));
  verification_->setWordWrap(true);
  layout->addWidget(verification_);

  auto* buttons = new QWidget(contents);
  auto* button_layout = new QHBoxLayout(buttons);
  button_layout->setContentsMargins(0, 0, 0, 0);

  search_ = new QPushButton(tr("Search now"), buttons);
  search_->setObjectName(QLatin1String(kSearchButtonName));
  search_->setToolTip(
      tr("Look for the player again straight away, rather than waiting for the "
         "next automatic attempt."));
  button_layout->addWidget(search_);

  remote_ = new QPushButton(tr("Remote…"), buttons);
  remote_->setObjectName(QLatin1String(kRemoteButtonName));
  remote_->setToolTip(
      tr("Drive the player by hand. The remote stays open while you work, so "
         "the spectrum and waveform can be watched at the same time."));
  button_layout->addWidget(remote_);

  use_model_ = new QPushButton(tr("Use this model"), buttons);
  use_model_->setObjectName(QLatin1String(kUseModelButtonName));
  use_model_->setToolTip(
      tr("Change the selected model to the one that actually answered."));
  use_model_->setVisible(false);
  button_layout->addWidget(use_model_);

  button_layout->addStretch(1);
  layout->addWidget(buttons);

  auto* form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignLeft);

  state_ = MakeReadout(contents, kStateLabelName);
  form->addRow(tr("State"), state_);

  tray_ = MakeReadout(contents, kTrayLabelName);
  form->addRow(tr("Tray"), tray_);

  disc_ = MakeReadout(contents, kDiscLabelName);
  form->addRow(tr("Disc"), disc_);

  address_ = MakeReadout(contents, kAddressLabelName);
  form->addRow(tr("Position"), address_);

  // Built and then hidden rather than not built: the row exists in every build
  // of the panel, so a widget test can find it and check that it stays hidden
  // for a model that cannot report one.
  position_row_ = new QWidget(contents);
  auto* position_layout = new QHBoxLayout(position_row_);
  position_layout->setContentsMargins(0, 0, 0, 0);
  position_ = MakeReadout(position_row_, kPositionLabelName);
  position_layout->addWidget(position_);
  form->addRow(tr("Optical assembly"), position_row_);
  position_row_->setVisible(false);

  layout->addLayout(form);
  layout->addStretch(1);

  if (controller_ == nullptr) {
    // No controller: everything builds, lays out and does nothing. The panel
    // still says what state it is in, which for a window with no player
    // support at all is the honest one.
    PlayerConnection disabled;
    OnConnectionChanged(disabled);
    enabled_check_->setEnabled(false);
    search_->setEnabled(false);
    remote_->setEnabled(false);
    return;
  }

  connect(enabled_check_, &QCheckBox::toggled, controller_,
          &PlayerController::SetEnabled);
  connect(search_, &QPushButton::clicked, controller_,
          &PlayerController::SearchNow);
  connect(remote_, &QPushButton::clicked, this, &PlayerPanel::RemoteRequested);
  connect(use_model_, &QPushButton::clicked, controller_,
          &PlayerController::UseConnectedModel);

  connect(controller_, &PlayerController::ConnectionChanged, this,
          &PlayerPanel::OnConnectionChanged);
  connect(controller_, &PlayerController::StatusUpdated, this,
          &PlayerPanel::OnStatusUpdated);

  // The checkbox is the settings, so anything else that changes them — the
  // settings dialog, the menu — is reflected here rather than leaving the two
  // disagreeing.
  connect(controller_, &PlayerController::SettingsChanged, this,
          [this](const PlayerSettings& settings) {
            const QSignalBlocker blocker(enabled_check_);
            enabled_check_->setChecked(settings.enabled);
          });

  enabled_check_->setChecked(controller_->settings().enabled);
  OnConnectionChanged(controller_->connection());
  OnStatusUpdated(controller_->status());
}

void PlayerPanel::OnConnectionChanged(const PlayerConnection& connection) {
  summary_->setText(PlayerConnectionSummary(connection));

  const QString detail = PlayerConnectionDetail(connection);
  detail_->setText(detail);
  detail_->setVisible(!detail.isEmpty());

  const QString source = PlayerConnectionSource(connection);
  source_->setText(source);
  source_->setVisible(!source.isEmpty());

  const QString verification = PlayerVerificationNote(connection);
  verification_->setText(verification);
  verification_->setVisible(!verification.isEmpty());

  use_model_->setVisible(connection.state ==
                         PlayerConnectionState::kModelMismatch);

  // Searching now is only meaningful when it is looking and has not found
  // anything: there is nothing to search for while connected, and nothing to
  // search with while switched off.
  search_->setEnabled(connection.state == PlayerConnectionState::kDisconnected);

  // There is nothing to drive without a player, and a remote full of greyed-out
  // buttons is a worse answer than a button that says "not yet".
  remote_->setEnabled(connection.live());

  if (!connection.live()) {
    ClearStatus();
  }
}

void PlayerPanel::OnStatusUpdated(const player::PlayerStatus& status) {
  if (!status.valid) {
    ClearStatus();
    return;
  }

  state_->setText(PlayerStateName(status.state));
  tray_->setText(TrayStateName(status.tray));
  disc_->setText(DiscTypeName(status.disc_type));
  address_->setText(PlayerAddressText(status));

  const QString position = PhysicalPositionText(status);
  position_->setText(position);
  position_row_->setVisible(!position.isEmpty());
}

void PlayerPanel::ClearStatus() {
  const QString nothing = QStringLiteral("—");
  state_->setText(nothing);
  tray_->setText(nothing);
  disc_->setText(nothing);
  address_->setText(nothing);
  position_->setText(nothing);
  position_row_->setVisible(false);
}

}  // namespace ddd::gui
