/************************************************************************

    headless_capture_runner.cpp

    A capture with no window around it, from start to finished file
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "headless_capture_runner.h"

#include <QMetaObject>
#include <QTextStream>
#include <memory>

#include "capture_controller.h"

namespace ddd::gui {

void StartCaptureWhenDeviceAppears(CaptureController* controller) {
  if (controller == nullptr) {
    return;
  }

  if (!controller->devices().empty()) {
    controller->StartCapture();
    return;
  }

  // Held through a shared pointer so that the connection can name itself: the
  // handler has to disconnect the very connection it is running in, and Qt has
  // no other way to say that. Qt::SingleShotConnection would say it in one word
  // and say the wrong thing — it is spent on the first report of any kind, and
  // the first report is nearly always the empty one.
  auto connection = std::make_shared<QMetaObject::Connection>();
  *connection = QObject::connect(
      controller, &CaptureController::DevicesChanged, controller,
      [controller,
       connection](const std::vector<capture::DeviceInfo>& devices) {
        if (devices.empty()) {
          return;
        }
        QObject::disconnect(*connection);
        controller->StartCapture();
      });
}

HeadlessCaptureRunner::HeadlessCaptureRunner(
    CaptureController* controller, QTextStream& out, QTextStream& error,
    const HeadlessCaptureOptions& options, QObject* parent)
    : QObject(parent),
      controller_(controller),
      out_(&out),
      error_(&error),
      options_(options) {
  device_timer_.setSingleShot(true);
  finish_timer_.setSingleShot(true);

  connect(&device_timer_, &QTimer::timeout, this, [this] {
    if (state_ != State::kWaitingForDevice) {
      return;
    }
    Say(QStringLiteral(
            "No Domesday Duplicator was found within %1 seconds. Nothing was "
            "captured.")
            .arg(options_.device_wait_milliseconds / 1000));
    Finish(kExitNoDevice);
  });

  connect(&finish_timer_, &QTimer::timeout, this, [this] {
    if (state_ == State::kDone) {
      return;
    }
    Say(QStringLiteral(
        "The capture was stopped but the file was never reported as finished. "
        "Whatever was written is on disk and may be incomplete."));
    Finish(kExitCaptureFailed);
  });
}

void HeadlessCaptureRunner::Begin() {
  if (controller_ == nullptr) {
    Say(QStringLiteral("There is no capture controller to run."));
    Finish(kExitCaptureFailed);
    return;
  }

  if (state_ != State::kIdle) {
    return;
  }
  state_ = State::kWaitingForDevice;

  connect(controller_, &CaptureController::DevicesChanged, this,
          &HeadlessCaptureRunner::OnDevicesChanged);
  connect(controller_, &CaptureController::CapturingChanged, this,
          &HeadlessCaptureRunner::OnCapturingChanged);
  connect(controller_, &CaptureController::CaptureFinished, this,
          &HeadlessCaptureRunner::OnCaptureFinished);
  connect(controller_, &CaptureController::Failed, this,
          &HeadlessCaptureRunner::OnFailed);

  // Reported rather than acted on. A capture written under a name other than
  // the one asked for is not a failure — nothing was overwritten — but a script
  // that named the file needs to be told, because the name it reads back is not
  // the one it gave. The path on stdout at the end is the authority.
  connect(controller_, &CaptureController::CaptureRenamed, this,
          [this](const QString& requested, const QString& written) {
            Say(QStringLiteral("'%1' was already taken, so the capture is "
                               "being written as '%2'.")
                    .arg(requested, written));
          });

  connect(controller_, &CaptureController::MetadataWriteFailed, this,
          [this](const QString& detail) {
            Say(QStringLiteral("The metadata file could not be written: %1")
                    .arg(detail));
          });

  connect(controller_, &CaptureController::LowSpaceWarning, this,
          [this](const QString& message) { Say(message); });

  if (options_.device_wait_milliseconds > 0) {
    device_timer_.start(options_.device_wait_milliseconds);
  }

  // A device that is already there, noticed one turn of the event loop later
  // rather than now. Begin() is called before exec(), so a capture that failed
  // to start from inside it would emit Finished() into a loop that had not
  // begun — and the quit it is connected to would be lost.
  QMetaObject::invokeMethod(
      this,
      [this] {
        if (state_ != State::kWaitingForDevice || controller_ == nullptr) {
          return;
        }
        if (!controller_->devices().empty()) {
          StartCapture();
        }
      },
      Qt::QueuedConnection);
}

void HeadlessCaptureRunner::OnDevicesChanged(
    const std::vector<capture::DeviceInfo>& devices) {
  if (state_ != State::kWaitingForDevice || devices.empty()) {
    return;
  }
  StartCapture();
}

void HeadlessCaptureRunner::StartCapture() {
  device_timer_.stop();
  state_ = State::kStarting;
  controller_->StartCapture();

  // Still starting means the controller neither began a capture nor said why,
  // which nothing in it does today — CapturingChanged or Failed comes back from
  // that call. Covered anyway, because the alternative is a headless process
  // that sits at a prompt forever having captured nothing.
  if (state_ == State::kStarting) {
    Say(QStringLiteral("The capture did not start."));
    Finish(kExitCaptureFailed);
  }
}

void HeadlessCaptureRunner::OnCapturingChanged(bool capturing,
                                               const QString& file_path) {
  if (state_ == State::kDone) {
    return;
  }

  if (capturing) {
    state_ = State::kCapturing;
    Say(QStringLiteral("Capturing to %1").arg(file_path));
    return;
  }

  if (state_ != State::kCapturing && state_ != State::kStarting) {
    return;
  }

  // Every way a capture ends arrives here — the duration limit, an interrupt, a
  // --stop-capture client, or the stream failing underneath it — so the wait
  // for the file is armed in one place rather than at each of them.
  state_ = State::kFinishing;
  Say(QStringLiteral("Stopped. Finishing the file."));
  if (options_.finish_wait_milliseconds > 0) {
    finish_timer_.start(options_.finish_wait_milliseconds);
  }
}

void HeadlessCaptureRunner::OnCaptureFinished(const QString& file_path,
                                              quint64 bytes) {
  if (state_ == State::kDone) {
    return;
  }

  state_ = State::kFinishing;
  finish_timer_.stop();

  *out_ << file_path << "\n";
  out_->flush();
  Say(QStringLiteral("Finished. %1 bytes written.").arg(bytes));

  ScheduleFinish();
}

void HeadlessCaptureRunner::OnFailed(const QString& title,
                                     const QString& detail) {
  if (state_ == State::kDone) {
    return;
  }

  Say(QStringLiteral("Error: %1. %2").arg(title, detail));
  exit_code_ = kExitCaptureFailed;

  // The file is already on its way and the exit code has just been made to say
  // so. This is the ordering the wait exists for: the controller reports the
  // finished file and then, in the same call, reports the failure of the run
  // that produced it.
  if (finish_scheduled_) {
    return;
  }

  if (state_ == State::kCapturing) {
    // Whatever has been written so far is worth closing properly, and stopping
    // is how that happens. The finish path takes it from here.
    controller_->StopCapture();
    return;
  }

  if (state_ == State::kFinishing) {
    return;
  }

  Finish(exit_code_);
}

void HeadlessCaptureRunner::RequestStop() {
  if (state_ == State::kDone) {
    return;
  }

  if (state_ == State::kFinishing) {
    Say(QStringLiteral(
        "Still finishing the file. Interrupting again will not make it "
        "quicker, and stopping here would leave it unreadable."));
    return;
  }

  if (state_ == State::kCapturing) {
    Say(QStringLiteral("Interrupted. Stopping the capture."));
    controller_->StopCapture();
    return;
  }

  // Interrupted before anything was captured. Reported as no device, because
  // that is what a script needs to know: there is no file, and the reason there
  // is no file is that nothing was ever there to capture from.
  Say(QStringLiteral(
      "Interrupted before a device appeared. Nothing was captured."));
  Finish(kExitNoDevice);
}

void HeadlessCaptureRunner::ScheduleFinish() {
  if (finish_scheduled_) {
    return;
  }
  finish_scheduled_ = true;

  QMetaObject::invokeMethod(
      this, [this] { Finish(exit_code_); }, Qt::QueuedConnection);
}

void HeadlessCaptureRunner::Finish(int exit_code) {
  if (state_ == State::kDone) {
    return;
  }
  state_ = State::kDone;
  device_timer_.stop();
  finish_timer_.stop();

  out_->flush();
  error_->flush();

  // Queued, always. Finish() is reachable from Begin() and from a controller
  // call made before exec(), and a caller that connects this to quit() would
  // otherwise lose it and wait forever in an event loop it had just been told
  // to leave.
  QMetaObject::invokeMethod(
      this, [this, exit_code] { emit Finished(exit_code); },
      Qt::QueuedConnection);
}

void HeadlessCaptureRunner::Say(const QString& line) {
  *error_ << line << "\n";
  error_->flush();
}

}  // namespace ddd::gui
