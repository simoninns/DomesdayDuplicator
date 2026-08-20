/************************************************************************

    spdlog_logger.cpp

    Console and file log destinations, over spdlog
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "spdlog_logger.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <exception>
#include <memory>
#include <vector>

namespace ddd::capture {
namespace {

// The whole pattern, applied to every sink. %^ and %$ bracket what the console
// sink colours, which is the level and not the message: a coloured message is
// harder to read and a file sink drops the markers anyway.
constexpr const char* kPattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v";

spdlog::level::level_enum ToSpdlogLevel(LogLevel level) {
  switch (level) {
    case LogLevel::kDebug:
      return spdlog::level::debug;
    case LogLevel::kInfo:
      return spdlog::level::info;
    case LogLevel::kWarning:
      return spdlog::level::warn;
    case LogLevel::kError:
      return spdlog::level::err;
    case LogLevel::kOff:
      return spdlog::level::off;
  }

  return spdlog::level::info;
}

}  // namespace

SpdlogLogger::SpdlogLogger(const LogConfig& config) {
  const LogSinkSelection selection =
      ResolveLogSinks(config.destination, !config.file.empty());

  std::vector<spdlog::sink_ptr> sinks;

  // The console half goes to standard error rather than standard output. The
  // application's one other command-line path, --analyse-test-data, puts its
  // verdict on standard output, and a script collecting that must not have to
  // filter log records out of it.
  if (selection.console) {
    sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
  }

  if (selection.file) {
    try {
      // Truncated rather than appended, so the file describes the run that
      // produced it. A user asked to reproduce a fault with a log file gets one
      // file per attempt, which is what they are then asked to attach.
      sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(
          config.file, true));
      writes_to_file_ = true;
    } catch (const std::exception& error) {
      warnings_.push_back("The log file '" + config.file +
                          "' could not be opened (" + error.what() +
                          "). The log goes to the console instead.");
    }
  }

  // Only reachable when the file was the only destination asked for and it
  // could not be opened. Silently discarding every record from then on would be
  // the worst of the available answers.
  if (sinks.empty()) {
    sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
    writes_to_console_ = true;
  } else {
    writes_to_console_ = selection.console;
  }

  logger_ = std::make_shared<spdlog::logger>("ddd", sinks.begin(), sinks.end());
  logger_->set_pattern(kPattern);
  logger_->set_level(ToSpdlogLevel(config.level));

  // Flushed per record rather than on warnings and above. The log's reason to
  // exist is explaining a run that ended badly, and the records immediately
  // before a crash are the ones a buffer would have swallowed. The engine does
  // not log per buffer (logger.h), so the cost of this is not on any path where
  // it matters.
  logger_->flush_on(spdlog::level::trace);

  // Deliberately not spdlog::register_logger(). Nothing here looks a logger up
  // by name, and registering would mean a second SpdlogLogger — one in a test,
  // say — throwing on the duplicate name.
}

// Out of line because the header only forward-declares spdlog::logger.
SpdlogLogger::~SpdlogLogger() = default;

void SpdlogLogger::Log(LogLevel level, std::string_view message) {
  if (!logger_) {
    return;
  }

  // The two-argument overload takes the message as text rather than as a format
  // string, so a message carrying braces — a JSON fragment, a Windows device
  // path — is written and not parsed.
  logger_->log(ToSpdlogLevel(level), message);
}

}  // namespace ddd::capture
