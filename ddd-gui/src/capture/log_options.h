/************************************************************************

    log_options.h

    Log level and destination names, as the command line spells them
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <optional>
#include <string_view>

#include "logger.h"

namespace ddd::capture {

// Where log records are written, chosen by --log-out. kBoth is the default and
// means "the console, plus the file when --log-file named one" — so a run with
// no log file logs to the console and nothing is lost by not having asked.
enum class LogDestination {
  kConsole,  // The console only; a configured log file is ignored
  kFile,     // The log file only; nothing reaches the console
  kBoth,     // The console, plus the log file when one is configured
};

// Which sinks a destination resolves to, once it is known whether there is a
// file to write to.
struct LogSinkSelection {
  bool console = true;
  bool file = false;
};

// Parses a --log-level value. The vocabulary is spdlog's, and wider than the
// four levels a record can carry, so that a level named on another of this
// project's tools means the same thing here: `trace` is accepted as a second
// name for debug and `critical` as a second name for error, because the engine
// has no separate level for either and rejecting a name a user reasonably
// expects to work would be the worse answer.
//
// `off` parses to LogLevel::kOff, which admits nothing.
//
// Comparison is case-sensitive, which keeps the accepted spelling one thing
// rather than a set that grows every time somebody types a capital letter.
//
// @return The level, or nullopt if the name is not one of the accepted ones
std::optional<LogLevel> ParseLogLevel(std::string_view value);

// Parses a --log-out value: "console", "file" or "both".
//
// @return The destination, or nullopt if the name is not one of those three
std::optional<LogDestination> ParseLogDestination(std::string_view value);

// The lower-case name of a destination, as --log-out spells it.
const char* LogDestinationName(LogDestination destination);

// Resolves a destination into the sinks to install.
//
// A file sink is only possible when a log file was named, so kFile without one
// would leave the log with no sinks at all and silently swallow every record.
// The console is kept in that case, and the front end says separately that the
// file it was asked for is not there.
//
// @param destination   What --log-out asked for
// @param have_log_file True when --log-file named a path
LogSinkSelection ResolveLogSinks(LogDestination destination,
                                 bool have_log_file);

}  // namespace ddd::capture
