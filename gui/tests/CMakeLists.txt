# Test suite for the GUI component.
#
# Tiers, as defined in TESTING.md, are attached to each test as a CTest label:
#   unit    T1  host-native, no I/O, no hardware
#   golden  T2  host-native, compared against committed reference data
#
# Run everything:            ctest --test-dir build
# Run one tier:              ctest --test-dir build -L unit
# Everything but hardware:   ctest --test-dir build -LE hil

find_package(GTest REQUIRED)
include(GoogleTest)

# Add a test executable, link it against GoogleTest, and label it with its tier(s).
# `tiers` is a CMake list, so pass "unit" or "unit;golden".
function(ddd_add_test name tiers)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE GTest::gtest GTest::gtest_main)
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

# T1 — StringUtilities is header-only and pure. This doubles as the smoke test for the
# harness itself: if it fails to build, nothing else in the suite means anything.
ddd_add_test(test_stringutilities "unit" test_stringutilities.cpp)
target_include_directories(test_stringutilities PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/DomesdayDuplicator
)

# T1 + T2 — the 10-bit/16-bit sample codec: round-trip properties plus golden vectors.
# A defect here silently corrupts every capture that is ever converted.
ddd_add_test(test_samplecodec "unit;golden" test_samplecodec.cpp)
target_include_directories(test_samplecodec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/dddconv
)
