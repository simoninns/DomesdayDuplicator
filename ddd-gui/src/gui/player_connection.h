/************************************************************************

    player_connection.h

    Whether there is a player, and how it was found
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <cstdint>

namespace ddd::gui {

enum class PlayerConnectionState : uint8_t {
  // Player control is switched off. No port on this machine is opened, written
  // to, or enumerated.
  kDisabled,

  // Looking. Ports are being probed right now.
  kSearching,

  kConnected,

  // A player answered, and it is not the model the user selected. Still a live
  // connection — it is a real player and everything works — but said plainly,
  // because it usually means the setup is not what the user thinks it is.
  kModelMismatch,

  // Nothing to talk to. Why is in `problem`, and a retry is already scheduled.
  kDisconnected,
};

// Why there is no connection. Separate from the state because "not connected"
// is one state with four quite different remedies, and a panel that said only
// "not connected" would send a user looking for a fault in the wrong place.
enum class PlayerConnectionProblem : uint8_t {
  kNone,

  // Ports were opened and probed; nothing answered.
  kNoPlayerFound,

  // The port could not be opened at all: busy, absent, or — the most likely
  // first-run experience on Linux — not permitted.
  kPortUnavailable,

  // Something answered, and it was not a player. Usually a serial device that
  // is not a player at all, which is worth saying rather than reporting as
  // silence.
  kNotAPlayer,

  // There was a player and the link went away underneath it.
  kLinkLost,
};

// The whole of what the interface knows about the link.
//
// Crosses a thread boundary — the worker fills it in, the panel and the status
// bar read it — so it is a plain value with no pointers into anything the
// worker owns. In particular the model is carried as its name and ID rather
// than as a PlayerDefinition pointer: the pointer would be perfectly valid
// (definitions are compile-time constants) and would still be the wrong thing
// to put in a signal, because it invites the interface to start asking the
// protocol layer questions on the GUI thread.
struct PlayerConnection {
  PlayerConnectionState state = PlayerConnectionState::kDisabled;
  PlayerConnectionProblem problem = PlayerConnectionProblem::kNone;

  // Where it was found and at what rate. Empty and zero when there is nothing.
  QString port_path;
  uint32_t baud_rate = 0;

  QString model_name;
  QString model_id_code;
  QString firmware_version;

  // The identifying string the player answered with, kept because it is what
  // somebody will be asked for when a definition needs writing for their
  // player.
  QString model_code;

  // False when the player identified itself correctly but with a model ID no
  // definition claims, and it is being driven with the generic command set.
  bool recognised_model = false;

  // False when this model's definition has never been exercised against the
  // hardware it describes. The panel says so — see players/README.md.
  bool bench_verified = false;

  // The model the user selected, when it is not the one that answered. Only
  // set in kModelMismatch, and only so the message can name both.
  QString selected_model_name;

  // Whatever else is worth showing: the unexpected reply for kNotAPlayer, the
  // port for kPortUnavailable.
  QString detail;

  // Is there a player on the end of this?
  bool live() const {
    return state == PlayerConnectionState::kConnected ||
           state == PlayerConnectionState::kModelMismatch;
  }

  bool operator==(const PlayerConnection& other) const {
    return state == other.state && problem == other.problem &&
           port_path == other.port_path && baud_rate == other.baud_rate &&
           model_name == other.model_name &&
           model_id_code == other.model_id_code &&
           firmware_version == other.firmware_version &&
           model_code == other.model_code &&
           recognised_model == other.recognised_model &&
           bench_verified == other.bench_verified &&
           selected_model_name == other.selected_model_name &&
           detail == other.detail;
  }
  bool operator!=(const PlayerConnection& other) const {
    return !(*this == other);
  }
};

}  // namespace ddd::gui
