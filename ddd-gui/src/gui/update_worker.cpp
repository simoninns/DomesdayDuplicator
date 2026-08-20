/************************************************************************

    update_worker.cpp

    Running an update off the interface thread
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "update_worker.h"

#include <string>
#include <utility>

#include "logger.h"
#include "update_bundle.h"

namespace ddd::gui {

UpdateWorker::UpdateWorker(UpdateDevice device, std::vector<uint8_t> archive,
                           capture::UpdateKeyPolicy policy,
                           capture::ILogger* logger, QObject* parent)
    : QObject(parent),
      device_(std::move(device)),
      archive_(std::move(archive)),
      policy_(policy),
      logger_(logger) {}

UpdateWorker::~UpdateWorker() = default;

void UpdateWorker::Cancel() { cancelled_.store(true); }

void UpdateWorker::Run() {
  if (!device_.in_recovery && device_.updater == nullptr) {
    emit Finished(false,
                  tr("The device could not be opened for updating. Unplug it, "
                     "plug it back in, and try again."),
                  capture::DeviceIdentity{});
    return;
  }

  std::string error;
  const std::optional<capture::UpdateBundle> bundle =
      capture::OpenUpdateBundleForPolicy(archive_, policy_, &error);
  if (!bundle.has_value()) {
    // Verified a second time, on this thread, and this is the one place that
    // failure is visible: the page verified the same bytes to decide what to
    // show and would have refused them there, so a bundle that fails here and
    // passed there is a fault worth having in the log by itself.
    if (logger_ != nullptr) {
      logger_->Warning("The update file could not be verified: " + error);
    }
    emit Finished(false, QString::fromStdString(error),
                  capture::DeviceIdentity{});
    return;
  }

  if (logger_ != nullptr) {
    logger_->Debug(std::string("Installing on a device ") +
                   (device_.in_recovery
                        ? "in recovery, which is woken with this file's own "
                          "firmware first"
                        : "that is already running firmware"));
  }

  const auto cancelled = [this] { return cancelled_.load(); };
  const auto report = [this](const capture::UpdateProgress& step) {
    emit Progress(static_cast<int>(step.stage), static_cast<int>(step.target),
                  step.done, step.total, QString::fromStdString(step.message));
  };

  capture::UpdateOutcome outcome;

  if (device_.in_recovery) {
    // The recovery installer's prelude wakes the device with this bundle's
    // own firmware and then runs the identical orchestrator over it, so
    // everything downstream of here — the stages, the progress, the
    // confirmation — is the same in both branches and the page below cannot
    // tell them apart.
    capture::RecoveryInstaller installer(std::move(device_.recovery), logger_);
    installer.SetCancelCallback(cancelled);
    installer.SetProgressCallback(report);
    outcome = installer.Run(*bundle);
  } else {
    capture::UpdateOrchestrator orchestrator(*device_.updater, logger_);
    orchestrator.SetCancelCallback(cancelled);
    orchestrator.SetProgressCallback(report);
    outcome = orchestrator.Run(*bundle);
  }

  emit Finished(outcome.succeeded, QString::fromStdString(outcome.problem),
                outcome.identity);
}

}  // namespace ddd::gui
