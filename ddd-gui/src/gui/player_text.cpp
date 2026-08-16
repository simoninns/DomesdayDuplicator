/************************************************************************

    player_text.cpp

    What the interface says about the player
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "player_text.h"

#include <QCoreApplication>
#include <QObject>

namespace ddd::gui {
namespace {

QString ModelDescription(const PlayerConnection& connection) {
  if (!connection.recognised_model) {
    return connection.model_id_code.isEmpty()
               ? QObject::tr("an unrecognised player")
               : QObject::tr("an unrecognised player (ID %1)")
                     .arg(connection.model_id_code);
  }

  if (connection.firmware_version.isEmpty()) {
    return connection.model_name;
  }

  return QObject::tr("%1 (firmware %2)")
      .arg(connection.model_name, connection.firmware_version);
}

}  // namespace

QString PlayerConnectionSummary(const PlayerConnection& connection) {
  switch (connection.state) {
    case PlayerConnectionState::kDisabled:
      return QObject::tr("Player control is off");

    case PlayerConnectionState::kSearching:
      return QObject::tr("Looking for a player…");

    case PlayerConnectionState::kConnected:
      return connection.recognised_model
                 ? connection.model_name
                 : QObject::tr("Unrecognised player connected");

    case PlayerConnectionState::kModelMismatch:
      return QObject::tr("Connected to a %1").arg(connection.model_name);

    case PlayerConnectionState::kDisconnected:
      break;
  }

  switch (connection.problem) {
    case PlayerConnectionProblem::kLinkLost:
      return QObject::tr("Player disconnected");
    case PlayerConnectionProblem::kPortUnavailable:
      return QObject::tr("No serial port available");
    case PlayerConnectionProblem::kNotAPlayer:
      return QObject::tr("No player found");
    case PlayerConnectionProblem::kNoPlayerFound:
    case PlayerConnectionProblem::kNone:
      break;
  }

  return QObject::tr("No player found");
}

QString PlayerConnectionDetail(const PlayerConnection& connection) {
  switch (connection.state) {
    case PlayerConnectionState::kDisabled:
      return QObject::tr(
          "While it is off, no serial port on this machine is opened or "
          "written to.");

    case PlayerConnectionState::kSearching:
      return QObject::tr(
          "Trying each serial port in turn, at each speed a player can be set "
          "to.");

    case PlayerConnectionState::kConnected:
      // A working connection to the expected model needs no explanation. The
      // one exception is a player nothing recognises, where what the user
      // needs to know is that it will mostly work and might not entirely.
      if (!connection.recognised_model) {
        return QObject::
            tr("This player answered correctly but is not a model this build "
               "knows about, so it is being driven with the standard Pioneer "
               "command set. Most controls will work; some may not. It "
               "reported "
               "itself as \"%1\".")
                .arg(connection.model_code);
      }
      return {};

    case PlayerConnectionState::kModelMismatch:
      return QObject::
          tr("The settings say you have a %1, and a %2 answered on %3. "
             "Everything "
             "works — this is the model that is actually connected — but it is "
             "worth knowing your setup is not what the settings describe.")
              .arg(connection.selected_model_name, connection.model_name,
                   connection.port_path);

    case PlayerConnectionState::kDisconnected:
      break;
  }

  switch (connection.problem) {
    case PlayerConnectionProblem::kLinkLost:
      return QObject::tr(
          "The link went away while the player was connected — usually a cable "
          "pulled out, an adapter unplugged, or a player switched off. Looking "
          "for it again.");

    case PlayerConnectionProblem::kPortUnavailable:
      if (connection.detail.isEmpty()) {
        return QObject::tr(
            "There is no serial port to look on. Check that the adapter is "
            "plugged in — and, on Linux, that you are in the group that owns "
            "the serial devices (usually dialout or uucp).");
      }
      return QObject::tr(
                 "%1 could not be opened. It may be in use by something else, "
                 "or you may not have permission for it — on Linux that "
                 "usually means being in the group that owns the serial "
                 "devices (dialout or uucp).")
          .arg(connection.detail);

    case PlayerConnectionProblem::kNotAPlayer:
      return QObject::tr(
                 "Something answered, and it was not a player: %1. If that is "
                 "equipment you would rather this application left alone, add "
                 "the port to the excluded list in the player settings.")
          .arg(connection.detail);

    case PlayerConnectionProblem::kNoPlayerFound:
    case PlayerConnectionProblem::kNone:
      break;
  }

  return QObject::tr(
      "Every serial port was tried, at every speed, and nothing answered. "
      "Check that the player is switched on and that its serial port is "
      "enabled.");
}

QString PlayerConnectionSource(const PlayerConnection& connection) {
  if (!connection.live()) {
    return {};
  }

  return QObject::tr("%1 on %2 at %3 baud")
      .arg(ModelDescription(connection), connection.port_path)
      .arg(connection.baud_rate);
}

QString PlayerStateName(player::PlayerState state) {
  switch (state) {
    case player::PlayerState::kUnknown:
      return QObject::tr("Unknown");
    case player::PlayerState::kDoorOpen:
      return QObject::tr("Tray open");
    case player::PlayerState::kParked:
      return QObject::tr("Stopped");
    case player::PlayerState::kSettingUp:
      return QObject::tr("Spinning up");
    case player::PlayerState::kUnloading:
      return QObject::tr("Spinning down");
    case player::PlayerState::kPlaying:
      return QObject::tr("Playing");
    case player::PlayerState::kStillFrame:
      return QObject::tr("Still frame");
    case player::PlayerState::kPaused:
      return QObject::tr("Paused");
    case player::PlayerState::kSearching:
      return QObject::tr("Searching");
    case player::PlayerState::kScanning:
      return QObject::tr("Scanning");
    case player::PlayerState::kMultiSpeed:
      return QObject::tr("Multi-speed");
  }
  return QObject::tr("Unknown");
}

QString TrayStateName(player::TrayState tray) {
  switch (tray) {
    case player::TrayState::kOpen:
      return QObject::tr("Open");
    case player::TrayState::kClosed:
      return QObject::tr("Closed");
    case player::TrayState::kUnknown:
      break;
  }
  return QObject::tr("Unknown");
}

QString DiscTypeName(player::DiscType type) {
  switch (type) {
    case player::DiscType::kCav:
      return QObject::tr("CAV");
    case player::DiscType::kClv:
      return QObject::tr("CLV");
    case player::DiscType::kUnknown:
      break;
  }
  return QObject::tr("Unknown");
}

QString FormatTimeCode(int32_t time_code) {
  if (time_code < 0) {
    return QObject::tr("Unknown");
  }

  // Seven digits: hours, minutes, seconds, frames. A shorter number is the
  // same thing with leading zeros, which is how the player sends it.
  const int32_t hours = time_code / 1000000;
  const int32_t minutes = (time_code / 10000) % 100;
  const int32_t seconds = (time_code / 100) % 100;

  return QStringLiteral("%1:%2:%3")
      .arg(hours)
      .arg(minutes, 2, 10, QLatin1Char('0'))
      .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString PlayerAddressText(const player::PlayerStatus& status) {
  if (!status.valid || !status.address.valid) {
    return QObject::tr("—");
  }

  // A position in its own right, and the number alongside it means nothing to
  // anybody: the lead-in has its own frame numbering.
  if (status.address.in_lead_in) {
    return QObject::tr("Lead-in");
  }
  if (status.address.in_lead_out) {
    return QObject::tr("Lead-out");
  }

  if (status.disc_type == player::DiscType::kClv) {
    return FormatTimeCode(status.address.value);
  }

  return QObject::tr("Frame %1").arg(status.address.value);
}

QString PhysicalPositionText(const player::PlayerStatus& status) {
  if (!status.physical_position_mm.has_value()) {
    return {};
  }

  return QObject::tr("%1 mm from the centre")
      .arg(static_cast<double>(*status.physical_position_mm), 0, 'f', 2);
}

QString PlayerStatusBarText(const PlayerConnection& connection,
                            const player::PlayerStatus& status) {
  if (!connection.live()) {
    return PlayerConnectionSummary(connection);
  }

  const QString name = connection.recognised_model
                           ? connection.model_name
                           : QObject::tr("Unrecognised player");

  if (!status.valid) {
    return name;
  }

  const QString address = PlayerAddressText(status);
  if (address == QObject::tr("—")) {
    return QObject::tr("%1 — %2").arg(name, PlayerStateName(status.state));
  }

  return QObject::tr("%1 — %2, %3")
      .arg(name, PlayerStateName(status.state), address);
}

QString PlayerVerificationNote(const PlayerConnection& connection) {
  if (!connection.live() || connection.bench_verified ||
      !connection.recognised_model) {
    return {};
  }

  return QObject::tr(
      "This model's command set has not yet been confirmed against the "
      "hardware. It is inherited from the shared Pioneer command set and is "
      "expected to work; if a control does the wrong thing, that is worth "
      "reporting.");
}

}  // namespace ddd::gui
