/************************************************************************

    test_user_code.cpp

    T1 tests for the Pioneer User's Code and its three regions
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "response_parser.h"
#include "user_code.h"

namespace ddd::player {
namespace {

// The 60-character record an MCA *Casper* disc carries, read off the project's
// own bench. Its Disc Control Data is this twice; its Key Data came back wholly
// unreadable; its Control Data is twenty zeros.
constexpr std::string_view kRecord =
    "#59-014    *MCA / CASPER THX LTBX         !2 %0510803@@@@@@@";

std::string CasperUserCode() {
  return std::string(kRecord) + std::string(kRecord) +
         std::string(60, kUnreadableCharacter) + std::string(20, '0');
}

TEST(UserCodeTest, TheThreeRegionsAccountForTheWholeThing) {
  // 120 + 60 + 20 = 200, in disc order. The library static_asserts both, so
  // this is the readable statement of what those assertions are protecting.
  const std::span<const UserCodeRegion> regions = PioneerUserCodeRegions();
  ASSERT_EQ(regions.size(), 3U);

  EXPECT_EQ(regions[0].name, "Disc Control Data");
  EXPECT_EQ(regions[0].offset, 0U);
  EXPECT_EQ(regions[0].length, 120U);

  // Always after the Disc Control Data and before the Control Data — the order
  // is part of the format rather than a convenience of this table.
  EXPECT_EQ(regions[1].name, "Key Data");
  EXPECT_EQ(regions[1].offset, 120U);
  EXPECT_EQ(regions[1].length, 60U);

  EXPECT_EQ(regions[2].name, "Control Data");
  EXPECT_EQ(regions[2].offset, 180U);
  EXPECT_EQ(regions[2].length, 20U);

  size_t total = 0;
  for (const UserCodeRegion& region : regions) {
    total += region.length;
  }
  EXPECT_EQ(total, kPioneerUserCodeLength);
}

TEST(UserCodeTest, ARealDiscSplitsAtTheDocumentedBoundaries) {
  const std::string code = CasperUserCode();
  ASSERT_EQ(code.size(), kPioneerUserCodeLength);

  const std::vector<UserCodeRegionReading> readings = ReadPioneerUserCode(code);
  ASSERT_EQ(readings.size(), 3U);

  // The whole point of splitting: undifferentiated, this reads as "sixty of the
  // two hundred failed" and invites a guess about which sixty. Split, it says
  // the Disc Control Data is intact and the customer's own identifying data is
  // entirely unreadable — which is a fact about the disc.
  EXPECT_EQ(readings[0].characters,
            std::string(kRecord) + std::string(kRecord));
  EXPECT_EQ(readings[0].unreadable, 0U);
  EXPECT_TRUE(readings[0].complete);
  EXPECT_FALSE(readings[0].wholly_unreadable());

  EXPECT_EQ(readings[1].unreadable, 60U);
  EXPECT_TRUE(readings[1].complete);
  EXPECT_TRUE(readings[1].wholly_unreadable());

  EXPECT_EQ(readings[2].characters, std::string(20, '0'));
  EXPECT_EQ(readings[2].unreadable, 0U);
  EXPECT_TRUE(readings[2].complete);
}

TEST(UserCodeTest, UnreadableCharactersAreCountedPerRegion) {
  // A disc that lost part of one region rather than all of it, which is the
  // case a whole-reply count could not describe at all.
  std::string code(kPioneerUserCodeLength, 'A');
  code[0] = kUnreadableCharacter;
  code[125] = kUnreadableCharacter;
  code[126] = kUnreadableCharacter;

  const std::vector<UserCodeRegionReading> readings = ReadPioneerUserCode(code);

  EXPECT_EQ(readings[0].unreadable, 1U);
  EXPECT_EQ(readings[1].unreadable, 2U);
  EXPECT_EQ(readings[2].unreadable, 0U);

  // Partly unreadable is not wholly unreadable, and the two want saying
  // differently.
  EXPECT_FALSE(readings[1].wholly_unreadable());
}

TEST(UserCodeTest, AShortReplyIsReportedRatherThanPaddedOver) {
  // A player that answers short is a thing to show, not a thing to crash on —
  // and a region that is missing entirely is not the same as one that came back
  // unreadable.
  const std::vector<UserCodeRegionReading> readings =
      ReadPioneerUserCode(std::string(130, 'A'));

  ASSERT_EQ(readings.size(), 3U);

  EXPECT_TRUE(readings[0].complete);
  EXPECT_EQ(readings[0].characters.size(), 120U);

  // Ten characters of the Key Data arrived and fifty did not.
  EXPECT_FALSE(readings[1].complete);
  EXPECT_EQ(readings[1].characters.size(), 10U);
  EXPECT_FALSE(readings[1].wholly_unreadable());

  // And the Control Data is not there at all.
  EXPECT_FALSE(readings[2].complete);
  EXPECT_TRUE(readings[2].characters.empty());
  EXPECT_FALSE(readings[2].wholly_unreadable());
}

TEST(UserCodeTest, NotEncodedIsNotTheSameFactAsCouldNotBeRead) {
  // Pioneer's own worked example has a Key Data region of sixty NULs — the disc
  // simply carries none. This bench's Casper disc has sixty unreadable markers
  // there — it carries some and the player could not get at it. Counted the
  // same way the two discs would look identical, and neither reading would be
  // true.
  std::string empty_key_data(kPioneerUserCodeLength, 'A');
  empty_key_data.replace(120, 60, std::string(60, kUnencodedCharacter));

  const std::vector<UserCodeRegionReading> pioneers_example =
      ReadPioneerUserCode(empty_key_data);

  EXPECT_TRUE(pioneers_example[1].wholly_unencoded());
  EXPECT_FALSE(pioneers_example[1].wholly_unreadable());
  EXPECT_EQ(pioneers_example[1].unencoded, 60U);
  EXPECT_EQ(pioneers_example[1].unreadable, 0U);

  const std::vector<UserCodeRegionReading> casper =
      ReadPioneerUserCode(CasperUserCode());

  EXPECT_TRUE(casper[1].wholly_unreadable());
  EXPECT_FALSE(casper[1].wholly_unencoded());
}

TEST(UserCodeTest, ANulByteSurvivesBeingCarriedThrough) {
  // The manual's example is padded with NULs, so a reply genuinely contains
  // them. Everything here is length-counted rather than NUL-terminated, and
  // this is the test that says so.
  std::string code(kPioneerUserCodeLength, kUnencodedCharacter);
  code[0] = '#';

  const std::vector<UserCodeRegionReading> readings = ReadPioneerUserCode(code);

  EXPECT_EQ(readings[0].characters.size(), 120U);
  EXPECT_EQ(readings[0].unencoded, 119U);
  EXPECT_TRUE(readings[2].wholly_unencoded());
}

TEST(UserCodeTest, AnEmptyReplyIsThreeEmptyRegions) {
  const std::vector<UserCodeRegionReading> readings = ReadPioneerUserCode("");

  ASSERT_EQ(readings.size(), 3U);
  for (const UserCodeRegionReading& reading : readings) {
    EXPECT_TRUE(reading.characters.empty());
    EXPECT_FALSE(reading.complete);
    EXPECT_EQ(reading.unreadable, 0U);
  }
}

}  // namespace
}  // namespace ddd::player
