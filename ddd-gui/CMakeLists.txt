# Build definition for the capture application.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Two targets carry the application, and the split is load-bearing rather than tidy:
#
#   ddd_capture    the capture engine. Qt-free, by rule. Everything a capture needs —
#                  device discovery, the transfer pipeline, validation, writers — lives
#                  here so it can be unit tested without a display and driven later by a
#                  headless command-line tool. Logic that ends up in ddd_gui_lib instead
#                  is logic no test and no CLI can reach.
#   ddd_gui_lib    the Qt layer, built as a static library so the test binaries can link
#                  it. `ddd-gui` itself is then a main() over that library.

cmake_minimum_required(VERSION 3.21)

# C is here for one reason: the vendored cryptography in src/vendor is C, and compiling it
# as C++ would mean modifying it, which vendored code is exactly what we do not do
# (AGENTS.md §3). No project-authored source is C.
project(ddd-gui VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The application's identity on a desktop: the Flatpak app ID, the .desktop file's name,
# the AppStream component ID and the installed icon's name are all this string, and the
# files under packaging/assets/ are named after it.
#
# It is deliberately not io.github.simoninns.DomesdayDuplicator, the identifier of the
# capture application this one replaced. That application has been removed from the
# repository but may still be installed on somebody's machine; a shared identifier would
# mean installing this one replaces it, which is not a decision a package manager should
# make on a user's behalf. The Windows installer keeps them apart the same way, with its
# own UpgradeCode.
#
# Changing this after the first publish is a migration rather than an edit — Flathub and
# Windows both key on it — so it wants settling before the first release that ships it.
set(DDD_APP_ID "io.github.simoninns.DddGui")

# Emit compile_commands.json so clangd — and therefore any editor — can index the tree
# without a per-editor project file (AGENTS.md §7.1).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# One stamp: the commit this binary was built from. See src/capture/version.h for why
# there is no release version beside it — the same rule the FX3 firmware and the FPGA
# gateware follow, so all three parts of a Duplicator report the same kind of thing and a
# bug report can quote three hashes.
#
# Mirrors fx3/firmware/CMakeLists.txt: callers building outside a git checkout — Nix,
# release tarballs, a shallow CI clone — pass -DDDD_COMMIT=<hash>, and only when they have not
# does the build ask git. An artefact that cannot name a commit reports "unknown" and
# fails the release gate (AGENTS.md §9).
if(NOT DEFINED DDD_COMMIT OR DDD_COMMIT STREQUAL "")
    execute_process(
        COMMAND git rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE DDD_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # A bare commit hash from a dirty tree names a commit that does not contain the code
    # being built.
    if(DDD_COMMIT)
        execute_process(
            COMMAND git status --porcelain
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE DDD_GIT_STATUS
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT DDD_GIT_STATUS STREQUAL "")
            set(DDD_COMMIT "${DDD_COMMIT}-dirty")
        endif()
    endif()
endif()

if(NOT DDD_COMMIT)
    set(DDD_COMMIT "unknown")
endif()

message(STATUS "Application commit: ${DDD_COMMIT}")

# Which update signing key this build trusts.
#
# A release build pins the release public key and accepts nothing else; a build with no
# release key pinned can verify no release bundle at all and says so rather than pretending
# to a trust it does not have (src/capture/update_key.cpp). The key is therefore a *build*
# input and never a file the application reads at run time — a key loaded from disk is a key
# an attacker can replace, and then the whole integrity chain proves nothing.
#
# The default is the repository's own tools/keys/release.pub when it is there, so that an
# in-tree build — a contributor's, or the native CI build the installers are made from —
# pins the same key a release does without anyone having to remember a flag. A build whose
# source tree does not carry that file (the Nix package closes over ddd-gui/ alone) passes
# the path explicitly. Publishing a release key is therefore one commit, not a sweep through
# every build definition.
set(DDD_RELEASE_UPDATE_KEY_FILE "" CACHE FILEPATH
    "minisign public key (.pub) whose signatures this build accepts as releases")

if(NOT DDD_RELEASE_UPDATE_KEY_FILE AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../tools/keys/release.pub")
    set(DDD_RELEASE_UPDATE_KEY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../tools/keys/release.pub")
endif()

set(DDD_RELEASE_UPDATE_KEY_TEXT "")

if(DDD_RELEASE_UPDATE_KEY_FILE)
    if(NOT EXISTS "${DDD_RELEASE_UPDATE_KEY_FILE}")
        message(FATAL_ERROR
            "DDD_RELEASE_UPDATE_KEY_FILE points at ${DDD_RELEASE_UPDATE_KEY_FILE}, "
            "which does not exist. A release build that silently fell back to no key "
            "would ship an application that refuses every release bundle.")
    endif()

    file(READ "${DDD_RELEASE_UPDATE_KEY_FILE}" DDD_RELEASE_UPDATE_KEY_TEXT)

    # Shape-checked here rather than only at run time, because a mistyped path or a secret
    # half committed by accident should stop the build rather than produce a binary that
    # fails on a user's machine. Two lines, the first an untrusted comment, is what a
    # minisign public key is; the secret half's comment line differs, and its data line is
    # far longer than the 56 characters a public key encodes to.
    string(REGEX MATCH "^untrusted comment:[^\n]*\n[A-Za-z0-9+/=]+\n?$"
           DDD_RELEASE_UPDATE_KEY_SHAPE "${DDD_RELEASE_UPDATE_KEY_TEXT}")
    if(NOT DDD_RELEASE_UPDATE_KEY_SHAPE)
        message(FATAL_ERROR
            "${DDD_RELEASE_UPDATE_KEY_FILE} is not a minisign public key: expected an "
            "untrusted comment line followed by one base64 line.")
    endif()

    # The development key is committed, secret half and all, so pinning it as *the release
    # key* would make every development bundle verify as a release — which is precisely the
    # confusion the two-key scheme exists to make impossible.
    if(DDD_RELEASE_UPDATE_KEY_TEXT MATCHES "development")
        message(FATAL_ERROR
            "${DDD_RELEASE_UPDATE_KEY_FILE} looks like the development key. Its secret "
            "half is public; it can never be a release key.")
    endif()

    message(STATUS "Release update key: ${DDD_RELEASE_UPDATE_KEY_FILE}")
else()
    message(STATUS "Release update key: none pinned — development bundles only")
endif()

# The provisioning set this build carries, for bringing a board up with no network.
#
# A board being brought up cannot be updated over USB — that is the whole reason the
# bring-up wizard exists — so the machine in front of it may be one that has just been
# built, with no other way to fetch a file. A packaged build therefore installs one
# provisioning set beside itself, and the wizard preselects it.
#
# Unlike the signing key above, this is *data the signature covers* rather than a trust
# anchor: it is verified before it is used, by the same reader and the same key policy as
# a file a user picked. So bundling it is a convenience and never a shortcut, and a build
# that bundles nothing is a build whose wizard opens with its file picker and no
# preselection — the honest state, and the same convention the unpinned release key
# follows.
#
# The packaging workflows pass this, having fetched the file by the digest pinned in
# ddd-gui/packaging/bundled-update.env. A local build can pass one produced by
# ./tools/dev-bundle.sh.
set(DDD_BUNDLED_UPDATE_FILE "" CACHE FILEPATH
    "provisioning set (.dddfw) to install beside the application for offline bring-up")

if(DDD_BUNDLED_UPDATE_FILE)
    if(NOT EXISTS "${DDD_BUNDLED_UPDATE_FILE}")
        message(FATAL_ERROR
            "DDD_BUNDLED_UPDATE_FILE points at ${DDD_BUNDLED_UPDATE_FILE}, "
            "which does not exist. A packaging job whose fetch had silently failed would "
            "otherwise ship an installer that looks complete and brings nothing up.")
    endif()

    # Shape-checked rather than taken on trust, because the failure this catches is a
    # fetch that produced an HTML error page with the right filename. A bundle is an
    # uncompressed ustar archive whose first member is manifest.json, and a tar header
    # begins with that member's name — so thirteen bytes at offset zero settle it.
    #
    # Compared as hex because file(READ) in its text mode does not give back exactly the
    # bytes it read — it is reading a tar header, which is not text.
    file(READ "${DDD_BUNDLED_UPDATE_FILE}" DDD_BUNDLED_UPDATE_HEAD
         LIMIT 13 HEX)
    if(NOT DDD_BUNDLED_UPDATE_HEAD STREQUAL "6d616e69666573742e6a736f6e")
        message(FATAL_ERROR
            "${DDD_BUNDLED_UPDATE_FILE} is not an update bundle: its first archive "
            "member is not manifest.json.")
    endif()

    message(STATUS "Bundled provisioning set: ${DDD_BUNDLED_UPDATE_FILE}")
else()
    message(STATUS "Bundled provisioning set: none — the bring-up wizard picks a file")
endif()

# Qt 6.5 is the floor: QStyleHints::colorScheme() and its change signal arrived in 6.5,
# and they are how the application follows the desktop's light/dark setting. Without them
# the theme could only ever be guessed from the palette.
#
# Test is part of qtbase everywhere the application is built, and only the test binaries
# link it: QSignalSpy and QAbstractItemModelTester live there.
#
# SerialPort is for the LaserDisc player link, and only the Qt layer links it — the player
# protocol itself is Qt-free and talks to an interface (src/player). It is a separate Qt
# module rather than part of qtbase, so a distribution build needs its package installed.
# The capture application this one replaced required it too, so no machine that could
# build this repository is losing anything.
#
# Network is for QLocalServer and QLocalSocket, and for nothing else: the control socket a
# script stops a capture through. Despite the module's name nothing here opens a network
# socket — a local socket is a named pipe on Windows and a Unix domain socket everywhere
# else, both of which are reachable only from the machine the application is running on.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network SerialPort Test)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# libFLAC, for the capture output. Not optional: it is the only format written, so a build
# without it could not capture anything. BSD-3-Clause, so linking it into a GPLv3
# application is fine (AGENTS.md §10).
find_package(FLAC REQUIRED)

# spdlog, for the console and file halves of the log. MIT, so linking it into a GPLv3
# application is fine (AGENTS.md §10). Only src/capture/spdlog_logger.cpp includes it: the
# engine's logging seam is logger.h and stays that way, and spdlog is one implementation
# of it rather than a logging library the whole application calls into.
find_package(spdlog REQUIRED)

# libusb, for talking to the device on Linux and macOS. Not needed on Windows, where the
# WinUSB backend uses libraries that ship with the system. LGPL-2.1-or-later, so linking
# it into a GPLv3 application is fine (AGENTS.md §10).
if(NOT WIN32)
    find_package(LibUSB REQUIRED)
endif()

# The engine runs its own threads. Named explicitly rather than left to the compiler
# default, because a static library that pulls in std::thread without -pthread links on
# some platforms and fails at run time on others.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
else()
    set(CMAKE_AUTOMOC ON)
    include(GNUInstallDirs)
endif()

add_subdirectory(src/vendor)
add_subdirectory(src/capture)
add_subdirectory(src/analysis)
add_subdirectory(src/player)
add_subdirectory(src/gui)
add_subdirectory(src/update-cli)
add_subdirectory(src/jtag-cli)

# Tests are on by default when this is the top-level project, so `cmake -B build` gives a
# contributor the suite without having to know a flag.
include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# --- Quality gates -----------------------------------------------------------------
#
# Both gates are development and CI checks, and both are turned off by the Nix package
# build. Two different reasons, both learned the hard way elsewhere:
#
#   clang-format — the formatter available in a packaging sandbox is an unrelated version
#   whose output differs, and a style difference must never fail a release build.
#
#   clang-tidy — a packaging sandbox exposes the stdenv's unwrapped clang-tidy, which
#   cannot resolve the standard library headers and fails on otherwise-valid sources.

set(ddd_gated_targets ddd_capture ddd_analysis ddd_player ddd_gui_lib ddd-gui ddd-update
    ddd-jtag)

if(BUILD_TESTING)
    get_property(ddd_test_targets GLOBAL PROPERTY DDD_TEST_TARGETS)
    list(APPEND ddd_gated_targets ${ddd_test_targets})
endif()

option(DDD_ENABLE_CLANG_FORMAT "Run clang-format checks as part of the build" ON)

if(DDD_ENABLE_CLANG_FORMAT)
    find_program(CLANG_FORMAT_EXE NAMES clang-format)

    if(CLANG_FORMAT_EXE)
        file(GLOB_RECURSE ddd_format_sources CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h"
        )

        # src/vendor is upstream cryptography, copied byte-for-byte and never edited
        # (AGENTS.md §3, src/vendor/VENDOR.md). Reformatting it would make the next
        # refresh a merge instead of a copy.
        list(FILTER ddd_format_sources EXCLUDE REGEX "/src/vendor/")

        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/clang-format-stamps")

        # One stamp per file, so an unchanged file is not re-checked and the failure names
        # the file that is wrong rather than the whole tree.
        set(ddd_format_stamps)
        foreach(source IN LISTS ddd_format_sources)
            file(RELATIVE_PATH rel_source "${CMAKE_CURRENT_SOURCE_DIR}" "${source}")
            string(REPLACE "/" "_" stamp_name "${rel_source}")
            string(REPLACE "." "_" stamp_name "${stamp_name}")
            set(stamp "${CMAKE_CURRENT_BINARY_DIR}/clang-format-stamps/${stamp_name}.stamp")
            add_custom_command(
                OUTPUT "${stamp}"
                COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror "${source}"
                COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
                DEPENDS "${source}"
                COMMENT "clang-format: ${rel_source}"
                VERBATIM
            )
            list(APPEND ddd_format_stamps "${stamp}")
        endforeach()

        add_custom_target(ddd_clang_format_check ALL DEPENDS ${ddd_format_stamps})

        foreach(target IN LISTS ddd_gated_targets)
            add_dependencies(${target} ddd_clang_format_check)
        endforeach()
    else()
        message(STATUS "clang-format not found — format checks disabled")
    endif()
else()
    message(STATUS "clang-format disabled (DDD_ENABLE_CLANG_FORMAT=OFF)")
endif()

option(DDD_ENABLE_CLANG_TIDY "Run clang-tidy static analysis as part of the build" ON)

if(DDD_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)

    if(CLANG_TIDY_EXE)
        set_target_properties(${ddd_gated_targets} PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
        )

        # Qt's moc output is compiled into every Qt target, and clang-tidy would hold that
        # generated code to the same warnings-as-errors bar as hand-written code. It is
        # not ours to fix and regenerating overwrites any change, so it is skipped
        # outright. HeaderFilterRegex above already keeps diagnostics out of the moc
        # headers; this covers the translation unit that includes them.
        foreach(target IN LISTS ddd_gated_targets)
            get_target_property(target_binary_dir ${target} BINARY_DIR)
            set_source_files_properties(
                "${target_binary_dir}/${target}_autogen/mocs_compilation.cpp"
                TARGET_DIRECTORY ${target}
                PROPERTIES SKIP_LINTING ON
            )
        endforeach()
    else()
        message(STATUS "clang-tidy not found — static analysis disabled")
    endif()
else()
    message(STATUS "clang-tidy disabled (DDD_ENABLE_CLANG_TIDY=OFF)")
endif()
