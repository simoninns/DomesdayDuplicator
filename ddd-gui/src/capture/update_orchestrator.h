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

  // Stop once the device has verified what it wrote, instead of restarting it
  // and reading back what came up.
  //
  // Inert unless set, and set by exactly one caller: the bring-up wizard's FX3
  // step, where the PMODE jumper is fitted. A reset with that jumper fitted
  // lands the FX3 back in its boot ROM rather than in the firmware just
  // written, so the confirmation would compare the new image against a device
  // that is not running it and report a failure for an update that worked.
  //
  // The device is left in a defined state either way — the EEPROM is written
  // and read back before this returns, which is the commit — and what is
  // deferred is only the proof. The caller then owns the power cycle and the
  // check afterwards, and must actually do both: an outcome from a deferred
  // run reports succeeded with identity_confirmed false, and nothing has been
  // read off the device to confirm.
  //
  // Reconfiguring the FPGA is deferred with it, for the same reason: the
  // power cycle that the caller owns reloads the gateware from flash anyway,
  // and asking a device to reload it a moment before it loses power is a
  // reconfiguration nobody watches the result of.
  void SetDeferRestart(bool defer) { defer_restart_ = defer; }

  // Install every component the bundle carries that this firmware can take,
  // then reset the device and confirm what came back.
  //
  // The gate is the caller's to run first — it needs the device identity and
  // the application's version, and it decides whether there is anything to
  // offer at all. Run() assumes it passed.
  //
  // Never writes the factory image, whatever the bundle carries. That is
  // InstallFactoryGateware below, and the separation is the point: an
  // ordinary update cannot reach the factory region even by being handed a
  // file that contains one.
  UpdateOutcome Run(const UpdateBundle& bundle);

  // Write the bundle's factory image to the EPCS at address 0.
  //
  // The image a board falls back to, and the one thing an ordinary update
  // never touches — so this is a separate entry point rather than another
  // branch of Run(), called by the bring-up path and by nothing else. The
  // firmware refuses it too, unless the request carries the factory-write
  // word, so this being reachable in the code is not what makes it reachable
  // on a device.
  //
  // Two conditions the caller owns, because this class cannot check either:
  // the FPGA must be running gateware with a flash bridge, which bring-up has
  // just configured over JTAG, and the power cycle afterwards is what makes
  // the written image the running one. No reset and no reconfiguration happen
  // here — reconfiguring would reload the *application* image, which is not
  // what has just been written.
  UpdateOutcome InstallFactoryGateware(const UpdateBundle& bundle);

  // Write everything a board being brought up needs, in the one order that
  // leaves every interruption recoverable.
  //
  // **The order is the whole reason this is not Run().** Three writes, and
  // each one is chosen by what a board looks like if the power goes out
  // immediately afterwards:
  //
  //   1. the EEPROM, so that every state from here on boots the new firmware
  //      rather than the legacy firmware this may be replacing — which is what
  //      keeps the interconnect out of the pairing where two outputs share a
  //      net (see bringup_orchestrator.h);
  //   2. the factory image, so that the board always has something valid to
  //      fall back to before anything is written to the region it falls back
  //      *from*. Interrupted here, the board comes up in its factory image
  //      with a flash bridge, which is the state the ordinary update path
  //      repairs;
  //   3. the application image and its boot block, which is an ordinary
  //      gateware update and the one write whose absence is harmless.
  //
  // Written the other way round — application before factory — an interrupted
  // run would leave a board with a valid application image and no factory
  // image to load it, which on a bare board is an FPGA that configures from
  // nothing and looks dead.
  //
  // Nothing is restarted, reconfigured or confirmed: the caller owns the power
  // cycle, which is what makes all three images the running ones at once, and
  // the check afterwards. The outcome reports succeeded with identity_confirmed
  // false, exactly as a deferred Run() does.
  //
  // Every payload must be present. Checked here as well as by the caller,
  // because a run that got two writes into three and then found nothing to do
  // the third with would have stopped in the one place this ordering exists to
  // avoid stopping.
  UpdateOutcome RunBringUp(const UpdateBundle& bundle);

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
  bool defer_restart_ = false;
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

// The same estimate for one component on its own.
//
// Public because the interface's progress bar needs it and must not derive it
// a second time: the bar is one bar over the whole update, so how much of it
// each component is entitled to *is* this arithmetic. Two copies of it would
// be a bar that disagreed with the estimate printed above it.
double EstimateComponentSeconds(UpdateTarget target,
                                const UpdateComponent& component);

// The device restarting and coming back, which no amount of arithmetic will
// predict. Public for the same reason as the function above.
inline constexpr int kUpdateRestartSeconds = 10;

}  // namespace ddd::capture
