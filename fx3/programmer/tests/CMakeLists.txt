# Test suite for the FX3 programmer.
#
# Tiers, as defined in TESTING.md, are attached to each test as a CTest label:
#   unit    T1  host-native, no I/O, no hardware
#
# Actual USB transfers are T5 (hardware-in-the-loop) and are not covered here: mocking
# libusb at this scale would cost more than it proves. What is covered is the pure logic
# whose failure modes are silent and expensive.

find_package(GTest REQUIRED)
include(GoogleTest)

function(ddd_add_test name tiers)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE GTest::gtest GTest::gtest_main)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
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

# T1 — EEPROM and flash paging arithmetic. An off-by-one across a page or slave boundary
# writes to the wrong address and bricks the device.
ddd_add_test(test_paging "unit" test_paging.cpp)

# T1 — secondary-loader path resolution, the site of D13.
ddd_add_test(test_flashprog "unit" test_flashprog.cpp ../src/fx3-flashprog.c)
target_compile_definitions(test_flashprog PRIVATE
    FLASHPROG_INSTALL_PATH="${CMAKE_INSTALL_FULL_DATADIR}/domesday-duplicator/${FLASHPROG_IMAGE}"
)

# T2 — the command-line contract. Not a unit test: it runs the built binary and asserts the
# help text does not promise behaviour the tool lacks. D24 and D25 both lived for a long
# time because words and code could disagree with nothing to catch it.
add_test(
    NAME cli-contract
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cli-contract.sh $<TARGET_FILE:fx3-programmer>
)

set_tests_properties(cli-contract PROPERTIES LABELS "golden")
