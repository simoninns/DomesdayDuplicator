/************************************************************************

    update_orchestrator.h

    Driving an update from a verified bundle to a device that reports the
    new version
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

#include "device_updater.h"
#include "update_bundle.h"

namespace ddd::capture {

class ILogger;

// The stages of an update, in the order they happen.
//
// These are the stages a user sees named on the screen, not internal states:
// each one answers "what is happening", and the progress within it answers
// "how much longer". They are here in the engine rather than in the
// interface because the command-line tool reports the same stages, and two
// descriptions of one process is one description too many.
enum class UpdateStage {
  // Reading the bundle, checking its signature and digests, and deciding
  // whether it may be installed on this device by this build.
  kChecking,

  // Waking a device that has no working firmware, by handing the bundle's
  // own firmware to its boot ROM and waiting for it to come back as a
  // Duplicator. Only the recovery path visits this stage; an ordinary update
  // goes straight from kChecking to kTransferring.
  kPreparing,

  // Streaming a payload to the device.
  kTransferring,

  // The device writing it to its own medium.
  kWriting,

  // The device reading it back and comparing it against the digest.
  kVerifying,

  // The device restarting and re-enumerating.
  kRestarting,

  // Reading the device's identity back and comparing it against what the
  // manifest said it would become.
  kConfirming,

  kComplete,
  kFailed,
};

// A human name for a stage, in the same words the documentation uses.
const char* UpdateStageName(UpdateStage stage);

// Where the update has got to, as reported to whoever is watching.
struct UpdateProgress {
  UpdateStage stage = UpdateStage::kChecking;

  // Which payload this is about. Meaningless outside the three stages that
  // carry one, which is why the message says so and this does not have to.
  UpdateTarget target = UpdateTarget::kFirmware;

  // Bytes done and bytes to do *within this stage*. Zero total means the
  // stage has no meaningful proportion, and the interface then says what it
  // is waiting for instead of guessing at a bar.
  uint64_t done = 0;
  uint64_t total = 0;

  // One line, already written for a user.
  std::string message;
};

using UpdateProgressCallback = std::function<void(const UpdateProgress&)>;

// How the update ended.
struct UpdateOutcome {
  bool succeeded = false;

  // The stage it stopped at. kComplete on success.
  UpdateStage stage = UpdateStage::kChecking;

  // Why it stopped, written for a user. Empty on success.
  std::string problem;

  // What the device reported once it came back. Filled in whenever the
  // device was reachable at the end, including after some failures, because
  // "which firmware is it running now" is the first thing anyone wants to
  // know after an update that went wrong.
  DeviceIdentity identity;

  // Whether the identity the device reports matches what the manifest said
  // it would. This is integrity link 8, and it is the difference between an
  // update that was performed and an update that is proved.
  bool identity_confirmed = false;
};

// Timings the orchestrator works to. Collected in a struct so that a test
// can drive the whole flow in milliseconds rather than minutes, and so that
// the numbers are visible rather than buried at their call sites.
struct UpdateTimings {
  // How long to wait for the device to come back after a reset.
  std::chrono::milliseconds return_timeout{30000};

  // How often to ask the device how the write and the verify are going.
  std::chrono::milliseconds poll_interval{200};

  // How long a write or verify may go with no progress at all before it is
  // called stuck. Generous, because an EEPROM page write is milliseconds and
  // an EPCS sector erase is most of a second.
  std::chrono::milliseconds stall_timeout{60000};
};

// Run an update.
//
// Qt-free and synchronous: it blocks for the whole of an update, which is
// minutes. The interface runs it on a worker thread and the command-line
// tool runs it on the only thread it has, and both get the identical code
// path — which is the point, because a bug found by the command-line tool is
// then a bug in what the interface runs rather than in something that
// resembles it.
//
// `cancel` is polled between chunks and between status reads. Cancelling is
// safe at any point up to the commit, and the commit is the last write of
// each target: the device is left in a state that either still holds the old
// image or falls back to a rescue mode the application recognises. It is
// never left half-working.
class UpdateOrchestrator {
 public:
  UpdateOrchestrator(IDeviceUpdater& device, ILogger* logger);

  UpdateOrchestrator(const UpdateOrchestrator&) = delete;
  UpdateOrchestrator& operator=(const UpdateOrchestrator&) = delete;
  UpdateOrchestrator(UpdateOrchestrator&&) = delete;
  UpdateOrchestrator& operator=(UpdateOrchestrator&&) = delete;

  void SetTimings(const UpdateTimings& timings) { timings_ = timings; }

  void SetProgressCallback(UpdateProgressCallback callback) {
    progress_ = std::move(callback);
  }

  // Polled repeatedly; return true to stop.
  void SetCancelCallback(std::function<bool()> cancel) {
    cancel_ = std::move(cancel);
  }

  // Install every component the bundle carries that this firmware can take,
  // then reset the device and confirm what came back.
  //
  // The gate is the caller's to run first — it needs the device identity and
  // the application's version, and it decides whether there is anything to
  // offer at all. Run() assumes it passed.
  UpdateOutcome Run(const UpdateBundle& bundle);

 private:
  bool InstallComponent(UpdateTarget target, const UpdateComponent& component,
                        std::span<const uint8_t> payload,
                        UpdateOutcome& outcome);

  // Poll the device until it leaves the phase it is in, feeding progress out
  // as it goes. Returns false and fills in the outcome on a failure, a
  // stall, or a cancellation.
  bool AwaitCompletion(UpdateTarget target, uint64_t total,
                       UpdateOutcome& outcome);

  void Report(UpdateStage stage, UpdateTarget target, uint64_t done,
              uint64_t total, std::string message);

  bool Cancelled() const { return cancel_ && cancel_(); }

  IDeviceUpdater& device_;
  ILogger* logger_ = nullptr;
  UpdateTimings timings_;
  UpdateProgressCallback progress_;
  std::function<bool()> cancel_;
};

// A rough estimate, in seconds, of how long installing this bundle will
// take. Shown before the first byte moves, because "about four minutes" is
// the difference between a user who waits and a user who unplugs it.
//
// Deliberately pessimistic and deliberately coarse. It is derived from the
// medium's own write rate rather than measured, so it is an honest upper
// bound rather than a prediction that will be wrong in the interesting
// direction.
int EstimateUpdateSeconds(const UpdateManifest& manifest);

}  // namespace ddd::capture
