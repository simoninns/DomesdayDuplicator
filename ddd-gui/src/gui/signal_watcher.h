/************************************************************************

    signal_watcher.h

    Ctrl+C and kill, delivered somewhere a capture can be finished properly
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QObject>

class QSocketNotifier;

namespace ddd::gui {

// Turns SIGINT and SIGTERM into a signal on the event loop.
//
// A headless capture is stopped by the script that started it, and the two ways
// a script does that are Ctrl+C and kill. Neither can be handled where it
// arrives: a signal handler may call almost nothing — not new, not a QObject's
// method, not a FLAC encoder's finaliser — and the default disposition of both
// is to end the process immediately, which leaves the last block unwritten and
// the sidecar never written at all.
//
// So the handler does the one thing it is allowed to do: write a byte down a
// socketpair. The read end is watched by a QSocketNotifier, so the interrupt
// arrives as an ordinary queued event on the event loop, in a context where
// stopping the capture and waiting for the file to close is just code.
//
// One at a time. The handler needs a descriptor it can see without touching
// anything of the object's, so it is held in a file-scope variable, and a
// second watcher would overwrite the first one's. Install() refuses instead.
class SignalWatcher : public QObject {
  Q_OBJECT

 public:
  ~SignalWatcher() override;

  SignalWatcher(const SignalWatcher&) = delete;
  SignalWatcher& operator=(const SignalWatcher&) = delete;
  SignalWatcher(SignalWatcher&&) = delete;
  SignalWatcher& operator=(SignalWatcher&&) = delete;

  // Install the handlers. Returns the watcher, or nullptr when there is nothing
  // to install: on Windows, which has no POSIX signals — a Windows script stops
  // a capture with --stop-capture instead — and when a watcher already exists.
  //
  // Ownership follows the parent, as it does for any QObject. Pass one, or
  // delete it yourself; destroying it puts the previous disposition back, so a
  // second interrupt after that ends the process as it normally would.
  static SignalWatcher* Install(QObject* parent = nullptr);

  // Whether a watcher is installed. Nothing in the application asks — the tests
  // do, because "it was cleaned up" is otherwise unobservable.
  static bool installed();

 signals:
  // An interrupt arrived. Emitted on the thread the watcher was made on.
  //
  // Once per arrival rather than once per signal: two interrupts close enough
  // together are one byte as far as the notifier is concerned. That is the
  // right way round — the first one already means "stop", and the second is a
  // user who thinks nothing happened rather than a different instruction.
  void Interrupted();

 private:
  SignalWatcher(int read_descriptor, int write_descriptor, QObject* parent);

  void OnActivated();

  int read_descriptor_ = -1;
  int write_descriptor_ = -1;
  QSocketNotifier* notifier_ = nullptr;
};

}  // namespace ddd::gui
