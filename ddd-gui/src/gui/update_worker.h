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

#include "device_recovery.h"
#include "device_updater.h"
#include "update_key.h"
#include "update_orchestrator.h"

namespace ddd::gui {

// The device an update is going to, in one of the two forms it can take.
//
// A device running its own firmware is handed over already open, because
// opening it is how the page finds out whether it can be updated at all. A
// device with no firmware cannot be: the updater it will eventually be driven
// through belongs to a device that does not exist yet, so what is handed over
// is the pair of factories that will make one.
struct UpdateDevice {
  std::unique_ptr<capture::IDeviceUpdater> updater;
  capture::DeviceAccess recovery;

  // Whether this is the recovery form. Explicit rather than inferred from
  // which member is set, so a half-filled structure fails loudly instead of
  // quietly taking the other path.
  bool in_recovery = false;
};

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
  UpdateWorker(UpdateDevice device, std::vector<uint8_t> archive,
               capture::UpdateKeyPolicy policy, QObject* parent = nullptr);
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
  UpdateDevice device_;
  std::vector<uint8_t> archive_;
  capture::UpdateKeyPolicy policy_;
  std::atomic<bool> cancelled_{false};
};

}  // namespace ddd::gui
