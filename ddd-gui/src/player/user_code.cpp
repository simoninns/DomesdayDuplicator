/************************************************************************

    user_code.cpp

    The Pioneer User's Code, and the three regions it is made of
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "user_code.h"

#include <algorithm>
#include <array>

#include "response_parser.h"

namespace ddd::player {
namespace {

// 120 + 60 + 20 = 200, which is the whole of it. The static_assert below is
// what keeps that true if anybody edits this table.
constexpr std::array<UserCodeRegion, 3> kRegions{{
    {"Disc Control Data", 0, 120},
    {"Key Data", 120, 60},
    {"Control Data", 180, 20},
}};

constexpr size_t TotalLength() {
  size_t total = 0;
  for (const UserCodeRegion& region : kRegions) {
    total += region.length;
  }
  return total;
}

static_assert(TotalLength() == kPioneerUserCodeLength,
              "the regions must account for the whole user code");

constexpr bool RegionsAreContiguous() {
  size_t expected = 0;
  for (const UserCodeRegion& region : kRegions) {
    if (region.offset != expected) {
      return false;
    }
    expected += region.length;
  }
  return true;
}

// The order is part of the format — the Key Data is always after the Disc
// Control Data and before the Control Data — so a table that had drifted out of
// order would be read at the wrong offsets rather than failing visibly.
static_assert(RegionsAreContiguous(),
              "the regions must run end to end in disc order");

}  // namespace

std::span<const UserCodeRegion> PioneerUserCodeRegions() { return kRegions; }

std::vector<UserCodeRegionReading> ReadPioneerUserCode(std::string_view reply) {
  std::vector<UserCodeRegionReading> readings;
  readings.reserve(kRegions.size());

  for (const UserCodeRegion& region : kRegions) {
    UserCodeRegionReading reading;
    reading.region = region;

    if (region.offset < reply.size()) {
      reading.characters = reply.substr(
          region.offset, std::min(region.length, reply.size() - region.offset));
    }

    reading.complete = reading.characters.size() == region.length;

    reading.unreadable = static_cast<size_t>(
        std::count(reading.characters.begin(), reading.characters.end(),
                   kUnreadableCharacter));

    reading.unencoded = static_cast<size_t>(
        std::count(reading.characters.begin(), reading.characters.end(),
                   kUnencodedCharacter));

    readings.push_back(reading);
  }

  return readings;
}

}  // namespace ddd::player
