/************************************************************************

    player_state.h

    What a player is doing, and what it has in it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>

namespace ddd::player {

// The states a player reports.
//
// Finer-grained than the old application, which folded search, scan, set-up and
// multi-speed all into "playing" and door-open, park and unloading all into
// "stopped". Both foldings are still available below as predicates, so nothing
// that wants the coarse view has to enumerate; but the examine sequence needs
// to tell "searching" from "playing" — a player that is still seeking has not
// arrived, and treating it as playing is how a length measurement is read too
// early.
enum class PlayerState : uint8_t {
  kUnknown,

  kDoorOpen,
  kParked,

  // Spinning up, about to play.
  kSettingUp,

  kUnloading,

  kPlaying,

  // A CAV frame held still. The disc is still turning.
  kStillFrame,

  // Paused with no picture.
  kPaused,

  kSearching,
  kScanning,
  kMultiSpeed,
};

// Is the disc turning?
constexpr bool IsSpinning(PlayerState state) {
  switch (state) {
    case PlayerState::kSettingUp:
    case PlayerState::kPlaying:
    case PlayerState::kStillFrame:
    case PlayerState::kPaused:
    case PlayerState::kSearching:
    case PlayerState::kScanning:
    case PlayerState::kMultiSpeed:
      return true;
    case PlayerState::kUnknown:
    case PlayerState::kDoorOpen:
    case PlayerState::kParked:
    case PlayerState::kUnloading:
      return false;
  }
  return false;
}

// Is the picture advancing? True only where the address is expected to be
// moving, which is what the automatic capture watches for a stall.
constexpr bool IsAdvancing(PlayerState state) {
  switch (state) {
    case PlayerState::kPlaying:
    case PlayerState::kScanning:
    case PlayerState::kMultiSpeed:
      return true;
    case PlayerState::kUnknown:
    case PlayerState::kDoorOpen:
    case PlayerState::kParked:
    case PlayerState::kSettingUp:
    case PlayerState::kUnloading:
    case PlayerState::kStillFrame:
    case PlayerState::kPaused:
    case PlayerState::kSearching:
      return false;
  }
  return false;
}

enum class TrayState : uint8_t {
  kUnknown,
  kOpen,
  kClosed,
};

// The tray follows from the reported state: a player that says the door is open
// has an open door, and one that says anything else has a closed one. Derived
// rather than asked for separately, because the old application asked the same
// question twice with the same command and could get two different answers.
constexpr TrayState TrayStateFor(PlayerState state) {
  switch (state) {
    case PlayerState::kUnknown:
      return TrayState::kUnknown;
    case PlayerState::kDoorOpen:
      return TrayState::kOpen;
    default:
      return TrayState::kClosed;
  }
}

enum class DiscType : uint8_t {
  kUnknown,

  // Constant angular velocity: one frame per revolution, addressed by frame
  // number, still frame and step available.
  kCav,

  // Constant linear velocity: addressed by time code.
  kClv,
};

// Which of the two disc diameters this is.
//
// Named in centimetres because that is what the format is specified in, and
// said in inches to a user because that is what a disc is sold as.
enum class DiscSize : uint8_t {
  kUnknown,

  // 30 cm — a 12-inch disc, which is nearly all of them.
  k30cm,

  // 20 cm — an 8-inch disc.
  k20cm,
};

// Which television standard the disc carries.
//
// Established by the TV system request — see TvSystemDecode. It is not in the
// disc-status reply, and the model does not imply it either: this project's own
// LD-V4300D is dual-format, and plays both.
enum class VideoStandard : uint8_t {
  kUnknown,
  kNtsc,
  kPal,
};

// How an address on this disc is written and read.
enum class AddressMode : uint8_t {
  kFrame,
  kTimeCode,
};

constexpr AddressMode AddressModeFor(DiscType type) {
  return type == DiscType::kClv ? AddressMode::kTimeCode : AddressMode::kFrame;
}

}  // namespace ddd::player
