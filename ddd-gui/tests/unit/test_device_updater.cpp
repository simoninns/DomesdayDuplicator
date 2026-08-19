/************************************************************************

    test_device_updater.cpp

    T1 unit test for the device update seam's wire decoding
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "device_updater.h"
#include "fake_usb_device.h"
#include "update_fixtures.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

// A status packet as the firmware writes it.
std::array<uint8_t, kUpdateStatusLength> MakeStatus(
    uint8_t phase, uint8_t error, uint16_t chunk, uint32_t received,
    uint32_t written, uint32_t verified) {
  std::array<uint8_t, kUpdateStatusLength> packet{};
  packet[0] = phase;
  packet[1] = error;
  packet[2] = static_cast<uint8_t>(chunk & 0xFF);
  packet[3] = static_cast<uint8_t>(chunk >> 8);

  const auto write32 = [&packet](size_t offset, uint32_t value) {
    packet[offset] = static_cast<uint8_t>(value & 0xFF);
    packet[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    packet[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    packet[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
  };
  write32(4, received);
  write32(8, written);
  write32(12, verified);
  return packet;
}

TEST(DeviceUpdaterStatus, DecodesEveryField) {
  const auto packet =
      MakeStatus(3, 0, 2048, 0x11223344, 0x00000100, 0x000000FF);

  const std::optional<DeviceUpdateStatus> parsed =
      ParseDeviceUpdateStatus(packet);
  ASSERT_TRUE(parsed.has_value());
  const DeviceUpdateStatus status = test::Checked(parsed);

  EXPECT_EQ(status.phase, UpdatePhase::kVerifying);
  EXPECT_EQ(status.error, DeviceUpdateError::kNone);
  EXPECT_EQ(status.maximum_chunk_bytes, 2048);
  EXPECT_EQ(status.bytes_received, 0x11223344u);
  EXPECT_EQ(status.bytes_written, 0x00000100u);
  EXPECT_EQ(status.bytes_verified, 0x000000FFu);
}

// The three counters are separate because they move at very different
// speeds, and a reader that confused two of them would show a progress bar
// that ran backwards.
TEST(DeviceUpdaterStatus, CountersAreNotInterchangeable) {
  const auto packet = MakeStatus(2, 0, 64, 1, 2, 3);

  const std::optional<DeviceUpdateStatus> parsed =
      ParseDeviceUpdateStatus(packet);
  ASSERT_TRUE(parsed.has_value());
  const DeviceUpdateStatus status = test::Checked(parsed);

  EXPECT_EQ(status.bytes_received, 1u);
  EXPECT_EQ(status.bytes_written, 2u);
  EXPECT_EQ(status.bytes_verified, 3u);
}

TEST(DeviceUpdaterStatus, RefusesTheWrongLength) {
  const auto packet = MakeStatus(0, 0, 2048, 0, 0, 0);

  EXPECT_FALSE(
      ParseDeviceUpdateStatus(std::span(packet).first(kUpdateStatusLength - 1))
          .has_value());

  std::vector<uint8_t> longer(packet.begin(), packet.end());
  longer.push_back(0);
  EXPECT_FALSE(ParseDeviceUpdateStatus(longer).has_value());
}

// A device answering with a phase or an error this build does not know is a
// device speaking a protocol this build must not be narrating. Refusing the
// packet is what turns that into "the device reported something I do not
// understand" rather than a confident sentence about the wrong thing.
TEST(DeviceUpdaterStatus, RefusesAPhaseItDoesNotKnow) {
  const auto packet = MakeStatus(6, 0, 2048, 0, 0, 0);
  EXPECT_FALSE(ParseDeviceUpdateStatus(packet).has_value());
}

TEST(DeviceUpdaterStatus, RefusesAnErrorItDoesNotKnow) {
  const auto packet = MakeStatus(5, 200, 2048, 0, 0, 0);
  EXPECT_FALSE(ParseDeviceUpdateStatus(packet).has_value());
}

TEST(DeviceUpdaterStatus, AcceptsAnIdleDeviceThatHasNeverBeenUpdated) {
  const auto packet = MakeStatus(0, 0, 2048, 0, 0, 0);

  const std::optional<DeviceUpdateStatus> parsed =
      ParseDeviceUpdateStatus(packet);
  ASSERT_TRUE(parsed.has_value());
  const DeviceUpdateStatus status = test::Checked(parsed);

  EXPECT_EQ(status.phase, UpdatePhase::kIdle);

  // Which is how the host discovers the chunk size rather than assuming one.
  EXPECT_EQ(status.maximum_chunk_bytes, 2048);
}

// Every error a device can report has to have something to say to a user. A
// code with no text would surface as an empty message box at exactly the
// moment somebody needs to know what happened.
TEST(DeviceUpdaterError, EveryCodeHasItsOwnSentence) {
  std::vector<std::string> seen;

  for (uint8_t code = 0;
       code <= static_cast<uint8_t>(DeviceUpdateError::kHardware); ++code) {
    const std::string text =
        DeviceUpdateErrorText(static_cast<DeviceUpdateError>(code));
    EXPECT_FALSE(text.empty()) << "code " << static_cast<int>(code);

    // No two errors say the same thing, because two failures that read
    // identically are one failure the user cannot tell apart.
    EXPECT_EQ(std::find(seen.begin(), seen.end(), text), seen.end())
        << "code " << static_cast<int>(code) << " repeats an earlier message";
    seen.push_back(text);
  }
}

TEST(DeviceUpdaterError, AnUnknownCodeSaysSoRatherThanGuessing) {
  const std::string text =
      DeviceUpdateErrorText(static_cast<DeviceUpdateError>(200));
  EXPECT_NE(text.find("does not recognise"), std::string::npos);
}

TEST(DeviceUpdaterTarget, BothTargetsAreNamed) {
  EXPECT_STREQ(UpdateTargetName(UpdateTarget::kFirmware), "firmware");
  EXPECT_STREQ(UpdateTargetName(UpdateTarget::kGateware), "gateware");
}

// The wire indices are protocol, not an implementation detail: the firmware
// switches on them and a change here would silently write a firmware image
// to the gateware target.
TEST(DeviceUpdaterTarget, TheWireIndicesAreWhatTheProtocolSays) {
  EXPECT_EQ(static_cast<uint16_t>(UpdateTarget::kFirmware),
            kUpdateTargetFirmware);
  EXPECT_EQ(static_cast<uint16_t>(UpdateTarget::kGateware),
            kUpdateTargetGateware);
}

// --- What the identity says about the gateware -----------------------------

TEST(DeviceIdentityGateware, TheFactoryImageIsRecognisedAsRecovery) {
  DeviceIdentity identity;
  identity.gateware_present = true;
  identity.register_map_version = kRegisterMapVersionMaximum;

  identity.image_role = kImageRoleApplication;
  EXPECT_FALSE(identity.GatewareIsRecovery());

  identity.image_role = kImageRoleFactory;
  EXPECT_TRUE(identity.GatewareIsRecovery());
}

// A role byte read from an FPGA that never answered is a byte that means
// nothing. Reading it as "factory" would put a working device into a recovery
// state that does not exist for it.
TEST(DeviceIdentityGateware, AnFpgaThatDidNotAnswerHasNoRoleToBelieve) {
  DeviceIdentity identity;
  identity.gateware_present = false;
  identity.register_map_version = kRegisterMapVersionMaximum;
  identity.image_role = kImageRoleFactory;

  EXPECT_FALSE(identity.GatewareIsRecovery());
}

// --- Reading a device back while holding it open ---------------------------

// The product string and the protocol version are descriptor fields, and the
// updater reports them while it has the device open for writing. Where they
// are *read* therefore matters: on the WinUSB backend, listing devices means
// opening each one, so an enumeration made from inside the updater asks
// Windows for a second handle to a device this process already holds — and the
// device is quietly missing from the list that comes back.
//
// It read as a board that had not been programmed. The bring-up wizard's last
// page ticked "the Duplicator's own firmware is running", which it takes from
// the device list it polls with nothing open, and then crossed the protocol,
// the firmware commit and the gateware — every row that comes from the
// identity — on a board that was in fact correctly programmed (bench, Windows,
// 2026-08-19).
TEST(DeviceUpdaterIdentity, ItReadsTheDescriptorsBeforeItHoldsTheDeviceOpen) {
  FakeUsbDevice usb;
  usb.SetSingleDevice("device", DeviceSpeed::kSuper,
                      "Domesday Duplicator (4d23a25)");
  usb.SetHiddenWhileChannelOpen(true);

  const std::unique_ptr<IDeviceUpdater> updater =
      MakeDeviceUpdater(usb, "device", nullptr);
  ASSERT_NE(updater, nullptr);

  const std::optional<DeviceIdentity> identity = updater->ReadIdentity();
  ASSERT_TRUE(identity.has_value());
  EXPECT_EQ(identity.value_or(DeviceIdentity{}).product_string,
            "Domesday Duplicator (4d23a25)");
}

// And the same backend behaviour must not stop a device being opened for
// updating in the first place, which is the other thing the enumeration was
// sitting in front of.
TEST(DeviceUpdaterIdentity,
     ADeviceIsStillOpenedWhenTheBackendHidesItAfterwards) {
  FakeUsbDevice usb;
  usb.SetSingleDevice("device", DeviceSpeed::kSuper, "Domesday Duplicator");
  usb.SetHiddenWhileChannelOpen(true);

  EXPECT_NE(MakeDeviceUpdater(usb, "device", nullptr), nullptr);
}

}  // namespace
}  // namespace ddd::capture
