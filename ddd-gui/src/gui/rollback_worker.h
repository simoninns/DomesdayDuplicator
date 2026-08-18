/************************************************************************

    rollback_worker.h

    Running one half of a rollback off the interface thread
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <cstdint>
#include <vector>

#include "capture_metatypes.h"
#include "rollback_orchestrator.h"
#include "update_key.h"

namespace ddd::gui {

// One half of a rollback, on a thread of its own. BringUpWorker's counterpart,
// and deliberately its shape: two halves, one orchestrator the wizard keeps
// across them, the bundle re-opened here rather than the window's already
// opened one being handed over.
//
// The orchestrator is not owned here for the same reason it is not owned
// there: the two halves are separate runs against one object, and that object
// is what carries the ordering rule from the first to the second.
//
// Thread-affinity: constructed on the interface thread, moved to a worker
// thread, and Run() is invoked there. Cancel() may be called from either.
class RollbackWorker : public QObject {
  Q_OBJECT

 public:
  enum class Task {
    // The FPGA's configuration flash, at the factory address. First, always.
    kGateware,

    // The FX3's EEPROM, by an ordinary transfer. Ends without a restart.
    kFirmware,
  };

  RollbackWorker(capture::RollbackOrchestrator* orchestrator, Task task,
                 std::vector<uint8_t> archive, capture::UpdateKeyPolicy policy,
                 QObject* parent = nullptr);
  ~RollbackWorker() override;

  // Ask the run to stop at the next safe point. Callable from any thread.
  void Cancel();

 public slots:
  void Run();

 signals:
  // Ints for the reason BringUpWorker sends ints: a queued connection carrying
  // an engine enum would need it registered with the meta-object system, and
  // there is nothing here a plain int does not say.
  void Progress(int stage, int target, quint64 done, quint64 total,
                const QString& message);

  void Finished(bool succeeded, bool stopped, const QString& problem);

 private:
  capture::RollbackOrchestrator* orchestrator_ = nullptr;
  Task task_ = Task::kGateware;
  std::vector<uint8_t> archive_;
  capture::UpdateKeyPolicy policy_;
  std::atomic<bool> cancelled_{false};
};

}  // namespace ddd::gui
