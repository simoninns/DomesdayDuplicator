/************************************************************************

    player_controller.cpp

    The bridge between the GUI and the player link
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "player_controller.h"

#include <QMetaObject>
#include <utility>

namespace ddd::gui {

PlayerController::PlayerController(PlayerBackend backend,
                                   capture::ILogger* logger, QObject* parent)
    : QObject(parent), settings_(LoadPlayerSettings()) {
  qRegisterMetaType<PlayerConnection>();
  qRegisterMetaType<PlayerSettings>();
  qRegisterMetaType<player::PlayerStatus>();

  worker_ = new PlayerWorker(std::move(backend), logger);
  worker_->moveToThread(&thread_);

  // Queued, because the worker lives on the other thread. Everything below
  // this line is the interface thread again.
  connect(worker_, &PlayerWorker::ConnectionChanged, this,
          [this](const PlayerConnection& connection) {
            connection_ = connection;
            emit ConnectionChanged(connection_);
          });

  connect(worker_, &PlayerWorker::StatusUpdated, this,
          [this](const player::PlayerStatus& status) {
            status_ = status;
            emit StatusUpdated(status_);
          });

  connect(worker_, &PlayerWorker::PortRemembered, this,
          [this](const QString& port_path, uint32_t baud_rate) {
            if (settings_.remembered_port == port_path &&
                settings_.remembered_baud == baud_rate) {
              return;
            }

            settings_.remembered_port = port_path;
            settings_.remembered_baud = baud_rate;

            // Saved here rather than in the worker because this is the thread
            // that owns the settings file — and because a worker that wrote to
            // QSettings would be a second writer racing the dialog.
            SavePlayerSettings(settings_);
            emit SettingsChanged(settings_);
          });

  connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
  thread_.start();
}

PlayerController::~PlayerController() {
  // Order matters, and the first line is why the worker has RequestStop() at
  // all: it may be waiting out a thirty-second seek, or halfway through
  // probing every serial port on the machine, and quit() alone would not be
  // seen until that finished.
  worker_->RequestStop();

  QMetaObject::invokeMethod(worker_, &PlayerWorker::Stop, Qt::QueuedConnection);

  thread_.quit();
  thread_.wait();
}

void PlayerController::Start() { ApplySettings(); }

void PlayerController::SetSettings(const PlayerSettings& settings) {
  if (settings == settings_) {
    return;
  }

  settings_ = settings;
  SavePlayerSettings(settings_);
  emit SettingsChanged(settings_);
  ApplySettings();
}

void PlayerController::SetEnabled(bool enabled) {
  if (settings_.enabled == enabled) {
    return;
  }

  PlayerSettings updated = settings_;
  updated.enabled = enabled;
  SetSettings(updated);
}

void PlayerController::SearchNow() {
  QMetaObject::invokeMethod(worker_, &PlayerWorker::SearchNow,
                            Qt::QueuedConnection);
}

void PlayerController::UseConnectedModel() {
  if (connection_.state != PlayerConnectionState::kModelMismatch) {
    return;
  }

  PlayerSettings updated = settings_;
  updated.model_id_code = connection_.model_id_code;
  SetSettings(updated);
}

void PlayerController::SetPaused(bool paused) {
  QMetaObject::invokeMethod(
      worker_, [worker = worker_, paused] { worker->SetPaused(paused); },
      Qt::QueuedConnection);
}

void PlayerController::ApplySettings() {
  // The settings are copied into the invocation rather than referenced, which
  // is what makes this safe to call while the worker is busy: by the time it
  // runs, this object's copy may already have changed again.
  QMetaObject::invokeMethod(
      worker_,
      [worker = worker_, settings = settings_] { worker->Start(settings); },
      Qt::QueuedConnection);
}

}  // namespace ddd::gui
