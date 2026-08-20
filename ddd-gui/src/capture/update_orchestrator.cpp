/************************************************************************

    update_orchestrator.cpp

    Driving an update from a verified bundle to a device that reports the
    new version
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "update_orchestrator.h"

#include <algorithm>
#include <string>
#include <thread>

#include "firmware_version.h"
#include "log_format.h"
#include "logger.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

// Roughly how fast each medium takes bytes, used only for the estimate the
// interface shows before an update starts.
//
// The EEPROM figure is the one that is measured: 64-byte pages over a
// 400 kHz I2C bus with an acknowledgement poll between them, written once
// and read back once.
//
// The EPCS figure is derived rather than measured, and the derivation is
// worth keeping because it is not obvious why the smaller number is the
// slower medium. Every byte of a gateware image crosses a bit-banged SPI
// link to the FPGA at roughly a hundred microseconds per byte: writing costs
// one such byte, because a whole flash page goes out in one framed
// transaction, and reading costs four, because each byte needs a write to
// shift it and a read to collect it. Write once and read back once is
// therefore about five hundred microseconds a byte, plus a second of erase
// per 64 KiB sector. That is around two thousand bytes a second, and it is
// deliberately the pessimistic end: an estimate that is too long makes a
// user wait, and an estimate that is too short makes them unplug the device.
//
// Replaced with a measurement when verification item V6 is taken on the
// bench, which is also where the decision to optimise the bit-bang is made.
constexpr double kEepromBytesPerSecond = 12000.0;
constexpr double kEpcsBytesPerSecond = 2000.0;

// A chunk is a whole number of the medium's pages except the last, which is
// what lets the firmware write a chunk straight to the medium with no
// assembly buffer in between. One alignment covers both targets — see
// kUpdateChunkAlignment.
uint64_t AlignedChunk(uint64_t maximum) {
  const uint64_t aligned = maximum - (maximum % kUpdateChunkAlignment);
  return aligned == 0 ? kUpdateChunkAlignment : aligned;
}

}  // namespace

const char* UpdateStageName(UpdateStage stage) {
  switch (stage) {
    case UpdateStage::kChecking:
      return "Checking";
    case UpdateStage::kPreparing:
      return "Preparing device";
    case UpdateStage::kTransferring:
      return "Sending";
    case UpdateStage::kWriting:
      return "Writing";
    case UpdateStage::kVerifying:
      return "Verifying";
    case UpdateStage::kRestarting:
      return "Restarting device";
    case UpdateStage::kConfirming:
      return "Confirming";
    case UpdateStage::kComplete:
      return "Complete";
    case UpdateStage::kFailed:
      return "Failed";
  }
  return "Unknown";
}

double EstimateComponentSeconds(UpdateTarget target,
                                const UpdateComponent& component) {
  // Both flash targets are the same flash written the same way, so they share
  // a rate. What differs is where the bytes land, and an erase costs the same
  // at either address.
  const double rate = target == UpdateTarget::kFirmware ? kEepromBytesPerSecond
                                                        : kEpcsBytesPerSecond;
  return static_cast<double>(component.length) / rate;
}

int EstimateUpdateSeconds(const UpdateManifest& manifest) {
  double seconds = kUpdateRestartSeconds;

  if (manifest.firmware.has_value()) {
    seconds +=
        EstimateComponentSeconds(UpdateTarget::kFirmware, *manifest.firmware);
  }
  if (manifest.gateware.has_value()) {
    seconds +=
        EstimateComponentSeconds(UpdateTarget::kGateware, *manifest.gateware);
  }

  return static_cast<int>(seconds) + 1;
}

UpdateOrchestrator::UpdateOrchestrator(IDeviceUpdater& device, ILogger* logger)
    : device_(device), logger_(logger) {}

namespace {

// A digest is 64 characters and a log line is read by eye. The first eight are
// enough to tell two payloads apart and to match a line against a manifest,
// which is all this is ever used for.
std::string ShortDigest(const Sha256Digest& digest) {
  return ToHex(digest).substr(0, 8);
}

// One component, as a log line: what it is, how big, what it will make the
// device report, and which payload it is.
std::string DescribeComponent(UpdateTarget target,
                              const UpdateComponent& component) {
  return std::string(UpdateTargetName(target)) + " " +
         (component.identity.empty() ? std::string("(unnamed)")
                                     : component.identity) +
         ", " + FormatBytes(component.length) + ", sha256 " +
         ShortDigest(component.sha256) + ", interface version " +
         std::to_string(component.interface_version);
}

}  // namespace

void UpdateOrchestrator::LogPlan(const char* what,
                                 const UpdateBundle& bundle) const {
  if (logger_ == nullptr) {
    return;
  }

  logger_->Debug(std::string(what) + " starting: bundle version " +
                 bundle.manifest.version + ", channel " +
                 UpdateChannelName(bundle.manifest.channel) +
                 ", restart deferred " + (defer_restart_ ? "yes" : "no"));

  // Every component the file carries, whether or not this flow will write it.
  // What a bundle contains and what a run installs are different questions,
  // and a log that only answered the second cannot tell a missing payload
  // from a payload that was deliberately skipped.
  if (bundle.manifest.firmware.has_value()) {
    logger_->Debug("  carries " + DescribeComponent(UpdateTarget::kFirmware,
                                                    *bundle.manifest.firmware));
  }
  if (bundle.manifest.gateware.has_value()) {
    logger_->Debug("  carries " + DescribeComponent(UpdateTarget::kGateware,
                                                    *bundle.manifest.gateware));
  }
  if (bundle.manifest.factory_gateware.has_value()) {
    logger_->Debug("  carries " +
                   DescribeComponent(UpdateTarget::kEpcsFactory,
                                     *bundle.manifest.factory_gateware));
  }
}

void UpdateOrchestrator::LogOutcome(
    const char* what, const UpdateOutcome& outcome,
    std::chrono::steady_clock::time_point started) const {
  if (logger_ == nullptr) {
    return;
  }

  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
          .count();

  std::string line =
      std::string(what) + " finished after " + FormatDuration(seconds) + ": " +
      (outcome.succeeded ? "succeeded" : "failed") + " at stage " +
      UpdateStageName(outcome.stage) + ", identity " +
      (outcome.identity_confirmed ? "confirmed" : "not confirmed");

  if (!outcome.identity.product_string.empty()) {
    line += ", device reports \"" + outcome.identity.product_string + "\"";
  }
  if (!outcome.identity.gateware_commit.empty()) {
    line += ", gateware " + outcome.identity.gateware_commit;
  }
  if (!outcome.problem.empty()) {
    line += "; " + outcome.problem;
  }

  logger_->Debug(line);
}

void UpdateOrchestrator::LogDeviceStatus(const char* when) {
  if (logger_ == nullptr) {
    return;
  }

  const std::optional<DeviceUpdateStatus> status = device_.ReadStatus();
  if (!status.has_value()) {
    logger_->Debug(std::string("Device status at ") + when +
                   ": the device did not answer");
    return;
  }

  logger_->Debug(std::string("Device status at ") + when + ": phase " +
                 UpdatePhaseName(status->phase) + ", error " +
                 DeviceUpdateErrorName(status->error) + " (" +
                 std::to_string(static_cast<int>(status->error)) + "), " +
                 std::to_string(status->bytes_received) + " received, " +
                 std::to_string(status->bytes_written) + " written, " +
                 std::to_string(status->bytes_verified) + " verified");
}

void UpdateOrchestrator::Report(UpdateStage stage, UpdateTarget target,
                                uint64_t done, uint64_t total,
                                std::string message) {
  if (!progress_) {
    return;
  }

  UpdateProgress progress;
  progress.stage = stage;
  progress.target = target;
  progress.done = done;
  progress.total = total;
  progress.message = std::move(message);
  progress_(progress);
}

bool UpdateOrchestrator::AwaitCompletion(UpdateTarget target, uint64_t total,
                                         UpdateOutcome& outcome) {
  // Written and verified separately, because they are separate stages to
  // whoever is watching and because a bar that showed their sum would appear
  // to run backwards when the second one started.
  uint32_t last_written = 0;
  uint32_t last_verified = 0;
  auto last_movement = std::chrono::steady_clock::now();

  while (true) {
    if (Cancelled()) {
      outcome.stage = UpdateStage::kFailed;
      outcome.problem =
          "The update was stopped. The device was not left half-written — it "
          "still has the firmware it started with.";
      return false;
    }

    const std::optional<DeviceUpdateStatus> status = device_.ReadStatus();
    if (!status.has_value()) {
      outcome.stage = UpdateStage::kFailed;
      outcome.problem =
          "The device stopped answering part way through the update. Leave it "
          "plugged in, then try again.";
      return false;
    }

    if (status->phase == UpdatePhase::kFailed) {
      outcome.stage = UpdateStage::kFailed;
      outcome.problem = DeviceUpdateErrorText(status->error);

      // And where it got to, which is often the whole diagnosis. A device
      // that fails at the first page and a device that fails after 128 KiB
      // report the same error code and have completely different faults: the
      // second one is a medium that ends where the writing stopped. The
      // counters are already being read for the progress bar, so this costs a
      // sentence and nothing else.
      if (status->bytes_written > 0) {
        outcome.problem += " It stopped after writing " +
                           std::to_string(status->bytes_written) + " of " +
                           std::to_string(total) + " bytes.";
      }
      return false;
    }

    if (status->phase == UpdatePhase::kComplete) {
      Report(UpdateStage::kVerifying, target, total, total,
             "The device has confirmed what it wrote.");
      return true;
    }

    if (status->bytes_written != last_written ||
        status->bytes_verified != last_verified) {
      last_written = status->bytes_written;
      last_verified = status->bytes_verified;
      last_movement = std::chrono::steady_clock::now();
    } else if (std::chrono::steady_clock::now() - last_movement >
               timings_.stall_timeout) {
      outcome.stage = UpdateStage::kFailed;
      outcome.problem =
          "The device stopped making progress. Leave it plugged in, unplug "
          "and replug it, then try again.";
      return false;
    }

    if (status->phase == UpdatePhase::kVerifying) {
      Report(UpdateStage::kVerifying, target, status->bytes_verified, total,
             "The device is reading back what it wrote and checking it.");
    } else {
      Report(UpdateStage::kWriting, target, status->bytes_written, total,
             "The device is writing the update to its own memory.");
    }

    std::this_thread::sleep_for(timings_.poll_interval);
  }
}

bool UpdateOrchestrator::InstallComponent(UpdateTarget target,
                                          const UpdateComponent& component,
                                          std::span<const uint8_t> payload,
                                          UpdateOutcome& outcome) {
  // A wrapper for the same reason the three entry points have one: a component
  // can fail at half a dozen points inside, and a line at each of them would
  // be six chances to add a seventh and forget.
  const auto started = std::chrono::steady_clock::now();

  const bool installed = WriteComponent(target, component, payload, outcome);

  if (logger_ != nullptr) {
    const double seconds = std::chrono::duration<double>(
                               std::chrono::steady_clock::now() - started)
                               .count();
    logger_->Debug(
        std::string(installed ? "Installed " : "Failed to install ") +
        UpdateTargetName(target) + " after " + FormatDuration(seconds) +
        (installed ? "" : ": " + outcome.problem));
  }

  if (!installed) {
    // The device's own account of itself at the moment it stopped, which is
    // the diagnosis and the one thing nobody can go back and ask for later.
    LogDeviceStatus("the failure");
  }

  return installed;
}

bool UpdateOrchestrator::WriteComponent(UpdateTarget target,
                                        const UpdateComponent& component,
                                        std::span<const uint8_t> payload,
                                        UpdateOutcome& outcome) {
  const uint64_t total = payload.size();

  // The chunk size comes from the device rather than from a constant here,
  // so a firmware that raises it needs no change on this side.
  const std::optional<DeviceUpdateStatus> initial = device_.ReadStatus();
  if (!initial.has_value()) {
    outcome.stage = UpdateStage::kFailed;
    outcome.problem =
        "This device's firmware cannot update itself. It has to be programmed "
        "with the bench procedure once before it can be updated from here.";
    return false;
  }

  if (initial->maximum_chunk_bytes == 0) {
    outcome.stage = UpdateStage::kFailed;
    outcome.problem = "The device did not say how much data it can take.";
    return false;
  }

  const uint64_t chunk_bytes = AlignedChunk(initial->maximum_chunk_bytes);

  const auto component_started = std::chrono::steady_clock::now();

  if (logger_ != nullptr) {
    // The chunk size is the device's, not this build's, so it belongs in the
    // log: a firmware that raises it changes the shape of every transfer that
    // follows, and this is the line that says which one was in force.
    logger_->Debug("Installing " + DescribeComponent(target, component) + ": " +
                   std::to_string((total + chunk_bytes - 1) / chunk_bytes) +
                   " chunks of " + FormatBytes(chunk_bytes) +
                   " (the device offered " +
                   std::to_string(initial->maximum_chunk_bytes) +
                   " bytes), device phase " + UpdatePhaseName(initial->phase));
  }

  // Named for what the *host* is doing, because that is what the progress
  // bar measures. On both targets the device writes each chunk to its medium
  // as it arrives — there is no assembly buffer — so this stage is where
  // most of the writing happens, and the message says so rather than leaving
  // a user to infer it from a "writing" stage that never appears.
  //
  // The gateware message says one thing more, because the gateware target
  // does one thing more: it erases each 64 KiB sector as the write reaches
  // it, which takes about a second and stops the bar for that long, roughly
  // every thirty chunks. A progress bar that pauses for a second with no
  // explanation is a progress bar a user starts to distrust.
  //
  // The factory target says which image it is writing, because it is the one
  // write in the application that replaces the thing a board falls back to,
  // and a user watching it should be able to tell it apart from the gateware
  // write that looks exactly like it.
  std::string sending;
  switch (target) {
    case UpdateTarget::kGateware:
      sending =
          "Sending the gateware to the device, which erases and writes its "
          "flash as it arrives. It pauses every few seconds while a block is "
          "erased.";
      break;
    case UpdateTarget::kEpcsFactory:
      sending =
          "Sending the factory image to the device, which erases and writes "
          "its flash as it arrives. It pauses every few seconds while a block "
          "is erased.";
      break;
    case UpdateTarget::kFirmware:
      sending =
          "Sending the firmware to the device, which writes it as it arrives.";
      break;
  }

  Report(UpdateStage::kTransferring, target, 0, total, sending);

  if (!device_.Begin(target, total, component.sha256)) {
    outcome.stage = UpdateStage::kFailed;

    // The device refused before a byte moved, and it will have said why.
    const std::optional<DeviceUpdateStatus> refused = device_.ReadStatus();
    outcome.problem =
        refused.has_value() && refused->error != DeviceUpdateError::kNone
            ? DeviceUpdateErrorText(refused->error)
            : std::string("The device would not start the update.");
    return false;
  }

  uint64_t sent = 0;
  uint16_t index = 0;
  while (sent < total) {
    if (Cancelled()) {
      outcome.stage = UpdateStage::kFailed;
      outcome.problem =
          "The update was stopped. Nothing was committed to the device — it "
          "still has the firmware it started with.";
      return false;
    }

    const uint64_t span = std::min(chunk_bytes, total - sent);
    if (!device_.SendChunk(target, index, payload.subspan(sent, span))) {
      outcome.stage = UpdateStage::kFailed;

      const std::optional<DeviceUpdateStatus> refused = device_.ReadStatus();
      outcome.problem =
          refused.has_value() && refused->error != DeviceUpdateError::kNone
              ? DeviceUpdateErrorText(refused->error)
              : std::string(
                    "The device stopped accepting the update. Leave it "
                    "plugged in, then try again.");
      return false;
    }

    sent += span;
    ++index;

    Report(UpdateStage::kTransferring, target, sent, total, sending);
  }

  // The device now hashes the stream it received and, if that matches, reads
  // the whole written region back off the medium and hashes that too. Only
  // then does it write the record that makes the image count — which is why
  // an update interrupted before this point leaves a device that still works
  // or a device in a rescue state, and never one that half-works.
  if (!device_.Finish(target)) {
    outcome.stage = UpdateStage::kFailed;

    // This is where a stream that did not arrive intact is caught — integrity
    // link 5 — so the device's own reason matters more here than anywhere
    // else in the flow, and a generic message would hide the one check that
    // stopped a corrupted image being committed.
    const std::optional<DeviceUpdateStatus> refused = device_.ReadStatus();
    outcome.problem =
        refused.has_value() && refused->error != DeviceUpdateError::kNone
            ? DeviceUpdateErrorText(refused->error)
            : std::string(
                  "The device would not finish the update. Nothing was "
                  "committed.");
    return false;
  }

  if (logger_ != nullptr) {
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                      component_started)
            .count();
    logger_->Debug(
        std::string("Sent the whole ") + UpdateTargetName(target) + ": " +
        std::to_string(index) + " chunks, " + FormatBytes(total) + " in " +
        FormatDuration(seconds) +
        (seconds > 0.0 ? " (" +
                             FormatBytes(static_cast<uint64_t>(
                                 static_cast<double>(total) / seconds)) +
                             "/s)"
                       : std::string()) +
        ". Waiting for the device to finish writing and reading back.");
  }

  // No stage is announced here. Which one comes next is the device's
  // business: both media have already been written as the chunks arrived, so
  // both go on to read back — but the device is the thing that says so, and
  // announcing a stage on its behalf would flash a stage it is not in.
  // AwaitCompletion's first poll reports whatever the device actually says.
  return AwaitCompletion(target, total, outcome);
}

UpdateOutcome UpdateOrchestrator::InstallFactoryGateware(
    const UpdateBundle& bundle) {
  const auto started = std::chrono::steady_clock::now();
  LogPlan("Factory image write", bundle);
  const UpdateOutcome outcome = RunFactoryWrite(bundle);
  LogOutcome("Factory image write", outcome, started);
  return outcome;
}

UpdateOutcome UpdateOrchestrator::RunFactoryWrite(const UpdateBundle& bundle) {
  UpdateOutcome outcome;
  outcome.stage = UpdateStage::kChecking;

  Report(UpdateStage::kChecking, UpdateTarget::kEpcsFactory, 0, 0,
         "Checking the factory image.");

  if (!bundle.manifest.factory_gateware.has_value() ||
      bundle.factory_gateware.empty()) {
    outcome.stage = UpdateStage::kFailed;
    outcome.problem = "This file carries no factory image.";
    return outcome;
  }

  if (!InstallComponent(UpdateTarget::kEpcsFactory,
                        *bundle.manifest.factory_gateware,
                        bundle.factory_gateware, outcome)) {
    return outcome;
  }

  // No reset, no reconfiguration, and no confirmation — deliberately, and not
  // as a convenience to the caller. Reconfiguring would reload the
  // *application* image from flash, which is not what has just been written
  // and, on a board being brought up, is not there at all. What proves this
  // write is the power cycle the wizard asks for and the identity it reads
  // afterwards.
  outcome.succeeded = true;
  outcome.stage = UpdateStage::kComplete;
  outcome.identity_confirmed = false;

  if (logger_ != nullptr) {
    logger_->Info(
        "Factory image written and read back. The board must be power-cycled "
        "to run it.");
  }

  Report(UpdateStage::kComplete, UpdateTarget::kEpcsFactory, 0, 0,
         "Written and checked. The board has not been restarted yet.");
  return outcome;
}

UpdateOutcome UpdateOrchestrator::RunBringUp(const UpdateBundle& bundle) {
  const auto started = std::chrono::steady_clock::now();
  LogPlan("Bring-up write", bundle);
  const UpdateOutcome outcome = RunBringUpWrites(bundle);
  LogOutcome("Bring-up write", outcome, started);
  return outcome;
}

UpdateOutcome UpdateOrchestrator::RunBringUpWrites(const UpdateBundle& bundle) {
  UpdateOutcome outcome;
  outcome.stage = UpdateStage::kChecking;

  Report(UpdateStage::kChecking, UpdateTarget::kFirmware, 0, 0,
         "Checking what is about to be written.");

  // All three, before the first one. See the header: a run that stopped
  // between two of these writes because the third was missing would stop in
  // exactly the state the ordering exists to avoid.
  if (!bundle.manifest.firmware.has_value() || bundle.firmware.empty() ||
      !bundle.manifest.factory_gateware.has_value() ||
      bundle.factory_gateware.empty() ||
      !bundle.manifest.gateware.has_value() || bundle.gateware.empty()) {
    outcome.stage = UpdateStage::kFailed;
    outcome.problem =
        "This file cannot bring a board up: it needs the firmware, the factory "
        "image and the gateware, and it does not carry all three.";
    return outcome;
  }

  if (!InstallComponent(UpdateTarget::kFirmware, *bundle.manifest.firmware,
                        bundle.firmware, outcome)) {
    return outcome;
  }

  if (!InstallComponent(UpdateTarget::kEpcsFactory,
                        *bundle.manifest.factory_gateware,
                        bundle.factory_gateware, outcome)) {
    return outcome;
  }

  if (!InstallComponent(UpdateTarget::kGateware, *bundle.manifest.gateware,
                        bundle.gateware, outcome)) {
    return outcome;
  }

  // No ReconfigureFpga: what the FPGA is running came from a JTAG cable rather
  // than from flash, and reloading it now would replace a working
  // configuration with one whose fate the power cycle decides anyway. No reset
  // either — the jumper may still be fitted, which would land the FX3 back in
  // its boot ROM.
  outcome.succeeded = true;
  outcome.stage = UpdateStage::kComplete;
  outcome.identity_confirmed = false;

  if (logger_ != nullptr) {
    logger_->Info(
        "Bring-up writes complete: firmware, factory image and gateware all "
        "written and read back. The restart and the confirmation are the "
        "caller's.");
  }

  Report(UpdateStage::kComplete, UpdateTarget::kGateware, 0, 0,
         "Written and checked. The board has not been restarted yet.");
  return outcome;
}

UpdateOutcome UpdateOrchestrator::Run(const UpdateBundle& bundle) {
  const auto started = std::chrono::steady_clock::now();
  LogPlan("Update", bundle);
  const UpdateOutcome outcome = RunUpdate(bundle);
  LogOutcome("Update", outcome, started);
  return outcome;
}

UpdateOutcome UpdateOrchestrator::RunUpdate(const UpdateBundle& bundle) {
  UpdateOutcome outcome;
  outcome.stage = UpdateStage::kChecking;

  Report(UpdateStage::kChecking, UpdateTarget::kFirmware, 0, 0,
         "Checking the update.");

  const bool has_firmware =
      bundle.manifest.firmware.has_value() && !bundle.firmware.empty();
  const bool has_gateware =
      bundle.manifest.gateware.has_value() && !bundle.gateware.empty();

  if (!has_firmware && !has_gateware) {
    outcome.stage = UpdateStage::kFailed;
    outcome.problem = "This update file contains nothing to install.";
    return outcome;
  }

  if (has_firmware) {
    if (!InstallComponent(UpdateTarget::kFirmware, *bundle.manifest.firmware,
                          bundle.firmware, outcome)) {
      return outcome;
    }
  }

  if (has_gateware) {
    if (!InstallComponent(UpdateTarget::kGateware, *bundle.manifest.gateware,
                          bundle.gateware, outcome)) {
      return outcome;
    }

    // Reconfiguration stops the clock underneath the capture path, so it is
    // always followed by the reset below rather than leaving the firmware
    // holding a data path whose clock has gone away.
    if (!defer_restart_ && !device_.ReconfigureFpga()) {
      outcome.stage = UpdateStage::kFailed;
      outcome.problem =
          "The device would not reload its gateware. Unplug it and plug it "
          "back in.";
      return outcome;
    }
  }

  // Deferred: the caller owns the restart and the check that follows it. Said
  // in the outcome as well as in the message, because identity_confirmed stays
  // false and a caller that ignored that would be reporting a proof it does
  // not have.
  if (defer_restart_) {
    outcome.succeeded = true;
    outcome.stage = UpdateStage::kComplete;
    outcome.identity_confirmed = false;

    if (logger_ != nullptr) {
      logger_->Info(
          "Device update written and verified; the restart and the "
          "confirmation are the caller's.");
    }

    Report(UpdateStage::kComplete, UpdateTarget::kFirmware, 0, 0,
           "Written and checked. The device has not been restarted yet.");
    return outcome;
  }

  Report(UpdateStage::kRestarting, UpdateTarget::kFirmware, 0, 0,
         "The device will disconnect and reconnect by itself. This is "
         "normal.");

  device_.Reset();

  const std::optional<DeviceIdentity> returned =
      device_.WaitForReturn(timings_.return_timeout);
  if (!returned.has_value()) {
    outcome.stage = UpdateStage::kFailed;
    outcome.problem =
        "The device did not reappear after restarting. Unplug it, plug it "
        "back in, and open this window again to see what it is running.";
    return outcome;
  }

  outcome.identity = *returned;

  Report(UpdateStage::kConfirming, UpdateTarget::kFirmware, 0, 0,
         "Checking what the device is now running.");

  // Integrity link 8, and the difference between an update that was
  // performed and an update that is proved: the identity is read off the
  // live device and compared against what the manifest said it would become.
  outcome.identity_confirmed = true;

  if (has_firmware && !bundle.manifest.firmware->identity.empty()) {
    const std::string& expected = bundle.manifest.firmware->identity;

    // Compared with CommitsMatch rather than as text, because the two sides
    // do not always name a commit at the same length: the firmware asks git
    // for eight characters and a Nix build passes seven. Two builds of one
    // commit must not be reported as differing.
    if (!CommitsMatch(ParseFirmwareCommit(returned->product_string)
                          .value_or(std::string()),
                      expected)) {
      const std::optional<std::string> running =
          ParseFirmwareCommit(returned->product_string);
      outcome.identity_confirmed = false;
      outcome.problem =
          "The update was written and checked, but the device reports "
          "firmware " +
          running.value_or(std::string("it could not name")) + " rather than " +
          expected + ". Try the update again.";
    }
  }

  if (has_gateware && !bundle.manifest.gateware->identity.empty() &&
      !CommitsMatch(returned->gateware_commit,
                    bundle.manifest.gateware->identity)) {
    outcome.identity_confirmed = false;
    outcome.problem =
        "The update was written and checked, but the device reports gateware " +
        (returned->gateware_commit.empty() ? std::string("it could not name")
                                           : returned->gateware_commit) +
        " rather than " + bundle.manifest.gateware->identity +
        ". Try the update again.";
  }

  if (!outcome.identity_confirmed) {
    outcome.stage = UpdateStage::kFailed;
    return outcome;
  }

  outcome.succeeded = true;
  outcome.stage = UpdateStage::kComplete;

  if (logger_ != nullptr) {
    logger_->Info("Device update complete: the device reports " +
                  returned->product_string);
  }

  Report(UpdateStage::kComplete, UpdateTarget::kFirmware, 0, 0,
         "Update complete.");

  return outcome;
}

}  // namespace ddd::capture
