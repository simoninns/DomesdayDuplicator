# Development shell for the GUI component.
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

  packages = with pkgs; [
    # Build
    cmake
    ninja
    pkg-config
    qt6.qtbase
    qt6.qtserialport
    qt6.qttools
    libusb1

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
    echo "  cmake --build build        build DomesdayDuplicator, dddutil, dddconv"
    echo "  ctest --test-dir build     run the T1/T2 test suite"
    echo
  '';
}
