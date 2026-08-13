# Test suite for the capture application.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Tiers, as defined in TESTING.md, are attached to each test as a CTest label:
#   unit        T1  host-native, no I/O, no hardware
#   golden      T2  host-native, compared against committed reference data
#   functional  T1  host-native pipeline and soak tests driven by the synthetic source
#
# Run everything:            ctest --test-dir build
# Run one tier:              ctest --test-dir build -L unit
# Everything but hardware:   ctest --test-dir build -LE hil
#
# Tests are grouped into binaries by what they need, not by what they cover:
#
#   ddd_capture_tests   the engine. Links no Qt at all, which is the check that the engine
#                       has not grown a Qt dependency — if it ever does, this stops linking.
#   ddd_gui_tests       the Qt layer. Runs under a QCoreApplication rather than a
#                       QApplication: no display is needed for models and controllers, and
#                       a test that needs one cannot run on a CI runner.

find_package(GTest REQUIRED)
include(GoogleTest)

# Adds a test executable, links it against GoogleTest, and labels every test it discovers
# with its tier(s). `tiers` is a CMake list, so pass "unit" or "unit;golden".
function(ddd_add_test name tiers)
    add_executable(${name} ${ARGN})
    target_compile_options(${name} PRIVATE -Wall -Wextra)

    # The separators must survive into the generated set_tests_properties() call. Left
    # unescaped, CMake expands the list there and reads the second tier as a property name,
    # so only the first label is ever applied.
    string(REPLACE ";" "\\;" tiers_escaped "${tiers}")

    gtest_discover_tests(${name}
        PROPERTIES LABELS "${tiers_escaped}"
        DISCOVERY_MODE PRE_TEST
    )

    # Collected for the top-level format and lint gates, which are configured after this
    # directory has been added and so cannot name these targets directly.
    set_property(GLOBAL APPEND PROPERTY DDD_TEST_TARGETS ${name})
endfunction()

# T1 — the engine. GTest::gtest_main supplies main(): nothing here needs an event loop.
ddd_add_test(ddd_capture_tests "unit"
    unit/test_logger.cpp
    unit/test_sample_format.cpp
    unit/test_test_pattern_verifier.cpp
    unit/test_sequence_validator.cpp
    unit/test_disk_buffer_ring.cpp
    unit/test_monitor_tap.cpp
    unit/test_capture_pipeline.cpp
    unit/test_firmware_version.cpp
    unit/test_usb_device.cpp
    unit/test_device_monitor.cpp
    unit/test_capture_naming.cpp
    unit/test_capture_provenance.cpp
    unit/test_free_space.cpp
)
target_link_libraries(ddd_capture_tests PRIVATE
    ddd_capture
    GTest::gtest
    GTest::gtest_main
)
target_include_directories(ddd_capture_tests PRIVATE support)

# T1 — the display mathematics. Links no Qt either, for the same reason ddd_capture_tests
# does not: a QPainter cannot be unit tested, and everything worth testing about a plot is
# the arithmetic behind it rather than the pixels it produced.
ddd_add_test(ddd_analysis_tests "unit"
    analysis/test_front_end_gain.cpp
    analysis/test_waveform_mapping.cpp
    analysis/test_amplitude_history.cpp
    analysis/test_signal_levels.cpp
    analysis/test_fourier_transform.cpp
    analysis/test_spectrum_analyser.cpp
    analysis/test_spectrogram_history.cpp
)
target_link_libraries(ddd_analysis_tests PRIVATE
    ddd_analysis
    GTest::gtest
    GTest::gtest_main
)

# T1 + T2 — the writer and the reader, round-tripped against each other and against
# what a FLAC file is required to look like on disk. Labelled golden as well as unit
# because some of it checks bytes at fixed offsets in the container, which is a
# reference comparison rather than a behavioural one.
ddd_add_test(ddd_capture_format_tests "unit;golden"
    golden/test_flac_round_trip.cpp
    golden/test_test_data_analysis.cpp
)
target_link_libraries(ddd_capture_format_tests PRIVATE
    ddd_capture
    GTest::gtest
    GTest::gtest_main
)

# T1 — the whole pipeline at the device's real rate, driven by the synthetic source.
#
# Labelled functional and nothing else, so the ordinary development loop
# (`ctest -L unit`) stays fast and this runs when the full suite does. It takes minutes,
# not seconds: that is the point of it.
ddd_add_test(ddd_pipeline_soak_tests "functional"
    functional/test_pipeline_soak.cpp
)
target_link_libraries(ddd_pipeline_soak_tests PRIVATE
    ddd_capture
    GTest::gtest
    GTest::gtest_main
)

# T5 — the real device. Excluded from every ordinary run by its label, and it does not
# skip when no device is attached: it fails. A hardware test that "passes" because there
# was no hardware is worse than no test at all.
#
#   ctest --test-dir build -L hil
#
# Nothing here reprogrammes anything. These stream from the device and send the 0xB6
# configuration request, which is what the application does in normal use; writing the
# FX3 EEPROM or the FPGA flash stays a manual procedure (AGENTS.md §4).
ddd_add_test(ddd_hardware_tests "hil"
    hardware/test_device_capture.cpp
)
target_link_libraries(ddd_hardware_tests PRIVATE
    ddd_capture
    GTest::gtest
    GTest::gtest_main
)

# T1 — the Qt layer. Its own main() builds a QCoreApplication, so queued connections and
# timers are delivered when a test pumps the event loop.
ddd_add_test(ddd_gui_tests "unit"
    support/gui_test_main.cpp
    gui/unit/test_about_text.cpp
    gui/unit/test_theme_manager.cpp
    gui/unit/test_log_message_model.cpp
    gui/unit/test_application_logger.cpp
    gui/unit/test_capture_settings.cpp
    gui/unit/test_capture_controller.cpp
    gui/unit/test_capture_to_disk.cpp
    gui/unit/test_capture_faults.cpp
    gui/unit/test_capture_failure_presenter.cpp
    gui/unit/test_analysis_cli.cpp
    gui/unit/test_statistics_presenter.cpp
    gui/unit/test_analysis_worker.cpp
)
target_link_libraries(ddd_gui_tests PRIVATE
    ddd_gui_lib
    Qt6::Test
    GTest::gtest
)
target_include_directories(ddd_gui_tests PRIVATE support)

# T1 — the widgets. Separate from ddd_gui_tests because these need a QApplication and a
# platform plugin, where those need neither; keeping them apart means the cheaper binary
# stays runnable in environments that cannot host a QApplication at all.
ddd_add_test(ddd_gui_widget_tests "unit"
    support/widget_test_main.cpp
    gui/widget/test_main_window_panels.cpp
    gui/widget/test_about_dialog.cpp
    gui/widget/test_analysis_dialog.cpp
    gui/widget/test_capture_panel.cpp
    gui/widget/test_statistics_panel.cpp
    gui/widget/test_waveform_panel.cpp
    gui/widget/test_spectrum_panel.cpp
    gui/widget/test_amplitude_panel.cpp
)
target_link_libraries(ddd_gui_widget_tests PRIVATE
    ddd_gui_lib
    Qt6::Test
    GTest::gtest
)
target_include_directories(ddd_gui_widget_tests PRIVATE support)
