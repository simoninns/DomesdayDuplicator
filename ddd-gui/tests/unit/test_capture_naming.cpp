/************************************************************************

    test_capture_naming.cpp

    T1 tests for what a capture file is called
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

#include "capture_naming.h"
#include "utc_time_zone.h"

namespace ddd::capture {
namespace {

// 2026-08-13 12:34:56 UTC. A fixed point, so the expected names can be written
// out in full rather than derived by repeating the implementation — a test that
// formats the time the same way the code does would pass however wrong both of
// them were.
constexpr std::time_t kFixedTime = 1'786'624'496;

// The names are in local time, so the tests have to say what local is.
class InUtc : public ::testing::Test {
 protected:
  void SetUp() override { ddd::capture::test::UseUtc(); }
};

using CaptureNamingTest = InUtc;

TEST_F(CaptureNamingTest, ATimestampSortsAsATextString) {
  // The whole reason for this format. A directory of captures listed
  // alphabetically has to come out in the order they were taken, which needs
  // fixed-width fields, most significant first, and no locale in sight.
  EXPECT_EQ(FormatCaptureTimestamp(kFixedTime), "2026-08-13_12-34-56");
  EXPECT_LT(FormatCaptureTimestamp(kFixedTime - 1),
            FormatCaptureTimestamp(kFixedTime));

  // Every field is padded, or January would sort after October.
  EXPECT_EQ(FormatCaptureTimestamp(0), "1970-01-01_00-00-00");
}

TEST_F(CaptureNamingTest, ATimestampCarriesNoCharacterAFilesystemRefuses) {
  const std::string stamp = FormatCaptureTimestamp(kFixedTime);
  EXPECT_EQ(stamp.find_first_of("<>:\"/\\|?* "), std::string::npos) << stamp;
}

TEST_F(CaptureNamingTest, AnOrdinaryCaptureIsCalledRfSample) {
  EXPECT_EQ(DefaultCaptureStem(false, kFixedTime),
            "RF-Sample_2026-08-13_12-34-56");
}

TEST_F(CaptureNamingTest, ATestCaptureIsCalledTestData) {
  EXPECT_EQ(DefaultCaptureStem(true, kFixedTime),
            "TestData_2026-08-13_12-34-56");
}

// --- What a user may type ------------------------------------------------

TEST_F(CaptureNamingTest, AnOrdinaryNameIsLeftAlone) {
  EXPECT_EQ(SanitiseCaptureStem("Blade Runner side 1"), "Blade Runner side 1");
  EXPECT_EQ(SanitiseCaptureStem("disc-01_side-A"), "disc-01_side-A");
}

// The half of this that matters. Without it a name is a path, and a text field
// decides where on the disk the capture is written.
TEST_F(CaptureNamingTest, ANameCannotEscapeIntoAPath) {
  EXPECT_EQ(SanitiseCaptureStem("../../etc/passwd"), "....etcpasswd");
  EXPECT_EQ(SanitiseCaptureStem("C:\\Windows\\System32\\evil"),
            "CWindowsSystem32evil");

  const std::filesystem::path built =
      BuildCapturePath("/captures", "../elsewhere/name", false, kFixedTime);
  EXPECT_EQ(built.parent_path(), std::filesystem::path("/captures"));
}

TEST_F(CaptureNamingTest, CharactersWindowsRefusesAreRemoved) {
  EXPECT_EQ(SanitiseCaptureStem("a<b>c:d\"e/f\\g|h?i*j"), "abcdefghij");
}

TEST_F(CaptureNamingTest, ControlCharactersAreRemoved) {
  EXPECT_EQ(SanitiseCaptureStem(std::string("side\x01\x1F\x7F"
                                            "1")),
            "side1");
}

TEST_F(CaptureNamingTest, SurroundingSpaceAndTrailingDotsGo) {
  // Windows silently drops a trailing dot or space, so a name kept as typed
  // would differ from the name that appears on the disk.
  EXPECT_EQ(SanitiseCaptureStem("  side one  "), "side one");
  EXPECT_EQ(SanitiseCaptureStem("side one..."), "side one");
  EXPECT_EQ(SanitiseCaptureStem("side one. . "), "side one");
}

TEST_F(CaptureNamingTest, ReservedDeviceNamesAreRefusedWholesale) {
  // Reserved with or without an extension on Windows, so "CON.ddd.flac" fails
  // to open just as "CON" does. Refused rather than mangled: a user who typed
  // one of these gets the default name, which works.
  EXPECT_TRUE(SanitiseCaptureStem("CON").empty());
  EXPECT_TRUE(SanitiseCaptureStem("nul").empty());
  EXPECT_TRUE(SanitiseCaptureStem("Com4").empty());
  EXPECT_TRUE(SanitiseCaptureStem("LPT9").empty());

  // But not names that merely start with one
  EXPECT_EQ(SanitiseCaptureStem("CONCERT"), "CONCERT");
}

TEST_F(CaptureNamingTest, ANameThatSanitisesToNothingFallsBackToTheDefault) {
  const std::filesystem::path path =
      BuildCapturePath("/captures", "///", false, kFixedTime);
  EXPECT_EQ(path.filename().string(), "RF-Sample_2026-08-13_12-34-56.ddd.flac");
}

// --- Building the path ---------------------------------------------------

TEST_F(CaptureNamingTest, ATypedNameGetsTheCaptureSuffix) {
  const std::filesystem::path path =
      BuildCapturePath("/captures", "side one", false, kFixedTime);
  EXPECT_EQ(path, std::filesystem::path("/captures/side one.ddd.flac"));
}

TEST_F(CaptureNamingTest, ANameThatAlreadyHasTheSuffixDoesNotGetASecond) {
  const std::filesystem::path path =
      BuildCapturePath("/captures", "side one.ddd.flac", false, kFixedTime);
  EXPECT_EQ(path.filename().string(), "side one.ddd.flac");
}

// Task 5.3, and the reason it is worth a test of its own: a test capture is a
// ramp from the pattern generator with no signal in it at all. A file called
// "Blade Runner side 1" that turns out to be ramps is a trap, so the name is
// forced rather than defaulted — what the user typed is deliberately ignored.
TEST_F(CaptureNamingTest, ATestCaptureIsNamedTestDataWhateverWasTyped) {
  const std::filesystem::path path =
      BuildCapturePath("/captures", "Blade Runner side 1", true, kFixedTime);
  EXPECT_EQ(path.filename().string(), "TestData_2026-08-13_12-34-56.ddd.flac");
}

// --- Not overwriting anything -------------------------------------------

class UniqueNameTest : public InUtc {
 protected:
  void SetUp() override {
    InUtc::SetUp();
    directory_ =
        std::filesystem::temp_directory_path() / "ddd-capture-naming-test";
    std::filesystem::remove_all(directory_);
    std::filesystem::create_directories(directory_);
  }

  void TearDown() override { std::filesystem::remove_all(directory_); }

  void Touch(const std::filesystem::path& path) {
    std::ofstream file(path);
    file << "x";
  }

  std::filesystem::path directory_;
};

TEST_F(UniqueNameTest, AFreePathIsUsedUnchanged) {
  const std::filesystem::path wanted = directory_ / "capture.ddd.flac";
  EXPECT_EQ(MakeUniqueCapturePath(wanted), wanted);
}

TEST_F(UniqueNameTest, AnExistingCaptureIsNeverOverwritten) {
  const std::filesystem::path wanted = directory_ / "capture.ddd.flac";
  Touch(wanted);

  const std::filesystem::path second = MakeUniqueCapturePath(wanted);
  EXPECT_NE(second, wanted);
  EXPECT_FALSE(std::filesystem::exists(second));

  // The number goes before the compound suffix, not inside it: taking
  // extension() off would leave ".ddd" behind and produce "capture.ddd_2.flac",
  // which no tool would recognise as a capture.
  EXPECT_EQ(second.filename().string(), "capture_2.ddd.flac");

  Touch(second);
  EXPECT_EQ(MakeUniqueCapturePath(wanted).filename().string(),
            "capture_3.ddd.flac");
}

}  // namespace
}  // namespace ddd::capture
