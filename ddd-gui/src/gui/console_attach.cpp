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

namespace {

// Whether the caller pointed this stream somewhere of its own — `> file`, or a
// pipe into another command — rather than leaving it to the console.
//
// Asked of the handle rather than of the process, because a windowed
// application started from a command prompt has no console but does have
// whatever handles the prompt passed it: a redirection is set up before the
// process starts, and the C runtime binds the stream to it at startup whatever
// subsystem the executable was linked for.
bool Redirected(HANDLE handle) {
  if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
    return false;
  }
  const DWORD type =
      GetFileType(handle) & ~static_cast<DWORD>(FILE_TYPE_REMOTE);
  return type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE;
}

}  // namespace

bool AttachParentConsole() {
  // A console this process already owns — a developer build linked without
  // WIN32_EXECUTABLE, or a second call — is left alone. Attaching over it
  // fails, and reopening the streams onto it would be pointless work.
  if (GetConsoleWindow() != nullptr) {
    return true;
  }

  // Asked before attaching, because attaching is entitled to replace the
  // process's standard handles with the console's own.
  const bool stdout_redirected = Redirected(GetStdHandle(STD_OUTPUT_HANDLE));
  const bool stderr_redirected = Redirected(GetStdHandle(STD_ERROR_HANDLE));

  if (AttachConsole(ATTACH_PARENT_PROCESS) == 0) {
    return false;
  }

  // The console is attached to the process, but the C runtime's output streams
  // were bound to nothing when it started and stay that way until they are
  // reopened onto the console's own device name. Without this the console
  // exists and every write still goes nowhere.
  //
  // A stream the caller redirected is left exactly as it was found. Reopening
  // that one onto the console would take a script's output away from the file
  // or the pipe it asked for and put it on the screen instead — which is the
  // whole of `ddd-gui --stop-capture > path.txt`, and the one thing this
  // function must not do. Standard input is left alone in every case, because
  // a windowed application reads none.
  const bool have_stdout =
      stdout_redirected || std::freopen("CONOUT$", "w", stdout) != nullptr;
  const bool have_stderr =
      stderr_redirected || std::freopen("CONOUT$", "w", stderr) != nullptr;

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
