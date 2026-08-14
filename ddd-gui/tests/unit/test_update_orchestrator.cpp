/************************************************************************

    test_update_orchestrator.cpp

    T1 unit test for the update flow, end to end, with no device
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <numeric>
#include <vector>

#include "fake_device_updater.h"
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
// advertisement's.
TEST(UpdateOrchestrator, RoundsAnAwkwardChunkSizeDownToWholePages) {
  const TestBundle test(4100);
  FakeDeviceUpdater device;
  device.SetMaximumChunkBytes(1000);

  EXPECT_TRUE(RunUpdate(device, test.bundle).succeeded);
  EXPECT_EQ(device.largest_chunk(), 960u);
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

TEST(UpdateStageNames, EveryStageIsNamed) {
  const UpdateStage stages[] = {
      UpdateStage::kChecking,   UpdateStage::kTransferring,
      UpdateStage::kWriting,    UpdateStage::kVerifying,
      UpdateStage::kRestarting, UpdateStage::kConfirming,
      UpdateStage::kComplete,   UpdateStage::kFailed};

  for (UpdateStage stage : stages) {
    EXPECT_STRNE(UpdateStageName(stage), "Unknown");
  }
}

}  // namespace
}  // namespace ddd::capture
