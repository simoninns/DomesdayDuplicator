/************************************************************************

    player_request.h

    Something asked of the player, and what came back
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <cstdint>
#include <optional>

#include "player_command.h"
#include "response_parser.h"

namespace ddd::gui {

// One thing the interface asked the player to do.
//
// A plain value that crosses the thread boundary in both directions: the remote
// fills one in, the worker executes it, and it comes back inside the reply. It
// comes back rather than being remembered on either side because the interface
// is allowed to have more than one thing outstanding — somebody may press Play
// while a manual command is still waiting for its answer — and matching a reply
// to a request by remembering "the last one" is how the wrong reply ends up in
// the wrong box.
struct PlayerRequest {
  enum class Kind : uint8_t {
    // A control from the generic set, encoded through the model's definition.
    kCommand,

    // The two commands whose parameter comes from a per-model table.
    kAudio,
    kSpeed,

    // Bytes the schema does not describe. The manual command field, and the
    // only way to find out what an unrecognised player does — which is how the
    // next definition header gets written.
    kRaw,
  };

  // Assigned by the controller when the request is submitted, and returned on
  // the reply. Zero means nobody cared which answer was which.
  uint64_t id = 0;

  Kind kind = Kind::kCommand;

  player::PlayerCommand command = player::PlayerCommand::kQueryActiveMode;
  std::optional<int32_t> argument;

  player::AudioMode audio = player::AudioMode::kAnalogStereo;
  player::PlaybackSpeed speed = player::PlaybackSpeed::kNormal;

  // For kRaw. Sent as typed, with the terminator added if it is missing.
  QString raw;

  // For kRaw only; the schema supplies both for every other kind.
  //
  // A manual command's reply is read as text rather than as an acknowledgement,
  // so that a refusal arrives as the bytes the player sent rather than as this
  // library's opinion of them — which is the entire point of the field. And it
  // gets the long timeout, because a hand-typed "FR20000SE" is a seek and a
  // seek is allowed thirty seconds.
  player::ResponseKind response = player::ResponseKind::kText;
  player::TimeoutClass timeout = player::TimeoutClass::kLong;
};

// The shapes a request comes in, so a caller states the shape rather than
// filling in fields most of which would be defaults.

inline PlayerRequest CommandRequest(player::PlayerCommand command,
                                    std::optional<int32_t> argument = {}) {
  PlayerRequest request;
  request.kind = PlayerRequest::Kind::kCommand;
  request.command = command;
  request.argument = argument;
  return request;
}

inline PlayerRequest AudioRequest(player::AudioMode mode) {
  PlayerRequest request;
  request.kind = PlayerRequest::Kind::kAudio;
  request.command = player::PlayerCommand::kSetAudio;
  request.audio = mode;
  return request;
}

inline PlayerRequest SpeedRequest(player::PlaybackSpeed speed) {
  PlayerRequest request;
  request.kind = PlayerRequest::Kind::kSpeed;
  request.command = player::PlayerCommand::kSetSpeed;
  request.speed = speed;
  return request;
}

inline PlayerRequest RawRequest(const QString& command) {
  PlayerRequest request;
  request.kind = PlayerRequest::Kind::kRaw;
  request.raw = command;
  return request;
}

// What the player said, and what was said to it.
struct PlayerReply {
  PlayerRequest request;

  player::ReplyStatus status = player::ReplyStatus::kNotConnected;

  // The reply with its terminator removed. Verbatim: nothing here interprets
  // it, which is what the manual command field needs and what makes the log a
  // serial trace rather than a summary of one.
  QString text;

  // The player's error code, where the reply was a refusal carrying a legible
  // one.
  QString error_code;

  // What went on the wire, with the trailing terminator taken off — a control
  // character in a label helps nobody. Empty where nothing was sent, which is
  // the case for a command the model does not have.
  QString sent;

  bool ok() const { return status == player::ReplyStatus::kOk; }
};

}  // namespace ddd::gui
