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

// Turns an interrupt into a signal on the event loop.
//
// A headless capture is stopped by the script that started it, and every way a
// script does that ends the process where it stands unless something catches
// it — leaving the last block unwritten and the sidecar never written at all.
// What arrives differs by platform; what has to happen does not.
//
// **On Unix** it is SIGINT and SIGTERM, and a signal handler may call almost
// nothing: not new, not a QObject's method, not a FLAC encoder's finaliser. So
// the handler does the one thing it is allowed to do — write a byte down a
// socketpair. The read end is watched by a QSocketNotifier, so the interrupt
// arrives as an ordinary queued event on the event loop, in a context where
// stopping the capture and waiting for the file to close is just code.
//
// **On Windows** it is a console control event: Ctrl+C, Ctrl+Break, or the
// CTRL_BREAK_EVENT a script sends with GenerateConsoleCtrlEvent. There are no
// POSIX signals to catch, and until this existed the default handler ended the
// process — which a headless run reaches through AttachConsole, so pressing
// Ctrl+C at the prompt that started a capture truncated its file. The handler
// runs on a thread Windows makes for it rather than in the middle of the
// program, so it may allocate and it may post; posting is what it does, and
// the interrupt arrives on the event loop exactly as it does on Unix.
//
// One at a time, on both. The Unix handler needs a descriptor it can see
// without touching anything of the object's, so it is held in a file-scope
// variable, and a second watcher would overwrite the first one's. Install()
// refuses instead.
class SignalWatcher : public QObject {
  Q_OBJECT

 public:
  ~SignalWatcher() override;

  SignalWatcher(const SignalWatcher&) = delete;
  SignalWatcher& operator=(const SignalWatcher&) = delete;
  SignalWatcher(SignalWatcher&&) = delete;
  SignalWatcher& operator=(SignalWatcher&&) = delete;

  // Install the handlers. Returns the watcher, or nullptr when there is
  // nothing to install: when one already exists, or when the platform refused
  // to take the handler. A caller carries on without one — a capture can still
  // be stopped with --stop-capture, and on Windows that remains the way a
  // script running outside the capture's own console stops it.
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

 private slots:
  // The interrupt, arriving on the event loop. A slot rather than a plain
  // method because the Windows handler reaches it by name from the thread
  // Windows called it on, which is the whole of how it crosses over.
  void Deliver();

 private:
  SignalWatcher(int read_descriptor, int write_descriptor, QObject* parent);

  void OnActivated();

  // Both -1 on Windows, where there is no socketpair and no notifier: a
  // console handler can post for itself.
  int read_descriptor_ = -1;
  int write_descriptor_ = -1;
  QSocketNotifier* notifier_ = nullptr;
};

}  // namespace ddd::gui
