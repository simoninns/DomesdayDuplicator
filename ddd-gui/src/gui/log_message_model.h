/************************************************************************

    log_message_model.h

    Bounded model of log records, for the Log panel
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QAbstractListModel>
#include <QString>
#include <deque>

#include "logger.h"

namespace ddd::gui {

// The records shown in the Log panel, held in a bounded ring so that a session
// running for hours cannot grow the model without limit. Oldest records are
// dropped once the cap is reached.
//
// Thread-safety: NOT thread-safe. GUI thread only. Records that originate on an
// engine thread reach it through a queued signal (see ApplicationLogger), which
// is what keeps that true.
class LogMessageModel : public QAbstractListModel {
  Q_OBJECT

 public:
  struct Record {
    QString timestamp;
    capture::LogLevel level;
    QString message;
  };

  // Roles beyond DisplayRole, so a delegate or view can colour by severity
  // without re-parsing the formatted line.
  enum Roles {
    kLevelRole = Qt::UserRole + 1,
    kMessageRole,
    kTimestampRole,
  };

  static constexpr int kDefaultMaximumRecords = 5000;

  explicit LogMessageModel(QObject* parent = nullptr,
                           int maximum_records = kDefaultMaximumRecords);

  // `parent` carries no default, unlike the base declaration: a default
  // argument on a virtual is bound statically, so a derived one that ever
  // disagreed with the base would silently change meaning with the static type
  // of the pointer. Views call through QAbstractItemModel and still get the
  // base's default; direct callers pass QModelIndex().
  int rowCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  int maximum_records() const { return maximum_records_; }

 public slots:
  // Appends a record, dropping the oldest if the cap has been reached.
  void Append(int level, const QString& timestamp, const QString& message);

  void Clear();

 private:
  std::deque<Record> records_;
  int maximum_records_;
};

}  // namespace ddd::gui
