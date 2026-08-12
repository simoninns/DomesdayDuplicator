cmake_minimum_required(VERSION 3.16)
project(DomesdayDuplicator VERSION 1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Emit compile_commands.json so clangd (and therefore any editor) can index the tree
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Commit stamp, so a released binary can be traced back to the source that produced it
# (D21). Mirrors what fx3/firmware/CMakeLists.txt does for FIRMWARE_VERSION: callers
# building outside a git checkout — Nix, release tarballs, a shallow CI clone — pass
# -DDDD_VERSION=<hash>, and only when they have not does the build ask git. It reaches
# every tool's --version and About dialog through DDD_VERSION below.
if(NOT DEFINED DDD_VERSION OR DDD_VERSION STREQUAL "")
    execute_process(
        COMMAND git rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE DDD_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # Same reasoning as fx3/firmware/CMakeLists.txt: a bare commit hash from a dirty tree
    # names a commit that does not contain the code being built.
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

add_compile_definitions(DDD_VERSION="${DDD_VERSION}")

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets SerialPort)
find_package(LibUSB REQUIRED)

# Use qt_standard_project_setup if available (Qt 6.3+), otherwise manual setup
if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
else()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    include(GNUInstallDirs)
endif()

add_subdirectory(src/DomesdayDuplicator)
add_subdirectory(src/dddconv)
add_subdirectory(src/dddutil)

# Tests. On by default when this is the top-level project, so `cmake -B build` gives a
# contributor the suite without having to know a flag; off when consumed as a subproject.
include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
