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

// The uncompressed format gets a suffix on the same pattern: ".ddd" says where
// the samples came from and ".s16" says what layout they are in, which is the
// only thing that can be read out of a headerless file.
TEST_F(CaptureNamingTest, TheUncompressedFormatHasASuffixOfItsOwn) {
  const std::filesystem::path path =
      BuildCapturePath("/captures", "side one", false, kFixedTime,
                       CaptureOutputFormat::kSigned16Bit);
  EXPECT_EQ(path, std::filesystem::path("/captures/side one.ddd.s16"));

  EXPECT_EQ(AddCaptureFileSuffix("side one", CaptureOutputFormat::kSigned16Bit)
                .string(),
            "side one.ddd.s16");

  // Idempotent for this one too, or a name typed with its suffix already on it
  // becomes "side one.ddd.s16.ddd.s16".
  EXPECT_EQ(AddCaptureFileSuffix("side one.ddd.s16",
                                 CaptureOutputFormat::kSigned16Bit)
                .string(),
            "side one.ddd.s16");
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

  // " (1)" and not "_2": the convention every desktop uses, so nobody has to
  // be told what it means, and the number counts the copies rather than the
  // files. And it goes before the compound suffix, not inside it — taking
  // extension() off would leave ".ddd" behind and produce
  // "capture.ddd (1).flac", which no tool would recognise as a capture.
  EXPECT_EQ(second.filename().string(), "capture (1).ddd.flac");

  Touch(second);
  EXPECT_EQ(MakeUniqueCapturePath(wanted).filename().string(),
            "capture (2).ddd.flac");
}

// The number goes in front of whichever compound suffix the path carries. The
// format is read off the path rather than passed in, so this has to recognise
// both — an uncompressed capture named "capture.ddd (1).s16" would be as
// unrecognisable as "capture.ddd (1).flac" is.
TEST_F(UniqueNameTest, TheUncompressedSuffixIsKeptWholeToo) {
  const std::filesystem::path wanted = directory_ / "capture.ddd.s16";
  Touch(wanted);

  EXPECT_EQ(MakeUniqueCapturePath(wanted).filename().string(),
            "capture (1).ddd.s16");
}

// --- Where a capture really goes -------------------------------------------

TEST_F(UniqueNameTest, AFreeNameResolvesToItself) {
  const CaptureDestination destination =
      ResolveCaptureDestination(directory_, "Casper side 1", false, kFixedTime);

  EXPECT_TRUE(destination.as_requested);
  EXPECT_EQ(destination.stem, "Casper side 1");
  EXPECT_EQ(destination.path.filename().string(),
            "Casper side 1" + std::string(kCaptureFileSuffix));
}

TEST_F(UniqueNameTest, ATakenNameResolvesToTheNameThatWillReallyBeUsed) {
  const std::filesystem::path first =
      BuildCapturePath(directory_, "Casper side 1", false, kFixedTime);
  Touch(first);

  const CaptureDestination destination =
      ResolveCaptureDestination(directory_, "Casper side 1", false, kFixedTime);

  // Never an overwrite, and now never a silent one either: the stem is what an
  // interface shows, so the name on screen is the name on disk.
  EXPECT_FALSE(destination.as_requested);
  EXPECT_EQ(destination.stem, "Casper side 1 (1)");
  EXPECT_NE(destination.path, first);
  EXPECT_TRUE(std::filesystem::exists(first));
}

TEST_F(UniqueNameTest, TheGeneratedNameIsFreeByConstruction) {
  // Nothing typed, so the name carries a timestamp — which is the whole reason
  // an empty name needs no warning and a typed one does.
  const CaptureDestination destination =
      ResolveCaptureDestination(directory_, "", false, kFixedTime);

  EXPECT_TRUE(destination.as_requested);
  EXPECT_EQ(destination.stem, DefaultCaptureStem(false, kFixedTime));
}

TEST_F(UniqueNameTest, TheStemComesBackWithoutTheCompoundSuffix) {
  // ".ddd.flac" is two extensions, so a stem taken with stem() would keep the
  // ".ddd" and an interface would show a name nobody typed.
  for (const CaptureOutputFormat format :
       {CaptureOutputFormat::kFlac, CaptureOutputFormat::kSigned16Bit}) {
    const CaptureDestination destination = ResolveCaptureDestination(
        directory_, "disc1", false, kFixedTime, format);
    EXPECT_EQ(destination.stem, "disc1");
  }
}

// --- Naming from what the user says the disc is ----------------------------

using NamingFieldsTest = InUtc;

TEST_F(NamingFieldsTest, NothingSaidGivesTheNameItAlwaysGave) {
  // The compatibility promise: somebody who never opens the naming dialog gets
  // exactly the name this application produced before it existed.
  const CaptureNamingFields fields;
  EXPECT_EQ(BuildCaptureStem(fields, "", false, kFixedTime),
            DefaultCaptureStem(false, kFixedTime));
}

TEST_F(NamingFieldsTest, ATypedNameWinsAndCarriesNoTimestamp) {
  CaptureNamingFields fields;
  fields.title_used = true;
  fields.title = "Casper";
  fields.side_used = true;
  fields.side = 2;

  // Somebody who typed a name meant that name. The fields are still recorded in
  // the sidecar; they simply do not get a say in what the file is called.
  EXPECT_EQ(BuildCaptureStem(fields, "my capture", false, kFixedTime),
            "my capture");
}

TEST_F(NamingFieldsTest, TestModeOverridesEverything) {
  CaptureNamingFields fields;
  fields.title_used = true;
  fields.title = "Blade Runner";

  // A file of ramps must never carry a disc's name, whatever has been typed
  // where — including in the Name field, which is the one that otherwise wins.
  EXPECT_EQ(BuildCaptureStem(fields, "Blade Runner", true, kFixedTime),
            "TestData_2026-08-13_12-34-56");
}

TEST_F(NamingFieldsTest, ATitleReplacesTheRfSamplePrefix) {
  CaptureNamingFields fields;
  fields.title_used = true;
  fields.title = "Casper";

  EXPECT_EQ(BuildCaptureStem(fields, "", false, kFixedTime),
            "Casper_2026-08-13_12-34-56");
}

TEST_F(NamingFieldsTest, TheSideIsInTheNameWithoutTheOtherDetails) {
  // The asymmetry that earns its place: two files made in a row are the two
  // sides of one disc, so the side is in the name whether or not the rest is.
  CaptureNamingFields fields;
  fields.title_used = true;
  fields.title = "Casper";
  fields.side_used = true;
  fields.side = 2;
  fields.disc_type_used = true;
  fields.disc_type = DiscTypeChoice::kCav;

  EXPECT_EQ(BuildCaptureStem(fields, "", false, kFixedTime),
            "Casper_side2_2026-08-13_12-34-56");
}

TEST_F(NamingFieldsTest, TheDetailsJoinTheNameOnlyWhenAskedFor) {
  CaptureNamingFields fields;
  fields.title_used = true;
  fields.title = "Casper";
  fields.disc_type_used = true;
  fields.disc_type = DiscTypeChoice::kClv;
  fields.video_standard_used = true;
  fields.video_standard = VideoStandardChoice::kPal;
  fields.audio_used = true;
  fields.audio = AudioTypeChoice::kAnalogue;
  fields.side_used = true;
  fields.side = 1;
  fields.notes_used = true;
  fields.notes = "second pressing";
  fields.mint_marks_used = true;
  fields.mint_marks = "NM";
  fields.metadata_in_name = true;

  EXPECT_EQ(BuildCaptureStem(fields, "", false, kFixedTime),
            "Casper_CLV_PAL_ANA_side1_second pressing_NM_2026-08-13_12-34-56");
}

TEST_F(NamingFieldsTest, DefaultAudioIsARealAnswerThatAddsNothingToAName) {
  // The one choice that means something in the metadata and contributes no
  // token: "_Default" in a file name says less than the eight characters cost.
  CaptureNamingFields fields;
  fields.audio_used = true;
  fields.audio = AudioTypeChoice::kDefault;
  fields.metadata_in_name = true;

  EXPECT_EQ(BuildCaptureStem(fields, "", false, kFixedTime),
            DefaultCaptureStem(false, kFixedTime));
  EXPECT_STREQ(AudioTypeChoiceName(AudioTypeChoice::kDefault), "Default");
  EXPECT_STREQ(AudioTypeChoiceToken(AudioTypeChoice::kDefault), "");
}

TEST_F(NamingFieldsTest, AFieldThatWasNotAskedForContributesNothing) {
  // The flag is what decides, not whether the value happens to be set. A title
  // typed and then unticked is a title nobody is claiming.
  CaptureNamingFields fields;
  fields.title = "Casper";
  fields.disc_type = DiscTypeChoice::kCav;
  fields.side = 3;
  fields.metadata_in_name = true;

  EXPECT_EQ(BuildCaptureStem(fields, "", false, kFixedTime),
            DefaultCaptureStem(false, kFixedTime));
}

TEST_F(NamingFieldsTest, ATitleCannotChooseWhereTheCaptureGoes) {
  // The security-relevant half of the sanitising, reached through the fields
  // rather than through the Name field: a title of "../../etc/passwd" must not
  // become a path.
  CaptureNamingFields fields;
  fields.title_used = true;
  fields.title = "../../etc/passwd";

  const std::string stem = BuildCaptureStem(fields, "", false, kFixedTime);
  EXPECT_EQ(stem.find_first_of("<>:\"/\\|?*"), std::string::npos) << stem;
}

TEST_F(NamingFieldsTest, ADurationGoesOnTheEndInFixedWidthFields) {
  // Letters between the fields rather than colons, which Windows will not
  // accept in a filename — the same constraint that shapes the timestamp.
  EXPECT_EQ(AppendDurationToStem("Casper_side1", 2472.0),
            "Casper_side1_00H41M12S");
  EXPECT_EQ(AppendDurationToStem("Casper", 1.4), "Casper_00H00M01S");
  EXPECT_EQ(AppendDurationToStem("Casper", 3661.0), "Casper_01H01M01S");
}

TEST_F(NamingFieldsTest, ADurationThatIsNotOneIsSimplyNotAppended) {
  // A capture that recorded nothing has no length to put in a name, and the
  // moment this is asked is the moment a capture has just ended — the worst
  // possible place to introduce a failure.
  EXPECT_EQ(AppendDurationToStem("Casper", 0.0), "Casper");
  EXPECT_EQ(AppendDurationToStem("Casper", -5.0), "Casper");
}

}  // namespace
}  // namespace ddd::capture
