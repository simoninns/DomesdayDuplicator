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

        # The USB-Blaster is reached over libusb, so this platform gets the stub that
        # says so. See usb_blaster_unsupported.cpp: the protocol, the player and
        # everything else above the byte pipe are built here, and only the pipe is
        # missing.
        usb_blaster_unsupported.cpp

        # Presence is asked without opening anything, so it has a backend of its own
        # rather than riding on IUsbDevice: the WinUSB backend can only see devices
        # bound to WinUSB, and the cable this has to answer for may well not be.
        usb_presence_winusb.cpp
    )
else()
    set(ddd_capture_usb_sources
        libusb_device.cpp
        libusb_source.cpp
        usb_blaster_libusb.cpp
        usb_presence_libusb.cpp
    )
endif()

add_library(ddd_capture STATIC
    ${ddd_capture_usb_sources}
    boot_image.cpp
    capture_format.cpp
    capture_metadata.cpp
    capture_naming.cpp
    capture_pipeline.cpp
    capture_provenance.cpp
    capture_reader.cpp
    device_monitor.cpp
    device_programmer.cpp
    device_recovery.cpp
    device_updater.cpp
    digest.cpp
    disk_buffer_ring.cpp
    firmware_version.cpp
    flac_sink.cpp
    fpga_telemetry.cpp
    fpga_version.cpp
    flac_writer.cpp
    free_space.cpp
    jtag_cli.cpp
    json_value.cpp
    logger.cpp
    memory_lock.cpp
    minisign_verify.cpp
    monitor_tap.cpp
    bringup_orchestrator.cpp
    raw_sink.cpp
    sample_metrics.cpp
    sample_sink.cpp
    sequence_validator.cpp
    svf_player.cpp
    synthetic_source.cpp
    test_data_analysis.cpp
    test_pattern_verifier.cpp
    thread_priority.cpp
    transfer_result.cpp
    update_bundle.cpp
    update_cli.cpp
    update_gate.cpp
    update_key.cpp
    update_manifest.cpp
    update_orchestrator.cpp
    usb_blaster_cable.cpp
    usb_device.cpp
    usb_device_info.cpp
    version.cpp
    yaml_writer.cpp
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

# The pinned release signing key, decided in the top-level CMakeLists.txt and delivered as a
# generated header rather than a compile definition: the key is two lines of text, and a
# multi-line string reaching a compiler through -D is a string that has to survive three
# layers of quoting. @ONLY so that nothing but the key's own placeholder is substituted.
configure_file(update_release_key.h.in
               "${CMAKE_CURRENT_BINARY_DIR}/generated/update_release_key.h" @ONLY)
target_include_directories(ddd_capture PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
