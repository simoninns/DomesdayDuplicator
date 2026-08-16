/************************************************************************

    winusb_source.cpp

    Streaming from the device with WinUSB (Windows)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2024 Roger Sanders
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "winusb_source.h"

#ifdef _WIN32

#include <algorithm>
#include <array>
#include <span>
#include <utility>

#include "logger.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

// How long to wait on a completion before looking around. Only the resolution
// at which an abort is noticed on a device that has gone silent; a working
// device completes a transfer every couple of milliseconds and never reaches
// the timeout at all.
constexpr DWORD kCompletionWaitMilliseconds = 100;

// Vendor request, device to host. The same value the other backends use, spelt
// out here because this file cannot reach libusb's constants.
constexpr UCHAR kVendorReadRequestType = 0xC0;

// Refusals before the device is left alone for the rest of the run.
constexpr int kTelemetryMaxFailures = 2;

}  // namespace

WinUsbSource::WinUsbSource(HANDLE device_handle,
                           WINUSB_INTERFACE_HANDLE interface_handle,
                           UCHAR pipe_id, size_t endpoint_max_packet_bytes,
                           size_t maximum_transfer_bytes,
                           const UsbSourceOptions& options, ILogger* logger)
    : device_handle_(device_handle),
      interface_handle_(interface_handle),
      pipe_id_(pipe_id),
      endpoint_max_packet_bytes_(endpoint_max_packet_bytes),
      maximum_transfer_bytes_(maximum_transfer_bytes),
      options_(options),
      logger_(logger) {}

WinUsbSource::~WinUsbSource() { Finish(); }

DiskBufferRing::Geometry WinUsbSource::PlanGeometry(
    size_t queue_size_bytes) const {
  return DiskBufferRing::PlanGeometry(queue_size_bytes,
                                      endpoint_max_packet_bytes_);
}

TransferResult WinUsbSource::Prepare(const DiskBufferRing& ring) {
  UsbSourceOptions options = options_;

  // Unlike libusb, WinUSB will say how much it is prepared to move in one
  // request, and exceeding it fails the read rather than splitting it. A slot
  // larger than the driver's limit therefore has to be read in pieces whatever
  // the user asked for.
  if (!options.small_transfers && maximum_transfer_bytes_ > 0 &&
      ring.slot_size_bytes() > maximum_transfer_bytes_) {
    options.small_transfers = true;
    if (logger_ != nullptr) {
      logger_->Info(
          "The driver will not move a whole buffer in one request (its limit "
          "is " +
          std::to_string(maximum_transfer_bytes_ / 1024) +
          " KiB), so smaller transfers are being used instead");
    }
  }

  layout_ = PlanTransferLayout(ring.slot_size_bytes(), ring.slot_count(),
                               endpoint_max_packet_bytes_, options);
  if (!layout_.valid) {
    last_error_ =
        "The transfer layout could not be worked out: " + layout_.problem;
    return TransferResult::kProgramError;
  }

  transfers_.assign(layout_.transfer_count, Transfer{});

  size_t slot_index = layout_.first_slot_index;
  size_t index_in_slot = 0;

  for (Transfer& entry : transfers_) {
    // Manual reset, initially unsignalled. The I/O manager clears it when a
    // request is queued and sets it on completion, so nothing here has to reset
    // it between reads.
    entry.overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (entry.overlapped.hEvent == nullptr) {
      last_error_ = "A completion event could not be created";
      return TransferResult::kProgramError;
    }

    entry.slot_index = slot_index;
    entry.index_in_slot = index_in_slot;
    entry.byte_offset = index_in_slot * layout_.transfer_bytes;

    ++index_in_slot;
    entry.last_in_slot = index_in_slot >= layout_.transfers_per_slot;
    if (entry.last_in_slot) {
      index_in_slot = 0;
      slot_index = (slot_index + 1) % ring.slot_count();
    }
  }

  // RAW_IO gives the best throughput the driver offers and is measurably more
  // resilient under CPU load. It requires every read to be a whole number of
  // packets and no larger than the pipe's maximum, both of which the layout
  // guarantees.
  BOOL raw_io = TRUE;
  if (WinUsb_SetPipePolicy(interface_handle_, pipe_id_, RAW_IO,
                           static_cast<ULONG>(sizeof(raw_io)),
                           &raw_io) != TRUE) {
    last_error_ = "Enabling raw I/O on the capture pipe failed with error " +
                  std::to_string(GetLastError());
    return TransferResult::kUsbTransferFailure;
  }

  if (logger_ != nullptr) {
    logger_->Info(
        "winusb: " + std::to_string(layout_.transfer_count) + " transfers of " +
        std::to_string(layout_.transfer_bytes / 1024) + " KiB, " +
        std::to_string(layout_.transfers_per_slot) + " per buffer, spanning " +
        std::to_string(layout_.slot_span) + " buffers, discarding " +
        std::to_string(layout_.discard_slots) + " at the start");
  }

  return TransferResult::kSuccess;
}

bool WinUsbSource::SubmitTransfer(Transfer& entry) {
  const BOOL queued = WinUsb_ReadPipe(
      interface_handle_, pipe_id_,
      ring_->SlotData(entry.slot_index) + entry.byte_offset,
      static_cast<ULONG>(layout_.transfer_bytes), nullptr, &entry.overlapped);

  if (queued != TRUE) {
    const DWORD error = GetLastError();
    if (error != ERROR_IO_PENDING) {
      Fail(TransferResult::kUsbTransferFailure,
           "Queuing a USB read failed with error " + std::to_string(error));
      return false;
    }
  }

  entry.submitted = true;
  return true;
}

bool WinUsbSource::AwaitTransfer(Transfer& entry, DiskBufferRing& ring,
                                 SourceControl& control) {
  while (true) {
    const DWORD waited = WaitForSingleObject(entry.overlapped.hEvent,
                                             kCompletionWaitMilliseconds);

    if (waited == WAIT_OBJECT_0) {
      return true;
    }

    if (waited != WAIT_TIMEOUT) {
      Fail(TransferResult::kUsbTransferFailure,
           "Waiting for a USB read failed with error " +
               std::to_string(GetLastError()));
      return false;
    }

    // The timeout is the whole reason this loop exists: it is the only moment
    // at which a thread waiting on a device that has stopped delivering can
    // find out that it should give up.
    if (control.AbortRequested() || ring.AbortRequested()) {
      if (!pipe_aborted_) {
        pipe_aborted_ = true;
        // Completes every outstanding request with ERROR_OPERATION_ABORTED,
        // which is what releases this wait and every later one.
        WinUsb_AbortPipe(interface_handle_, pipe_id_);
      }
      Fail(TransferResult::kForcedAbort, "The capture was stopped by force");
      return false;
    }
  }
}

TransferResult WinUsbSource::Run(DiskBufferRing& ring, SourceControl& control) {
  ring_ = &ring;
  discarded_transfers_ = 0;
  capture_complete_ = false;
  failed_ = false;
  pipe_aborted_ = false;
  result_ = TransferResult::kSuccess;
  last_error_.clear();

  for (Transfer& entry : transfers_) {
    entry.submitted = false;
    if (!SubmitTransfer(entry)) {
      break;
    }
  }

  telemetry_primed_ = false;
  telemetry_failures_ = 0;
  telemetry_due_ = std::chrono::steady_clock::now();

  size_t current = 0;
  bool drained = false;

  while (!failed_ && !drained) {
    PollTelemetry();

    Transfer& entry = transfers_[current];

    if (!entry.submitted) {
      // Walked all the way round to a request that was never resubmitted, so
      // everything in flight has been reaped.
      if (capture_complete_) {
        drained = true;
        continue;
      }
    } else {
      if (!AwaitTransfer(entry, ring, control)) {
        continue;
      }

      DWORD transferred = 0;
      const BOOL completed = WinUsb_GetOverlappedResult(
          interface_handle_, &entry.overlapped, &transferred, FALSE);
      entry.submitted = false;

      if (completed != TRUE) {
        const DWORD error = GetLastError();
        if (error == ERROR_OPERATION_ABORTED) {
          Fail(TransferResult::kForcedAbort,
               "The capture was stopped by force");
        } else if (error == ERROR_DEVICE_NOT_CONNECTED ||
                   error == ERROR_NO_SUCH_DEVICE ||
                   error == ERROR_GEN_FAILURE) {
          Fail(TransferResult::kConnectionFailure,
               "The device was disconnected (error " + std::to_string(error) +
                   ")");
        } else {
          Fail(TransferResult::kUsbTransferFailure,
               "A USB read failed with error " + std::to_string(error));
        }
        continue;
      }

      if (transferred != layout_.transfer_bytes) {
        Fail(TransferResult::kUsbTransferFailure,
             "A USB read returned " + std::to_string(transferred) +
                 " bytes when " + std::to_string(layout_.transfer_bytes) +
                 " were asked for");
        continue;
      }

      if (control.AbortRequested()) {
        if (!pipe_aborted_) {
          pipe_aborted_ = true;
          WinUsb_AbortPipe(interface_handle_, pipe_id_);
        }
        Fail(TransferResult::kForcedAbort, "The capture was stopped by force");
        continue;
      }

      control.AddCompletedTransfers(1);

      const bool stop_requested = control.StopRequested();

      if (discarded_transfers_ < layout_.discard_transfers) {
        ++discarded_transfers_;
        if (stop_requested && entry.last_in_slot) {
          capture_complete_ = true;
        }
      } else if (entry.last_in_slot) {
        switch (ring.MarkSlotFull(entry.slot_index)) {
          case DiskBufferRing::FillResult::kHandedOver:
            break;
          case DiskBufferRing::FillResult::kOverflow:
            Fail(TransferResult::kProgramError,
                 "A buffer was handed over while the previous one was still "
                 "being processed");
            continue;
          case DiskBufferRing::FillResult::kAborted:
            Fail(TransferResult::kForcedAbort,
                 "The capture was stopped by force");
            continue;
        }

        if (stop_requested) {
          capture_complete_ = true;
        }
      }
    }

    if (!capture_complete_) {
      const size_t next_slot =
          (entry.slot_index + layout_.slot_span) % ring.slot_count();
      entry.slot_index = next_slot;

      if (entry.index_in_slot == 0 && !ring.WaitForSlotFree(next_slot)) {
        Fail(TransferResult::kForcedAbort, "The capture was stopped by force");
        continue;
      }

      if (!SubmitTransfer(entry)) {
        continue;
      }

      // The underflow probe. If the request submitted just before this one has
      // already completed, then between its completion and this submission
      // there was a moment with nothing outstanding on the pipe — and whatever
      // the device sent in that moment is gone. Windows reports no error for
      // it, so asking is the only way to find out.
      //
      // Skipped during the discard phase, where the queue is still filling up
      // and gaps are expected rather than significant.
      if (discarded_transfers_ >= layout_.discard_transfers) {
        Transfer& previous =
            transfers_[(current + transfers_.size() - 1) % transfers_.size()];
        if (previous.submitted) {
          DWORD transferred = 0;
          const BOOL finished = WinUsb_GetOverlappedResult(
              interface_handle_, &previous.overlapped, &transferred, FALSE);
          const DWORD error = GetLastError();
          if (finished == TRUE) {
            Fail(TransferResult::kHostUnderflow,
                 "A read request completed before its successor was queued, so "
                 "the device had nowhere to put its data");
            continue;
          }
          if (error != ERROR_IO_INCOMPLETE) {
            Fail(TransferResult::kUsbTransferFailure,
                 "Checking a USB read failed with error " +
                     std::to_string(error));
            continue;
          }
        }
      }
    }

    current = (current + 1) % transfers_.size();
  }

  // Nothing may still be writing into the ring's memory once this returns.
  if (failed_) {
    bool outstanding = false;
    for (const Transfer& entry : transfers_) {
      outstanding = outstanding || entry.submitted;
    }
    if (outstanding) {
      if (!pipe_aborted_) {
        pipe_aborted_ = true;
        WinUsb_AbortPipe(interface_handle_, pipe_id_);
      }
      for (Transfer& entry : transfers_) {
        if (!entry.submitted) {
          continue;
        }
        DWORD transferred = 0;
        WinUsb_GetOverlappedResult(interface_handle_, &entry.overlapped,
                                   &transferred, TRUE);
        entry.submitted = false;
      }
    }
  }

  ring_ = nullptr;
  return result_;
}

void WinUsbSource::Fail(TransferResult result, std::string detail) {
  if (!failed_) {
    failed_ = true;
    result_ = result;
    last_error_ = std::move(detail);
    if (logger_ != nullptr && result != TransferResult::kForcedAbort) {
      logger_->Error("winusb: " + last_error_);
    }
  }
}

void WinUsbSource::PollTelemetry() {
  if (telemetry_failures_ >= kTelemetryMaxFailures) {
    return;
  }

  const std::chrono::steady_clock::time_point now =
      std::chrono::steady_clock::now();
  if (now < telemetry_due_) {
    return;
  }
  telemetry_due_ =
      now + std::chrono::milliseconds(kTelemetryIntervalMilliseconds);

  std::array<UCHAR, kTelemetryBlockLength> block{};

  WINUSB_SETUP_PACKET setup = {};
  setup.RequestType = kVendorReadRequestType;
  setup.Request = kRegisterReadRequest;
  setup.Value = kRegisterTelemetryId;
  setup.Index = 0;
  setup.Length = static_cast<USHORT>(block.size());

  ULONG transferred = 0;
  if (WinUsb_ControlTransfer(interface_handle_, setup, block.data(),
                             static_cast<ULONG>(block.size()), &transferred,
                             nullptr) != TRUE) {
    // Deliberately not a capture failure. This is an instrument, and a capture
    // that stopped because its instrument could not be read would be the
    // instrument doing precisely the damage it exists to detect. A stall is
    // how firmware without the register requests answers.
    ++telemetry_failures_;
    return;
  }

  if (transferred < block.size()) {
    ++telemetry_failures_;
    return;
  }

  const FpgaTelemetry telemetry = ParseFpgaTelemetry(std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(block.data()), block.size()));

  // The first reading of a run is thrown away — see telemetry_primed_. What it
  // is good for is having taken place: the read is what clears the device's
  // interval counters, so the reading after it covers this capture and nothing
  // before it.
  if (!telemetry_primed_) {
    telemetry_primed_ = true;
    return;
  }

  telemetry_.Publish(telemetry);
}

void WinUsbSource::Finish() {
  for (Transfer& entry : transfers_) {
    if (entry.overlapped.hEvent != nullptr) {
      CloseHandle(entry.overlapped.hEvent);
      entry.overlapped.hEvent = nullptr;
    }
  }
  transfers_.clear();

  if (interface_handle_ != nullptr) {
    WinUsb_Free(interface_handle_);
    interface_handle_ = nullptr;
  }
  if (device_handle_ != nullptr) {
    CloseHandle(device_handle_);
    device_handle_ = nullptr;
  }
}

}  // namespace ddd::capture

#endif  // _WIN32
