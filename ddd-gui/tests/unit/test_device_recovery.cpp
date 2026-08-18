/************************************************************************

    test_device_recovery.cpp

    Installing onto a device that has no working firmware to install with
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "boot_image_fixture.h"
#include "device_recovery.h"
#include "digest.h"
#include "fake_device_programmer.h"
#include "fake_device_updater.h"
#include "update_bundle.h"
#include "update_fixtures.h"

namespace ddd::capture {
namespace {

// A wrapper that forwards to a fake the test keeps.
//
// The installer takes ownership of what its factories hand back, so the fakes
// themselves cannot be what is handed over: a test that wanted to read what
// happened afterwards would be reading freed memory.
class BorrowedProgrammer : public IDeviceProgrammer {
 public:
  explicit BorrowedProgrammer(FakeDeviceProgrammer* fake) : fake_(fake) {}

  bool WriteRam(uint32_t address, std::span<const uint8_t> data) override {
    return fake_->WriteRam(address, data);
  }
  bool Start(uint32_t entry_address) override {
    return fake_->Start(entry_address);
  }
  std::optional<std::string> WaitForApplication(
      std::chrono::milliseconds timeout) override {
    return fake_->WaitForApplication(timeout);
  }

 private:
  FakeDeviceProgrammer* fake_ = nullptr;
};

class BorrowedUpdater : public IDeviceUpdater {
 public:
  explicit BorrowedUpdater(FakeDeviceUpdater* fake) : fake_(fake) {}

  std::optional<DeviceIdentity> ReadIdentity() override {
    return fake_->ReadIdentity();
  }
  std::optional<DeviceUpdateStatus> ReadStatus() override {
    return fake_->ReadStatus();
  }
  bool Begin(UpdateTarget target, uint64_t length,
             const Sha256Digest& digest) override {
    return fake_->Begin(target, length, digest);
  }
  bool SendChunk(UpdateTarget target, uint16_t index,
                 std::span<const uint8_t> data) override {
    return fake_->SendChunk(target, index, data);
  }
  bool Finish(UpdateTarget target) override { return fake_->Finish(target); }
  bool Reset() override { return fake_->Reset(); }
  bool ReconfigureFpga() override { return fake_->ReconfigureFpga(); }
  std::optional<DeviceIdentity> WaitForReturn(
      std::chrono::milliseconds timeout) override {
    return fake_->WaitForReturn(timeout);
  }

 private:
  FakeDeviceUpdater* fake_ = nullptr;
};

// A bundle built in memory rather than opened from a signed archive.
//
// UpdateBundle is what OpenUpdateBundle produces *after* the signature and
// every digest have passed, so constructing one directly is not skipping a
// check — it is starting where the checks finished, which is exactly where
// the installer starts.
struct RecoveryFixture {
  std::vector<uint8_t> firmware = test::MakeBootImage();
  std::vector<uint8_t> gateware;
  UpdateBundle bundle;

  FakeDeviceProgrammer programmer;
  FakeDeviceUpdater updater;

  // Where the updater factory was asked to look, so a test can check the
  // installer used the path the device came back at rather than the one it
  // went away from.
  std::string updater_path;
  int updater_opens = 0;

  RecoveryFixture() {
    bundle.manifest.manifest_version = 1;
    bundle.manifest.version = "1.4.0";
    bundle.manifest.commit = "0123abcd";

    UpdateComponent component;
    component.file = "firmware.img";
    component.length = firmware.size();
    component.sha256 = Sha256(firmware);
    component.identity = "0123abcd";
    component.interface_version = 1;
    bundle.manifest.firmware = component;

    bundle.firmware = firmware;
  }

  DeviceAccess Access() {
    DeviceAccess access;
    access.open_programmer = [this] {
      return std::make_unique<BorrowedProgrammer>(&programmer);
    };
    access.open_updater = [this](const std::string& path) {
      updater_path = path;
      ++updater_opens;
      return std::make_unique<BorrowedUpdater>(&updater);
    };
    return access;
  }

  UpdateOutcome Run() {
    RecoveryInstaller installer(Access(), nullptr);

    // The whole flow in milliseconds rather than in seconds. Nothing here
    // waits for a real device.
    DeviceRecoveryTimings timings;
    timings.return_timeout = std::chrono::milliseconds(50);
    installer.SetTimings(timings);

    UpdateTimings update_timings;
    update_timings.poll_interval = std::chrono::milliseconds(1);
    update_timings.return_timeout = std::chrono::milliseconds(50);
    update_timings.stall_timeout = std::chrono::milliseconds(500);
    installer.SetUpdateTimings(update_timings);

    return installer.Run(bundle);
  }
};

// The whole point of the design: a device with no firmware is programmed by
// the ordinary update path, so the bytes reach its EEPROM through the same
// protocol, the same stream digest and the same readback digest a routine
// update uses. Nothing about a first-time programming is a shortcut.
TEST(DeviceRecoveryTest, ItWakesTheDeviceAndThenUpdatesItNormally) {
  RecoveryFixture fixture;

  const UpdateOutcome outcome = fixture.Run();

  ASSERT_TRUE(outcome.succeeded) << outcome.problem;
  EXPECT_EQ(outcome.stage, UpdateStage::kComplete);

  // The prelude: both sections downloaded, then started.
  EXPECT_EQ(fixture.programmer.sections_written(), 2u);
  EXPECT_EQ(fixture.programmer.bytes_written(), 320u);
  EXPECT_TRUE(fixture.programmer.started());
  EXPECT_EQ(fixture.programmer.entry_address(), 0x40003000u);

  // And then an ordinary update, which is what wrote the EEPROM.
  EXPECT_EQ(fixture.updater.begin_count(), 1u);
  EXPECT_EQ(fixture.updater.reset_count(), 1u);
}

TEST(DeviceRecoveryTest, TheDownloadedBytesAreTheImagesOwnBytes) {
  RecoveryFixture fixture;
  ASSERT_TRUE(fixture.Run().succeeded);

  const std::vector<uint8_t>& first = fixture.programmer.section_at(0x40003000);
  ASSERT_EQ(first.size(), 256u);
  EXPECT_EQ(first.front(), 0xA5);
  EXPECT_EQ(first.back(), 0xA5);

  const std::vector<uint8_t>& second =
      fixture.programmer.section_at(0x40008000);
  ASSERT_EQ(second.size(), 64u);
  EXPECT_EQ(second.front(), 0x5A);
}

// The path is asked for rather than assumed, because on Windows a device's
// path changes when its personality does.
TEST(DeviceRecoveryTest, TheUpdaterIsOpenedAtThePathTheDeviceCameBackAt) {
  RecoveryFixture fixture;
  fixture.programmer.SetReturnedPath("/sys/bus/usb/devices/3-2.1");

  ASSERT_TRUE(fixture.Run().succeeded);

  EXPECT_EQ(fixture.updater_opens, 1);
  EXPECT_EQ(fixture.updater_path, "/sys/bus/usb/devices/3-2.1");
}

TEST(DeviceRecoveryTest, ABundleWithNoFirmwareCannotRecoverADevice) {
  RecoveryFixture fixture;
  fixture.bundle.manifest.firmware.reset();
  fixture.bundle.firmware = {};

  const UpdateOutcome outcome = fixture.Run();

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.stage, UpdateStage::kPreparing);
  EXPECT_NE(outcome.problem.find("does not contain any"), std::string::npos)
      << outcome.problem;

  // And nothing was sent anywhere.
  EXPECT_EQ(fixture.programmer.sections_written(), 0u);
  EXPECT_FALSE(fixture.programmer.started());
}

// A payload that is not an FX3 image is caught before the device is opened,
// let alone written to.
TEST(DeviceRecoveryTest,
     APayloadThatIsNotAnImageIsRefusedBeforeAnythingIsSent) {
  RecoveryFixture fixture;
  const std::string text = "this is not a boot image";
  fixture.firmware.assign(text.begin(), text.end());
  fixture.bundle.firmware = fixture.firmware;
  UpdateComponent component = test::Checked(fixture.bundle.manifest.firmware);
  component.length = fixture.firmware.size();
  component.sha256 = Sha256(fixture.firmware);
  fixture.bundle.manifest.firmware = component;

  const UpdateOutcome outcome = fixture.Run();

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.stage, UpdateStage::kPreparing);
  EXPECT_EQ(fixture.programmer.sections_written(), 0u);
  EXPECT_FALSE(fixture.programmer.started());
}

TEST(DeviceRecoveryTest, ADownloadThatStopsPartWayLeavesTheDeviceAsItWas) {
  RecoveryFixture fixture;
  fixture.programmer.SetFault(FakeDeviceProgrammer::Fault::kRefuseDownload);
  fixture.programmer.SetFailAtSection(1);

  const UpdateOutcome outcome = fixture.Run();

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.stage, UpdateStage::kPreparing);
  EXPECT_FALSE(fixture.programmer.started());

  // Nothing permanent was written, and the message says so — a device whose
  // RAM download failed is exactly as recoverable as it was a moment before.
  EXPECT_NE(outcome.problem.find("Nothing was written permanently"),
            std::string::npos)
      << outcome.problem;
  EXPECT_EQ(fixture.updater.begin_count(), 0u);
}

TEST(DeviceRecoveryTest, ADeviceThatWillNotStartWhatItWasGivenIsReported) {
  RecoveryFixture fixture;
  fixture.programmer.SetFault(FakeDeviceProgrammer::Fault::kRefuseStart);

  const UpdateOutcome outcome = fixture.Run();

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.stage, UpdateStage::kPreparing);
  EXPECT_EQ(fixture.updater.begin_count(), 0u);
  EXPECT_FALSE(outcome.problem.empty());
}

TEST(DeviceRecoveryTest, ADeviceThatNeverComesBackIsReportedWithANextStep) {
  RecoveryFixture fixture;
  fixture.programmer.SetFault(FakeDeviceProgrammer::Fault::kNeverReturns);

  const UpdateOutcome outcome = fixture.Run();

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.stage, UpdateStage::kPreparing);
  EXPECT_NE(outcome.problem.find("Unplug it"), std::string::npos)
      << outcome.problem;
  EXPECT_EQ(fixture.updater.begin_count(), 0u);
}

// The stages a caller sees are the same stages an ordinary update produces,
// with one prelude in front of them. Anything watching — the wizard, the
// command-line tool — therefore needs no second description of what an
// install looks like.
TEST(DeviceRecoveryTest, ItReportsThePreparingStageAndThenTheOrdinaryOnes) {
  RecoveryFixture fixture;

  std::vector<UpdateStage> stages;
  RecoveryInstaller installer(fixture.Access(), nullptr);

  DeviceRecoveryTimings timings;
  timings.return_timeout = std::chrono::milliseconds(50);
  installer.SetTimings(timings);

  UpdateTimings update_timings;
  update_timings.poll_interval = std::chrono::milliseconds(1);
  update_timings.return_timeout = std::chrono::milliseconds(50);
  update_timings.stall_timeout = std::chrono::milliseconds(500);
  installer.SetUpdateTimings(update_timings);

  installer.SetProgressCallback([&stages](const UpdateProgress& step) {
    if (stages.empty() || stages.back() != step.stage) {
      stages.push_back(step.stage);
    }
  });

  ASSERT_TRUE(installer.Run(fixture.bundle).succeeded);

  ASSERT_FALSE(stages.empty());
  EXPECT_EQ(stages.front(), UpdateStage::kPreparing);
  EXPECT_EQ(stages.back(), UpdateStage::kComplete);

  EXPECT_NE(std::find(stages.begin(), stages.end(), UpdateStage::kTransferring),
            stages.end());
  EXPECT_NE(std::find(stages.begin(), stages.end(), UpdateStage::kConfirming),
            stages.end());
}

// Cancelling during the prelude stops before the device has been started, so
// nothing is left running from memory and nothing has been written.
TEST(DeviceRecoveryTest, CancellingDuringThePreludeStopsBeforeTheDeviceStarts) {
  RecoveryFixture fixture;

  RecoveryInstaller installer(fixture.Access(), nullptr);
  installer.SetCancelCallback([] { return true; });

  const UpdateOutcome outcome = installer.Run(fixture.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(fixture.programmer.sections_written(), 0u);
  EXPECT_FALSE(fixture.programmer.started());
}

}  // namespace
}  // namespace ddd::capture
