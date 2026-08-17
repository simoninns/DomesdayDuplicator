/************************************************************************

    capture_failure_presenter.h

    Turning a failure code into something a user can act on
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>

#include "transfer_result.h"

namespace ddd::gui {

// Task 5.2. The point of the whole error taxonomy is that a user who has just
// lost forty minutes of a disc is told which of four different things to do,
// and the difference between a code and a remedy is made here.
//
// A presenter rather than message-box calls at the point of failure, for the
// same reason the statistics have one: this is the part that can be wrong, and
// it can only be checked by a test if it produces a value. "Every failure code
// produces its own message and none falls through to a generic one" is an
// assertion about a function; it is not an assertion anybody can make about a
// QMessageBox.
struct CaptureFailureView {
  // The window title. Carries the result's name so a user has something to
  // search for and a maintainer something to act on.
  QString title;

  // What went wrong, in one sentence
  QString summary;

  // What to do about it. Never empty — a message with no remedy is the generic
  // message this file exists to prevent.
  QString remedy;

  // What happened to the file, when a capture was running. Empty otherwise.
  QString file_note;

  // The whole thing as one block of text, which is what a message box takes.
  QString ToMessage() const;
};

// The view for a failed run.
//
// `detail` is the pipeline's own account of the failure — the sample offset of
// a sequence break, the encoder's error string — and is included when it says
// more than the code does. `file_path` names a capture that was being written,
// or is empty for a monitor-mode failure.
CaptureFailureView PresentCaptureFailure(capture::TransferResult result,
                                         const QString& detail,
                                         const QString& file_path);

// The instruction for raising the kernel's usbfs limit, with the number to
// write. Separated out because it is the one remedy that is a command rather
// than an action, and it has to be exact: a user is going to copy it.
QString UsbfsMemoryLimitInstruction();

// What to say about a capture name that is already taken.
//
// **A capture has never overwritten another** — the path is made unique before
// the file is opened, and has been since before this existed. What was missing
// was anybody being told, and a typed name has no timestamp in it, so the
// second capture of "Casper side 1" quietly becoming "Casper side 1 (1)" is
// the ordinary case rather than an edge one. Two files nobody can tell apart
// later is a slower way to lose a capture than overwriting one, but it is not
// a much slower way.
//
// It says the resulting name and nothing else. What is being done is what
// every desktop does with a name already in use, and a sentence explaining it
// is a sentence nobody needs to read twice.
//
// Empty where the name is free, so a caller can hide the row. `written` is the
// name the file will really carry.
QString CaptureNameTakenNote(const QString& written);

}  // namespace ddd::gui
