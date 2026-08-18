/************************************************************************

    command_encoder.cpp

    A definition and an argument, turned into bytes for the wire
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "command_encoder.h"

#include <cstddef>

namespace ddd::player {
namespace {

// The largest value that fits in `digits` decimal digits.
int32_t LargestValueIn(uint8_t digits) {
  int32_t limit = 1;
  for (uint8_t digit = 0; digit < digits; ++digit) {
    limit *= 10;
  }
  return limit - 1;
}

EncodedCommand Failure(EncodeStatus status) {
  EncodedCommand encoded;
  encoded.status = status;
  return encoded;
}

// Look a parameter up in one of a definition's parameter tables and encode the
// command it belongs to. Shared by the audio and speed entry points, which
// differ only in which table they read.
EncodedCommand EncodeParameterised(const PlayerDefinition& definition,
                                   PlayerCommand command, int8_t parameter) {
  if (parameter == kParameterUnsupported) {
    return Failure(EncodeStatus::kParameterUnsupported);
  }
  return EncodeCommand(definition, command, parameter);
}

}  // namespace

EncodedCommand EncodeCommand(const PlayerDefinition& definition,
                             PlayerCommand command,
                             std::optional<int32_t> argument) {
  const CommandSpec& spec = Spec(definition, command);

  if (!spec.present()) {
    return Failure(EncodeStatus::kCommandUnsupported);
  }

  if (!spec.takes_argument() && argument.has_value()) {
    return Failure(EncodeStatus::kUnexpectedArgument);
  }

  std::string bytes(spec.prefix);

  if (spec.takes_argument()) {
    if (!argument.has_value()) {
      return Failure(EncodeStatus::kArgumentRequired);
    }

    // Unsigned and bounded by the declared width. A negative address is not a
    // position on a disc, and a value too wide for the format would be sent as
    // a different address rather than refused.
    const int32_t value = *argument;
    if (value < 0 || value > LargestValueIn(spec.argument_digits)) {
      return Failure(EncodeStatus::kArgumentOutOfRange);
    }

    bytes += std::to_string(value);
  }

  bytes += spec.suffix;
  bytes += kCommandTerminator;

  if (bytes.size() > kMaximumCommandLength) {
    return Failure(EncodeStatus::kCommandTooLong);
  }

  EncodedCommand encoded;
  encoded.status = EncodeStatus::kOk;
  encoded.bytes = std::move(bytes);
  return encoded;
}

EncodedCommand EncodeAudio(const PlayerDefinition& definition, AudioMode mode) {
  const size_t index = static_cast<size_t>(mode);
  if (index >= kAudioModeCount) {
    return Failure(EncodeStatus::kParameterUnsupported);
  }
  return EncodeParameterised(definition, PlayerCommand::kSetAudio,
                             definition.audio_parameters[index]);
}

EncodedCommand EncodeSpeed(const PlayerDefinition& definition,
                           PlaybackSpeed speed) {
  const size_t index = static_cast<size_t>(speed);
  if (index >= kPlaybackSpeedCount) {
    return Failure(EncodeStatus::kParameterUnsupported);
  }
  return EncodeParameterised(definition, PlayerCommand::kSetSpeed,
                             definition.speed_parameters[index]);
}

}  // namespace ddd::player
