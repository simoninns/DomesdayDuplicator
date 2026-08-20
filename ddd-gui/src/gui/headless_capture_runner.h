/************************************************************************

    headless_capture_runner.h

    A capture with no window around it, from start to finished file
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtGlobal>
#include <vector>

#include "capture_cli.h"
#include "usb_device_info.h"

class QTextStream;

namespace ddd::gui {

class CaptureController;

// Start a capture as soon as a device is there, and then stop watching.
//
// The windowed half of --start-capture. There is no timeout and no exit code:
// the window is up, so a device plugged in five minutes later still starts the
// capture the command line asked for, and a user who would rather it did not
// can simply not wait for it. The headless half is the runner below, which
// needs both.
//
// The watch ends at the first device report that has something in it. An empty
// report is what the monitor produces on every poll with nothing attached, so
// a one-shot connection would be spent on the first of those and the capture
// would never start.
void StartCaptureWhenDeviceAppears(CaptureController* controller);

// The two waits a headless run is bounded by. At namespace scope, as
// StopCaptureOptions is, because a nested struct with defaults cannot be the
// default argument of a constructor in the class that nests it.
struct HeadlessCaptureOptions {
  // How long to wait for a device before giving up. A script that runs this on
  // a schedule wants to be told that nothing was plugged in, rather than to
  // leave a process waiting until somebody notices it. Zero or less waits for
  // as long as it takes.
  int device_wait_milliseconds = 10000;

  // How long to wait for the file after the capture has stopped. Not a guess at
  // how long finalising takes — it is the bound that stops a run hanging
  // forever if the pipeline never reports the file at all.
  int finish_wait_milliseconds = 30000;
};

// Runs one capture from a command line and reports what happened through an
// exit code.
//
// This is the whole of --headless: wait for a device, start, run until
// something says to stop, and — the part that makes it worth having as a class
// rather than as a few connections in main() — wait for the *file* rather than
// for the capture. Stopping detaches the writer and says so immediately, but at
// that moment the encoder has not closed the file, a capture whose naming asks
// for its duration has not been renamed, and the sidecar has not been written.
// A process that exited there would leave a script holding a path that is not
// finished and may not even exist under that name.
//
// The two streams say different things and are meant for different readers. The
// finished file's path goes to stdout, alone and unadorned, so that a script
// can take what it reads without having to take it apart; everything meant for
// a person watching goes to stderr. That is the same split --stop-capture uses.
//
// Nothing here quits the application. It reports an exit code and the caller
// decides — which is what lets the whole lifecycle be tested against a fake
// device with no process to end.
class HeadlessCaptureRunner : public QObject {
  Q_OBJECT

 public:
  HeadlessCaptureRunner(CaptureController* controller, QTextStream& out,
                        QTextStream& error,
                        const HeadlessCaptureOptions& options = {},
                        QObject* parent = nullptr);

  // Connect to the controller and start waiting for a device.
  //
  // Call this *before* CaptureController::Start(). The controller's first
  // device report is the one that starts the capture, and a runner that was
  // connected afterwards would miss it and wait for the next poll — or, if a
  // device were already attached and then removed, for one that never comes.
  void Begin();

 public slots:
  // Stop, and finish the file. What an interrupt means, and harmless both
  // before a capture has started and after one has been stopped.
  //
  // A second call while the file is still being written is deliberately not a
  // harder stop. The file is the point of the run, and the seconds an encoder
  // takes to close one are the cheapest seconds in the whole capture.
  void RequestStop();

 signals:
  // The run is over and this is what to exit with. Always emitted from the
  // event loop, never from inside Begin() or from a controller call, so that a
  // caller connecting this to QCoreApplication::quit() cannot have its quit
  // arrive before exec() does.
  void Finished(int exit_code);

 private:
  enum class State {
    kIdle,
    kWaitingForDevice,

    // StartCapture() has been called and has not yet said what came of it.
    kStarting,

    kCapturing,

    // The writer is detached and the file is being closed.
    kFinishing,

    kDone,
  };

  void OnDevicesChanged(const std::vector<capture::DeviceInfo>& devices);
  void OnCapturingChanged(bool capturing, const QString& file_path);
  void OnCaptureFinished(const QString& file_path, quint64 bytes);
  void OnFailed(const QString& title, const QString& detail);

  void StartCapture();

  // Wait one turn of the event loop, then finish with whatever the exit code
  // has become. The turn is what makes the exit code right: the controller
  // emits CaptureFinished and then, in the same call, may emit Failed for the
  // run that produced it — so a runner that exited on the first of those would
  // report a failed capture as a success.
  void ScheduleFinish();

  void Finish(int exit_code);

  // A line for whoever is watching. Flushed, because the reader is a terminal
  // or a log being tailed and a line held in a buffer for the rest of a disc
  // side is a line nobody sees.
  void Say(const QString& line);

  CaptureController* controller_ = nullptr;
  QTextStream* out_ = nullptr;
  QTextStream* error_ = nullptr;
  HeadlessCaptureOptions options_;

  QTimer device_timer_;
  QTimer finish_timer_;

  State state_ = State::kIdle;

  // What the run will exit with unless something worse happens to it. Only ever
  // moves away from success: a failure reported while the file is still being
  // written has to survive the wait for it.
  int exit_code_ = kExitSuccess;

  bool finish_scheduled_ = false;
};

}  // namespace ddd::gui
