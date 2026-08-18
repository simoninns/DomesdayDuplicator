# The default development shell: everything free, across every component.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Where to run it: anywhere in the working tree.
#
#   nix develop
#
# There is a single flake.nix at the repository root and a single flake.lock beside it —
# components carry package.nix and shell.nix but no flakes of their own, so there is one
# pinned nixpkgs for the whole repository however you enter it. Nix walks up to find the
# root flake, so both `nix develop` and `nix develop .#ddd-gui` work from any subdirectory.
#
# Use this to move between components without switching shells. It deliberately excludes
# Quartus (unfree, multi-gigabyte, x86_64-linux only) and KiCad (large, and only needed for
# board work) — reach for `nix develop .#fpga` plus Phase 6's Quartus shell, or
# `nix develop .#hardware`, when you need those.

{ pkgs }:

pkgs.mkShell {
  name = "ddd";

  # Same reasoning as ddd-gui/shell.nix: Qt's moc/uic/rcc warn on every invocation unless the
  # locale is UTF-8, and a session exporting codeset-less region variables (GNOME's
  # LC_TIME=en_GB and friends) leaves setlocale failing back to C/ANSI_X3.4-1968. This shell
  # builds the GUI too, so it pins the same locale.
  LC_ALL = "C.UTF-8";

  packages =
    with pkgs;
    [
      # Host C/C++ toolchain — the GUI, the programmer and fx3-mkimage
      cmake
      ninja
      pkg-config
      gnumake

      # GUI
      qt6.qtbase
      qt6.qtserialport
      qt6.qttools
      libusb1

      # The .ldf capture output. Both are needed for the same reason ddd-gui/package.nix gives:
      # without them the GUI does not configure in this shell at all, so the one component
      # the shell exists to develop could only be built through the packaged derivation.
      flac
      libogg

      # FX3 firmware cross toolchain
      gcc-arm-embedded

      # generate-descriptor.sh, plus the documentation site toolchain
      (python3.withPackages (ps: [
        ps.mkdocs
        ps.mkdocs-material
        ps.mkdocs-awesome-nav
      ]))

      # Test
      gtest

      # Signing and verifying update bundles — tools/make-update-bundle.sh and
      # tools/dev-bundle.sh both need it, and so does the update-bundle check.
      minisign

      # Gateware: lint and simulate without Quartus
      verible
      verilator
      iverilog

      # Editor tooling, so an editor with an LSP client works in every component
      clang-tools
      cmake-language-server
      nixd
      nixfmt

      # General
      gdb
      git
    ]
    ++ lib.optionals stdenv.isLinux [
      # gdbserver and friends; also keeps the shell usable for the udev-facing work
      usbutils
    ];

  shellHook = ''
    echo "Domesday Duplicator — development shell (all free components)"
    echo
    echo "Component shells (run from anywhere in the working tree):"
    echo
    echo "  nix develop .#ddd-gui     Qt 6 capture application and tools"
    echo "  nix develop .#fx3         FX3 firmware and programmer"
    echo "  nix develop .#fpga        Verilog lint and simulation (no Quartus)"
    echo "  nix develop .#hardware    KiCad"
    echo "  nix develop .#docs        MkDocs documentation site"
    echo
    echo "  ./tools/dev-bundle.sh     package what is built locally as an update bundle"
    echo
    echo "  nix build .#ddd-gui .#fx3-programmer .#docs-site"
    echo "  nix flake check           build everything and run the T1-T4 tests"
    echo
    echo "Editor configuration: https://simoninns.github.io/DomesdayDuplicator/development/editor-setup/"
    echo
  '';
}
