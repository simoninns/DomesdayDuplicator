/************************************************************************

    console_attach.cpp

    Reaching the console that started a windowed application
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "console_attach.h"

#ifdef _WIN32
#include <windows.h>

#include <cstdio>
#endif

namespace ddd::gui {

#ifdef _WIN32

bool AttachParentConsole() {
  // A console this process already owns — a developer build linked without
  // WIN32_EXECUTABLE, or a second call — is left alone. Attaching over it
  // fails, and reopening the streams onto it would be pointless work.
  if (GetConsoleWindow() != nullptr) {
    return true;
  }

  if (AttachConsole(ATTACH_PARENT_PROCESS) == 0) {
    return false;
  }

  // The console is attached to the process, but the C runtime's output streams
  // were bound to nothing when it started and stay that way until they are
  // reopened onto the console's own device name. Without this the console
  // exists and every write still goes nowhere.
  //
  // Both are attempted whatever the other does. A stream that will not reopen
  // has almost certainly been redirected to a file by the caller, which is a
  // use of the console this does not need to understand and has no business
  // overriding — and standard input is left alone entirely, because a windowed
  // application reads none.
  const bool have_stdout = std::freopen("CONOUT$", "w", stdout) != nullptr;
  const bool have_stderr = std::freopen("CONOUT$", "w", stderr) != nullptr;

  return have_stdout || have_stderr;
}

#else

bool AttachParentConsole() {
  // Every other platform starts a process with its parent's streams already
  // attached, whatever kind of application it is.
  return true;
}

#endif

}  // namespace ddd::gui
