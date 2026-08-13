# Development shell for the GUI component.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Where to run it: anywhere in the working tree.
#
#   nix develop .#gui
#
# There is a single flake.nix at the repository root and a single flake.lock beside it.
# Nix walks up to find them, so `.#gui` resolves identically from `gui/` and from the root.
# A bare `nix develop` gives the all-components default shell, not this one, whatever
# directory you happen to be in.
#
# Carries the build dependencies plus the editor tooling, so a contributor gets working
# completion and diagnostics from the shell rather than from a per-developer install.

{ pkgs }:

pkgs.mkShell {
  name = "ddd-gui";

  # Qt's code generators (moc, uic, rcc) call setlocale(LC_ALL, "") and require the result to
  # be UTF-8, warning on every invocation when it is not. A desktop session that exports
  # region variables without a codeset — GNOME emits LC_TIME=en_GB, LC_NUMERIC=en_GB and
  # friends from its Formats setting — makes that call fail outright unless the bare name was
  # generated, so the tools land on C/ANSI_X3.4-1968 and print four lines of warning per
  # build. Pinning the shell to C.UTF-8 sidesteps the whole class of it: always present in
  # glibc, UTF-8 by construction, and identical on every developer's machine, which is what a
  # build shell wants anyway. Nothing here formats output for a human reader.
  LC_ALL = "C.UTF-8";

  packages = with pkgs; [
    # Build
    cmake
    ninja
    pkg-config
    qt6.qtbase
    qt6.qtserialport
    qt6.qttools
    libusb1

    # The .ldf capture output (P7-21). 1.5 is what the packaging paths should install —
    # multithreaded encoding arrived there — but the build works against 1.4 as well.
    #
    # libogg is listed explicitly because flac.pc has `Requires: ogg`, and nixpkgs does not
    # propagate it. Without it pkg-config resolves nothing and the build silently falls
    # through to flac's CMake config, which works but hides the missing dependency until a
    # platform that only ships the .pc file fails.
    flac
    libogg

    # Test
    gtest

    # Editor tooling — clangd reads build/compile_commands.json, which gui/.clangd points at
    clang-tools
    cmake-language-server
    gdb
  ];

  # qtbase's setup hook exports the plugin path; without a display, tests and GUI runs need
  # the offscreen platform. Unset QT_QPA_PLATFORM to run the real application.
  shellHook = ''
    echo "Domesday Duplicator — GUI development shell"
    echo
    echo "  cmake -B build -S .        configure (writes build/compile_commands.json)"
    echo "  cmake --build build        build DomesdayDuplicator"
    echo "  ctest --test-dir build     run the T1/T2 test suite"
    echo
  '';
}
