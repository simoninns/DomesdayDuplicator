/************************************************************************

    test_capture_provenance.cpp

    T1 tests for what a capture file says about where it came from
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <algorithm>
#include <ctime>
#include <optional>
#include <string>

#include "capture_format.h"
#include "capture_provenance.h"
#include "sample_format.h"
#include "utc_time_zone.h"

namespace ddd::capture {
namespace {

constexpr std::time_t kFixedTime = 1'786'624'496;  // 2026-08-13 12:34:56 UTC

class CaptureProvenanceTest : public ::testing::Test {
 protected:
  void SetUp() override { ddd::capture::test::UseUtc(); }
};

CaptureProvenance Facts() {
  CaptureProvenance facts;
  facts.title = "RF-Sample_2026-08-13_12-34-56";
  facts.application_version = "abcd1234";
  facts.test_mode = false;
  facts.started = kFixedTime;
  return facts;
}

std::optional<std::string> Value(const std::vector<FlacWriter::Tag>& tags,
                                 const std::string& name) {
  const auto found = std::find_if(
      tags.begin(), tags.end(),
      [&name](const FlacWriter::Tag& tag) { return tag.name == name; });
  if (found == tags.end()) {
    return std::nullopt;
  }
  return found->value;
}

TEST_F(CaptureProvenanceTest, ACaptureNamesTheBuildThatProducedIt) {
  const std::vector<FlacWriter::Tag> tags = BuildProvenanceTags(Facts());

  EXPECT_EQ(Value(tags, kTagVersion), "abcd1234");
  EXPECT_EQ(Value(tags, kTagEncoder), "ddd-gui abcd1234");
  EXPECT_EQ(Value(tags, kTagTitle), "RF-Sample_2026-08-13_12-34-56");
}

// The tag that exists because the container cannot carry the truth. FLAC's
// sample-rate field stops at 655,350 Hz, so the header says 40,000 and this is
// the only place the file records that the device ran at forty million.
TEST_F(CaptureProvenanceTest, TheRealSampleRateIsRecordedNotTheFlacLabel) {
  const std::vector<FlacWriter::Tag> tags = BuildProvenanceTags(Facts());

  EXPECT_EQ(Value(tags, kTagSampleRate).value_or(""),
            std::to_string(kSampleRateHz));
  EXPECT_NE(Value(tags, kTagSampleRate).value_or(""),
            std::to_string(kFlacSampleRateLabel));
}

// The rate of the *file*, so a decimated capture says what it holds rather than
// what the device ran at. Without this the only evidence a file is half-rate is
// that it decodes an octave out, which is the sort of thing that gets blamed on
// the player.
TEST_F(CaptureProvenanceTest, ADecimatedCaptureRecordsTheRateItWasWrittenAt) {
  CaptureProvenance facts = Facts();
  facts.decimation_factor = kTapeDecimationFactor;

  const std::vector<FlacWriter::Tag> tags = BuildProvenanceTags(facts);

  EXPECT_EQ(Value(tags, kTagSampleRate).value_or(""),
            std::to_string(kSampleRateHz / 2));
  EXPECT_EQ(Value(tags, kTagDecimation).value_or(""), "2");
}

// And an ordinary capture says so explicitly rather than by the tag's absence,
// so a reader never has to decide what a missing one meant.
TEST_F(CaptureProvenanceTest,
       AnUndecimatedCaptureSaysSoRatherThanStayingSilent) {
  EXPECT_EQ(Value(BuildProvenanceTags(Facts()), kTagDecimation).value_or(""),
            "1");
}

// A test capture and a real one are indistinguishable by inspection until
// somebody decodes one, so the file has to say which it is.
TEST_F(CaptureProvenanceTest, TestModeIsRecordedEitherWay) {
  CaptureProvenance facts = Facts();

  facts.test_mode = true;
  EXPECT_EQ(Value(BuildProvenanceTags(facts), kTagTestMode), "true");

  facts.test_mode = false;
  EXPECT_EQ(Value(BuildProvenanceTags(facts), kTagTestMode), "false");
}

// The gain tag is a declaration the user made, never a value this application
// inferred — so when no declaration was made the tag is absent rather than
// carrying a default. A default here would look like calibration data and would
// be wrong.
TEST_F(CaptureProvenanceTest, AnUndeclaredGainIsWrittenAsNothingAtAll) {
  CaptureProvenance facts = Facts();
  facts.front_end_gain.clear();

  const std::vector<FlacWriter::Tag> tags = BuildProvenanceTags(facts);
  EXPECT_FALSE(Value(tags, kTagFrontEndGain).has_value());
}

TEST_F(CaptureProvenanceTest, ADeclaredGainTravelsWithTheSamples) {
  CaptureProvenance facts = Facts();
  facts.front_end_gain = "switches 1, 3 (x3.34, 599 mV p-p full scale)";

  EXPECT_EQ(Value(BuildProvenanceTags(facts), kTagFrontEndGain),
            "switches 1, 3 (x3.34, 599 mV p-p full scale)");
}

TEST_F(CaptureProvenanceTest, TheDateIsIso8601) {
  EXPECT_EQ(Value(BuildProvenanceTags(Facts()), kTagDate), "2026-08-13");
}

TEST_F(CaptureProvenanceTest, EveryTagHasANameAndAValue) {
  // An empty name produces a comment libFLAC will not build, and an empty value
  // is indistinguishable from an absent tag to a reader.
  for (const FlacWriter::Tag& tag : BuildProvenanceTags(Facts())) {
    EXPECT_FALSE(tag.name.empty());
    EXPECT_FALSE(tag.value.empty()) << tag.name;
  }
}

}  // namespace
}  // namespace ddd::capture
