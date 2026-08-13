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
)
target_link_libraries(ddd_capture_tests PRIVATE
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
)
target_link_libraries(ddd_gui_tests PRIVATE
    ddd_gui_lib
    Qt6::Test
    GTest::gtest
)

# T1 — the widgets. Separate from ddd_gui_tests because these need a QApplication and a
# platform plugin, where those need neither; keeping them apart means the cheaper binary
# stays runnable in environments that cannot host a QApplication at all.
ddd_add_test(ddd_gui_widget_tests "unit"
    support/widget_test_main.cpp
    gui/widget/test_main_window_panels.cpp
)
target_link_libraries(ddd_gui_widget_tests PRIVATE
    ddd_gui_lib
    Qt6::Test
    GTest::gtest
)
