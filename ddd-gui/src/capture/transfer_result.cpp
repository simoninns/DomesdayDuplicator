/************************************************************************

    transfer_result.cpp

    How a capture ended
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "transfer_result.h"

namespace ddd::capture {

const char* TransferResultName(TransferResult result) {
  switch (result) {
    case TransferResult::kRunning:
      return "running";
    case TransferResult::kSuccess:
      return "success";
    case TransferResult::kFileCreationError:
      return "file-creation-error";
    case TransferResult::kFileWriteError:
      return "file-write-error";
    case TransferResult::kBufferOverflow:
      return "buffer-overflow";
    case TransferResult::kConnectionFailure:
      return "connection-failure";
    case TransferResult::kUsbTransferFailure:
      return "usb-transfer-failure";
    case TransferResult::kUsbMemoryLimit:
      return "usb-memory-limit";
    case TransferResult::kSequenceMismatch:
      return "sequence-mismatch";
    case TransferResult::kVerificationError:
      return "verification-error";
    case TransferResult::kSourceStalled:
      return "source-stalled";
    case TransferResult::kProgramError:
      return "program-error";
    case TransferResult::kForcedAbort:
      return "forced-abort";
  }
  return "unknown";
}

const char* TransferResultDescription(TransferResult result) {
  switch (result) {
    case TransferResult::kRunning:
      return "The capture is running.";
    case TransferResult::kSuccess:
      return "The capture completed.";
    case TransferResult::kFileCreationError:
      return "The capture file could not be created. Check that the folder "
             "exists and is writable.";
    case TransferResult::kFileWriteError:
      return "Writing to the capture file failed. The volume may be full, or "
             "the device may have been removed.";
    case TransferResult::kBufferOverflow:
      return "This machine could not write the data as fast as the device "
             "produced it, and samples were lost. Try a faster drive, a lower "
             "compression level, or a larger buffer queue.";
    case TransferResult::kConnectionFailure:
      return "The connection to the device failed. Check that it is plugged in "
             "and that no other application is using it.";
    case TransferResult::kUsbTransferFailure:
      return "A USB transfer failed. Try a different cable or a port connected "
             "directly to the computer rather than through a hub.";
    case TransferResult::kUsbMemoryLimit:
      return "The kernel's usbfs memory limit is too low for the requested "
             "buffer queue. Raise "
             "/sys/module/usbcore/parameters/usbfs_memory_mb, or reduce the "
             "queue size.";
    case TransferResult::kSequenceMismatch:
      return "The device's sequence numbering broke, which means samples were "
             "lost. The capture is not bit-perfect and should be repeated.";
    case TransferResult::kVerificationError:
      return "The device's test pattern did not arrive intact, so the capture "
             "path is corrupting data.";
    case TransferResult::kSourceStalled:
      return "The device stopped sending data. Check the cable and the "
             "device's power.";
    case TransferResult::kProgramError:
      return "The capture failed because of a fault in this application.";
    case TransferResult::kForcedAbort:
      return "The capture was stopped immediately, discarding buffered data.";
  }
  return "The capture stopped for an unknown reason.";
}

}  // namespace ddd::capture
