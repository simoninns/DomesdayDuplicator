/************************************************************************

    test_log_message_model.cpp

    T1 tests for the bounded log record model
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QAbstractItemModelTester>
#include <QSignalSpy>

#include "log_message_model.h"
#include "logger.h"

namespace ddd::gui {
namespace {

QString MessageAt(const LogMessageModel& model, int row) {
  return model.data(model.index(row, 0), LogMessageModel::kMessageRole)
      .toString();
}

TEST(LogMessageModelTest, StartsEmpty) {
  LogMessageModel model;
  EXPECT_EQ(model.rowCount(QModelIndex()), 0);
}

TEST(LogMessageModelTest, AppendsRecordsInOrder) {
  LogMessageModel model;

  model.Append(static_cast<int>(capture::LogLevel::kInfo),
               QStringLiteral("00:00:01.000"), QStringLiteral("first"));
  model.Append(static_cast<int>(capture::LogLevel::kError),
               QStringLiteral("00:00:02.000"), QStringLiteral("second"));

  ASSERT_EQ(model.rowCount(QModelIndex()), 2);
  EXPECT_EQ(MessageAt(model, 0), QStringLiteral("first"));
  EXPECT_EQ(MessageAt(model, 1), QStringLiteral("second"));
}

TEST(LogMessageModelTest, ExposesLevelAndTimestampSeparately) {
  LogMessageModel model;
  model.Append(static_cast<int>(capture::LogLevel::kWarning),
               QStringLiteral("12:34:56.789"), QStringLiteral("careful"));

  const QModelIndex index = model.index(0, 0);
  EXPECT_EQ(model.data(index, LogMessageModel::kLevelRole).toInt(),
            static_cast<int>(capture::LogLevel::kWarning));
  EXPECT_EQ(model.data(index, LogMessageModel::kTimestampRole).toString(),
            QStringLiteral("12:34:56.789"));

  // The display line carries all three, so a plain view is still readable.
  const QString display = model.data(index, Qt::DisplayRole).toString();
  EXPECT_TRUE(display.contains(QStringLiteral("12:34:56.789")));
  EXPECT_TRUE(display.contains(QStringLiteral("WARNING")));
  EXPECT_TRUE(display.contains(QStringLiteral("careful")));
}

TEST(LogMessageModelTest, DropsOldestRecordsAtTheCap) {
  // A capture runs for hours and logs throughout; the model must not grow
  // without limit, and must drop from the old end rather than the new.
  constexpr int kCap = 4;
  LogMessageModel model(nullptr, kCap);

  for (int record = 0; record < 10; ++record) {
    model.Append(static_cast<int>(capture::LogLevel::kInfo),
                 QStringLiteral("00:00:00.000"),
                 QStringLiteral("record %1").arg(record));
  }

  ASSERT_EQ(model.rowCount(QModelIndex()), kCap);
  EXPECT_EQ(MessageAt(model, 0), QStringLiteral("record 6"));
  EXPECT_EQ(MessageAt(model, kCap - 1), QStringLiteral("record 9"));
}

TEST(LogMessageModelTest, NeverReportsMoreRowsThanTheCap) {
  // Row counts are read by views in response to the model's own signals, so a
  // moment where the model is over its cap would be visible to them.
  constexpr int kCap = 3;
  LogMessageModel model(nullptr, kCap);

  int observed_maximum = 0;
  QObject::connect(&model, &QAbstractItemModel::rowsInserted, &model,
                   [&model, &observed_maximum](const QModelIndex&, int, int) {
                     observed_maximum = std::max(observed_maximum,
                                                 model.rowCount(QModelIndex()));
                   });

  for (int record = 0; record < 20; ++record) {
    model.Append(static_cast<int>(capture::LogLevel::kInfo),
                 QStringLiteral("00:00:00.000"), QStringLiteral("x"));
  }

  EXPECT_EQ(observed_maximum, kCap);
}

TEST(LogMessageModelTest, ClearRemovesEverything) {
  LogMessageModel model;
  model.Append(static_cast<int>(capture::LogLevel::kInfo),
               QStringLiteral("00:00:00.000"), QStringLiteral("gone"));

  model.Clear();

  EXPECT_EQ(model.rowCount(QModelIndex()), 0);
}

TEST(LogMessageModelTest, ClearOnAnEmptyModelDoesNotResetIt) {
  LogMessageModel model;
  const QSignalSpy reset_spy(&model, &QAbstractItemModel::modelReset);

  model.Clear();

  EXPECT_EQ(reset_spy.count(), 0);
}

TEST(LogMessageModelTest, RejectsOutOfRangeIndices) {
  LogMessageModel model;
  EXPECT_FALSE(model.data(QModelIndex(), Qt::DisplayRole).isValid());
  EXPECT_FALSE(model.data(model.index(0, 0), Qt::DisplayRole).isValid());
}

TEST(LogMessageModelTest, SatisfiesTheModelContract) {
  // Qt's own conformance checker: it asserts on inconsistent parent/child,
  // row-count and signal behaviour that a hand-written test would miss.
  LogMessageModel model;
  QAbstractItemModelTester tester(
      &model, QAbstractItemModelTester::FailureReportingMode::Warning);

  for (int record = 0; record < 5; ++record) {
    model.Append(static_cast<int>(capture::LogLevel::kInfo),
                 QStringLiteral("00:00:00.000"), QStringLiteral("x"));
  }
  model.Clear();

  SUCCEED();
}

}  // namespace
}  // namespace ddd::gui
