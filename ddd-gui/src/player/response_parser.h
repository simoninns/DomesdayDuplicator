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

  // What went on the wire, terminator included. Empty where nothing did: a
  // command the model does not have, or one that could not be encoded.
  //
  // Carried on the reply because this is the only place the bytes exist, and a
  // serial trace showing what was sent beside what came back is what makes a
  // misbehaving player diagnosable by somebody who does not have it in front of
  // them. It is also what the remote's manual command field echoes.
  std::string sent;

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
//
// Only the terminator comes off, and that is deliberate. The Pioneer user code
// is a fixed-width record whose fields are space-padded, so the whitespace
// trimming StripTerminator does — right for a reply that is about to be parsed
// as a number or a state code — would quietly eat payload here. Everything that
// does parse a reply strips for itself, so nothing downstream depends on this
// having done it.
Reply ParseText(std::string_view raw);

// The character a Pioneer player sends in place of one it could not read off
// the disc.
//
// Documented in the LD-V4400 Level I & III manual: "If the player experiences
// an error in reading the data an '`' (60 HEX) character is returned." It is
// per-character, so a user-code reply can come back as a long run of these —
// on the project's own bench, sixty of them in the middle of a 200-byte reply.
// Worth naming, because it is the difference between "this application cannot
// decode the reply" and "the player could not read the disc".
inline constexpr char kUnreadableCharacter = '`';

// Is this text reply the player's error code rather than an answer?
//
// The counterpart to the leniency above, and it exists because of a real
// reading: an LD-V4300D on the bench answered the Pioneer user-code query with
// "E04" while parked. Put through the acknowledgement convention that would
// have been a refusal; taken as text it would have been shown to the user as
// their disc's user code, which is worse.
//
// Deliberately much stricter than the acknowledgement convention — the whole
// reply must be 'E' followed by digits and nothing else — because the reason
// text replies avoid that convention in the first place is that a user code may
// legitimately contain an 'E'.
bool IsErrorCode(std::string_view text);

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

// What the disc itself says it is.
//
// The programme status, read out of the lead-in by the player and handed over
// in one exchange. Every field is optional and independently so, because the
// player answers 'X' for a field it could not determine — which is a third
// answer, and folding it into "no" would turn "I could not tell which side
// this is" into "side 1".
struct DiscStatus {
  // The reply was long enough to read the fields this model's decode names.
  // False for a reply of the wrong shape, which is worth showing rather than
  // reading whatever characters happened to be there.
  bool valid = false;

  // Is there a disc the player has managed to read? Absent where the model does
  // not report it.
  std::optional<bool> loaded;

  DiscType type = DiscType::kUnknown;
  DiscSize size = DiscSize::kUnknown;

  // 1 or 2. Absent where the player could not tell, which it says explicitly.
  std::optional<int> side;

  std::optional<bool> chapters;
};

// Read a disc-status reply through a model's decode.
DiscStatus ParseDiscStatus(std::string_view raw,
                           const DiscStatusDecode& decode);

// Just the type, for the status poll — which asks this several times a minute
// and has no use for the rest.
DiscType ParseDiscType(std::string_view raw, const DiscStatusDecode& decode);

// Which television standard is on the disc, and which is coming out.
//
// The two are separate fields because on a player that converts they are
// different answers, and it is the disc's that a capture is of. See
// TvSystemDecode for the reply's layout and where each value came from.
struct TvSystem {
  bool valid = false;

  // What the disc carries. The answer this exists for.
  VideoStandard disc = VideoStandard::kUnknown;

  // What the player is putting out.
  VideoStandard output = VideoStandard::kUnknown;

  // The external sync generator's standard, or kUnknown where none is
  // connected — which is what the reply says rather than a separate flag.
  VideoStandard external_sync = VideoStandard::kUnknown;

  bool sync_connected() const {
    return external_sync != VideoStandard::kUnknown;
  }
};

TvSystem ParseTvSystem(std::string_view raw, const TvSystemDecode& decode);

// Read a physical-position reply, in millimetres.
//
// The player reports the slider position in units of 10 micrometres as
// hexadecimal, and — because the processor doing the reporting is little-endian
// — with the two bytes the other way round. Both are undone here.
std::optional<float> ParsePhysicalPositionMillimetres(std::string_view raw);

}  // namespace ddd::player
