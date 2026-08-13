/************************************************************************

    test_disk_buffer_ring.cpp

    T1 tests for the producer-to-consumer handoff
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "disk_buffer_ring.h"
#include "sample_format.h"

namespace ddd::capture {
namespace {

// Small enough to be quick, large enough to exercise wrapping.
DiskBufferRing::Geometry SmallGeometry(size_t slot_count = 4) {
  DiskBufferRing::Geometry geometry;
  geometry.slot_size_bytes = 1024;
  geometry.slot_count = slot_count;
  return geometry;
}

TEST(DiskBufferRingTest, GeometryRoundsSlotsToWholeEndpointPackets) {
  // A transfer that ended mid-packet would make a short packet ambiguous —
  // possibly an error, possibly a boundary — and short packets have to be
  // unambiguously fatal.
  constexpr size_t kPacketBytes = 1024;
  const DiskBufferRing::Geometry geometry = DiskBufferRing::PlanGeometry(
      DiskBufferRing::kDefaultQueueSizeBytes, kPacketBytes);

  EXPECT_EQ(geometry.slot_size_bytes % kPacketBytes, 0U);
  EXPECT_LE(geometry.slot_size_bytes, DiskBufferRing::kTargetSlotSizeBytes);
}

TEST(DiskBufferRingTest, TheDefaultQueueIsThreeSecondsOfSlack) {
  // The number that answers "how long may a disk write stall?". If the default
  // ever changes, this is where the consequence is stated.
  const DiskBufferRing::Geometry geometry =
      DiskBufferRing::PlanGeometry(DiskBufferRing::kDefaultQueueSizeBytes, 0);

  const double seconds = static_cast<double>(geometry.TotalBytes()) /
                         static_cast<double>(kWireBytesPerSecond);
  EXPECT_GT(seconds, 3.0);
  EXPECT_LT(seconds, 3.5);
}

TEST(DiskBufferRingTest, AQueueSizeOutsideTheSupportedRangeIsClamped) {
  const DiskBufferRing::Geometry tiny = DiskBufferRing::PlanGeometry(1024, 0);
  EXPECT_GE(tiny.TotalBytes(), DiskBufferRing::kMinimumQueueSizeBytes);

  const DiskBufferRing::Geometry huge =
      DiskBufferRing::PlanGeometry(size_t{8} << 30, 0);
  EXPECT_LE(huge.TotalBytes(), DiskBufferRing::kMaximumQueueSizeBytes);
}

TEST(DiskBufferRingTest, AnUnreadSlotBeingRefilledIsReportedAsOverflow) {
  // The one condition that would make a capture silently wrong. It has to be
  // detected where it happens, because nothing downstream can tell that a
  // buffer's contents were replaced before they were written out.
  DiskBufferRing ring(SmallGeometry());

  EXPECT_EQ(ring.MarkSlotFull(0), DiskBufferRing::FillResult::kHandedOver);
  EXPECT_EQ(ring.MarkSlotFull(0), DiskBufferRing::FillResult::kOverflow);
}

TEST(DiskBufferRingTest, AFreedSlotCanBeFilledAgain) {
  DiskBufferRing ring(SmallGeometry());

  ASSERT_EQ(ring.MarkSlotFull(0), DiskBufferRing::FillResult::kHandedOver);
  ASSERT_TRUE(ring.WaitForSlotFull(0));
  ring.MarkSlotFree(0);
  EXPECT_EQ(ring.MarkSlotFull(0), DiskBufferRing::FillResult::kHandedOver);
}

TEST(DiskBufferRingTest, TheFillLevelTracksWhatHasNotBeenConsumed) {
  DiskBufferRing ring(SmallGeometry());
  EXPECT_EQ(ring.SlotsInUse(), 0U);

  ASSERT_EQ(ring.MarkSlotFull(0), DiskBufferRing::FillResult::kHandedOver);
  ASSERT_EQ(ring.MarkSlotFull(1), DiskBufferRing::FillResult::kHandedOver);
  EXPECT_EQ(ring.SlotsInUse(), 2U);
  EXPECT_EQ(ring.PeakSlotsInUse(), 2U);

  ring.MarkSlotFree(0);
  EXPECT_EQ(ring.SlotsInUse(), 1U);

  // The peak is what an operator wants after the fact: a capture that was fine
  // except for one stall thirty minutes in still says so here.
  EXPECT_EQ(ring.PeakSlotsInUse(), 2U);
}

TEST(DiskBufferRingTest, EverySampleSurvivesAContendedHandover) {
  // A producer and a consumer running flat out against each other, with the
  // ring small enough that both block on it constantly. Each slot carries a
  // serial number, and the consumer checks it saw every one exactly once and in
  // order.
  constexpr uint32_t kSlotsToSend = 4000;
  DiskBufferRing ring(SmallGeometry(3));

  std::atomic<bool> overflow{false};
  std::atomic<uint32_t> mismatches{0};
  std::atomic<uint32_t> received{0};

  std::thread consumer([&] {
    size_t index = 0;
    for (uint32_t expected = 0; expected < kSlotsToSend; ++expected) {
      if (!ring.WaitForSlotFull(index)) {
        break;
      }

      uint32_t serial = 0;
      std::memcpy(&serial, ring.SlotData(index), sizeof(serial));
      if (serial != expected) {
        ++mismatches;
      }
      ++received;

      ring.MarkSlotFree(index);
      index = (index + 1) % ring.slot_count();
    }
  });

  size_t index = 0;
  for (uint32_t serial = 0; serial < kSlotsToSend; ++serial) {
    ring.WaitForSlotFree(index);
    std::memcpy(ring.SlotData(index), &serial, sizeof(serial));
    if (ring.MarkSlotFull(index) != DiskBufferRing::FillResult::kHandedOver) {
      overflow = true;
      break;
    }
    index = (index + 1) % ring.slot_count();
  }

  consumer.join();

  EXPECT_FALSE(overflow.load());
  EXPECT_EQ(mismatches.load(), 0U);
  EXPECT_EQ(received.load(), kSlotsToSend);
}

TEST(DiskBufferRingTest, AbortReleasesAConsumerWaitingForDataThatWillNotCome) {
  DiskBufferRing ring(SmallGeometry());

  std::atomic<bool> released{false};
  std::thread waiter([&] {
    ring.WaitForSlotFull(0);
    released = true;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  ASSERT_FALSE(released.load()) << "the waiter should still be blocked";

  ring.Abort();
  waiter.join();
  EXPECT_TRUE(released.load());
}

TEST(DiskBufferRingTest, AbortReleasesAProducerWaitingForASlotToBeEmptied) {
  // The other half of the double toggle. A single transition would release one
  // of the two waiters and leave the other blocked forever, which is a hang at
  // shutdown rather than a visible bug.
  DiskBufferRing ring(SmallGeometry());
  ASSERT_EQ(ring.MarkSlotFull(0), DiskBufferRing::FillResult::kHandedOver);

  std::atomic<bool> released{false};
  std::thread waiter([&] {
    ring.WaitForSlotFree(0);
    released = true;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  ASSERT_FALSE(released.load()) << "the waiter should still be blocked";

  ring.Abort();
  waiter.join();
  EXPECT_TRUE(released.load());
}

TEST(DiskBufferRingTest, AWaitInterruptedByAbortSaysSoRatherThanLookingNormal) {
  DiskBufferRing ring(SmallGeometry());
  ring.Abort();

  EXPECT_FALSE(ring.WaitForSlotFull(0));
  EXPECT_FALSE(ring.WaitForSlotFree(0));
  EXPECT_TRUE(ring.AbortRequested());
}

TEST(DiskBufferRingTest, DumpedSlotsWakeAConsumerWithoutPretendingToHoldData) {
  // The graceful stop: the producer has finished, so the slots it never filled
  // are marked as dumped and the consumer is released rather than left waiting.
  DiskBufferRing ring(SmallGeometry());
  ASSERT_EQ(ring.MarkSlotFull(0), DiskBufferRing::FillResult::kHandedOver);

  ring.MarkEmptySlotsDumped();

  EXPECT_TRUE(ring.WaitForSlotFull(0)) << "real data must still read as real";
  EXPECT_FALSE(ring.WaitForSlotFull(1)) << "a dumped slot must not";
  EXPECT_FALSE(ring.AbortRequested()) << "a graceful stop is not an abort";
}

TEST(DiskBufferRingTest, ConsumingADumpedSlotDoesNotDisturbTheFillLevel) {
  DiskBufferRing ring(SmallGeometry());
  ASSERT_EQ(ring.MarkSlotFull(0), DiskBufferRing::FillResult::kHandedOver);
  ring.MarkEmptySlotsDumped();

  ring.MarkSlotFree(1);
  ring.MarkSlotFree(2);

  EXPECT_EQ(ring.SlotsInUse(), 1U) << "only slot 0 ever held data";
}

}  // namespace
}  // namespace ddd::capture
