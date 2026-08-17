/************************************************************************

    player_session.h

    Finding a player, identifying it, and talking to it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "player_command.h"
#include "player_definition.h"
#include "response_parser.h"
#include "serial_port.h"

namespace ddd::player {

// Which player answered.
struct PlayerIdentity {
  // The whole identifying part of the reply — "P1506A9" — kept because it is
  // what a user should be shown when nothing recognises it, and what they will
  // be asked for when a new definition is being written.
  std::string model_code;

  std::string id_code;
  std::string firmware_version;

  // Never null once a probe has succeeded: either the model's definition or the
  // generic one.
  const PlayerDefinition* definition = nullptr;

  // False when the player identified itself correctly but with a model ID no
  // definition claims. The application says so; some controls may not work, and
  // a user who knows that can work around it.
  bool recognised = false;

  bool valid() const { return definition != nullptr; }
};

struct ProbeResult {
  enum class Status : uint8_t {
    // A player answered and was identified. The session is now connected.
    kConnected,

    // The port could not be opened at all — busy, absent, or not permitted.
    kPortUnavailable,

    // The port opened and nothing answered at any rate tried.
    kNoAnswer,

    // Something answered, at some rate, in a shape no probe recognises. Worth
    // separating from silence: it usually means a serial device that is not a
    // player, and telling a user that is more use than "not found".
    kUnusableAnswer,
  };

  Status status = Status::kNoAnswer;
  PlayerIdentity identity;

  // Why no port would open, for kPortUnavailable. The most specific cause seen
  // across every port and rate tried, which on a scan is the only one worth
  // reporting: half a dozen absent ports and one that was refused is a
  // permission problem, not six missing ones.
  PortOpenError open_error = PortOpenError::kNone;

  // The rate the player answered at, or the rate the unusable answer came from.
  uint32_t baud_rate = 0;

  // What arrived, for kUnusableAnswer.
  std::string unexpected_reply;

  bool connected() const { return status == Status::kConnected; }
};

// One conversation with one player.
//
// Owns no port and no thread: it is handed an ISerialPort and driven by
// whatever is above it, which in the application is a worker thread and in a
// test is the test itself. Every method blocks for as long as the command's
// timeout allows, which is exactly why nothing above this may run on a GUI
// thread.
//
// Thread-safety: none, deliberately. One thread drives one session.
class PlayerSession {
 public:
  // Where "now" comes from.
  //
  // Injectable so that the timeout logic is genuinely tested rather than waited
  // out: a test's fake port advances this clock by the timeout it was asked to
  // wait, so a command that times out does so in microseconds and does so
  // deterministically. Defaults to the steady clock.
  using Clock = std::function<std::chrono::steady_clock::time_point()>;

  explicit PlayerSession(ISerialPort* port, Clock clock = {});

  // Find and identify a player on `path`.
  //
  // With no rate given, every rate the probe lists is tried in turn with a
  // short timeout and a few attempts each — most of the rates tried are wrong,
  // and being patient at each would be paid for four times over. With a rate
  // given, only that rate is tried, once, with a long timeout: at a known rate
  // a slow answer is still an answer.
  //
  // Any previous connection is dropped first. On success the port is left open
  // and the session is connected.
  ProbeResult Probe(const std::string& path,
                    std::optional<uint32_t> baud_rate = std::nullopt);

  // Close the port and forget the player. Safe to call when not connected.
  void Disconnect();

  bool connected() const { return connected_; }
  const PlayerIdentity& identity() const { return identity_; }
  const std::string& port_path() const { return port_path_; }
  uint32_t baud_rate() const { return baud_rate_; }

  // Does the connected player, running the firmware it reported, report a
  // physical position?
  bool SupportsPhysicalPosition() const;

  // Send one command and read its reply.
  //
  // One path for every command, so timeouts, the error convention and link
  // failure are handled identically for all of them — the old application
  // repeated that handling per command and could not guarantee it.
  Reply Execute(PlayerCommand command,
                std::optional<int32_t> argument = std::nullopt);

  Reply SetAudio(AudioMode mode);
  Reply SetSpeed(PlaybackSpeed speed);

  // Send something the schema does not describe.
  //
  // The manual command field in the remote, and the only way to find out what
  // an unrecognised player does — which is how the next definition header gets
  // written. `expected_replies` is for the commands that answer more than once.
  Reply SendRaw(std::string_view command, ResponseKind response,
                TimeoutClass timeout, int expected_replies = 1);

 private:
  // Write, wait, parse. Assumes the bytes are complete and terminated.
  Reply Send(std::string_view bytes, ResponseKind response,
             TimeoutClass timeout, int expected_replies);

  // Read until `expected_replies` terminators have arrived or the timeout
  // elapses. Returns false only when the port failed; a timeout leaves `into`
  // empty, because a partial reply parsed is a wrong answer rather than a
  // partial one.
  bool ReadReply(std::string& into, std::chrono::milliseconds timeout,
                 int expected_replies);

  // Match a reply against a probe and work out which model it is.
  static bool Identify(const ProbeSpec& spec, std::string_view text,
                       PlayerIdentity& identity);

  ISerialPort* port_ = nullptr;
  Clock clock_;

  bool connected_ = false;
  PlayerIdentity identity_;
  std::string port_path_;
  uint32_t baud_rate_ = 0;
};

}  // namespace ddd::player
