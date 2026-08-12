# Test suite for fx3-mkimage.
#
# Tiers, as defined in TESTING.md, are attached to each test as a CTest label:
#   unit    T1  host-native, no I/O, no hardware
#   golden  T2  host-native, output compared against committed reference data
#
# The whole tool is pure: ELF buffer in, image buffer out. That is why the
# logic lives in fx3-bootimage.c with all file I/O in main.c — everything worth
# testing can be tested without touching a filesystem or a device.
#
# There is no test here comparing against the vendor tool it replaces, because
# that tool is no longer in the tree. That comparison was the acceptance check
# for the replacement and is recorded in ../README.md; what guards the format
# from here on is the golden vectors below.

find_package(GTest REQUIRED)
include(GoogleTest)

function(ddd_add_test name tiers)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE fx3-bootimage GTest::gtest GTest::gtest_main)
    target_compile_options(${name} PRIVATE -Wall -Wextra)

    # The separators must survive into the generated set_tests_properties() call. Left
    # unescaped, CMake expands the list there and reads the second tier as a property name,
    # so only the first label is ever applied.
    string(REPLACE ";" "\\;" tiers_escaped "${tiers}")

    gtest_discover_tests(${name}
        PROPERTIES LABELS "${tiers_escaped}"
        DISCOVERY_MODE PRE_TEST
    )
endfunction()

# T1 + T2 — boot image construction. A defect here produces an image the FX3
# bootloader silently refuses, or worse, one it accepts and runs incorrectly.
ddd_add_test(test_bootimage "unit;golden" test_bootimage.cpp)
