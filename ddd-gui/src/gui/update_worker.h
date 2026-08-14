/************************************************************************

    update_worker.h

    Running an update off the interface thread
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "device_updater.h"
#include "update_key.h"
#include "update_orchestrator.h"

namespace ddd::gui {

// An update on a thread of its own.
//
// It has to be off the interface thread: an update is minutes of blocking
// USB control transfers, and a window that stopped repainting for that long
// would be a window a user force-quits — during the one operation where
// force-quitting is the worst thing they could do.
//
// The worker owns the bundle's bytes and re-opens the bundle itself rather
// than being handed one the page already opened. Two reasons, and the second
// is the real one. The signature and digest checks then happen off the
// interface thread as well. And no link in the chain trusts the previous
// link's verification — the page verifies to decide what to show, and this
// verifies to decide what to install, and those are different decisions made
// from the same bytes.
//
// Thread-affinity: constructed on the interface thread, moved to a worker
// thread, and Run() is invoked there. Cancel() is the one method that may be
// called from either.
class UpdateWorker : public QObject {
  Q_OBJECT

 public:
  UpdateWorker(std::unique_ptr<capture::IDeviceUpdater> updater,
               std::vector<uint8_t> archive, capture::UpdateKeyPolicy policy,
               QObject* parent = nullptr);
  ~UpdateWorker() override;

  // Ask the update to stop at the next safe point, which is any point before
  // the commit. Callable from any thread.
  void Cancel();

 public slots:
  // The whole update. Emits Progress as it goes and Finished exactly once.
  void Run();

 signals:
  // `stage` is a capture::UpdateStage. Sent as an int because a queued
  // connection carrying an engine enum would need it registered with the
  // meta-object system, and there is nothing here that a plain int does not
  // say.
  void Progress(int stage, quint64 done, quint64 total, const QString& message);

  // Exactly once, whatever happened. `problem` is empty on success.
  void Finished(bool succeeded, const QString& problem,
                const QString& product_string, const QString& gateware_commit);

 private:
  std::unique_ptr<capture::IDeviceUpdater> updater_;
  std::vector<uint8_t> archive_;
  capture::UpdateKeyPolicy policy_;
  std::atomic<bool> cancelled_{false};
};

}  // namespace ddd::gui
