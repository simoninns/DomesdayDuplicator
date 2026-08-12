# The default development shell: everything free, across every component.
#
#   nix develop
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
      # Host C/C++ toolchain — the GUI, the programmer and elf2img
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

      # generate-descriptor.sh
      python3

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
    echo "  nix develop .#gui         Qt 6 capture GUI and tools"
    echo "  nix develop .#fx3         FX3 firmware and programmer"
    echo "  nix develop .#fpga        Verilog lint and simulation (no Quartus)"
    echo "  nix develop .#hardware    KiCad"
    echo
    echo "  nix build .#gui .#fx3-programmer"
    echo "  nix flake check           build everything and run the T1-T4 tests"
    echo
    echo "See docs-tech/editor-setup.md for editor configuration."
    echo
  '';
}
