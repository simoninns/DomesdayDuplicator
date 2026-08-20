/************************************************************************

    test_capture_cli.cpp

    T1 tests for the capture options the command line accepts
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCommandLineParser>
#include <QString>
#include <QStringList>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "capture_cli.h"
#include "capture_format.h"
#include "capture_settings.h"

namespace ddd::gui {
namespace {

// What a run of the command line produced: whether Qt's parser accepted the
// tokens at all, and then what this application made of them. The two are
// separate failures — an unknown switch is Qt's complaint, a switch that cannot
// mean what it says is this application's — and a test that conflated them
// would pass when the wrong half worked.
struct Parsed {
  bool accepted = false;
  CaptureCliOptions options;
  QString error;

  bool ok() const { return accepted && error.isEmpty(); }
};

Parsed Parse(const QStringList& arguments) {
  QCommandLineParser parser;
  const CaptureCliOptionSet set = AddCaptureCliOptions(parser);

  QStringList line;
  line.append(QStringLiteral("ddd-gui"));
  line.append(arguments);

  Parsed parsed;
  parsed.accepted = parser.parse(line);
  if (!parsed.accepted) {
    parsed.error = parser.errorText();
    return parsed;
  }

  const CaptureCliParseResult result = ParseCaptureCliOptions(parser, set);
  parsed.options = result.options;
  parsed.error = result.error;
  return parsed;
}

// WantsCoreApplication() reads the raw arguments, so a test has to hand it the
// same thing main() gets rather than a QStringList.
bool WantsCore(const std::vector<std::string>& arguments) {
  std::vector<std::string> storage;
  storage.emplace_back("ddd-gui");
  storage.insert(storage.end(), arguments.begin(), arguments.end());

  std::vector<char*> line;
  line.reserve(storage.size());
  for (std::string& token : storage) {
    line.push_back(token.data());
  }

  return WantsCoreApplication(static_cast<int>(line.size()), line.data());
}

// --- What the switches mean ----------------------------------------------

TEST(CaptureCliTest, ACommandLineWithNoneOfThemAsksForNothing) {
  const Parsed parsed = Parse({});

  ASSERT_TRUE(parsed.ok()) << parsed.error.toStdString();
  EXPECT_FALSE(parsed.options.start_capture);
  EXPECT_FALSE(parsed.options.stop_capture);
  EXPECT_FALSE(parsed.options.headless);
  EXPECT_FALSE(parsed.options.HasAttributeOverrides());
}

// The window still opens. This is the case the feature was asked for: a script
// starts the capture at the same moment it starts recording audio somewhere
// else, and the user watches both from the application they already had open.
TEST(CaptureCliTest, StartCaptureOnItsOwnKeepsTheWindow) {
  const Parsed parsed = Parse({QStringLiteral("--start-capture")});

  ASSERT_TRUE(parsed.ok()) << parsed.error.toStdString();
  EXPECT_TRUE(parsed.options.start_capture);
  EXPECT_FALSE(parsed.options.headless);
}

TEST(CaptureCliTest, HeadlessWithoutStartCaptureIsRefused) {
  const Parsed parsed = Parse({QStringLiteral("--headless")});

  EXPECT_TRUE(parsed.accepted);
  EXPECT_TRUE(parsed.error.contains(QStringLiteral("--start-capture")))
      << parsed.error.toStdString();
}

TEST(CaptureCliTest, HeadlessWithStartCaptureIsTheScriptedRun) {
  const Parsed parsed =
      Parse({QStringLiteral("--headless"), QStringLiteral("--start-capture")});

  ASSERT_TRUE(parsed.ok()) << parsed.error.toStdString();
  EXPECT_TRUE(parsed.options.headless);
  EXPECT_TRUE(parsed.options.start_capture);
}

// --stop-capture is a message to a process that was set up long ago. Anything
// beside it is an instruction with nowhere to go.
TEST(CaptureCliTest, StopCaptureBesideStartCaptureIsRefused) {
  const Parsed parsed = Parse(
      {QStringLiteral("--stop-capture"), QStringLiteral("--start-capture")});

  EXPECT_TRUE(parsed.accepted);
  EXPECT_FALSE(parsed.error.isEmpty());
}

TEST(CaptureCliTest, StopCaptureBesideAnAttributeIsRefused) {
  const Parsed parsed =
      Parse({QStringLiteral("--stop-capture"), QStringLiteral("--capture-name"),
             QStringLiteral("disc-1")});

  EXPECT_TRUE(parsed.accepted);
  EXPECT_FALSE(parsed.error.isEmpty());
}

// Attributes with no start command are not a mistake: they are "set this up for
// me and I will press the button myself", and they populate the window.
TEST(CaptureCliTest, AttributesWithNoStartCommandAreForTheWindow) {
  const Parsed parsed =
      Parse({QStringLiteral("--capture-name"), QStringLiteral("disc-1"),
             QStringLiteral("--duration-limit"), QStringLiteral("90")});

  ASSERT_TRUE(parsed.ok()) << parsed.error.toStdString();
  EXPECT_FALSE(parsed.options.start_capture);
  EXPECT_TRUE(parsed.options.HasAttributeOverrides());
}

// --- Values ----------------------------------------------------------------

TEST(CaptureCliTest, TheTapeRateIsTheDecimatedOne) {
  const Parsed parsed =
      Parse({QStringLiteral("--sample-rate"), QStringLiteral("20")});

  ASSERT_TRUE(parsed.ok()) << parsed.error.toStdString();
  EXPECT_EQ(parsed.options.decimation_factor,
            std::optional<int>(capture::kTapeDecimationFactor));
}

TEST(CaptureCliTest, TheDiscRateIsNoDecimationAtAll) {
  const Parsed parsed =
      Parse({QStringLiteral("--sample-rate"), QStringLiteral("40")});

  ASSERT_TRUE(parsed.ok()) << parsed.error.toStdString();
  EXPECT_EQ(parsed.options.decimation_factor,
            std::optional<int>(capture::kUndecimatedFactor));
}

// The message names the rates that would have worked. A script author reading
// it in a log has no application in front of them to go and look at.
TEST(CaptureCliTest, ARateTheDeviceCannotCaptureAtIsRefused) {
  const Parsed parsed =
      Parse({QStringLiteral("--sample-rate"), QStringLiteral("30")});

  EXPECT_TRUE(parsed.accepted);
  EXPECT_TRUE(parsed.error.contains(QStringLiteral("40")))
      << parsed.error.toStdString();
  EXPECT_TRUE(parsed.error.contains(QStringLiteral("20")))
      << parsed.error.toStdString();
}

TEST(CaptureCliTest, ARateThatIsNotANumberIsRefused) {
  const Parsed parsed =
      Parse({QStringLiteral("--sample-rate"), QStringLiteral("fast")});

  EXPECT_TRUE(parsed.accepted);
  EXPECT_FALSE(parsed.error.isEmpty());
}

TEST(CaptureCliTest, ADurationLimitIsTakenInSeconds) {
  const Parsed parsed =
      Parse({QStringLiteral("--duration-limit"), QStringLiteral("3600")});

  ASSERT_TRUE(parsed.ok()) << parsed.error.toStdString();
  EXPECT_EQ(parsed.options.duration_limit_seconds, std::optional<int>(3600));
}

// Zero is how the settings spell "no limit", and it is refused here rather than
// read as one: a script that computed a limit of zero has a bug, and a capture
// that then ran until the disk filled would be a long way from where the bug
// is. Leaving the switch out is how a script asks for no limit.
TEST(CaptureCliTest, ADurationLimitOfZeroIsRefused) {
  const Parsed parsed =
      Parse({QStringLiteral("--duration-limit"), QStringLiteral("0")});

  EXPECT_TRUE(parsed.accepted);
  EXPECT_TRUE(parsed.error.contains(QStringLiteral("leave it out")))
      << parsed.error.toStdString();
}

TEST(CaptureCliTest, ADurationLimitBeyondADayIsRefused) {
  const Parsed parsed = Parse(
      {QStringLiteral("--duration-limit"),
       QString::number(CaptureSettings::kMaximumDurationLimitSeconds + 1)});

  EXPECT_TRUE(parsed.accepted);
  EXPECT_FALSE(parsed.error.isEmpty());
}

TEST(CaptureCliTest, BothOutputFormatsAreAccepted) {
  const Parsed flac =
      Parse({QStringLiteral("--output-format"), QStringLiteral("flac")});
  ASSERT_TRUE(flac.ok()) << flac.error.toStdString();
  EXPECT_EQ(flac.options.output_format,
            std::optional<capture::CaptureOutputFormat>(
                capture::CaptureOutputFormat::kFlac));

  const Parsed raw =
      Parse({QStringLiteral("--output-format"), QStringLiteral("s16")});
  ASSERT_TRUE(raw.ok()) << raw.error.toStdString();
  EXPECT_EQ(raw.options.output_format,
            std::optional<capture::CaptureOutputFormat>(
                capture::CaptureOutputFormat::kSigned16Bit));
}

// The settings loader reads anything it does not recognise as FLAC, because a
// settings file naming a format this build does not have should still produce a
// working capture. A command line is the opposite case: it was typed for this
// run, and a script that asked for raw samples and silently got FLAC would find
// out when something downstream failed to read the file.
TEST(CaptureCliTest, AFormatThisBuildDoesNotWriteIsRefusedRatherThanIgnored) {
  const Parsed parsed =
      Parse({QStringLiteral("--output-format"), QStringLiteral("ldf")});

  EXPECT_TRUE(parsed.accepted);
  EXPECT_TRUE(parsed.error.contains(QStringLiteral("ldf")))
      << parsed.error.toStdString();
}

TEST(CaptureCliTest, AnEmptyCaptureNameIsRefused) {
  const Parsed parsed = Parse({QStringLiteral("--capture-name"), QString()});

  EXPECT_TRUE(parsed.accepted);
  EXPECT_FALSE(parsed.error.isEmpty());
}

// The window's name field would take this and fail when the file was opened,
// which for a scripted run means a failure minutes later and nothing captured.
TEST(CaptureCliTest, ACaptureNameHoldingAPathIsRefused) {
  const Parsed parsed = Parse(
      {QStringLiteral("--capture-name"), QStringLiteral("disc-1/side-a")});

  EXPECT_TRUE(parsed.accepted);
  EXPECT_TRUE(parsed.error.contains(QStringLiteral("--capture-directory")))
      << parsed.error.toStdString();
}

// --- The capture folder ----------------------------------------------------

class CaptureCliDirectoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    directory_ = std::filesystem::temp_directory_path() /
                 (std::string("ddd-capture-cli-") + info->name());
    std::filesystem::remove_all(directory_);
    std::filesystem::create_directories(directory_);
  }

  void TearDown() override { std::filesystem::remove_all(directory_); }

  QString PathOf(const std::filesystem::path& path) const {
    return QString::fromStdString(path.string());
  }

  std::filesystem::path directory_;
};

TEST_F(CaptureCliDirectoryTest, AFolderThatIsThereIsAccepted) {
  const Parsed parsed =
      Parse({QStringLiteral("--capture-directory"), PathOf(directory_)});

  ASSERT_TRUE(parsed.ok()) << parsed.error.toStdString();
  EXPECT_EQ(parsed.options.capture_directory,
            std::optional<QString>(PathOf(directory_)));
}

// A capture started from the window makes its folder, so one that is not there
// yet is an ordinary instruction rather than a mistake: a script naming a
// folder per disc should not have to create it first.
TEST_F(CaptureCliDirectoryTest, AFolderThatIsNotThereYetIsAccepted) {
  const Parsed parsed = Parse(
      {QStringLiteral("--capture-directory"), PathOf(directory_ / "disc-042")});

  EXPECT_TRUE(parsed.ok()) << parsed.error.toStdString();
}

// A file where a folder was named is the one case nothing downstream can
// recover from, so it is the one case refused here.
TEST_F(CaptureCliDirectoryTest, AFileWhereAFolderWasNamedIsRefused) {
  const std::filesystem::path file = directory_ / "not-a-folder";
  {
    std::ofstream stream(file);
    stream << "occupied";
  }

  const Parsed parsed =
      Parse({QStringLiteral("--capture-directory"), PathOf(file)});

  EXPECT_TRUE(parsed.accepted);
  EXPECT_TRUE(parsed.error.contains(QStringLiteral("not a folder")))
      << parsed.error.toStdString();
}

// --- Laying them over the settings -----------------------------------------

TEST(CaptureCliTest, OnlyWhatWasNamedChanges) {
  CaptureSettings settings;
  settings.capture_directory = QStringLiteral("/captures");
  settings.capture_name = QStringLiteral("saved-name");
  settings.decimation_factor = capture::kUndecimatedFactor;
  settings.duration_limit_seconds = 0;
  settings.output_format = capture::CaptureOutputFormat::kFlac;
  settings.compression_level = 3;

  CaptureCliOptions options;
  options.capture_name = QStringLiteral("disc-1");
  options.duration_limit_seconds = 120;

  ApplyCliOverrides(settings, options);

  EXPECT_EQ(settings.capture_name, QStringLiteral("disc-1"));
  EXPECT_EQ(settings.duration_limit_seconds, 120);

  // Everything the command line said nothing about is the user's saved answer,
  // untouched.
  EXPECT_EQ(settings.capture_directory, QStringLiteral("/captures"));
  EXPECT_EQ(settings.decimation_factor, capture::kUndecimatedFactor);
  EXPECT_EQ(settings.output_format, capture::CaptureOutputFormat::kFlac);
  EXPECT_EQ(settings.compression_level, 3);
}

TEST(CaptureCliTest, ACommandLineThatNamedNothingChangesNothing) {
  const CaptureSettings saved;
  CaptureSettings settings = saved;

  ApplyCliOverrides(settings, CaptureCliOptions{});

  EXPECT_EQ(settings, saved);
}

// --- Which application object to build -------------------------------------

// Read before any application object exists, so that a machine with no display
// can run a scripted capture at all.
TEST(CaptureCliTest, TheScriptedModesNeedNoDisplay) {
  EXPECT_TRUE(WantsCore({"--headless", "--start-capture"}));
  EXPECT_TRUE(WantsCore({"--stop-capture"}));
}

TEST(CaptureCliTest, TheWindowedModesDo) {
  EXPECT_FALSE(WantsCore({}));
  EXPECT_FALSE(WantsCore({"--start-capture"}));
  EXPECT_FALSE(WantsCore({"--capture-name", "disc-1"}));
}

// Qt's parser takes a long name with one dash as well as two, so this has to
// recognise both or a spelling the application accepts would build the wrong
// application object and then fail to start.
TEST(CaptureCliTest, OneDashIsTheSameSwitch) {
  EXPECT_TRUE(WantsCore({"-headless", "-start-capture"}));
  EXPECT_TRUE(WantsCore({"-stop-capture"}));
}

TEST(CaptureCliTest, NothingAfterABareDoubleDashIsASwitch) {
  EXPECT_FALSE(WantsCore({"--", "--headless"}));
}

}  // namespace
}  // namespace ddd::gui
