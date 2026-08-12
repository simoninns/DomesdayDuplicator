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
  # The commit these binaries were built from. It reaches --version and the About dialog
  # of all three tools, which is the only way a released GUI artefact can be traced back
  # to its source (D21). There is no .git in a Nix sandbox, so CMake's own git fallback
  # cannot fire and this must be passed — the flake passes self.shortRev. A build that
  # reports "unknown" fails P7-9's release gate.
  dddVersion ? "unknown",
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
    (lib.cmakeFeature "DDD_VERSION" dddVersion)
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

  # D21 end to end: ask each installed binary what it is. This runs after fixupPhase, so
  # it exercises the wrapped executables users actually get, and it fails the build rather
  # than shipping an artefact that cannot be traced to a commit.
  doInstallCheck = true;

  installCheckPhase = ''
    runHook preInstallCheck

    export QT_QPA_PLATFORM=offscreen
    for tool in DomesdayDuplicator dddconv dddutil; do
      reported=$("$out/bin/$tool" --version)
      echo "$tool: $reported"
      case "$reported" in
        *"(${dddVersion})"*) ;;
        *)
          echo "$tool reports '$reported', which does not carry the build version '${dddVersion}'" >&2
          exit 1
          ;;
      esac
    done

    runHook postInstallCheck
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
