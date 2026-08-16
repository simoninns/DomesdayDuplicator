/************************************************************************

    test_fpga_telemetry.cpp

    Reading the gateware's account of its capture buffer
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "fpga_telemetry.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

// The geometry the application gateware reports: a 16384-word FIFO offered to
// the FX3 in 8192-word packets, with the near-full mark three quarters up.
// These are the figures every percentage below is relative to.
constexpr uint16_t kDepth = 16384;
constexpr uint16_t kPacket = 8192;
constexpr uint16_t kNearFull = 12288;

void PutWord(std::vector<uint8_t>& block, size_t offset, uint16_t value) {
  block[offset] = static_cast<uint8_t>(value & 0xFF);
  block[offset + 1] = static_cast<uint8_t>(value >> 8);
}

// A block as the register read returns one, with a healthy capture in it.
std::vector<uint8_t> MakeBlock(uint16_t peak, uint16_t overflows = 0,
                               uint16_t dropped = 0,
                               uint8_t status = kTelemetryFormat) {
  std::vector<uint8_t> block(kTelemetryBlockLength, 0);

  block[0] = kTelemetryIdValue;
  block[kTelemetryOffsetStatus] = status;
  block[kTelemetryOffsetLatchCount] = 7;

  PutWord(block, kTelemetryOffsetUsedNow, 4000);
  PutWord(block, kTelemetryOffsetPeak, peak);
  PutWord(block, kTelemetryOffsetPeakLifetime, peak);
  PutWord(block, kTelemetryOffsetOverflows, overflows);
  PutWord(block, kTelemetryOffsetDropped, dropped);
  PutWord(block, kTelemetryOffsetPackets, 1221);
  PutWord(block, kTelemetryOffsetNearFull, 0);

  PutWord(block, kTelemetryOffsetDepth, kDepth);
  PutWord(block, kTelemetryOffsetPacketWords, kPacket);
  PutWord(block, kTelemetryOffsetNearFullWords, kNearFull);

  return block;
}

TEST(FpgaTelemetryTest, AWellFormedBlockIsRead) {
  const FpgaTelemetry telemetry = ParseFpgaTelemetry(MakeBlock(9000));

  EXPECT_TRUE(telemetry.present);
  EXPECT_EQ(telemetry.used_now, 4000);
  EXPECT_EQ(telemetry.peak, 9000);
  EXPECT_EQ(telemetry.packets_read, 1221);
  EXPECT_EQ(telemetry.latch_count, 7);
  EXPECT_EQ(telemetry.depth_words, kDepth);
  EXPECT_EQ(telemetry.packet_words, kPacket);
  EXPECT_EQ(telemetry.near_full_words, kNearFull);
  EXPECT_FALSE(telemetry.overflow_since_open);
  EXPECT_FALSE(telemetry.saturated);
}

TEST(FpgaTelemetryTest, GatewareWithoutTheInstrumentIsNotPresent) {
  // Unmapped addresses read zero, so this is what every gateware built before
  // the instrument existed returns — and what the recovery image returns, which
  // has no capture buffer at all. It must not read as a device whose buffer is
  // empty.
  const std::vector<uint8_t> zeros(kTelemetryBlockLength, 0);

  EXPECT_FALSE(ParseFpgaTelemetry(zeros).present);
}

TEST(FpgaTelemetryTest, AFloatingLinkIsNotPresent) {
  const std::vector<uint8_t> ones(kTelemetryBlockLength, 0xFF);

  EXPECT_FALSE(ParseFpgaTelemetry(ones).present);
}

TEST(FpgaTelemetryTest, AShortBlockIsNotPresent) {
  std::vector<uint8_t> block = MakeBlock(9000);
  block.resize(kTelemetryBlockLength - 1);

  EXPECT_FALSE(ParseFpgaTelemetry(block).present);
}

TEST(FpgaTelemetryTest, AnUnknownLayoutIsNotRead) {
  // The format nibble changes when a field starts meaning something else, so a
  // value this build does not know is a block whose fields cannot be read.
  // Reporting a buffer wrongly is worse than reporting that it cannot be read.
  const FpgaTelemetry telemetry =
      ParseFpgaTelemetry(MakeBlock(9000, 0, 0, kTelemetryFormat + 1));

  EXPECT_FALSE(telemetry.present);
}

TEST(FpgaTelemetryTest, ImpossibleGeometryIsNotRead) {
  // Everything downstream divides by the headroom between the depth and the
  // packet size, so a block claiming a packet larger than the buffer is a
  // misread link rather than an unusual device.
  std::vector<uint8_t> block = MakeBlock(9000);
  PutWord(block, kTelemetryOffsetPacketWords, kDepth + 1);

  EXPECT_FALSE(ParseFpgaTelemetry(block).present);
}

TEST(FpgaTelemetryTest, TheStatusFlagsAreRead) {
  const uint8_t status = static_cast<uint8_t>(
      kTelemetryFormat | kTelemetryFlagOverflowSeen | kTelemetryFlagSaturated);
  const FpgaTelemetry telemetry =
      ParseFpgaTelemetry(MakeBlock(16384, 0, 0, status));

  EXPECT_TRUE(telemetry.present);
  EXPECT_TRUE(telemetry.overflow_since_open);
  EXPECT_TRUE(telemetry.saturated);
}

// --- The back-pressure scale -------------------------------------------------
//
// Zero is "the FX3 took every packet as soon as it was offered" and 100 is
// "samples were lost". Both ends are literal, which is the property that makes
// the figure worth putting on a bar.

TEST(FpgaTelemetryTest, ANormalSawtoothIsNoBackPressure) {
  // A healthy capture peaks at about the packet threshold, because the gateware
  // offers a packet only once a whole one is queued. If that read as 50% the
  // indicator would sit at half scale on a device that is working perfectly.
  EXPECT_EQ(ParseFpgaTelemetry(MakeBlock(kPacket)).BackPressurePercent(), 0);
  EXPECT_EQ(ParseFpgaTelemetry(MakeBlock(kPacket - 100)).BackPressurePercent(),
            0);
}

TEST(FpgaTelemetryTest, HalfTheHeadroomIsHalfTheScale) {
  const FpgaTelemetry telemetry = ParseFpgaTelemetry(MakeBlock(kNearFull));

  EXPECT_EQ(telemetry.BackPressurePercent(), 50);
  EXPECT_EQ(telemetry.PeakPercentOfDepth(), 75);
}

TEST(FpgaTelemetryTest, AFullBufferIsTheTopOfTheScale) {
  EXPECT_EQ(ParseFpgaTelemetry(MakeBlock(kDepth)).BackPressurePercent(), 100);
}

TEST(FpgaTelemetryTest, AnIntervalThatLostSamplesIsAlwaysOneHundred) {
  // A stall that dropped samples and then drained before the reading was taken
  // would otherwise report a comfortable peak, which is the one reading that
  // must not be possible.
  const FpgaTelemetry telemetry = ParseFpgaTelemetry(MakeBlock(1000, 1, 4200));

  EXPECT_EQ(telemetry.BackPressurePercent(), 100);
  EXPECT_EQ(telemetry.dropped_words, 4200);
  EXPECT_EQ(telemetry.overflow_events, 1);
}

TEST(FpgaTelemetryTest, AbsentTelemetryIsNotZeroPressure) {
  // Zero is a measurement. A device that cannot report has not made one, and
  // the difference is what stops an older gateware looking like a flawless one.
  const FpgaTelemetry telemetry;

  EXPECT_FALSE(telemetry.present);
  EXPECT_EQ(telemetry.BackPressurePercent(), 0);
}

}  // namespace
}  // namespace ddd::capture
