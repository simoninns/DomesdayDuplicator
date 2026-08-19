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

// How many times the vectors are played before a failure is a failure.
//
// Two rather than more: one retry covers the transient that a bench has
// actually seen, and a third would only postpone telling a user about a cable
// or a board that is genuinely wrong.
constexpr int kConfigureAttempts = 2;

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

BringUpConfigureOutcome BringUpOrchestrator::PlayVectors(std::string_view text,
                                                         int attempt,
                                                         bool& cable_opened) {
  BringUpConfigureOutcome outcome;
  cable_opened = false;

  // Opened per attempt rather than per call, because re-opening is half of
  // what a second attempt is worth: it resets the FT245 and empties both its
  // buffers, so an attempt that failed because the cable's answers had got
  // out of step does not hand that state to the next one.
  std::string problem;
  const std::unique_ptr<IJtagCable> cable = access_.open_cable(&problem);
  if (cable == nullptr) {
    outcome.problem = problem.empty()
                          ? std::string("The USB-Blaster could not be opened.")
                          : problem;
    return outcome;
  }
  cable_opened = true;

  SvfPlayer player(*cable, logger_);

  // Bytes of the file, straight through to the caller's bar. The player counts
  // in the one unit that is known before the run starts, so the proportion is
  // honest from the first update rather than after a guess.
  //
  // A second attempt says that it is one. The bar counts bytes of the file,
  // so starting it over sends it back to nothing, and a bar that rewinds
  // without a word about why reads as something going wrong rather than as
  // the step doing what a user would otherwise have done by hand.
  const std::string message =
      attempt > 1
          ? std::string(
                "Loading the gateware into the FPGA through the USB-Blaster, "
                "on a second attempt after the first did not take. Nothing is "
                "written to the board by this step.")
          : std::string(
                "Loading the gateware into the FPGA through the USB-Blaster. "
                "Nothing is written to the board by this step.");

  if (progress_) {
    player.SetProgressCallback([this, message](size_t done, size_t total) {
      UpdateProgress progress;
      progress.stage = UpdateStage::kWriting;
      progress.target = UpdateTarget::kEpcsFactory;
      progress.done = done;
      progress.total = total;
      progress.message = message;
      progress_(progress);
    });
  }
  if (cancel_) {
    player.SetStopCallback(cancel_);
  }

  outcome.play = player.Play(text);
  outcome.succeeded = outcome.play.succeeded;
  outcome.stopped = outcome.play.stopped;

  if (!outcome.succeeded) {
    // The player's own words, which name both values on a mismatch, and after
    // them where in the file to look. The sentence is not wrapped or
    // rephrased: it was written for a user, and the place it stopped is the
    // one thing it cannot know about itself.
    outcome.problem = outcome.play.problem;
    if (outcome.play.line > 0 && !outcome.stopped) {
      // Written as a label rather than as prose, because no article reads
      // right in front of every SVF keyword there is — "an SDR" and "a
      // STATE" — and because this half is what gets copied into a report.
      outcome.problem +=
          " (Programming file line " + std::to_string(outcome.play.line);
      if (!outcome.play.statement_keyword.empty()) {
        outcome.problem += ", " + outcome.play.statement_keyword;
      }
      outcome.problem += ".)";
    }
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

  const std::string_view text(
      reinterpret_cast<const char*>(bundle.provisioning.data()),
      bundle.provisioning.size());

  // Played again if the first attempt fails, and this is the one step of a
  // bring-up where that is allowed.
  //
  // What makes it safe is what makes this half different from every other:
  // nothing is written. The vectors put the factory image into the FPGA's
  // configuration memory and stop, so an attempt that fails part way leaves
  // the board exactly as it was, and playing the file again from the
  // beginning is what the wizard already tells a user to do by hand. It takes
  // about three seconds.
  //
  // Why it is worth doing for them: 5.7 Mbit go through a full-speed
  // bit-banged link, and the comparison at the end of the file is there to
  // catch a configuration that did not take. A single caught one is not a
  // fault to report — it is the check working, and the answer to it is
  // another attempt (TESTING.md, B-V1, 2026-08-19).
  BringUpConfigureOutcome outcome;
  for (int attempt = 1; attempt <= kConfigureAttempts; ++attempt) {
    bool cable_opened = false;
    outcome = PlayVectors(text, attempt, cable_opened);
    outcome.attempts = attempt;

    if (outcome.succeeded || outcome.stopped) {
      break;
    }

    // A cable that could not be opened is not a transient. Trying again would
    // only say the same sentence twice as slowly, and the sentence — no
    // cable, the wrong cable, jtagd holding it — is already the one a user
    // has to act on.
    if (!cable_opened) {
      break;
    }

    if (attempt < kConfigureAttempts && logger_ != nullptr) {
      logger_->Warning(
          "Loading the gateware failed and is being tried once more; nothing "
          "has been written to the board. The attempt that failed said: " +
          outcome.problem);
    }
  }

  if (!outcome.succeeded) {
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
