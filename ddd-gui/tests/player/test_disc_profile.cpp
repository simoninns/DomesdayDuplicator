/************************************************************************

    test_disc_profile.cpp

    T1 tests for what an examination found, and what follows from it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include "disc_profile.h"

namespace ddd::player {
namespace {

// A CLV disc running to 0:50:45, measured.
DiscProfile ClvDisc() {
  DiscProfile disc;
  disc.disc_type.Record(DiscType::kClv, Provenance::kReported);
  disc.addressing.Record(AddressMode::kTimeCode, Provenance::kInferred);
  disc.programme_start.Record(0, Provenance::kMeasured);
  disc.programme_end.Record(504500, Provenance::kMeasured);
  return disc;
}

// A CAV disc of 54,000 frames, measured.
DiscProfile CavDisc() {
  DiscProfile disc;
  disc.disc_type.Record(DiscType::kCav, Provenance::kReported);
  disc.addressing.Record(AddressMode::kFrame, Provenance::kInferred);
  disc.programme_start.Record(1, Provenance::kMeasured);
  disc.programme_end.Record(54000, Provenance::kMeasured);
  return disc;
}

TEST(FactTest, AFieldNobodyHasFilledInSaysSoRatherThanReadingAsZero) {
  const Fact<int32_t> length;

  EXPECT_FALSE(length.known());
  EXPECT_EQ(length.provenance, Provenance::kUnknown);

  // The point of the type: zero is a perfectly good frame number, so "unknown"
  // cannot be represented by the value alone.
  EXPECT_EQ(length.value, 0);
}

TEST(FactTest, RecordingAValueRecordsWhereItCameFrom) {
  Fact<int32_t> length;
  length.Record(54000, Provenance::kMeasured);

  EXPECT_TRUE(length.known());
  EXPECT_EQ(length.value, 54000);
  EXPECT_EQ(length.provenance, Provenance::kMeasured);
}

TEST(DiscProfileTest, AFreshProfileKnowsNothingAndClaimsNothing) {
  const DiscProfile disc;

  EXPECT_FALSE(disc.disc_present.known());
  EXPECT_FALSE(disc.disc_type.known());
  EXPECT_FALSE(disc.programme_end.known());
  EXPECT_FALSE(disc.video_standard.known());
  EXPECT_EQ(disc.pioneer_user_code.outcome, UserCodeReading::Outcome::kNotRead);
  EXPECT_TRUE(disc.disc_status_reply.empty());
}

TEST(DiscProfileTest, AClvDiscsLengthIsATimeAlreadyAndNeedsNoStandard) {
  const DiscProfile disc = ClvDisc();

  ASSERT_FALSE(disc.video_standard.known());

  // 0:50:45. Compared whole rather than dereferenced, so that a duration which
  // was never worked out fails here rather than needing its own assertion.
  EXPECT_EQ(ProgrammeDuration(disc), std::chrono::seconds{(50 * 60) + 45});
}

TEST(DiscProfileTest, ACavDiscsLengthIsFramesAndSaysNothingWithoutAStandard) {
  // The honest answer, and the one that matters. Assuming thirty frames a
  // second here would make a PAL side read twenty per cent short — which is a
  // capture that runs off the end of the volume it was estimated to fit on.
  EXPECT_FALSE(ProgrammeDuration(CavDisc()).has_value());
}

TEST(DiscProfileTest, ACavDiscsLengthFollowsFromTheStandardOnceItIsDeclared) {
  DiscProfile ntsc = CavDisc();
  ntsc.video_standard.Record(VideoStandard::kNtsc, Provenance::kDeclared);

  DiscProfile pal = CavDisc();
  pal.video_standard.Record(VideoStandard::kPal, Provenance::kDeclared);

  const std::optional<std::chrono::seconds> ntsc_duration =
      ProgrammeDuration(ntsc);
  const std::optional<std::chrono::seconds> pal_duration =
      ProgrammeDuration(pal);

  // 54,000 frames: half an hour of NTSC, thirty-six minutes of PAL.
  EXPECT_EQ(ntsc_duration, std::chrono::seconds{1802});
  EXPECT_EQ(pal_duration, std::chrono::seconds{2160});
}

TEST(DiscProfileTest, ALengthNobodyMeasuredProducesNoDuration) {
  DiscProfile disc = CavDisc();
  disc.video_standard.Record(VideoStandard::kNtsc, Provenance::kDeclared);
  disc.programme_end = Fact<int32_t>{};

  EXPECT_FALSE(ProgrammeDuration(disc).has_value());
}

TEST(DiscProfileTest, AnEndBeforeItsStartIsRefusedRatherThanNegated) {
  DiscProfile disc = CavDisc();
  disc.video_standard.Record(VideoStandard::kNtsc, Provenance::kDeclared);
  disc.programme_start.Record(54000, Provenance::kMeasured);
  disc.programme_end.Record(1, Provenance::kMeasured);

  EXPECT_FALSE(ProgrammeDuration(disc).has_value());
}

TEST(DiscProfileTest, AStartNobodyMeasuredIsTakenAsTheBeginning) {
  DiscProfile disc = ClvDisc();
  disc.programme_start = Fact<int32_t>{};

  EXPECT_EQ(ProgrammeDuration(disc), std::chrono::seconds{(50 * 60) + 45});
}

TEST(DiscProfileTest, TheFrameRateIsTheStandardsAndNotARoundNumber) {
  // The sentinel can never pass either comparison, so a rate that was not
  // reported fails here rather than needing an assertion of its own.
  EXPECT_NEAR(FrameRate(VideoStandard::kNtsc).value_or(0.0), 29.97, 0.001);
  EXPECT_DOUBLE_EQ(FrameRate(VideoStandard::kPal).value_or(0.0), 25.0);

  EXPECT_FALSE(FrameRate(VideoStandard::kUnknown).has_value());
}

TEST(UserCodeReadingTest, NotAskedNotEncodedAndUnreadableAreThreeAnswers) {
  const UserCodeReading not_read;
  EXPECT_FALSE(not_read.read());

  UserCodeReading not_encoded;
  not_encoded.outcome = UserCodeReading::Outcome::kNotEncoded;
  not_encoded.text = "E04";
  EXPECT_FALSE(not_encoded.read());

  UserCodeReading read;
  read.outcome = UserCodeReading::Outcome::kRead;
  read.text = "#59-014";
  EXPECT_TRUE(read.read());

  // The distinction the whole type exists for: none of these three compares
  // equal to another, so nothing downstream can accidentally treat "we did not
  // ask" as "the disc has none".
  EXPECT_NE(not_read, not_encoded);
  EXPECT_NE(not_encoded, read);
}

}  // namespace
}  // namespace ddd::player
