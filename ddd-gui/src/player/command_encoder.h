/************************************************************************

    command_encoder.h

    A definition and an argument, turned into bytes for the wire
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "player_command.h"
#include "player_definition.h"

namespace ddd::player {

// Why a command could not be encoded.
enum class EncodeStatus : uint8_t {
  kOk,

  // The model has no sequence for this command.
  kCommandUnsupported,

  kArgumentRequired,
  kUnexpectedArgument,

  // Negative, or wider than the command's declared digits.
  kArgumentOutOfRange,

  // The model has no wire parameter for this audio mode or playback speed.
  kParameterUnsupported,

  // Longer than a player will accept.
  //
  // A failure rather than a truncation, and that is the whole reason it is
  // checked. The old application truncated on the way out, which turns a
  // command that is too long into a different, perfectly valid one — a seek to
  // the wrong frame rather than a refusal, and nothing to see afterwards.
  kCommandTooLong,
};

struct EncodedCommand {
  EncodeStatus status = EncodeStatus::kCommandUnsupported;

  // The bytes to write, terminator included. Empty unless the status is kOk.
  std::string bytes;

  bool ok() const { return status == EncodeStatus::kOk; }
};

// Encode one command for one model.
//
// The argument must be supplied exactly when the command's spec takes one; both
// forgetting it and supplying a spurious one are refusals rather than silent
// omissions, because a seek with no address and a play with an address are both
// somebody's mistake further up.
EncodedCommand EncodeCommand(const PlayerDefinition& definition,
                             PlayerCommand command,
                             std::optional<int32_t> argument = std::nullopt);

// Encode a change of audio, through the model's parameter table.
EncodedCommand EncodeAudio(const PlayerDefinition& definition, AudioMode mode);

// Encode a change of playback speed, through the model's parameter table.
EncodedCommand EncodeSpeed(const PlayerDefinition& definition,
                           PlaybackSpeed speed);

}  // namespace ddd::player
