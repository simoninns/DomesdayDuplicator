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

// How an address on this disc is written and read.
enum class AddressMode : uint8_t {
  kFrame,
  kTimeCode,
};

constexpr AddressMode AddressModeFor(DiscType type) {
  return type == DiscType::kClv ? AddressMode::kTimeCode : AddressMode::kFrame;
}

}  // namespace ddd::player
