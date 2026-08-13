/************************************************************************

    synthetic_source.cpp

    A device that is not there
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "synthetic_source.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#include "sample_format.h"

namespace ddd::capture {
namespace {

uint64_t NowNanoseconds() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

// Amplitude and period of the sine pattern. A period that is not a power of two
// keeps it from lining up with buffer boundaries, so a display driven by it
// shows the waveform moving rather than standing still.
constexpr double kSineAmplitude = 400.0;
constexpr double kSinePeriodSamples = 1000.0;

// M_PI is a POSIX extension rather than standard C++, and is absent from MSVC's
// <cmath> without a feature macro. Spelling it out is one line and portable.
constexpr double kTwoPi = 6.283185307179586;

}  // namespace

SyntheticSource::SyntheticSource(Options options) : options_(options) {}

DiskBufferRing::Geometry SyntheticSource::PlanGeometry(
    size_t queue_size_bytes) const {
  if (options_.slot_size_bytes != 0 && options_.slot_count != 0) {
    DiskBufferRing::Geometry geometry;
    geometry.slot_size_bytes = options_.slot_size_bytes;
    geometry.slot_count = options_.slot_count;
    return geometry;
  }

  // No endpoint to round to, so the slot size is the target as it stands.
  return DiskBufferRing::PlanGeometry(queue_size_bytes, 0);
}

TransferResult SyntheticSource::Prepare(const DiskBufferRing& /*ring*/) {
  ramp_value_ = 0;
  sequence_counter_ = 0;
  samples_until_counter_increment_ = kSamplesPerSequenceCounter;
  sine_phase_samples_ = 0;
  slots_generated_ = 0;
  slots_delivered_ = 0;
  have_first_delivered_sample_ = false;
  first_delivered_sample_value_ = 0;
  paced_bytes_ = 0;
  pacing_origin_nanoseconds_ = 0;
  return TransferResult::kSuccess;
}

void SyntheticSource::GenerateInto(uint8_t* destination, size_t bytes) {
  const size_t sample_count = bytes / kBytesPerSample;

  for (size_t index = 0; index < sample_count; ++index) {
    uint16_t value = 0;
    if (options_.pattern == Pattern::kRamp) {
      value = ramp_value_;
      ++ramp_value_;
      if (ramp_value_ >= kRampLength) {
        ramp_value_ = 0;
      }
    } else {
      const double phase = (kTwoPi * static_cast<double>(sine_phase_samples_)) /
                           kSinePeriodSamples;
      value = static_cast<uint16_t>(
          std::clamp(kSampleZeroOffset +
                         static_cast<int32_t>(kSineAmplitude * std::sin(phase)),
                     static_cast<int32_t>(kMinimumSampleValue),
                     static_cast<int32_t>(kMaximumSampleValue)));
      ++sine_phase_samples_;
    }

    const uint16_t word = MakeWireWord(value, sequence_counter_);
    destination[index * kBytesPerSample] = static_cast<uint8_t>(word & 0xFF);
    destination[(index * kBytesPerSample) + 1] =
        static_cast<uint8_t>((word >> 8) & 0xFF);

    --samples_until_counter_increment_;
    if (samples_until_counter_increment_ == 0) {
      ++sequence_counter_;
      if (sequence_counter_ >= kSequenceCounterValues) {
        sequence_counter_ = 0;
      }
      samples_until_counter_increment_ = kSamplesPerSequenceCounter;
    }
  }
}

void SyntheticSource::PaceForBytes(uint64_t bytes_generated) {
  if (options_.rate_bytes_per_second == 0) {
    return;
  }

  paced_bytes_ += bytes_generated;

  const uint64_t now = NowNanoseconds();
  if (pacing_origin_nanoseconds_ == 0) {
    pacing_origin_nanoseconds_ = now;
    return;
  }

  // When this stretch of data should have finished arriving, had it come off a
  // wire running at the configured rate. Computed from the origin rather than
  // from the previous slot, so a slot that ran late does not push every
  // subsequent deadline out with it — the source catches up instead, which is
  // what a real device does.
  const uint64_t due_nanoseconds =
      pacing_origin_nanoseconds_ +
      ((paced_bytes_ * 1'000'000'000ULL) / options_.rate_bytes_per_second);

  if (due_nanoseconds > now) {
    std::this_thread::sleep_for(
        std::chrono::nanoseconds(due_nanoseconds - now));
  }
}

TransferResult SyntheticSource::Run(DiskBufferRing& ring,
                                    SourceControl& control) {
  const size_t slot_bytes = ring.slot_size_bytes();
  size_t slot_index = 0;

  while (true) {
    if (control.AbortRequested() || ring.AbortRequested()) {
      return TransferResult::kForcedAbort;
    }

    if (control.StopRequested()) {
      return TransferResult::kSuccess;
    }

    if (options_.slot_limit != 0 &&
        slots_delivered_.load() >= options_.slot_limit) {
      return TransferResult::kSuccess;
    }

    const bool discarding = slots_generated_ < options_.discard_slots;
    const uint64_t delivered_index = slots_delivered_.load();
    const bool faulting = !discarding && options_.fault != Fault::kNone &&
                          delivered_index == options_.fault_at_slot;

    if (faulting && options_.fault == Fault::kStall) {
      // Deliberately produce nothing at all. Waiting on the ring's abort rather
      // than sleeping in a loop means the watchdog's abort releases this
      // immediately, so the test does not pay for the watchdog's timeout twice.
      control.Log(
          "Synthetic source: injecting a stall; no further data will be "
          "produced");
      while (!ring.AbortRequested() && !control.AbortRequested() &&
             !control.StopRequested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      return control.StopRequested() ? TransferResult::kSuccess
                                     : TransferResult::kForcedAbort;
    }

    if (faulting && options_.fault == Fault::kTransferFailure) {
      control.Log("Synthetic source: injecting a transfer failure");
      return TransferResult::kUsbTransferFailure;
    }

    if (!ring.WaitForSlotFree(slot_index)) {
      return TransferResult::kForcedAbort;
    }

    size_t bytes_to_generate = slot_bytes;
    if (faulting && options_.fault == Fault::kShortDelivery) {
      // Half a slot, rounded to a whole sample. A real short packet would leave
      // the rest of the slot holding the previous pass's data, and so does
      // this.
      control.Log("Synthetic source: injecting a short delivery");
      bytes_to_generate = (slot_bytes / 2) & ~static_cast<size_t>(1);
    }

    GenerateInto(ring.SlotData(slot_index), bytes_to_generate);

    if (faulting && options_.fault == Fault::kRampBreak) {
      // One extra step in the ramp generator, and nothing else touched. The
      // sequence counters carry on in perfect order, so this is invisible to
      // the validator and visible only to the test-pattern check — which is
      // the whole reason the test-pattern check exists.
      control.Log("Synthetic source: injecting a test-pattern break");
      ramp_value_ = static_cast<uint16_t>((ramp_value_ + 1) % kRampLength);
    }

    if (faulting && options_.fault == Fault::kSequenceBreak) {
      // Advance the counter by one extra step for everything that follows, so
      // the very next slot's first sample carries a value the validator is not
      // expecting. Corrupting this slot's bytes would be caught too, but this
      // way the break falls at a slot boundary, which is the harder case and
      // the one a genuine dropped transfer produces.
      control.Log("Synthetic source: injecting a sequence break");
      ++sequence_counter_;
      if (sequence_counter_ >= kSequenceCounterValues) {
        sequence_counter_ = 0;
      }
    }

    PaceForBytes(bytes_to_generate);
    ++slots_generated_;

    if (discarding) {
      // Generated and thrown away, exactly as the USB backends discard their
      // first transfers. The ring never sees it.
      continue;
    }

    if (!have_first_delivered_sample_) {
      const uint8_t* data = ring.SlotData(slot_index);
      const uint16_t word = static_cast<uint16_t>(
          static_cast<uint16_t>(data[0]) |
          static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8));
      first_delivered_sample_value_ = SampleValueFromWord(word);
      have_first_delivered_sample_ = true;
    }

    if (faulting && options_.fault == Fault::kShortDelivery) {
      return TransferResult::kUsbTransferFailure;
    }

    switch (ring.MarkSlotFull(slot_index)) {
      case DiskBufferRing::FillResult::kHandedOver:
        break;
      case DiskBufferRing::FillResult::kOverflow:
        // The consumer never emptied this slot. Samples have been overwritten.
        return TransferResult::kBufferOverflow;
      case DiskBufferRing::FillResult::kAborted:
        return TransferResult::kForcedAbort;
    }

    slots_delivered_.fetch_add(1);
    control.AddCompletedTransfers(1);

    slot_index = (slot_index + 1) % ring.slot_count();
  }
}

void SyntheticSource::Finish() {}

}  // namespace ddd::capture
