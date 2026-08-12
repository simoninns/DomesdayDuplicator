# The default development shell: everything free, across every component.
#
# Where to run it: anywhere in the working tree.
#
#   nix develop
#
# There is a single flake.nix at the repository root and a single flake.lock beside it —
# components carry package.nix and shell.nix but no flakes of their own, so there is one
# pinned nixpkgs for the whole repository however you enter it. Nix walks up to find the
# root flake, so both `nix develop` and `nix develop .#gui` work from any subdirectory.
#
# Use this to move between components without switching shells. It deliberately excludes
# Quartus (unfree, multi-gigabyte, x86_64-linux only) and KiCad (large, and only needed for
# board work) — reach for `nix develop .#fpga` plus Phase 6's Quartus shell, or
# `nix develop .#hardware`, when you need those.

{ pkgs }:

pkgs.mkShell {
  name = "ddd";

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

      # FX3 firmware cross toolchain
      gcc-arm-embedded

      # generate-descriptor.sh, plus the documentation site toolchain
      (python312.withPackages (ps: [
        ps.mkdocs
        ps.mkdocs-material
        ps.mkdocs-awesome-nav
      ]))

      # Test
      gtest

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
    echo "  nix develop .#gui         Qt 6 capture GUI and tools"
    echo "  nix develop .#fx3         FX3 firmware and programmer"
    echo "  nix develop .#fpga        Verilog lint and simulation (no Quartus)"
    echo "  nix develop .#hardware    KiCad"
    echo "  nix develop .#docs        MkDocs documentation site"
    echo
    echo "  nix build .#gui .#fx3-programmer .#docs-site"
    echo "  nix flake check           build everything and run the T1-T4 tests"
    echo
    echo "See docs-tech/editor-setup.md for editor configuration."
    echo
  '';
}
