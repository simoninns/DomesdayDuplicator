/************************************************************************

    logger.cpp

    Logging seam for the capture engine
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "logger.h"

#include <utility>

namespace ddd::capture {

const char* LogLevelName(LogLevel level) {
  switch (level) {
    case LogLevel::kDebug:
      return "debug";
    case LogLevel::kInfo:
      return "info";
    case LogLevel::kWarning:
      return "warning";
    case LogLevel::kError:
      return "error";
    case LogLevel::kOff:
      return "off";
  }

  return "unknown";
}

CallbackLogger::CallbackLogger(Sink sink, LogLevel minimum_level)
    : sink_(std::move(sink)), minimum_level_(minimum_level) {}

void CallbackLogger::Log(LogLevel level, std::string_view message) {
  const std::lock_guard<std::mutex> guard(mutex_);

  if (static_cast<int>(level) < static_cast<int>(minimum_level_)) {
    return;
  }
  if (!sink_) {
    return;
  }

  sink_(level, std::string(message));
}

void CallbackLogger::SetMinimumLevel(LogLevel level) {
  const std::lock_guard<std::mutex> guard(mutex_);
  minimum_level_ = level;
}

LogLevel CallbackLogger::minimum_level() const {
  const std::lock_guard<std::mutex> guard(mutex_);
  return minimum_level_;
}

}  // namespace ddd::capture
