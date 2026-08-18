/************************************************************************

    test_logger.cpp

    T1 tests for the engine logging seam
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

#include "logger.h"

namespace ddd::capture {
namespace {

// Collects what a CallbackLogger emits, so a test can assert on the records
// rather than on side effects.
class RecordCollector {
 public:
  CallbackLogger::Sink Sink() {
    return [this](LogLevel level, const std::string& message) {
      levels_.push_back(level);
      messages_.push_back(message);
    };
  }

  const std::vector<LogLevel>& levels() const { return levels_; }
  const std::vector<std::string>& messages() const { return messages_; }

 private:
  std::vector<LogLevel> levels_;
  std::vector<std::string> messages_;
};

TEST(LogLevelNameTest, NamesEveryLevel) {
  EXPECT_STREQ(LogLevelName(LogLevel::kDebug), "debug");
  EXPECT_STREQ(LogLevelName(LogLevel::kInfo), "info");
  EXPECT_STREQ(LogLevelName(LogLevel::kWarning), "warning");
  EXPECT_STREQ(LogLevelName(LogLevel::kError), "error");
}

TEST(CallbackLoggerTest, ForwardsRecordsAtOrAboveTheMinimumLevel) {
  RecordCollector collector;
  CallbackLogger logger(collector.Sink(), LogLevel::kWarning);

  logger.Debug("debug");
  logger.Info("info");
  logger.Warning("warning");
  logger.Error("error");

  ASSERT_EQ(collector.levels().size(), 2U);
  EXPECT_EQ(collector.levels()[0], LogLevel::kWarning);
  EXPECT_EQ(collector.levels()[1], LogLevel::kError);
  EXPECT_EQ(collector.messages()[0], "warning");
  EXPECT_EQ(collector.messages()[1], "error");
}

TEST(CallbackLoggerTest, DefaultsToInfo) {
  RecordCollector collector;
  CallbackLogger logger(collector.Sink());

  EXPECT_EQ(logger.minimum_level(), LogLevel::kInfo);

  logger.Debug("dropped");
  logger.Info("kept");

  ASSERT_EQ(collector.messages().size(), 1U);
  EXPECT_EQ(collector.messages()[0], "kept");
}

TEST(CallbackLoggerTest, MinimumLevelCanBeLoweredAtRuntime) {
  RecordCollector collector;
  CallbackLogger logger(collector.Sink(), LogLevel::kInfo);

  logger.Debug("before");
  logger.SetMinimumLevel(LogLevel::kDebug);
  logger.Debug("after");

  ASSERT_EQ(collector.messages().size(), 1U);
  EXPECT_EQ(collector.messages()[0], "after");
}

TEST(CallbackLoggerTest, AnEmptySinkDiscardsRecords) {
  CallbackLogger logger(nullptr, LogLevel::kDebug);

  // The contract is that this is a no-op rather than a crash: a front end that
  // has not attached a sink yet still logs.
  logger.Debug("no sink");
  logger.Error("still no sink");

  SUCCEED();
}

TEST(CallbackLoggerTest, PreservesMessagesLoggedConcurrently) {
  // The engine logs from several threads, so the sink must never be entered
  // concurrently and no record may be lost.
  constexpr int kThreads = 4;
  constexpr int kRecordsPerThread = 200;

  std::vector<std::string> received;
  CallbackLogger logger(
      [&received](LogLevel, const std::string& message) {
        received.push_back(message);
      },
      LogLevel::kDebug);

  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (int thread_index = 0; thread_index < kThreads; ++thread_index) {
    threads.emplace_back([&logger, thread_index] {
      for (int record = 0; record < kRecordsPerThread; ++record) {
        logger.Info(std::to_string(thread_index) + ":" +
                    std::to_string(record));
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(received.size(),
            static_cast<std::size_t>(kThreads * kRecordsPerThread));
}

}  // namespace
}  // namespace ddd::capture
