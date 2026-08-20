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
    unit/test_log_format.cpp
    unit/test_log_options.cpp
    unit/test_fill_history.cpp
    unit/test_spdlog_logger.cpp
    unit/test_sample_format.cpp
    unit/test_test_pattern_verifier.cpp
    unit/test_sequence_validator.cpp
    unit/test_sample_metrics.cpp
    unit/test_disk_buffer_ring.cpp
    unit/test_monitor_tap.cpp
    unit/test_capture_pipeline.cpp
    unit/test_firmware_version.cpp
    unit/test_fpga_version.cpp
    unit/test_fpga_telemetry.cpp
    unit/test_usb_device.cpp
    unit/test_device_monitor.cpp
    unit/test_sysfs_device_list.cpp
    unit/test_capture_naming.cpp
    unit/test_capture_provenance.cpp
    unit/test_capture_metadata.cpp
    unit/test_yaml_writer.cpp
    unit/test_free_space.cpp
)
target_link_libraries(ddd_capture_tests PRIVATE
    ddd_capture
    GTest::gtest
    GTest::gtest_main
)
target_include_directories(ddd_capture_tests PRIVATE support)

# T1 — the device-update bundle: the digest, the JSON reader, the manifest, the archive
# and the signature verifier.
#
# A binary of its own rather than more files in ddd_capture_tests, because this is the
# only part of the engine whose input is a file a user downloaded from somewhere. Keeping
# it separate means `ctest -R update` runs exactly the suite that covers that surface,
# which is the suite anyone touching the update path needs to be able to run in a second.
ddd_add_test(ddd_update_tests "unit"
    unit/test_digest.cpp
    unit/test_json_value.cpp
    unit/test_minisign_verify.cpp
    unit/test_update_manifest.cpp
    unit/test_update_bundle.cpp
    unit/test_update_key.cpp
    unit/test_boot_image.cpp
    unit/test_device_updater.cpp
    unit/test_device_recovery.cpp
    unit/test_update_gate.cpp
    unit/test_update_orchestrator.cpp
    unit/test_bringup_orchestrator.cpp
    unit/test_update_cli.cpp
)
target_link_libraries(ddd_update_tests PRIVATE
    ddd_capture
    GTest::gtest
    GTest::gtest_main
)
target_include_directories(ddd_update_tests PRIVATE support)

# T1 — the JTAG programming path: the USB-Blaster's wire protocol, the SVF player and
# the TAP state machine it walks, and the shell tool's contract.
#
# A binary of its own for the reason ddd_update_tests is one: this is the surface that
# writes an FPGA's configuration flash, and `ctest -R jtag` is the suite anyone touching
# it needs to be able to run in a second. Nothing here touches hardware — the cable is a
# fake byte pipe and the player's far end is a fake cable, which between them cover
# everything but whether a real device agrees (TESTING.md, B-V1).
ddd_add_test(ddd_jtag_tests "unit"
    unit/test_usb_blaster_cable.cpp
    unit/test_svf_player.cpp
    unit/test_jtag_cli.cpp
)
target_link_libraries(ddd_jtag_tests PRIVATE
    ddd_capture
    GTest::gtest
    GTest::gtest_main
)
target_include_directories(ddd_jtag_tests PRIVATE support)

# T1 — the LaserDisc player protocol. Links no Qt, which is the check that the protocol
# has not grown one: the whole of it — probing for a player, identifying the model,
# encoding every command and reading every reply — runs against a scripted fake serial
# port, on a machine with no player and no adapter attached.
#
# The fake carries the clock as well as the bytes, so the timeouts are exercised rather
# than waited out: a command that gives up after five seconds does so instantly here, and
# does so at exactly the moment it should.
ddd_add_test(ddd_player_tests "unit"
    player/test_player_registry.cpp
    player/test_player_controls.cpp
    player/test_command_encoder.cpp
    player/test_disc_profile.cpp
    player/test_disc_examiner.cpp
    player/test_auto_capture_plan.cpp
    player/test_auto_capture_sequence.cpp
    player/test_response_parser.cpp
    player/test_user_code.cpp
    player/test_player_session.cpp
)
target_link_libraries(ddd_player_tests PRIVATE
    ddd_player
    GTest::gtest
    GTest::gtest_main
)
target_include_directories(ddd_player_tests PRIVATE support)

# T1 — the display mathematics. Links no Qt either, for the same reason ddd_capture_tests
# does not: a QPainter cannot be unit tested, and everything worth testing about a plot is
# the arithmetic behind it rather than the pixels it produced.
ddd_add_test(ddd_analysis_tests "unit"
    analysis/test_front_end_gain.cpp
    analysis/test_frequency_axis.cpp
    analysis/test_sinc_interpolation.cpp
    analysis/test_waveform_mapping.cpp
    analysis/test_waveform_trigger.cpp
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
    golden/test_stock_tar_bundle.cpp
)
target_link_libraries(ddd_capture_format_tests PRIVATE
    ddd_capture
    GTest::gtest
    GTest::gtest_main
)
target_include_directories(ddd_capture_format_tests PRIVATE support)

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

# T5 — a real LaserDisc player, which is different hardware from the one above and is
# not on every bench that has a Duplicator. Its own label for exactly that reason: a
# machine with a device and no player must not fail tests it cannot run.
#
#   ctest --test-dir build -L hil-player
#
# Note that `-L hil` matches this label too — CTest's label selection is an unanchored
# regular expression — so the device tier alone is `-L "^hil$"`. Excluding with `-LE hil`
# does the right thing for both, which is what CI and the Nix build do.
#
# Links the Qt layer because the real serial backend lives there, and it is the real
# backend that is being tested: the protocol above it has already been exercised in
# ddd_player_tests against a scripted port, and what is left for hardware is whether the
# bytes mean to a player what the manuals say.
ddd_add_test(ddd_player_hardware_tests "hil-player"
    support/gui_test_main.cpp
    hardware/test_player_hardware.cpp
)
target_link_libraries(ddd_player_hardware_tests PRIVATE
    ddd_gui_lib
    GTest::gtest
)
target_include_directories(ddd_player_hardware_tests PRIVATE support)

# T1 — the Qt layer. Its own main() builds a QCoreApplication, so queued connections and
# timers are delivered when a test pumps the event loop.
ddd_add_test(ddd_gui_tests "unit"
    support/gui_test_main.cpp
    gui/unit/test_about_text.cpp
    gui/unit/test_bringup_text.cpp
    gui/unit/test_bundled_update.cpp
    gui/unit/test_firmware_text.cpp
    gui/unit/test_update_text.cpp
    gui/unit/test_update_steps.cpp
    gui/unit/test_platform_description.cpp
    gui/unit/test_theme_manager.cpp
    gui/unit/test_log_message_model.cpp
    gui/unit/test_application_logger.cpp
    gui/unit/test_capture_settings.cpp
    gui/unit/test_capture_cli.cpp
    gui/unit/test_capture_control_server.cpp
    gui/unit/test_capture_controller.cpp
    gui/unit/test_signal_watcher.cpp
    gui/unit/test_headless_capture_runner.cpp
    gui/unit/test_capture_to_disk.cpp
    gui/unit/test_capture_faults.cpp
    gui/unit/test_capture_failure_presenter.cpp
    gui/unit/test_analysis_cli.cpp
    gui/unit/test_statistics_presenter.cpp
    gui/unit/test_analysis_worker.cpp
    gui/unit/test_player_discovery.cpp
    gui/unit/test_player_settings.cpp
    gui/unit/test_player_text.cpp
    gui/unit/test_player_controller.cpp
    gui/unit/test_auto_capture_controller.cpp
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
    gui/widget/test_floating_dock_drag.cpp
    gui/widget/test_about_dialog.cpp
    gui/widget/test_firmware_dialog.cpp
    gui/widget/test_update_page.cpp
    gui/widget/test_analysis_dialog.cpp
    gui/widget/test_capture_panel.cpp
    gui/widget/test_capture_naming_dialog.cpp
    gui/widget/test_examine_dialog.cpp
    gui/widget/test_capture_plan_form.cpp
    gui/widget/test_auto_capture_wizard.cpp
    gui/widget/test_board_bringup_wizard.cpp
    gui/widget/test_player_remote_dialog.cpp
    gui/widget/test_settings_dialog.cpp
    gui/widget/test_log_panel.cpp
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
