/************************************************************************

    test_player_registry.cpp

    T1 tests sweeping every registered player definition
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "player_definition.h"
#include "player_registry.h"
#include "players/pioneer_ld_v4300d.h"
#include "players/pioneer_ld_v8000.h"

namespace ddd::player {
namespace {

// The models this build claims to support, and what each answers with.
//
// A table rather than assertions inside a loop, because it is also the thing a
// reviewer reads to check that a new player was added correctly: adding a
// definition without adding a row here fails the count assertion below, which
// is the point.
struct ExpectedPlayer {
  std::string_view id_code;
  std::string_view name;
};

constexpr ExpectedPlayer kExpected[] = {
    {"07", "Pioneer LD-V2200"},  {"02", "Pioneer LD-V4200"},
    {"15", "Pioneer LD-V4300D"}, {"16", "Pioneer LD-V4400"},
    {"06", "Pioneer LD-V8000"},  {"18", "Pioneer CLD-V2400"},
    {"27", "Pioneer CLD-V2600"}, {"37", "Pioneer CLD-V2800"},
    {"42", "Pioneer CLD-V5000"}, {"05", "Pioneer VC-V330"},
};

TEST(PlayerRegistryTest, EveryExpectedModelIsRegisteredAndNothingElseIs) {
  ASSERT_EQ(RegisteredPlayers().size(), std::size(kExpected));

  for (const ExpectedPlayer& expected : kExpected) {
    const PlayerDefinition* definition = FindPlayerByIdCode(expected.id_code);
    ASSERT_NE(definition, nullptr)
        << "no definition claims " << expected.id_code;
    EXPECT_EQ(definition->name, expected.name);
  }
}

TEST(PlayerRegistryTest, ModelIdsAndNamesAreUnique) {
  // Two definitions claiming one model ID would make which one a player
  // resolved to depend on the order of the table, which is a bug nobody would
  // find by reading either header.
  std::set<std::string_view> id_codes;
  std::set<std::string_view> names;

  for (const PlayerDefinition* definition : RegisteredPlayers()) {
    EXPECT_TRUE(id_codes.insert(definition->id_code).second)
        << "duplicate model ID " << definition->id_code;
    EXPECT_TRUE(names.insert(definition->name).second)
        << "duplicate name " << definition->name;
  }
}

TEST(PlayerRegistryTest, EveryDefinitionIsInternallyConsistent) {
  // Also asserted at compile time in player_registry.cpp. Repeated here so the
  // failure names the definition rather than the static_assert line, and so the
  // rule is visible to somebody reading the tests to find out what a definition
  // has to satisfy.
  for (const PlayerDefinition* definition : RegisteredPlayers()) {
    EXPECT_TRUE(IsConsistent(*definition)) << definition->name;
  }

  EXPECT_TRUE(IsConsistent(GenericPlayer()));
}

TEST(PlayerRegistryTest, EveryDefinitionHasAProbeTheRegistryKnowsAbout) {
  const std::span<const ProbeSpec* const> probes = RegisteredProbes();
  ASSERT_FALSE(probes.empty());

  for (const PlayerDefinition* definition : RegisteredPlayers()) {
    ASSERT_NE(definition->probe, nullptr) << definition->name;

    // A definition pointing at a probe the session never iterates is a player
    // that can be selected and can never be found.
    EXPECT_NE(std::find(probes.begin(), probes.end(), definition->probe),
              probes.end())
        << definition->name;
  }
}

TEST(PlayerRegistryTest, EveryProbeCanIdentifyWhatItAsksFor) {
  for (const ProbeSpec* probe : RegisteredProbes()) {
    EXPECT_FALSE(probe->request.empty());
    EXPECT_FALSE(probe->reply_prefix.empty());
    EXPECT_FALSE(probe->baud_rates.empty());

    // The model ID has to sit after the prefix, or the prefix check and the ID
    // extraction would be reading the same bytes.
    EXPECT_GE(probe->id_code_offset, probe->reply_prefix.size());
    EXPECT_GT(probe->id_code_length, 0U);
    EXPECT_GE(probe->firmware_offset,
              probe->id_code_offset + probe->id_code_length);

    EXPECT_GT(probe->search_attempts, 0);
    EXPECT_GT(probe->fixed_attempts, 0);

    // Searching is meant to be cheap per rate and patient once the rate is
    // known. A search timeout that exceeded the fixed one would make finding a
    // player slower than talking to a known one.
    EXPECT_LE(probe->search_timeout, probe->fixed_timeout);
  }
}

TEST(PlayerRegistryTest, AnUnclaimedModelIdResolvesToNothing) {
  EXPECT_EQ(FindPlayerByIdCode("99"), nullptr);

  // Not to the generic definition, which carries an empty ID. A player that
  // answered with no ID at all has not been identified, and the caller has to
  // be able to tell that from a player that was.
  EXPECT_EQ(FindPlayerByIdCode(""), nullptr);
}

TEST(PlayerRegistryTest, TheGenericPlayerIsUsableButDoesNotClaimToBeAModel) {
  const PlayerDefinition& generic = GenericPlayer();

  EXPECT_TRUE(generic.is_generic);
  EXPECT_TRUE(generic.id_code.empty());
  EXPECT_FALSE(generic.bench_verified);

  // It still has the whole command set, because a usable guess is the point of
  // it: an unrecognised player can still be driven.
  EXPECT_TRUE(Spec(generic, PlayerCommand::kPlay).present());
  EXPECT_TRUE(Spec(generic, PlayerCommand::kQueryAddress).present());
}

TEST(PlayerRegistryTest, NoDefinitionClaimsEvidenceItDoesNotHave) {
  // bench_verified is a claim that somebody walked players/README.md's
  // checklist with the hardware. Nothing in this repository has yet, and the
  // interface tells users so; this pins that the claim is not made by accident
  // — a definition copied from a verified one would otherwise inherit it.
  for (const PlayerDefinition* definition : RegisteredPlayers()) {
    EXPECT_FALSE(definition->bench_verified)
        << definition->name
        << " claims bench verification; if that is now true, record the "
           "session in TESTING.md and update this test";
  }
}

TEST(PlayerRegistryTest, PhysicalPositionIsGatedOnTheFirmwareAndNotTheModel) {
  // The LD-V8000 reports the position of its optical assembly from firmware A9
  // onwards. On an earlier revision the same command reads something else, so a
  // capability keyed on the model alone would produce a confident wrong number.
  EXPECT_TRUE(SupportsPhysicalPosition(pioneer::kLdV8000, "A9"));
  EXPECT_FALSE(SupportsPhysicalPosition(pioneer::kLdV8000, "A8"));

  // A player that would not say which firmware it runs gets the answer that
  // costs least: no position.
  EXPECT_FALSE(SupportsPhysicalPosition(pioneer::kLdV8000, ""));

  // No other model claims it, whatever firmware it reports.
  EXPECT_FALSE(SupportsPhysicalPosition(pioneer::kLdV4300D, "A9"));
  EXPECT_FALSE(SupportsPhysicalPosition(GenericPlayer(), "A9"));
}

TEST(PlayerRegistryTest, AnInconsistentDefinitionIsRejected) {
  // The check that fails the build. Exercised here on definitions built to be
  // wrong, because a compile-time assertion cannot be tested by compiling.

  // Claims a control it has nothing to send for.
  PlayerDefinition missing_command = pioneer::kLdV4300D;
  missing_command.commands[Index(PlayerCommand::kSeekFrame)] = CommandSpec{};
  EXPECT_FALSE(IsConsistent(missing_command));

  // Same definition with the claim withdrawn is consistent again, which is what
  // a model genuinely lacking frame search would look like.
  missing_command.capabilities.frame_search = false;
  EXPECT_TRUE(IsConsistent(missing_command));

  // Takes an argument without saying how wide it may be.
  PlayerDefinition widthless = pioneer::kLdV4300D;
  widthless.commands[Index(PlayerCommand::kSeekFrame)].argument_digits = 0;
  EXPECT_FALSE(IsConsistent(widthless));

  // A firmware gate with no firmware to gate on.
  PlayerDefinition ungated = pioneer::kLdV8000;
  ungated.physical_position_firmware = "";
  EXPECT_FALSE(IsConsistent(ungated));

  // Claims digital audio with no parameter for it.
  PlayerDefinition no_digital = pioneer::kLdV4300D;
  no_digital.audio_parameters[static_cast<size_t>(AudioMode::kDigitalStereo)] =
      kParameterUnsupported;
  EXPECT_FALSE(IsConsistent(no_digital));

  no_digital.capabilities.digital_audio = false;
  EXPECT_TRUE(IsConsistent(no_digital));
}

}  // namespace
}  // namespace ddd::player
