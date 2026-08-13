# Development shell covering both FX3 components — the cross-compiled firmware and the
# host-side programmer.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Where to run it: anywhere in the working tree.
#
#   nix develop .#fx3
#
# There is a single flake.nix at the repository root and a single flake.lock beside it.
# Nix walks up to find them, so `.#fx3` resolves identically from `fx3/` and from the root.
# A bare `nix develop` gives the all-components default shell, not this one, whatever
# directory you happen to be in.
#
# One shell rather than two, because the normal workflow is to build the firmware and then
# immediately load it with the programmer.

{ pkgs }:

pkgs.mkShell {
  name = "ddd-fx3";

  packages = with pkgs; [
    # Cross toolchain for the firmware (ARM926EJ-S, bare metal)
    gcc-arm-embedded

    # fx3-mkimage, the project's ELF-to-boot-image converter. Having it on PATH is what
    # makes firmware/CMakeLists.txt's find_program(FX3_MKIMAGE fx3-mkimage) succeed, so the
    # build skips the fallback that compiles it from source on every fresh build tree.
    (callPackage ./mkimage/package.nix { })

    # Host toolchain for the programmer and mkimage, and for the mkimage fallback
    stdenv.cc
    cmake
    ninja
    pkg-config
    libusb1

    # generate-descriptor.sh runs at firmware configure time and shells out to python3
    python3

    # Test
    gtest

    # Editor tooling. fx3/firmware/.clangd pins Compiler: arm-none-eabi-gcc so clangd does
    # not analyse the freestanding ARM sources against the host libc.
    clang-tools
    cmake-language-server
    gdb
  ];

  shellHook = ''
    echo "Domesday Duplicator — FX3 development shell"
    echo
    echo "  firmware:"
    echo "    cmake -B firmware/build -S firmware \\"
    echo "          -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake"
    echo "    cmake --build firmware/build"
    echo "    ctest --test-dir firmware/build"
    echo
    echo "  mkimage (host tool, builds the boot image from the ELF):"
    echo "    cmake -B mkimage/build -S mkimage"
    echo "    cmake --build mkimage/build"
    echo "    ctest --test-dir mkimage/build"
    echo
    echo "  programmer:"
    echo "    cmake -B programmer/build -S programmer"
    echo "    cmake --build programmer/build"
    echo "    ctest --test-dir programmer/build"
    echo
  '';
}
