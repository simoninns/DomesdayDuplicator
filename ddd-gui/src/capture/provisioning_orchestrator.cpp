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

// The rate the estimate is built on. See the header for where it comes from.
constexpr double kProvisioningBytesPerSecond = 60000.0;

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
        "This provisioning set carries no gateware for the FPGA.");
  }

  if (!access_.open_cable) {
    return GatewareFailure("No JTAG cable is available in this build.");
  }

  std::string problem;
  const std::unique_ptr<IJtagCable> cable = access_.open_cable(&problem);
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
      progress.target = UpdateTarget::kGateware;
      progress.done = done;
      progress.total = total;
      progress.message =
          "Programming the FPGA's configuration flash through the "
          "USB-Blaster. It pauses for a long while as each block is erased.";
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

  if (logger_ != nullptr) {
    logger_->Info("FPGA provisioning complete: " +
                  std::to_string(outcome.play.statements) +
                  " statements played, " +
                  std::to_string(outcome.play.shifted_bits) +
                  " bits shifted. The board must now be power-cycled.");
  }

  return outcome;
}

}  // namespace ddd::capture
