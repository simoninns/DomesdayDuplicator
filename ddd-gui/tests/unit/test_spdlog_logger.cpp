/************************************************************************

    test_spdlog_logger.cpp

    T1 tests for the console and file log destinations
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "log_options.h"
#include "logger.h"
#include "spdlog_logger.h"

namespace ddd::capture {
namespace {

// The file is the half of this that can be asserted on: what a console sink
// writes goes to this process's own standard error, and a test that captured
// that would be testing spdlog rather than anything here. What is checked of
// the console half is whether it was installed at all, which is the decision
// this code makes.
class SpdlogLoggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    directory_ = std::filesystem::temp_directory_path() / "ddd-spdlog-test";
    std::filesystem::remove_all(directory_);
    std::filesystem::create_directories(directory_);
  }

  void TearDown() override { std::filesystem::remove_all(directory_); }

  std::filesystem::path LogFile() const { return directory_ / "capture.log"; }

  static std::string Contents(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
  }

  std::filesystem::path directory_;
};

TEST_F(SpdlogLoggerTest, WritesRecordsToTheFileItWasGiven) {
  LogConfig config;
  config.destination = LogDestination::kFile;
  config.file = LogFile().string();

  {
    SpdlogLogger logger(config);
    EXPECT_TRUE(logger.warnings().empty());
    EXPECT_TRUE(logger.writes_to_file());
    EXPECT_FALSE(logger.writes_to_console());

    logger.Info("capture started");
    logger.Error("sequence break");
  }

  const std::string written = Contents(LogFile());
  EXPECT_NE(written.find("capture started"), std::string::npos);
  EXPECT_NE(written.find("sequence break"), std::string::npos);

  // The level is part of every line, because a log read a week later has to
  // say which records were the bad news.
  EXPECT_NE(written.find("info"), std::string::npos);
  EXPECT_NE(written.find("error"), std::string::npos);
}

TEST_F(SpdlogLoggerTest, DropsRecordsBelowTheConfiguredLevel) {
  LogConfig config;
  config.level = LogLevel::kWarning;
  config.destination = LogDestination::kFile;
  config.file = LogFile().string();

  {
    SpdlogLogger logger(config);
    logger.Debug("dropped debug");
    logger.Info("dropped info");
    logger.Warning("kept warning");
  }

  const std::string written = Contents(LogFile());
  EXPECT_EQ(written.find("dropped debug"), std::string::npos);
  EXPECT_EQ(written.find("dropped info"), std::string::npos);
  EXPECT_NE(written.find("kept warning"), std::string::npos);
}

TEST_F(SpdlogLoggerTest, OffWritesNothingAtAll) {
  LogConfig config;
  config.level = LogLevel::kOff;
  config.destination = LogDestination::kFile;
  config.file = LogFile().string();

  {
    SpdlogLogger logger(config);
    logger.Error("not written");
  }

  // The file is still created — the sink opens it before anything is logged —
  // and stays empty, which is what "off" has to look like.
  ASSERT_TRUE(std::filesystem::exists(LogFile()));
  EXPECT_TRUE(Contents(LogFile()).empty());
}

// A message is text and not a format string. A device path or a JSON fragment
// carries braces, and formatting one would corrupt it or throw.
TEST_F(SpdlogLoggerTest, WritesMessagesContainingBracesVerbatim) {
  LogConfig config;
  config.destination = LogDestination::kFile;
  config.file = LogFile().string();

  {
    SpdlogLogger logger(config);
    logger.Info(R"(device {"vid": 4617} at {0})");
  }

  EXPECT_NE(Contents(LogFile()).find(R"(device {"vid": 4617} at {0})"),
            std::string::npos);
}

TEST_F(SpdlogLoggerTest, BothWritesToTheConsoleAndTheFile) {
  LogConfig config;
  config.destination = LogDestination::kBoth;
  config.file = LogFile().string();

  SpdlogLogger logger(config);

  EXPECT_TRUE(logger.writes_to_console());
  EXPECT_TRUE(logger.writes_to_file());
  EXPECT_TRUE(logger.warnings().empty());
}

// The ordinary case: no --log-file, so "both" is the console and nothing else.
TEST_F(SpdlogLoggerTest, BothWithNoFileIsJustTheConsole) {
  const SpdlogLogger logger{LogConfig{}};

  EXPECT_TRUE(logger.writes_to_console());
  EXPECT_FALSE(logger.writes_to_file());
  EXPECT_TRUE(logger.warnings().empty());
}

// The default destination. Records are accepted and dropped: the GUI's own Log
// panel is fed by the bridge above this and is unaffected, which is the whole
// point of the destination existing.
TEST_F(SpdlogLoggerTest, NoneWritesNowhereAtAll) {
  LogConfig config;
  config.destination = LogDestination::kNone;
  config.file = LogFile().string();

  {
    SpdlogLogger logger(config);
    EXPECT_FALSE(logger.writes_to_console());
    EXPECT_FALSE(logger.writes_to_file());
    EXPECT_TRUE(logger.warnings().empty());

    // Accepted rather than refused, and provably harmless: a front end hands
    // every record to this whatever the destination is.
    logger.Error("dropped on the floor");
  }

  EXPECT_FALSE(std::filesystem::exists(LogFile()));
}

// The fallback that puts the console back when a file could not be opened must
// not fire for the destination that asked for nothing.
TEST_F(SpdlogLoggerTest, NoneIsNotMistakenForAFailedFile) {
  LogConfig config;
  config.destination = LogDestination::kNone;

  const SpdlogLogger logger(config);

  EXPECT_FALSE(logger.writes_to_console());
  EXPECT_TRUE(logger.warnings().empty());
}

TEST_F(SpdlogLoggerTest, ConsoleLeavesAConfiguredFileAlone) {
  LogConfig config;
  config.destination = LogDestination::kConsole;
  config.file = LogFile().string();

  {
    SpdlogLogger logger(config);
    logger.Info("console only");
    EXPECT_TRUE(logger.writes_to_console());
    EXPECT_FALSE(logger.writes_to_file());
  }

  EXPECT_FALSE(std::filesystem::exists(LogFile()));
}

// Asking for the file alone and naming none leaves the console rather than
// silently discarding the log.
TEST_F(SpdlogLoggerTest, FileWithNoFileFallsBackToTheConsole) {
  LogConfig config;
  config.destination = LogDestination::kFile;

  const SpdlogLogger logger(config);

  EXPECT_TRUE(logger.writes_to_console());
  EXPECT_FALSE(logger.writes_to_file());
}

// A log file that cannot be opened must not stop the application starting. The
// directory stands in for every reason a path might not be writable, which is
// what a user typing one gets wrong.
TEST_F(SpdlogLoggerTest, ReportsAFileItCouldNotOpenAndKeepsTheConsole) {
  LogConfig config;
  config.destination = LogDestination::kFile;
  config.file = directory_.string();

  SpdlogLogger logger(config);

  EXPECT_FALSE(logger.writes_to_file());
  EXPECT_TRUE(logger.writes_to_console());
  ASSERT_EQ(logger.warnings().size(), 1U);
  // The message has to name the path, because a user who mistyped it needs to
  // see which path was tried.
  EXPECT_NE(logger.warnings().front().find(directory_.string()),
            std::string::npos);

  logger.Info("still logging");
}

TEST_F(SpdlogLoggerTest, SatisfiesTheEngineLoggingSeam) {
  LogConfig config;
  config.destination = LogDestination::kFile;
  config.file = LogFile().string();

  {
    SpdlogLogger logger(config);
    ILogger& seam = logger;
    seam.Warning("through the interface");
  }

  EXPECT_NE(Contents(LogFile()).find("through the interface"),
            std::string::npos);
}

}  // namespace
}  // namespace ddd::capture
