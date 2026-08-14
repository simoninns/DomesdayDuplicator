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

# Emit compile_commands.json so clangd — and therefore any editor — can index the tree
# without a per-editor project file (AGENTS.md §7.1).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Commit stamp, so a released binary can be traced back to the source that produced it.
# Mirrors gui/CMakeLists.txt and fx3/firmware/CMakeLists.txt: callers building outside a
# git checkout — Nix, release tarballs, a shallow CI clone — pass -DDDD_VERSION=<hash>,
# and only when they have not does the build ask git. A release artefact reporting
# "unknown" fails the release gate (AGENTS.md §9).
if(NOT DEFINED DDD_VERSION OR DDD_VERSION STREQUAL "")
    execute_process(
        COMMAND git rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE DDD_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # A bare commit hash from a dirty tree names a commit that does not contain the code
    # being built.
    if(DDD_VERSION)
        execute_process(
            COMMAND git status --porcelain
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE DDD_GIT_STATUS
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT DDD_GIT_STATUS STREQUAL "")
            set(DDD_VERSION "${DDD_VERSION}-dirty")
        endif()
    endif()
endif()

if(NOT DDD_VERSION)
    set(DDD_VERSION "unknown")
endif()

message(STATUS "Application version: ${DDD_VERSION}")

# Qt 6.5 is the floor, not 6.2 as in gui/: QStyleHints::colorScheme() and its change
# signal arrived in 6.5, and they are how the application follows the desktop's light/dark
# setting. Without them the theme could only ever be guessed from the palette.
#
# Test is part of qtbase everywhere the application is built, and only the test binaries
# link it: QSignalSpy and QAbstractItemModelTester live there.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Test)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# libFLAC, for the capture output. Not optional: it is the only format written, so a build
# without it could not capture anything. BSD-3-Clause, so linking it into a GPLv3
# application is fine (AGENTS.md §10).
find_package(FLAC REQUIRED)

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
add_subdirectory(src/gui)

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

set(ddd_gated_targets ddd_capture ddd_analysis ddd_gui_lib ddd-gui)

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
