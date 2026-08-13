/************************************************************************

    transfer_result.h

    How a capture ended
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

namespace ddd::capture {

// Why a capture stopped.
//
// The taxonomy is deliberately specific rather than a single "failed". A user
// who has just lost forty minutes of a disc needs to know whether to free disk
// space, change a USB cable, raise a kernel limit or close the application that
// was competing for the CPU — and those are four different codes, not one.
enum class TransferResult {
  // Still going. The initial state, and never a final one.
  kRunning,

  kSuccess,

  // The output file could not be created — permissions, a path that does not
  // exist, a full volume.
  kFileCreationError,

  // A write to the output file failed partway through.
  kFileWriteError,

  // The producer filled a slot the consumer had not emptied. The machine could
  // not keep up, and samples have been overwritten before they were written
  // out. This is the failure the whole ring exists to detect.
  kBufferOverflow,

  // The device could not be opened or was lost mid-capture.
  kConnectionFailure,

  // A USB transfer failed, or returned fewer bytes than were asked for. Short
  // reads are fatal by design: the transfer geometry makes every transfer a
  // whole number of packets, so a short one is data loss and not a boundary.
  kUsbTransferFailure,

  // Windows only. A read request completed before its successor was submitted,
  // so for a moment nothing was listening to the pipe and the device had
  // nowhere to put its data.
  //
  // Distinct from kBufferOverflow, which is the ring filling up because the
  // disk could not keep up. This is the host briefly not asking for data at
  // all, and the remedy is different: fewer, larger transfers, or a machine
  // less busy elsewhere. Windows does not report it — the samples are simply
  // gone — so the backend probes for it, and this is what the probe says.
  kHostUnderflow,

  // Linux only. The kernel's usbfs memory limit is lower than the queue the
  // capture asked for. Distinct from a generic transfer failure because the fix
  // is one sysfs write, and saying so is the difference between a solved
  // problem and an abandoned one.
  kUsbMemoryLimit,

  // The sequence counters did not follow. Samples are missing.
  kSequenceMismatch,

  // Test mode only: the device's ramp broke. Everything from the ADC to the
  // file writer is in scope.
  kVerificationError,

  // Nothing arrived for longer than the watchdog allows. A device that stops
  // delivering without failing a transfer would otherwise hang the capture
  // indefinitely, which is worse than a reported error.
  kSourceStalled,

  // A bug here, not a condition out in the world.
  kProgramError,

  // Stopped by force rather than gracefully, so the tail of the data was
  // discarded.
  kForcedAbort,
};

const char* TransferResultName(TransferResult result);

// A one-line explanation suitable for showing a user. The names above are for
// logs and tests; these are for the person who has to decide what to do next.
const char* TransferResultDescription(TransferResult result);

// Whether a result means the capture is over. Everything except kRunning does.
inline bool TransferFinished(TransferResult result) {
  return result != TransferResult::kRunning;
}

// Whether a result means the capture is over and went wrong.
inline bool TransferFailed(TransferResult result) {
  return result != TransferResult::kRunning &&
         result != TransferResult::kSuccess;
}

}  // namespace ddd::capture
