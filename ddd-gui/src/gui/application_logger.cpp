/************************************************************************

    application_logger.cpp

    Bridges engine log records onto the GUI thread
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "application_logger.h"

#include <QDateTime>

namespace ddd::gui {

ApplicationLogger::ApplicationLogger(capture::ILogger* mirror, QObject* parent)
    : QObject(parent), mirror_(mirror) {}

void ApplicationLogger::Log(capture::LogLevel level, std::string_view message) {
  if (static_cast<int>(level) <
      static_cast<int>(minimum_level_.load(std::memory_order_relaxed))) {
    return;
  }

  // Stamped here, on the thread that logged, rather than when the queued
  // record is delivered: a busy GUI thread would otherwise give every record
  // the same misleading arrival time.
  const QString timestamp =
      QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));

  // Before the signal, not after. The signal is queued when the record came
  // from an engine thread, so the panel sees it whenever the GUI thread next
  // runs; the console and the file see it now, which is what makes them useful
  // for a fault that ends the process before that happens.
  if (mirror_ != nullptr) {
    mirror_->Log(level, message);
  }

  emit RecordLogged(static_cast<int>(level), timestamp,
                    QString::fromUtf8(message.data(),
                                      static_cast<qsizetype>(message.size())));
}

void ApplicationLogger::SetMinimumLevel(capture::LogLevel level) {
  minimum_level_.store(level, std::memory_order_relaxed);
}

capture::LogLevel ApplicationLogger::minimum_level() const {
  return minimum_level_.load(std::memory_order_relaxed);
}

}  // namespace ddd::gui
