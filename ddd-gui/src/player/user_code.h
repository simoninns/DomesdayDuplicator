/************************************************************************

    user_code.h

    The Pioneer User's Code, and the three regions it is made of
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace ddd::player {

// The Pioneer User's Code is a Pioneer standard, encoded in the last 100 frames
// (200 fields) of the lead-in per IEC specifications. It is 200 characters —
// one character per field — and it is not one blob: it is three regions of
// fixed size, always in this order.
//
//   Disc Control Data   60 frames (120 fields)   120 characters
//   Key Data            30 frames  (60 fields)    60 characters
//   Control Data        10 frames  (20 fields)    20 characters
//
// The Key Data carries up to 60 characters of disc-identifying information
// specified by the customer, encoded during mastering, and it always sits after
// the Disc Control Data and before the Control Data.
//
// Two characters have meanings of their own, and telling them apart is the
// difference between "this disc does not carry that" and "this player could not
// read it":
//
//   0x60  '`'   the player tried and failed — see kUnreadableCharacter
//   0x00  NUL         nothing was encoded there
//
// Pioneer's own worked example has a Key Data region of sixty NULs, meaning the
// disc simply carries none. The Casper disc on this project's bench has a Key
// Data region of sixty unreadable markers, meaning it carries some and the
// player could not get at it. Reported the same way, those two discs would look
// identical and neither reading would be true.
//
// Knowing the boundaries is what turns the reply from a wall of characters into
// three answers. On the project's own bench an MCA *Casper* disc read its Disc
// Control Data perfectly and returned all sixty Key Data characters as the
// unreadable marker — which, without the regions, looks like "a third of the
// reply failed" rather than the much more specific "the customer's identifying
// data could not be read".
//
// Pioneer-specific, like the error-code and unreadable-character conventions in
// response_parser.h, and kept out of PlayerDefinition for the same reason: it
// is a property of the format on the disc rather than of any one model.

inline constexpr size_t kPioneerUserCodeLength = 200;

struct UserCodeRegion {
  std::string_view name;
  size_t offset = 0;
  size_t length = 0;
};

// The three regions, in the order they appear on the disc.
std::span<const UserCodeRegion> PioneerUserCodeRegions();

// One region as it came back.
struct UserCodeRegionReading {
  UserCodeRegion region;

  // The characters themselves. A view into the reply that was read, so it lives
  // exactly as long as that does.
  std::string_view characters;

  // How many of them the player could not read off the disc.
  size_t unreadable = 0;

  // How many of them were never encoded — NUL, which is what Pioneer's own
  // example shows an empty Key Data region filled with.
  size_t unencoded = 0;

  // Was the whole region there? False for a reply shorter than the format says,
  // which is worth showing rather than padding over.
  bool complete = false;

  // Nothing in this region could be read. Distinct from an empty region: it
  // means the player tried and failed rather than that there was nothing there.
  bool wholly_unreadable() const {
    return !characters.empty() && unreadable == characters.size();
  }

  // Nothing was encoded in this region at all.
  bool wholly_unencoded() const {
    return !characters.empty() && unencoded == characters.size();
  }
};

// The character standing for a field that was never encoded.
inline constexpr char kUnencodedCharacter = '\0';

// Split a user-code reply into its regions.
//
// Tolerant of a reply that is not the full 200 characters: each region reports
// what was actually there and whether that was all of it. A player that answers
// short is a thing to be shown, not a thing to crash on.
std::vector<UserCodeRegionReading> ReadPioneerUserCode(std::string_view reply);

}  // namespace ddd::player
