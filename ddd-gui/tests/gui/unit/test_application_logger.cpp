/************************************************************************

    test_application_logger.cpp

    T1 tests for the engine-to-GUI logging bridge
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "application_logger.h"
#include "log_message_model.h"
#include "logger.h"

namespace ddd::gui {
namespace {

// Stands in for the console and file destinations, which is all the bridge
// knows about them: it hands records to an ILogger and never asks what it is.
class RecordingMirror : public capture::ILogger {
 public:
  void Log(capture::LogLevel level, std::string_view message) override {
    levels_.push_back(level);
    messages_.emplace_back(message);
  }

  const std::vector<capture::LogLevel>& levels() const { return levels_; }
  const std::vector<std::string>& messages() const { return messages_; }

 private:
  std::vector<capture::LogLevel> levels_;
  std::vector<std::string> messages_;
};

TEST(ApplicationLoggerTest, EmitsARecordPerAdmittedMessage) {
  ApplicationLogger logger;
  const QSignalSpy spy(&logger, &ApplicationLogger::RecordLogged);

  logger.Info("hello");

  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.at(0).at(0).toInt(),
            static_cast<int>(capture::LogLevel::kInfo));
  EXPECT_EQ(spy.at(0).at(2).toString(), QStringLiteral("hello"));
  // Stamped when logged, so the record can be correlated with the capture.
  EXPECT_FALSE(spy.at(0).at(1).toString().isEmpty());
}

TEST(ApplicationLoggerTest, DefaultsToInfoAndDropsDebug) {
  ApplicationLogger logger;
  EXPECT_EQ(logger.minimum_level(), capture::LogLevel::kInfo);

  const QSignalSpy spy(&logger, &ApplicationLogger::RecordLogged);
  logger.Debug("dropped");

  EXPECT_EQ(spy.count(), 0);
}

TEST(ApplicationLoggerTest, DebugRecordsSurviveOnceTheLevelIsLowered) {
  // This is what --debug does.
  ApplicationLogger logger;
  logger.SetMinimumLevel(capture::LogLevel::kDebug);

  const QSignalSpy spy(&logger, &ApplicationLogger::RecordLogged);
  logger.Debug("kept");

  EXPECT_EQ(spy.count(), 1);
}

TEST(ApplicationLoggerTest, PreservesUtf8Messages) {
  ApplicationLogger logger;
  const QSignalSpy spy(&logger, &ApplicationLogger::RecordLogged);

  logger.Error("40 Msps — sequence break at ±1 sample");

  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.at(0).at(2).toString(),
            QStringLiteral("40 Msps — sequence break at ±1 sample"));
}

TEST(ApplicationLoggerTest, RecordsFromAnotherThreadReachTheModelQueued) {
  // The bridge exists for exactly this: an engine thread logs, and the model —
  // which is GUI-thread-only — must be touched by this thread and no other.
  // If the connection were direct, the append below would happen on the worker
  // thread and the count would be non-zero before any event is processed.
  ApplicationLogger logger;
  LogMessageModel model;
  QObject::connect(&logger, &ApplicationLogger::RecordLogged, &model,
                   &LogMessageModel::Append);

  std::thread worker([&logger] { logger.Info("from a worker thread"); });
  worker.join();

  EXPECT_EQ(model.rowCount(QModelIndex()), 0);

  QCoreApplication::processEvents();

  ASSERT_EQ(model.rowCount(QModelIndex()), 1);
  EXPECT_EQ(
      model.data(model.index(0, 0), LogMessageModel::kMessageRole).toString(),
      QStringLiteral("from a worker thread"));
}

// The panel and the console have to carry the same log. A record that reaches
// one and not the other is the failure this bridge exists to prevent: a user
// reading the panel and a developer reading the file would be looking at two
// different accounts of the same run.
TEST(ApplicationLoggerTest, MirrorsEveryShownRecordToTheConsoleAndFile) {
  RecordingMirror mirror;
  ApplicationLogger logger(&mirror);
  const QSignalSpy spy(&logger, &ApplicationLogger::RecordLogged);

  logger.Info("shown and written");

  EXPECT_EQ(spy.count(), 1);
  ASSERT_EQ(mirror.messages().size(), 1U);
  EXPECT_EQ(mirror.messages().front(), "shown and written");
  EXPECT_EQ(mirror.levels().front(), capture::LogLevel::kInfo);
}

TEST(ApplicationLoggerTest, RecordsBelowTheLevelReachNeitherDestination) {
  RecordingMirror mirror;
  ApplicationLogger logger(&mirror);
  const QSignalSpy spy(&logger, &ApplicationLogger::RecordLogged);

  logger.Debug("dropped by the level");

  EXPECT_EQ(spy.count(), 0);
  EXPECT_TRUE(mirror.messages().empty());
}

// A front end with nowhere to mirror to still logs to its panel. This is the
// shape every other test here uses, and the one the widget tests construct.
TEST(ApplicationLoggerTest, WorksWithNoMirrorAtAll) {
  ApplicationLogger logger;
  const QSignalSpy spy(&logger, &ApplicationLogger::RecordLogged);

  logger.Info("panel only");

  EXPECT_EQ(spy.count(), 1);
}

TEST(ApplicationLoggerTest, SatisfiesTheEngineLoggingSeam) {
  // The engine only ever sees an ILogger, so the bridge has to work through
  // that interface rather than through its own type.
  ApplicationLogger logger;
  capture::ILogger& seam = logger;

  const QSignalSpy spy(&logger, &ApplicationLogger::RecordLogged);
  seam.Warning("through the interface");

  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.at(0).at(0).toInt(),
            static_cast<int>(capture::LogLevel::kWarning));
}

}  // namespace
}  // namespace ddd::gui
