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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "monitor_tap.h"
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
  //
  // The lease is the backend's token saying this context is in use. It is
  // opaque here and only has to be kept: the backend recycles its libusb
  // context when the kernel and libusb disagree about what is attached, and a
  // context recycled underneath a running capture would close this handle from
  // another thread. Holding a copy for as long as the handle lives is what
  // makes that impossible — see sysfs_device_list.h for why the recycling
  // exists at all.
  LibUsbSource(libusb_context* context, std::shared_ptr<const void> lease,
               libusb_device_handle* handle, uint8_t endpoint,
               size_t endpoint_max_packet_bytes,
               const UsbSourceOptions& options, ILogger* logger);
  ~LibUsbSource() override;

  const char* Name() const override { return "libusb"; }

  DiskBufferRing::Geometry PlanGeometry(size_t queue_size_bytes) const override;

  TransferResult Prepare(const DiskBufferRing& ring) override;
  TransferResult Run(DiskBufferRing& ring, SourceControl& control) override;
  void Finish() override;

  FpgaTelemetry DeviceTelemetry() const override { return telemetry_.Read(); }

  // How often the gateware's capture buffer is read, in milliseconds.
  //
  // Four times a second. Each reading costs one control request of 23 bytes,
  // which the FX3 answers by bit-banging its register link at about 80
  // microseconds a byte — so this is roughly two milliseconds of the device's
  // endpoint-0 thread every 250, and nothing at all of the path the samples
  // take, which is a hardware DMA channel the FX3's CPU never touches.
  //
  // Faster would not say more: each reading already reports the *peak* of its
  // own interval, so nothing is missed between them, and the peak of a shorter
  // interval is a smaller number rather than a better one.
  static constexpr int kTelemetryIntervalMilliseconds = 250;

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

  // Ask the gateware how its capture buffer is doing, at most every
  // kTelemetryIntervalMilliseconds.
  //
  // Asynchronous, and submitted from inside the event loop, which is the whole
  // of why it cannot cost a sample: the request is reaped by the same
  // libusb_handle_events() call that reaps the bulk transfers, so nothing waits
  // for it and the completion callback that is allowed to block is never
  // blocked by it. A synchronous request from another thread would contend with
  // that callback for libusb's event lock, which is the one arrangement that
  // could turn a slow reading into a stalled capture.
  void PollTelemetry();
  static void LIBUSB_CALL TelemetryTrampoline(libusb_transfer* transfer);
  void OnTelemetryCompletion();

  // Record the first failure. Later ones are consequences of it.
  void Fail(TransferResult result, std::string detail);

  bool SubmitTransfer(Transfer& entry);

  libusb_context* context_ = nullptr;

  // Kept, never read. See the constructor.
  std::shared_ptr<const void> lease_;
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

  // The buffer telemetry request and what it lands in. One transfer, reused for
  // every reading, and never more than one of them in flight.
  libusb_transfer* telemetry_transfer_ = nullptr;
  std::vector<uint8_t> telemetry_buffer_;
  bool telemetry_in_flight_ = false;
  std::chrono::steady_clock::time_point telemetry_due_{};

  // The first reading of a run is thrown away. Its interval reaches back to
  // whenever the registers were last read, which is before this capture
  // started — and with nothing draining the device, its buffer sits full and
  // overflowing. Publishing that would open every capture with a report of a
  // catastrophe that never happened.
  bool telemetry_primed_ = false;

  // Refusals, counted so that a device that cannot answer is asked a couple of
  // times and then left alone. Gateware without the instrument answers a read
  // of an unmapped address with zeros rather than a stall, so this is really
  // about firmware that predates the register requests, where every poll is a
  // stall on endpoint 0 for nothing.
  int telemetry_failures_ = 0;

  TelemetryPublisher telemetry_;
};

}  // namespace ddd::capture
