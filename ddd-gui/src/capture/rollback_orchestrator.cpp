/************************************************************************

    rollback_orchestrator.cpp

    Returning a unit to the original firmware and gateware, in the one
    order that is safe
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "rollback_orchestrator.h"

#include <utility>

#include "logger.h"

namespace ddd::capture {

RollbackOrchestrator::RollbackOrchestrator(IDeviceUpdater& device,
                                           ILogger* logger)
    : device_(device), logger_(logger) {}

UpdateOutcome RollbackOrchestrator::Failure(std::string problem) const {
  UpdateOutcome outcome;
  outcome.stage = UpdateStage::kFailed;
  outcome.problem = std::move(problem);
  if (logger_ != nullptr) {
    logger_->Error(outcome.problem);
  }
  return outcome;
}

void RollbackOrchestrator::Configure(UpdateOrchestrator& orchestrator) const {
  orchestrator.SetTimings(timings_);
  if (progress_) {
    orchestrator.SetProgressCallback(progress_);
  }
  if (cancel_) {
    orchestrator.SetCancelCallback(cancel_);
  }
}

UpdateOutcome RollbackOrchestrator::ProgramGateware(
    const UpdateBundle& bundle) {
  gateware_installed_ = false;

  if (!bundle.manifest.factory_gateware.has_value() ||
      bundle.factory_gateware.empty()) {
    return Failure(
        "This rollback file carries no gateware, and a unit whose firmware "
        "went back on its own would be the original firmware driving the "
        "current gateware — which is the one combination this never creates.");
  }

  UpdateOrchestrator orchestrator(device_, logger_);
  Configure(orchestrator);

  const UpdateOutcome outcome = orchestrator.InstallFactoryGateware(bundle);
  gateware_installed_ = outcome.succeeded;

  if (outcome.succeeded && logger_ != nullptr) {
    logger_->Info(
        "The original gateware is in the flash. The FPGA is still running the "
        "current one until the board is power-cycled.");
  }

  return outcome;
}

UpdateOutcome RollbackOrchestrator::InstallFirmware(
    const UpdateBundle& bundle) {
  // The ordering rule, checked rather than assumed. A caller that reached here
  // first would be about to write the original firmware while the current
  // gateware is still what the flash holds — and one power cycle later, both
  // ends would be driving CTL_07.
  if (!gateware_installed_) {
    return Failure(
        "The firmware cannot be rolled back until the gateware has been. A "
        "rollback does the FPGA first so that the original firmware is never "
        "running underneath the current gateware.");
  }

  if (!bundle.manifest.firmware.has_value() || bundle.firmware.empty()) {
    return Failure("This rollback file carries no firmware.");
  }

  UpdateOrchestrator orchestrator(device_, logger_);
  Configure(orchestrator);

  // Deferred, and this is where the two halves are held together: the EEPROM
  // is written and read back, and then nothing happens until the user pulls
  // the cable. Both images become the running ones at that moment, or neither
  // does.
  orchestrator.SetDeferRestart(true);

  // Run() rather than a firmware-only entry point, because a rollback bundle
  // carries no `gateware` component — its gateware is the factory one, written
  // above — so this installs exactly the firmware and nothing else. If a
  // rollback file ever did carry an application image, installing it here
  // would be right too: it would be going to the address the legacy image does
  // not boot from and does not read.
  const UpdateOutcome outcome = orchestrator.Run(bundle);

  if (outcome.succeeded && logger_ != nullptr) {
    logger_->Info(
        "Both original images are written. The unit is still running the "
        "current firmware and gateware until it is power-cycled.");
  }

  return outcome;
}

}  // namespace ddd::capture
