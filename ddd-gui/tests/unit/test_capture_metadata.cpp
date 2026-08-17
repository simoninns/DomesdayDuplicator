/************************************************************************

    test_capture_metadata.cpp

    T1 tests for the YAML sidecar written beside every capture
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "capture_metadata.h"
#include "utc_time_zone.h"

namespace ddd::capture {
namespace {

// 2026-08-13 12:34:56 UTC, the same fixed point the naming tests use.
constexpr std::time_t kFixedTime = 1'786'624'496;

class CaptureMetadataTest : public ::testing::Test {
 protected:
  void SetUp() override { ddd::capture::test::UseUtc(); }

  // A capture that ran and finished, with nothing said about the disc and no
  // player attached — which is what almost every capture is.
  static CaptureMetadata Ordinary() {
    CaptureMetadata metadata;
    metadata.capture_file_name = "RF-Sample_2026-08-13_12-34-56.ddd.flac";
    metadata.application_version = "1.2.3";
    metadata.format = "FLAC";
    metadata.sample_rate_hz = 40'000'000;
    metadata.decimation_factor = 1;
    metadata.started = kFixedTime;
    metadata.finished = kFixedTime + 60;
    metadata.outcome.duration_seconds = 60.0;
    metadata.outcome.samples = 2'400'000'000;
    metadata.outcome.bytes = 1'234'567'890;
    metadata.outcome.sequence_check = "running";
    return metadata;
  }

  static bool Contains(const std::string& document, const std::string& line) {
    return document.find(line) != std::string::npos;
  }
};

TEST_F(CaptureMetadataTest, TheSidecarSitsBesideTheCaptureItDescribes) {
  EXPECT_EQ(CaptureMetadataPath("/captures/Casper_side1.ddd.flac"),
            std::filesystem::path("/captures/Casper_side1.ddd.yaml"));

  // The uncompressed format has no tags of its own, so this file is the whole
  // of its provenance — and it is named on the same rule.
  EXPECT_EQ(CaptureMetadataPath("/captures/Casper_side1.ddd.s16"),
            std::filesystem::path("/captures/Casper_side1.ddd.yaml"));
}

TEST_F(CaptureMetadataTest, APathWithNoCaptureSuffixGainsRatherThanLosesOne) {
  // Nothing in this application produces such a path, but appending cannot
  // destroy the association between the two files where replacing an unknown
  // extension could.
  EXPECT_EQ(CaptureMetadataPath("/captures/oddity.bin"),
            std::filesystem::path("/captures/oddity.bin.ddd.yaml"));
}

TEST_F(CaptureMetadataTest, AnOrdinaryCaptureRecordsWhatItWas) {
  const std::string document = BuildCaptureMetadataYaml(Ordinary());

  EXPECT_TRUE(Contains(document, "\"schema_version\": 1"));
  EXPECT_TRUE(Contains(document, "\"application_version\": \"1.2.3\""));
  EXPECT_TRUE(Contains(document,
                       "\"file\": \"RF-Sample_2026-08-13_12-34-56.ddd.flac\""));
  EXPECT_TRUE(Contains(document, "\"format\": \"FLAC\""));
  EXPECT_TRUE(Contains(document, "\"sample_rate_hz\": 40000000"));
  EXPECT_TRUE(Contains(document, "\"duration_seconds\": 60.000"));
  EXPECT_TRUE(Contains(document, "\"samples\": 2400000000"));
  EXPECT_TRUE(Contains(document, "\"completed\": true"));
  EXPECT_TRUE(Contains(document, "\"sequence_check\": \"running\""));

  // Local time with the offset on the end, so the timestamp is a moment rather
  // than a reading on a clock. The tests run in UTC.
  EXPECT_TRUE(Contains(document, "\"started\": \"2026-08-13T12:34:56+00:00\""))
      << document;
}

TEST_F(CaptureMetadataTest, TheTestModeFlagIsAlwaysWritten) {
  // Never left out, whichever way round it is. A capture of ramps and a capture
  // of signal are indistinguishable by inspection until somebody decodes one,
  // so this is the one boolean that is worth stating even when it is false.
  EXPECT_TRUE(
      Contains(BuildCaptureMetadataYaml(Ordinary()), "\"test_mode\": false"));

  CaptureMetadata test_capture = Ordinary();
  test_capture.test_mode = true;
  EXPECT_TRUE(
      Contains(BuildCaptureMetadataYaml(test_capture), "\"test_mode\": true"));
}

TEST_F(CaptureMetadataTest, NothingEstablishedIsNothingWritten) {
  const std::string document = BuildCaptureMetadataYaml(Ordinary());

  // The gain in particular: a capture carrying a default figure nobody had
  // checked would read as calibration data, which is worse than saying nothing.
  EXPECT_FALSE(Contains(document, "front_end_gain"));
  EXPECT_FALSE(Contains(document, "\"title\""));
  EXPECT_FALSE(Contains(document, "model_name"));

  // But the sections themselves are there and empty, so a reader can tell this
  // document from one written before those sections existed.
  EXPECT_TRUE(Contains(document, "\"naming\": {}"));
  EXPECT_TRUE(Contains(document, "\"player\": {}"));
  EXPECT_TRUE(Contains(document, "\"disc\": {}"));
}

TEST_F(CaptureMetadataTest, WhatTheUserSaidTheDiscWasIsRecorded) {
  CaptureMetadata metadata = Ordinary();
  metadata.naming.title_used = true;
  metadata.naming.title = "Casper";
  metadata.naming.disc_type_used = true;
  metadata.naming.disc_type = DiscTypeChoice::kClv;
  metadata.naming.video_standard_used = true;
  metadata.naming.video_standard = VideoStandardChoice::kPal;
  metadata.naming.audio_used = true;
  metadata.naming.audio = AudioTypeChoice::kAnalogue;
  metadata.naming.side_used = true;
  metadata.naming.side = 2;
  metadata.naming.notes_used = true;
  metadata.naming.notes = "second pressing";
  metadata.naming.mint_marks_used = true;
  metadata.naming.mint_marks = "NM";
  metadata.naming.metadata_notes = "Rot on the outer edge.";

  const std::string document = BuildCaptureMetadataYaml(metadata);

  EXPECT_TRUE(Contains(document, "\"title\": \"Casper\""));
  EXPECT_TRUE(Contains(document, "\"disc_type\": \"CLV\""));
  EXPECT_TRUE(Contains(document, "\"video_standard\": \"PAL\""));

  // Spelled out in the metadata where the file name gets "ANA". A file name is
  // short because it is a file name; a metadata field is read by somebody who
  // was not there.
  EXPECT_TRUE(Contains(document, "\"audio\": \"Analogue\""));
  EXPECT_TRUE(Contains(document, "\"side\": 2"));
  EXPECT_TRUE(Contains(document, "\"notes\": \"second pressing\""));
  EXPECT_TRUE(Contains(document, "\"mint_marks\": \"NM\""));
  EXPECT_TRUE(
      Contains(document, "\"metadata_notes\": \"Rot on the outer edge.\""));
}

TEST_F(CaptureMetadataTest, AFieldNobodyWasAskedAboutStaysOut) {
  // The flag decides, not the value. A title typed and then unticked is a title
  // nobody is claiming, and the sidecar must not claim it either.
  CaptureMetadata metadata = Ordinary();
  metadata.naming.title = "Casper";
  metadata.naming.side = 2;

  const std::string document = BuildCaptureMetadataYaml(metadata);
  EXPECT_FALSE(Contains(document, "Casper"));
  EXPECT_FALSE(Contains(document, "\"side\""));
}

TEST_F(CaptureMetadataTest, ThePlayerIsRecordedWhereThereWasOne) {
  CaptureMetadata metadata = Ordinary();
  metadata.player.model_name = "Pioneer LD-V4300D";
  metadata.player.model_id_code = "P15";
  metadata.player.model_code = "P1512";
  metadata.player.firmware_version = "12";
  metadata.player.port = "/dev/ttyUSB0";
  metadata.player.baud_rate = 9600;
  metadata.player.recognised_model = true;

  const std::string document = BuildCaptureMetadataYaml(metadata);

  EXPECT_TRUE(Contains(document, "\"model_name\": \"Pioneer LD-V4300D\""));
  EXPECT_TRUE(Contains(document, "\"model_id_code\": \"P15\""));
  EXPECT_TRUE(Contains(document, "\"model_code\": \"P1512\""));
  EXPECT_TRUE(Contains(document, "\"firmware_version\": \"12\""));
  EXPECT_TRUE(Contains(document, "\"port\": \"/dev/ttyUSB0\""));
  EXPECT_TRUE(Contains(document, "\"baud_rate\": 9600"));
  EXPECT_TRUE(Contains(document, "\"recognised_model\": true"));
}

TEST_F(CaptureMetadataTest, TheScanRecordsEveryFactWithHowItWasEstablished) {
  CaptureMetadata metadata = Ordinary();
  metadata.disc.examined = true;
  metadata.disc.disc_type = ScannedFact{"CLV", "reported"};
  metadata.disc.disc_side = ScannedFact{"2", "reported"};
  metadata.disc.programme_end = ScannedFact{"1:02:03", "measured"};
  metadata.disc.video_standard = ScannedFact{"PAL", "declared"};
  metadata.disc.disc_status_reply = "11011";

  const std::string document = BuildCaptureMetadataYaml(metadata);

  // The provenance travels with the value rather than being implied by the
  // section. A length that came from seeking past the end of the side and one a
  // disc merely claims are both numbers, and a file showing them alike would
  // have to be believed rather than read.
  EXPECT_TRUE(Contains(document,
                       "    \"value\": \"1:02:03\"\n"
                       "    \"source\": \"measured\""));
  EXPECT_TRUE(Contains(document,
                       "    \"value\": \"PAL\"\n"
                       "    \"source\": \"declared\""));
  EXPECT_TRUE(Contains(document, "\"examined\": true"));

  // The working, not the answer: a sidecar that says "side 2" and shows the
  // characters it read that from is one somebody can check.
  EXPECT_TRUE(Contains(document, "\"disc_status_reply\": \"11011\""));
}

TEST_F(CaptureMetadataTest, AnExaminationThatFoundNothingIsNotNoExamination) {
  // The distinction the `examined` flag exists for: a player that refused every
  // query still produced a finding, and it is not the same finding as a capture
  // taken with no examination at all.
  CaptureMetadata metadata = Ordinary();
  metadata.disc.examined = true;

  const std::string document = BuildCaptureMetadataYaml(metadata);
  EXPECT_TRUE(Contains(document, "\"examined\": true"));
  EXPECT_FALSE(Contains(document, "\"disc\": {}"));
}

TEST_F(CaptureMetadataTest, AUserCodeKeepsItsOutcomeAndItsCharacters) {
  CaptureMetadata metadata = Ordinary();
  metadata.disc.examined = true;
  metadata.disc.standard_user_code_outcome = "not encoded on the disc";
  metadata.disc.pioneer_user_code_outcome = "read";

  // Sixty characters the player could not read, and then one that was never
  // encoded. Recording those alike would record the absence of evidence as
  // evidence of absence.
  metadata.disc.pioneer_user_code = std::string("``` ") + '\0';

  const std::string document = BuildCaptureMetadataYaml(metadata);

  EXPECT_TRUE(Contains(document, "\"outcome\": \"not encoded on the disc\""));
  EXPECT_TRUE(Contains(document, "\"text\": \"``` \\x00\""));
}

TEST_F(CaptureMetadataTest, AFailedCaptureSaysSoAndSaysWhy) {
  CaptureMetadata metadata = Ordinary();
  metadata.outcome.completed = false;
  metadata.outcome.detail = "The device was disconnected";

  const std::string document = BuildCaptureMetadataYaml(metadata);
  EXPECT_TRUE(Contains(document, "\"completed\": false"));
  EXPECT_TRUE(
      Contains(document, "\"detail\": \"The device was disconnected\""));
}

TEST_F(CaptureMetadataTest, TheSignalFiguresAreAboutThisFile) {
  CaptureMetadata metadata = Ordinary();
  metadata.signal.known = true;
  metadata.signal.minimum_value = 12;
  metadata.signal.maximum_value = 1008;
  metadata.signal.rms = 123.456;
  metadata.signal.clipped_low_samples = 4;

  const std::string document = BuildCaptureMetadataYaml(metadata);

  EXPECT_TRUE(Contains(document, "\"minimum_value\": 12"));
  EXPECT_TRUE(Contains(document, "\"rms\": 123.46"));
  EXPECT_TRUE(Contains(document, "\"clipped_low_samples\": 4"));

  // Said in the file rather than only in the source, because a number in a
  // file's metadata will be read as a number about that file whatever the
  // source says.
  EXPECT_TRUE(Contains(document, "# Measured over this file's own samples"));
}

// Metadata is data about the data. Anything measured over the monitoring
// session either side of the file describes something that was never recorded,
// so it is not in the document at all — not written with a caveat, absent.
//
// Ring depth and the device's back-pressure peak are the examples: they say how
// hard this machine was working, which is worth watching live and is not a
// property of a recording that outlives the session by years.
TEST_F(CaptureMetadataTest, NothingAboutTheSessionRatherThanTheRecording) {
  const std::string document = BuildCaptureMetadataYaml(Ordinary());

  EXPECT_FALSE(Contains(document, "peak_buffers_in_use"));
  EXPECT_FALSE(Contains(document, "buffer_count"));
  EXPECT_FALSE(Contains(document, "back_pressure"));
  EXPECT_FALSE(Contains(document, "monitoring run"));
}

// The two that stay, and why they are different from the ones above: a dropped
// word is a sample that existed on the disc and is not in this file.
TEST_F(CaptureMetadataTest, WhatTheDeviceLostWhileWritingThisFileIsRecorded) {
  CaptureMetadata metadata = Ordinary();
  metadata.outcome.device_overflow_events = 2;
  metadata.outcome.device_dropped_words = 1024;

  const std::string document = BuildCaptureMetadataYaml(metadata);
  EXPECT_TRUE(Contains(document, "\"device_overflow_events\": 2"));
  EXPECT_TRUE(Contains(document, "\"device_dropped_words\": 1024"));
}

TEST_F(CaptureMetadataTest, TheDocumentIsWrittenToDiskAsItWasBuilt) {
  const std::filesystem::path directory =
      std::filesystem::temp_directory_path() / "ddd_metadata_test";
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);

  const std::filesystem::path path = directory / "capture.ddd.yaml";
  const CaptureMetadata metadata = Ordinary();

  std::string error;
  ASSERT_TRUE(WriteCaptureMetadataFile(path, metadata, error)) << error;
  ASSERT_TRUE(std::filesystem::exists(path));

  std::ifstream file(path, std::ios::binary);
  const std::string written((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
  EXPECT_EQ(written, BuildCaptureMetadataYaml(metadata));

  std::filesystem::remove_all(directory);
}

TEST_F(CaptureMetadataTest, AFailureToWriteIsReportedRatherThanThrown) {
  // Never treated as a capture failure by the caller — the recording is on disk
  // and complete — so this has to come back as a value rather than as an
  // exception somebody has to remember to catch.
  std::string error;
  EXPECT_FALSE(WriteCaptureMetadataFile(
      std::filesystem::path("/this/directory/does/not/exist/x.ddd.yaml"),
      Ordinary(), error));
  EXPECT_FALSE(error.empty());
}

}  // namespace
}  // namespace ddd::capture
