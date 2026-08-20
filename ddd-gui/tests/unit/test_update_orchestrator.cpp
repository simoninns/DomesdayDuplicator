/************************************************************************

    test_update_orchestrator.cpp

    T1 unit test for the update flow, end to end, with no device
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include "fake_device_updater.h"
#include "logger.h"
#include "update_orchestrator.h"

namespace ddd::capture {
namespace {

// Timings that make a whole update take microseconds rather than minutes.
// The flow is identical; only the waiting is not real.
UpdateTimings FastTimings() {
  UpdateTimings timings;
  timings.poll_interval = std::chrono::milliseconds(0);
  timings.return_timeout = std::chrono::milliseconds(10);
  timings.stall_timeout = std::chrono::milliseconds(1000);
  return timings;
}

// A payload that is not a round number of chunks, so the last chunk is a
// short one — which is the case the alignment rule is about.
std::vector<uint8_t> MakePayload(size_t bytes) {
  std::vector<uint8_t> payload(bytes);
  std::iota(payload.begin(), payload.end(), uint8_t{0});
  return payload;
}

// A bundle carrying firmware only, which is what the development loop and
// this phase produce.
struct TestBundle {
  std::vector<uint8_t> firmware_payload;
  UpdateBundle bundle;

  explicit TestBundle(size_t bytes = 4100)
      : firmware_payload(MakePayload(bytes)) {
    bundle.manifest.manifest_version = kUpdateManifestVersion;
    bundle.manifest.channel = UpdateChannel::kDevelopment;
    bundle.manifest.version = "0.0.0";
    bundle.manifest.commit = "0123abcd";

    UpdateComponent firmware;
    firmware.file = "firmware.img";
    firmware.length = firmware_payload.size();
    firmware.sha256 = Sha256(std::span<const uint8_t>(firmware_payload));
    firmware.identity = "0123abcd";
    firmware.interface_version = 1;
    bundle.manifest.firmware = firmware;

    bundle.firmware = firmware_payload;
  }
};

UpdateOutcome RunUpdate(FakeDeviceUpdater& device, const UpdateBundle& bundle,
                        std::vector<UpdateProgress>* progress = nullptr) {
  UpdateOrchestrator orchestrator(device, nullptr);
  orchestrator.SetTimings(FastTimings());
  if (progress != nullptr) {
    orchestrator.SetProgressCallback(
        [progress](const UpdateProgress& step) { progress->push_back(step); });
  }
  return orchestrator.Run(bundle);
}

TEST(UpdateOrchestrator, InstallsFirmwareAndProvesItAfterwards) {
  const TestBundle test;
  FakeDeviceUpdater device;

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_TRUE(outcome.succeeded);
  EXPECT_EQ(outcome.stage, UpdateStage::kComplete);
  EXPECT_TRUE(outcome.problem.empty());

  // Integrity link 8: proved by reading the device rather than by assuming
  // the write worked.
  EXPECT_TRUE(outcome.identity_confirmed);
  EXPECT_EQ(outcome.identity.product_string, "Domesday Duplicator (0123abcd)");

  // And the bytes that arrived are the bytes that were sent.
  EXPECT_EQ(device.received(), test.firmware_payload);
  EXPECT_EQ(device.begin_count(), 1u);
  EXPECT_EQ(device.reset_count(), 1u);
}

// --- the deferred restart ------------------------------------------------
//
// What the bring-up wizard's FX3 step runs. The jumper that put the device in
// its boot ROM is still fitted, so a reset would land it back there and the
// confirmation would compare the firmware just written against a device that
// is not running it.

TEST(UpdateOrchestrator, ADeferredRestartWritesAndVerifiesButDoesNotReset) {
  const TestBundle test;
  FakeDeviceUpdater device;

  UpdateOrchestrator orchestrator(device, nullptr);
  orchestrator.SetTimings(FastTimings());
  orchestrator.SetDeferRestart(true);

  const UpdateOutcome outcome = orchestrator.Run(test.bundle);

  // The write happened and the device checked it, which is the commit.
  EXPECT_TRUE(outcome.succeeded);
  EXPECT_EQ(outcome.stage, UpdateStage::kComplete);
  EXPECT_EQ(device.received(), test.firmware_payload);

  // And nothing was restarted or claimed.
  EXPECT_EQ(device.reset_count(), 0u);
  EXPECT_FALSE(outcome.identity_confirmed);
  EXPECT_TRUE(outcome.identity.product_string.empty());
}

// The FPGA reload goes with the restart, and for the same reason: the power
// cycle the caller owns reloads the gateware from flash anyway.
TEST(UpdateOrchestrator, ADeferredRestartDoesNotReloadTheGatewareEither) {
  TestBundle test;
  std::vector<uint8_t> gateware_payload = MakePayload(2048);

  UpdateComponent gateware;
  gateware.file = "gateware-app.rpd";
  gateware.length = gateware_payload.size();
  gateware.sha256 = Sha256(std::span<const uint8_t>(gateware_payload));
  gateware.identity = "0123abcd";
  gateware.interface_version = 2;
  test.bundle.manifest.gateware = gateware;
  test.bundle.gateware = gateware_payload;

  FakeDeviceUpdater device;
  UpdateOrchestrator orchestrator(device, nullptr);
  orchestrator.SetTimings(FastTimings());
  orchestrator.SetDeferRestart(true);

  EXPECT_TRUE(orchestrator.Run(test.bundle).succeeded);
  EXPECT_EQ(device.reconfigure_count(), 0u);
  EXPECT_EQ(device.reset_count(), 0u);
}

// Inert unless it is set, which is the property that keeps the ordinary
// update path out of this plan's way.
TEST(UpdateOrchestrator, TheOrdinaryPathStillRestartsAndConfirms) {
  const TestBundle test;
  FakeDeviceUpdater device;

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_EQ(device.reset_count(), 1u);
  EXPECT_TRUE(outcome.identity_confirmed);
}

// Every chunk but the last is a whole number of EEPROM pages, so the
// firmware can write a chunk straight to the medium with no assembly buffer.
// The fake enforces it, so a host that broke the rule fails here rather than
// on a bench.
TEST(UpdateOrchestrator, EveryChunkButTheLastIsPageAligned) {
  const TestBundle test(4100);
  FakeDeviceUpdater device;

  EXPECT_TRUE(RunUpdate(device, test.bundle).succeeded);
  EXPECT_EQ(device.chunk_count(), 3u);
}

// The chunk size comes from the device, so a firmware that advertises a
// different one needs no change on this side.
TEST(UpdateOrchestrator, TakesTheChunkSizeFromTheDevice) {
  const TestBundle test(4100);
  FakeDeviceUpdater device;
  device.SetMaximumChunkBytes(512);

  EXPECT_TRUE(RunUpdate(device, test.bundle).succeeded);
  EXPECT_EQ(device.largest_chunk(), 512u);
  EXPECT_EQ(device.chunk_count(), 9u);
}

// A device whose advertised maximum is not a whole number of pages still has
// to be sent whole pages, because that is the firmware's rule and not the
// advertisement's — and the page it is rounded to is the larger of the two
// media's, so the same chunk suits either target.
TEST(UpdateOrchestrator, RoundsAnAwkwardChunkSizeDownToWholePages) {
  const TestBundle test(4100);
  FakeDeviceUpdater device;
  device.SetMaximumChunkBytes(1000);

  EXPECT_TRUE(RunUpdate(device, test.bundle).succeeded);
  EXPECT_EQ(device.largest_chunk(), 768u);
}

TEST(UpdateOrchestrator, ReportsEveryStageInOrder) {
  const TestBundle test;
  FakeDeviceUpdater device;
  std::vector<UpdateProgress> progress;

  EXPECT_TRUE(RunUpdate(device, test.bundle, &progress).succeeded);

  std::vector<UpdateStage> stages;
  for (const UpdateProgress& step : progress) {
    if (stages.empty() || stages.back() != step.stage) {
      stages.push_back(step.stage);
    }
  }

  const std::vector<UpdateStage> expected = {
      UpdateStage::kChecking,   UpdateStage::kTransferring,
      UpdateStage::kWriting,    UpdateStage::kVerifying,
      UpdateStage::kRestarting, UpdateStage::kConfirming,
      UpdateStage::kComplete};
  EXPECT_EQ(stages, expected);
}

// Progress is honest: the transfer bar is fed from real byte counts and
// reaches its total, rather than being a spinner over a silence.
TEST(UpdateOrchestrator, TransferProgressReachesItsTotal) {
  const TestBundle test;
  FakeDeviceUpdater device;
  std::vector<UpdateProgress> progress;

  EXPECT_TRUE(RunUpdate(device, test.bundle, &progress).succeeded);

  uint64_t furthest = 0;
  uint64_t total = 0;
  for (const UpdateProgress& step : progress) {
    if (step.stage == UpdateStage::kTransferring) {
      EXPECT_GE(step.done, furthest) << "transfer progress went backwards";
      furthest = step.done;
      total = step.total;
    }
  }

  EXPECT_EQ(total, test.firmware_payload.size());
  EXPECT_EQ(furthest, total);
}

TEST(UpdateOrchestrator, RefusesABundleWithNothingInIt) {
  UpdateBundle empty;
  empty.manifest.manifest_version = kUpdateManifestVersion;

  FakeDeviceUpdater device;
  const UpdateOutcome outcome = RunUpdate(device, empty);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.stage, UpdateStage::kFailed);
  EXPECT_EQ(device.begin_count(), 0u);
}

// --- The failure branches, which is what the fake exists for ---------------

TEST(UpdateOrchestrator, ReportsFirmwareWithNoUpdateAgentInIt) {
  const TestBundle test;
  FakeDeviceUpdater device;
  device.SetFault(FakeDeviceUpdater::Fault::kNoStatus);

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(device.begin_count(), 0u);
  EXPECT_NE(outcome.problem.find("bench procedure"), std::string::npos);
}

TEST(UpdateOrchestrator, ReportsADeviceThatIsCapturing) {
  const TestBundle test;
  FakeDeviceUpdater device;
  device.SetFault(FakeDeviceUpdater::Fault::kRefuseBegin);
  device.SetFailureError(DeviceUpdateError::kBusy);

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.problem, DeviceUpdateErrorText(DeviceUpdateError::kBusy));
  EXPECT_EQ(device.chunk_count(), 0u);
}

TEST(UpdateOrchestrator, ReportsAFileThatIsNotFirmwareForThisDevice) {
  const TestBundle test;
  FakeDeviceUpdater device;
  device.SetFault(FakeDeviceUpdater::Fault::kRefuseChunk);
  device.SetFailAtChunk(0);
  device.SetFailureError(DeviceUpdateError::kImage);

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.problem, DeviceUpdateErrorText(DeviceUpdateError::kImage));
}

// Integrity link 5, modelled: what arrived is hashed against what
// UPDATE_BEGIN promised, and a mismatch stops the update before anything is
// committed.
TEST(UpdateOrchestrator, CatchesAPayloadThatDoesNotMatchItsDigest) {
  TestBundle test;
  FakeDeviceUpdater device;

  // The manifest says one thing and the payload is another, which is what a
  // corrupted transfer looks like from the device's side.
  UpdateComponent firmware =
      test.bundle.manifest.firmware.value_or(UpdateComponent{});
  firmware.sha256[0] ^= 0xFF;
  test.bundle.manifest.firmware = firmware;

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.problem,
            DeviceUpdateErrorText(DeviceUpdateError::kStreamDigest));
  EXPECT_EQ(device.reset_count(), 0u);
}

// Integrity link 6: the device could not confirm what it had written, so the
// commit record was never written and the device falls back to a rescue
// state the application recognises.
TEST(UpdateOrchestrator, ReportsAReadbackThatDoesNotMatch) {
  const TestBundle test;
  FakeDeviceUpdater device;
  device.SetFault(FakeDeviceUpdater::Fault::kFailDuringWrite);
  device.SetFailureError(DeviceUpdateError::kMediumDigest);

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_NE(outcome.problem.find("recovery mode"), std::string::npos);
}

// A write that stopped part way says where, and that sentence is the whole
// diagnosis often enough to be worth carrying: an error at the first page is a
// device that cannot write at all, and the same error code after 131,072 bytes
// is a medium that ends there. Found on the bench, where a firmware image
// larger than any release had ever been reported only as "could not write to
// its own memory".
TEST(UpdateOrchestrator, ReportsHowFarAFailedWriteGot) {
  const TestBundle test;
  FakeDeviceUpdater device;
  device.FailDuringWriteAfter(2048);
  device.SetFailureError(DeviceUpdateError::kWrite);

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_NE(outcome.problem.find("2048"), std::string::npos) << outcome.problem;
  EXPECT_NE(outcome.problem.find("stopped after writing"), std::string::npos)
      << outcome.problem;

  // The error the device gave is still the first thing said; the offset is an
  // addition to it and not a replacement.
  EXPECT_EQ(
      outcome.problem.find(DeviceUpdateErrorText(DeviceUpdateError::kWrite)),
      0u);
}

// And a device that wrote nothing is not given a sentence about the nothing it
// wrote.
TEST(UpdateOrchestrator, AFailureBeforeAnythingWasWrittenSaysNoOffset) {
  const TestBundle test;
  FakeDeviceUpdater device;
  device.SetFault(FakeDeviceUpdater::Fault::kFailDuringWrite);
  device.SetFailureError(DeviceUpdateError::kWrite);

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.problem, DeviceUpdateErrorText(DeviceUpdateError::kWrite));
}

TEST(UpdateOrchestrator, ReportsADeviceThatStopsAnsweringMidWrite) {
  const TestBundle test;
  FakeDeviceUpdater device;
  device.SetFault(FakeDeviceUpdater::Fault::kVanishDuringWrite);

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_NE(outcome.problem.find("Leave it"), std::string::npos);
}

TEST(UpdateOrchestrator, ReportsADeviceThatNeverComesBack) {
  const TestBundle test;
  FakeDeviceUpdater device;
  device.SetFault(FakeDeviceUpdater::Fault::kNeverReturns);

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.stage, UpdateStage::kFailed);
  EXPECT_EQ(device.reset_count(), 1u);
  EXPECT_NE(outcome.problem.find("did not reappear"), std::string::npos);
}

// The update ran, the device came back, and it is running something else.
// This is the one failure that only the post-update identity check can find,
// and it is why the check exists.
TEST(UpdateOrchestrator, RefusesToCallItDoneWhenTheWrongBuildCameBack) {
  const TestBundle test;
  FakeDeviceUpdater device;
  device.SetFault(FakeDeviceUpdater::Fault::kWrongIdentityAfterUpdate);

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_FALSE(outcome.identity_confirmed);
  EXPECT_NE(outcome.problem.find("deadbeef"), std::string::npos);

  // The device is still reported, because "what is it running now" is the
  // first thing anyone wants to know after an update that went wrong.
  EXPECT_FALSE(outcome.identity.product_string.empty());
}

TEST(UpdateOrchestrator, StopsWhenAskedTo) {
  const TestBundle test(65536);
  FakeDeviceUpdater device;

  UpdateOrchestrator orchestrator(device, nullptr);
  orchestrator.SetTimings(FastTimings());

  int polls = 0;
  orchestrator.SetCancelCallback([&polls] { return ++polls > 3; });

  const UpdateOutcome outcome = orchestrator.Run(test.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(device.reset_count(), 0u);

  // Cancelling is safe: the device is never left half-working, and the
  // message says so rather than leaving a user to wonder.
  EXPECT_NE(outcome.problem.find("still has the firmware it started with"),
            std::string::npos);
}

// --- The gateware target ---------------------------------------------------

// A bundle carrying both halves, which is what a release bundle is.
struct BothTargetsBundle {
  std::vector<uint8_t> firmware_payload;
  std::vector<uint8_t> gateware_payload;
  UpdateBundle bundle;

  BothTargetsBundle()
      : firmware_payload(MakePayload(4100)),
        // Not a round number of chunks and not a round number of flash
        // pages, so both the last-chunk case and the last-page case are
        // exercised by an ordinary run.
        gateware_payload(MakePayload(9000)) {
    bundle.manifest.manifest_version = kUpdateManifestVersion;
    bundle.manifest.channel = UpdateChannel::kDevelopment;
    bundle.manifest.version = "0.0.0";
    bundle.manifest.commit = "0123abcd";

    UpdateComponent firmware;
    firmware.file = "firmware.img";
    firmware.length = firmware_payload.size();
    firmware.sha256 = Sha256(std::span<const uint8_t>(firmware_payload));
    firmware.identity = "0123abcd";
    firmware.interface_version = 1;
    bundle.manifest.firmware = firmware;
    bundle.firmware = firmware_payload;

    UpdateComponent gateware;
    gateware.file = "gateware-app.rpd";
    gateware.length = gateware_payload.size();
    gateware.sha256 = Sha256(std::span<const uint8_t>(gateware_payload));
    gateware.identity = "0123abcd";
    gateware.interface_version = 2;
    bundle.manifest.gateware = gateware;
    bundle.gateware = gateware_payload;
  }
};

// Both halves, in order, each proved by its own digest, and the FPGA told to
// reload itself before the device is reset.
TEST(UpdateOrchestrator, InstallsBothHalvesAndReloadsTheFpga) {
  const BothTargetsBundle test;
  FakeDeviceUpdater device;

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_TRUE(outcome.succeeded);
  EXPECT_TRUE(outcome.identity_confirmed);

  // Two transfers, and the bytes of each reached the half they were for.
  EXPECT_EQ(device.begin_count(), 2u);
  EXPECT_EQ(device.received(UpdateTarget::kFirmware), test.firmware_payload);
  EXPECT_EQ(device.received(UpdateTarget::kGateware), test.gateware_payload);

  // Reconfiguration stops the clock underneath the capture path, so it is
  // always followed by the reset rather than left to the next power cycle.
  EXPECT_EQ(device.reconfigure_count(), 1u);
  EXPECT_EQ(device.reset_count(), 1u);
}

// A firmware-only bundle must not ask the FPGA to reload itself. Doing so
// would interrupt a perfectly good gateware for no reason, and on a unit
// whose boot block is invalid it would drop a working session into recovery.
TEST(UpdateOrchestrator, DoesNotReloadTheFpgaWhenNoGatewareWasInstalled) {
  const TestBundle test;
  FakeDeviceUpdater device;

  EXPECT_TRUE(RunUpdate(device, test.bundle).succeeded);
  EXPECT_EQ(device.reconfigure_count(), 0u);
}

// The stage sequence a user watching a two-part update sees: the transfer,
// write and verify stages happen once per component, and each one says which
// component it is about.
TEST(UpdateOrchestrator, ReportsEachComponentsStagesAgainstItsOwnTarget) {
  const BothTargetsBundle test;
  FakeDeviceUpdater device;
  std::vector<UpdateProgress> progress;

  EXPECT_TRUE(RunUpdate(device, test.bundle, &progress).succeeded);

  int firmware_transfers = 0;
  int gateware_transfers = 0;
  int gateware_verifies = 0;

  for (const UpdateProgress& step : progress) {
    if (step.stage == UpdateStage::kTransferring) {
      if (step.target == UpdateTarget::kFirmware) {
        ++firmware_transfers;
      } else {
        ++gateware_transfers;
      }
    }
    if (step.stage == UpdateStage::kVerifying &&
        step.target == UpdateTarget::kGateware) {
      ++gateware_verifies;
    }
  }

  EXPECT_GT(firmware_transfers, 0);
  EXPECT_GT(gateware_transfers, 0);
  EXPECT_GT(gateware_verifies, 0);
}

// The gateware's transfer message warns about the erase pauses, because a
// bar that stops for a second every thirty chunks with no explanation is a
// bar a user starts to distrust.
TEST(UpdateOrchestrator, SaysWhyTheGatewareTransferPauses) {
  const BothTargetsBundle test;
  FakeDeviceUpdater device;
  std::vector<UpdateProgress> progress;

  EXPECT_TRUE(RunUpdate(device, test.bundle, &progress).succeeded);

  bool mentions_erasing = false;
  for (const UpdateProgress& step : progress) {
    if (step.stage == UpdateStage::kTransferring &&
        step.target == UpdateTarget::kGateware &&
        step.message.find("erase") != std::string::npos) {
      mentions_erasing = true;
    }
  }

  EXPECT_TRUE(mentions_erasing);
}

// Every chunk but the last is a whole number of the target medium's pages,
// and the EPCS's page is four times the EEPROM's. The fake enforces both, so
// a host that sent an EEPROM-aligned chunk to the flash fails here.
TEST(UpdateOrchestrator, GatewareChunksAreAlignedToTheFlashPage) {
  const BothTargetsBundle test;
  FakeDeviceUpdater device;

  // Awkward on purpose: rounded down to whole flash pages this is 768, and
  // rounded down to EEPROM pages alone it would be 960.
  device.SetMaximumChunkBytes(1000);

  EXPECT_TRUE(RunUpdate(device, test.bundle).succeeded);
}

// The device is asked to reload its gateware and refuses. That leaves a unit
// whose flash is written and whose FPGA is still running the old image,
// which is a real state and one the user is told how to leave.
TEST(UpdateOrchestrator, ReportsAnFpgaThatWillNotReload) {
  const BothTargetsBundle test;
  FakeDeviceUpdater device;
  device.SetReconfigureSucceeds(false);

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_EQ(outcome.stage, UpdateStage::kFailed);
  EXPECT_NE(outcome.problem.find("plug it back in"), std::string::npos)
      << outcome.problem;
}

// A device that comes back running gateware the manifest did not describe is
// not a successful update, however well the writing went.
TEST(UpdateOrchestrator, RefusesToCallItDoneWhenTheWrongGatewareCameBack) {
  const BothTargetsBundle test;
  FakeDeviceUpdater device;
  device.SetFault(FakeDeviceUpdater::Fault::kWrongIdentityAfterUpdate);

  const UpdateOutcome outcome = RunUpdate(device, test.bundle);

  EXPECT_FALSE(outcome.succeeded);
  EXPECT_FALSE(outcome.identity_confirmed);
}

// --- The estimate the interface shows before anything moves ---------------

TEST(UpdateEstimate, GrowsWithThePayloadAndIsNeverZero) {
  UpdateManifest manifest;

  UpdateComponent small;
  small.length = 1024;
  manifest.firmware = small;
  const int small_seconds = EstimateUpdateSeconds(manifest);

  UpdateComponent large;
  large.length = uint64_t{1024} * 1024;
  manifest.firmware = large;
  const int large_seconds = EstimateUpdateSeconds(manifest);

  EXPECT_GT(small_seconds, 0);
  EXPECT_GT(large_seconds, small_seconds);
}

// The gateware is the slower medium by a wide margin — every byte crosses a
// bit-banged link four links deep, and it is read back as well as written —
// so an estimate that treated the two alike would be wrong in the direction
// that makes users unplug the device.
TEST(UpdateEstimate, TheGatewareIsEstimatedAsTheSlowerMedium) {
  UpdateComponent component;
  component.length = 350000;

  UpdateManifest firmware_only;
  firmware_only.firmware = component;

  UpdateManifest gateware_only;
  gateware_only.gateware = component;

  EXPECT_GT(EstimateUpdateSeconds(gateware_only),
            EstimateUpdateSeconds(firmware_only));

  // And a real gateware image is a minutes-long operation, which is what the
  // interface has to say before the first byte moves.
  EXPECT_GT(EstimateUpdateSeconds(gateware_only), 60);
}

// --- What the log says about an update ------------------------------------
//
// The engine's own account, which is what an investigation has when the
// window that showed the progress bar has been closed. Asserted by fragment
// rather than by whole line: the wording is meant to be edited, and what must
// not change is that each fact is recorded at all.

class RecordingLog {
 public:
  CallbackLogger::Sink Sink() {
    return [this](LogLevel /*level*/, const std::string& message) {
      lines_.push_back(message);
    };
  }

  bool Contains(std::string_view fragment) const {
    for (const std::string& line : lines_) {
      if (line.find(fragment) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

 private:
  std::vector<std::string> lines_;
};

TEST(UpdateOrchestratorLog, SaysWhatItWasGivenAndWhatItDid) {
  const TestBundle test;
  FakeDeviceUpdater device;

  RecordingLog log;
  CallbackLogger logger(log.Sink(), LogLevel::kDebug);

  UpdateOrchestrator orchestrator(device, &logger);
  orchestrator.SetTimings(FastTimings());
  ASSERT_TRUE(orchestrator.Run(test.bundle).succeeded);

  // What was in the file, before anything was written. A payload that is not
  // installed is still worth naming: what a bundle contains and what a run
  // installs are different questions.
  EXPECT_TRUE(log.Contains("Update starting: bundle version"));
  EXPECT_TRUE(log.Contains("carries firmware 0123abcd"));

  // What the transfer was actually shaped like. The chunk size is the
  // device's, not this build's, so a firmware that changes it changes every
  // transfer that follows and this is the line that says which was in force.
  EXPECT_TRUE(log.Contains("Installing firmware"));
  EXPECT_TRUE(log.Contains("chunks of"));
  EXPECT_TRUE(log.Contains("the device offered"));
  EXPECT_TRUE(log.Contains("Sent the whole firmware"));
  EXPECT_TRUE(log.Contains("Installed firmware after"));

  // And how it ended, including the proof: an update that was performed and
  // an update that is proved are different things.
  EXPECT_TRUE(log.Contains("Update finished after"));
  EXPECT_TRUE(log.Contains("succeeded at stage"));
  EXPECT_TRUE(log.Contains("identity confirmed"));
}

TEST(UpdateOrchestratorLog, RecordsTheDevicesOwnStatusWhenItFails) {
  const TestBundle test;
  FakeDeviceUpdater device;
  device.SetFault(FakeDeviceUpdater::Fault::kRefuseFinish);
  device.SetFailureError(DeviceUpdateError::kStreamDigest);

  RecordingLog log;
  CallbackLogger logger(log.Sink(), LogLevel::kDebug);

  UpdateOrchestrator orchestrator(device, &logger);
  orchestrator.SetTimings(FastTimings());
  ASSERT_FALSE(orchestrator.Run(test.bundle).succeeded);

  // The stage it stopped at, and the device's own account of itself at that
  // moment — which is the diagnosis, and the thing nobody can go back and ask
  // for afterwards.
  EXPECT_TRUE(log.Contains("Failed to install firmware"));
  EXPECT_TRUE(log.Contains("Device status at the failure"));
  EXPECT_TRUE(log.Contains("phase "));
  EXPECT_TRUE(log.Contains("Update finished after"));
  EXPECT_TRUE(log.Contains("failed at stage"));
  EXPECT_TRUE(log.Contains("not confirmed"));
}

TEST(UpdateOrchestratorLog, NamesTheFlowSoThreeOfThemAreToldApart) {
  // One orchestrator runs three different flows, and a log that called them
  // all "update" would leave a bring-up indistinguishable from the ordinary
  // path it deliberately is not.
  const TestBundle test;
  FakeDeviceUpdater device;

  RecordingLog log;
  CallbackLogger logger(log.Sink(), LogLevel::kDebug);

  UpdateOrchestrator orchestrator(device, &logger);
  orchestrator.SetTimings(FastTimings());
  orchestrator.InstallFactoryGateware(test.bundle);

  EXPECT_TRUE(log.Contains("Factory image write starting"));
  EXPECT_TRUE(log.Contains("Factory image write finished"));
  EXPECT_FALSE(log.Contains("Update starting"));
}

TEST(UpdatePhaseNames, EveryPhaseIsNamed) {
  const UpdatePhase phases[] = {UpdatePhase::kIdle,     UpdatePhase::kReceiving,
                                UpdatePhase::kWriting,  UpdatePhase::kVerifying,
                                UpdatePhase::kComplete, UpdatePhase::kFailed};

  for (const UpdatePhase phase : phases) {
    EXPECT_STRNE(UpdatePhaseName(phase), "unknown");
  }
}

TEST(UpdateStageNames, EveryStageIsNamed) {
  const UpdateStage stages[] = {
      UpdateStage::kChecking,     UpdateStage::kPreparing,
      UpdateStage::kTransferring, UpdateStage::kWriting,
      UpdateStage::kVerifying,    UpdateStage::kRestarting,
      UpdateStage::kConfirming,   UpdateStage::kComplete,
      UpdateStage::kFailed};

  for (UpdateStage stage : stages) {
    EXPECT_STRNE(UpdateStageName(stage), "Unknown");
  }
}

}  // namespace
}  // namespace ddd::capture
