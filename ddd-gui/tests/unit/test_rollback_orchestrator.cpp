/************************************************************************

    test_rollback_orchestrator.cpp

    T1 unit test for the rollback ordering and the two halves it sequences
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <span>
#include <string>
#include <vector>

#include "boot_image_fixture.h"
#include "digest.h"
#include "fake_device_updater.h"
#include "rollback_orchestrator.h"
#include "update_fixtures.h"

namespace ddd::capture {
namespace {

// A rollback set built in memory: the original firmware and the original
// gateware, the second of which goes to the factory region because that is
// where an EPCS boots from.
struct Fixture {
  std::vector<uint8_t> firmware = test::MakeBootImage();
  std::vector<uint8_t> gateware;
  UpdateBundle bundle;
  FakeDeviceUpdater updater;

  Fixture() {
    bundle.manifest.manifest_version = kUpdateManifestVersion;
    bundle.manifest.purpose = UpdatePurpose::kRollback;
    bundle.manifest.version = "1.4.0";
    bundle.manifest.commit = "bb65470";

    UpdateComponent image;
    image.file = std::string(kFirmwareEntryName);
    image.length = firmware.size();
    image.sha256 = Sha256(firmware);
    image.identity = "bb65470";
    image.interface_version = 1;
    bundle.manifest.firmware = image;
    bundle.firmware = firmware;

    gateware.assign(
        reinterpret_cast<const uint8_t*>(test::kFactoryGatewarePayload.data()),
        reinterpret_cast<const uint8_t*>(test::kFactoryGatewarePayload.data()) +
            test::kFactoryGatewarePayload.size());

    UpdateComponent factory;
    factory.file = std::string(kFactoryGatewareEntryName);
    factory.length = gateware.size();
    factory.sha256 = Sha256(gateware);
    factory.identity = "bb65470";
    factory.interface_version = 1;
    bundle.manifest.factory_gateware = factory;
    bundle.factory_gateware = gateware;
  }

  void Configure(RollbackOrchestrator& orchestrator) {
    UpdateTimings timings;
    timings.poll_interval = std::chrono::milliseconds(1);
    timings.return_timeout = std::chrono::milliseconds(50);
    timings.stall_timeout = std::chrono::milliseconds(500);
    orchestrator.SetTimings(timings);
  }
};

// --- the ordering ---------------------------------------------------------
//
// The one property in this file that protects hardware rather than data, and
// it is the mirror image of the bring-up rule: the FX3 is the last thing to
// become legacy, so the FPGA goes first.

TEST(RollbackOrchestrator, RefusesTheFx3BeforeTheFpga) {
  Fixture fixture;
  RollbackOrchestrator orchestrator(fixture.updater, nullptr);
  fixture.Configure(orchestrator);

  const UpdateOutcome outcome = orchestrator.InstallFirmware(fixture.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_NE(outcome.problem.find("gateware"), std::string::npos)
      << outcome.problem;

  // And nothing was sent: the refusal happens before the transfer opens, so a
  // wrongly wired caller does not write a byte.
  EXPECT_EQ(fixture.updater.begin_count(), 0u);
}

TEST(RollbackOrchestrator, RefusesTheFx3WhenTheFpgaStepFailed) {
  Fixture fixture;
  fixture.updater.RefuseTarget(UpdateTarget::kEpcsFactory);

  RollbackOrchestrator orchestrator(fixture.updater, nullptr);
  fixture.Configure(orchestrator);

  ASSERT_FALSE(orchestrator.ProgramGateware(fixture.bundle).succeeded);
  EXPECT_FALSE(orchestrator.gateware_installed());

  const UpdateOutcome outcome = orchestrator.InstallFirmware(fixture.bundle);
  EXPECT_FALSE(outcome.succeeded);
}

TEST(RollbackOrchestrator, ProgramsBothHalvesInOrder) {
  Fixture fixture;
  RollbackOrchestrator orchestrator(fixture.updater, nullptr);
  fixture.Configure(orchestrator);

  const UpdateOutcome gateware = orchestrator.ProgramGateware(fixture.bundle);
  ASSERT_TRUE(gateware.succeeded) << gateware.problem;
  EXPECT_TRUE(orchestrator.gateware_installed());

  const UpdateOutcome firmware = orchestrator.InstallFirmware(fixture.bundle);
  ASSERT_TRUE(firmware.succeeded) << firmware.problem;

  EXPECT_EQ(fixture.updater.begin_count(), 2u);
  EXPECT_EQ(fixture.updater.target(), UpdateTarget::kFirmware);

  // What each half committed, read back off the fake. The gateware went to the
  // factory region and not the application one, which is the difference
  // between a unit that boots the original image and a unit that boots
  // nothing.
  EXPECT_EQ(fixture.updater.received(UpdateTarget::kEpcsFactory),
            fixture.gateware);
  EXPECT_TRUE(fixture.updater.received(UpdateTarget::kGateware).empty());
  EXPECT_EQ(fixture.updater.received(UpdateTarget::kFirmware),
            fixture.firmware);
}

// Neither half restarts the device. Both images become the running ones at the
// power cycle the wizard asks for, or neither does — which is what keeps the
// original firmware from ever meeting the current gateware.
TEST(RollbackOrchestrator, NeitherHalfRestartsTheDevice) {
  Fixture fixture;
  RollbackOrchestrator orchestrator(fixture.updater, nullptr);
  fixture.Configure(orchestrator);

  ASSERT_TRUE(orchestrator.ProgramGateware(fixture.bundle).succeeded);
  const UpdateOutcome firmware = orchestrator.InstallFirmware(fixture.bundle);
  ASSERT_TRUE(firmware.succeeded) << firmware.problem;

  EXPECT_EQ(fixture.updater.reset_count(), 0u);
  EXPECT_FALSE(firmware.identity_confirmed);

  // And the FPGA is not reloaded either. Reconfiguring would load the image
  // that has just been written over — from a flash the device is in the middle
  // of rolling back — while the modern firmware is still driving it.
  EXPECT_EQ(fixture.updater.reconfigure_count(), 0u);
}

// --- what it refuses ------------------------------------------------------

TEST(RollbackOrchestrator, RefusesASetThatCarriesNoGateware) {
  Fixture fixture;
  fixture.bundle.manifest.factory_gateware.reset();
  fixture.bundle.factory_gateware = {};

  RollbackOrchestrator orchestrator(fixture.updater, nullptr);
  fixture.Configure(orchestrator);

  const UpdateOutcome outcome = orchestrator.ProgramGateware(fixture.bundle);
  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(fixture.updater.begin_count(), 0u);
}

// Firmware that predates the factory target refuses it before a byte moves,
// and the message a user sees has to be the device's own — "no such target on
// this firmware" — rather than a generic failure. This is the one way a
// rollback can meet a device it cannot roll back, because the target is an
// additive protocol change that deliberately did not bump the version.
TEST(RollbackOrchestrator, OlderFirmwareRefusesTheFactoryTargetByName) {
  Fixture fixture;
  fixture.updater.RefuseTarget(UpdateTarget::kEpcsFactory);

  RollbackOrchestrator orchestrator(fixture.updater, nullptr);
  fixture.Configure(orchestrator);

  const UpdateOutcome outcome = orchestrator.ProgramGateware(fixture.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.problem, DeviceUpdateErrorText(DeviceUpdateError::kTarget));
  EXPECT_TRUE(fixture.updater.received(UpdateTarget::kEpcsFactory).empty());
}

// --- progress -------------------------------------------------------------

TEST(RollbackOrchestrator, ReportsProgressAsTheUpdateFlowDoes) {
  Fixture fixture;
  std::vector<UpdateProgress> reports;

  RollbackOrchestrator orchestrator(fixture.updater, nullptr);
  fixture.Configure(orchestrator);
  orchestrator.SetProgressCallback(
      [&reports](const UpdateProgress& report) { reports.push_back(report); });

  ASSERT_TRUE(orchestrator.ProgramGateware(fixture.bundle).succeeded);
  ASSERT_FALSE(reports.empty());
  EXPECT_EQ(reports.back().target, UpdateTarget::kEpcsFactory);

  reports.clear();
  ASSERT_TRUE(orchestrator.InstallFirmware(fixture.bundle).succeeded);
  ASSERT_FALSE(reports.empty());
  EXPECT_FALSE(reports.back().message.empty());
}

}  // namespace
}  // namespace ddd::capture
