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
#include <thread>

#include "application_logger.h"
#include "log_message_model.h"
#include "logger.h"

namespace ddd::gui {
namespace {

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
