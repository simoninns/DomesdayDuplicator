/************************************************************************

    test_usb_device.cpp

    T1 tests for device selection, speed rules and the transfer layout
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "disk_buffer_ring.h"
#include "usb_device.h"
#include "usb_device_info.h"

namespace ddd::capture {
namespace {

DeviceInfo DeviceAt(const std::string& path,
                    DeviceSpeed speed = DeviceSpeed::kSuper) {
  DeviceInfo info;
  info.path = path;
  info.speed = speed;
  return info;
}

// --- Speed rules ---------------------------------------------------------

// The check the old backends did not make, and the reason the plan calls for
// it: High-speed USB 2 tops out below the 80 MB/s the device produces
// continuously, so a device on a USB 2 port cannot work. The old application
// opened it anyway and failed some seconds later with a sequence mismatch,
// which sends a user looking for a bad cable rather than at the port.
TEST(DeviceSpeedTest, OnlySuperSpeedAndAboveCanCarryACapture) {
  EXPECT_FALSE(SpeedCanCarryCapture(DeviceSpeed::kLow));
  EXPECT_FALSE(SpeedCanCarryCapture(DeviceSpeed::kFull));
  EXPECT_FALSE(SpeedCanCarryCapture(DeviceSpeed::kHigh));
  EXPECT_TRUE(SpeedCanCarryCapture(DeviceSpeed::kSuper));
  EXPECT_TRUE(SpeedCanCarryCapture(DeviceSpeed::kSuperPlus));
}

// A backend that cannot work out the speed must not be able to veto a device
// that might be fine. The sequence markers will catch it if it is not, which is
// exactly where the old application stood and no worse.
TEST(DeviceSpeedTest, AnUnknownSpeedIsAllowedThrough) {
  EXPECT_TRUE(SpeedCanCarryCapture(DeviceSpeed::kUnknown));
}

TEST(DeviceSpeedTest, EverySpeedHasItsOwnName) {
  const DeviceSpeed speeds[] = {DeviceSpeed::kUnknown, DeviceSpeed::kLow,
                                DeviceSpeed::kFull,    DeviceSpeed::kHigh,
                                DeviceSpeed::kSuper,   DeviceSpeed::kSuperPlus};

  std::set<std::string> names;
  for (DeviceSpeed speed : speeds) {
    names.insert(DeviceSpeedName(speed));
  }
  EXPECT_EQ(names.size(), std::size(speeds));
}

// --- Device selection ----------------------------------------------------

TEST(SelectDeviceTest, NothingAttachedSelectsNothing) {
  const std::vector<DeviceInfo> devices;
  EXPECT_EQ(SelectDevice(devices, "anything"), nullptr);
}

TEST(SelectDeviceTest, TheOnlyDeviceIsChosenWithoutAPreference) {
  const std::vector<DeviceInfo> devices{DeviceAt("bus-1")};
  ASSERT_NE(SelectDevice(devices, ""), nullptr);
  EXPECT_EQ(SelectDevice(devices, "")->path, "bus-1");
}

TEST(SelectDeviceTest, ThePreferredDeviceWinsWhenItIsThere) {
  const std::vector<DeviceInfo> devices{DeviceAt("bus-1"), DeviceAt("bus-2"),
                                        DeviceAt("bus-3")};
  ASSERT_NE(SelectDevice(devices, "bus-2"), nullptr);
  EXPECT_EQ(SelectDevice(devices, "bus-2")->path, "bus-2");
}

// A remembered preference is a preference, not a requirement. Someone who moved
// their device to another port should get a working capture, not a refusal
// naming a port they no longer use.
TEST(SelectDeviceTest, AnAbsentPreferenceFallsBackRatherThanFailing) {
  const std::vector<DeviceInfo> devices{DeviceAt("bus-1"), DeviceAt("bus-2")};
  ASSERT_NE(SelectDevice(devices, "bus-9"), nullptr);
  EXPECT_EQ(SelectDevice(devices, "bus-9")->path, "bus-1");
}

// --- Personalities -------------------------------------------------------
//
// A device that is not running the Duplicator's firmware is still a device,
// and everything that captures has to refuse it while everything that
// programs has to reach it. That split lives in the selection.

DeviceInfo RecoveryDeviceAt(const std::string& path) {
  DeviceInfo info = DeviceAt(path);
  info.personality = DevicePersonality::kRecovery;
  info.product_string.clear();
  info.protocol_version = 0;
  return info;
}

TEST(DevicePersonalityTest, EveryPersonalityHasItsOwnName) {
  const DevicePersonality personalities[] = {
      DevicePersonality::kApplication, DevicePersonality::kRecovery,
      DevicePersonality::kFlashProgrammer};

  std::set<std::string> names;
  for (DevicePersonality personality : personalities) {
    names.insert(DevicePersonalityName(personality));
  }
  EXPECT_EQ(names.size(), std::size(personalities));
}

// The default, so that a caller which has not thought about recovery devices
// cannot be handed one to capture with.
TEST(SelectDeviceTest, ADeviceWithNoFirmwareIsNotSelectedForCapture) {
  const std::vector<DeviceInfo> devices{RecoveryDeviceAt("bus-1")};
  EXPECT_EQ(SelectDevice(devices, ""), nullptr);
}

TEST(SelectDeviceTest, ADeviceWithNoFirmwareIsSkippedInFavourOfAWorkingOne) {
  const std::vector<DeviceInfo> devices{RecoveryDeviceAt("bus-1"),
                                        DeviceAt("bus-2")};
  ASSERT_NE(SelectDevice(devices, ""), nullptr);
  EXPECT_EQ(SelectDevice(devices, "")->path, "bus-2");
}

// Even when it is the remembered preference: a preference names a port, and a
// port whose device cannot capture is not a capture device today.
TEST(SelectDeviceTest, APreferredDeviceWithNoFirmwareIsNotSelectedForCapture) {
  const std::vector<DeviceInfo> devices{RecoveryDeviceAt("bus-1"),
                                        DeviceAt("bus-2")};
  ASSERT_NE(SelectDevice(devices, "bus-1"), nullptr);
  EXPECT_EQ(SelectDevice(devices, "bus-1")->path, "bus-2");
}

// And the other half of the rule: the firmware dialog and ddd-update ask for
// any personality, because a device that can do nothing else is the device
// they exist to fix.
TEST(SelectDeviceTest, AskingForAnyPersonalityFindsTheRecoveryDevice) {
  const std::vector<DeviceInfo> devices{RecoveryDeviceAt("bus-1")};

  const DeviceInfo* const found =
      SelectDevice(devices, "", DeviceSelection::kAny);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->path, "bus-1");
  EXPECT_FALSE(found->is_application());
}

// A device falling back to its boot ROM is at the same path it was at a
// moment ago, and that transition is the most important change the
// application can be told about — so it has to count as a change.
TEST(DevicePersonalityTest, AChangeOfPersonalityIsAChangeOfDevice) {
  const DeviceInfo working = DeviceAt("bus-1");
  DeviceInfo fallen = working;
  fallen.personality = DevicePersonality::kRecovery;

  EXPECT_NE(working, fallen);
}

// --- Transfer layout -----------------------------------------------------
//
// The most intricate arithmetic in either backend and the least observable on
// hardware: getting the stride wrong by one produces a capture that is subtly
// interleaved rather than one that fails, and the only way to notice that with
// a device attached is for a disc to sound wrong. As a pure function it can
// simply be checked.

// The real geometry: a SuperSpeed bulk endpoint is 1024 bytes, and the default
// queue gives 2 MB slots.
constexpr size_t kPacketBytes = 1024;

DiskBufferRing::Geometry RealGeometry() {
  return DiskBufferRing::PlanGeometry(DiskBufferRing::kDefaultQueueSizeBytes,
                                      kPacketBytes);
}

TEST(TransferLayoutTest, TheDefaultGeometryProducesTransfersNear128KB) {
  const DiskBufferRing::Geometry geometry = RealGeometry();
  const TransferLayout layout = PlanTransferLayout(
      geometry.slot_size_bytes, geometry.slot_count, kPacketBytes, {});

  ASSERT_TRUE(layout.valid) << layout.problem;
  EXPECT_EQ(layout.transfer_bytes, size_t{128} << 10);
  EXPECT_EQ(layout.transfers_per_slot, 16U);
}

// Every transfer being a whole number of packets is what makes a short read
// unambiguously a fault rather than a boundary effect, which is the whole basis
// of the SHORT_NOT_OK flag both backends set.
TEST(TransferLayoutTest, ATransferIsAlwaysAWholeNumberOfPackets) {
  for (size_t packet : {size_t{512}, size_t{1024}}) {
    const DiskBufferRing::Geometry geometry = DiskBufferRing::PlanGeometry(
        DiskBufferRing::kDefaultQueueSizeBytes, packet);
    const TransferLayout layout = PlanTransferLayout(
        geometry.slot_size_bytes, geometry.slot_count, packet, {});

    ASSERT_TRUE(layout.valid) << layout.problem;
    EXPECT_EQ(layout.transfer_bytes % packet, 0U);
  }
}

// A transfer that did not divide a slot exactly would leave the last one
// straddling a slot boundary, and every buffer after the first would be one
// fragment out of alignment.
TEST(TransferLayoutTest, TransfersDivideASlotExactly) {
  const DiskBufferRing::Geometry geometry = RealGeometry();
  const TransferLayout layout = PlanTransferLayout(
      geometry.slot_size_bytes, geometry.slot_count, kPacketBytes, {});

  ASSERT_TRUE(layout.valid) << layout.problem;
  EXPECT_EQ(layout.transfers_per_slot * layout.transfer_bytes,
            geometry.slot_size_bytes);
}

// A packet size that does not divide 128 KB evenly. The planner has to move to
// the next size that divides the slot rather than rounding down and leaving a
// remainder.
TEST(TransferLayoutTest, AnAwkwardPacketSizeStillDividesTheSlot) {
  constexpr size_t kAwkwardPacket = 768;
  const size_t slot_bytes = kAwkwardPacket * 2048;

  const TransferLayout layout =
      PlanTransferLayout(slot_bytes, 64, kAwkwardPacket, {});

  ASSERT_TRUE(layout.valid) << layout.problem;
  EXPECT_EQ(layout.transfer_bytes % kAwkwardPacket, 0U);
  EXPECT_EQ(layout.transfers_per_slot * layout.transfer_bytes, slot_bytes);
}

TEST(TransferLayoutTest, OneTransferPerBufferIsExactlyThat) {
  UsbSourceOptions options;
  options.small_transfers = false;

  const DiskBufferRing::Geometry geometry = RealGeometry();
  const TransferLayout layout = PlanTransferLayout(
      geometry.slot_size_bytes, geometry.slot_count, kPacketBytes, options);

  ASSERT_TRUE(layout.valid) << layout.problem;
  EXPECT_EQ(layout.transfers_per_slot, 1U);
  EXPECT_EQ(layout.transfer_bytes, geometry.slot_size_bytes);
  EXPECT_EQ(layout.slot_span, geometry.slot_count - 1);
}

// The usbfs limit. A queue span larger than the Linux kernel will map fails at
// submission with ENOMEM, which is why the option exists at all.
TEST(TransferLayoutTest, TheTransferQueueIsCappedByTheOption) {
  UsbSourceOptions options;
  options.transfer_queue_bytes = size_t{12} << 20;

  const DiskBufferRing::Geometry geometry = RealGeometry();
  const TransferLayout layout = PlanTransferLayout(
      geometry.slot_size_bytes, geometry.slot_count, kPacketBytes, options);

  ASSERT_TRUE(layout.valid) << layout.problem;
  EXPECT_LE(layout.slot_span * geometry.slot_size_bytes,
            options.transfer_queue_bytes);
}

// At least one slot has to be left for the consumer to work in, or the producer
// and consumer would be contending for the same buffer at all times.
TEST(TransferLayoutTest, TheSpanNeverCoversTheWholeRing) {
  const DiskBufferRing::Geometry geometry = RealGeometry();

  for (bool small : {true, false}) {
    UsbSourceOptions options;
    options.small_transfers = small;
    options.transfer_queue_bytes = DiskBufferRing::kMaximumQueueSizeBytes;

    const TransferLayout layout = PlanTransferLayout(
        geometry.slot_size_bytes, geometry.slot_count, kPacketBytes, options);

    ASSERT_TRUE(layout.valid) << layout.problem;
    EXPECT_LT(layout.slot_span, geometry.slot_count);
    EXPECT_GE(layout.slot_span, 1U);
  }
}

// The property the whole first_slot_index field exists for, and the one that
// was written twice with subtly different arithmetic in the old engine.
//
// The consumer reads slots in order from zero. So the first slot handed to it
// must be slot zero, which means the discarded slots have to come before it,
// wrapping round the end of the ring. Starting anywhere else and the consumer's
// first buffer is one the producer had already decided to throw away.
TEST(TransferLayoutTest, TheFirstSlotHandedOverIsSlotZero) {
  const DiskBufferRing::Geometry geometry = RealGeometry();

  for (uint64_t discard :
       {uint64_t{0}, uint64_t{1}, uint64_t{4}, uint64_t{17}}) {
    UsbSourceOptions options;
    options.discard_slots = discard;

    const TransferLayout layout = PlanTransferLayout(
        geometry.slot_size_bytes, geometry.slot_count, kPacketBytes, options);
    ASSERT_TRUE(layout.valid) << layout.problem;

    // Walk the slots in the order the transfers reach them, skipping the ones
    // that will be discarded, and check where the first survivor lands.
    const size_t first_delivered =
        (layout.first_slot_index + static_cast<size_t>(layout.discard_slots)) %
        geometry.slot_count;
    EXPECT_EQ(first_delivered, 0U) << "with " << discard << " slots discarded";
  }
}

TEST(TransferLayoutTest, ADiscardLongerThanTheRingIsClampedToIt) {
  const DiskBufferRing::Geometry geometry = RealGeometry();

  UsbSourceOptions options;
  options.discard_slots = geometry.slot_count * 4;

  const TransferLayout layout = PlanTransferLayout(
      geometry.slot_size_bytes, geometry.slot_count, kPacketBytes, options);

  ASSERT_TRUE(layout.valid) << layout.problem;
  EXPECT_LE(layout.discard_slots, geometry.slot_count);
}

TEST(TransferLayoutTest, ADeviceWithNoPacketSizeIsRefusedWithAReason) {
  const TransferLayout layout = PlanTransferLayout(size_t{2} << 20, 64, 0, {});

  EXPECT_FALSE(layout.valid);
  EXPECT_FALSE(layout.problem.empty());
}

TEST(TransferLayoutTest, ASlotThatIsNotWholePacketsIsRefusedWithAReason) {
  const TransferLayout layout =
      PlanTransferLayout((size_t{2} << 20) + 1, 64, kPacketBytes, {});

  EXPECT_FALSE(layout.valid);
  EXPECT_FALSE(layout.problem.empty());
}

TEST(TransferLayoutTest, ARingTooSmallToRollThroughIsRefusedWithAReason) {
  const TransferLayout layout =
      PlanTransferLayout(size_t{2} << 20, 3, kPacketBytes, {});

  EXPECT_FALSE(layout.valid);
  EXPECT_FALSE(layout.problem.empty());
}

// Simulating the whole rolling scheme, which is the only way to check the one
// property the consumer depends on and no single field expresses: buffers are
// handed over in increasing slot order, starting at zero.
//
// The consumer in capture_pipeline.cpp reads slot 0, then 1, then 2, and wraps.
// It has no way to be told otherwise. So a producer that handed slots over in
// any other order would not fail — it would silently deliver the buffers out of
// sequence, and a capture would be interleaved rather than broken. This walks
// the transfers through several laps of the ring and checks the order they come
// out in.
TEST(TransferLayoutTest, BuffersAreHandedOverInTheOrderTheConsumerReadsThem) {
  const DiskBufferRing::Geometry geometry = RealGeometry();

  for (bool small : {true, false}) {
    for (uint64_t discard : {uint64_t{0}, uint64_t{4}}) {
      UsbSourceOptions options;
      options.small_transfers = small;
      options.discard_slots = discard;

      const TransferLayout layout = PlanTransferLayout(
          geometry.slot_size_bytes, geometry.slot_count, kPacketBytes, options);
      ASSERT_TRUE(layout.valid) << layout.problem;

      // The transfers, laid out exactly as both backends lay them out.
      struct Entry {
        size_t slot;
        size_t index_in_slot;
        bool last_in_slot;
      };
      std::vector<Entry> transfers;
      transfers.reserve(layout.transfer_count);

      size_t slot = layout.first_slot_index;
      size_t index_in_slot = 0;
      for (size_t index = 0; index < layout.transfer_count; ++index) {
        Entry entry{slot, index_in_slot, false};
        ++index_in_slot;
        entry.last_in_slot = index_in_slot >= layout.transfers_per_slot;
        if (entry.last_in_slot) {
          index_in_slot = 0;
          slot = (slot + 1) % geometry.slot_count;
        }
        transfers.push_back(entry);
      }

      // Bulk transfers on one endpoint complete in submission order, so
      // reaping them round-robin in index order is what actually happens.
      std::vector<size_t> handed_over;
      uint64_t discarded = 0;
      const size_t laps = 3;
      const size_t completions = layout.transfer_count * laps +
                                 static_cast<size_t>(layout.discard_transfers);

      for (size_t step = 0; step < completions; ++step) {
        Entry& entry = transfers[step % transfers.size()];

        if (discarded < layout.discard_transfers) {
          ++discarded;
        } else if (entry.last_in_slot) {
          handed_over.push_back(entry.slot);
        }

        entry.slot = (entry.slot + layout.slot_span) % geometry.slot_count;
      }

      ASSERT_FALSE(handed_over.empty());
      for (size_t index = 0; index < handed_over.size(); ++index) {
        EXPECT_EQ(handed_over[index], index % geometry.slot_count)
            << "handover " << index << " with small_transfers=" << small
            << " and " << discard << " slots discarded";
      }
    }
  }
}

}  // namespace
}  // namespace ddd::capture
