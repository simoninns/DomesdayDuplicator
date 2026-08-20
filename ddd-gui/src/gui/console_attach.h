/************************************************************************

    console_attach.h

    Reaching the console that started a windowed application
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

namespace ddd::gui {

// Attaches this process to the console it was started from, so that anything
// written to standard output or standard error is seen there.
//
// This exists for Windows and does nothing anywhere else. The application is
// linked as a GUI-subsystem executable so that starting it from a desktop does
// not also open an empty console window behind it, and the consequence is that
// a build started from a command prompt has no console at all: --version and
// --help wrote into nothing, and so would the console half of the log. Windows
// lets a process borrow its parent's console instead, which is what this does,
// and the C runtime's streams are then pointed at it.
//
// Call it before anything writes to a stream — the first thing main() does.
//
// Two things follow from how Windows works here, rather than from this code.
// The command prompt does not wait for a windowed application, so it prints its
// next prompt straight away and the application's output arrives underneath it;
// and a process started from a desktop shortcut has no parent console to
// borrow, so it goes on having none. Neither is a fault, and neither can be
// fixed from inside the application.
//
// @return True when a console is attached afterwards. False means there was
//         none to attach to, which is the ordinary case for a desktop launch.
bool AttachParentConsole();

}  // namespace ddd::gui
