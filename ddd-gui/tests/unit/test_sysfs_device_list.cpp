/************************************************************************

    test_sysfs_device_list.cpp

    T1 tests for the kernel's own view of what is attached
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "sysfs_device_list.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

// A directory laid out the way /sys/bus/usb/devices is: one directory per
// device, each with an idVendor and an idProduct file holding four hex digits.
class SysfsFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    root_ = std::filesystem::temp_directory_path() /
            (std::string("ddd-sysfs-") + info->name());
    std::filesystem::remove_all(root_);
    std::filesystem::create_directories(root_);
  }

  void TearDown() override { std::filesystem::remove_all(root_); }

  // One device directory, named the way the kernel names them.
  void AddDevice(const std::string& name, const std::string& vendor,
                 const std::string& product) {
    const std::filesystem::path directory = root_ / name;
    std::filesystem::create_directories(directory);
    Write(directory / "idVendor", vendor);
    Write(directory / "idProduct", product);
  }

  // A directory with no identifier files at all, which is what a USB
  // *interface* looks like: they sit in the same directory as the devices.
  void AddInterface(const std::string& name) {
    const std::filesystem::path directory = root_ / name;
    std::filesystem::create_directories(directory);
    Write(directory / "bInterfaceNumber", "00");
  }

  static void Write(const std::filesystem::path& path,
                    const std::string& contents) {
    std::ofstream file(path);
    file << contents << "\n";
  }

  std::filesystem::path root_;
};

// A stand-in for the absent answer, so the tests read an optional the way the
// rest of the suite does — value_or with a default rather than a dereference
// the static analysis cannot see the check for.
std::vector<UsbIdentity> EmptyList() { return {}; }

// Four hex digits, as sysfs writes them.
std::string Hex(uint16_t value) {
  static const char kDigits[] = "0123456789abcdef";
  std::string text(4, '0');
  for (int position = 3; position >= 0; --position) {
    text[static_cast<size_t>(position)] = kDigits[value & 0xF];
    value = static_cast<uint16_t>(value >> 4);
  }
  return text;
}

TEST_F(SysfsFixture, ADeviceOnTheBusIsListed) {
  AddDevice("1-2", Hex(kVendorId), Hex(kProductId));

  const std::optional<std::vector<UsbIdentity>> found =
      ReadSysfsDeviceIdentities(root_);
  ASSERT_TRUE(found.has_value());

  const std::vector<UsbIdentity> listed = found.value_or(EmptyList());
  ASSERT_EQ(listed.size(), 1U);
  EXPECT_EQ(listed.front().vendor, kVendorId);
  EXPECT_EQ(listed.front().product, kProductId);
}

TEST_F(SysfsFixture, EveryPersonalityIsRecognisedAndNothingElseIs) {
  AddDevice("1-1", Hex(kVendorId), Hex(kProductId));
  AddDevice("1-2", Hex(kLegacyVendorId), Hex(kLegacyProductId));
  AddDevice("1-3", Hex(kCypressVendorId), Hex(kRecoveryProductId));
  AddDevice("1-4", Hex(kCypressVendorId), Hex(kFlashProgrammerProductId));

  // The Explorer Kit's on-board USB-UART, which shares the Cypress vendor
  // identifier and is emphatically not a Duplicator. Counting it would have
  // the debug serial port look like a device appearing.
  AddDevice("1-5", Hex(kCypressVendorId), "0007");

  // And somebody's mouse.
  AddDevice("2-1", "046d", "c52b");

  const std::optional<std::vector<UsbIdentity>> found =
      ReadSysfsDeviceIdentities(root_);
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found.value_or(EmptyList()).size(), 4U);
}

// The directory holds interfaces beside devices, and holds files the kernel
// puts there for its own reasons. Anything without a readable pair of
// identifier files is not a device as far as this is concerned.
TEST_F(SysfsFixture, EntriesThatAreNotDevicesAreSkipped) {
  AddDevice("1-1", Hex(kVendorId), Hex(kProductId));

  // The kernel names interfaces "1-1:1.0", and that is the name used where
  // the filesystem allows it. Windows reserves the colon for alternate data
  // streams, so creating that directory throws there and failed every MSI
  // build; the stand-in below is the same thing to the code under test, which
  // only ever asks whether the identifier files can be read.
#ifdef _WIN32
  AddInterface("1-1_1.0");
#else
  AddInterface("1-1:1.0");
#endif

  Write(root_ / "usb1", "not a directory at all");

  const std::optional<std::vector<UsbIdentity>> found =
      ReadSysfsDeviceIdentities(root_);
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found.value_or(EmptyList()).size(), 1U);
}

// A misread identifier is worse than a missing one: it would name a device
// that is not there, and this list is compared against libusb's to decide
// whether to restart the USB context. A permanent phantom would restart it
// for ever.
TEST_F(SysfsFixture, AnIdentifierThatIsNotFourHexDigitsIsNotGuessedAt) {
  AddDevice("1-1", "12", Hex(kProductId));
  AddDevice("1-2", "zzzz", Hex(kProductId));
  AddDevice("1-3", Hex(kVendorId), "234567");

  const std::optional<std::vector<UsbIdentity>> found =
      ReadSysfsDeviceIdentities(root_);
  ASSERT_TRUE(found.has_value());
  EXPECT_TRUE(found.value_or(EmptyList()).empty());
}

// The distinction the whole mechanism rests on. A platform with no sysfs must
// answer "I cannot tell you", because "nothing is attached" would be read as a
// disagreement with libusb and restart the context on every poll.
TEST_F(SysfsFixture, NoSysfsIsNoAnswerRatherThanAnEmptyOne) {
  EXPECT_FALSE(ReadSysfsDeviceIdentities(root_ / "does-not-exist").has_value());

  const std::optional<std::vector<UsbIdentity>> empty =
      ReadSysfsDeviceIdentities(root_);
  ASSERT_TRUE(empty.has_value())
      << "an empty bus is an answer and must not read as an absent one";
  EXPECT_TRUE(empty.value_or(EmptyList()).empty());
}

TEST(UsbIdentityTest, OnlyTheFourPersonalitiesAreRecognised) {
  EXPECT_TRUE(IsRecognisedUsbIdentity({kVendorId, kProductId}));
  EXPECT_TRUE(IsRecognisedUsbIdentity({kLegacyVendorId, kLegacyProductId}));
  EXPECT_TRUE(IsRecognisedUsbIdentity({kCypressVendorId, kRecoveryProductId}));
  EXPECT_TRUE(
      IsRecognisedUsbIdentity({kCypressVendorId, kFlashProgrammerProductId}));

  EXPECT_FALSE(IsRecognisedUsbIdentity({kCypressVendorId, 0x0007}));
  EXPECT_FALSE(IsRecognisedUsbIdentity({kVendorId, kLegacyProductId}));
  EXPECT_FALSE(IsRecognisedUsbIdentity({0x046d, 0xc52b}));
}

TEST(DeviceViewsTest, TheSameDevicesInAnyOrderAgree) {
  const std::vector<UsbIdentity> kernel{{kVendorId, kProductId},
                                        {kCypressVendorId, kRecoveryProductId}};
  const std::vector<UsbIdentity> libusb{{kCypressVendorId, kRecoveryProductId},
                                        {kVendorId, kProductId}};

  EXPECT_FALSE(DeviceViewsDisagree(kernel, libusb));
  EXPECT_FALSE(DeviceViewsDisagree({}, {}));
}

// The case this exists for: the device came back after an update and libusb
// was never told.
TEST(DeviceViewsTest, AnAttachLibUsbMissedIsADisagreement) {
  EXPECT_TRUE(DeviceViewsDisagree({{kVendorId, kProductId}}, {}));
}

// And the other direction, which is the same stale cache seen from the other
// side and is mended the same way.
TEST(DeviceViewsTest, ADetachLibUsbMissedIsADisagreement) {
  EXPECT_TRUE(DeviceViewsDisagree({}, {{kVendorId, kProductId}}));
}

// Two boards on one bench is a real arrangement, and a second one appearing is
// news even though its identifiers were already in both lists.
TEST(DeviceViewsTest, ASecondDeviceOfTheSameKindIsCounted) {
  const std::vector<UsbIdentity> one{{kVendorId, kProductId}};
  const std::vector<UsbIdentity> two{{kVendorId, kProductId},
                                     {kVendorId, kProductId}};

  EXPECT_TRUE(DeviceViewsDisagree(two, one));
  EXPECT_FALSE(DeviceViewsDisagree(two, two));
}

// A device wearing the flash programmer's identifiers that fails the 0xB0
// probe is deliberately dropped from the device list. The comparison is made
// against what libusb *enumerated* rather than what survived that filter,
// because a disagreement no rescan can resolve would restart the context on
// every poll for ever.
TEST(DeviceViewsTest, TheProbeFilterIsNotAllowedToLookLikeADisagreement) {
  const std::vector<UsbIdentity> kernel{
      {kCypressVendorId, kFlashProgrammerProductId}};
  const std::vector<UsbIdentity> enumerated{
      {kCypressVendorId, kFlashProgrammerProductId}};

  EXPECT_FALSE(DeviceViewsDisagree(kernel, enumerated));
}

}  // namespace
}  // namespace ddd::capture
