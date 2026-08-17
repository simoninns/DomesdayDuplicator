/************************************************************************

    player_settings.h

    How the application is told to find the player, and where that is kept
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <QStringList>
#include <cstdint>

namespace ddd::gui {

// Everything the user can say about their player.
//
// A plain value, like CaptureSettings and for the same reasons: it is passed
// whole to the controller, compared whole in tests, and saved whole, so there
// is no way for a dialog and a worker to disagree about which half has been
// applied.
//
// Three of the fields are "or work it out", and the sentinel is the empty or
// zero value in each case. That is deliberate: the state a user should be in is
// the one where they have told the application nothing at all, and it found the
// player anyway.
struct PlayerSettings {
  // Off by default, and it has to be. Searching for a player means writing
  // bytes to every serial port on the machine, and a capture application that
  // did that on first launch — to a UPS, an instrument, whatever else is on the
  // bench — would be doing something nobody asked it to.
  bool enabled = false;

  // The model the user says they have, as its registry ID code. Empty means
  // "whatever answers", which is the recommended setting: the player says which
  // model it is, so selecting one only adds a way to be wrong. It earns its
  // place as a check — if the user has said LD-V4300D and an LD-V8000 answers,
  // something is not the setup they think it is.
  QString model_id_code;

  // The port the user has fixed on. Empty means "find it".
  //
  // A fixed port is never departed from: if the player is not there, the
  // application says so rather than searching elsewhere. Quietly succeeding on
  // a different port would leave a setting that does not describe the hardware
  // and a fault that reappears on the next machine.
  QString port_path;

  // The rate the user has fixed on, or zero for "work it out".
  uint32_t baud_rate = 0;

  // Ports never to open. The user's answer to "something else is on that one",
  // and the only protection available for equipment this application knows
  // nothing about.
  QStringList excluded_ports;

  // --- Remembered rather than chosen ---------------------------------------
  //
  // Written by the controller when a player is found, not by any dialog. Their
  // whole purpose is that the second run costs one probe instead of a scan of
  // every port at every rate — and that a machine which has connected before
  // never writes to any other port again unless that one has stopped answering.

  QString remembered_port;
  uint32_t remembered_baud = 0;

  // --- How the player and the capture are coupled --------------------------
  //
  // One preference, and it runs in one direction only: the player may stop the
  // capture, and the capture may never stop the player.
  //
  // **Outside an automatic capture, nothing in this application sends the
  // player a command the user did not ask for.** The old application had a
  // second preference here — "stop the player when a capture stops" — and it is
  // deliberately not carried over. The automatic capture stops the player
  // itself, as a step of its own sequence, so that preference only ever acted
  // on captures taken by hand: pressing Stop capture spun the disc down, which
  // is the application taking over a piece of equipment somebody else is
  // operating. Somebody who wants the disc stopped has a Stop button on the
  // player and one in the remote.
  //
  // The surviving direction sends nothing down the cable. It watches the status
  // the player is already being polled for and stops the *capture*, which is
  // this application's own to stop.

  // Stop the capture when the player stops.
  //
  // **Off by default, and this is a considered default rather than timidity.**
  // A player that briefly reports a stopped state partway through a side —
  // which a disc with a defect will make it do — would truncate a capture that
  // was going perfectly well, and a truncated capture of a side is a side that
  // has to be captured again. When it is on, the state has to persist across
  // several readings before anything is stopped.
  bool stop_capture_with_player = false;

  bool operator==(const PlayerSettings& other) const {
    return enabled == other.enabled && model_id_code == other.model_id_code &&
           port_path == other.port_path && baud_rate == other.baud_rate &&
           excluded_ports == other.excluded_ports &&
           remembered_port == other.remembered_port &&
           remembered_baud == other.remembered_baud &&
           stop_capture_with_player == other.stop_capture_with_player;
  }
  bool operator!=(const PlayerSettings& other) const {
    return !(*this == other);
  }
};

// Is this a rate some registered player family can be found at?
//
// Asked of the registry rather than of a list here, so that a player family
// added later with a different set of rates needs no change in this file.
bool IsSupportedPlayerBaudRate(uint32_t baud_rate);

// Read the saved settings, falling back to "work it out" for anything missing
// or nonsensical.
//
// Clamping rather than rejecting, as the capture settings do: a hand-edited
// file naming a baud rate no player uses, or a model this build does not know,
// should give a working search rather than a refusal — and searching is what
// the application would have done had the setting never been written.
PlayerSettings LoadPlayerSettings();

void SavePlayerSettings(const PlayerSettings& settings);

}  // namespace ddd::gui
