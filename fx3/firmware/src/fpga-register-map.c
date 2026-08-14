/************************************************************************

    fpga-register-map.c

    The FPGA register map, and the decisions about it that need no hardware
    DomesdayDuplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "fpga-register-map.h"

// The gateware writes lower case, but accepting either costs nothing and
// means a future generator that does not is a cosmetic difference rather
// than a commit that silently reads as empty.
static int isHexDigit(uint8_t character)
{
    return ((character >= '0' && character <= '9') ||
            (character >= 'a' && character <= 'f') ||
            (character >= 'A' && character <= 'F')) ? 1 : 0;
}

int fpgaIdentityIsValid(const uint8_t *identity)
{
    if (identity == NULL) {
        return 0;
    }

    return (identity[FPGA_REGISTER_ID] == FPGA_IDENTITY_VALUE) ? 1 : 0;
}

uint8_t fpgaIdentityMapVersion(const uint8_t *identity)
{
    if (!fpgaIdentityIsValid(identity)) {
        return 0u;
    }

    return identity[FPGA_REGISTER_MAP_VERSION];
}

int fpgaIdentityIsDirty(const uint8_t *identity)
{
    if (!fpgaIdentityIsValid(identity)) {
        return 0;
    }

    return ((identity[FPGA_REGISTER_BUILD_FLAGS] & FPGA_BUILD_FLAG_DIRTY) != 0u) ? 1 : 0;
}

uint8_t fpgaIdentityImageRole(const uint8_t *identity)
{
    if (!fpgaIdentityIsValid(identity)) {
        return FPGA_IMAGE_ROLE_APPLICATION;
    }

    // A gateware whose map predates the register has no answer to give, and
    // the honest reading of that is "this is the one image there is".
    if (fpgaIdentityMapVersion(identity) < FPGA_MAP_VERSION_WITH_BRIDGE) {
        return FPGA_IMAGE_ROLE_APPLICATION;
    }

    return identity[FPGA_REGISTER_IMAGE_ROLE];
}

int fpgaIdentityHasFlashBridge(const uint8_t *identity)
{
    if (!fpgaIdentityIsValid(identity)) {
        return 0;
    }

    // A range with no upper bound, because the bridge is additive: a later
    // map version may add registers beside it but cannot take it away
    // without being a different device.
    return (fpgaIdentityMapVersion(identity) >= FPGA_MAP_VERSION_WITH_BRIDGE)
        ? 1 : 0;
}

void fpgaIdentityCommitText(const uint8_t *identity, char *text, size_t size)
{
    size_t index = 0u;

    if (text == NULL || size == 0u) {
        return;
    }

    text[0] = '\0';

    if (!fpgaIdentityIsValid(identity)) {
        return;
    }

    // The valid bit is positive logic, so every "I do not know" case — a
    // gateware built outside a checkout, a block that was never read — reads
    // as no commit rather than as a confident claim about commit 00000000.
    if ((identity[FPGA_REGISTER_BUILD_FLAGS] & FPGA_BUILD_FLAG_COMMIT) == 0u) {
        return;
    }

    while (index < FPGA_COMMIT_LENGTH && (index + 1u) < size) {
        const uint8_t character = identity[FPGA_REGISTER_COMMIT + index];

        // A short commit is null padded, and anything that is not a hex digit
        // is not part of one either way
        if (!isHexDigit(character)) {
            break;
        }

        text[index] = (char)character;
        index++;
    }

    text[index] = '\0';
}

int fpgaRegisterIsHostWritable(uint8_t address)
{
    // Test mode is the only register the host has any business writing.
    //
    // The LED register is excluded even though the gateware would accept the
    // write, because the LEDs are a status output and status outputs have
    // exactly one owner. Two writers means the display shows whichever wrote
    // last, which is worse than useless during a fault — the state the LEDs
    // exist to report is the state where you can least afford to distrust
    // them.
    return (address == FPGA_REGISTER_TEST_MODE) ? 1 : 0;
}

int fpgaReadRequestIsValid(uint16_t address, uint16_t length)
{
    if (address > FPGA_REGISTER_ADDRESS_MAX) {
        return 0;
    }

    // Unmapped addresses are readable on purpose: reading one is how a host
    // discovers a register does not exist, and it gets zero back. Only the
    // address space itself is bounded here.
    if (length == 0u || length > FPGA_REGISTER_READ_MAX) {
        return 0;
    }

    return 1;
}

int fpgaWriteRequestIsValid(uint16_t value)
{
    const uint16_t address = (uint16_t)(value >> 8);

    if (address > FPGA_REGISTER_ADDRESS_MAX) {
        return 0;
    }

    return fpgaRegisterIsHostWritable((uint8_t)address);
}
