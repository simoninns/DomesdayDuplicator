/************************************************************************

    log_options.cpp

    Log level and destination names, as the command line spells them
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "log_options.h"

namespace ddd::capture {

std::optional<LogLevel> ParseLogLevel(std::string_view value) {
  if (value == "trace" || value == "debug") {
    return LogLevel::kDebug;
  }
  if (value == "info") {
    return LogLevel::kInfo;
  }
  if (value == "warn" || value == "warning") {
    return LogLevel::kWarning;
  }
  if (value == "error" || value == "critical") {
    return LogLevel::kError;
  }
  if (value == "off") {
    return LogLevel::kOff;
  }

  return std::nullopt;
}

std::optional<LogDestination> ParseLogDestination(std::string_view value) {
  if (value == "console") {
    return LogDestination::kConsole;
  }
  if (value == "file") {
    return LogDestination::kFile;
  }
  if (value == "both") {
    return LogDestination::kBoth;
  }

  return std::nullopt;
}

const char* LogDestinationName(LogDestination destination) {
  switch (destination) {
    case LogDestination::kConsole:
      return "console";
    case LogDestination::kFile:
      return "file";
    case LogDestination::kBoth:
      return "both";
  }

  return "unknown";
}

LogSinkSelection ResolveLogSinks(LogDestination destination,
                                 bool have_log_file) {
  LogSinkSelection selection;
  selection.file = have_log_file && destination != LogDestination::kConsole;
  selection.console = destination != LogDestination::kFile || !selection.file;
  return selection;
}

}  // namespace ddd::capture
