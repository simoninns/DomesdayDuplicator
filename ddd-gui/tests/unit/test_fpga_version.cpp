/************************************************************************

    test_fpga_version.cpp

    Reading the gateware's identity block
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "fpga_version.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

// An identity block as the register read returns one.
std::vector<uint8_t> MakeIdentity(uint8_t id, uint8_t map_version,
                                  uint8_t flags, const std::string& commit) {
  std::vector<uint8_t> identity(kIdentityLength, 0);
  identity[kRegisterId] = id;
  identity[kRegisterMapVersion] = map_version;
  identity[kRegisterBuildFlags] = flags;

  for (size_t index = 0; index < commit.size() && index < kCommitLength;
       ++index) {
    identity[kRegisterCommit + index] = static_cast<uint8_t>(commit[index]);
  }
  return identity;
}

std::vector<uint8_t> MakeGoodIdentity(const std::string& commit) {
  return MakeIdentity(kIdentityValue, kIdentityMapVersion, kBuildFlagCommit,
                      commit);
}

TEST(FpgaVersionTest, AWellFormedBlockNamesItsCommit) {
  const FpgaVersion version = ParseFpgaIdentity(MakeGoodIdentity("7713495d"));

  EXPECT_TRUE(version.present);
  EXPECT_EQ(version.commit, "7713495d");
  EXPECT_FALSE(version.dirty);
  EXPECT_TRUE(version.MapVersionIsKnown());
}

TEST(FpgaVersionTest, ASevenCharacterCommitKeepsItsLength) {
  // The commit is seven characters from a Nix build and eight from CMake, and
  // the register holds text precisely so that both survive. A parser that
  // padded or truncated would make two builds of one commit look different.
  const FpgaVersion version = ParseFpgaIdentity(MakeGoodIdentity("7713495"));

  EXPECT_TRUE(version.present);
  EXPECT_EQ(version.commit, "7713495");
}

TEST(FpgaVersionTest, AllZerosIsNotAGateware) {
  // What an unconfigured FPGA looks like when its MISO line is held low. The
  // signature is the only thing that separates it from a real answer, because
  // SPI gives no acknowledgement to fail on.
  const std::vector<uint8_t> identity(kIdentityLength, 0x00);
  const FpgaVersion version = ParseFpgaIdentity(identity);

  EXPECT_FALSE(version.present);
  EXPECT_TRUE(version.commit.empty());
  EXPECT_FALSE(version.MapVersionIsKnown());
}

TEST(FpgaVersionTest, AllOnesIsNotAGateware) {
  // And what it looks like when the line floats instead.
  const std::vector<uint8_t> identity(kIdentityLength, 0xFF);
  const FpgaVersion version = ParseFpgaIdentity(identity);

  EXPECT_FALSE(version.present);
  EXPECT_TRUE(version.commit.empty());
}

TEST(FpgaVersionTest, AShortBlockIsRejected) {
  // A truncated read is not a partial answer to be salvaged; it is no answer.
  std::vector<uint8_t> identity = MakeGoodIdentity("7713495d");
  identity.pop_back();

  EXPECT_FALSE(ParseFpgaIdentity(identity).present);
  EXPECT_FALSE(ParseFpgaIdentity({}).present);
}

TEST(FpgaVersionTest, TheCommitFlagIsWhatMakesTheCharactersACommit) {
  // Positive logic on purpose: every way of not knowing reads as no commit,
  // rather than as a confident claim about whatever the bytes happened to be.
  const std::vector<uint8_t> identity =
      MakeIdentity(kIdentityValue, kIdentityMapVersion, 0, "7713495d");
  const FpgaVersion version = ParseFpgaIdentity(identity);

  EXPECT_TRUE(version.present);
  EXPECT_TRUE(version.commit.empty());
}

TEST(FpgaVersionTest, ADirtyBuildStillNamesItsCommit) {
  // The hash is still the best available description of what was built. It is
  // simply not the whole of it, which is what the flag says.
  const std::vector<uint8_t> identity =
      MakeIdentity(kIdentityValue, kIdentityMapVersion,
                   kBuildFlagCommit | kBuildFlagDirty, "7713495d");
  const FpgaVersion version = ParseFpgaIdentity(identity);

  EXPECT_TRUE(version.present);
  EXPECT_EQ(version.commit, "7713495d");
  EXPECT_TRUE(version.dirty);
}

TEST(FpgaVersionTest, NonHexCharactersEndTheCommit) {
  // A misread link must not be able to put arbitrary bytes into a dialog.
  const FpgaVersion version = ParseFpgaIdentity(MakeGoodIdentity("77ZZ4567"));

  EXPECT_TRUE(version.present);
  EXPECT_EQ(version.commit, "77");
}

TEST(FpgaVersionTest, AnUnknownMapVersionIsStillAGateware) {
  // Gateware newer than this build is reported rather than discarded: the
  // identity block is frozen across map versions precisely so that it can be.
  const std::vector<uint8_t> identity = MakeIdentity(
      kIdentityValue, kIdentityMapVersion + 1, kBuildFlagCommit, "7713495d");
  const FpgaVersion version = ParseFpgaIdentity(identity);

  EXPECT_TRUE(version.present);
  EXPECT_EQ(version.commit, "7713495d");
  EXPECT_FALSE(version.MapVersionIsKnown());
}

}  // namespace
}  // namespace ddd::capture
