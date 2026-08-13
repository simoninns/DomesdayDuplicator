/************************************************************************

    log_message_model.cpp

    Bounded model of log records, for the Log panel
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "log_message_model.h"

#include <algorithm>

namespace ddd::gui {

LogMessageModel::LogMessageModel(QObject* parent, int maximum_records)
    : QAbstractListModel(parent),
      maximum_records_(std::max(1, maximum_records)) {}

int LogMessageModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(records_.size());
}

QVariant LogMessageModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= static_cast<int>(records_.size())) {
    return {};
  }

  const Record& record = records_[static_cast<std::size_t>(index.row())];

  switch (role) {
    case Qt::DisplayRole:
      return QStringLiteral("%1  %2  %3")
          .arg(record.timestamp,
               QString::fromLatin1(capture::LogLevelName(record.level))
                   .toUpper()
                   .leftJustified(7),
               record.message);
    case kLevelRole:
      return static_cast<int>(record.level);
    case kMessageRole:
      return record.message;
    case kTimestampRole:
      return record.timestamp;
    default:
      return {};
  }
}

void LogMessageModel::Append(int level, const QString& timestamp,
                             const QString& message) {
  // Trimming before the insert keeps the model at its cap at all times, so a
  // view never briefly sees more rows than the model promises to hold.
  if (static_cast<int>(records_.size()) >= maximum_records_) {
    const int excess = static_cast<int>(records_.size()) - maximum_records_ + 1;
    beginRemoveRows(QModelIndex(), 0, excess - 1);
    records_.erase(records_.begin(), records_.begin() + excess);
    endRemoveRows();
  }

  const auto row = static_cast<int>(records_.size());
  beginInsertRows(QModelIndex(), row, row);
  records_.push_back(
      Record{timestamp, static_cast<capture::LogLevel>(level), message});
  endInsertRows();
}

void LogMessageModel::Clear() {
  if (records_.empty()) {
    return;
  }

  beginResetModel();
  records_.clear();
  endResetModel();
}

}  // namespace ddd::gui
