# Development shell covering both FX3 components — the cross-compiled firmware and the
# host-side programmer.
#
#   nix develop ./fx3        (or `nix develop .#fx3` from the repository root)
#
# One shell rather than two, because the normal workflow is to build the firmware and then
# immediately load it with the programmer.

{ pkgs }:

pkgs.mkShell {
  name = "ddd-fx3";

  packages = with pkgs; [
    # Cross toolchain for the firmware (ARM926EJ-S, bare metal)
    gcc-arm-embedded

    # Host toolchain for the programmer and for elf2img
    stdenv.cc
    cmake
    ninja
    pkg-config
    libusb1

    # elf2img needs python3 only for generate-descriptor.sh
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
    echo
    echo "  programmer:"
    echo "    cmake -B programmer/build -S programmer"
    echo "    cmake --build programmer/build"
    echo "    ctest --test-dir programmer/build"
    echo
  '';
}
