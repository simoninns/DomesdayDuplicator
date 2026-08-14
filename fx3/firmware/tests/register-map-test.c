/************************************************************************

    register-map-test.c

    T1 unit test for the FPGA register map decisions
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    Compiled and run on the build host, not the FX3. That is possible at
    all only because fpga-register-map.c deliberately does not include the
    Cypress SDK — everything it does is arithmetic over bytes the host sent
    or the FPGA returned, and none of it needs a device.

    What is covered here is the half of the register interface that can be
    got wrong quietly: which registers the host may write, which requests
    are refused, and how an identity block turns into a commit. The SPI
    transport in fpga-registers.c has no coverage here and cannot have any
    — it is GPIO edges, and the only test for it is a board.

************************************************************************/

#include <stdio.h>
#include <string.h>

#include "fpga-register-map.h"

static int failures = 0;

static void check(int condition, const char *what)
{
    if (!condition) {
        printf("FAIL: %s\n", what);
        failures++;
    }
}

static void checkText(const char *got, const char *want, const char *what)
{
    if (strcmp(got, want) != 0) {
        printf("FAIL: %s: got \"%s\", expected \"%s\"\n", what, got, want);
        failures++;
    }
}

// Build an identity block: signature, map version, build flags, then the
// commit characters, null padded to the full eight.
static void makeIdentity(uint8_t *identity, uint8_t id, uint8_t mapVersion,
                         uint8_t flags, const char *commit)
{
    size_t index;

    memset(identity, 0, FPGA_IDENTITY_LENGTH);
    identity[FPGA_REGISTER_ID] = id;
    identity[FPGA_REGISTER_MAP_VERSION] = mapVersion;
    identity[FPGA_REGISTER_BUILD_FLAGS] = flags;

    for (index = 0u; index < FPGA_COMMIT_LENGTH && commit[index] != '\0'; index++) {
        identity[FPGA_REGISTER_COMMIT + index] = (uint8_t)commit[index];
    }
}

static void testIdentityValidity(void)
{
    uint8_t identity[FPGA_IDENTITY_LENGTH];

    makeIdentity(identity, FPGA_IDENTITY_VALUE, 1u, FPGA_BUILD_FLAG_COMMIT, "7713495d");
    check(fpgaIdentityIsValid(identity), "a real identity block is recognised");

    // The two values a missing FPGA produces. SPI has no acknowledgement, so
    // these are what an absent board looks like: MISO floating high, or held
    // low. Reading either as a valid gateware would report a version for a
    // device that is not there.
    makeIdentity(identity, 0x00u, 1u, FPGA_BUILD_FLAG_COMMIT, "7713495d");
    check(!fpgaIdentityIsValid(identity), "an all-zero block is rejected");

    makeIdentity(identity, 0xFFu, 1u, FPGA_BUILD_FLAG_COMMIT, "7713495d");
    check(!fpgaIdentityIsValid(identity), "an all-ones block is rejected");

    check(!fpgaIdentityIsValid(NULL), "a null block is rejected");

    makeIdentity(identity, FPGA_IDENTITY_VALUE, 3u, 0u, "");
    check(fpgaIdentityMapVersion(identity) == 3u, "the map version is reported");

    makeIdentity(identity, 0x00u, 3u, 0u, "");
    check(fpgaIdentityMapVersion(identity) == 0u,
          "an invalid block reports no map version rather than its second byte");
}

static void testCommitText(void)
{
    uint8_t identity[FPGA_IDENTITY_LENGTH];
    char text[FPGA_COMMIT_LENGTH + 1];

    // The ordinary case: eight characters from CMake's --short=8
    makeIdentity(identity, FPGA_IDENTITY_VALUE, 1u, FPGA_BUILD_FLAG_COMMIT, "7713495d");
    fpgaIdentityCommitText(identity, text, sizeof(text));
    checkText(text, "7713495d", "an eight-character commit");
    check(!fpgaIdentityIsDirty(identity), "a clean build is not reported dirty");

    // Seven characters, which is what a Nix build stamps. The eighth byte is
    // a null, and must end the string rather than appear in it.
    makeIdentity(identity, FPGA_IDENTITY_VALUE, 1u, FPGA_BUILD_FLAG_COMMIT, "7713495");
    fpgaIdentityCommitText(identity, text, sizeof(text));
    checkText(text, "7713495", "a seven-character commit");

    makeIdentity(identity, FPGA_IDENTITY_VALUE, 1u,
                 FPGA_BUILD_FLAG_COMMIT | FPGA_BUILD_FLAG_DIRTY, "7713495d");
    fpgaIdentityCommitText(identity, text, sizeof(text));
    checkText(text, "7713495d", "a dirty build still names its commit");
    check(fpgaIdentityIsDirty(identity), "a dirty build is reported dirty");

    // The valid bit is positive logic, so a gateware built outside a checkout
    // reports no commit rather than a confident claim about commit 00000000.
    makeIdentity(identity, FPGA_IDENTITY_VALUE, 1u, 0u, "7713495d");
    fpgaIdentityCommitText(identity, text, sizeof(text));
    checkText(text, "", "no commit flag means no commit, whatever the bytes say");

    makeIdentity(identity, 0x00u, 1u, FPGA_BUILD_FLAG_COMMIT, "7713495d");
    fpgaIdentityCommitText(identity, text, sizeof(text));
    checkText(text, "", "an invalid block yields no commit");

    // A misread link must not be able to put arbitrary bytes into the debug
    // console, so anything that is not a hex digit ends the string.
    makeIdentity(identity, FPGA_IDENTITY_VALUE, 1u, FPGA_BUILD_FLAG_COMMIT, "77ZZ4567");
    fpgaIdentityCommitText(identity, text, sizeof(text));
    checkText(text, "77", "a non-hex character ends the commit");

    // A caller with a short buffer gets a truncated but terminated string
    {
        char small[4];
        makeIdentity(identity, FPGA_IDENTITY_VALUE, 1u, FPGA_BUILD_FLAG_COMMIT, "7713495d");
        fpgaIdentityCommitText(identity, small, sizeof(small));
        checkText(small, "771", "a short buffer truncates rather than overruns");
    }
}

static void testHostWritable(void)
{
    // Test mode is the only register the host has any business writing
    check(fpgaRegisterIsHostWritable(FPGA_REGISTER_TEST_MODE),
          "the host may write test mode");

    // The gateware would accept this write. The firmware refuses to relay it,
    // because the LEDs are a status output with exactly one owner.
    check(!fpgaRegisterIsHostWritable(FPGA_REGISTER_LED),
          "the host may not write the LED register");

    check(!fpgaRegisterIsHostWritable(FPGA_REGISTER_ID),
          "the host may not write a read-only register");
    check(!fpgaRegisterIsHostWritable(0x20u),
          "the host may not write an unmapped register");
}

static void testReadRequests(void)
{
    // The identity block, which is the request that matters most
    check(fpgaReadRequestIsValid(FPGA_REGISTER_ID, FPGA_IDENTITY_LENGTH),
          "the identity block may be read in one request");

    check(fpgaReadRequestIsValid(FPGA_REGISTER_ADDRESS_MAX, 1u),
          "the last address may be read");
    check(!fpgaReadRequestIsValid(FPGA_REGISTER_ADDRESS_MAX + 1u, 1u),
          "an address past the seven-bit space is refused");

    // Unmapped addresses are readable on purpose: that is how a host finds
    // out a register does not exist, and it gets zero back.
    check(fpgaReadRequestIsValid(0x20u, 1u), "an unmapped address may be read");

    check(!fpgaReadRequestIsValid(0u, 0u), "a zero-length read is refused");
    check(fpgaReadRequestIsValid(0u, FPGA_REGISTER_READ_MAX),
          "the largest allowed read is accepted");
    check(!fpgaReadRequestIsValid(0u, FPGA_REGISTER_READ_MAX + 1u),
          "a read past the staging buffer is refused");
}

static void testWriteRequests(void)
{
    // Turning test mode on and off, which is the only write the host makes
    check(fpgaWriteRequestIsValid(0x1001u), "test mode on is accepted");
    check(fpgaWriteRequestIsValid(0x1000u), "test mode off is accepted");

    check(!fpgaWriteRequestIsValid(0x1101u), "a write to the LED register is refused");
    check(!fpgaWriteRequestIsValid(0x0044u), "a write to a read-only register is refused");
    check(!fpgaWriteRequestIsValid(0x2000u), "a write to an unmapped register is refused");
    check(!fpgaWriteRequestIsValid(0x8000u), "a write past the address space is refused");
}

int main(void)
{
    testIdentityValidity();
    testCommitText();
    testHostWritable();
    testReadRequests();
    testWriteRequests();

    if (failures != 0) {
        printf("register-map-test: FAIL (%d failures)\n", failures);
        return 1;
    }

    printf("register-map-test: PASS\n");
    return 0;
}
