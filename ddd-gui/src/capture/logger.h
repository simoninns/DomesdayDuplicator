/************************************************************************

    logger.h

    Logging seam for the capture engine
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <string_view>

namespace ddd::capture {

// Severity of a log record, ordered least to most severe. The ordering is
// relied upon: a minimum level admits every level at or above it.
enum class LogLevel : int {
  kDebug = 0,
  kInfo,
  kWarning,
  kError,
};

// Returns the lower-case name of a level ("debug", "info", ...).
const char* LogLevelName(LogLevel level);

// Where the engine sends diagnostics. The engine is deliberately Qt-free, so
// it cannot log through Qt; a front end supplies an implementation of this and
// routes records wherever it wants them. Implementations must tolerate calls
// from any thread.
//
// Not for the real-time path. The transfer and processing threads must not
// log per buffer: an implementation is free to take a lock or touch the
// filesystem, and either would put an unbounded stall in a pipeline that has
// milliseconds of slack. Log around a capture, not inside one.
class ILogger {
 public:
  ILogger() = default;
  virtual ~ILogger() = default;

  ILogger(const ILogger&) = delete;
  ILogger& operator=(const ILogger&) = delete;
  ILogger(ILogger&&) = delete;
  ILogger& operator=(ILogger&&) = delete;

  virtual void Log(LogLevel level, std::string_view message) = 0;

  void Debug(std::string_view message) { Log(LogLevel::kDebug, message); }
  void Info(std::string_view message) { Log(LogLevel::kInfo, message); }
  void Warning(std::string_view message) { Log(LogLevel::kWarning, message); }
  void Error(std::string_view message) { Log(LogLevel::kError, message); }
};

// An ILogger that filters by severity and hands surviving records to a
// callback. This is the whole of the engine's coupling to a front end: the GUI
// passes a callback that posts to its log panel, and a future command-line tool
// passes one that writes to stderr.
//
// Thread-safety: safe to call from any thread. The callback is invoked under a
// lock, so it is never entered concurrently and records arrive in order.
class CallbackLogger : public ILogger {
 public:
  using Sink = std::function<void(LogLevel, const std::string&)>;

  // A default-constructed (empty) sink discards everything, which is what a
  // test that only cares about the filtering wants.
  explicit CallbackLogger(Sink sink, LogLevel minimum_level = LogLevel::kInfo);

  void Log(LogLevel level, std::string_view message) override;

  void SetMinimumLevel(LogLevel level);
  LogLevel minimum_level() const;

 private:
  mutable std::mutex mutex_;
  Sink sink_;
  LogLevel minimum_level_;
};

}  // namespace ddd::capture
