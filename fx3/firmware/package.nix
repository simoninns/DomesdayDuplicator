# Domesday Duplicator FX3 firmware — bare-metal ARM926EJ-S, cross-compiled.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Output layout is deliberately flat:
#
#   $out/firmware.img   the boot-loadable image fx3-programmer writes to the device
#   $out/firmware.elf   the linked ELF, for debugging
#   $out/firmware.map   the linker map, which is how a dead-code claim was settled
#
# so `nix build .#fx3-firmware && ls result/` is the whole story and CI can upload the
# directory as-is. The CMake project installs to `bin/` by default for non-Nix builders;
# FIRMWARE_INSTALL_DIR is what redirects it here.

{
  lib,
  stdenv,
  cmake,
  gcc-arm-embedded,
  python3,
  fx3-mkimage,
  # The commit this firmware was built from. It reaches the USB product descriptor, so
  # `lsusb -v` on a running device reports it — that is the whole traceability story for
  # the firmware. The flake passes self.shortRev; a release build that let this fall
  # back to "unknown" would be untraceable, which the release workflow gates on.
  firmwareVersion ? "unknown",
  doCheck ? true,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "domesday-duplicator-fx3-firmware";
  version = firmwareVersion;

  # Only what the build reads. README.md and .clangd edits must not trigger a rebuild.
  src = lib.fileset.toSource {
    root = ./.;
    fileset = lib.fileset.unions [
      ./CMakeLists.txt
      ./arm-none-eabi-toolchain.cmake
      ./generate-descriptor.sh
      ./src
      ./tests
    ];
  };

  # The vendored SDK is a sibling directory, so it cannot be part of the fileset above.
  # Narrowed to the three subtrees CMakeLists.txt asserts on, which keeps the store path
  # small and stops a README edit in fx3/sdk/ from rebuilding the firmware.
  cyfx3sdk = lib.fileset.toSource {
    root = ../sdk;
    fileset = lib.fileset.unions [
      ../sdk/fw_lib/1_3_5/inc
      ../sdk/fw_lib/1_3_5/fx3_release
      ../sdk/fw_build/fx3_fw/fx3.ld
    ];
  };

  nativeBuildInputs = [
    cmake
    gcc-arm-embedded
    fx3-mkimage
    # generate-descriptor.sh runs at configure time and shells out to python3.
    python3
  ];

  cmakeFlags = [
    "-DCMAKE_TOOLCHAIN_FILE=${./arm-none-eabi-toolchain.cmake}"

    # The toolchain file locates the cross compiler with find_program, which is a no-op
    # when the variable is already in the cache — and nixpkgs' cmake setup hook puts the
    # *host* gcc there before our flags are seen. Left alone, the configure picks up host
    # gcc for C (while still finding arm-none-eabi-gcc for ASM, which is how the mismatch
    # announces itself) and fails. Naming the cross tools outright also removes the
    # PATH lookup, so the derivation depends on this exact toolchain and not on whatever
    # arm-none-eabi-gcc happens to be first.
    (lib.cmakeFeature "CMAKE_C_COMPILER" "${gcc-arm-embedded}/bin/arm-none-eabi-gcc")
    (lib.cmakeFeature "CMAKE_CXX_COMPILER" "${gcc-arm-embedded}/bin/arm-none-eabi-g++")
    (lib.cmakeFeature "CMAKE_ASM_COMPILER" "${gcc-arm-embedded}/bin/arm-none-eabi-gcc")
    # Same story for the binutils: the setup hook points these at the host gcc-wrapper, and
    # the toolchain file's CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY makes the compiler
    # check archive ARM objects with them.
    (lib.cmakeFeature "CMAKE_AR" "${gcc-arm-embedded}/bin/arm-none-eabi-ar")
    (lib.cmakeFeature "CMAKE_RANLIB" "${gcc-arm-embedded}/bin/arm-none-eabi-ranlib")
    (lib.cmakeFeature "CMAKE_STRIP" "${gcc-arm-embedded}/bin/arm-none-eabi-strip")

    (lib.cmakeFeature "FIRMWARE_VERSION" firmwareVersion)
    (lib.cmakeFeature "CYFX3SDK_PATH" "${finalAttrs.cyfx3sdk}")
    (lib.cmakeFeature "FIRMWARE_INSTALL_DIR" ".")
    (lib.cmakeBool "BUILD_TESTING" finalAttrs.doCheck)
  ];

  # A freestanding target links with -nostartfiles against its own linker script. nixpkgs'
  # default hardening flags assume a hosted libc and a wrapped compiler, and neither holds
  # here — arm-none-eabi-gcc is found by the toolchain file, not through the cc-wrapper.
  hardeningDisable = [ "all" ];

  # Cross-compiled ARM artefacts. Stripping them would throw away the debug information
  # that makes firmware.elf worth shipping, and patchelf has no business near a bare-metal
  # image with no dynamic section.
  dontStrip = true;
  dontPatchELF = true;

  inherit doCheck;

  checkPhase = ''
    runHook preCheck
    ctest --output-on-failure --label-exclude 'hil'
    runHook postCheck
  '';

  postInstall = ''
    for f in firmware.img firmware.elf firmware.map; do
      if [ ! -f "$out/$f" ]; then
        echo "$f was not installed — the build produced an incomplete firmware set" >&2
        exit 1
      fi
    done

    # The descriptor is the only place the commit reaches the device, so a silent fallback
    # to "unknown" would produce an image that cannot be traced to its source.
    if ! grep -q 'Commit: ${firmwareVersion}' generated_descriptor_data.h; then
      echo "the generated descriptor does not carry version '${firmwareVersion}'" >&2
      exit 1
    fi
  '';

  meta = {
    description = "FX3 USB 3.0 controller firmware for the Domesday Duplicator";
    longDescription = ''
      Bare-metal ARM926EJ-S firmware for the Cypress FX3 on the Domesday Duplicator,
      streaming 10-bit samples at 40 Msps over USB 3.0. Build it and write it to a device
      with fx3-programmer.

      The output is a directory, not a program: firmware.img is the boot-loadable image,
      firmware.elf and firmware.map are for debugging. The USB product descriptor carries
      the commit the image was built from, so `lsusb -v` identifies a running device.
    '';
    homepage = "https://github.com/simoninns/DomesdayDuplicator";
    # The firmware sources are GPLv3; they link against the vendored Cypress SDK, whose
    # licensing is recorded in fx3/sdk/README.md.
    license = lib.licenses.gpl3Plus;
    platforms = lib.platforms.unix;
  };
})
