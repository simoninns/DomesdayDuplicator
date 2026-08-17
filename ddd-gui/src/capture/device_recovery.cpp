/************************************************************************

    device_recovery.cpp

    Installing onto a device that has no working firmware to install with
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "device_recovery.h"

#include <utility>

#include "boot_image.h"
#include "logger.h"
#include "update_bundle.h"

namespace ddd::capture {
namespace {

// A failed prelude, described the way every other failure in this mechanism
// is: what happened, and the fact that the device is no worse off than it was.
//
// That last part is not reassurance for its own sake. A device in a recovery
// personality has already had its EEPROM rejected by its own boot ROM, and
// nothing the prelude does writes to the EEPROM at all — the firmware goes
// into RAM, which is empty again the moment the device loses power. A recovery
// that fails half way leaves a device that is exactly as recoverable as it was
// before it was tried.
UpdateOutcome Failure(std::string problem) {
  UpdateOutcome outcome;
  outcome.succeeded = false;
  outcome.stage = UpdateStage::kPreparing;
  outcome.problem = std::move(problem);
  return outcome;
}

}  // namespace

RecoveryInstaller::RecoveryInstaller(DeviceAccess access, ILogger* logger)
    : access_(std::move(access)), logger_(logger) {}

void RecoveryInstaller::Report(uint64_t done, uint64_t total,
                               std::string message) {
  if (!progress_) {
    return;
  }

  UpdateProgress step;
  step.stage = UpdateStage::kPreparing;
  step.target = UpdateTarget::kFirmware;
  step.done = done;
  step.total = total;
  step.message = std::move(message);
  progress_(step);
}

std::optional<std::string> RecoveryInstaller::Wake(const UpdateBundle& bundle,
                                                   UpdateOutcome& outcome) {
  if (bundle.firmware.empty()) {
    outcome = Failure(
        "This device has no working firmware, and this update file does not "
        "contain any. Choose an update that includes firmware.");
    return std::nullopt;
  }

  // Parsed before the device is opened, so a file that is not an image is
  // refused without anything having been sent anywhere.
  std::string problem;
  const std::optional<BootImage> image =
      ParseBootImage(bundle.firmware, &problem);
  if (!image.has_value()) {
    outcome = Failure(problem);
    return std::nullopt;
  }

  if (!access_.open_programmer) {
    outcome = Failure("This device cannot be reached for programming.");
    return std::nullopt;
  }

  std::unique_ptr<IDeviceProgrammer> programmer = access_.open_programmer();
  if (programmer == nullptr) {
    outcome = Failure(
        "The device in recovery mode could not be opened. Unplug it, plug it "
        "back in, and try again.");
    return std::nullopt;
  }

  Report(0, image->payload_bytes,
         "Sending firmware to the device's memory. Nothing is written "
         "permanently yet.");

  uint64_t sent = 0;
  for (const BootImageSection& section : image->sections) {
    if (Cancelled()) {
      outcome = Failure(
          "Stopped before anything was written. The device is in the same "
          "state it was in before.");
      return std::nullopt;
    }

    if (!programmer->WriteRam(
            section.address,
            bundle.firmware.subspan(section.offset, section.length))) {
      outcome = Failure(
          "The device stopped accepting the firmware. Nothing was written "
          "permanently; unplug it, plug it back in, and try again.");
      return std::nullopt;
    }

    sent += section.length;
    Report(sent, image->payload_bytes,
           "Sending firmware to the device's memory. Nothing is written "
           "permanently yet.");
  }

  if (!programmer->Start(image->entry_address)) {
    outcome = Failure(
        "The device did not start the firmware it was given. Unplug it, plug "
        "it back in, and try again.");
    return std::nullopt;
  }

  // No proportion to report: the device is away and the only honest thing to
  // show is what is being waited for.
  Report(0, 0,
         "The device is restarting with the new firmware. It will disconnect "
         "and reconnect by itself.");

  const std::optional<std::string> path =
      programmer->WaitForApplication(timings_.return_timeout);
  if (!path.has_value()) {
    outcome = Failure(
        "The device did not come back after being given its firmware. Unplug "
        "it, plug it back in, and try again.");
    return std::nullopt;
  }

  if (logger_ != nullptr) {
    logger_->Info("The device is running firmware from memory at " + *path +
                  "; writing it to the device");
  }

  return path;
}

UpdateOutcome RecoveryInstaller::Run(const UpdateBundle& bundle) {
  UpdateOutcome outcome;

  const std::optional<std::string> path = Wake(bundle, outcome);
  if (!path.has_value()) {
    return outcome;
  }

  if (!access_.open_updater) {
    return Failure("This device cannot be reached for updating.");
  }

  std::unique_ptr<IDeviceUpdater> updater = access_.open_updater(*path);
  if (updater == nullptr) {
    return Failure(
        "The device came back but could not be opened. Unplug it, plug it "
        "back in, and try again.");
  }

  // From here it is an ordinary update, and deliberately so: the firmware now
  // running in the device's memory is the same firmware that is about to be
  // written to its EEPROM, and it writes it through the same protocol, the
  // same digest of the incoming stream and the same digest of the readback
  // that a routine update uses. A first-time programming of a bare board is
  // covered by the whole integrity chain, not by a shortcut around it.
  UpdateOrchestrator orchestrator(*updater, logger_);
  orchestrator.SetTimings(update_timings_);
  orchestrator.SetDeferRestart(defer_restart_);
  if (progress_) {
    orchestrator.SetProgressCallback(progress_);
  }
  if (cancel_) {
    orchestrator.SetCancelCallback(cancel_);
  }

  return orchestrator.Run(bundle);
}

}  // namespace ddd::capture
