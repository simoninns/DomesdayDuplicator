/************************************************************************

    libusb_source.h

    Streaming from the device with libusb (Linux and macOS)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2024 Roger Sanders
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <libusb.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "sample_source.h"
#include "usb_device.h"

namespace ddd::capture {

class ILogger;

// Reads the bulk endpoint into ring slots, several transfers at a time.
//
// The transfer geometry is ported from the old engine rather than redesigned,
// and deliberately so: it has carried hours-long bit-perfect captures in the
// field, and field evidence outranks anything that could be argued from first
// principles here. What is new is where the data lands. The old engine owned
// its own buffers; this writes straight into the ring, so there is no copy
// between the USB stack and the processing thread at all.
//
// The one piece of the old design worth restating, because it looks like a bug
// until it is explained: the completion callback can block. When a transfer
// completes and the slot it would be resubmitted into is still full, the
// callback waits for the consumer to catch up, and libusb's event handling
// waits with it. That is intentional. The alternative — dropping the transfer
// and carrying on — would silently lose samples, and the whole point of the
// sequence markers is that samples are never silently lost. If the consumer
// really cannot keep up, the transfers already submitted continue to be filled
// by the kernel, the device's 64 KB FIFO eventually overruns, and the sequence
// validator reports exactly that. A stall becomes a reported error rather than
// a quiet corruption.
//
// Thread-safety: as ISampleSource. Completion callbacks run on whichever thread
// is inside Run(), so the state they touch needs no locking.
class LibUsbSource : public ISampleSource {
 public:
  // Takes an already-open handle with interface kInterfaceNumber claimed. The
  // handle is closed by Finish(), which the pipeline always calls.
  LibUsbSource(libusb_context* context, libusb_device_handle* handle,
               uint8_t endpoint, size_t endpoint_max_packet_bytes,
               const UsbSourceOptions& options, ILogger* logger);
  ~LibUsbSource() override;

  const char* Name() const override { return "libusb"; }

  DiskBufferRing::Geometry PlanGeometry(size_t queue_size_bytes) const override;

  TransferResult Prepare(const DiskBufferRing& ring) override;
  TransferResult Run(DiskBufferRing& ring, SourceControl& control) override;
  void Finish() override;

  // What Prepare() worked out. Exposed so a caller can log it and a test can
  // check it against PlanTransferLayout().
  const TransferLayout& layout() const { return layout_; }

  // The most specific description of what went wrong, when Run() returned a
  // failure.
  const std::string& last_error() const { return last_error_; }

 private:
  // One in-flight transfer and where it is aimed.
  struct Transfer {
    LibUsbSource* owner = nullptr;
    libusb_transfer* transfer = nullptr;
    size_t slot_index = 0;
    size_t index_in_slot = 0;
    size_t byte_offset = 0;
    bool last_in_slot = false;
    bool submitted = false;
    bool cancelled = false;
  };

  static void LIBUSB_CALL CompletionTrampoline(libusb_transfer* transfer);
  void OnCompletion(Transfer& entry);

  // Record the first failure. Later ones are consequences of it.
  void Fail(TransferResult result, std::string detail);

  bool SubmitTransfer(Transfer& entry);

  libusb_context* context_ = nullptr;
  libusb_device_handle* handle_ = nullptr;
  uint8_t endpoint_ = 0;
  size_t endpoint_max_packet_bytes_ = 0;
  UsbSourceOptions options_;
  ILogger* logger_ = nullptr;

  TransferLayout layout_;
  std::vector<Transfer> transfers_;

  // Run() state, touched only by the thread inside Run() and its callbacks
  DiskBufferRing* ring_ = nullptr;
  SourceControl* control_ = nullptr;
  size_t transfers_in_flight_ = 0;
  uint64_t discarded_transfers_ = 0;
  bool capture_complete_ = false;
  bool failed_ = false;

  TransferResult result_ = TransferResult::kSuccess;
  std::string last_error_;

  bool interface_claimed_ = false;
};

}  // namespace ddd::capture
