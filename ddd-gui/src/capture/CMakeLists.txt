# The capture engine — Qt-free by rule.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Nothing here may link or include Qt. That is what lets the engine be unit tested without
# a display, and what will let a headless command-line capture tool reuse it unchanged.

# The USB backend is chosen at configure time rather than at run time: a build for
# one platform has no use for the other's dependency, and compiling both would mean
# requiring libusb on Windows to reach code that would never run there.
if(WIN32)
    set(ddd_capture_usb_sources
        winusb_device.cpp
        winusb_source.cpp
    )
else()
    set(ddd_capture_usb_sources
        libusb_device.cpp
        libusb_source.cpp
    )
endif()

add_library(ddd_capture STATIC
    ${ddd_capture_usb_sources}
    capture_format.cpp
    capture_naming.cpp
    capture_pipeline.cpp
    capture_provenance.cpp
    capture_reader.cpp
    device_monitor.cpp
    digest.cpp
    disk_buffer_ring.cpp
    firmware_version.cpp
    flac_sink.cpp
    fpga_version.cpp
    flac_writer.cpp
    free_space.cpp
    json_value.cpp
    logger.cpp
    memory_lock.cpp
    minisign_verify.cpp
    monitor_tap.cpp
    sample_metrics.cpp
    sample_sink.cpp
    sequence_validator.cpp
    synthetic_source.cpp
    test_data_analysis.cpp
    test_pattern_verifier.cpp
    thread_priority.cpp
    transfer_result.cpp
    update_bundle.cpp
    update_manifest.cpp
    usb_device.cpp
    usb_device_info.cpp
    version.cpp
)

target_include_directories(ddd_capture PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_options(ddd_capture PRIVATE -Wall -Wextra)

target_include_directories(ddd_capture SYSTEM PUBLIC ${FLAC_INCLUDE_DIRS})
target_link_libraries(ddd_capture PUBLIC ${FLAC_LIBRARIES} Threads::Threads)

# The vendored SHA-256 and Ed25519, behind digest.h and minisign_verify.h. PRIVATE
# because nothing outside those two wrappers may reach the vendored headers — a call to
# crypto_ed25519_check from anywhere else would be a second place that knows which
# implementation this is (src/vendor/VENDOR.md).
target_link_libraries(ddd_capture PRIVATE ddd_vendor_crypto)

# The USB backend's own dependencies. On Windows both libraries ship with the SDK and
# with mingw-w64, so there is nothing to find; everywhere else libusb comes from
# pkg-config. LGPL-2.1-or-later, so linking it into a GPLv3 application is fine
# (AGENTS.md §10).
if(WIN32)
    target_link_libraries(ddd_capture PUBLIC winusb cfgmgr32)
    target_compile_definitions(ddd_capture PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_include_directories(ddd_capture SYSTEM PUBLIC ${LibUSB_INCLUDE_DIRS})
    target_link_libraries(ddd_capture PUBLIC ${LibUSB_LIBRARIES})
endif()

# Only this target sees the version stamp, so a new commit rebuilds one object file rather
# than every Qt translation unit in the application.
target_compile_definitions(ddd_capture PRIVATE DDD_VERSION="${DDD_VERSION}")
