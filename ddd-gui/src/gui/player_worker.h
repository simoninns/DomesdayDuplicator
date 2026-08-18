/************************************************************************

    player_worker.h

    The thread that owns the serial link
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// Included rather than forward declared: LogLevel is an enumeration, and the
// worker's own logging helper takes one.
#include "logger.h"
#include "player_connection.h"
#include "player_metatypes.h"
#include "player_request.h"
#include "player_session.h"
#include "player_settings.h"
#include "player_status.h"
#include "serial_port.h"
#include "serial_port_scanner.h"

class QTimer;

namespace ddd::gui {

// What the worker talks to.
//
// Borrowed rather than built, exactly as CaptureController borrows its USB
// backend: with these three supplied, a test drives the whole connection state
// machine — searching, connecting, mismatching, losing the link — against a
// scripted fake port on a machine with nothing attached, and with a clock that
// makes a five-second timeout take microseconds.
struct PlayerBackend {
  // A fresh serial port. Null means a real QtSerialPort.
  std::function<std::unique_ptr<player::ISerialPort>()> make_port;

  // What ports exist. Null means the system's own list.
  std::function<std::vector<SerialPortCandidate>()> list_ports;

  // Where "now" comes from inside the protocol. Null means the steady clock.
  player::PlayerSession::Clock clock;
};

// The serial link, on a thread of its own.
//
// It has to be off the interface thread, and the numbers say why: a command is
// allowed five seconds and a seek thirty, a full search is four baud rates
// times three attempts on every serial port the machine has, and at 1200 baud a
// status poll alone is the better part of a second. None of that can happen
// between two paint events.
//
// The worker does three things and nothing else: it looks for a player, it
// polls the one it found, and it says what happened. Commands from the remote
// arrive as queued invocations and are executed here too, so the session is
// only ever touched by this thread.
//
// Thread-affinity: constructed on the interface thread, moved to a thread of
// its own, and every slot runs there. RequestStop() is the exception and may be
// called from either — see ISerialPort::RequestAbort for why one has to exist.
class PlayerWorker : public QObject {
  Q_OBJECT

 public:
  PlayerWorker(PlayerBackend backend, capture::ILogger* logger,
               QObject* parent = nullptr);
  ~PlayerWorker() override;

  // Abandon whatever is in flight and stop looking. Callable from any thread,
  // and the reason the application can quit during a thirty-second seek.
  void RequestStop();

  // Give up on the examination in progress. **Callable from any thread**, and
  // it has to be: while Examine() is running, this thread's event loop is not,
  // so a queued slot would not be delivered until the sequence it is meant to
  // cancel had already finished.
  //
  // Takes effect between steps rather than during one, so a cancel pressed
  // during a seek is honoured when that seek answers — up to the long timeout
  // away in the worst case. The alternative is ISerialPort::RequestAbort, and
  // that is deliberately not used here: an abandoned read is reported as a port
  // failure, so cancelling an examination would drop the link and cost a
  // rediscovery. Waiting out one command is much the smaller price.
  void RequestExamineCancel();

  // The floor and ceiling on how often the player is asked what it is doing.
  //
  // The interval is the time the last poll took, bounded by these — so the link
  // is never more than about half busy with polling, whatever its speed. At
  // 9600 baud that settles around four readings a second, which looks live; at
  // 1200 it backs off on its own to about one, which is all the link can carry.
  static constexpr std::chrono::milliseconds kMinimumPollInterval{250};
  static constexpr std::chrono::milliseconds kMaximumPollInterval{2000};

  // How often the disc status is asked for, in polls. The disc does not change
  // while it is spinning, and asking every time would spend a third of the link
  // on an answer that is already known.
  static constexpr int kDiscStatusEveryPolls = 8;

 public slots:
  // Begin, or apply changed settings. Disabled settings stop everything and
  // release the port.
  void Start(const ddd::gui::PlayerSettings& settings);

  // Drop the link and stop looking.
  void Stop();

  // Search now, whatever the backoff was about to do. What the user gets when
  // they have just turned their player on and would rather not wait.
  void SearchNow();

  // Stop polling without dropping the link, while a multi-step sequence owns
  // the session. Interleaving a status query into the middle of a seek is how a
  // reply gets attributed to the wrong command.
  void SetPaused(bool paused);

  // Do one thing the interface asked for, and say what came back.
  //
  // Arrives as a queued invocation, so it runs between polls rather than during
  // one: the session is only ever touched by this thread, and that — not a lock
  // — is what stops a reply being attributed to the wrong command.
  void Send(const ddd::gui::PlayerRequest& request);

  // Work out what is in the player, and say so.
  //
  // Runs the whole examine sequence here rather than handing its steps back
  // across the thread boundary one at a time, and that is the right side of the
  // line for it: every step is a blocking exchange the session already knows
  // how to make, and driving it from the interface would be a round trip per
  // step for no gain. What crosses the boundary is progress and a profile.
  //
  // Polling is suspended for the duration. Interleaving a status query into the
  // middle of a seek is how a reply gets attributed to the wrong command, and
  // an examination is nothing but seeks.
  //
  // The scope decides how much is asked; see ExamineScope. An identifying pass
  // has no seeks in it at all, but the poll is suspended just the same — the
  // reason is about one reply reaching the wrong reader, not about how long the
  // sequence takes.
  void Examine(
      ddd::player::ExamineScope scope = ddd::player::ExamineScope::kFull);

 signals:
  void ConnectionChanged(const ddd::gui::PlayerConnection& connection);
  void StatusUpdated(const ddd::player::PlayerStatus& status);
  void RequestCompleted(const ddd::gui::PlayerReply& reply);

  // One per step, before it is sent. `completed` and `total` are the step
  // counts, so a progress bar has both without having to know the sequence.
  void ExamineProgress(ddd::player::ExamineStage stage, int completed,
                       int total);

  void ExamineFinished(const ddd::player::DiscProfile& disc,
                       ddd::player::ExamineOutcome outcome);

  // A player was found here. The controller writes it into the settings, so
  // the next run costs one probe rather than a scan of every port.
  void PortRemembered(const QString& port_path, uint32_t baud_rate);

 private:
  void Tick();

  // One pass over the planned ports. Returns having either connected or
  // reported why not.
  void AttemptDiscovery();

  // One reading of the player.
  void Poll();

  // Give up on the link and schedule another search.
  void ReportLinkLost();

  void Publish(const PlayerConnection& connection);
  void ScheduleSearch();
  void SchedulePoll(std::chrono::milliseconds last_poll);

  // Fill in the parts of a connection report that come from the session.
  PlayerConnection DescribeConnection() const;

  void Log(capture::LogLevel level, const QString& message) const;

  PlayerBackend backend_;
  capture::ILogger* logger_ = nullptr;

  std::unique_ptr<player::ISerialPort> port_;
  std::unique_ptr<player::PlayerSession> session_;

  QTimer* timer_ = nullptr;

  PlayerSettings settings_;
  PlayerConnection connection_;

  bool running_ = false;
  bool paused_ = false;

  // Consecutive failed searches, which is what the retry delay grows on.
  int failures_ = 0;

  int polls_since_disc_status_ = 0;
  player::DiscType last_disc_type_ = player::DiscType::kUnknown;

  // Read by the discovery loop between probes and by the port's own wait, so
  // that a stop during a scan of every port on the machine takes milliseconds
  // rather than the rest of the scan.
  std::atomic<bool> stopping_{false};

  // Set from the interface thread, read between examine steps. See
  // RequestExamineCancel().
  std::atomic<bool> examine_cancelled_{false};

  // True while Examine() is on the stack, so a second one cannot start on top
  // of the first. Only this thread touches it.
  bool examining_ = false;
};

}  // namespace ddd::gui
