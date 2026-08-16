/************************************************************************

    response_parser.h

    What came back from the player, turned into something typed
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "player_definition.h"
#include "player_state.h"

namespace ddd::player {

// What a reply amounted to.
//
// Four outcomes where the old application had two. It returned -1 both for "the
// player did not answer" and for "the player answered with something I could
// not read", and false both for "refused" and "timed out". Those want different
// messages and, in the automatic capture, different responses: a refusal means
// the disc or the state is wrong, and silence means the link is.
enum class ReplyStatus : uint8_t {
  kOk,

  // The player answered and said no.
  kRefused,

  // Nothing arrived before the deadline.
  kNoAnswer,

  // Something arrived that this cannot make sense of.
  kUnparseable,

  // The port failed underneath the exchange.
  kLinkFailed,

  // Asked of a session that is not connected.
  kNotConnected,

  // The connected model has no sequence for the command, or no parameter for
  // the mode asked for.
  kUnsupported,

  // The command could not be built from the argument given.
  kInvalidArgument,
};

struct Reply {
  ReplyStatus status = ReplyStatus::kNoAnswer;

  // The reply with its terminator removed. Empty unless something arrived.
  std::string text;

  // The player's error code — "E04" and the like — when the reply was a refusal
  // and carried a legible one.
  std::string error_code;

  bool ok() const { return status == ReplyStatus::kOk; }
};

// Everything below takes the raw reply, terminator and all.

// How many complete replies are in this buffer.
size_t CountTerminators(std::string_view raw);

// The reply without its terminator, and without leading whitespace left over
// from a previous exchange.
std::string StripTerminator(std::string_view raw);

// Read an acknowledgement.
//
// The convention is the old application's, unchanged and deliberately lenient:
// any reply containing 'E' is a refusal and anything else that arrived is
// success. Requiring the documented "R" instead would be stricter than the
// players are — several answer commands with nothing resembling it — and this
// port is not the place to find out which.
Reply ParseAcknowledgement(std::string_view raw);

// Read a reply whose content is the answer, rather than an acknowledgement of a
// command. Not put through the error convention above: a user code is arbitrary
// bytes and may perfectly well contain an 'E'.
Reply ParseText(std::string_view raw);

// Where the player says it is.
struct DiscAddress {
  bool valid = false;

  // Frame number or time code, depending on what was asked for.
  int32_t value = -1;

  // The player is before the start of the programme.
  //
  // Carried rather than stripped and forgotten, because capturing from the
  // lead-in is a thing the automatic capture does and knowing the player has
  // reached it is how that works.
  bool in_lead_in = false;

  // The player is past the end of the programme — which is how the end of a
  // side is recognised.
  bool in_lead_out = false;
};

// Read an address reply in the given mode.
//
// The mode is passed in rather than guessed. A CLV time code read as a frame
// number is a plausible-looking number that is wrong by orders of magnitude,
// and the old application would silently take the first five digits of one and
// report it as a frame; here a reply with more digits than the mode allows is
// unparseable instead.
DiscAddress ParseAddress(std::string_view raw, AddressMode mode);

// Read an active-mode reply through a model's state table.
PlayerState ParsePlayerState(std::string_view raw, const StateDecode& decode);

// Read a disc-status reply through a model's decode.
DiscType ParseDiscType(std::string_view raw, const DiscStatusDecode& decode);

// Read a physical-position reply, in millimetres.
//
// The player reports the slider position in units of 10 micrometres as
// hexadecimal, and — because the processor doing the reporting is little-endian
// — with the two bytes the other way round. Both are undone here.
std::optional<float> ParsePhysicalPositionMillimetres(std::string_view raw);

}  // namespace ddd::player
