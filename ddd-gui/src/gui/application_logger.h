/************************************************************************

    application_logger.h

    Bridges engine log records onto the GUI thread
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <string_view>

#include "logger.h"

namespace ddd::gui {

// The GUI's implementation of the engine's logging seam. Log() may be called
// from any engine thread; it stamps the record and emits RecordLogged.
//
// The signal is the thread boundary and the reason this class exists. The
// object lives on the GUI thread, so a record emitted from an engine thread is
// delivered through a queued connection — the engine thread returns immediately
// and the model is only ever touched by the GUI thread. Calling into a widget
// directly from an engine thread would be the alternative, and it would be a
// data race.
//
// Thread-safety: Log() is safe from any thread. Everything else is GUI thread
// only.
class ApplicationLogger : public QObject, public capture::ILogger {
  Q_OBJECT

 public:
  explicit ApplicationLogger(QObject* parent = nullptr);

  void Log(capture::LogLevel level, std::string_view message) override;

  // Records below this level are discarded. Set from the --debug switch.
  void SetMinimumLevel(capture::LogLevel level);
  capture::LogLevel minimum_level() const;

 signals:
  // `level` is a capture::LogLevel; it crosses as an int because that needs no
  // metatype registration for a queued connection.
  void RecordLogged(int level, const QString& timestamp,
                    const QString& message);

 private:
  // Written from any thread, read from any thread. Relaxed ordering is
  // sufficient: a record logged around the instant the level changes may go
  // either way, and neither answer is wrong.
  std::atomic<capture::LogLevel> minimum_level_{capture::LogLevel::kInfo};
};

}  // namespace ddd::gui
