/************************************************************************

    fpga_telemetry.cpp

    What the gateware reports about its own capture buffer
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "fpga_telemetry.h"

#include <algorithm>

#include "wire_protocol.h"

namespace ddd::capture {
namespace {

// Two bytes of the block, least significant first — the order every multi-byte
// field in this map arrives in.
uint16_t ReadWord(std::span<const uint8_t> block, size_t offset) {
  return static_cast<uint16_t>(block[offset] |
                               (static_cast<uint16_t>(block[offset + 1]) << 8));
}

}  // namespace

int FpgaTelemetry::BackPressurePercent() const {
  if (!present) {
    return 0;
  }

  // An interval that lost a sample is at the top of the scale whatever the
  // arithmetic says, so that 100 always means the same thing. It usually would
  // anyway — the buffer has to reach its depth to drop anything — but a drop
  // right at the end of an interval can be followed by a drain before the
  // reading is taken, and a stall that cost samples must not then report as
  // comfortable.
  if (overflow_events > 0) {
    return 100;
  }

  // Below a packet there is no back pressure to report: the FX3 took every
  // packet as soon as one was offered, which is the whole of what it is asked
  // to do.
  if (peak <= packet_words) {
    return 0;
  }

  const int headroom =
      static_cast<int>(depth_words) - static_cast<int>(packet_words);
  const int excursion = static_cast<int>(peak) - static_cast<int>(packet_words);

  return std::clamp(excursion * 100 / headroom, 0, 100);
}

int FpgaTelemetry::PeakPercentOfDepth() const {
  if (!present || depth_words == 0) {
    return 0;
  }

  return std::clamp(
      static_cast<int>(peak) * 100 / static_cast<int>(depth_words), 0, 100);
}

int FpgaTelemetry::UsedPercentOfDepth() const {
  if (!present || depth_words == 0) {
    return 0;
  }

  return std::clamp(
      static_cast<int>(used_now) * 100 / static_cast<int>(depth_words), 0, 100);
}

FpgaTelemetry ParseFpgaTelemetry(std::span<const uint8_t> block) {
  FpgaTelemetry telemetry;

  if (block.size() < kTelemetryBlockLength) {
    return telemetry;
  }

  // The signature, which is the whole of how a host tells this instrument from
  // an address that reads zero because nothing implements it
  if (block[0] != kTelemetryIdValue) {
    return telemetry;
  }

  const uint8_t status = block[kTelemetryOffsetStatus];
  const uint8_t format = static_cast<uint8_t>(status & kTelemetryFormatMask);

  // Exactly the layout this build understands, and not a range.
  //
  // The map version elsewhere is a range because additions there are additive
  // by rule. This nibble is not that: it changes when a field of this block
  // starts meaning something else, so a value this build does not know is a
  // block whose fields cannot be read — and reporting a buffer wrongly is
  // worse than reporting that it cannot be read.
  if (format != kTelemetryFormat) {
    return telemetry;
  }

  const uint16_t depth = ReadWord(block, kTelemetryOffsetDepth);
  const uint16_t packet = ReadWord(block, kTelemetryOffsetPacketWords);

  // Geometry that cannot be true is a misread link rather than a device with an
  // unusual buffer. Everything downstream divides by the headroom between
  // these two, so this is also what makes that division safe.
  if (packet == 0 || depth <= packet) {
    return telemetry;
  }

  telemetry.present = true;
  telemetry.format = format;
  telemetry.overflow_since_open = (status & kTelemetryFlagOverflowSeen) != 0;
  telemetry.saturated = (status & kTelemetryFlagSaturated) != 0;
  telemetry.latch_count = block[kTelemetryOffsetLatchCount];

  telemetry.used_now = ReadWord(block, kTelemetryOffsetUsedNow);
  telemetry.peak = ReadWord(block, kTelemetryOffsetPeak);
  telemetry.peak_since_open = ReadWord(block, kTelemetryOffsetPeakLifetime);
  telemetry.overflow_events = ReadWord(block, kTelemetryOffsetOverflows);
  telemetry.dropped_words = ReadWord(block, kTelemetryOffsetDropped);
  telemetry.packets_read = ReadWord(block, kTelemetryOffsetPackets);
  telemetry.near_full_units = ReadWord(block, kTelemetryOffsetNearFull);

  telemetry.depth_words = depth;
  telemetry.packet_words = packet;
  telemetry.near_full_words = ReadWord(block, kTelemetryOffsetNearFullWords);

  return telemetry;
}

}  // namespace ddd::capture
