# Test suite for the FX3 firmware.
#
# Tiers, as defined in TESTING.md, are attached to each test as a CTest label:
#   unit    T1  host-native, exercising a source file the firmware also compiles
#   golden  T2  host-native, output compared against committed reference data
#
# Most of this project cannot be tested here: it is freestanding ARM926EJ-S code calling
# into the Cypress SDK, and the build host cannot execute any of it. Two things can be.
# The host-side tooling that decides what ends up in the image — which is where the
# version stamp comes from. And any firmware source deliberately written without the SDK
# so that it compiles on the host too, which today means fpga-register-map.c.
#
# That second kind is worth the effort it takes to keep SDK-free. The decisions in it —
# which registers a host may write, which requests are refused — are exactly the sort that
# fail quietly on hardware, by allowing something rather than by crashing.

# T2 — the generated USB product descriptor. Fixed commit strings in, byte-for-byte
# comparison against committed reference headers, including the computed length byte.
add_test(
    NAME descriptor-golden
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/descriptor-golden.sh
        ${CMAKE_CURRENT_SOURCE_DIR}/../generate-descriptor.sh
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_tests_properties(descriptor-golden PROPERTIES LABELS "golden")

# T1 — the FPGA register map decisions.
#
# CMAKE_TOOLCHAIN_FILE applies to the whole build tree, so this cannot be an ordinary
# add_executable: that would cross-compile it for the FX3, which cannot run it. Compiling
# directly with a host compiler is the same route the firmware build already takes for
# fx3-mkimage, and for the same reason.
find_program(HOST_CC NAMES cc gcc clang)

if(HOST_CC)
    set(REGISTER_MAP_TEST ${CMAKE_CURRENT_BINARY_DIR}/register-map-test)
    set(REGISTER_MAP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/register-map-test.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/fpga-register-map.c
    )

    add_custom_command(
        OUTPUT ${REGISTER_MAP_TEST}
        COMMAND ${HOST_CC} -std=c11 -O1 -Wall -Wextra -Werror
                -I${CMAKE_CURRENT_SOURCE_DIR}/../src
                -o ${REGISTER_MAP_TEST} ${REGISTER_MAP_SOURCES}
        DEPENDS ${REGISTER_MAP_SOURCES}
                ${CMAKE_CURRENT_SOURCE_DIR}/../src/fpga-register-map.h
        COMMENT "Building host test register-map-test"
        VERBATIM
    )

    add_custom_target(register_map_test ALL DEPENDS ${REGISTER_MAP_TEST})

    add_test(NAME register-map COMMAND ${REGISTER_MAP_TEST})
    set_tests_properties(register-map PROPERTIES LABELS "unit")

    # T1 — the device update protocol's decisions, built the same way and for the same
    # reason. This one earns its keep more than most: what it covers is the arithmetic
    # that decides where each byte of a firmware image lands in the boot EEPROM, and the
    # cost of getting that wrong is a device that will not enumerate.
    set(UPDATE_PROTOCOL_TEST ${CMAKE_CURRENT_BINARY_DIR}/update-protocol-test)
    set(UPDATE_PROTOCOL_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/update-protocol-test.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/update-protocol.c
    )

    add_custom_command(
        OUTPUT ${UPDATE_PROTOCOL_TEST}
        COMMAND ${HOST_CC} -std=c11 -O1 -Wall -Wextra -Werror
                -I${CMAKE_CURRENT_SOURCE_DIR}/../src
                -o ${UPDATE_PROTOCOL_TEST} ${UPDATE_PROTOCOL_SOURCES}
        DEPENDS ${UPDATE_PROTOCOL_SOURCES}
                ${CMAKE_CURRENT_SOURCE_DIR}/../src/update-protocol.h
        COMMENT "Building host test update-protocol-test"
        VERBATIM
    )

    add_custom_target(update_protocol_test ALL DEPENDS ${UPDATE_PROTOCOL_TEST})

    add_test(NAME update-protocol COMMAND ${UPDATE_PROTOCOL_TEST})
    set_tests_properties(update-protocol PROPERTIES LABELS "unit")
else()
    message(WARNING
        "No host C compiler found; the host-side unit tests will not be built or run.")
endif()
