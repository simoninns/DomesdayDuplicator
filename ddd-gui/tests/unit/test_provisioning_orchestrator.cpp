/************************************************************************

    test_provisioning_orchestrator.cpp

    T1 unit test for the bring-up ordering and the two halves it sequences
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "boot_image_fixture.h"
#include "digest.h"
#include "fake_device_programmer.h"
#include "fake_device_updater.h"
#include "fake_jtag_cable.h"
#include "provisioning_orchestrator.h"
#include "svf_fixtures.h"
#include "update_fixtures.h"

namespace ddd::capture {
namespace {

// Forwarders, for the reason test_device_recovery.cpp has them: the
// orchestrator takes ownership of what its factories hand back, so the fakes
// themselves cannot be what is handed over or a test could not read them
// afterwards.
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

class BorrowedCable : public IJtagCable {
 public:
  explicit BorrowedCable(FakeJtagCable* fake) : fake_(fake) {}

  bool Shift(std::span<const uint8_t> tms, std::span<const uint8_t> tdi,
             size_t bit_count, std::vector<uint8_t>* tdo) override {
    return fake_->Shift(tms, tdi, bit_count, tdo);
  }
  bool RunClock(size_t count) override { return fake_->RunClock(count); }
  bool Flush() override { return fake_->Flush(); }
  const char* Name() const override { return fake_->Name(); }

 private:
  FakeJtagCable* fake_ = nullptr;
};

// A provisioning set built in memory. UpdateBundle is what OpenUpdateBundle
// produces *after* the signature and every digest have passed, so building one
// directly starts where the checks finished — which is where the orchestrator
// starts.
struct Fixture {
  std::vector<uint8_t> firmware = test::MakeBootImage();
  std::vector<uint8_t> vectors;
  std::vector<uint8_t> factory_image;
  UpdateBundle bundle;

  FakeDeviceProgrammer programmer;
  FakeDeviceUpdater updater;
  FakeJtagCable cable;

  int cable_opens = 0;
  bool cable_available = true;

  explicit Fixture(std::string_view svf = kGrammarSvf)
      : vectors(reinterpret_cast<const uint8_t*>(svf.data()),
                reinterpret_cast<const uint8_t*>(svf.data()) + svf.size()) {
    bundle.manifest.manifest_version = kUpdateManifestVersion;
    bundle.manifest.version = "1.4.0";
    bundle.manifest.commit = "0123abcd";

    UpdateComponent component;
    component.file = std::string(kFirmwareEntryName);
    component.length = firmware.size();
    component.sha256 = Sha256(firmware);
    component.identity = "0123abcd";
    component.interface_version = 1;
    bundle.manifest.firmware = component;
    bundle.firmware = firmware;

    UpdateComponent provisioning;
    provisioning.file = std::string(kProvisioningEntryName);
    provisioning.length = vectors.size();
    provisioning.sha256 = Sha256(vectors);
    provisioning.identity = "0123abcd";
    provisioning.interface_version = 2;
    bundle.manifest.provisioning = provisioning;
    bundle.provisioning = vectors;

    // The image the vectors make writable. A set carrying only vectors cannot
    // bring a board up: they configure an FPGA and write nothing, so without
    // this the flash would still hold whatever it held before.
    factory_image.assign(
        reinterpret_cast<const uint8_t*>(test::kFactoryGatewarePayload.data()),
        reinterpret_cast<const uint8_t*>(test::kFactoryGatewarePayload.data()) +
            test::kFactoryGatewarePayload.size());

    UpdateComponent factory;
    factory.file = std::string(kFactoryGatewareEntryName);
    factory.length = factory_image.size();
    factory.sha256 = Sha256(factory_image);
    factory.identity = "0123abcd";
    factory.interface_version = 2;
    bundle.manifest.factory_gateware = factory;
    bundle.factory_gateware = factory_image;
  }

  ProvisioningAccess Access() {
    ProvisioningAccess access;
    access.fx3.open_programmer = [this] {
      return std::make_unique<BorrowedProgrammer>(&programmer);
    };
    access.fx3.open_updater = [this](const std::string&) {
      return std::make_unique<BorrowedUpdater>(&updater);
    };
    access.open_cable =
        [this](std::string* problem) -> std::unique_ptr<IJtagCable> {
      ++cable_opens;
      if (!cable_available) {
        if (problem != nullptr) {
          *problem = "No USB-Blaster is attached.";
        }
        return nullptr;
      }
      return std::make_unique<BorrowedCable>(&cable);
    };
    return access;
  }

  // The whole flow in milliseconds rather than in minutes.
  void Configure(ProvisioningOrchestrator& orchestrator) {
    DeviceRecoveryTimings timings;
    timings.return_timeout = std::chrono::milliseconds(50);
    orchestrator.SetTimings(timings);

    UpdateTimings update_timings;
    update_timings.poll_interval = std::chrono::milliseconds(1);
    update_timings.return_timeout = std::chrono::milliseconds(50);
    update_timings.stall_timeout = std::chrono::milliseconds(500);
    orchestrator.SetUpdateTimings(update_timings);
  }
};

// --- the ordering ---------------------------------------------------------
//
// The one property in this file that protects hardware rather than data. The
// FX3 and the FPGA share a net that the legacy firmware drives and the modern
// gateware drives, so the FX3 must become modern first — and that is checked
// here rather than left to the order a wizard happens to lay its pages out in.

TEST(ProvisioningOrchestrator, RefusesTheFpgaBeforeTheFx3) {
  Fixture fixture;
  ProvisioningOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  const ProvisioningGatewareOutcome outcome =
      orchestrator.ProgramGateware(fixture.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_NE(outcome.problem.find("FX3"), std::string::npos) << outcome.problem;

  // And nothing was reached for: the refusal happens before the cable is
  // opened, so a wrongly wired caller does not so much as claim the cable.
  EXPECT_EQ(fixture.cable_opens, 0);
  EXPECT_EQ(fixture.cable.clocks(), 0u);
}

TEST(ProvisioningOrchestrator, RefusesTheFpgaWhenTheFx3StepFailed) {
  Fixture fixture;
  fixture.programmer.SetFault(FakeDeviceProgrammer::Fault::kRefuseStart);

  ProvisioningOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  EXPECT_FALSE(orchestrator.InstallFirmware(fixture.bundle).succeeded);
  EXPECT_FALSE(orchestrator.firmware_installed());
  EXPECT_FALSE(orchestrator.ProgramGateware(fixture.bundle).succeeded);
  EXPECT_EQ(fixture.cable_opens, 0);
}

TEST(ProvisioningOrchestrator, ProgramsBothHalvesInOrder) {
  Fixture fixture;
  ProvisioningOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  const UpdateOutcome firmware = orchestrator.InstallFirmware(fixture.bundle);
  ASSERT_TRUE(firmware.succeeded) << firmware.problem;
  EXPECT_TRUE(orchestrator.firmware_installed());

  const ProvisioningGatewareOutcome gateware =
      orchestrator.ProgramGateware(fixture.bundle);
  ASSERT_TRUE(gateware.succeeded) << gateware.problem;

  // Two transfers, not one: the firmware into the EEPROM, and then the factory
  // image into the flash the configured FPGA has just made reachable.
  EXPECT_EQ(fixture.updater.begin_count(), 2u);
  EXPECT_EQ(fixture.updater.target(), UpdateTarget::kEpcsFactory);
  EXPECT_EQ(fixture.cable_opens, 1);
  EXPECT_GT(gateware.play.statements, 0u);
  EXPECT_GT(fixture.cable.clocks(), 0u);
  EXPECT_TRUE(gateware.configured);
}

// The two halves of the FPGA step fail differently, and a page that could not
// tell them apart would send the user to check a cable that is working.
TEST(ProvisioningOrchestrator, AFlashFailureIsNotACableFailure) {
  Fixture fixture;
  fixture.updater.RefuseTarget(UpdateTarget::kEpcsFactory);

  ProvisioningOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  ASSERT_TRUE(orchestrator.InstallFirmware(fixture.bundle).succeeded);

  const ProvisioningGatewareOutcome outcome =
      orchestrator.ProgramGateware(fixture.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_TRUE(outcome.configured) << "the cable was blamed for a flash failure";
  EXPECT_FALSE(outcome.problem.empty());
}

// A set that could configure the FPGA but not write its flash is refused
// before the cable is opened. Such a set would leave a board looking
// provisioned until the next power cycle, which is the most confusing state
// this flow could stop in.
TEST(ProvisioningOrchestrator, RefusesASetThatCarriesNoFactoryImage) {
  Fixture fixture;
  fixture.bundle.manifest.factory_gateware.reset();
  fixture.bundle.factory_gateware = {};

  ProvisioningOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  ASSERT_TRUE(orchestrator.InstallFirmware(fixture.bundle).succeeded);

  const ProvisioningGatewareOutcome outcome =
      orchestrator.ProgramGateware(fixture.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_NE(outcome.problem.find("factory image"), std::string::npos)
      << outcome.problem;
  EXPECT_EQ(fixture.cable_opens, 0);
}

// --- the FX3 half ---------------------------------------------------------

// The jumper is still fitted, so the device must not be reset: it would come
// back in its boot ROM rather than in the firmware just written. The wizard
// owns the power cycle and the check afterwards.
TEST(ProvisioningOrchestrator, TheFx3StepDefersTheRestart) {
  Fixture fixture;
  ProvisioningOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  const UpdateOutcome outcome = orchestrator.InstallFirmware(fixture.bundle);

  ASSERT_TRUE(outcome.succeeded) << outcome.problem;
  EXPECT_EQ(fixture.updater.reset_count(), 0u);
  EXPECT_FALSE(outcome.identity_confirmed);
}

TEST(ProvisioningOrchestrator, RefusesASetThatCarriesNoFirmware) {
  Fixture fixture;
  fixture.bundle.manifest.firmware.reset();
  fixture.bundle.firmware = {};

  ProvisioningOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  const UpdateOutcome outcome = orchestrator.InstallFirmware(fixture.bundle);
  EXPECT_FALSE(outcome.succeeded);
  EXPECT_NE(outcome.problem.find("no firmware"), std::string::npos)
      << outcome.problem;
}

// --- the FPGA half --------------------------------------------------------

TEST(ProvisioningOrchestrator, RefusesASetThatCarriesNoVectors) {
  Fixture fixture;
  fixture.bundle.manifest.provisioning.reset();
  fixture.bundle.provisioning = {};

  ProvisioningOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  ASSERT_TRUE(orchestrator.InstallFirmware(fixture.bundle).succeeded);

  const ProvisioningGatewareOutcome outcome =
      orchestrator.ProgramGateware(fixture.bundle);
  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(fixture.cable_opens, 0);
}

// The cable's own sentence, carried through rather than replaced. It is the
// one that says which of "not attached", "attached but not permitted" and
// "that is a USB-Blaster II" happened.
TEST(ProvisioningOrchestrator, SaysWhyTheCableCouldNotBeOpened) {
  Fixture fixture;
  fixture.cable_available = false;

  ProvisioningOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);
  ASSERT_TRUE(orchestrator.InstallFirmware(fixture.bundle).succeeded);

  const ProvisioningGatewareOutcome outcome =
      orchestrator.ProgramGateware(fixture.bundle);
  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.problem, "No USB-Blaster is attached.");
}

TEST(ProvisioningOrchestrator, AStoppedPlayIsNotAFailure) {
  Fixture fixture(kQuartusOpeningSvf);
  fixture.cable.AnswerWith(QuartusOpeningAnswers());

  // Asked for only once the FX3 half is done, which is what the wizard's Stop
  // button does: the FPGA half is the one that runs for minutes.
  bool stop = false;
  ProvisioningOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);
  orchestrator.SetCancelCallback([&stop] { return stop; });

  ASSERT_TRUE(orchestrator.InstallFirmware(fixture.bundle).succeeded);
  stop = true;

  const ProvisioningGatewareOutcome outcome =
      orchestrator.ProgramGateware(fixture.bundle);
  EXPECT_FALSE(outcome.succeeded);
  EXPECT_TRUE(outcome.stopped);
}

// The bar the wizard shows is fed in the same shape the update page already
// consumes, so there is one description of progress in the application rather
// than two that have to be kept in step.
TEST(ProvisioningOrchestrator, ReportsProgressAsTheUpdateFlowDoes) {
  Fixture fixture(kQuartusOpeningSvf);
  fixture.cable.AnswerWith(QuartusOpeningAnswers());

  std::vector<UpdateProgress> reports;
  ProvisioningOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);
  orchestrator.SetProgressCallback(
      [&reports](const UpdateProgress& report) { reports.push_back(report); });

  ASSERT_TRUE(orchestrator.InstallFirmware(fixture.bundle).succeeded);
  reports.clear();
  ASSERT_TRUE(orchestrator.ProgramGateware(fixture.bundle).succeeded);

  ASSERT_FALSE(reports.empty());

  // Both halves report against the factory target, because both are that one
  // thing: the vectors that make the flash reachable and the write that
  // reaches it.
  EXPECT_EQ(reports.front().target, UpdateTarget::kEpcsFactory);
  EXPECT_EQ(reports.back().target, UpdateTarget::kEpcsFactory);
  EXPECT_FALSE(reports.back().message.empty());

  // The configuration counts bytes of the file it is playing, and it happens
  // first.
  const auto configuring = std::find_if(
      reports.begin(), reports.end(), [&fixture](const UpdateProgress& report) {
        return report.total == fixture.vectors.size();
      });
  EXPECT_NE(configuring, reports.end())
      << "no report counted the vectors being played";

  // And the write counts bytes of the image, which is a different number.
  const auto writing = std::find_if(
      reports.begin(), reports.end(), [&fixture](const UpdateProgress& report) {
        return report.total == fixture.factory_image.size();
      });
  EXPECT_NE(writing, reports.end())
      << "no report counted the factory image being written";
}

// --- the estimate ---------------------------------------------------------

TEST(ProvisioningEstimate, GrowsWithTheFileAndIsSecondsForARealOne) {
  EXPECT_LT(EstimateProvisioningSeconds(1000),
            EstimateProvisioningSeconds(1000000));

  // This project's own configuration file is 1,450,426 bytes and took 2.6
  // seconds on the bench (B-V1). The estimate must be of that order and never
  // shorter than the measurement — an estimate a user beats is one they stop
  // believing.
  const int seconds = EstimateProvisioningSeconds(1450426);
  EXPECT_GE(seconds, 3);
  EXPECT_LT(seconds, 30);

  // It used to be minutes, and deliberately: what was played then was an
  // 18.4 MB flash-writing file whose idle clocks alone stood for 105 seconds.
  // That file cannot be played at all outside Quartus, which is why nothing
  // plays it any more.
}

}  // namespace
}  // namespace ddd::capture
