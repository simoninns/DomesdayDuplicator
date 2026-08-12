# Domesday Duplicator capture GUI and companion tools.
#
# Call with qt6Packages.callPackage, not plain callPackage — the Qt package set supplies
# wrapQtAppsHook and the matching qtbase/qtserialport.

{
  lib,
  stdenv,
  cmake,
  pkg-config,
  qt6,
  libusb1,
  wrapQtAppsHook,
  gtest,
  # Set false to skip the test suite (it needs an offscreen Qt platform plugin)
  doCheck ? true,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "domesday-duplicator-gui";
  version = "1.0";

  # Only the files the build actually reads. Without this, editing gui/README.md or
  # gui/BUILD.md invalidates the derivation and forces a full Qt rebuild.
  src = lib.fileset.toSource {
    root = ./.;
    fileset = lib.fileset.unions [
      ./CMakeLists.txt
      ./cmake
      ./src
      ./tests
    ];
  };

  nativeBuildInputs = [
    cmake
    pkg-config
    qt6.qttools
    wrapQtAppsHook
  ];

  buildInputs = [
    qt6.qtbase
    qt6.qtserialport
    libusb1
  ];

  checkInputs = [ gtest ];

  cmakeFlags = [
    (lib.cmakeBool "BUILD_TESTING" finalAttrs.doCheck)
  ];

  inherit doCheck;

  # The widget tests instantiate QWidgets, which needs a platform plugin. There is no display
  # in the build sandbox, so use Qt's offscreen one.
  preCheck = ''
    export QT_QPA_PLATFORM=offscreen
  '';

  checkPhase = ''
    runHook preCheck
    ctest --output-on-failure --label-exclude 'hil'
    runHook postCheck
  '';

  meta = {
    description = "Capture GUI and tools for the Domesday Duplicator";
    longDescription = ''
      The Domesday Duplicator is a LaserDisc-focused RF capture device sampling at
      40 Msps with 10-bit resolution over USB 3.0. This package provides the capture
      application, the dddutil analysis utility and the dddconv command-line converter.
    '';
    homepage = "https://github.com/simoninns/DomesdayDuplicator";
    license = lib.licenses.gpl3Plus;
    mainProgram = "DomesdayDuplicator";
    platforms = lib.platforms.unix;
  };
})
