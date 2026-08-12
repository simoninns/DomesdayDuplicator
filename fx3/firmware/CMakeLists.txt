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

# elf2img converts the linked .elf into the boot-loadable image the FX3 expects. It is a
# host tool, not a cross-compiled one, so it must not be built with this project's ARM
# toolchain. Prefer one already on PATH — that is what the Nix build and any packaged
# toolchain provide — and only fall back to building the copy vendored in the SDK.
find_program(ELF2IMG elf2img)

if(ELF2IMG)
    set(ELF2IMG_DEP "")
    message(STATUS "elf2img: using ${ELF2IMG}")
else()
    # CMAKE_TOOLCHAIN_FILE applies to the entire build tree, so elf2img cannot be built
    # as a normal target here — it would be cross-compiled for ARM and could not run.
    # It is a single self-contained C file, so compile it directly with a host compiler.
    # This replaces an ExternalProject_Add() that ran a full nested CMake configure.
    find_program(HOST_CC NAMES cc gcc clang)
    if(NOT HOST_CC)
        message(FATAL_ERROR
            "elf2img is not on PATH and no host C compiler was found to build the copy in "
            "${CYFX3SDK_PATH}/util/elf2img. Install a host C compiler, or put elf2img on PATH.")
    endif()
    set(ELF2IMG ${CMAKE_CURRENT_BINARY_DIR}/elf2img)
    add_custom_command(
        OUTPUT ${ELF2IMG}
        COMMAND ${HOST_CC} -O2 -Wall -o ${ELF2IMG} ${CYFX3SDK_PATH}/util/elf2img/elf2img.c
        DEPENDS ${CYFX3SDK_PATH}/util/elf2img/elf2img.c
        COMMENT "Building host tool elf2img"
        VERBATIM
    )
    set(ELF2IMG_DEP ${ELF2IMG})
    message(STATUS "elf2img: building the SDK copy with ${HOST_CC}")
endif()

# Custom command to generate .img file from .elf
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.img
    COMMAND ${ELF2IMG}
        -i ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.elf
        -o ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.img
        -v
    DEPENDS ${PROJECT_NAME}.elf ${ELF2IMG_DEP}
    COMMENT "Generating boot-loadable binary image"
    VERBATIM
)

# Add custom target for the .img file
add_custom_target(${PROJECT_NAME}_img ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.img
)

# Install targets
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.elf
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.img
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    DESTINATION bin
)
