# fx3-mkimage: builds the FX3 boot image from the firmware ELF.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# A host tool, never cross-compiled: the firmware build needs to *run* it.
# CMAKE_TOOLCHAIN_FILE applies to a whole build tree, which is why this is its
# own derivation rather than a target inside fx3/firmware.
#
# It replaces the Cypress SDK's elf2img. See README.md for the format reference
# and for the byte-identity check against the tool it replaced.

{
  lib,
  stdenv,
  cmake,
  gtest,
  # Stamped into --version, same convention as the firmware and the GUI.
  dddVersion ? "unknown",
  doCheck ? true,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "fx3-mkimage";
  version = "1.0";

  src = lib.fileset.toSource {
    root = ./.;
    fileset = lib.fileset.unions [
      ./CMakeLists.txt
      ./src
      ./tests
    ];
  };

  nativeBuildInputs = [ cmake ];

  checkInputs = [ gtest ];

  cmakeFlags = [
    (lib.cmakeBool "BUILD_TESTING" finalAttrs.doCheck)
    (lib.cmakeFeature "DDD_VERSION" dddVersion)
  ];

  inherit doCheck;

  checkPhase = ''
    runHook preCheck
    ctest --output-on-failure --label-exclude 'hil'
    runHook postCheck
  '';

  meta = {
    description = "Builds an FX3 boot image from a firmware ELF";
    longDescription = ''
      Converts the linked firmware ELF into the boot-loadable image format the
      Cypress FX3 bootloader expects, as specified in Infineon application note
      AN76405 section 4.4.

      This is a from-scratch implementation written against that public
      specification. It replaces the proprietary elf2img utility that used to be
      vendored with the FX3 SDK, and it produces byte-identical output.
    '';
    homepage = "https://github.com/simoninns/DomesdayDuplicator";
    license = lib.licenses.gpl3Plus;
    mainProgram = "fx3-mkimage";
    platforms = lib.platforms.unix;
  };
})
