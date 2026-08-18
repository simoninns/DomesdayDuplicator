/************************************************************************

    bringup_orchestrator.cpp

    Bringing a board up to current firmware and gateware, in the one order
    that is safe
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "bringup_orchestrator.h"

#include <string_view>
#include <utility>

#include "logger.h"

namespace ddd::capture {
namespace {

// The rate the estimate is built on. See the header for where it comes from:
// 1,450,426 bytes in 2.6 seconds, measured, rounded down.
constexpr double kConfigureBytesPerSecond = 500000.0;

}  // namespace

int EstimateConfigureSeconds(uint64_t svf_bytes) {
  return static_cast<int>(static_cast<double>(svf_bytes) /
                          kConfigureBytesPerSecond) +
         1;
}

BringUpOrchestrator::BringUpOrchestrator(BringUpAccess access, ILogger* logger)
    : access_(std::move(access)), logger_(logger) {}

BringUpConfigureOutcome BringUpOrchestrator::ConfigureFailure(
    std::string problem) const {
  BringUpConfigureOutcome outcome;
  outcome.problem = std::move(problem);
  if (logger_ != nullptr) {
    logger_->Error(outcome.problem);
  }
  return outcome;
}

BringUpConfigureOutcome BringUpOrchestrator::ConfigureFpga(
    const UpdateBundle& bundle) {
  fpga_configured_ = false;

  if (!bundle.manifest.provisioning.has_value() ||
      bundle.provisioning.empty()) {
    return ConfigureFailure(
        "This file carries no JTAG vectors, so there is no way to give this "
        "board a gateware to be reached through.");
  }

  if (!access_.open_cable) {
    return ConfigureFailure("No JTAG cable is available in this build.");
  }

  std::string problem;
  const std::unique_ptr<IJtagCable> cable = access_.open_cable(&problem);
  if (cable == nullptr) {
    return ConfigureFailure(problem.empty()
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
          "Loading the gateware into the FPGA through the USB-Blaster. "
          "Nothing is written to the board by this step.";
      progress_(progress);
    });
  }
  if (cancel_) {
    player.SetStopCallback(cancel_);
  }

  const std::string_view text(
      reinterpret_cast<const char*>(bundle.provisioning.data()),
      bundle.provisioning.size());

  BringUpConfigureOutcome outcome;
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

  fpga_configured_ = true;

  if (logger_ != nullptr) {
    logger_->Info("The FPGA is running the factory image: " +
                  std::to_string(outcome.play.statements) +
                  " statements played, " +
                  std::to_string(outcome.play.shifted_bits) +
                  " bits shifted. Nothing has been written to the board yet.");
  }

  return outcome;
}

UpdateOutcome BringUpOrchestrator::ProgramDevice(const UpdateBundle& bundle) {
  UpdateOutcome outcome;

  // The ordering rule, checked rather than assumed. A caller that reached here
  // first would be about to run firmware over whatever gateware the board came
  // with — and on a board that came with none, over nothing at all.
  if (!fpga_configured_) {
    outcome.stage = UpdateStage::kFailed;
    outcome.problem =
        "The FPGA has to be configured before anything can be written. "
        "Bring-up loads the gateware over the JTAG cable first, so that the "
        "firmware has a flash bridge to write through.";
    if (logger_ != nullptr) {
      logger_->Error(outcome.problem);
    }
    return outcome;
  }

  // Every payload, before any of them is used. A bundle that could do half the
  // job would leave a board in a state nobody planned for — and, unlike the
  // ordinary update path, there is no half of a bring-up that is worth having.
  if (!bundle.manifest.firmware.has_value() || bundle.firmware.empty()) {
    outcome.stage = UpdateStage::kFailed;
    outcome.problem =
        "This file carries no firmware, and a board being brought up needs "
        "firmware before anything else can be done to it.";
    return outcome;
  }

  if (!bundle.manifest.factory_gateware.has_value() ||
      bundle.factory_gateware.empty()) {
    outcome.stage = UpdateStage::kFailed;
    outcome.problem =
        "This file carries no factory image for the FPGA's flash.";
    return outcome;
  }

  if (!bundle.manifest.gateware.has_value() || bundle.gateware.empty()) {
    outcome.stage = UpdateStage::kFailed;
    outcome.problem = "This file carries no gateware for the FPGA's flash.";
    return outcome;
  }

  RecoveryInstaller installer(access_.fx3, logger_);
  installer.SetTimings(timings_);
  installer.SetUpdateTimings(update_timings_);

  if (progress_) {
    installer.SetProgressCallback(progress_);
  }
  if (cancel_) {
    installer.SetCancelCallback(cancel_);
  }

  // The jumper may still be fitted and the FPGA is running a configuration it
  // will lose, so nothing here restarts anything: the wizard's one power cycle
  // is what makes every image written below the running one, and it owns the
  // check afterwards.
  outcome = installer.RunBringUp(bundle);

  if (outcome.succeeded && logger_ != nullptr) {
    logger_->Info(
        "The board is programmed: EEPROM, factory image and application image "
        "all written and read back. It must be power-cycled to run them.");
  }

  return outcome;
}

}  // namespace ddd::capture
