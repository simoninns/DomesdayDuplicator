/************************************************************************

    test_log_options.cpp

    T1 tests for the log level and destination names
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include "log_options.h"
#include "logger.h"

namespace ddd::capture {
namespace {

TEST(ParseLogLevelTest, TakesTheFourLevelsARecordCanCarry) {
  EXPECT_EQ(ParseLogLevel("debug"), LogLevel::kDebug);
  EXPECT_EQ(ParseLogLevel("info"), LogLevel::kInfo);
  EXPECT_EQ(ParseLogLevel("warn"), LogLevel::kWarning);
  EXPECT_EQ(ParseLogLevel("error"), LogLevel::kError);
}

// The vocabulary is wider than the engine's four levels so that a level named
// on another of the project's tools means something here rather than failing.
TEST(ParseLogLevelTest, MapsTheLevelsTheEngineDoesNotHaveOntoTheOnesItDoes) {
  EXPECT_EQ(ParseLogLevel("trace"), LogLevel::kDebug);
  EXPECT_EQ(ParseLogLevel("warning"), LogLevel::kWarning);
  EXPECT_EQ(ParseLogLevel("critical"), LogLevel::kError);
}

TEST(ParseLogLevelTest, OffIsALevelOfItsOwn) {
  EXPECT_EQ(ParseLogLevel("off"), LogLevel::kOff);
}

// A minimum level of kOff has to admit nothing, which is the whole of what the
// value is for. Checked here rather than only in the loggers, because it is a
// property of the ordering the enum promises.
TEST(ParseLogLevelTest, OffOutranksEveryLevelARecordCanCarry) {
  for (const LogLevel level : {LogLevel::kDebug, LogLevel::kInfo,
                               LogLevel::kWarning, LogLevel::kError}) {
    EXPECT_LT(static_cast<int>(level), static_cast<int>(LogLevel::kOff));
  }
}

TEST(ParseLogLevelTest, RefusesAnUnknownName) {
  EXPECT_FALSE(ParseLogLevel("verbose").has_value());
  EXPECT_FALSE(ParseLogLevel("").has_value());
  // Case-sensitive, so there is one spelling of each name rather than a set.
  EXPECT_FALSE(ParseLogLevel("Debug").has_value());
}

TEST(ParseLogDestinationTest, TakesTheThreeDestinations) {
  EXPECT_EQ(ParseLogDestination("console"), LogDestination::kConsole);
  EXPECT_EQ(ParseLogDestination("file"), LogDestination::kFile);
  EXPECT_EQ(ParseLogDestination("both"), LogDestination::kBoth);
}

TEST(ParseLogDestinationTest, RefusesAnythingElse) {
  EXPECT_FALSE(ParseLogDestination("stderr").has_value());
  EXPECT_FALSE(ParseLogDestination("Both").has_value());
  EXPECT_FALSE(ParseLogDestination("").has_value());
}

TEST(ParseLogDestinationTest, NamesRoundTripThroughTheParser) {
  for (const LogDestination destination :
       {LogDestination::kConsole, LogDestination::kFile,
        LogDestination::kBoth}) {
    EXPECT_EQ(ParseLogDestination(LogDestinationName(destination)),
              destination);
  }
}

TEST(ResolveLogSinksTest, ConsoleIgnoresAConfiguredFile) {
  const LogSinkSelection selection =
      ResolveLogSinks(LogDestination::kConsole, true);

  EXPECT_TRUE(selection.console);
  EXPECT_FALSE(selection.file);
}

TEST(ResolveLogSinksTest, BothUsesTheFileOnlyWhenThereIsOne) {
  const LogSinkSelection with_file =
      ResolveLogSinks(LogDestination::kBoth, true);
  EXPECT_TRUE(with_file.console);
  EXPECT_TRUE(with_file.file);

  const LogSinkSelection without_file =
      ResolveLogSinks(LogDestination::kBoth, false);
  EXPECT_TRUE(without_file.console);
  EXPECT_FALSE(without_file.file);
}

TEST(ResolveLogSinksTest, FileSilencesTheConsole) {
  const LogSinkSelection selection =
      ResolveLogSinks(LogDestination::kFile, true);

  EXPECT_FALSE(selection.console);
  EXPECT_TRUE(selection.file);
}

// Asking for the file alone and naming no file would otherwise resolve to no
// sinks at all, which would discard every record without saying so.
TEST(ResolveLogSinksTest, FileWithNoFileKeepsTheConsole) {
  const LogSinkSelection selection =
      ResolveLogSinks(LogDestination::kFile, false);

  EXPECT_TRUE(selection.console);
  EXPECT_FALSE(selection.file);
}

}  // namespace
}  // namespace ddd::capture
