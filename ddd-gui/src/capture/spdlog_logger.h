/************************************************************************

    spdlog_logger.h

    Console and file log destinations, over spdlog
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "log_options.h"
#include "logger.h"

// spdlog's own forward declarations, repeated rather than included: this header
// is reached by most of the application, and the point of holding the logger
// behind a pointer is that spdlog's headers — and the formatting library under
// them — are compiled once, in spdlog_logger.cpp, rather than everywhere.
// Identical to <spdlog/fwd.h>.
namespace spdlog {
class logger;
}  // namespace spdlog

namespace ddd::capture {

// What the --log-level, --log-file and --log-out switches add up to.
struct LogConfig {
  LogLevel level = LogLevel::kInfo;
  LogDestination destination = LogDestination::kBoth;

  // Empty when no --log-file was given, which is the ordinary case.
  std::string file;
};

// An ILogger that writes records to the console, to a file, or to both.
//
// This is the second implementation of the engine's logging seam, beside
// CallbackLogger, and it is what makes a record visible outside the running
// application: the GUI's own log panel is a window that closes with the
// application, and a file is what survives to be attached to a bug report.
//
// A front end typically owns one of these for the whole run and hands it to
// whatever else logs — the GUI mirrors every record it shows into one, so the
// panel and the console carry the same log rather than two different ones.
//
// Thread-safety: safe to call from any thread; the underlying sinks each hold
// their own lock. It is subject to logger.h's rule about the real-time path all
// the same — a file write is exactly the unbounded stall that rule exists for,
// so do not log per buffer.
class SpdlogLogger : public ILogger {
 public:
  // Installs the sinks the configuration asks for. Never throws for a
  // destination it could not provide: a log file that cannot be opened is
  // reported through warnings() and the console is kept, because losing the log
  // is not a reason to refuse to start a capture application.
  explicit SpdlogLogger(const LogConfig& config);
  ~SpdlogLogger() override;

  void Log(LogLevel level, std::string_view message) override;

  bool writes_to_console() const { return writes_to_console_; }
  bool writes_to_file() const { return writes_to_file_; }

  // What could not be done as asked, in words meant for a user. Empty when the
  // configuration was honoured in full. Read once after construction and log
  // through whatever the front end shows a user — these are the messages that
  // explain a missing log file.
  const std::vector<std::string>& warnings() const { return warnings_; }

 private:
  // Null only if spdlog itself could not be given a single sink, which cannot
  // happen: the console is the fallback for everything. Log() checks anyway,
  // because a null dereference in the logging path would be a poor way to find
  // out otherwise.
  std::shared_ptr<spdlog::logger> logger_;

  std::vector<std::string> warnings_;
  bool writes_to_console_ = false;
  bool writes_to_file_ = false;
};

}  // namespace ddd::capture
