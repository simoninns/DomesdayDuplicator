/************************************************************************

    libusb_source.cpp

    Streaming from the device with libusb (Linux and macOS)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2024 Roger Sanders
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "libusb_source.h"

#include <utility>

#include "logger.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

// How long libusb waits for an event before returning to let the loop look
// around. Nothing on a deadline depends on it: transfers complete through the
// callback whenever they complete, and this only bounds how long an abort takes
// to be noticed once every transfer has already been cancelled.
constexpr int64_t kEventTimeoutMicroseconds = 100'000;

const char* TransferStatusName(int status) {
  switch (status) {
    case LIBUSB_TRANSFER_COMPLETED:
      return "completed";
    case LIBUSB_TRANSFER_ERROR:
      return "error";
    case LIBUSB_TRANSFER_TIMED_OUT:
      return "timed out";
    case LIBUSB_TRANSFER_CANCELLED:
      return "cancelled";
    case LIBUSB_TRANSFER_STALL:
      return "endpoint stalled";
    case LIBUSB_TRANSFER_NO_DEVICE:
      return "device disconnected";
    case LIBUSB_TRANSFER_OVERFLOW:
      return "overflow";
    default:
      return "unrecognised status";
  }
}

}  // namespace

LibUsbSource::LibUsbSource(libusb_context* context,
                           libusb_device_handle* handle, uint8_t endpoint,
                           size_t endpoint_max_packet_bytes,
                           const UsbSourceOptions& options, ILogger* logger)
    : context_(context),
      handle_(handle),
      endpoint_(endpoint),
      endpoint_max_packet_bytes_(endpoint_max_packet_bytes),
      options_(options),
      logger_(logger),
      interface_claimed_(true) {}

LibUsbSource::~LibUsbSource() { Finish(); }

DiskBufferRing::Geometry LibUsbSource::PlanGeometry(
    size_t queue_size_bytes) const {
  return DiskBufferRing::PlanGeometry(queue_size_bytes,
                                      endpoint_max_packet_bytes_);
}

TransferResult LibUsbSource::Prepare(const DiskBufferRing& ring) {
  layout_ = PlanTransferLayout(ring.slot_size_bytes(), ring.slot_count(),
                               endpoint_max_packet_bytes_, options_);
  if (!layout_.valid) {
    last_error_ =
        "The transfer layout could not be worked out: " + layout_.problem;
    return TransferResult::kProgramError;
  }

  transfers_.assign(layout_.transfer_count, Transfer{});

  // Where each transfer starts. They are laid out slot by slot rather than
  // striped, so that the transfers covering one slot are submitted
  // consecutively — which is what makes "the last transfer in a slot has
  // completed" mean "the whole slot has arrived". Bulk transfers on one
  // endpoint complete in submission order, and this is the only place that
  // relies on it.
  size_t slot_index = layout_.first_slot_index;
  size_t index_in_slot = 0;

  for (Transfer& entry : transfers_) {
    entry.owner = this;
    entry.transfer = libusb_alloc_transfer(0);
    if (entry.transfer == nullptr) {
      last_error_ = "libusb could not allocate a transfer structure";
      return TransferResult::kUsbTransferFailure;
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

  if (logger_ != nullptr) {
    logger_->Info(
        "libusb: " + std::to_string(layout_.transfer_count) + " transfers of " +
        std::to_string(layout_.transfer_bytes / 1024) + " KiB, " +
        std::to_string(layout_.transfers_per_slot) + " per buffer, spanning " +
        std::to_string(layout_.slot_span) + " buffers, discarding " +
        std::to_string(layout_.discard_slots) + " at the start");
  }

  return TransferResult::kSuccess;
}

bool LibUsbSource::SubmitTransfer(Transfer& entry) {
  libusb_fill_bulk_transfer(
      entry.transfer, handle_, endpoint_,
      ring_->SlotData(entry.slot_index) + entry.byte_offset,
      static_cast<int>(layout_.transfer_bytes),
      &LibUsbSource::CompletionTrampoline, &entry, 0);

  // A short packet is data loss, not the end of a message: the geometry makes
  // every transfer a whole number of packets, so anything shorter means the
  // device or the host controller dropped something. Failing the transfer is
  // how that becomes an error rather than a silently truncated buffer.
  entry.transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;

  const int submitted = libusb_submit_transfer(entry.transfer);
  if (submitted != 0) {
    // ENOMEM here is almost never actual memory. On Linux, usbfs caps the
    // buffer a single process may have mapped for transfers — 16 MB by default
    // — and asking for more fails exactly like this. Saying so turns an
    // unexplained failure into a one-line fix.
    if (submitted == LIBUSB_ERROR_NO_MEM) {
      Fail(TransferResult::kUsbMemoryLimit,
           "The kernel refused to allocate a USB transfer buffer. This is "
           "usually the usbfs memory limit rather than actual memory: reduce "
           "the transfer queue size, or raise "
           "/sys/module/usbcore/parameters/usbfs_memory_mb.");
    } else {
      Fail(TransferResult::kUsbTransferFailure,
           std::string("Submitting a USB transfer failed: ") +
               libusb_error_name(submitted));
    }
    return false;
  }

  entry.submitted = true;
  ++transfers_in_flight_;
  return true;
}

TransferResult LibUsbSource::Run(DiskBufferRing& ring, SourceControl& control) {
  ring_ = &ring;
  control_ = &control;
  transfers_in_flight_ = 0;
  discarded_transfers_ = 0;
  capture_complete_ = false;
  failed_ = false;
  result_ = TransferResult::kSuccess;
  last_error_.clear();

  for (Transfer& entry : transfers_) {
    entry.submitted = false;
    entry.cancelled = false;
    if (!SubmitTransfer(entry)) {
      break;
    }
  }

  timeval timeout{};
  timeout.tv_sec = 0;
  timeout.tv_usec =
      static_cast<decltype(timeout.tv_usec)>(kEventTimeoutMicroseconds);

  while (!failed_ && transfers_in_flight_ > 0) {
    libusb_handle_events_timeout_completed(context_, &timeout, nullptr);

    // An abort has to be noticed here as well as in the callback: if the device
    // has gone quiet, no callback will run again and only this loop is left to
    // see it.
    if (control.AbortRequested() || ring.AbortRequested()) {
      Fail(TransferResult::kForcedAbort, "The capture was stopped by force");
    }
  }

  // Everything still outstanding has to be cancelled and reaped before the
  // transfer structures can be freed, or libusb will write into memory this
  // object no longer owns.
  if (transfers_in_flight_ > 0) {
    for (Transfer& entry : transfers_) {
      if (!entry.submitted) {
        continue;
      }
      entry.cancelled = true;
      const int cancelled = libusb_cancel_transfer(entry.transfer);
      // NOT_FOUND means the transfer completed between the check and the
      // cancel, which is ordinary rather than a problem.
      if (cancelled != 0 && cancelled != LIBUSB_ERROR_NOT_FOUND &&
          logger_ != nullptr) {
        logger_->Debug(std::string("libusb: cancelling a transfer returned ") +
                       libusb_error_name(cancelled));
      }
    }

    while (transfers_in_flight_ > 0) {
      libusb_handle_events_timeout_completed(context_, &timeout, nullptr);
    }
  }

  ring_ = nullptr;
  control_ = nullptr;

  return result_;
}

void LIBUSB_CALL LibUsbSource::CompletionTrampoline(libusb_transfer* transfer) {
  Transfer* const entry = static_cast<Transfer*>(transfer->user_data);
  entry->owner->OnCompletion(*entry);
}

void LibUsbSource::OnCompletion(Transfer& entry) {
  entry.submitted = false;
  --transfers_in_flight_;

  if (entry.transfer->status != LIBUSB_TRANSFER_COMPLETED) {
    // A cancellation we asked for is not a failure. Anything else is.
    if (entry.cancelled &&
        entry.transfer->status == LIBUSB_TRANSFER_CANCELLED) {
      return;
    }

    const bool disconnected =
        entry.transfer->status == LIBUSB_TRANSFER_NO_DEVICE;
    Fail(disconnected ? TransferResult::kConnectionFailure
                      : TransferResult::kUsbTransferFailure,
         std::string("A USB transfer ended with '") +
             TransferStatusName(entry.transfer->status) + "'");
    return;
  }

  if (entry.transfer->actual_length != entry.transfer->length) {
    Fail(TransferResult::kUsbTransferFailure,
         "A USB transfer returned " +
             std::to_string(entry.transfer->actual_length) + " bytes when " +
             std::to_string(entry.transfer->length) + " were asked for");
    return;
  }

  if (control_->AbortRequested()) {
    Fail(TransferResult::kForcedAbort, "The capture was stopped by force");
    return;
  }

  control_->AddCompletedTransfers(1);

  const bool stop_requested = control_->StopRequested();

  if (discarded_transfers_ < layout_.discard_transfers) {
    // Still clearing whatever was already in flight when the device was opened.
    ++discarded_transfers_;

    // A stop during the discard still has to end the run, or nothing would ever
    // reach the handover below to notice it.
    if (stop_requested && entry.last_in_slot) {
      capture_complete_ = true;
    }
  } else if (entry.last_in_slot) {
    switch (ring_->MarkSlotFull(entry.slot_index)) {
      case DiskBufferRing::FillResult::kHandedOver:
        break;
      case DiskBufferRing::FillResult::kOverflow:
        // Reachable only if the wait below failed to hold a transfer back,
        // which would be a bug here rather than a slow machine — a slow machine
        // blocks instead.
        Fail(TransferResult::kProgramError,
             "A buffer was handed over while the previous one was still being "
             "processed");
        return;
      case DiskBufferRing::FillResult::kAborted:
        Fail(TransferResult::kForcedAbort, "The capture was stopped by force");
        return;
    }

    if (stop_requested) {
      capture_complete_ = true;
    }
  }

  // Nothing more goes on the wire once the run is over, however it ended.
  //
  // The failure half of this is not defensive: without it, a transfer that had
  // already completed successfully but had not yet been reaped would be
  // resubmitted during the shutdown sweep, after the sweep had decided which
  // transfers to cancel. The reap loop would then be waiting for a transfer
  // created after it started, and would wait for it forever — the transfer
  // thread spinning in libusb with the application apparently hung, which is
  // precisely the failure this engine exists to have got rid of.
  if (capture_complete_ || failed_) {
    return;
  }

  const size_t next_slot =
      (entry.slot_index + layout_.slot_span) % ring_->slot_count();
  entry.slot_index = next_slot;

  // Only the first transfer of a slot waits. The rest are aimed at a slot the
  // first one has already secured, so checking again would cost an atomic load
  // per transfer for an answer that cannot have changed.
  if (entry.index_in_slot == 0 && !ring_->WaitForSlotFree(next_slot)) {
    Fail(TransferResult::kForcedAbort, "The capture was stopped by force");
    return;
  }

  SubmitTransfer(entry);
}

void LibUsbSource::Fail(TransferResult result, std::string detail) {
  if (!failed_) {
    failed_ = true;
    result_ = result;
    last_error_ = std::move(detail);
    if (logger_ != nullptr && result != TransferResult::kForcedAbort) {
      logger_->Error("libusb: " + last_error_);
    }
  }
}

void LibUsbSource::Finish() {
  for (Transfer& entry : transfers_) {
    if (entry.transfer != nullptr) {
      libusb_free_transfer(entry.transfer);
      entry.transfer = nullptr;
    }
  }
  transfers_.clear();

  if (handle_ != nullptr) {
    if (interface_claimed_) {
      libusb_release_interface(handle_, kInterfaceNumber);
      interface_claimed_ = false;
    }
    libusb_close(handle_);
    handle_ = nullptr;
  }
}

}  // namespace ddd::capture
