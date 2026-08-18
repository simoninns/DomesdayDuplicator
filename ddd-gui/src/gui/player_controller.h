/************************************************************************

    player_controller.h

    The bridge between the GUI and the player link
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QObject>
#include <QThread>
#include <cstdint>

#include "player_connection.h"
#include "player_controls.h"
#include "player_metatypes.h"
#include "player_request.h"
#include "player_settings.h"
#include "player_status.h"
#include "player_worker.h"

namespace ddd::capture {
class ILogger;
}

namespace ddd::gui {

// Owns the link on the interface's behalf, and is the only place the two meet.
//
// What CaptureController is to the capture engine, this is to the player: the
// one object where a QObject and a worker thread are in scope together, and the
// one place the interface asks about the player at all.
//
// Nothing here blocks. Every method returns immediately in every state,
// including the states where the worker is halfway through a thirty-second
// seek — the settings are pushed across as a queued invocation and the answers
// come back as signals. That is not a nicety: a window that stopped repainting
// while a player spun up would be indistinguishable from one that had crashed.
//
// The backend is borrowed rather than built, so a test can supply a scripted
// serial port, a list of ports that do not exist, and a clock that makes a
// five-second timeout take microseconds. With that, every connection state
// this class can reach is reachable on a machine with nothing plugged in.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class PlayerController : public QObject {
  Q_OBJECT

 public:
  explicit PlayerController(PlayerBackend backend = {},
                            capture::ILogger* logger = nullptr,
                            QObject* parent = nullptr);
  ~PlayerController() override;

  // Apply the saved settings, which begins a search if player control is on.
  // Separate from the constructor so a caller can connect to the signals first
  // and not miss the first report.
  void Start();

  const PlayerSettings& settings() const { return settings_; }
  const PlayerConnection& connection() const { return connection_; }
  const player::PlayerStatus& status() const { return status_; }

  bool enabled() const { return settings_.enabled; }
  bool connected() const { return connection_.live(); }

  // What the connected player can be asked to do. All false when there is
  // nothing connected, so a caller gating a control on this needs no separate
  // test for whether there is a player.
  const player::PlayerControls& controls() const {
    return connection_.controls;
  }

  // Replace the settings whole, save them, and tell the worker.
  void SetSettings(const PlayerSettings& settings);

  // Ask the player to do something.
  //
  // Returns at once, having queued the request for the worker's thread;
  // whatever the player says arrives later on RequestCompleted, carrying the
  // request back with it. The returned id is that request's, so a caller with
  // more than one thing outstanding can tell the answers apart.
  uint64_t Send(PlayerRequest request);

  // Work out what is in the player.
  //
  // Returns at once. Progress arrives on ExamineProgress and the result on
  // ExamineFinished, which is emitted exactly once per accepted examination —
  // including when it fails, so a dialog waiting for it never waits forever.
  //
  // The scope decides how much is asked, and defaults to everything so that no
  // existing caller changes. See ExamineScope: an identifying pass reads what
  // the disc says about itself in a few seconds and leaves it where it was,
  // where a full one measures the side and takes about a minute.
  void Examine(player::ExamineScope scope = player::ExamineScope::kFull);

  // Stop the examination in progress.
  //
  // Called directly rather than queued, because the worker's event loop is not
  // running while an examination is: a queued call would be delivered after the
  // thing it was meant to stop had finished. See
  // PlayerWorker::RequestExamineCancel.
  void CancelExamine();

 public slots:
  // Turn player control on or off. Off releases the port and stops every
  // enumeration — a machine with this switched off is exactly as it would be if
  // the feature did not exist.
  void SetEnabled(bool enabled);

  // Look now rather than when the backoff next comes round. What somebody
  // wants when they have just switched their player on.
  void SearchNow();

  // Accept the player that answered as the one that was meant. The way out of
  // a model mismatch that does not involve the settings dialog.
  void UseConnectedModel();

  // Stop polling without dropping the link, while a longer sequence owns the
  // session. Phases 4 and 5 are what this is for.
  void SetPaused(bool paused);

 signals:
  void ConnectionChanged(const ddd::gui::PlayerConnection& connection);
  void StatusUpdated(const ddd::player::PlayerStatus& status);
  void SettingsChanged(const ddd::gui::PlayerSettings& settings);
  void RequestCompleted(const ddd::gui::PlayerReply& reply);

  void ExamineProgress(ddd::player::ExamineStage stage, int completed,
                       int total);
  void ExamineFinished(const ddd::player::DiscProfile& disc,
                       ddd::player::ExamineOutcome outcome);

 private:
  void ApplySettings();

  QThread thread_;

  // Lives on thread_, and is deleted by it. Never touched directly from here
  // except through queued invocations and RequestStop().
  PlayerWorker* worker_ = nullptr;

  PlayerSettings settings_;
  PlayerConnection connection_;
  player::PlayerStatus status_;

  // Handed out by Send(). Starts at one so that zero keeps its meaning of
  // "nobody cared which answer was which".
  uint64_t last_request_id_ = 0;
};

}  // namespace ddd::gui
