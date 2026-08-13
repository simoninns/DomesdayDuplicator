# Development shell for the capture application.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Where to run it: anywhere in the working tree.
#
#   nix develop .#ddd-gui
#
# There is a single flake.nix at the repository root and a single flake.lock beside it.
# Nix walks up to find them, so `.#ddd-gui` resolves identically from here and from the
# root. A bare `nix develop` gives the all-components default shell, not this one.
#
# Carries the build dependencies plus the editor tooling, so a contributor gets working
# completion and diagnostics from the shell rather than from a per-developer install.
# clang-tools is not optional here: the build runs clang-format and clang-tidy as gates,
# and without them in the shell the gates silently do not run.

{ pkgs }:

pkgs.mkShell {
  name = "ddd-gui";

  # Qt's code generators call setlocale(LC_ALL, "") and require the result to be UTF-8,
  # warning on every invocation when it is not. A desktop session that exports region
  # variables without a codeset makes that call fail outright, so the tools land on
  # C/ANSI_X3.4-1968 and print warnings on every build. Pinning the shell to C.UTF-8
  # sidesteps the whole class of it, and is identical on every developer's machine.
  LC_ALL = "C.UTF-8";

  packages = with pkgs; [
    # Build
    cmake
    ninja
    pkg-config
    qt6.qtbase

    # The capture engine's non-Qt dependencies. libFLAC is BSD-3-Clause and libusb is
    # LGPL-2.1-or-later, so linking either into a GPLv3 application is fine
    # (AGENTS.md §10). Not needed on Windows, where the WinUSB backend uses libraries
    # that ship with the system.
    flac
    libusb1

    # Test
    gtest

    # Quality gates and editor tooling — clangd reads build/compile_commands.json, which
    # ddd-gui/.clangd points at
    clang-tools
    cmake-language-server
    gdb
  ];

  # qtbase's setup hook exports the plugin path; without a display, tests and application
  # runs need the offscreen platform. Unset QT_QPA_PLATFORM to run the real application.
  shellHook = ''
    echo "Domesday Duplicator — capture application development shell"
    echo
    echo "  cmake -B build -S .        configure (writes build/compile_commands.json)"
    echo "  cmake --build build        build ddd-gui, with the format and lint gates"
    echo "  ctest --test-dir build     run the T1 test suite"
    echo
  '';
}
