/************************************************************************

    disc_profile.h

    What an examination of the disc found, and how it found each part
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "player_state.h"

namespace ddd::player {

// Where a fact about the disc came from.
//
// Carried on every field rather than kept in the report's wording, and that is
// the point of this type existing. The old application presented what the
// player claimed and what it had worked out for itself in the same typeface, so
// a disc length that came from seeking to the end and a disc length that came
// from a decode nobody had ever checked looked identical — and when one of them
// was wrong there was no way to tell which.
enum class Provenance : uint8_t {
  // Not established. Distinct from a zero or an empty string, both of which are
  // real answers to some of the questions below.
  kUnknown,

  // The player said so, in answer to a query.
  kReported,

  // Established by driving the player and reading the result. The disc length
  // is the one that matters: it is found by seeking past the end of the side
  // and asking where that landed.
  kMeasured,

  // Derived from another fact here — the addressing follows from the disc type
  // and nothing has to be asked for it.
  kInferred,

  // The user said so. Nothing in the examination produces this; it is for the
  // fields a model cannot be asked about, which the capture setup collects.
  kDeclared,
};

// One field of the profile, with where it came from.
template <typename T>
struct Fact {
  T value{};
  Provenance provenance = Provenance::kUnknown;

  bool known() const { return provenance != Provenance::kUnknown; }

  // Record a value and where it came from.
  //
  // A named call rather than two assignments, so that a step which learns
  // something cannot record it and leave the provenance saying it never did.
  void Record(T new_value, Provenance from) {
    value = std::move(new_value);
    provenance = from;
  }

  bool operator==(const Fact&) const = default;
};

// A user code as it came back.
//
// Four outcomes rather than a string that may be empty, because the bench found
// three of them on five discs and they are not the same answer. A disc that
// carries no user code, a disc whose user code could not be read, and a disc
// nobody asked about all produce nothing to show, and reporting them alike
// would be reporting the absence of evidence as evidence of absence.
struct UserCodeReading {
  enum class Outcome : uint8_t {
    // Never asked for. The Pioneer read is optional and off by default.
    kNotRead,

    // The player returned a code.
    kRead,

    // The player answered with an error code, which its manual documents as no
    // user code being encoded on the disc. Two of the five discs on this
    // project's bench answer this way.
    kNotEncoded,

    // Asked, and no usable answer came back.
    kRefused,
  };

  Outcome outcome = Outcome::kNotRead;

  // The code itself, or the error code the player sent instead. Kept verbatim,
  // including the characters the player could not read — see user_code.h, where
  // telling those apart from characters that were never encoded is the whole
  // job.
  std::string text;

  bool read() const { return outcome == Outcome::kRead; }

  bool operator==(const UserCodeReading&) const = default;
};

// Everything an examination established about the disc in the player.
//
// **Nothing here is required to be known.** A player that refuses half of these
// queries yields a profile with the other half in it, and the capture setup
// asks about what is missing. The old application's equivalent gave up at the
// first refusal and produced nothing at all, which is why a disc that would not
// report its status could not be captured automatically even though its length
// was perfectly measurable.
//
// Qt-free like everything else in this library, so the whole examination can be
// driven and compared in a test with no player and no event loop.
struct DiscProfile {
  // Is there a disc, and is the tray shut? The first questions, because every
  // other one is meaningless without them.
  Fact<bool> disc_present;
  Fact<TrayState> tray;

  Fact<DiscType> disc_type;

  // Follows from the disc type and is never asked for separately.
  Fact<AddressMode> addressing;

  // Which disc, and which of its two sides.
  //
  // Both come from the disc's own programme status, read out of the lead-in by
  // the player and handed over in the disc-status reply — so they cost nothing
  // and move nothing. The side is the one worth having: a capture of side 2 and
  // a capture of side 1 are different files, and until now the application had
  // no way of knowing which it was making.
  Fact<DiscSize> disc_size;
  Fact<int> disc_side;

  // The first and last addresses of the programme, in whichever way this disc
  // is addressed — frame numbers on a CAV disc, time codes on a CLV one.
  //
  // Both measured, by seeking to an address the disc cannot have and reading
  // back where the player actually stopped. That is the old application's
  // technique, kept because it measures the thing itself rather than trusting
  // anything the disc claims about its own length.
  Fact<int32_t> programme_start;
  Fact<int32_t> programme_end;

  // Did seeking to the start of the disc land in the lead-in? Capturing from
  // the lead-in is a thing the automatic capture offers, and this is whether
  // this disc and this player can be asked to do it.
  Fact<bool> lead_in_reachable;

  // Does the disc have chapters? The disc says so itself, in the same reply as
  // the type and the side.
  Fact<bool> chapters;

  // **Informational only, and this is a hard constraint rather than a
  // preference.** Nothing derives a length, a start or an end from a user code.
  // The Pioneer code's Disc Control Data appears to carry the side number and
  // the side's playing time, which makes it tempting; it is not to be used,
  // because it is absent on some perfectly healthy discs, its field meanings
  // are inferred rather than documented, and reading it costs eleven seconds
  // and the player's position. Five discs on the bench settled it.
  UserCodeReading standard_user_code;
  UserCodeReading pioneer_user_code;

  // Which television standard the disc carries.
  //
  // Asked for, in the one command that answers it — see TvSystemDecode. It is
  // not in the disc status, and the model does not imply it: this project's own
  // LD-V4300D is dual-format. A model that cannot be asked leaves this unknown
  // and the capture setup asks the user, which is where kDeclared comes from.
  Fact<VideoStandard> video_standard;

  // The disc-status reply exactly as it arrived.
  //
  // Every documented character of it is now decoded into the fields above, so
  // this is no longer the place the information lives — it is the working. A
  // report that says "side 2" and shows the `11011` it read that from is one
  // somebody can check; a report that only says "side 2" has to be believed.
  std::string disc_status_reply;

  bool operator==(const DiscProfile&) const = default;
};

// How long the programme runs, where that can be worked out.
//
// Direct for a CLV disc, whose addresses are times already. For a CAV disc the
// addresses are frames and turning frames into a duration needs the frame rate,
// which needs the video standard — so an unexamined CAV disc gives nothing here
// until somebody says which standard it is. That is not a gap to paper over
// with an assumed 30 frames a second: on a PAL disc that assumption is twenty
// per cent out, and twenty per cent out is a capture that runs off the end of
// the volume it was estimated to fit on.
std::optional<std::chrono::seconds> ProgrammeDuration(const DiscProfile& disc);

// How long it takes to play from one address to another on a disc of this type
// carrying this standard.
//
// The general form of the above, which is this applied to the two ends of the
// programme — and what the capture setup needs, since a capture of part of a
// side wants its own estimate rather than the whole side's. Nothing where the
// span is empty or backwards, and nothing for a CAV span whose standard is
// unknown, for the reason given above.
std::optional<std::chrono::seconds> AddressSpanDuration(int32_t start,
                                                        int32_t end,
                                                        DiscType type,
                                                        VideoStandard standard);

// Frames a second, for a disc carrying this standard.
//
// The same rate for CAV and CLV — the difference between them is how many
// frames go round once, not how many go past a second — so the disc type is not
// asked for. Nothing for a standard that has not been established, which is the
// usual case after an examination.
std::optional<double> FrameRate(VideoStandard standard);

}  // namespace ddd::player
