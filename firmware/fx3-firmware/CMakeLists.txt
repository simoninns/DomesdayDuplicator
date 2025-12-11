cmake_minimum_required(VERSION 3.10)
project(firmware C ASM)

# Set the CyFX3 SDK path relative to this project
set(CYFX3SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cyfx3sdk" CACHE PATH "Path to CyFX3 SDK")

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
    firmware/cyfxtx.c
    firmware/domesday-duplicator.c
    firmware/usb-descriptor.c
)

set(ASM_SOURCES
    firmware/cyfx_gcc_startup.S
)

# Create executable
add_executable(${PROJECT_NAME}.elf
    ${C_SOURCES}
    ${ASM_SOURCES}
)

# Include directories
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware
    ${CYFX3SDK_INCLUDE_DIR}
)

# Compiler definitions
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    __CYU3P_TX__=1
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

# Build elf2img utility first (as a host tool)
include(ExternalProject)
ExternalProject_Add(elf2img_build
    SOURCE_DIR ${CYFX3SDK_PATH}/util/elf2img
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/tools
        -DCMAKE_BUILD_TYPE=Release
    BUILD_ALWAYS 0
)

# Custom command to generate .img file from .elf
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.img
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tools/bin/elf2img
        -i ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.elf
        -o ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.img
        -v
    DEPENDS ${PROJECT_NAME}.elf elf2img_build
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
