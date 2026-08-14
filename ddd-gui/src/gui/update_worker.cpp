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

#include "update_bundle.h"

namespace ddd::gui {

UpdateWorker::UpdateWorker(std::unique_ptr<capture::IDeviceUpdater> updater,
                           std::vector<uint8_t> archive,
                           capture::UpdateKeyPolicy policy, QObject* parent)
    : QObject(parent),
      updater_(std::move(updater)),
      archive_(std::move(archive)),
      policy_(policy) {}

UpdateWorker::~UpdateWorker() = default;

void UpdateWorker::Cancel() { cancelled_.store(true); }

void UpdateWorker::Run() {
  if (updater_ == nullptr) {
    emit Finished(false,
                  tr("The device could not be opened for updating. Unplug it, "
                     "plug it back in, and try again."),
                  QString(), QString());
    return;
  }

  std::string error;
  const std::optional<capture::UpdateBundle> bundle =
      capture::OpenUpdateBundleForPolicy(archive_, policy_, &error);
  if (!bundle.has_value()) {
    emit Finished(false, QString::fromStdString(error), QString(), QString());
    return;
  }

  capture::UpdateOrchestrator orchestrator(*updater_, nullptr);

  orchestrator.SetCancelCallback([this] { return cancelled_.load(); });

  orchestrator.SetProgressCallback([this](const capture::UpdateProgress& step) {
    emit Progress(static_cast<int>(step.stage), step.done, step.total,
                  QString::fromStdString(step.message));
  });

  const capture::UpdateOutcome outcome = orchestrator.Run(*bundle);

  emit Finished(outcome.succeeded, QString::fromStdString(outcome.problem),
                QString::fromStdString(outcome.identity.product_string),
                QString::fromStdString(outcome.identity.gateware_commit));
}

}  // namespace ddd::gui
