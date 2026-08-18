/************************************************************************

    test_digest.cpp

    SHA-256 against the published vectors
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "digest.h"

namespace ddd::capture {
namespace {

// The digest is the spine of the whole update chain, and it is vendored code
// (src/vendor/VENDOR.md). These vectors are what makes a vendor refresh a
// checkable act rather than a hopeful one: they come from FIPS 180-2's own
// examples and from the widely published million-a case, so they are agreed on
// by everyone and by nothing in this repository.

TEST(Digest, MatchesThePublishedVectorForTheEmptyInput) {
  EXPECT_EQ(ToHex(Sha256(std::string_view(""))),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Digest, MatchesThePublishedVectorForAbc) {
  EXPECT_EQ(ToHex(Sha256(std::string_view("abc"))),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Digest, MatchesThePublishedVectorForTwoBlocks) {
  EXPECT_EQ(ToHex(Sha256(std::string_view(
                "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"))),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(Digest, MatchesThePublishedVectorForAMillionCharacters) {
  const std::string input(1000000, 'a');
  EXPECT_EQ(ToHex(Sha256(std::string_view(input))),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

// The streaming interface is the one the update flow uses, because a payload is
// hashed as it is transferred rather than held twice in memory. It has to agree
// with the one-shot function whatever the chunk boundaries are, including
// boundaries that fall inside the algorithm's own 64-byte block.
TEST(Digest, StreamingAgreesWithOneShotWhateverTheChunking) {
  std::vector<uint8_t> data(1000);
  for (size_t index = 0; index < data.size(); ++index) {
    data[index] = static_cast<uint8_t>(index * 7);
  }
  const Sha256Digest expected = Sha256(data);

  for (size_t chunk : {size_t{1}, size_t{7}, size_t{63}, size_t{64}, size_t{65},
                       size_t{999}, size_t{1000}}) {
    Sha256Hasher hasher;
    for (size_t offset = 0; offset < data.size(); offset += chunk) {
      const size_t length = std::min(chunk, data.size() - offset);
      hasher.Update(std::span<const uint8_t>(data).subspan(offset, length));
    }
    EXPECT_EQ(hasher.Finish(), expected) << "chunk size " << chunk;
  }
}

TEST(Digest, StreamingNothingGivesTheEmptyDigest) {
  Sha256Hasher hasher;
  EXPECT_EQ(ToHex(hasher.Finish()),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Digest, HexRoundTrips) {
  const Sha256Digest digest = Sha256(std::string_view("abc"));
  const std::optional<Sha256Digest> parsed = ParseHexDigest(ToHex(digest));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed.value_or(Sha256Digest{}), digest);
}

TEST(Digest, HexParsingAcceptsUpperCase) {
  EXPECT_EQ(ParseHexDigest(
                "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015"
                "AD"),
            ParseHexDigest(
                "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015"
                "ad"));
}

// A manifest digest that is the wrong length or carries a stray character is a
// manifest that was edited by hand or damaged. Neither is interpreted
// charitably, because both have the same answer: this bundle is not usable.
TEST(Digest, HexParsingRefusesAnythingButSixtyFourHexCharacters) {
  EXPECT_FALSE(ParseHexDigest("").has_value());
  EXPECT_FALSE(ParseHexDigest("ba7816bf").has_value());
  EXPECT_FALSE(
      ParseHexDigest(
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad0")
          .has_value());
  EXPECT_FALSE(
      ParseHexDigest(
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015a ")
          .has_value());
  EXPECT_FALSE(
      ParseHexDigest(
          "ga7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
          .has_value());
}

}  // namespace
}  // namespace ddd::capture
