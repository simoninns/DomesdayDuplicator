/************************************************************************

    provisioning_orchestrator.cpp

    Bringing a board up to current firmware and gateware, in the one order
    that is safe
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "provisioning_orchestrator.h"

#include <string_view>
#include <utility>

#include "logger.h"

namespace ddd::capture {
namespace {

// The rate the estimate is built on. See the header for where it comes from:
// 1,450,426 bytes in 2.6 seconds, measured, rounded down.
constexpr double kProvisioningBytesPerSecond = 500000.0;

}  // namespace

int EstimateProvisioningSeconds(uint64_t svf_bytes) {
  return static_cast<int>(static_cast<double>(svf_bytes) /
                          kProvisioningBytesPerSecond) +
         1;
}

ProvisioningOrchestrator::ProvisioningOrchestrator(ProvisioningAccess access,
                                                   ILogger* logger)
    : access_(std::move(access)), logger_(logger) {}

UpdateOutcome ProvisioningOrchestrator::InstallFirmware(
    const UpdateBundle& bundle) {
  firmware_installed_ = false;

  UpdateOutcome outcome;
  if (!bundle.manifest.firmware.has_value() || bundle.firmware.empty()) {
    outcome.stage = UpdateStage::kFailed;
    outcome.problem =
        "This provisioning set carries no firmware, and a board being brought "
        "up needs firmware before anything else can be done to it.";
    return outcome;
  }

  RecoveryInstaller installer(access_.fx3, logger_);
  installer.SetTimings(timings_);
  installer.SetUpdateTimings(update_timings_);

  // The jumper is still fitted, so the device must not be reset here: it would
  // come back in its boot ROM and the confirmation would compare the firmware
  // just written against a device that is not running it.
  installer.SetDeferRestart(true);

  if (progress_) {
    installer.SetProgressCallback(progress_);
  }
  if (cancel_) {
    installer.SetCancelCallback(cancel_);
  }

  outcome = installer.Run(bundle);
  firmware_installed_ = outcome.succeeded;
  fx3_path_ = installer.device_path();
  return outcome;
}

ProvisioningGatewareOutcome ProvisioningOrchestrator::GatewareFailure(
    std::string problem) const {
  ProvisioningGatewareOutcome outcome;
  outcome.problem = std::move(problem);
  if (logger_ != nullptr) {
    logger_->Error(outcome.problem);
  }
  return outcome;
}

ProvisioningGatewareOutcome ProvisioningOrchestrator::ProgramGateware(
    const UpdateBundle& bundle) {
  // The ordering rule, checked rather than assumed. A caller that reached here
  // first would be about to make the gateware modern while the FX3 may still
  // be running the legacy firmware — which drives the same net the modern
  // gateware drives.
  if (!firmware_installed_) {
    return GatewareFailure(
        "The FPGA cannot be programmed until the FX3 firmware has been "
        "installed. Bring-up does the FX3 first so that the original firmware "
        "is never running underneath the new gateware.");
  }

  if (!bundle.manifest.provisioning.has_value() ||
      bundle.provisioning.empty()) {
    return GatewareFailure(
        "This provisioning set carries no JTAG vectors, so there is no way to "
        "give this board a gateware to be reached through.");
  }

  // Both halves, checked before either runs. A set that could configure the
  // FPGA but not write its flash would leave a board looking provisioned
  // until the next power cycle, which is the most confusing state this flow
  // could possibly stop in.
  if (!bundle.manifest.factory_gateware.has_value() ||
      bundle.factory_gateware.empty()) {
    return GatewareFailure(
        "This provisioning set carries no factory image for the FPGA's flash.");
  }

  if (!access_.open_cable) {
    return GatewareFailure("No JTAG cable is available in this build.");
  }

  std::string problem;
  std::unique_ptr<IJtagCable> cable = access_.open_cable(&problem);
  if (cable == nullptr) {
    return GatewareFailure(problem.empty()
                               ? std::string("The USB-Blaster could not be "
                                             "opened.")
                               : problem);
  }

  SvfPlayer player(*cable, logger_);

  // Bytes of the file, straight through to the caller's bar. The player counts
  // in the one unit that is known before the run starts, so the proportion is
  // honest from the first update rather than after a guess.
  if (progress_) {
    player.SetProgressCallback([this](size_t done, size_t total) {
      UpdateProgress progress;
      progress.stage = UpdateStage::kWriting;
      progress.target = UpdateTarget::kEpcsFactory;
      progress.done = done;
      progress.total = total;
      progress.message =
          "Loading the recovery gateware into the FPGA through the "
          "USB-Blaster. Nothing is written to the board by this step.";
      progress_(progress);
    });
  }
  if (cancel_) {
    player.SetStopCallback(cancel_);
  }

  const std::string_view text(
      reinterpret_cast<const char*>(bundle.provisioning.data()),
      bundle.provisioning.size());

  ProvisioningGatewareOutcome outcome;
  outcome.play = player.Play(text);
  outcome.succeeded = outcome.play.succeeded;
  outcome.stopped = outcome.play.stopped;

  if (!outcome.succeeded) {
    // The player's own words, which name the line and both values on a
    // mismatch. Prefixed with nothing: it has already written a sentence for a
    // user, and wrapping it would only bury the part that matters.
    outcome.problem = outcome.play.problem;
    if (logger_ != nullptr) {
      logger_->Error(outcome.problem);
    }
    return outcome;
  }

  outcome.configured = true;

  if (logger_ != nullptr) {
    logger_->Info("The FPGA is running the recovery gateware: " +
                  std::to_string(outcome.play.statements) +
                  " statements played, " +
                  std::to_string(outcome.play.shifted_bits) +
                  " bits shifted. Nothing has been written to the board yet.");
  }

  // The cable's work is done, and it is let go before the flash write starts.
  // Two reasons, and the second is the one that would be hard to diagnose:
  // the write does not need it, and holding a USB-Blaster open across an
  // operation that has nothing to do with it is how Quartus's jtagd and this
  // application end up fighting over a cable neither is using.
  cable.reset();

  // And now the half that writes something. The FPGA is running the factory
  // image out of its own RAM, so it is offering the flash bridge the firmware
  // needs — which is the whole point of the step above. From here it is an
  // ordinary update transfer, aimed at the factory region: the same protocol,
  // the same digest of the stream, the same readback digest off the medium.
  //
  // The host cannot do this part itself. A register write carries one byte per
  // USB control transfer, so driving the bridge from here would be millions of
  // round trips for an image the device writes in seconds.
  //
  // Failures from here on are reported through `outcome` rather than through
  // GatewareFailure, so that `configured` survives: the cable did its job, and
  // a page that blamed the cable for a flash failure would send the user to
  // check the one thing that is working.
  const auto fail = [this, &outcome](std::string problem) {
    outcome.succeeded = false;
    outcome.problem = std::move(problem);
    if (logger_ != nullptr) {
      logger_->Error(outcome.problem);
    }
    return outcome;
  };

  if (!access_.fx3.open_updater) {
    return fail("This device cannot be reached for updating.");
  }
  if (fx3_path_.empty()) {
    return fail(
        "The device the firmware was installed on is no longer known, so its "
        "FPGA cannot be programmed. Start the wizard again.");
  }

  const std::unique_ptr<IDeviceUpdater> updater =
      access_.fx3.open_updater(fx3_path_);
  if (updater == nullptr) {
    return fail(
        "The FPGA is loaded, but the Duplicator could not be opened to write "
        "it to the board. Unplug both cables, reconnect them, and start the "
        "wizard again.");
  }

  UpdateOrchestrator flash(*updater, logger_);
  flash.SetTimings(update_timings_);
  if (progress_) {
    flash.SetProgressCallback(progress_);
  }
  if (cancel_) {
    flash.SetCancelCallback(cancel_);
  }

  const UpdateOutcome written = flash.InstallFactoryGateware(bundle);
  outcome.succeeded = written.succeeded;
  if (!written.succeeded) {
    return fail(written.problem);
  }

  if (logger_ != nullptr) {
    logger_->Info(
        "The FPGA's flash now holds the recovery gateware. The board must be "
        "power-cycled to run it from there.");
  }

  return outcome;
}

}  // namespace ddd::capture
