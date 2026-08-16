/************************************************************************

    player_command.h

    The controls a player offers, independent of any one model
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ddd::player {

// The generic controls. Everything above this library — the remote, the examine
// sequence, the automatic capture — asks for one of these, and a model's
// definition says what to put on the wire for it.
//
// The set is deliberately what the old application's PlayerCommunication
// already exposed, because that set is known to be sufficient to drive a
// capture and is known to work on real players. Two deliberate absences:
//
//   The stop registers. The old application declared setStopFrame() and
//   setStopTimeCode() and both were stubs that sent nothing. Rather than invent
//   a command sequence for them, the automatic capture watches the address and
//   stops the capture itself, which is what the old application actually did.
//
//   The model request. It is the one command that has to be sent before any
//   definition is known, so it lives in ProbeSpec instead. Having it here as
//   well would be two places to change it.
enum class PlayerCommand : uint8_t {
  // Tray
  kTrayOpen,
  kTrayClose,

  // Transport
  kPlay,

  // Play with the disc's stop codes ignored. A CAV disc with stop codes pauses
  // part way through a side, which ends a whole-side capture early; this is how
  // the old application got past that.
  kPlayWithoutStopCodes,

  kPause,
  kStillFrame,
  kStop,
  kStepForward,
  kStepReverse,
  kScanForward,
  kScanReverse,
  kMultiSpeedForward,
  kMultiSpeedReverse,
  kSetSpeed,

  // Seeking
  kSeekFrame,
  kSeekTimeCode,
  kSeekChapter,

  // Presentation
  kDisplayOn,
  kDisplayOff,
  kSetAudio,
  kKeyLockOn,
  kKeyLockOff,

  // Queries
  kQueryActiveMode,
  kQueryAddress,
  kQueryDiscStatus,
  kQueryStandardUserCode,
  kQueryPioneerUserCode,

  // Which television standard the disc carries. The only command that answers
  // it — it is not in the disc status, and the model does not imply it.
  kQueryTvSystem,

  kQueryPhysicalPosition,

  kCount,
};

inline constexpr size_t kPlayerCommandCount =
    static_cast<size_t>(PlayerCommand::kCount);

// The command table is an array, so this is how it is indexed.
constexpr size_t Index(PlayerCommand command) {
  return static_cast<size_t>(command);
}

// Which audio the player should present. The parameter each of these becomes on
// the wire is per-model data, in PlayerDefinition::audio_parameters, because a
// player with no digital audio has no parameter for the digital modes.
enum class AudioMode : uint8_t {
  kOff,
  kAnalogChannel1,
  kAnalogChannel2,
  kAnalogStereo,
  kDigitalChannel1,
  kDigitalChannel2,
  kDigitalStereo,
  kCount,
};

inline constexpr size_t kAudioModeCount =
    static_cast<size_t>(AudioMode::kCount);

// Multi-speed playback rates, likewise mapped to a wire parameter per model.
enum class PlaybackSpeed : uint8_t {
  kSixth,
  kQuarter,
  kThird,
  kHalf,
  kNormal,
  kDouble,
  kTriple,
  kQuadruple,
  kCount,
};

inline constexpr size_t kPlaybackSpeedCount =
    static_cast<size_t>(PlaybackSpeed::kCount);

// A parameter table entry meaning "this model cannot do that".
inline constexpr int8_t kParameterUnsupported = -1;

// How a command's argument is written.
//
// One encoding, because one is what the Pioneer command set uses: the argument
// is decimal and unpadded, so a seek to frame 1 is "FR1SE" and not "FR00001SE".
// A model needing zero padding would be a new encoding here and a change to one
// function in command_encoder.cpp; adding it before anything needs it would be
// a code path nothing exercises.
enum class ArgumentEncoding : uint8_t {
  kNone,
  kDecimal,
};

// What kind of answer the command produces.
//
// The distinction is load-bearing rather than descriptive: an acknowledgement
// is checked for the player's error convention, and a text reply is not. A user
// code is arbitrary bytes and may well contain an 'E'.
enum class ResponseKind : uint8_t {
  kAcknowledgement,
  kText,
};

// How long to wait for the answer.
//
// Two classes, as in the old application: most commands answer promptly, while
// play, stop, seek and tray movement have a mechanism to wait for. The numbers
// are the old application's N_TIMEOUT and L_TIMEOUT unchanged, because they are
// what years of captures were taken with.
enum class TimeoutClass : uint8_t {
  kNormal,
  kLong,
};

inline constexpr std::chrono::milliseconds kNormalCommandTimeout{5000};
inline constexpr std::chrono::milliseconds kLongCommandTimeout{30000};

constexpr std::chrono::milliseconds CommandTimeout(TimeoutClass timeout_class) {
  return timeout_class == TimeoutClass::kLong ? kLongCommandTimeout
                                              : kNormalCommandTimeout;
}

// Every command and every reply ends with a carriage return.
inline constexpr char kCommandTerminator = '\r';

// The longest command a player will accept, in bytes including the terminator.
//
// The old application truncated to twenty characters on the way out, which
// turns a command that is too long into a *different, valid* command — a seek
// to the wrong frame rather than a refusal. Here it is a validation failure
// instead; see command_encoder.h.
inline constexpr size_t kMaximumCommandLength = 20;

// What to send for one command on one model.
//
// Data rather than a format string, and that is the point of the whole schema.
// "FR%uSE" cannot be checked by the compiler, cannot be swept by a test, and
// gives a definition author a way to format a frame number into a tray command.
// Split into its parts, every registered model can be validated in one pass and
// the encoder is a single function with a single set of edge cases.
//
// An empty prefix and an empty suffix mean the model does not have the command.
struct CommandSpec {
  // "FR" in "FR100SE"; empty where the argument comes first, as in "1AD".
  std::string_view prefix;

  // "SE" in "FR100SE", or "AD" in "1AD".
  std::string_view suffix;

  ArgumentEncoding argument = ArgumentEncoding::kNone;

  // The most digits the argument may have. A guard against a programming
  // mistake rather than a validator of user input — a five-digit frame number
  // is what the format allows, and something asking for six has gone wrong
  // somewhere above.
  uint8_t argument_digits = 0;

  ResponseKind response = ResponseKind::kAcknowledgement;
  TimeoutClass timeout = TimeoutClass::kNormal;

  constexpr bool present() const { return !prefix.empty() || !suffix.empty(); }

  constexpr bool takes_argument() const {
    return argument != ArgumentEncoding::kNone;
  }
};

// The three shapes a command comes in, so a definition header states the shape
// rather than filling in six fields — most of which would be defaults, and a
// table of defaults is a table nobody reads.

// A command with no argument: "PL", "0KL", "PL64RBMF".
constexpr CommandSpec Command(std::string_view sequence,
                              TimeoutClass timeout = TimeoutClass::kNormal) {
  CommandSpec spec;
  spec.prefix = sequence;
  spec.timeout = timeout;
  return spec;
}

// A command with a decimal argument, which may sit between a prefix and a
// suffix ("FR" 100 "SE") or in front of one ("" 1 "AD").
constexpr CommandSpec CommandWithArgument(
    std::string_view prefix, std::string_view suffix, uint8_t digits,
    TimeoutClass timeout = TimeoutClass::kNormal) {
  CommandSpec spec;
  spec.prefix = prefix;
  spec.suffix = suffix;
  spec.argument = ArgumentEncoding::kDecimal;
  spec.argument_digits = digits;
  spec.timeout = timeout;
  return spec;
}

// A question, whose reply is the answer rather than an acknowledgement of it.
constexpr CommandSpec Query(std::string_view sequence,
                            TimeoutClass timeout = TimeoutClass::kNormal) {
  CommandSpec spec;
  spec.prefix = sequence;
  spec.response = ResponseKind::kText;
  spec.timeout = timeout;
  return spec;
}

}  // namespace ddd::player
