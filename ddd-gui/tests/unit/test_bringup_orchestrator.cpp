/************************************************************************

    test_bringup_orchestrator.cpp

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
#include "bringup_orchestrator.h"
#include "digest.h"
#include "fake_device_programmer.h"
#include "fake_device_updater.h"
#include "fake_jtag_cable.h"
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

// Two statements and one comparison, for the tests about playing the file
// more than once. Small deliberately: what they are about is how many times
// it is played, and a file with a real wait in it would spend a second of
// each run proving nothing.
inline constexpr char kRetrySvf[] = R"(
STATE IDLE;
SDR 4 TDI (0) TDO (A) MASK (F);
)";

// A complete update bundle built in memory. UpdateBundle is what
// OpenUpdateBundle produces *after* the signature and every digest have passed,
// so building one directly starts where the checks finished — which is where
// the orchestrator starts.
struct Fixture {
  std::vector<uint8_t> firmware = test::MakeBootImage();
  std::vector<uint8_t> vectors;
  std::vector<uint8_t> factory_image;
  std::vector<uint8_t> application_image;
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

    // And the image the board actually captures with. Bring-up finishes the
    // job rather than handing over to an ordinary update, so this is as
    // required as the other three.
    application_image.assign(
        reinterpret_cast<const uint8_t*>(test::kGatewarePayload.data()),
        reinterpret_cast<const uint8_t*>(test::kGatewarePayload.data()) +
            test::kGatewarePayload.size());

    UpdateComponent application;
    application.file = std::string(kGatewareEntryName);
    application.length = application_image.size();
    application.sha256 = Sha256(application_image);
    application.identity = "0123abcd";
    application.interface_version = 2;
    bundle.manifest.gateware = application;
    bundle.gateware = application_image;
  }

  BringUpAccess Access() {
    BringUpAccess access;
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
  void Configure(BringUpOrchestrator& orchestrator) {
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
// gateware drives, and what keeps a board out of that pairing is where the FX3
// is while the FPGA changes: in its boot ROM, with every shared pin idle. So
// the configure comes first, and that is checked here rather than left to the
// order a wizard happens to lay its pages out in.

TEST(BringUpOrchestrator, RefusesToWriteBeforeTheFpgaIsConfigured) {
  Fixture fixture;
  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  const UpdateOutcome outcome = orchestrator.ProgramDevice(fixture.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_NE(outcome.problem.find("configured"), std::string::npos)
      << outcome.problem;

  // And nothing was reached for: the refusal happens before the boot ROM is
  // opened, so a wrongly wired caller does not so much as wake the device.
  EXPECT_EQ(fixture.programmer.sections_written(), 0u);
  EXPECT_EQ(fixture.updater.begin_count(), 0u);
}

TEST(BringUpOrchestrator, RefusesToWriteWhenTheConfigureFailed) {
  Fixture fixture;
  fixture.cable_available = false;

  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  EXPECT_FALSE(orchestrator.ConfigureFpga(fixture.bundle).succeeded);
  EXPECT_FALSE(orchestrator.fpga_configured());
  EXPECT_FALSE(orchestrator.ProgramDevice(fixture.bundle).succeeded);
  EXPECT_EQ(fixture.updater.begin_count(), 0u);
}

TEST(BringUpOrchestrator, ConfiguresThenWritesEverything) {
  Fixture fixture;
  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  const BringUpConfigureOutcome configured =
      orchestrator.ConfigureFpga(fixture.bundle);
  ASSERT_TRUE(configured.succeeded) << configured.problem;
  EXPECT_TRUE(orchestrator.fpga_configured());
  EXPECT_EQ(fixture.cable_opens, 1);
  EXPECT_GT(configured.play.statements, 0u);
  EXPECT_GT(fixture.cable.clocks(), 0u);

  const UpdateOutcome written = orchestrator.ProgramDevice(fixture.bundle);
  ASSERT_TRUE(written.succeeded) << written.problem;

  // Three transfers: the EEPROM, the factory image and the application image.
  EXPECT_EQ(fixture.updater.begin_count(), 3u);

  // And the cable was not reopened for any of them. Everything after the
  // configure goes over the USB 3.0 link, through the firmware's own agent.
  EXPECT_EQ(fixture.cable_opens, 1);
}

// **The order of the three writes**, which is chosen by what a board looks
// like if the power goes out immediately after each one. The EEPROM first, so
// every state from here on boots the new firmware; then the factory image, so
// the board always has something valid to fall back to before the region it
// falls back *from* is touched; the application image last, because it is the
// one whose absence is harmless.
//
// Written the other way round, an interrupted run would leave a bare board
// with a valid application image and no factory image to load it — an FPGA
// that configures from nothing and looks dead.
TEST(BringUpOrchestrator, WritesTheEepromThenTheFactoryThenTheApplication) {
  Fixture fixture;
  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  ASSERT_TRUE(orchestrator.ConfigureFpga(fixture.bundle).succeeded);
  ASSERT_TRUE(orchestrator.ProgramDevice(fixture.bundle).succeeded);

  EXPECT_EQ(fixture.updater.begun_targets(),
            (std::vector<UpdateTarget>{UpdateTarget::kFirmware,
                                       UpdateTarget::kEpcsFactory,
                                       UpdateTarget::kGateware}));
}

// The jumper may still be fitted and the FPGA is running a configuration it is
// about to lose, so nothing is restarted: the wizard's one power cycle is what
// makes all three images the running ones, together or not at all.
TEST(BringUpOrchestrator, DefersTheRestart) {
  Fixture fixture;
  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  ASSERT_TRUE(orchestrator.ConfigureFpga(fixture.bundle).succeeded);
  const UpdateOutcome outcome = orchestrator.ProgramDevice(fixture.bundle);

  ASSERT_TRUE(outcome.succeeded) << outcome.problem;
  EXPECT_EQ(fixture.updater.reset_count(), 0u);
  EXPECT_EQ(fixture.updater.reconfigure_count(), 0u);
  EXPECT_FALSE(outcome.identity_confirmed);
}

// --- what a file has to carry ---------------------------------------------
//
// All four payloads, checked before the first write rather than discovered
// between two of them. A run that stopped part way through this ordering
// because the next payload was missing would stop in exactly the state the
// ordering exists to avoid.

TEST(BringUpOrchestrator, RefusesAFileThatCarriesNoFirmware) {
  Fixture fixture;
  fixture.bundle.manifest.firmware.reset();
  fixture.bundle.firmware = {};

  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);
  ASSERT_TRUE(orchestrator.ConfigureFpga(fixture.bundle).succeeded);

  const UpdateOutcome outcome = orchestrator.ProgramDevice(fixture.bundle);
  EXPECT_FALSE(outcome.succeeded);
  EXPECT_NE(outcome.problem.find("no firmware"), std::string::npos)
      << outcome.problem;
  EXPECT_EQ(fixture.programmer.sections_written(), 0u);
}

TEST(BringUpOrchestrator, RefusesAFileThatCarriesNoFactoryImage) {
  Fixture fixture;
  fixture.bundle.manifest.factory_gateware.reset();
  fixture.bundle.factory_gateware = {};

  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);
  ASSERT_TRUE(orchestrator.ConfigureFpga(fixture.bundle).succeeded);

  const UpdateOutcome outcome = orchestrator.ProgramDevice(fixture.bundle);
  EXPECT_FALSE(outcome.succeeded);
  EXPECT_NE(outcome.problem.find("factory image"), std::string::npos)
      << outcome.problem;
  EXPECT_EQ(fixture.updater.begin_count(), 0u)
      << "the EEPROM was written for a run that could not finish";
}

TEST(BringUpOrchestrator, RefusesAFileThatCarriesNoApplicationGateware) {
  Fixture fixture;
  fixture.bundle.manifest.gateware.reset();
  fixture.bundle.gateware = {};

  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);
  ASSERT_TRUE(orchestrator.ConfigureFpga(fixture.bundle).succeeded);

  const UpdateOutcome outcome = orchestrator.ProgramDevice(fixture.bundle);
  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(fixture.updater.begin_count(), 0u);
}

TEST(BringUpOrchestrator, RefusesAFileThatCarriesNoVectors) {
  Fixture fixture;
  fixture.bundle.manifest.provisioning.reset();
  fixture.bundle.provisioning = {};

  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  const BringUpConfigureOutcome outcome =
      orchestrator.ConfigureFpga(fixture.bundle);
  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(fixture.cable_opens, 0);
}

// --- the configure --------------------------------------------------------

// The cable's own sentence, carried through rather than replaced. It is the
// one that says which of "not attached", "attached but not permitted" and
// "that is a USB-Blaster II" happened.
TEST(BringUpOrchestrator, SaysWhyTheCableCouldNotBeOpened) {
  Fixture fixture;
  fixture.cable_available = false;

  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  const BringUpConfigureOutcome outcome =
      orchestrator.ConfigureFpga(fixture.bundle);
  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.problem, "No USB-Blaster is attached.");
}

// A configure that fails is played again, once.
//
// The whole half writes nothing, so a failed attempt leaves the board as it
// was and the file can simply be played from the beginning — which is what
// the wizard already tells a user to do by hand after a failure like the one
// this exists for (TESTING.md, B-V1, 2026-08-19).
TEST(BringUpOrchestrator, AConfigureThatFailsOnceIsPlayedAgain) {
  Fixture fixture(kRetrySvf);

  // The first answer disagrees with the file and the second does not, and the
  // cable serves them in the order they are asked for — so attempt one fails
  // on the comparison and attempt two gets through it.
  fixture.cable.AnswerWith({true, true, false, true, false, true, false, true});

  std::vector<UpdateProgress> reports;
  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);
  orchestrator.SetProgressCallback(
      [&reports](const UpdateProgress& report) { reports.push_back(report); });

  const BringUpConfigureOutcome outcome =
      orchestrator.ConfigureFpga(fixture.bundle);

  EXPECT_TRUE(outcome.succeeded) << outcome.problem;
  EXPECT_EQ(outcome.attempts, 2);

  // The bar counts bytes of the file, so a second attempt sends it back to
  // nothing. It says why, because a bar that rewinds silently reads as a
  // fault rather than as the step doing what a user would have done by hand.
  const auto again = std::find_if(
      reports.begin(), reports.end(), [](const UpdateProgress& report) {
        return report.message.find("second attempt") != std::string::npos;
      });
  EXPECT_NE(again, reports.end())
      << "nothing on the bar said the file was being played again";
  EXPECT_TRUE(outcome.problem.empty());
  EXPECT_TRUE(orchestrator.fpga_configured());

  // Re-opened rather than reused, which is half of what the second attempt is
  // worth: opening resets the cable and empties its buffers.
  EXPECT_EQ(fixture.cable_opens, 2);
}

// Twice and no more. A board or a cable that is genuinely wrong should be
// reported, not retried until a user gives up on it.
TEST(BringUpOrchestrator, AConfigureThatKeepsFailingIsReported) {
  Fixture fixture(kRetrySvf);
  fixture.cable.AnswerWith({true, true, false, true, true, true, false, true});

  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  const BringUpConfigureOutcome outcome =
      orchestrator.ConfigureFpga(fixture.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.attempts, 2);
  EXPECT_EQ(fixture.cable_opens, 2);
  EXPECT_FALSE(orchestrator.fpga_configured());

  // The player's own sentence, and after it where in the file to look.
  EXPECT_NE(outcome.problem.find("did not answer"), std::string::npos)
      << outcome.problem;
  EXPECT_NE(outcome.problem.find("(Programming file line 3, SDR.)"),
            std::string::npos)
      << outcome.problem;
}

// A cable that is not there is not a transient, and trying again would only
// say the same sentence twice as slowly.
TEST(BringUpOrchestrator, ACableThatCannotBeOpenedIsNotTriedTwice) {
  Fixture fixture;
  fixture.cable_available = false;

  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);

  const BringUpConfigureOutcome outcome =
      orchestrator.ConfigureFpga(fixture.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(fixture.cable_opens, 1);
}

TEST(BringUpOrchestrator, AStoppedPlayIsNotAFailure) {
  Fixture fixture(kQuartusOpeningSvf);
  fixture.cable.AnswerWith(QuartusOpeningAnswers());

  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);
  orchestrator.SetCancelCallback([] { return true; });

  const BringUpConfigureOutcome outcome =
      orchestrator.ConfigureFpga(fixture.bundle);
  EXPECT_FALSE(outcome.succeeded);
  EXPECT_TRUE(outcome.stopped);
  EXPECT_FALSE(orchestrator.fpga_configured());
}

// The bar the wizard shows is fed in the same shape the update page already
// consumes, so there is one description of progress in the application rather
// than two that have to be kept in step.
TEST(BringUpOrchestrator, ReportsProgressAsTheUpdateFlowDoes) {
  Fixture fixture(kQuartusOpeningSvf);
  fixture.cable.AnswerWith(QuartusOpeningAnswers());

  std::vector<UpdateProgress> reports;
  BringUpOrchestrator orchestrator(fixture.Access(), nullptr);
  fixture.Configure(orchestrator);
  orchestrator.SetProgressCallback(
      [&reports](const UpdateProgress& report) { reports.push_back(report); });

  ASSERT_TRUE(orchestrator.ConfigureFpga(fixture.bundle).succeeded);

  ASSERT_FALSE(reports.empty());
  EXPECT_FALSE(reports.back().message.empty());

  // The configuration counts bytes of the file it is playing.
  const auto configuring = std::find_if(
      reports.begin(), reports.end(), [&fixture](const UpdateProgress& report) {
        return report.total == fixture.vectors.size();
      });
  EXPECT_NE(configuring, reports.end())
      << "no report counted the vectors being played";

  reports.clear();
  ASSERT_TRUE(orchestrator.ProgramDevice(fixture.bundle).succeeded);
  ASSERT_FALSE(reports.empty());

  // And the writes count bytes of each image, which are different numbers.
  const auto writing = std::find_if(
      reports.begin(), reports.end(), [&fixture](const UpdateProgress& report) {
        return report.total == fixture.factory_image.size();
      });
  EXPECT_NE(writing, reports.end())
      << "no report counted the factory image being written";
}

// --- the estimate ---------------------------------------------------------

TEST(ConfigureEstimate, GrowsWithTheFileAndIsSecondsForARealOne) {
  EXPECT_LT(EstimateConfigureSeconds(1000), EstimateConfigureSeconds(1000000));

  // This project's own configuration file is 1,450,426 bytes and took 2.6
  // seconds on the bench (B-V1). The estimate must be of that order and never
  // shorter than the measurement — an estimate a user beats is one they stop
  // believing.
  const int seconds = EstimateConfigureSeconds(1450426);
  EXPECT_GE(seconds, 3);
  EXPECT_LT(seconds, 30);

  // It used to be minutes, and deliberately: what was played then was an
  // 18.4 MB flash-writing file whose idle clocks alone stood for 105 seconds.
  // That file cannot be played at all outside Quartus, which is why nothing
  // plays it any more.
}

}  // namespace
}  // namespace ddd::capture
