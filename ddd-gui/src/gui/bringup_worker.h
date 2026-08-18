/************************************************************************

    bringup_worker.h

    Running one half of a bring-up off the interface thread
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

#include "bringup_orchestrator.h"
#include "capture_metatypes.h"
#include "update_key.h"

namespace ddd::gui {

// One half of a bring-up, on a thread of its own.
//
// Off the interface thread for the reason UpdateWorker is: writing an EEPROM
// and two flash images is minutes of blocking control transfers, and a window
// that stopped repainting for that long is a window somebody force-quits
// during the one operation where force-quitting is the worst thing they could
// do.
//
// **The orchestrator is not owned here.** The two halves are separated by a
// page, so they are two runs of this worker against one orchestrator that the
// wizard keeps — which is what carries the ordering rule across the gap
// between them. The wizard guarantees the orchestrator outlives every worker
// it starts, and that only one runs at a time.
//
// The bundle's bytes are re-opened here rather than the wizard's already
// opened bundle being handed over, again as UpdateWorker does: the signature
// and digest work then happens off the interface thread, and no link in the
// chain trusts the previous link's verification.
//
// Thread-affinity: constructed on the interface thread, moved to a worker
// thread, and Run() is invoked there. Cancel() may be called from either.
class BringUpWorker : public QObject {
  Q_OBJECT

 public:
  enum class Task {
    // The FPGA, through the USB-Blaster. Volatile: it writes nothing.
    kConfigure,

    // Everything permanent — the FX3's EEPROM and both images in the FPGA's
    // flash — through the firmware the boot ROM is handed. Ends without a
    // restart, because the wizard owns the power cycle.
    kProgram,
  };

  BringUpWorker(capture::BringUpOrchestrator* orchestrator, Task task,
                std::vector<uint8_t> archive, capture::UpdateKeyPolicy policy,
                QObject* parent = nullptr);
  ~BringUpWorker() override;

  // Ask the run to stop at the next safe point. Callable from any thread.
  void Cancel();

 public slots:
  // The whole of one half. Emits Progress as it goes and Finished exactly
  // once.
  void Run();

 signals:
  // `stage` is a capture::UpdateStage and `target` a capture::UpdateTarget,
  // sent as ints for the reason UpdateWorker sends them as ints: a queued
  // connection carrying an engine enum would need it registered with the
  // meta-object system, and there is nothing here a plain int does not say.
  void Progress(int stage, int target, quint64 done, quint64 total,
                const QString& message);

  // Exactly once, whatever happened. `stopped` distinguishes a run somebody
  // asked to stop from one that went wrong, because nothing is wrong with the
  // first and the pages say so differently.
  void Finished(bool succeeded, bool stopped, const QString& problem);

 private:
  capture::BringUpOrchestrator* orchestrator_ = nullptr;
  Task task_ = Task::kConfigure;
  std::vector<uint8_t> archive_;
  capture::UpdateKeyPolicy policy_;
  std::atomic<bool> cancelled_{false};
};

}  // namespace ddd::gui
