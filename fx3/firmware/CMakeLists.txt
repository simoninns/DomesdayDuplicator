cmake_minimum_required(VERSION 3.10)
project(firmware C ASM)

# Emit compile_commands.json so clangd (and therefore any editor) can index the tree
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Firmware version. Callers that build outside a git checkout — Nix, release tarballs,
# CI with a shallow clone — pass -DFIRMWARE_VERSION=<hash>. Only fall back to asking git
# when they have not, because git is unavailable in a sandboxed build and the version
# would otherwise silently become "unknown".
if(NOT DEFINED FIRMWARE_VERSION OR FIRMWARE_VERSION STREQUAL "")
    execute_process(
        COMMAND git rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE FIRMWARE_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # A commit hash alone describes the build only if the tree matches that commit.
    # Without this, a developer build with uncommitted work stamps a hash whose source
    # does not contain that work — the image says one thing and is another, which is
    # exactly the untraceability the version stamp exists to prevent. Untracked files
    # count: they are not noise here, they can be part of the build.
    if(FIRMWARE_VERSION)
        execute_process(
            COMMAND git status --porcelain
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE FIRMWARE_GIT_STATUS
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT FIRMWARE_GIT_STATUS STREQUAL "")
            set(FIRMWARE_VERSION "${FIRMWARE_VERSION}-dirty")
        endif()
    endif()
endif()

if(NOT FIRMWARE_VERSION)
    set(FIRMWARE_VERSION "unknown")
endif()

message(STATUS "Firmware version: ${FIRMWARE_VERSION}")

# Generate USB descriptor with embedded git commit at build time
execute_process(
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/generate-descriptor.sh" "${CMAKE_CURRENT_BINARY_DIR}" "${FIRMWARE_VERSION}"
    OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/generated_descriptor_data.h"
    RESULT_VARIABLE desc_result
)

if(NOT desc_result EQUAL 0)
    message(WARNING "Failed to generate USB descriptor header")
endif()

# Set the CyFX3 SDK path relative to this project
set(CYFX3SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../sdk" CACHE PATH "Path to CyFX3 SDK")

# Resolve to absolute path
get_filename_component(CYFX3SDK_PATH "${CYFX3SDK_PATH}" ABSOLUTE)

message(STATUS "Using CyFX3 SDK at: ${CYFX3SDK_PATH}")

# Verify SDK path exists
if(NOT EXISTS "${CYFX3SDK_PATH}")
    message(FATAL_ERROR "CyFX3 SDK not found at ${CYFX3SDK_PATH}")
endif()

# SDK version - using 1.3.5 (latest)
set(CYFX3SDK_VERSION "1_3_5")

# Include directories
set(CYFX3SDK_INCLUDE_DIR "${CYFX3SDK_PATH}/fw_lib/${CYFX3SDK_VERSION}/inc")
set(CYFX3SDK_LIB_DIR "${CYFX3SDK_PATH}/fw_lib/${CYFX3SDK_VERSION}/fx3_release")
set(CYFX3SDK_LINKER_SCRIPT "${CYFX3SDK_PATH}/fw_build/fx3_fw/fx3.ld")

# Verify paths exist
if(NOT EXISTS "${CYFX3SDK_INCLUDE_DIR}")
    message(FATAL_ERROR "SDK include directory not found at ${CYFX3SDK_INCLUDE_DIR}")
endif()

if(NOT EXISTS "${CYFX3SDK_LIB_DIR}")
    message(FATAL_ERROR "SDK library directory not found at ${CYFX3SDK_LIB_DIR}")
endif()

if(NOT EXISTS "${CYFX3SDK_LINKER_SCRIPT}")
    message(FATAL_ERROR "Linker script not found at ${CYFX3SDK_LINKER_SCRIPT}")
endif()

# Source files
set(C_SOURCES
    src/cyfxtx.c
    src/domesday-duplicator.c
    src/fpga-register-map.c
    src/fpga-registers.c
    src/usb-descriptor.c
)

set(ASM_SOURCES
    src/cyfx_gcc_startup.S
)

# Create executable
add_executable(${PROJECT_NAME}.elf
    ${C_SOURCES}
    ${ASM_SOURCES}
)

# Include directories
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CYFX3SDK_INCLUDE_DIR}
)

# Compiler definitions
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    __CYU3P_TX__=1
    FIRMWARE_GIT_COMMIT="${FIRMWARE_VERSION}"
)

# Compiler flags for C files
target_compile_options(${PROJECT_NAME}.elf PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-mcpu=arm926ej-s>
    $<$<COMPILE_LANGUAGE:C>:-mthumb>
    $<$<COMPILE_LANGUAGE:C>:-O3>
    $<$<COMPILE_LANGUAGE:C>:-fmessage-length=0>
    $<$<COMPILE_LANGUAGE:C>:-fsigned-char>
    $<$<COMPILE_LANGUAGE:C>:-ffunction-sections>
    $<$<COMPILE_LANGUAGE:C>:-fdata-sections>
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-g>
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
)

# Compiler flags for ASM files
target_compile_options(${PROJECT_NAME}.elf PRIVATE
    $<$<COMPILE_LANGUAGE:ASM>:-mcpu=arm926ej-s>
    $<$<COMPILE_LANGUAGE:ASM>:-mthumb>
    $<$<COMPILE_LANGUAGE:ASM>:-O3>
    $<$<COMPILE_LANGUAGE:ASM>:-fmessage-length=0>
    $<$<COMPILE_LANGUAGE:ASM>:-fsigned-char>
    $<$<COMPILE_LANGUAGE:ASM>:-ffunction-sections>
    $<$<COMPILE_LANGUAGE:ASM>:-fdata-sections>
    $<$<COMPILE_LANGUAGE:ASM>:-Wall>
    $<$<COMPILE_LANGUAGE:ASM>:-g>
)

# Linker flags
target_link_options(${PROJECT_NAME}.elf PRIVATE
    -mcpu=arm926ej-s
    -mthumb
    -O3
    -fmessage-length=0
    -fsigned-char
    -ffunction-sections
    -fdata-sections
    -Wall
    -g
    -T${CYFX3SDK_LINKER_SCRIPT}
    -nostartfiles
    -Xlinker --gc-sections
    -L${CYFX3SDK_LIB_DIR}
    -Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    -Wl,-d
    -Wl,--no-wchar-size-warning
    -Wl,--entry,CyU3PFirmwareEntry
)

# Link libraries
target_link_libraries(${PROJECT_NAME}.elf PRIVATE
    cyu3lpp
    cyfxapi
    cyu3threadx
    c
    gcc
)

# fx3-mkimage converts the linked .elf into the boot-loadable image the FX3 expects. It is
# a host tool, not a cross-compiled one, so it must not be built with this project's ARM
# toolchain. Prefer one already on PATH — that is what the Nix build and the dev shell
# provide — and only fall back to compiling the sources in ../mkimage directly.
set(MKIMAGE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../mkimage)

find_program(FX3_MKIMAGE fx3-mkimage)

if(FX3_MKIMAGE)
    set(FX3_MKIMAGE_DEP "")
    message(STATUS "fx3-mkimage: using ${FX3_MKIMAGE}")
else()
    # CMAKE_TOOLCHAIN_FILE applies to the entire build tree, so fx3-mkimage cannot be
    # built as a normal target here — it would be cross-compiled for ARM and could not
    # run. Compile it directly with a host compiler instead; it is two self-contained C
    # files with no dependencies beyond libc.
    find_program(HOST_CC NAMES cc gcc clang)
    if(NOT HOST_CC)
        message(FATAL_ERROR
            "fx3-mkimage is not on PATH and no host C compiler was found to build it from "
            "${MKIMAGE_DIR}. Install a host C compiler, or put fx3-mkimage on PATH.")
    endif()
    set(FX3_MKIMAGE ${CMAKE_CURRENT_BINARY_DIR}/fx3-mkimage)
    set(MKIMAGE_SOURCES
        ${MKIMAGE_DIR}/src/fx3-bootimage.c
        ${MKIMAGE_DIR}/src/main.c
    )
    add_custom_command(
        OUTPUT ${FX3_MKIMAGE}
        COMMAND ${HOST_CC} -std=c11 -O2 -Wall -I${MKIMAGE_DIR}/src
                -DFX3_MKIMAGE_VERSION="${FIRMWARE_VERSION}"
                -o ${FX3_MKIMAGE} ${MKIMAGE_SOURCES}
        DEPENDS ${MKIMAGE_SOURCES}
        COMMENT "Building host tool fx3-mkimage"
        VERBATIM
    )
    set(FX3_MKIMAGE_DEP ${FX3_MKIMAGE})
    message(STATUS "fx3-mkimage: building from ${MKIMAGE_DIR} with ${HOST_CC}")
endif()

# Custom command to generate .img file from .elf
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.img
    COMMAND ${FX3_MKIMAGE}
        -i ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.elf
        -o ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.img
        -v
    DEPENDS ${PROJECT_NAME}.elf ${FX3_MKIMAGE_DEP}
    COMMENT "Generating boot-loadable binary image"
    VERBATIM
)

# Add custom target for the .img file
add_custom_target(${PROJECT_NAME}_img ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.img
)

# Install targets.
#
# `bin` by default so an ordinary `cmake --install` behaves as it always has. The Nix
# derivation passes `.` so the three artefacts land at the root of the store path, where
# CI can upload the directory without knowing the layout.
set(FIRMWARE_INSTALL_DIR "bin" CACHE STRING
    "Directory under the install prefix to receive firmware.{elf,img,map}")

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.elf
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.img
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    DESTINATION ${FIRMWARE_INSTALL_DIR}
)

# Tests. On by default when this is the top-level project, matching gui/ and
# fx3/programmer/. Everything here is host-side — the cross toolchain cannot run its own
# output, so nothing in this project can be tested by executing it.
include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
