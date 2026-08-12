# Test suite for the FX3 firmware.
#
# Tiers, as defined in TESTING.md, are attached to each test as a CTest label:
#   golden  T2  host-native, output compared against committed reference data
#
# There is no unit tier here and there cannot be a meaningful one: every source file in
# this project is freestanding ARM926EJ-S code calling into the Cypress SDK, so the build
# host cannot execute any of it. What *is* testable is the host-side tooling that decides
# what ends up in the image — which is where the version stamp comes from.

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
