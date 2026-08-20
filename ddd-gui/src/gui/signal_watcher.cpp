/************************************************************************

    signal_watcher.cpp

    Ctrl+C and kill, delivered somewhere a capture can be finished properly
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "signal_watcher.h"

#include <QSocketNotifier>

#ifndef Q_OS_WIN
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#endif

namespace ddd::gui {
namespace {

#ifndef Q_OS_WIN

// The write end of the socketpair, where the handler can reach it.
//
// A file-scope variable rather than anything belonging to the object, because
// the handler runs with no context of its own: it is handed a signal number and
// nothing else, and sig_atomic_t is the only kind of variable the standard says
// it may read.
volatile std::sig_atomic_t g_write_descriptor = -1;

// The one watcher, so that a second Install() can refuse rather than quietly
// take the descriptor above away from the first.
SignalWatcher* g_watcher = nullptr;

extern "C" void HandleInterrupt(int /*number*/) {
  const int descriptor = g_write_descriptor;
  if (descriptor < 0) {
    return;
  }

  // errno belongs to whoever was interrupted. A handler that returns having
  // changed it turns an unrelated call's success into a spurious failure — and
  // this one runs in the middle of a capture, where the calls being interrupted
  // are the ones writing the file.
  const int saved_errno = errno;

  const char byte = 1;
  const ssize_t written = ::write(descriptor, &byte, 1);

  // Nothing can be done about a write that failed from here, and there is
  // nowhere to report it to: the descriptor is a socketpair that only fills if
  // the notifier has stopped reading, which is a stopped event loop, which is a
  // process that is already leaving.
  static_cast<void>(written);

  errno = saved_errno;
}

bool InstallHandler(int number) {
  struct sigaction action = {};
  action.sa_handler = HandleInterrupt;
  sigemptyset(&action.sa_mask);

  // SA_RESTART so that a read or a write the interrupt lands in the middle of
  // resumes instead of failing with EINTR. The point of this class is that an
  // interrupt does not disturb a capture until the event loop chooses to act on
  // it, and a transfer that failed on the way in would disturb it.
  action.sa_flags = SA_RESTART;

  return ::sigaction(number, &action, nullptr) == 0;
}

void RestoreHandler(int number) {
  struct sigaction action = {};
  action.sa_handler = SIG_DFL;
  sigemptyset(&action.sa_mask);
  static_cast<void>(::sigaction(number, &action, nullptr));
}

#endif  // !Q_OS_WIN

}  // namespace

SignalWatcher::SignalWatcher(int read_descriptor, int write_descriptor,
                             QObject* parent)
    : QObject(parent),
      read_descriptor_(read_descriptor),
      write_descriptor_(write_descriptor),
      notifier_(
          new QSocketNotifier(read_descriptor, QSocketNotifier::Read, this)) {
  connect(notifier_, &QSocketNotifier::activated, this,
          &SignalWatcher::OnActivated);
}

SignalWatcher::~SignalWatcher() {
#ifndef Q_OS_WIN
  // The handlers first: after this nothing can write to the descriptor being
  // closed below, which is the only ordering that matters here.
  RestoreHandler(SIGINT);
  RestoreHandler(SIGTERM);
  g_write_descriptor = -1;
  g_watcher = nullptr;

  delete notifier_;
  notifier_ = nullptr;

  if (read_descriptor_ >= 0) {
    ::close(read_descriptor_);
  }
  if (write_descriptor_ >= 0) {
    ::close(write_descriptor_);
  }
#endif
}

SignalWatcher* SignalWatcher::Install(QObject* parent) {
#ifdef Q_OS_WIN
  // No POSIX signals, and a GUI-subsystem executable has no console to be
  // interrupted from in the first place. A Windows script stops a capture by
  // running the application again with --stop-capture, which works everywhere
  // and is what the documentation tells everyone to use.
  Q_UNUSED(parent);
  return nullptr;
#else
  if (g_watcher != nullptr) {
    return nullptr;
  }

  int descriptors[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) != 0) {
    return nullptr;
  }

  g_write_descriptor = descriptors[1];

  if (!InstallHandler(SIGINT) || !InstallHandler(SIGTERM)) {
    RestoreHandler(SIGINT);
    RestoreHandler(SIGTERM);
    g_write_descriptor = -1;
    ::close(descriptors[0]);
    ::close(descriptors[1]);
    return nullptr;
  }

  auto* watcher = new SignalWatcher(descriptors[0], descriptors[1], parent);
  g_watcher = watcher;
  return watcher;
#endif
}

bool SignalWatcher::installed() {
#ifdef Q_OS_WIN
  return false;
#else
  return g_watcher != nullptr;
#endif
}

void SignalWatcher::OnActivated() {
#ifndef Q_OS_WIN
  // Drained rather than read one byte at a time, so that several interrupts
  // arriving together leave nothing behind to fire the notifier again for an
  // instruction already acted on.
  char buffer[16] = {};
  const ssize_t read_bytes = ::read(read_descriptor_, buffer, sizeof(buffer));
  static_cast<void>(read_bytes);
#endif

  emit Interrupted();
}

}  // namespace ddd::gui
