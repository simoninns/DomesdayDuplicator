/************************************************************************

    synthetic_source.h

    A device that is not there
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "sample_source.h"

namespace ddd::capture {

// Generates the device's stream in software, at whatever rate is asked for,
// including the real one.
//
// This is the single largest thing the old application could not do. Every
// property of the capture path that matters — that the ring hands over cleanly
// under contention, that an overflow is detected rather than silently
// tolerated, that a sequence break stops the capture, that a sink can be
// attached mid-stream without losing a sample — was previously provable only by
// attaching hardware and hoping the fault reproduced. Here they are ordinary
// tests that run on every push, and the failures can be *asked for* rather than
// waited for.
//
// It is not a substitute for the hardware procedure in TESTING.md §5. It cannot
// be: it exercises everything except the device, the cable and the USB stack,
// which is where the interesting failures live. What it does is make everything
// on this side of the wire cheap to be sure about, so that a hardware session
// is spent on hardware questions.
//
// Thread-safety: as ISampleSource. The fault-injection setters are for the test
// that owns the object, before Run() starts.
class SyntheticSource : public ISampleSource {
 public:
  // What the generated samples look like
  enum class Pattern {
    // The FPGA's test-pattern ramp: 0, 1, 2, ... wrapping at kRampLength. This
    // is what makes an end-to-end integrity check possible, because every
    // sample's correct value is known in advance.
    kRamp,

    // A sine wave near the middle of the range. Nothing verifies it; it exists
    // so the monitoring panels have something recognisable to draw when there
    // is no device to hand.
    kSine,
  };

  // Faults the source can be told to produce, so the pipeline's handling of
  // them is tested rather than assumed.
  enum class Fault {
    kNone,

    // Skip a sequence counter value. The validator must notice on the very next
    // sample and stop the capture with kSequenceMismatch.
    kSequenceBreak,

    // Skip a step in the test-pattern ramp, leaving the sequence counters
    // intact. Only meaningful with Pattern::kRamp and the pipeline in test
    // mode, and that is exactly the point of it: this is the corruption the
    // sequence markers cannot see, because the markers are still perfectly in
    // order. Only the ramp check finds it, and the result is
    // kVerificationError rather than kSequenceMismatch.
    kRampBreak,

    // Deliver fewer bytes than a slot holds. Stands in for a short USB packet,
    // which is fatal by design.
    kShortDelivery,

    // Stop producing entirely, without failing. Nothing in the old engine could
    // detect this, and the capture would hang; here the orchestrator's watchdog
    // must turn it into kSourceStalled.
    kStall,

    // Report a transfer failure, as a USB backend would on a lost device.
    kTransferFailure,
  };

  struct Options {
    Pattern pattern = Pattern::kRamp;

    // Bytes per second to generate. 0 means as fast as the machine can, which
    // is what the correctness tests want; kWireBytesPerSecond is what the soak
    // test wants.
    uint64_t rate_bytes_per_second = 0;

    // Stop after this many slots have been handed over. 0 means run until
    // asked to stop.
    uint64_t slot_limit = 0;

    // Slots to discard before handing any over.
    //
    // The real device is already streaming when the host opens it, so the first
    // transfers hold whatever was mid-flight — a partial packet, a stale
    // buffer, a sequence phase that has nothing to do with what follows.
    // Discarding a few is how the old engine avoided starting every capture
    // with a spurious sequence error, and the synthetic source honours the same
    // setting so that the discard logic is exercised rather than bypassed.
    uint64_t discard_slots = 0;

    Fault fault = Fault::kNone;

    // Which slot the fault appears in, counted from the first one handed over.
    uint64_t fault_at_slot = 4;

    // Override the ring geometry. Zero means the standard 2 MB slots and the
    // queue size the pipeline was given.
    //
    // These exist so that a test of the pipeline's behaviour runs in
    // milliseconds and a few kilobytes instead of seconds and 64 MB. A test
    // that had to allocate and fill a real queue to find out whether a sink
    // swap lands on a buffer boundary would be slow enough that nobody ran it,
    // and the swap logic does not care how big the buffers are.
    size_t slot_size_bytes = 0;
    size_t slot_count = 0;
  };

  // The ramp length the current gateware produces. 1021, not 1024: the test
  // pattern is not a full 10-bit counter.
  static constexpr uint16_t kRampLength = 1021;

  explicit SyntheticSource(Options options);
  ~SyntheticSource() override = default;

  const char* Name() const override { return "synthetic"; }

  DiskBufferRing::Geometry PlanGeometry(size_t queue_size_bytes) const override;

  TransferResult Prepare(const DiskBufferRing& ring) override;
  TransferResult Run(DiskBufferRing& ring, SourceControl& control) override;
  void Finish() override;

  // Slots actually handed to the consumer, not counting discarded ones
  uint64_t SlotsDelivered() const { return slots_delivered_.load(); }

  // The first sample value of the first slot handed over. A consumer that knows
  // this can predict every sample that should follow, which is how the sink
  // swap is checked for lost or duplicated samples.
  uint16_t FirstDeliveredSampleValue() const {
    return first_delivered_sample_value_;
  }

  // The largest amount, in seconds, by which the source ever fell behind the
  // rate it was asked to generate at. Zero when the rate is unpaced, and near
  // zero when the host kept up.
  //
  // A paced source can only run late. PaceForBytes holds back when it is ahead
  // of schedule and can do nothing at all when it is behind, so a host that
  // cannot generate, validate and write at the configured rate does not produce
  // a slower version of the same capture: it produces one in which the pacing
  // never applies at all and the rate is the machine's rather than the one
  // asked for. Anything measuring against the configured rate has to check this
  // first, or it is measuring how fast the machine is.
  double PacingSlipSeconds() const {
    return static_cast<double>(
               max_pacing_slip_nanoseconds_.load(std::memory_order_relaxed)) /
           1e9;
  }

 private:
  // Fill `bytes` of a slot with the next stretch of the stream.
  void GenerateInto(uint8_t* destination, size_t bytes);

  // Hold back until the configured rate allows the next slot.
  void PaceForBytes(uint64_t bytes_generated);

  Options options_;

  // Generator state, carried across slots so the stream is continuous
  uint16_t ramp_value_ = 0;
  uint8_t sequence_counter_ = 0;
  uint32_t samples_until_counter_increment_ = 0;
  uint64_t sine_phase_samples_ = 0;

  uint64_t slots_generated_ = 0;
  std::atomic<uint64_t> slots_delivered_{0};
  uint16_t first_delivered_sample_value_ = 0;
  bool have_first_delivered_sample_ = false;

  // Nanoseconds since the run started, as the pacing clock's origin
  uint64_t pacing_origin_nanoseconds_ = 0;
  uint64_t paced_bytes_ = 0;

  // Written only by the source's own thread; read by whoever is measuring,
  // which is why it is atomic and nothing else here is.
  std::atomic<uint64_t> max_pacing_slip_nanoseconds_{0};
};

}  // namespace ddd::capture
