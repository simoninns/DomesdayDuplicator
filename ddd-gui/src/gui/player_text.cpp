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
#include <QStringList>
#include <QtGlobal>
#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

#include "player_controls.h"
#include "player_registry.h"
#include "response_parser.h"
#include "statistics_presenter.h"
#include "user_code.h"

namespace ddd::gui {

HostPlatform ThisPlatform() {
#if defined(Q_OS_WIN)
  return HostPlatform::kWindows;
#elif defined(Q_OS_MACOS)
  return HostPlatform::kMacOs;
#else
  return HostPlatform::kLinux;
#endif
}

QString SerialPermissionAdvice(HostPlatform platform) {
  switch (platform) {
    case HostPlatform::kLinux:
      // The group is the whole answer, and which group it is depends on the
      // distribution — so both are named rather than one guessed at. Naming the
      // command as well, because "add yourself to the group" is not a thing
      // everybody knows how to do, and a log-out is the part that is always
      // forgotten.
      return QObject::tr(
          "On Linux the serial devices belong to a group — usually dialout, "
          "or uucp on Arch and its derivatives — and you have to be in it. "
          "Add yourself with \"sudo usermod -a -G dialout $USER\", then log "
          "out and back in: a group you have just been given does not apply "
          "to the session you were already in.");

    case HostPlatform::kMacOs:
      // Not a permission problem in the Unix sense at all — /dev/cu.* is world
      // writable — so the advice that would be right on Linux is useless here
      // and this says what actually happens instead.
      return QObject::tr(
          "On macOS a serial adapter needs its driver, and a third-party one "
          "has to be allowed in System Settings under Privacy & Security after "
          "it is installed. Until it is, the adapter's port either does not "
          "appear or cannot be opened. Adapters using the built-in drivers "
          "appear as /dev/cu.usbserial-… and need nothing installed.");

    case HostPlatform::kWindows:
      // Windows has no group to join: an unopenable COM port is somebody else
      // holding it, which is a different remedy entirely.
      return QObject::tr(
          "On Windows a COM port can only be opened by one program at a time. "
          "Close anything else that might have it — a terminal, a logging "
          "tool, or another copy of this application — and check in Device "
          "Manager, under Ports (COM & LPT), that the adapter has a driver "
          "and a port number.");
  }

  return QString();
}

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

// What a field that was never established reads as.
//
// A word rather than a blank, because a blank in a report a user is about to
// paste into an issue is indistinguishable from a bug in the report.
QString UnknownField() { return QObject::tr("not known"); }

// How this disc is addressed, defaulting to frames while that is not known —
// which is what the address parser is strictest about, and so the safest thing
// to be wrong about.
player::AddressMode Addressing(const player::DiscProfile& disc) {
  return disc.addressing.known() ? disc.addressing.value
                                 : player::AddressMode::kFrame;
}

// A reply's bytes, exactly.
//
// Latin-1 rather than UTF-8 for the reason set out in player_worker.cpp: a user
// code is arbitrary bytes and this maps all 256 of them one-to-one.
QString Payload(const std::string& bytes) {
  return QString::fromLatin1(bytes.data(),
                             static_cast<qsizetype>(bytes.size()));
}

// One user-code line of the report.
//
// Four outcomes and four different sentences, because they are four different
// findings and the whole point of separating them is not to have them read
// alike.
QString UserCodeLine(const QString& label,
                     const player::UserCodeReading& code) {
  switch (code.outcome) {
    case player::UserCodeReading::Outcome::kNotRead:
      return QObject::tr("    %1: not read").arg(label);

    case player::UserCodeReading::Outcome::kNotEncoded:
      return QObject::tr(
                 "    %1: none encoded on this disc (the player answered "
                 "\"%2\")")
          .arg(label, Payload(code.text));

    case player::UserCodeReading::Outcome::kRefused:
      return QObject::tr("    %1: asked for, and the player did not answer")
          .arg(label);

    case player::UserCodeReading::Outcome::kRead:
      break;
  }

  const size_t unreadable = static_cast<size_t>(std::count(
      code.text.begin(), code.text.end(), player::kUnreadableCharacter));

  if (unreadable == 0) {
    return QObject::tr("    %1: %2").arg(label, Payload(code.text));
  }

  // The distinction that took a bench session to establish: a run of these is
  // the player saying it could not read those characters off the disc, not the
  // disc saying it carries none there.
  return QObject::tr(
             "    %1: %2\n        %3 of %4 characters could not be read off "
             "the disc")
      .arg(label, Payload(code.text))
      .arg(unreadable)
      .arg(code.text.size());
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
    case PlayerConnectionProblem::kPortNotPermitted:
      return QObject::tr("Not allowed to use the serial port");
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
                   "There is no serial port to look on. Check that the adapter "
                   "is plugged in. %1")
            .arg(SerialPermissionAdvice(ThisPlatform()));
      }
      return QObject::tr(
                 "%1 could not be opened. It may be in use by something else, "
                 "or you may not have permission for it. %2")
          .arg(connection.detail, SerialPermissionAdvice(ThisPlatform()));

    // The port is there, and the system said no. The one connection problem
    // whose remedy is entirely on the user's side of the cable, so the message
    // is the remedy and nothing else.
    case PlayerConnectionProblem::kPortNotPermitted:
      if (connection.detail.isEmpty()) {
        return SerialPermissionAdvice(ThisPlatform());
      }
      return QObject::tr(
                 "%1 is there, and this account is not allowed to open "
                 "it. %2")
          .arg(connection.detail, SerialPermissionAdvice(ThisPlatform()));

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

std::optional<int32_t> ParseTimeCodeEntry(const QString& text) {
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return std::nullopt;
  }

  // The bare form: the player's own seven digits, for somebody reading them off
  // its display. Taken as written, since that is already what the wire wants.
  if (!trimmed.contains(QLatin1Char(':'))) {
    bool numeric = false;
    const int32_t value = trimmed.toInt(&numeric);
    if (!numeric || value < 0 || trimmed.size() > 7) {
      return std::nullopt;
    }
    return value;
  }

  const QStringList parts = trimmed.split(QLatin1Char(':'));
  if (parts.size() > 3) {
    return std::nullopt;
  }

  // Right-aligned, the way a clock is read: "23:45" is minutes and seconds on a
  // disc under an hour, not hours and minutes.
  int32_t fields[3] = {0, 0, 0};
  const qsizetype offset = 3 - parts.size();
  for (qsizetype index = 0; index < parts.size(); ++index) {
    bool numeric = false;
    fields[offset + index] = parts.at(index).toInt(&numeric);
    if (!numeric || parts.at(index).isEmpty()) {
      return std::nullopt;
    }
  }

  const int32_t hours = fields[0];
  const int32_t minutes = fields[1];
  const int32_t seconds = fields[2];

  // One digit for the hours, because that is all the seven-digit format has.
  if (hours < 0 || hours > 9 || minutes < 0 || minutes > 59 || seconds < 0 ||
      seconds > 59) {
    return std::nullopt;
  }

  // The trailing frame count is left at zero. A user typing a time means the
  // second, and the player seeks to the first frame of it.
  return (hours * 1000000) + (minutes * 10000) + (seconds * 100);
}

QString PlayerCommandName(player::PlayerCommand command) {
  switch (command) {
    case player::PlayerCommand::kTrayOpen:
      return QObject::tr("Open the tray");
    case player::PlayerCommand::kTrayClose:
      return QObject::tr("Close the tray");
    case player::PlayerCommand::kPlay:
      return QObject::tr("Play");
    case player::PlayerCommand::kPlayWithoutStopCodes:
      return QObject::tr("Play, ignoring stop codes");
    case player::PlayerCommand::kPause:
      return QObject::tr("Pause");
    case player::PlayerCommand::kStillFrame:
      return QObject::tr("Still frame");
    case player::PlayerCommand::kStop:
      return QObject::tr("Reject");
    case player::PlayerCommand::kStepForward:
      return QObject::tr("Step forward");
    case player::PlayerCommand::kStepReverse:
      return QObject::tr("Step back");
    case player::PlayerCommand::kScanForward:
      return QObject::tr("Scan forward");
    case player::PlayerCommand::kScanReverse:
      return QObject::tr("Scan back");
    case player::PlayerCommand::kMultiSpeedForward:
      return QObject::tr("Multi-speed forward");
    case player::PlayerCommand::kMultiSpeedReverse:
      return QObject::tr("Multi-speed back");
    case player::PlayerCommand::kSetSpeed:
      return QObject::tr("Set the multi-speed rate");
    case player::PlayerCommand::kSeekFrame:
      return QObject::tr("Go to a frame");
    case player::PlayerCommand::kSeekTimeCode:
      return QObject::tr("Go to a time");
    case player::PlayerCommand::kSeekChapter:
      return QObject::tr("Go to a chapter");
    case player::PlayerCommand::kDisplayOn:
      return QObject::tr("Turn the on-screen display on");
    case player::PlayerCommand::kDisplayOff:
      return QObject::tr("Turn the on-screen display off");
    case player::PlayerCommand::kSetAudio:
      return QObject::tr("Choose the audio");
    case player::PlayerCommand::kKeyLockOn:
      return QObject::tr("Lock the player's own controls");
    case player::PlayerCommand::kKeyLockOff:
      return QObject::tr("Unlock the player's own controls");
    case player::PlayerCommand::kQueryActiveMode:
      return QObject::tr("Ask what the player is doing");
    case player::PlayerCommand::kQueryAddress:
      return QObject::tr("Ask where the player is");
    case player::PlayerCommand::kQueryDiscStatus:
      return QObject::tr("Ask about the disc");
    case player::PlayerCommand::kQueryStandardUserCode:
      return QObject::tr("Read the standard user code");
    case player::PlayerCommand::kQueryPioneerUserCode:
      return QObject::tr("Read the Pioneer user code");
    case player::PlayerCommand::kQueryTvSystem:
      return QObject::tr("Ask which television standard the disc carries");
    case player::PlayerCommand::kQueryPhysicalPosition:
      return QObject::tr("Read the optical assembly's position");
    case player::PlayerCommand::kCount:
      break;
  }
  return QObject::tr("Unknown command");
}

QString AudioModeName(player::AudioMode mode) {
  switch (mode) {
    case player::AudioMode::kOff:
      return QObject::tr("Muted");
    case player::AudioMode::kAnalogChannel1:
      return QObject::tr("Analogue left");
    case player::AudioMode::kAnalogChannel2:
      return QObject::tr("Analogue right");
    case player::AudioMode::kAnalogStereo:
      return QObject::tr("Analogue stereo");
    case player::AudioMode::kDigitalChannel1:
      return QObject::tr("Digital left");
    case player::AudioMode::kDigitalChannel2:
      return QObject::tr("Digital right");
    case player::AudioMode::kDigitalStereo:
      return QObject::tr("Digital stereo");
    case player::AudioMode::kCount:
      break;
  }
  return QObject::tr("Unknown");
}

QString PlaybackSpeedName(player::PlaybackSpeed speed) {
  switch (speed) {
    case player::PlaybackSpeed::kSixth:
      return QObject::tr("1/6×");
    case player::PlaybackSpeed::kQuarter:
      return QObject::tr("1/4×");
    case player::PlaybackSpeed::kThird:
      return QObject::tr("1/3×");
    case player::PlaybackSpeed::kHalf:
      return QObject::tr("1/2×");
    case player::PlaybackSpeed::kNormal:
      return QObject::tr("1×");
    case player::PlaybackSpeed::kDouble:
      return QObject::tr("2×");
    case player::PlaybackSpeed::kTriple:
      return QObject::tr("3×");
    case player::PlaybackSpeed::kQuadruple:
      return QObject::tr("4×");
    case player::PlaybackSpeed::kCount:
      break;
  }
  return QObject::tr("Unknown");
}

QString UnsupportedControlNote(const PlayerConnection& connection,
                               player::PlayerCommand command) {
  const QString model =
      connection.recognised_model && !connection.model_name.isEmpty()
          ? connection.model_name
          : QObject::tr("This player");

  QStringList capable;
  for (const player::PlayerDefinition* definition :
       player::RegisteredPlayers()) {
    // Asked with the firmware the model's own gate names, so the answer is
    // "this model can, given the firmware for it" rather than "this model
    // cannot, on the firmware that happens to be in the room".
    const player::PlayerControls controls = player::ControlsFor(
        *definition, definition->physical_position_firmware);

    if (controls.Has(command)) {
      capable.append(
          QString::fromUtf8(definition->name.data(),
                            static_cast<qsizetype>(definition->name.size())));
    }
  }

  if (capable.isEmpty()) {
    return QObject::tr(
               "%1 does not offer this control, and nor does any other player "
               "this build knows about.")
        .arg(model);
  }

  // Enough names to be useful without a tooltip that has to be scrolled. With
  // ten models sharing one command set this only ever triggers for a control
  // almost every player has — in which case naming three of them makes the
  // point as well as naming all ten.
  constexpr qsizetype kNamesShown = 3;
  QString names;
  if (capable.size() > kNamesShown) {
    names = QObject::tr("%1 and others")
                .arg(capable.mid(0, kNamesShown).join(QStringLiteral(", ")));
  } else {
    names = capable.join(QStringLiteral(", "));
  }

  return QObject::tr("%1 does not offer this control. Available on: %2.")
      .arg(model, names);
}

QString PlayerReplyText(const PlayerReply& reply) {
  const QString sent =
      reply.sent.isEmpty() ? QObject::tr("(nothing sent)") : reply.sent;

  switch (reply.status) {
    case player::ReplyStatus::kOk:
      // Verbatim, and marked when it is the player's error code rather than an
      // answer: a text reply is deliberately not put through the error
      // convention, because a user code may contain an 'E' — but a reply that
      // is exactly 'E' and digits is not a user code. An LD-V4300D answers the
      // Pioneer user-code query that way on a parked player.
      if (player::IsErrorCode(reply.text.toStdString())) {
        return QObject::tr("%1 → %2 (the player refused)")
            .arg(sent, reply.text);
      }
      return reply.text.isEmpty()
                 ? QObject::tr("%1 → done").arg(sent)
                 : QObject::tr("%1 → %2").arg(sent, reply.text);

    case player::ReplyStatus::kRefused:
      return reply.error_code.isEmpty()
                 ? QObject::tr("%1 → the player refused").arg(sent)
                 : QObject::tr("%1 → the player refused (%2)")
                       .arg(sent, reply.error_code);

    case player::ReplyStatus::kNoAnswer:
      return QObject::tr("%1 → no answer").arg(sent);

    case player::ReplyStatus::kUnparseable:
      return QObject::tr("%1 → %2, which cannot be read").arg(sent, reply.text);

    case player::ReplyStatus::kLinkFailed:
      return QObject::tr("%1 → the link failed").arg(sent);

    case player::ReplyStatus::kNotConnected:
      return QObject::tr("Not sent: there is no player connected.");

    case player::ReplyStatus::kUnsupported:
      return QObject::tr("Not sent: this player has no command for that.");

    case player::ReplyStatus::kInvalidArgument:
      return QObject::tr(
          "Not sent: that is not something this command can be "
          "given.");
  }

  return QObject::tr("%1 → unknown").arg(sent);
}

QString FormatByteDump(const QByteArray& bytes, qsizetype first_offset) {
  if (bytes.isEmpty()) {
    return {};
  }

  static constexpr char kHexDigits[] = "0123456789ABCDEF";
  constexpr qsizetype kPerLine = 16;

  QString dump;
  dump.reserve(bytes.size() * 5);

  for (qsizetype offset = 0; offset < bytes.size(); offset += kPerLine) {
    if (offset > 0) {
      dump += QLatin1Char('\n');
    }

    dump += QStringLiteral("%1  ").arg(first_offset + offset, 4, 10,
                                       QLatin1Char('0'));

    for (qsizetype index = 0; index < kPerLine; ++index) {
      // A gap at the halfway mark, so a byte can be counted off by eye without
      // counting sixteen columns.
      if (index == kPerLine / 2) {
        dump += QLatin1Char(' ');
      }

      if (offset + index < bytes.size()) {
        const auto value = static_cast<unsigned char>(bytes[offset + index]);
        dump += QLatin1Char(kHexDigits[value >> 4]);
        dump += QLatin1Char(kHexDigits[value & 0x0F]);
        dump += QLatin1Char(' ');
      } else {
        dump += QStringLiteral("   ");
      }
    }

    dump += QLatin1Char('|');
    for (qsizetype index = 0; index < kPerLine && offset + index < bytes.size();
         ++index) {
      const auto value = static_cast<unsigned char>(bytes[offset + index]);
      dump += (value >= 0x20 && value < 0x7F)
                  ? QLatin1Char(static_cast<char>(value))
                  : QLatin1Char('.');
    }
    dump += QLatin1Char('|');
  }

  return dump;
}

namespace {

// Whether a reply to a user-code query is a refusal rather than a reading.
//
// Two shapes, because the players produce both: on some models the error code
// arrives as an ordinary text reply — a user code may contain an 'E', so text
// replies are deliberately not put through the error convention — and on
// others as a refusal.
bool IsUserCodeRefusal(const PlayerReply& reply) {
  if (reply.status == player::ReplyStatus::kRefused) {
    return true;
  }
  return reply.ok() && player::IsErrorCode(reply.text.toStdString());
}

// What a refused user-code query means, which is not only what the manuals say
// it means.
//
// Both readings are worth giving, because both have been seen on this project's
// bench with the same command: a disc spinning at frame one that simply has no
// user code, and a disc whose codes read perfectly while it is playing but
// which answers this way with the player stopped. Given only "the player
// refused", somebody has no way to tell those apart, and the wrong one of them
// is a conclusion about the disc.
QString UserCodeRefusalNote(bool pioneer) {
  if (pioneer) {
    return QObject::tr(
        "The disc must be playing for this to work. Pioneer documents this "
        "answer as meaning the disc carries no Pioneer User's Code, but a "
        "player that cannot reach the lead-in from where it is — one that is "
        "stopped rather than spinning — answers the same way, so try again "
        "with the disc playing before concluding the disc has none.");
  }

  return QObject::tr(
      "The disc must be playing for this to work. This answer is documented as "
      "meaning the disc carries no user code, but a stopped player answers the "
      "same way whatever the disc carries, so try again with the disc playing "
      "before concluding the disc has none.");
}

}  // namespace

QString PioneerUserCodeReport(const PlayerReply& reply) {
  QStringList lines;
  lines.append(PlayerReplyText(reply));

  const QByteArray bytes = reply.text.toLatin1();

  // An error code is not a short user code, and splitting one into three
  // regions would be inventing a reading of it.
  if (IsUserCodeRefusal(reply)) {
    lines.append(QString());
    lines.append(UserCodeRefusalNote(true));
    return lines.join(QLatin1Char('\n'));
  }

  if (!reply.ok() || bytes.isEmpty()) {
    return lines.join(QLatin1Char('\n'));
  }

  lines.append(QString());
  lines.append(
      QObject::tr("%1 characters. The Pioneer User's Code is recorded in the "
                  "last 100 frames of the lead-in, one character per field, in "
                  "three regions.")
          .arg(bytes.size()));

  // The whole-reply count, because the per-region headings alone leave somebody
  // adding up three numbers to notice that most of a disc's user code did not
  // read. A PAL CLV disc on this bench returns 180 unreadable characters out of
  // 200 — everything but the Control Data — and that is the first thing to
  // know about it.
  const qsizetype unreadable = bytes.count(player::kUnreadableCharacter);
  if (unreadable > 0) {
    lines.append(QObject::tr("%1 of the %2 could not be read by the player.")
                     .arg(unreadable)
                     .arg(bytes.size()));
  }

  if (bytes.size() != static_cast<qsizetype>(player::kPioneerUserCodeLength)) {
    // Said rather than papered over. A reply of the wrong length means the
    // region boundaries below are the format's and not necessarily this
    // player's, and somebody reading a dump needs to know which.
    lines.append(
        QObject::tr("The format says %1. The regions below are shown at the "
                    "offsets the format defines.")
            .arg(player::kPioneerUserCodeLength));
  }

  for (const player::UserCodeRegionReading& reading :
       player::ReadPioneerUserCode(std::string_view(
           bytes.constData(), static_cast<size_t>(bytes.size())))) {
    const QString name =
        QString::fromUtf8(reading.region.name.data(),
                          static_cast<qsizetype>(reading.region.name.size()));

    lines.append(QString());

    QString heading =
        QObject::tr("%1 — %2 characters, %3 to %4")
            .arg(name)
            .arg(reading.region.length)
            .arg(reading.region.offset)
            .arg(reading.region.offset + reading.region.length - 1);

    if (reading.characters.empty()) {
      lines.append(heading);
      lines.append(QObject::tr("  Not in the reply."));
      continue;
    }

    // "Could not be read" and "was never encoded" are different facts about a
    // disc, and Pioneer's own example has an empty Key Data where this bench's
    // Casper disc has an unreadable one. Reported the same way, those two discs
    // would look identical and neither reading would be true.
    if (reading.wholly_unreadable()) {
      heading += QObject::tr("; none of it could be read");
    } else if (reading.wholly_unencoded()) {
      heading += QObject::tr("; nothing is encoded here");
    } else {
      if (reading.unreadable > 0) {
        heading += QObject::tr("; %1 characters could not be read")
                       .arg(reading.unreadable);
      }
      if (reading.unencoded > 0) {
        heading +=
            QObject::tr("; %1 characters unencoded").arg(reading.unencoded);
      }
    }

    if (!reading.complete) {
      heading += QObject::tr("; the reply stopped part way through");
    }

    lines.append(heading);
    lines.append(FormatByteDump(
        QByteArray(reading.characters.data(),
                   static_cast<qsizetype>(reading.characters.size())),
        static_cast<qsizetype>(reading.region.offset)));
  }

  return lines.join(QLatin1Char('\n'));
}

QString PlayerReplyReport(const PlayerReply& reply) {
  // The one reply whose structure is known rather than guessed at.
  if (reply.request.kind == PlayerRequest::Kind::kCommand &&
      reply.request.command == player::PlayerCommand::kQueryPioneerUserCode) {
    return PioneerUserCodeReport(reply);
  }

  QStringList lines;
  lines.append(PlayerReplyText(reply));

  // The standard code is read off the disc just as the Pioneer one is, and a
  // stopped player refuses it for the same reason — so a bare "the player
  // refused" would be read as "this disc has no user code" here too.
  if (reply.request.kind == PlayerRequest::Kind::kCommand &&
      reply.request.command == player::PlayerCommand::kQueryStandardUserCode &&
      IsUserCodeRefusal(reply)) {
    lines.append(QString());
    lines.append(UserCodeRefusalNote(false));
    return lines.join(QLatin1Char('\n'));
  }

  // Latin-1 throughout: this is what the worker decoded the payload with, and
  // it round-trips every byte the player sent.
  const QByteArray bytes = reply.text.toLatin1();
  if (bytes.isEmpty()) {
    return lines.join(QLatin1Char('\n'));
  }

  const bool printable =
      std::all_of(bytes.begin(), bytes.end(), [](char character) {
        const auto value = static_cast<unsigned char>(character);
        return value >= 0x20 && value < 0x7F;
      });

  if (printable && bytes.size() <= 16) {
    return lines.join(QLatin1Char('\n'));
  }

  lines.append(QString());
  lines.append(QObject::tr("%1 bytes.").arg(bytes.size()));

  const qsizetype unreadable = bytes.count(player::kUnreadableCharacter);
  if (unreadable > 0) {
    // The distinction that matters when somebody is looking at a wall of
    // backticks and wondering what this application failed to decode: nothing
    // did. The player is reporting, one character at a time, that it could not
    // read them off the disc.
    lines.append(
        QObject::tr("%1 of them are characters the player could not read off "
                    "the disc — it sends ` (0x60) in place of each one.")
            .arg(unreadable));
  }

  lines.append(QString());
  lines.append(FormatByteDump(bytes));

  return lines.join(QLatin1Char('\n'));
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

// --- Examining the disc ----------------------------------------------------

QString ExamineStageName(player::ExamineStage stage) {
  switch (stage) {
    case player::ExamineStage::kIdle:
      return QObject::tr("Ready");
    case player::ExamineStage::kCheckingPlayer:
      return QObject::tr("Asking the player what it is doing");
    case player::ExamineStage::kSpinningUp:
      return QObject::tr("Spinning the disc up");
    case player::ExamineStage::kReadingDiscStatus:
      return QObject::tr("Reading the disc status");
    case player::ExamineStage::kReadingTvSystem:
      return QObject::tr("Asking which television standard the disc carries");
    case player::ExamineStage::kReadingPioneerUserCode:
      // Said in full because it is the slow one, and a progress line that has
      // not moved for eleven seconds should explain itself.
      return QObject::tr(
          "Reading the Pioneer user code (the player searches to the lead-in "
          "for this, which takes about ten seconds)");
    case player::ExamineStage::kReadingStandardUserCode:
      return QObject::tr("Reading the standard user code");
    case player::ExamineStage::kCheckingChapters:
      return QObject::tr("Looking for chapters");
    case player::ExamineStage::kFindingEnd:
      return QObject::tr("Finding the end of the side");
    case player::ExamineStage::kReadingEnd:
      return QObject::tr("Reading the last address");
    case player::ExamineStage::kFindingStart:
      return QObject::tr("Going back to the start of the side");
    case player::ExamineStage::kReadingStart:
      return QObject::tr("Reading the first address");
    case player::ExamineStage::kSettling:
      return QObject::tr("Holding the disc still");
    case player::ExamineStage::kCheckingTransport:
      return QObject::tr("Checking whether the disc is still turning");
    case player::ExamineStage::kSpinningDown:
      return QObject::tr("Stopping the disc, as it was found");
    case player::ExamineStage::kFinished:
      return QObject::tr("Finished");
  }
  return QObject::tr("Working");
}

QString ExamineOutcomeText(player::ExamineOutcome outcome) {
  switch (outcome) {
    case player::ExamineOutcome::kInProgress:
      return QObject::tr("in progress");
    case player::ExamineOutcome::kCompleted:
      return QObject::tr("completed");
    case player::ExamineOutcome::kTrayOpen:
      return QObject::tr("stopped: the tray is open");
    case player::ExamineOutcome::kNoDisc:
      return QObject::tr("stopped: there is no disc the player can read");
    case player::ExamineOutcome::kLinkFailed:
      return QObject::tr("stopped: the link to the player failed");
    case player::ExamineOutcome::kCancelled:
      return QObject::tr("cancelled");
  }
  return QObject::tr("finished");
}

QString ExamineStepText(player::ExamineStage stage, const QString& sent,
                        const QString& reply) {
  if (sent.isEmpty()) {
    return QObject::tr("%1 — nothing sent").arg(ExamineStageName(stage));
  }

  if (reply.isEmpty()) {
    return QObject::tr("%1 — \"%2\" → no answer")
        .arg(ExamineStageName(stage), sent);
  }

  return QObject::tr("%1 — \"%2\" → \"%3\"")
      .arg(ExamineStageName(stage), sent, reply);
}

QString ProvenanceNote(player::Provenance provenance) {
  switch (provenance) {
    case player::Provenance::kUnknown:
      return QObject::tr("not established");
    case player::Provenance::kReported:
      return QObject::tr("reported by the player");
    case player::Provenance::kMeasured:
      return QObject::tr("measured");
    case player::Provenance::kInferred:
      return QObject::tr("inferred");
    case player::Provenance::kDeclared:
      return QObject::tr("declared");
  }
  return QObject::tr("not established");
}

QString VideoStandardName(player::VideoStandard standard) {
  switch (standard) {
    case player::VideoStandard::kNtsc:
      return QObject::tr("NTSC");
    case player::VideoStandard::kPal:
      return QObject::tr("PAL");
    case player::VideoStandard::kUnknown:
      break;
  }
  return QObject::tr("Unknown");
}

QString DiscSizeName(player::DiscSize size) {
  switch (size) {
    case player::DiscSize::k30cm:
      return QObject::tr("12 inch");
    case player::DiscSize::k20cm:
      return QObject::tr("8 inch");
    case player::DiscSize::kUnknown:
      break;
  }
  return UnknownField();
}

QString FormatDiscAddress(int32_t address, player::AddressMode mode) {
  if (address < 0) {
    return UnknownField();
  }

  return mode == player::AddressMode::kTimeCode
             ? FormatTimeCode(address)
             : QObject::tr("Frame %1").arg(address);
}

QString ExamineSummary(const player::DiscProfile& disc,
                       player::ExamineOutcome outcome) {
  switch (outcome) {
    case player::ExamineOutcome::kInProgress:
      return QObject::tr("Examining the disc…");
    case player::ExamineOutcome::kTrayOpen:
      return QObject::tr("The tray is open — there is nothing to examine.");
    case player::ExamineOutcome::kNoDisc:
      return QObject::tr(
          "The player would not start a disc. Check that there is one in the "
          "tray and that it is the right way up.");
    case player::ExamineOutcome::kLinkFailed:
      return QObject::tr(
          "The link to the player failed part way through. What had been found "
          "by then is below.");
    case player::ExamineOutcome::kCancelled:
    case player::ExamineOutcome::kCompleted:
      break;
  }

  const QString prefix = outcome == player::ExamineOutcome::kCancelled
                             ? QObject::tr("Stopped early. ")
                             : QString();

  if (!disc.disc_type.known()) {
    return prefix + QObject::tr("The disc did not say what kind it is.");
  }

  const QString type = DiscTypeName(disc.disc_type.value);

  if (!disc.programme_end.known()) {
    return prefix +
           QObject::tr("A %1 disc, whose length could not be measured.")
               .arg(type);
  }

  return prefix + QObject::tr("A %1 disc, running to %2.")
                      .arg(type, FormatDiscAddress(disc.programme_end.value,
                                                   Addressing(disc)));
}

QString DiscProfileReport(const player::DiscProfile& disc,
                          player::ExamineOutcome outcome,
                          double bytes_per_second) {
  const player::AddressMode mode = Addressing(disc);

  QStringList lines;
  lines << ExamineSummary(disc, outcome);
  lines << QString();

  const auto row = [&lines](const QString& label, const QString& value,
                            player::Provenance provenance) {
    lines << QObject::tr("%1: %2  (%3)")
                 .arg(label, value, ProvenanceNote(provenance));
  };

  const auto fact = [&row](const QString& label, const QString& value,
                           player::Provenance provenance) {
    row(label,
        provenance == player::Provenance::kUnknown ? UnknownField() : value,
        provenance);
  };

  fact(QObject::tr("Disc"),
       disc.disc_present.value ? QObject::tr("present")
                               : QObject::tr("none the player could read"),
       disc.disc_present.provenance);

  fact(QObject::tr("Tray"), TrayStateName(disc.tray.value),
       disc.tray.provenance);

  fact(QObject::tr("Type"), DiscTypeName(disc.disc_type.value),
       disc.disc_type.provenance);

  fact(QObject::tr("Size"), DiscSizeName(disc.disc_size.value),
       disc.disc_size.provenance);

  // The one the capture actually needs and the old application never had: two
  // sides of one disc are two files, and nothing before this could tell them
  // apart without being told.
  fact(QObject::tr("Side"), QObject::tr("side %1").arg(disc.disc_side.value),
       disc.disc_side.provenance);

  fact(QObject::tr("Addressing"),
       mode == player::AddressMode::kTimeCode ? QObject::tr("time code")
                                              : QObject::tr("frame number"),
       disc.addressing.provenance);

  fact(QObject::tr("First address"),
       FormatDiscAddress(disc.programme_start.value, mode),
       disc.programme_start.provenance);

  fact(QObject::tr("Last address"),
       FormatDiscAddress(disc.programme_end.value, mode),
       disc.programme_end.provenance);

  fact(QObject::tr("Lead-in"),
       disc.lead_in_reachable.value
           ? QObject::tr("reached by seeking to the start")
           : QObject::tr("not reached by seeking to the start"),
       disc.lead_in_reachable.provenance);

  fact(QObject::tr("Chapters"),
       disc.chapters.value ? QObject::tr("present") : QObject::tr("none"),
       disc.chapters.provenance);

  fact(QObject::tr("Video standard"),
       VideoStandardName(disc.video_standard.value),
       disc.video_standard.provenance);
  if (!disc.video_standard.known()) {
    // The row is always there, and when it is empty it says why rather than
    // leaving a blank nobody can account for.
    lines << QObject::tr(
        "    This player did not answer the TV system request. It is the only "
        "command that carries the standard — the disc status reads the same "
        "for a PAL and an NTSC disc, and the model does not imply it either.");
  }

  const std::optional<std::chrono::seconds> duration = ProgrammeDuration(disc);
  if (duration.has_value()) {
    lines << QObject::tr("Playing time: %1")
                 .arg(FormatElapsed(static_cast<double>(duration->count())));

    if (bytes_per_second > 0.0) {
      const auto bytes = static_cast<uint64_t>(
          static_cast<double>(duration->count()) * bytes_per_second);
      lines << QObject::tr("Capture size at the current settings: about %1")
                   .arg(FormatByteSize(bytes));
    }
  } else if (disc.programme_end.known() &&
             disc.disc_type.value == player::DiscType::kCav &&
             !disc.video_standard.known()) {
    lines << QObject::tr(
        "Playing time: not known — a frame count is only a duration once the "
        "video standard is known.");
  }

  lines << QString();
  lines << QObject::tr(
      "User codes — informational only. Nothing in a capture "
      "is derived from them.");
  lines << UserCodeLine(QObject::tr("Standard"), disc.standard_user_code);
  lines << UserCodeLine(QObject::tr("Pioneer"), disc.pioneer_user_code);

  if (!disc.disc_status_reply.empty()) {
    lines << QString();
    // The working, beside the answer. Every character of it is decoded into the
    // rows above; showing it too is what lets somebody check the decode rather
    // than take it on trust.
    lines << QObject::tr(
                 "Disc status reply: \"%1\" — loaded, CAV/CLV, size, side, "
                 "chapters, in that order.")
                 .arg(Payload(disc.disc_status_reply));
  }

  return lines.join(QLatin1Char('\n'));
}

// --- Setting up and running an automatic capture ---------------------------

QString CaptureShapeName(player::CaptureShape shape) {
  switch (shape) {
    case player::CaptureShape::kWholeSide:
      return QObject::tr("The whole side, spin-up to spin-down");
    case player::CaptureShape::kRange:
      return QObject::tr("From one address to another");
    case player::CaptureShape::kFromSpinUp:
      return QObject::tr("From spin-up to an address");
  }
  return QObject::tr("The whole side, spin-up to spin-down");
}

QString PlanProblemText(player::PlanProblem problem) {
  switch (problem) {
    case player::PlanProblem::kNone:
      return QString();
    case player::PlanProblem::kNoDisc:
      return QObject::tr(
          "The examination found no disc in the player, so there is nothing to "
          "capture.");
    case player::PlanProblem::kUnknownDiscType:
      return QObject::tr(
          "The examination could not establish whether this is a CAV or a CLV "
          "disc, and the two are addressed differently. Examine the disc "
          "again.");
    case player::PlanProblem::kAddressingMismatch:
      return QObject::tr(
          "This setup was built for a disc of the other type. Examine the disc "
          "in the player and set the capture up from that.");
    case player::PlanProblem::kUnknownLength:
      return QObject::tr(
          "The end of the side was never measured, so there is nothing to stop "
          "at and no bound to check a range against. Examine the disc again.");
    case player::PlanProblem::kMalformedAddress:
      return QObject::tr("That is not an address a disc has.");
    case player::PlanProblem::kEndBeforeStart:
      return QObject::tr(
          "The capture would contain nothing: it ends where it starts, or "
          "before it.");
    case player::PlanProblem::kStartBeforeProgramme:
      return QObject::tr("The programme starts later than that.");
    case player::PlanProblem::kEndBeyondProgramme:
      return QObject::tr("The side ends before that.");
  }
  return QString();
}

QString AutoCaptureStageName(player::AutoCaptureStage stage) {
  switch (stage) {
    case player::AutoCaptureStage::kIdle:
      return QObject::tr("Ready");
    case player::AutoCaptureStage::kLockingFrontPanel:
      return QObject::tr("Locking the player's front panel");
    case player::AutoCaptureStage::kConfirmingDisc:
      return QObject::tr("Checking that this is still the same disc");
    case player::AutoCaptureStage::kCheckingTransport:
      // The same question at both of the two moments a stop might follow, but
      // worded for where it is asked: a stop sent to a disc that is not turning
      // ejects it, so neither stop goes out unasked.
      return QObject::tr("Checking whether the disc is already stopped");
    case player::AutoCaptureStage::kCheckingBeforeStopping:
      return QObject::tr(
          "Checking the disc is still turning before stopping "
          "it");
    case player::AutoCaptureStage::kSpinningDown:
      // Said in full, because a user watching their disc stop when they asked
      // for a capture is entitled to know why.
      return QObject::tr(
          "Stopping the disc — the spin-up can only be captured from a stop");
    case player::AutoCaptureStage::kSeekingStart:
      return QObject::tr("Going to the start of the capture");
    case player::AutoCaptureStage::kStartingCapture:
      return QObject::tr("Starting the capture");
    case player::AutoCaptureStage::kSpinningUp:
      return QObject::tr("Starting the disc");
    case player::AutoCaptureStage::kWatching:
      return QObject::tr("Capturing");
    case player::AutoCaptureStage::kCheckingStall:
      return QObject::tr(
          "Asking the player what it is doing — the disc has not moved for a "
          "few seconds");
    case player::AutoCaptureStage::kStoppingCapture:
      return QObject::tr("Finishing the capture file");
    case player::AutoCaptureStage::kStoppingPlayer:
      // Which on a whole-side capture happens with the writer still attached,
      // because the run-out is not an address and this is the only way it
      // reaches a file.
      return QObject::tr("Spinning the disc down");
    case player::AutoCaptureStage::kUnlockingFrontPanel:
      return QObject::tr("Releasing the player's front panel");
    case player::AutoCaptureStage::kFinished:
      return QObject::tr("Finished");
  }
  return QObject::tr("Working");
}

QString AutoCaptureOutcomeText(player::AutoCaptureOutcome outcome) {
  switch (outcome) {
    case player::AutoCaptureOutcome::kInProgress:
      return QObject::tr("in progress");
    case player::AutoCaptureOutcome::kCompleted:
      return QObject::tr("completed");
    case player::AutoCaptureOutcome::kInvalidPlan:
      return QObject::tr("refused: the setup does not describe this disc");
    case player::AutoCaptureOutcome::kUnsupportedPlayer:
      return QObject::tr("refused: this player cannot be driven automatically");
    case player::AutoCaptureOutcome::kDiscChanged:
      return QObject::tr("stopped: the disc is not the one that was examined");
    case player::AutoCaptureOutcome::kPlayerRefused:
      return QObject::tr("stopped: the player refused");
    case player::AutoCaptureOutcome::kStalled:
      return QObject::tr("stopped: the disc stopped advancing");
    case player::AutoCaptureOutcome::kPlayerStopped:
      return QObject::tr("stopped: the player stopped");
    case player::AutoCaptureOutcome::kCaptureFailed:
      return QObject::tr("stopped: the capture could not be written");
    case player::AutoCaptureOutcome::kLinkFailed:
      return QObject::tr("stopped: the link to the player failed");
    case player::AutoCaptureOutcome::kCancelled:
      return QObject::tr("stopped at your request");
  }
  return QObject::tr("finished");
}

QString AutoCaptureSummary(player::AutoCaptureOutcome outcome) {
  switch (outcome) {
    case player::AutoCaptureOutcome::kInProgress:
      return QObject::tr("Capturing.");
    case player::AutoCaptureOutcome::kCompleted:
      return QObject::tr("The capture finished and the file is closed.");
    case player::AutoCaptureOutcome::kInvalidPlan:
      return QObject::tr(
          "Nothing was captured: the setup does not describe the disc in the "
          "player.");
    case player::AutoCaptureOutcome::kUnsupportedPlayer:
      return QObject::tr(
          "Nothing was captured: this player has no command for something the "
          "capture needs. Use the remote to drive it by hand.");
    case player::AutoCaptureOutcome::kDiscChanged:
      return QObject::tr(
          "Nothing was captured: the disc in the player is not the one that "
          "was examined. Examine it and set the capture up again.");
    case player::AutoCaptureOutcome::kPlayerRefused:
      return QObject::tr(
          "The player refused a command the capture depends on. Anything that "
          "had been captured has been written and the file closed.");
    case player::AutoCaptureOutcome::kStalled:
      return QObject::tr(
          "The disc stopped advancing while the player went on reporting that "
          "it was playing. The capture was stopped rather than left writing, "
          "and the file is closed. This usually means a disc the player cannot "
          "read past.");
    case player::AutoCaptureOutcome::kPlayerStopped:
      return QObject::tr(
          "The player stopped before the end of the capture. What was captured "
          "up to then has been written and the file closed.");
    case player::AutoCaptureOutcome::kCaptureFailed:
      return QObject::tr("The capture could not be written. See the log.");
    case player::AutoCaptureOutcome::kLinkFailed:
      return QObject::tr(
          "The link to the player failed, so the automation has stopped. **The "
          "capture is still running** — the player will play to the end of the "
          "side by itself, and stopping the capture is now yours to do.");
    case player::AutoCaptureOutcome::kCancelled:
      return QObject::tr(
          "Stopped at your request. The file was finished properly and the "
          "player put back.");
  }
  return QObject::tr("The capture finished.");
}

QString AutoCaptureEstimate(const player::AutoCapturePlan& plan,
                            const player::DiscProfile& disc,
                            double bytes_per_second) {
  const std::optional<std::chrono::seconds> duration =
      player::PlannedDuration(plan, disc);
  if (!duration.has_value()) {
    return QString();
  }

  const QString elapsed = FormatElapsed(static_cast<double>(duration->count()));

  if (bytes_per_second <= 0.0) {
    return QObject::tr("About %1.").arg(elapsed);
  }

  const auto bytes = static_cast<uint64_t>(
      static_cast<double>(duration->count()) * bytes_per_second);
  return QObject::tr("About %1, and about %2 on disk.")
      .arg(elapsed, FormatByteSize(bytes));
}

QString AutoCaptureRemainingText(const player::AutoCapturePlan& plan,
                                 const player::DiscProfile& disc,
                                 int32_t address) {
  if (address < 0 || address >= plan.end_address || !disc.disc_type.known()) {
    return QString();
  }

  const std::optional<std::chrono::seconds> remaining =
      player::AddressSpanDuration(address, plan.end_address,
                                  disc.disc_type.value,
                                  disc.video_standard.value);
  if (!remaining.has_value()) {
    return QString();
  }

  return QObject::tr("about %1 left")
      .arg(FormatElapsed(static_cast<double>(remaining->count())));
}

QString SuggestedCaptureName(const player::DiscProfile& disc) {
  QStringList parts;

  if (disc.disc_type.known()) {
    parts << DiscTypeName(disc.disc_type.value);
  }

  if (disc.video_standard.known()) {
    parts << VideoStandardName(disc.video_standard.value);
  }

  // The one that earns its place. Two files made in a row are the two sides of
  // one disc, and telling them apart afterwards is the whole problem.
  if (disc.disc_side.known()) {
    parts << QObject::tr("Side%1").arg(disc.disc_side.value);
  }

  if (parts.isEmpty()) {
    return QString();
  }

  return parts.join(QLatin1Char('_'));
}

namespace {

// The fixed English vocabulary the sidecar records provenance in — see the
// header, where the reason it is not translated is set out.
const char* ProvenanceKeyword(player::Provenance provenance) {
  switch (provenance) {
    case player::Provenance::kReported:
      return "reported";
    case player::Provenance::kMeasured:
      return "measured";
    case player::Provenance::kInferred:
      return "inferred";
    case player::Provenance::kDeclared:
      return "declared";
    case player::Provenance::kUnknown:
      break;
  }
  return "";
}

const char* UserCodeOutcomeKeyword(player::UserCodeReading::Outcome outcome) {
  switch (outcome) {
    case player::UserCodeReading::Outcome::kNotRead:
      return "not read";
    case player::UserCodeReading::Outcome::kRead:
      return "read";
    case player::UserCodeReading::Outcome::kNotEncoded:
      return "not encoded on the disc";
    case player::UserCodeReading::Outcome::kRefused:
      return "no usable answer";
  }
  return "not read";
}

// One field of the profile as a value and where it came from.
//
// A template rather than a function per field, because the shape is the same
// for all of them and only the formatting of the value differs — which is what
// the caller supplies.
template <typename T, typename Format>
capture::ScannedFact Fact(const player::Fact<T>& fact, Format format) {
  if (!fact.known()) {
    return {};
  }

  capture::ScannedFact scanned;
  scanned.value = format(fact.value);
  scanned.source = ProvenanceKeyword(fact.provenance);
  return scanned;
}

std::string YesOrNo(bool value) { return value ? "yes" : "no"; }

}  // namespace

capture::PlayerIdentity DescribePlayerIdentity(
    const PlayerConnection& connection) {
  if (!connection.live()) {
    // Not merely unpopulated: a link that is searching, disconnected or
    // switched off has no player on the end of it, and half-filling this from
    // whatever the last one said would put the previous session's player in
    // this session's file.
    return {};
  }

  capture::PlayerIdentity player;
  player.model_name = connection.model_name.toStdString();
  player.model_id_code = connection.model_id_code.toStdString();
  player.model_code = connection.model_code.toStdString();
  player.firmware_version = connection.firmware_version.toStdString();
  player.port = connection.port_path.toStdString();
  player.baud_rate = connection.baud_rate;
  player.recognised_model = connection.recognised_model;
  return player;
}

capture::DiscScan DescribeDiscScan(const player::DiscProfile& disc) {
  capture::DiscScan scan;
  scan.examined = true;

  // The addressing decides how the two programme ends are written, and it is
  // taken from the profile rather than assumed: a CLV disc's addresses are time
  // codes, and printing one as a frame number produces a seven-digit number
  // that looks like a frame count and is not.
  const player::AddressMode mode = disc.addressing.known()
                                       ? disc.addressing.value
                                       : player::AddressMode::kFrame;

  scan.disc_present = Fact(disc.disc_present, YesOrNo);
  scan.tray = Fact(disc.tray, [](player::TrayState tray) {
    return TrayStateName(tray).toStdString();
  });
  scan.disc_type = Fact(disc.disc_type, [](player::DiscType type) {
    return DiscTypeName(type).toStdString();
  });
  scan.addressing = Fact(disc.addressing, [](player::AddressMode value) {
    return std::string(value == player::AddressMode::kTimeCode ? "time code"
                                                               : "frame");
  });
  scan.disc_size = Fact(disc.disc_size, [](player::DiscSize size) {
    return DiscSizeName(size).toStdString();
  });
  scan.disc_side =
      Fact(disc.disc_side, [](int side) { return std::to_string(side); });
  scan.video_standard =
      Fact(disc.video_standard, [](player::VideoStandard standard) {
        return VideoStandardName(standard).toStdString();
      });

  scan.programme_start = Fact(disc.programme_start, [mode](int32_t address) {
    return FormatDiscAddress(address, mode).toStdString();
  });
  scan.programme_end = Fact(disc.programme_end, [mode](int32_t address) {
    return FormatDiscAddress(address, mode).toStdString();
  });

  // Derived rather than read, so it is recorded as inferred: it follows from
  // the two ends above and, for a CAV disc, from the video standard. Nothing at
  // all where that standard was never established — a frame count is not a
  // duration until something says how many frames go past in a second, and
  // assuming thirty is twenty per cent wrong on a PAL disc.
  if (const std::optional<std::chrono::seconds> duration =
          player::ProgrammeDuration(disc);
      duration.has_value()) {
    scan.programme_duration.value = std::to_string(duration->count());
    scan.programme_duration.source = "inferred";
  }

  scan.lead_in_reachable = Fact(disc.lead_in_reachable, YesOrNo);
  scan.chapters = Fact(disc.chapters, YesOrNo);

  scan.disc_status_reply = disc.disc_status_reply;

  scan.standard_user_code_outcome =
      UserCodeOutcomeKeyword(disc.standard_user_code.outcome);
  scan.standard_user_code = disc.standard_user_code.text;

  // Recorded exactly as it arrived, unreadable characters and all. Those
  // characters are the evidence — see user_code.h, where telling a field that
  // was never encoded from one the player could not read is the whole job — and
  // the metadata writer escapes them rather than dropping them.
  scan.pioneer_user_code_outcome =
      UserCodeOutcomeKeyword(disc.pioneer_user_code.outcome);
  scan.pioneer_user_code = disc.pioneer_user_code.text;

  return scan;
}

}  // namespace ddd::gui
