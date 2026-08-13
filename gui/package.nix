# Domesday Duplicator capture application.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
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
  flac,
  libogg,
  wrapQtAppsHook,
  gtest,
  # The commit this binary was built from. It reaches --version and the About dialog,
  # which is the only way a released GUI artefact can be traced back to its source (D21).
  # There is no .git in a Nix sandbox, so CMake's own git fallback cannot fire and this
  # must be passed — the flake passes self.shortRev. A build that reports "unknown" fails
  # P7-9's release gate.
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
      # The .desktop and AppStream files the install step places (P7-13). Only the assets
      # are included, not the whole packaging directory: the workflows and manifests under
      # it change often and would otherwise invalidate the derivation on every edit.
      ./packaging/assets
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

    # The .ldf capture output (P7-21). libogg is listed explicitly because flac.pc declares
    # `Requires: ogg` and nixpkgs does not propagate it, so without it pkg-config finds
    # nothing and the build falls back to flac's CMake config — which works here and then
    # fails on a platform that ships only the .pc file.
    flac
    libogg
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

  # D21 end to end: ask the installed binary what it is. This runs after fixupPhase, so it
  # exercises the wrapped executable users actually get, and it fails the build rather than
  # shipping an artefact that cannot be traced to a commit.
  doInstallCheck = true;

  installCheckPhase = ''
    runHook preInstallCheck

    export QT_QPA_PLATFORM=offscreen

    reported=$("$out/bin/DomesdayDuplicator" --version)
    echo "DomesdayDuplicator: $reported"
    case "$reported" in
      *"(${dddVersion})"*) ;;
      *)
        echo "DomesdayDuplicator reports '$reported', which does not carry the build version '${dddVersion}'" >&2
        exit 1
        ;;
    esac

    # The desktop integration is easy to break silently — a renamed asset or a changed
    # application ID leaves an installed application with no icon and no launcher entry,
    # and nothing else in the build notices.
    for required in \
      "$out/share/applications/io.github.simoninns.DomesdayDuplicator.desktop" \
      "$out/share/metainfo/io.github.simoninns.DomesdayDuplicator.metainfo.xml" \
      "$out/share/icons/hicolor/256x256/apps/io.github.simoninns.DomesdayDuplicator.png"; do
      if [ ! -f "$required" ]; then
        echo "Missing desktop integration file: $required" >&2
        exit 1
      fi
    done

    runHook postInstallCheck
  '';

  meta = {
    description = "Capture application for the Domesday Duplicator";
    longDescription = ''
      The Domesday Duplicator is a LaserDisc-focused RF capture device sampling at
      40 Msps with 10-bit resolution over USB 3.0. This package provides the capture
      application, which writes FLAC (.ldf) captures the ld-decode toolchain reads
      directly.
    '';
    homepage = "https://github.com/simoninns/DomesdayDuplicator";
    license = lib.licenses.gpl3Plus;
    mainProgram = "DomesdayDuplicator";
    platforms = lib.platforms.unix;
  };
})
