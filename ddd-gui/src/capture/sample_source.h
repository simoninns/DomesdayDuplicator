/************************************************************************

    sample_source.h

    Where samples come from
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "disk_buffer_ring.h"
#include "transfer_result.h"

namespace ddd::capture {

// What the orchestrator lets a source do while it runs.
//
// Deliberately narrow. A source can ask whether it should stop, report that it
// has, and count what it has delivered — and nothing else. Everything about
// files, sinks, validation and statistics is on the other side of this
// interface, which is what keeps a backend to the one job of getting bytes off
// the wire.
class SourceControl {
 public:
  SourceControl() = default;
  virtual ~SourceControl() = default;

  SourceControl(const SourceControl&) = delete;
  SourceControl& operator=(const SourceControl&) = delete;
  SourceControl(SourceControl&&) = delete;
  SourceControl& operator=(SourceControl&&) = delete;

  // Stop at the next slot boundary. A source that sees this finishes the slot
  // it is filling, hands it over, and returns.
  virtual bool StopRequested() const = 0;

  // Stop now and throw away what is in flight. Set when something has already
  // failed, so there is nothing worth finishing.
  virtual bool AbortRequested() const = 0;

  // Count a completed transfer. Purely for the statistics the user sees; the
  // orchestrator's watchdog also uses it to tell a working source from a
  // stalled one, so a source that stops calling this will be declared stalled
  // even if it thinks it is busy.
  virtual void AddCompletedTransfers(uint64_t count) = 0;

  // Log something. Never per transfer — see logger.h.
  virtual void Log(const std::string& message) = 0;
};

// A producer of device data.
//
// The source writes straight into the ring's slots and marks them full. It is
// not handed a buffer to fill and asked to return it, because the USB backends
// do not work that way: libusb and WinUSB both want a set of transfers
// submitted ahead of time, each pointing into memory that stays put, and
// several of them are in flight into different slots at once. An interface that
// took a buffer per call would force a copy of 80 MB/s to bridge the mismatch.
//
// Thread-safety: Run() is called on a thread of the orchestrator's making and
// owns the object for its duration. PlanGeometry() is called before that, on
// the orchestrator's thread.
class ISampleSource {
 public:
  ISampleSource() = default;
  virtual ~ISampleSource() = default;

  ISampleSource(const ISampleSource&) = delete;
  ISampleSource& operator=(const ISampleSource&) = delete;
  ISampleSource(ISampleSource&&) = delete;
  ISampleSource& operator=(ISampleSource&&) = delete;

  // A name for logs and error messages ("synthetic", "libusb", ...)
  virtual const char* Name() const = 0;

  // How this source wants the ring laid out for a given queue size. A USB
  // backend rounds the slot size to whole endpoint packets; a synthetic source
  // has no such constraint and takes the default.
  virtual DiskBufferRing::Geometry PlanGeometry(
      size_t queue_size_bytes) const = 0;

  // Prepare to run: open the device, claim the interface, allocate transfers.
  // Returns kSuccess when ready. Failing here is how a capture declines to
  // start rather than starting and immediately dying.
  virtual TransferResult Prepare(const DiskBufferRing& ring) = 0;

  // Fill slots until told to stop or something goes wrong.
  //
  // Returns the reason it stopped: kSuccess for a requested stop, or the
  // specific failure. Must return — a source that blocks forever on a device
  // that has gone quiet is exactly what the orchestrator's watchdog exists to
  // catch, but the watchdog can only abort the ring, so Run() has to notice
  // that and come back.
  virtual TransferResult Run(DiskBufferRing& ring, SourceControl& control) = 0;

  // Release whatever Prepare() acquired. Always called if Prepare() was, even
  // if Run() was not.
  virtual void Finish() = 0;
};

}  // namespace ddd::capture
