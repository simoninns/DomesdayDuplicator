/************************************************************************

    bringup_worker.cpp

    Running one half of a bring-up off the interface thread
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "bringup_worker.h"

#include <optional>
#include <string>
#include <utility>

#include "update_bundle.h"

namespace ddd::gui {

BringUpWorker::BringUpWorker(capture::BringUpOrchestrator* orchestrator,
                             Task task, std::vector<uint8_t> archive,
                             capture::UpdateKeyPolicy policy, QObject* parent)
    : QObject(parent),
      orchestrator_(orchestrator),
      task_(task),
      archive_(std::move(archive)),
      policy_(policy) {}

BringUpWorker::~BringUpWorker() = default;

void BringUpWorker::Cancel() { cancelled_.store(true); }

void BringUpWorker::Run() {
  if (orchestrator_ == nullptr) {
    emit Finished(false, false, tr("This build cannot program a board."));
    return;
  }

  std::string error;
  const std::optional<capture::UpdateBundle> bundle =
      capture::OpenUpdateBundleForPolicy(archive_, policy_, &error);
  if (!bundle.has_value()) {
    emit Finished(false, false, QString::fromStdString(error));
    return;
  }

  orchestrator_->SetCancelCallback([this] { return cancelled_.load(); });
  orchestrator_->SetProgressCallback([this](
                                         const capture::UpdateProgress& step) {
    emit Progress(static_cast<int>(step.stage), static_cast<int>(step.target),
                  step.done, step.total, QString::fromStdString(step.message));
  });

  if (task_ == Task::kConfigure) {
    const capture::BringUpConfigureOutcome outcome =
        orchestrator_->ConfigureFpga(*bundle);
    emit Finished(outcome.succeeded, outcome.stopped,
                  QString::fromStdString(outcome.problem));
    return;
  }

  const capture::UpdateOutcome outcome = orchestrator_->ProgramDevice(*bundle);

  // The update path has no "stopped" of its own — a cancellation arrives as a
  // failure with a sentence about it — so the one thing this side knows that
  // the outcome does not is asked here: was a stop requested? A user who
  // pressed Stop is not shown a failure.
  emit Finished(outcome.succeeded, !outcome.succeeded && cancelled_.load(),
                QString::fromStdString(outcome.problem));
}

}  // namespace ddd::gui
