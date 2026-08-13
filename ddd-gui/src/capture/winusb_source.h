/************************************************************************

    winusb_source.h

    Streaming from the device with WinUSB (Windows)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2024 Roger Sanders
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winusb.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "sample_source.h"
#include "usb_device.h"

namespace ddd::capture {

class ILogger;

// Reads the bulk pipe into ring slots using overlapped I/O.
//
// The same transfer geometry as the libusb backend — both call
// PlanTransferLayout() and neither has its own copy of the arithmetic — but a
// different way of waiting for it. WinUSB has no completion callback, so this
// keeps a fixed set of overlapped requests in flight and walks round them in
// submission order, reaping each in turn. Bulk reads on one pipe complete in
// order, so walking the ring in order is the same thing as reaping them as they
// finish.
//
// Two things here are not in the old backend:
//
// RAW_IO is set on the pipe, as before, because it measurably improves
// resilience under load. What is new is that the wait for a completion is
// bounded rather than indefinite. The old code called
// WinUsb_GetOverlappedResult with wait set, which returns when the transfer
// completes and never otherwise — so a device that stopped delivering without
// failing left the transfer thread blocked in the kernel with no way back, and
// the application hung. Waiting on the event with a timeout and looking around
// between waits is what lets the pipeline's stall watchdog actually stop this
// thread.
//
// The underflow probe is kept. After submitting a request it asks whether the
// previous one had already completed, which would mean a window in which
// nothing was listening to the pipe. Windows does not report that as an error —
// the data is simply gone — so the probe is the only evidence it happened.
//
// Thread-safety: as ISampleSource.
class WinUsbSource : public ISampleSource {
 public:
  // Takes an already-open device with its WinUSB interface initialised. Both
  // handles are closed by Finish().
  WinUsbSource(HANDLE device_handle, WINUSB_INTERFACE_HANDLE interface_handle,
               UCHAR pipe_id, size_t endpoint_max_packet_bytes,
               size_t maximum_transfer_bytes, const UsbSourceOptions& options,
               ILogger* logger);
  ~WinUsbSource() override;

  const char* Name() const override { return "winusb"; }

  DiskBufferRing::Geometry PlanGeometry(size_t queue_size_bytes) const override;

  TransferResult Prepare(const DiskBufferRing& ring) override;
  TransferResult Run(DiskBufferRing& ring, SourceControl& control) override;
  void Finish() override;

  const TransferLayout& layout() const { return layout_; }
  const std::string& last_error() const { return last_error_; }

 private:
  struct Transfer {
    OVERLAPPED overlapped = {};
    size_t slot_index = 0;
    size_t index_in_slot = 0;
    size_t byte_offset = 0;
    bool last_in_slot = false;
    bool submitted = false;
  };

  bool SubmitTransfer(Transfer& entry);

  // Wait for one request, looking around every so often so that an abort is
  // noticed even when the device has gone silent. Returns false if the wait
  // ended for any reason other than the request completing.
  bool AwaitTransfer(Transfer& entry, DiskBufferRing& ring,
                     SourceControl& control);

  void Fail(TransferResult result, std::string detail);

  HANDLE device_handle_ = nullptr;
  WINUSB_INTERFACE_HANDLE interface_handle_ = nullptr;
  UCHAR pipe_id_ = 0;
  size_t endpoint_max_packet_bytes_ = 0;
  size_t maximum_transfer_bytes_ = 0;
  UsbSourceOptions options_;
  ILogger* logger_ = nullptr;

  TransferLayout layout_;
  std::vector<Transfer> transfers_;

  DiskBufferRing* ring_ = nullptr;
  uint64_t discarded_transfers_ = 0;
  bool capture_complete_ = false;
  bool failed_ = false;
  bool pipe_aborted_ = false;

  TransferResult result_ = TransferResult::kSuccess;
  std::string last_error_;
};

}  // namespace ddd::capture

#endif  // _WIN32
