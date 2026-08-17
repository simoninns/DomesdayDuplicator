/************************************************************************

    capture_failure_presenter.cpp

    Turning a failure code into something a user can act on
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_failure_presenter.h"

#include <QCoreApplication>

namespace ddd::gui {
namespace {

// The figure the old application told people to write, and the reason it is
// spelled out rather than described: a user hitting this is going to copy the
// line, and a paraphrase does not paste.
constexpr int kSuggestedUsbfsMemoryMegabytes = 1000;

QString Translate(const char* text) {
  return QCoreApplication::translate("CaptureFailurePresenter", text);
}

QString Summary(capture::TransferResult result) {
  switch (result) {
    case capture::TransferResult::kRunning:
    case capture::TransferResult::kSuccess:
      return Translate("The capture stopped.");
    case capture::TransferResult::kFileCreationError:
      return Translate("The capture file could not be created.");
    case capture::TransferResult::kFileWriteError:
      return Translate("Writing to the capture file failed.");
    case capture::TransferResult::kBufferOverflow:
      return Translate(
          "This machine could not write the data as fast as the device "
          "produced it, and samples were lost.");
    case capture::TransferResult::kConnectionFailure:
      return Translate("The connection to the device failed.");
    case capture::TransferResult::kUsbTransferFailure:
      return Translate("A USB transfer failed.");
    case capture::TransferResult::kHostUnderflow:
      return Translate(
          "This machine did not keep a read request outstanding, so the device "
          "had nowhere to put its data and samples were lost.");
    case capture::TransferResult::kUsbMemoryLimit:
      return Translate(
          "The kernel's usbfs memory limit is lower than the buffer queue this "
          "capture asked for.");
    case capture::TransferResult::kSequenceMismatch:
      return Translate(
          "The device's sequence numbering broke, which means samples were "
          "lost. This capture is not bit-perfect.");
    case capture::TransferResult::kVerificationError:
      return Translate(
          "The device's test pattern did not arrive intact, so something in "
          "the capture path is corrupting data.");
    case capture::TransferResult::kSourceStalled:
      return Translate("The device stopped sending data.");
    case capture::TransferResult::kProgramError:
      return Translate(
          "The capture failed because of a fault in this "
          "application.");
    case capture::TransferResult::kForcedAbort:
      return Translate(
          "The capture was stopped immediately, discarding buffered data.");
  }
  return Translate(
      "The capture stopped for a reason this application does "
      "not recognise.");
}

// What to do next. This is the half that makes the taxonomy worth having, and
// every branch has to name an action — a remedy of "try again" is the generic
// message the whole file exists to avoid.
QString Remedy(capture::TransferResult result) {
  switch (result) {
    case capture::TransferResult::kRunning:
    case capture::TransferResult::kSuccess:
      return Translate("Nothing needs to be done.");
    case capture::TransferResult::kFileCreationError:
      return Translate(
          "Check that the destination folder exists, that it is writable, and "
          "that the volume is not full. Choosing a different folder in the "
          "Capture panel is the quickest way to test this.");
    case capture::TransferResult::kFileWriteError:
      return Translate(
          "Check the free space on the destination volume, and that the drive "
          "has not been disconnected. If the volume is full, free space or "
          "capture to a different drive.");
    case capture::TransferResult::kBufferOverflow:
      return Translate(
          "Capture to a faster drive, lower the FLAC compression level, or "
          "raise the buffer queue size in Settings. The buffer-queue and "
          "encoder-backlog figures in the Statistics panel say which of the "
          "three is the bottleneck: a backlog that climbs is the encoder, a "
          "queue that climbs with no backlog is the disk.");
    case capture::TransferResult::kConnectionFailure:
      return Translate(
          "Check that the device is plugged in, and that no other application "
          "is using it. Unplugging it and plugging it back in resets the "
          "device's own state.");
    case capture::TransferResult::kUsbTransferFailure:
      return Translate(
          "Try a different USB cable, and a port connected directly to the "
          "computer rather than through a hub. A capture needs a USB 3 port to "
          "carry 80 MB/s at all.");
    case capture::TransferResult::kHostUnderflow:
      return Translate(
          "Turn off small transfers in Settings so that fewer, larger requests "
          "are outstanding, and close whatever else is competing for the CPU.");
    case capture::TransferResult::kUsbMemoryLimit:
      return UsbfsMemoryLimitInstruction();
    case capture::TransferResult::kSequenceMismatch:
      return Translate(
          "Repeat the capture. If it happens again, this machine is most "
          "likely not keeping up — the same remedies as a buffer overflow "
          "apply, starting with a faster drive and a lower compression level.");
    case capture::TransferResult::kVerificationError:
      return Translate(
          "This is a fault in the hardware or the cabling rather than in the "
          "recording. Check the USB cable and the port, then repeat the test "
          "capture. A test capture that fails repeatedly is what the "
          "capture-integrity procedure in TESTING.md exists to diagnose.");
    case capture::TransferResult::kSourceStalled:
      return Translate(
          "Check the USB cable and the device's power, then unplug the device "
          "and plug it back in.");
    case capture::TransferResult::kProgramError:
      return Translate(
          "Please report this, with the contents of the Log panel — run with "
          "--debug to record the full diagnostics.");
    case capture::TransferResult::kForcedAbort:
      return Translate(
          "Anything already written has been kept. Start the capture again "
          "when ready.");
  }
  return Translate(
      "Please report this, with the contents of the Log panel — a failure this "
      "application cannot name is a fault in the application.");
}

}  // namespace

QString UsbfsMemoryLimitInstruction() {
  return Translate(
             "Raise the kernel's limit, which needs administrator rights:\n"
             "\n"
             "    sudo sh -c 'echo %1 > "
             "/sys/module/usbcore/parameters/usbfs_memory_mb'\n"
             "\n"
             "That lasts until the machine is restarted; the project's udev "
             "rules set it permanently. Reducing the buffer queue size in "
             "Settings works without administrator rights.")
      .arg(kSuggestedUsbfsMemoryMegabytes);
}

QString CaptureFailureView::ToMessage() const {
  QString message = summary;
  if (!remedy.isEmpty()) {
    message += QStringLiteral("\n\n") + remedy;
  }
  if (!file_note.isEmpty()) {
    message += QStringLiteral("\n\n") + file_note;
  }
  return message;
}

CaptureFailureView PresentCaptureFailure(capture::TransferResult result,
                                         const QString& detail,
                                         const QString& file_path) {
  CaptureFailureView view;

  view.title = Translate("Capture stopped: %1")
                   .arg(QString::fromUtf8(capture::TransferResultName(result)));

  view.summary = Summary(result);

  // The pipeline's own account, when it has one. It carries the things no
  // enumeration can — which sample the sequence broke at, what libFLAC said —
  // and those are what turns a report into a diagnosis.
  if (!detail.isEmpty()) {
    view.summary += QStringLiteral("\n\n") + detail;
  }

  view.remedy = Remedy(result);

  if (!file_path.isEmpty()) {
    // Said plainly, because the first thing anyone wants to know after losing a
    // capture is whether any of it survived. It does: the pipeline finalises
    // the sink on the way out however it stopped, so the FLAC stream is closed
    // properly and the partial file is readable up to where it ends.
    view.file_note =
        Translate(
            "What had already been written was finalised and is readable:\n%1")
            .arg(file_path);
  }

  return view;
}

QString CaptureNameTakenNote(const QString& written) {
  if (written.isEmpty()) {
    return QString();
  }

  return Translate(
             "A capture of that name is already there. This one will be "
             "written as \u201c%1\u201d — nothing is overwritten.")
      .arg(written);
}

}  // namespace ddd::gui
