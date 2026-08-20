/************************************************************************

    capture_cli.h

    The capture options the command line accepts, and what they mean
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QCommandLineOption>
#include <QString>
#include <optional>

#include "capture_format.h"
#include "capture_settings.h"

class QCommandLineParser;

namespace ddd::gui {

// What a scripted run exits with.
//
// The exit code is the interface: a script that starts a capture, waits for it
// and moves on reads nothing else, so each of these is a different thing to do
// about the failure rather than a different way of saying "it did not work".
// Zero is success everywhere, and 1 is what main() already returns for a
// command line it could not make sense of.
enum CaptureCliExit : int {
  kExitSuccess = 0,
  kExitBadArguments = 1,

  // Another instance holds the control socket. Two processes streaming from one
  // device is not something either of them can do, so the second says so rather
  // than racing for the USB claim.
  kExitInstanceRunning = 2,

  // Nothing was attached before the wait ran out.
  kExitNoDevice = 3,

  // A capture started and something went wrong with it. Distinct from the two
  // above because it is the only one where a file exists to go and look at.
  kExitCaptureFailed = 4,

  // --stop-capture found nothing to stop.
  kExitNoRunningInstance = 5,
};

// Every capture option the command line accepts, kept together so that main()
// and the tests add exactly the same set.
//
// QCommandLineOption has no default constructor, so this is built by
// AddCaptureCliOptions() below and is never half-populated.
struct CaptureCliOptionSet {
  QCommandLineOption start_capture;
  QCommandLineOption stop_capture;
  QCommandLineOption headless;
  QCommandLineOption capture_directory;
  QCommandLineOption capture_name;
  QCommandLineOption sample_rate;
  QCommandLineOption duration_limit;
  QCommandLineOption output_format;
};

// Add them to a parser, and hand back the set to read the values out of.
CaptureCliOptionSet AddCaptureCliOptions(QCommandLineParser& parser);

// What the command line asked for, once it has been made sense of.
//
// The attributes are optional rather than pre-filled with the defaults, because
// "not mentioned" and "asked for, and it happens to be the default" are
// different instructions: the first leaves the user's saved setting alone and
// the second overrides it with the same value. A struct of plain values could
// not tell the two apart.
struct CaptureCliOptions {
  bool start_capture = false;
  bool stop_capture = false;
  bool headless = false;

  std::optional<QString> capture_directory;
  std::optional<QString> capture_name;

  // Held as the factor the device is given rather than as the rate the user
  // typed, because the factor is what a setting is and the rate is how it is
  // spelled. See capture_format.h.
  std::optional<int> decimation_factor;

  std::optional<int> duration_limit_seconds;
  std::optional<capture::CaptureOutputFormat> output_format;

  // Whether anything about the capture itself was named. An attribute given
  // with no start command is not an error: it populates the window, which is
  // the "set this up for me and I will press the button" case.
  bool HasAttributeOverrides() const;
};

// The options, or the first thing wrong with them.
//
// One error rather than a list. The first one is the one a user fixes, and a
// second complaint about a line that is about to be retyped is noise.
struct CaptureCliParseResult {
  CaptureCliOptions options;
  QString error;

  bool ok() const { return error.isEmpty(); }
};

// Read the options off a parsed command line and check them over.
//
// Checks the combinations as well as the values: --headless without
// --start-capture is a request to do nothing invisibly, and --stop-capture
// beside anything else is two instructions for one process, where the second
// one would be silently dropped.
CaptureCliParseResult ParseCaptureCliOptions(const QCommandLineParser& parser,
                                             const CaptureCliOptionSet& set);

// Lay whatever was named over the settings a capture is about to run with.
//
// Deliberately a mutation of a value rather than a call on the controller:
// what the command line says applies to this run and is forgotten, and
// CaptureController::SetSettings() persists everything it is given. See
// CaptureController::ApplySessionSettings(), which is the other half of that.
void ApplyCliOverrides(CaptureSettings& settings,
                       const CaptureCliOptions& options);

// Whether this command line asks for something that needs no display.
//
// Read before any application object exists, because which one to construct is
// the question it answers: a headless capture and a --stop-capture client both
// run under a QCoreApplication, and a QApplication would need a platform plugin
// neither of them has any use for — on a machine with no display, the
// difference between working and refusing to start.
//
// A scan of the raw arguments rather than a parse, since QCommandLineParser
// needs the application it is about to decide on. It is deliberately literal:
// it recognises the two switches exactly as the parser accepts them, and
// anything else it sees is left for the parser to complain about properly.
bool WantsCoreApplication(int argc, char* argv[]);

}  // namespace ddd::gui
