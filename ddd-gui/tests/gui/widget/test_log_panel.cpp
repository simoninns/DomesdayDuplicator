/************************************************************************

    test_log_panel.cpp

    T1 tests for the log panel's level chooser and copying
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QItemSelectionModel>
#include <QListView>
#include <QString>
#include <QStringList>

#include "application_logger.h"
#include "log_message_model.h"
#include "log_panel.h"
#include "logger.h"

// The model's own behaviour — the cap, the formatting of a record — is covered
// in tests/gui/unit/test_log_message_model.cpp. What is left here is what only
// the widget can answer: that the chooser moves the logger's threshold, and
// that a selection reaches the clipboard.

namespace ddd::gui {
namespace {

QComboBox* LevelCombo(const LogPanel& panel) {
  return panel.findChild<QComboBox*>(QLatin1String(LogPanel::kLevelComboName));
}

QListView* View(const LogPanel& panel) { return panel.findChild<QListView*>(); }

QAction* ActionNamed(const LogPanel& panel, const char* name) {
  return panel.findChild<QAction*>(QLatin1String(name));
}

// Appends `count` records, oldest first, as the logger's signal would.
void AppendRecords(LogMessageModel* model, int count) {
  for (int i = 0; i < count; ++i) {
    model->Append(static_cast<int>(capture::LogLevel::kInfo),
                  QStringLiteral("00:00:0%1.000").arg(i),
                  QStringLiteral("record %1").arg(i));
  }
}

// --- The level chooser ---------------------------------------------------

TEST(LogPanelTest, TheChooserStartsWhereTheLoggerAlreadyIs) {
  LogMessageModel model;
  ApplicationLogger logger;
  logger.SetMinimumLevel(capture::LogLevel::kWarning);

  LogPanel panel(&model, &logger);

  EXPECT_EQ(LevelCombo(panel)->currentData().toInt(),
            static_cast<int>(capture::LogLevel::kWarning));
}

TEST(LogPanelTest, ChoosingALevelMovesTheLoggersThreshold) {
  LogMessageModel model;
  ApplicationLogger logger;
  logger.SetMinimumLevel(capture::LogLevel::kInfo);

  LogPanel panel(&model, &logger);
  QComboBox* const level = LevelCombo(panel);

  const int debug =
      level->findData(static_cast<int>(capture::LogLevel::kDebug));
  ASSERT_GE(debug, 0);
  level->setCurrentIndex(debug);

  EXPECT_EQ(logger.minimum_level(), capture::LogLevel::kDebug);
}

TEST(LogPanelTest, EveryLevelTheCommandLineAcceptsIsOffered) {
  LogMessageModel model;
  ApplicationLogger logger;
  LogPanel panel(&model, &logger);

  QComboBox* const level = LevelCombo(panel);
  for (const capture::LogLevel wanted :
       {capture::LogLevel::kDebug, capture::LogLevel::kInfo,
        capture::LogLevel::kWarning, capture::LogLevel::kError,
        capture::LogLevel::kOff}) {
    EXPECT_GE(level->findData(static_cast<int>(wanted)), 0)
        << "no entry for " << capture::LogLevelName(wanted);
  }
}

TEST(LogPanelTest, TheChooserRaisedPastARecordDropsIt) {
  LogMessageModel model;
  ApplicationLogger logger;
  LogPanel panel(&model, &logger);
  QObject::connect(&logger, &ApplicationLogger::RecordLogged, &model,
                   &LogMessageModel::Append);

  QComboBox* const level = LevelCombo(panel);
  level->setCurrentIndex(
      level->findData(static_cast<int>(capture::LogLevel::kError)));

  logger.Info("not this one");
  EXPECT_EQ(model.rowCount(QModelIndex()), 0);

  logger.Error("but this one");
  EXPECT_EQ(model.rowCount(QModelIndex()), 1);
}

TEST(LogPanelTest, WithNoLoggerTheChooserSaysItCannotBeUsed) {
  LogMessageModel model;
  LogPanel panel(&model, nullptr);

  EXPECT_FALSE(LevelCombo(panel)->isEnabled());
}

// --- Copying -------------------------------------------------------------

TEST(LogPanelTest, CopyingPutsTheSelectedRecordsOnTheClipboard) {
  LogMessageModel model;
  ApplicationLogger logger;
  LogPanel panel(&model, &logger);
  AppendRecords(&model, 3);

  QListView* const view = View(panel);
  ASSERT_NE(view, nullptr);
  view->selectionModel()->select(model.index(1, 0),
                                 QItemSelectionModel::Select);

  QApplication::clipboard()->clear();
  ActionNamed(panel, LogPanel::kCopyActionName)->trigger();

  const QString copied = QApplication::clipboard()->text();
  EXPECT_TRUE(copied.contains(QStringLiteral("record 1")))
      << copied.toStdString();
  EXPECT_FALSE(copied.contains(QStringLiteral("record 0")))
      << copied.toStdString();
}

TEST(LogPanelTest, CopiedRecordsAreOldestFirstWhicheverWayTheyWerePicked) {
  LogMessageModel model;
  ApplicationLogger logger;
  LogPanel panel(&model, &logger);
  AppendRecords(&model, 3);

  QListView* const view = View(panel);
  // Picked bottom-up, which is what a shift-click upwards produces.
  view->selectionModel()->select(model.index(2, 0),
                                 QItemSelectionModel::Select);
  view->selectionModel()->select(model.index(0, 0),
                                 QItemSelectionModel::Select);

  ActionNamed(panel, LogPanel::kCopyActionName)->trigger();

  const QStringList lines = QApplication::clipboard()->text().split(
      QLatin1Char('\n'), Qt::SkipEmptyParts);
  ASSERT_EQ(lines.size(), 2);
  EXPECT_TRUE(lines.at(0).endsWith(QStringLiteral("record 0")));
  EXPECT_TRUE(lines.at(1).endsWith(QStringLiteral("record 2")));
}

TEST(LogPanelTest, CopyingNothingLeavesTheClipboardAlone) {
  LogMessageModel model;
  ApplicationLogger logger;
  LogPanel panel(&model, &logger);
  AppendRecords(&model, 2);

  QApplication::clipboard()->setText(QStringLiteral("something else"));
  ActionNamed(panel, LogPanel::kCopyActionName)->trigger();

  EXPECT_EQ(QApplication::clipboard()->text(),
            QStringLiteral("something else"));
}

TEST(LogPanelTest, SelectAllSelectsEveryRecord) {
  LogMessageModel model;
  ApplicationLogger logger;
  LogPanel panel(&model, &logger);
  AppendRecords(&model, 4);

  ActionNamed(panel, LogPanel::kSelectAllActionName)->trigger();

  EXPECT_EQ(View(panel)->selectionModel()->selectedIndexes().size(), 4);
}

}  // namespace
}  // namespace ddd::gui
