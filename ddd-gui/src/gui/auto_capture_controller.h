/************************************************************************

    auto_capture_controller.h

    Where the player and the capture engine meet
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QObject>
#include <QString>
#include <cstdint>
#include <memory>

#include "auto_capture_plan.h"
#include "auto_capture_sequence.h"
#include "capture_provenance.h"
#include "disc_profile.h"
#include "player_metatypes.h"
#include "player_request.h"
#include "player_settings.h"
#include "player_status.h"

class QTimer;

namespace ddd::capture {
class ILogger;
}

namespace ddd::gui {

class CaptureController;
class PlayerController;

// The one object that holds both controllers, and the only one that does.
//
// Everything about "the disc is playing" and "the capture is running" being
// connected lives here. That is worth a class of its own rather than a few
// connections in the main window: the coupling is where the surprising failures
// are — a capture truncated by a player that stumbled, a player left spinning
// after a capture ended, a file that outlives the sequence that opened it — and
// they are only testable if there is one place they happen.
//
// It does two things:
//
//   It runs an automatic capture, by driving AutoCaptureSequence one step at a
//   time. The player steps go through PlayerController and come back as
//   signals; the two capture steps are direct calls. Nothing blocks, so the
//   window keeps painting through a forty-minute side and Stop is answered
//   between one step and the next.
//
//   It stops the capture when the player stops, if that is asked for — and only
//   after the player has said so several times running. See
//   PlayerSettings::stop_capture_with_player for why that debounce is not
//   optional.
//
// The coupling runs in **one direction only**: the player may stop the capture,
// and the capture may never stop the player. Outside an automatic capture this
// object sends the player nothing at all — see OnCapturingChanged, where the
// old application's opposite preference is not carried over and why.
//
// **The sequence is driven from this thread, not the worker's.** That is the
// opposite of the examine sequence, and for a good reason: an examination is a
// run of blocking exchanges that the session already knows how to make, while
// this one interleaves player commands with attaching and detaching a writer —
// which is the interface thread's to do. A step per queued round trip costs
// nothing here, because the watch polls twice a second rather than as fast as
// the link will go.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class AutoCaptureController : public QObject {
  Q_OBJECT

 public:
  // Either controller may be null, and the tests pass null deliberately: the
  // coupling then does nothing rather than crashing, which is also what the
  // application does with player control switched off.
  AutoCaptureController(PlayerController* player, CaptureController* capture,
                        capture::ILogger* logger = nullptr,
                        QObject* parent = nullptr);
  ~AutoCaptureController() override;

  // Only the coupling preference is read. Passed whole so that there is one
  // settings value in the application rather than a copy of one field.
  void SetSettings(const PlayerSettings& settings);
  const PlayerSettings& settings() const { return settings_; }

  bool running() const { return sequence_ != nullptr; }

  // The two controllers, borrowed. Exposed so that the guided setup can apply
  // the capture name and the coupling preference to the settings they belong
  // to rather than being handed three pointers to keep in step.
  PlayerController* player() const { return player_; }
  CaptureController* capture() const { return capture_; }

  // What the run in progress was asked to do.
  const player::AutoCapturePlan& plan() const { return plan_; }

  // How many readings of a stopped player it takes before a capture is stopped
  // for it.
  //
  // Three, at four readings a second: about three-quarters of a second of the
  // player insisting it has stopped. Long enough that the momentary stop a disc
  // defect produces does not truncate a good capture, short enough that a
  // deliberate stop is acted on while somebody is still watching.
  static constexpr int kStoppedReadings = 3;

 public slots:
  // Begin an automatic capture. Does nothing when one is already running, when
  // there is no player, or when the plan could not be run against the disc —
  // the guided setup enables its button on the same function, so the last of
  // those is a guard rather than a path.
  void Start(const ddd::player::AutoCapturePlan& plan,
             const ddd::player::DiscProfile& disc);

  // Stop the run. The capture is finalised and the player put back rather than
  // the run being abandoned where it stood.
  void Cancel();

 signals:
  void RunningChanged(bool running);

  // Where the run is. `address` is where the player was last seen, or -1 before
  // the watch has read one — which is most of the setting-up.
  void Progress(ddd::player::AutoCaptureStage stage, int address);

  void Finished(ddd::player::AutoCaptureOutcome outcome);

 private:
  // Pull the next step and carry it out. The whole of the run's control flow.
  void Step();

  // Hand a result back to the sequence and go round again.
  void Apply(const player::StepResult& result);

  void OnReply(const PlayerReply& reply);
  void OnCapturingChanged(bool capturing, const QString& path);
  void OnStatusUpdated(const player::PlayerStatus& status);

  // Tell the capture engine which player it is beside, so a capture's metadata
  // names it. Follows the link rather than the run: a capture taken by hand
  // with a player connected is still a capture off that player.
  void OnConnectionChanged(const PlayerConnection& connection);

  void FinishRun();

  // What the file should say about the disc it came from.
  capture::DiscProvenance DescribeDisc(const player::DiscProfile& disc) const;

  void Log(const QString& message) const;

  PlayerController* player_ = nullptr;
  CaptureController* capture_ = nullptr;
  capture::ILogger* logger_ = nullptr;

  PlayerSettings settings_;

  std::unique_ptr<player::AutoCaptureSequence> sequence_;
  player::AutoCapturePlan plan_;

  // The request the sequence is waiting on. Matched on the way back so that a
  // reply to something else — the remote is still usable during a capture —
  // cannot be read as this step's answer.
  uint64_t pending_request_ = 0;

  // Paces the watch, and is why a step can carry a delay at all.
  QTimer* timer_ = nullptr;

  // Whether the step in hand has already had its delay waited out. The sequence
  // hands out the same step until it is answered, so without this the watch
  // would rearm its own timer for ever and never actually poll.
  bool delayed_ = false;

  // Consecutive readings of a player that is not spinning. Reset by anything
  // else, which is what makes it a debounce rather than a count.
  int stopped_readings_ = 0;
};

}  // namespace ddd::gui
