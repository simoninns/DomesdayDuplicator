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
# A defect here silently misreads every legacy capture that is ever analysed.
ddd_add_test(test_samplecodec "unit;golden" test_samplecodec.cpp)
target_include_directories(test_samplecodec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common
)

# T1 — the test-pattern ramp check that backs the T5 capture-integrity gate (P7-19). The
# gate itself needs hardware; this is the part that can be checked without any, and a
# broken analyser would make the hardware gate report a pass it had not earned.
ddd_add_test(test_testdataanalyser "unit" test_testdataanalyser.cpp)
target_link_libraries(test_testdataanalyser PRIVATE ddd-common)

# T1 + T2 — the FLAC writer and the capture reader, round-tripped against each other and
# against golden vectors. This is what proves the .ldf the application writes is the format
# ld-decode reads (P7-23); the ld-compress and ld-decode interop checks that complete that
# proof need those tools present, so they are a separate scripted step rather than ctest.
ddd_add_test(test_flacroundtrip "unit;golden" test_flacroundtrip.cpp)
target_link_libraries(test_flacroundtrip PRIVATE ddd-common)

# Interop fixture generator for interop-ldf.sh (P7-23). Built with the tests, never
# installed: it exists so the interop script can produce a file through the production
# encoder, which is the only way to check the format claim against ld-decode's own tools.
add_executable(ddd-ldfgen tools/ldfgen.cpp)
target_link_libraries(ddd-ldfgen PRIVATE ddd-common)
set_target_properties(ddd-ldfgen PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tools")
