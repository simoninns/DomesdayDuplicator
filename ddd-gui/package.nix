# Domesday Duplicator capture application.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Call with qt6Packages.callPackage, not plain callPackage — the Qt package set supplies
# wrapQtAppsHook and the matching qtbase.
#
# The capture application this replaced was a separate derivation with a separate
# executable, so both could be installed at once. It has since been removed from the
# repository; a machine that still has it installed keeps it.

{
  lib,
  stdenv,
  cmake,
  ninja,
  pkg-config,
  qt6,
  wrapQtAppsHook,
  flac,
  libusb1,
  spdlog,
  gtest,
  # The commit this binary was built from. It reaches --version and the About dialog, which
  # is the only way a released artefact can be traced back to its source. There is no .git
  # in a Nix sandbox, so CMake's own git fallback cannot fire and this must be passed — the
  # flake passes self.shortRev. A build that reports "unknown" fails the release gate.
  dddCommit ? "unknown",
  # The minisign public key whose signatures this build accepts as releases. The source
  # fileset below closes over ddd-gui/ alone, so the repository's tools/keys/release.pub is
  # not visible to CMake's in-tree default and the flake passes the path in. Null gives a
  # build with no release key pinned: it can install development bundles and honestly
  # cannot verify a release one.
  releaseUpdateKeyFile ? null,
  # Set false to skip the test suite (it needs an offscreen Qt platform plugin)
  doCheck ? true,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "domesday-duplicator-ddd-gui";
  version = "1.0";

  # Only the files the build actually reads, so editing the README does not invalidate the
  # derivation and force a full Qt rebuild.
  src = lib.fileset.toSource {
    root = ./.;
    fileset = lib.fileset.unions [
      ./CMakeLists.txt
      ./cmake
      ./src
      ./tests
      # The .desktop file and the AppStream metadata, which the Linux install rules put
      # into the output. Inside the fileset rather than outside it because the build
      # genuinely reads them: a build that installs a file it cannot see fails, which is
      # how this was found.
      ./packaging
    ];
  };

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    wrapQtAppsHook
  ];

  buildInputs = [
    qt6.qtbase

    # The LaserDisc player link. A separate Qt module rather than part of qtbase, and only
    # the Qt layer links it: the player protocol itself is Qt-free.
    qt6.qtserialport

    flac
    libusb1

    # The console and file halves of the log, behind src/capture/spdlog_logger.h. MIT, so
    # linking it into a GPLv3 application is fine (AGENTS.md §10).
    spdlog
  ];

  checkInputs = [ gtest ];

  cmakeFlags = [
    (lib.cmakeBool "BUILD_TESTING" finalAttrs.doCheck)
    (lib.cmakeFeature "DDD_COMMIT" dddCommit)

    # Both quality gates are development and CI checks, and both are wrong here. The
    # sandbox's clang-format is an unrelated version whose output differs, so a style
    # difference would fail a release build; and its clang-tidy is unwrapped and cannot
    # resolve the standard library headers, so it fails on valid sources. Neither is a
    # statement about the code. They run in the dev shell and in the native CI build.
    (lib.cmakeBool "DDD_ENABLE_CLANG_FORMAT" false)
    (lib.cmakeBool "DDD_ENABLE_CLANG_TIDY" false)
  ]
  # Passed rather than left to CMake's in-tree default, because the fileset above cannot
  # see tools/keys/release.pub. Omitted entirely when there is no key: a flag with an empty
  # value would be a path that does not exist, which the build treats as an error.
  ++ lib.optional (releaseUpdateKeyFile != null) (
    lib.cmakeFeature "DDD_RELEASE_UPDATE_KEY_FILE" "${releaseUpdateKeyFile}"
  );

  inherit doCheck;

  # The Qt tests instantiate QObjects and need a platform plugin. There is no display in
  # the build sandbox, so use Qt's offscreen one.
  preCheck = ''
    export QT_QPA_PLATFORM=offscreen
  '';

  # The soak tests run for a minute each by default, which is right on a CI runner and
  # wrong in a packaging build that is only asking whether this commit compiles and works.
  # Shortened rather than skipped: a ten-second soak still catches a pipeline that cannot
  # sustain the rate at all, which is the failure worth catching here.
  checkPhase = ''
    runHook preCheck
    DDD_SOAK_SECONDS=10 ctest --output-on-failure --label-exclude 'hil'
    runHook postCheck
  '';

  # Ask the installed binary what it is. This runs after fixupPhase, so it exercises the
  # wrapped executable users actually get, and it fails the build rather than shipping an
  # artefact that cannot be traced to a commit.
  doInstallCheck = true;

  installCheckPhase = ''
    runHook preInstallCheck

    export QT_QPA_PLATFORM=offscreen

    reported=$("$out/bin/ddd-gui" --version)
    echo "ddd-gui: $reported"
    case "$reported" in
      *"${dddCommit}"*) ;;
      *)
        echo "ddd-gui reports '$reported', which does not carry the build commit '${dddCommit}'" >&2
        exit 1
        ;;
    esac

    # ddd-update is the headless half of the device update path, over the same Qt-free
    # engine the application's update dialog drives. Asking it for its usage text is
    # enough to prove it was built and linked; it needs a device to do anything else, and
    # nothing automated in this project ever writes to one (AGENTS.md §4).
    "$out/bin/ddd-update" --help > /dev/null
    echo "ddd-update: present"

    runHook postInstallCheck
  '';

  meta = {
    description = "Capture and signal monitoring application for the Domesday Duplicator";
    longDescription = ''
      The Domesday Duplicator is a LaserDisc-focused RF capture device sampling at
      40 Msps with 10-bit resolution over USB 3.0. This package provides the capture
      application, built around real-time monitoring of the incoming signal.
    '';
    homepage = "https://github.com/simoninns/DomesdayDuplicator";
    license = lib.licenses.gpl3Plus;
    mainProgram = "ddd-gui";
    platforms = lib.platforms.unix;
  };
})
