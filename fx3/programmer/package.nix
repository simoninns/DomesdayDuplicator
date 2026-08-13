# fx3-programmer: libusb host tool for loading firmware onto a Cypress FX3.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

{
  lib,
  stdenv,
  cmake,
  pkg-config,
  libusb1,
  gtest,
  doCheck ? true,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "domesday-duplicator-fx3-programmer";
  version = "1.0";

  src = lib.fileset.toSource {
    root = ./.;
    fileset = lib.fileset.unions [
      ./CMakeLists.txt
      ./src
      ./tests
      ./configs
      # The Cypress secondary loader. Permanent (EEPROM/SPI) programming pushes this into
      # FX3 RAM first, so without it the packaged binary can only do RAM downloads.
      ./cyfxflashprog.img
    ];
  };

  nativeBuildInputs = [
    cmake
    pkg-config
  ];

  buildInputs = [ libusb1 ];

  checkInputs = [ gtest ];

  cmakeFlags = [
    (lib.cmakeBool "BUILD_TESTING" finalAttrs.doCheck)
  ];

  inherit doCheck;

  checkPhase = ''
    runHook preCheck
    ctest --output-on-failure --label-exclude 'hil'
    runHook postCheck
  '';

  # CMake installs the rule to $out/lib/udev/rules.d, which is where services.udev.packages
  # looks. nix/modules/udev.nix wires it up; see also passthru.udevRules below.
  postInstall = ''
    if [ ! -f "$out/share/domesday-duplicator/cyfxflashprog.img" ]; then
      echo "cyfxflashprog.img was not installed — EEPROM programming would fail at runtime" >&2
      exit 1
    fi
    # The filename matters, not just the presence: a uaccess rule sorting after
    # 73-seat-late.rules never has its tag consumed. See the comment in the rule itself.
    if [ ! -f "$out/lib/udev/rules.d/70-domesday-duplicator.rules" ]; then
      echo "70-domesday-duplicator.rules was not installed — the NixOS module would silently do nothing" >&2
      exit 1
    fi
  '';

  meta = {
    description = "Command-line programmer for the Cypress FX3 USB 3.0 controller";
    longDescription = ''
      A minimal libusb-based tool for loading firmware onto Cypress FX3 devices, derived
      from Cypress' cyusb_linux with the Qt GUI and FX2 support removed. Supports RAM
      download, I2C EEPROM and SPI flash programming.

      Permanent programming needs the Cypress secondary loader, cyfxflashprog.img, which
      this package installs to share/domesday-duplicator/ and the binary locates through a
      compiled-in path — so it works from any working directory.

      NixOS users should enable hardware.domesdayDuplicator.enable from the repository's
      nixosModules.udev to get the device permissions.
    '';
    homepage = "https://github.com/simoninns/DomesdayDuplicator";
    # The tool derives from cyusb_linux (LGPL-2.1) and ships in a GPLv3 project.
    # fx3/programmer/VENDOR.md records the analysis.
    license = lib.licenses.gpl3Plus;
    mainProgram = "fx3-programmer";
    platforms = lib.platforms.linux;
  };
})
