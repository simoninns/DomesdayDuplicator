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
// from any engine thread; it stamps the record, hands it to the mirror and
// emits RecordLogged.
//
// The mirror is the console and file half of the log. Records that survive the
// level filter go to both places: the Log panel is a window that closes with
// the application, and what --log-file writes is what survives to be attached
// to a bug report. Passing none — which the tests do, and which a front end
// with nowhere to mirror to would — leaves the panel as the only destination.
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
  // `mirror` is not owned and must outlive this object. It is taken at
  // construction rather than set later because Log() reads it from every engine
  // thread, and a pointer that can change under those threads would need a lock
  // to say nothing more than this does.
  explicit ApplicationLogger(capture::ILogger* mirror = nullptr,
                             QObject* parent = nullptr);

  void Log(capture::LogLevel level, std::string_view message) override;

  // Records below this level are discarded, for the panel and for the mirror
  // alike. Set from --log-level, or from --debug.
  void SetMinimumLevel(capture::LogLevel level);
  capture::LogLevel minimum_level() const;

 signals:
  // `level` is a capture::LogLevel; it crosses as an int because that needs no
  // metatype registration for a queued connection.
  void RecordLogged(int level, const QString& timestamp,
                    const QString& message);

 private:
  // Not owned; null when there is nowhere to mirror to.
  capture::ILogger* const mirror_;

  // Written from any thread, read from any thread. Relaxed ordering is
  // sufficient: a record logged around the instant the level changes may go
  // either way, and neither answer is wrong.
  std::atomic<capture::LogLevel> minimum_level_{capture::LogLevel::kInfo};
};

}  // namespace ddd::gui
