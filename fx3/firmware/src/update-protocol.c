/************************************************************************

    update-protocol.c

    The device update protocol, and the decisions about it that need no
    hardware
    DomesdayDuplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "update-protocol.h"

// Read a little-endian 32-bit field out of a host-supplied buffer.
//
// Byte at a time rather than a cast, because the buffer is whatever the USB
// driver handed over and this code runs on a processor that would fault on
// an unaligned word load.
static uint32_t updateReadLittleEndian32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static void updateWriteLittleEndian16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void updateWriteLittleEndian32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
    out[2] = (uint8_t)((value >> 16) & 0xFFu);
    out[3] = (uint8_t)((value >> 24) & 0xFFu);
}

void updateStateReset(updateState_t *state)
{
    uint32_t index;

    if (state == NULL) return;

    state->phase = UPDATE_PHASE_IDLE;
    state->error = UPDATE_ERROR_NONE;
    state->target = UPDATE_TARGET_EEPROM;
    state->length = 0;
    state->received = 0;
    state->written = 0;
    state->verified = 0;
    state->nextChunk = 0;

    for (index = 0; index < UPDATE_DIGEST_LENGTH; index++) {
        state->digest[index] = 0;
    }
}

void updateStateFail(updateState_t *state, uint8_t error)
{
    if (state == NULL) return;

    // The first error wins. Once an update has failed, everything that
    // happens afterwards is a consequence, and reporting the last one would
    // name the symptom rather than the fault.
    if (state->phase == UPDATE_PHASE_FAILED) return;

    state->phase = UPDATE_PHASE_FAILED;
    state->error = error;
}

int updateBeginDecode(const uint8_t *data, uint16_t length, updateBegin_t *out)
{
    uint32_t index;

    if (data == NULL || out == NULL) return 0;
    if (length != UPDATE_BEGIN_LENGTH) return 0;

    out->length = updateReadLittleEndian32(data);
    for (index = 0; index < UPDATE_DIGEST_LENGTH; index++) {
        out->digest[index] = data[4 + index];
    }
    out->flags = updateReadLittleEndian32(data + 4 + UPDATE_DIGEST_LENGTH);

    // Every flag bit is reserved, so a set bit is a host asking for
    // behaviour this firmware does not have.
    if (out->flags != 0) return 0;

    return 1;
}

void updateStatusEncode(const updateState_t *state, uint8_t *out)
{
    if (state == NULL || out == NULL) return;

    out[0] = state->phase;
    out[1] = state->error;
    updateWriteLittleEndian16(out + 2, (uint16_t)UPDATE_MAX_CHUNK);
    updateWriteLittleEndian32(out + 4, state->received);
    updateWriteLittleEndian32(out + 8, state->written);
    updateWriteLittleEndian32(out + 12, state->verified);
}

uint8_t updateBeginIsAllowed(const updateState_t *state, uint8_t target,
                             uint32_t length, int captureRunning)
{
    if (state == NULL) return UPDATE_ERROR_SEQUENCE;

    // The EPCS target arrives with the gateware's flash bridge. Refused
    // rather than accepted-and-ignored, so a host built against a later
    // firmware finds out before it streams a megabyte.
    if (target != UPDATE_TARGET_EEPROM) return UPDATE_ERROR_TARGET;

    if (captureRunning) return UPDATE_ERROR_BUSY;

    // An update already under way is not restarted by a second BEGIN. Two
    // hosts updating one device is not a case to arbitrate between.
    if (updateIsInProgress(state)) return UPDATE_ERROR_SEQUENCE;

    if (length < UPDATE_IMAGE_MINIMUM_LENGTH) return UPDATE_ERROR_LENGTH;
    if (length > UPDATE_EEPROM_SIZE) return UPDATE_ERROR_LENGTH;

    return UPDATE_ERROR_NONE;
}

uint8_t updateChunkIsAllowed(const updateState_t *state, uint8_t target,
                             uint16_t index, uint16_t length)
{
    uint32_t remaining;

    if (state == NULL) return UPDATE_ERROR_SEQUENCE;
    if (target != state->target) return UPDATE_ERROR_TARGET;
    if (state->phase != UPDATE_PHASE_RECEIVING) return UPDATE_ERROR_SEQUENCE;
    if (index != state->nextChunk) return UPDATE_ERROR_SEQUENCE;

    if (length == 0) return UPDATE_ERROR_CHUNK;
    if (length > UPDATE_MAX_CHUNK) return UPDATE_ERROR_CHUNK;

    remaining = state->length - state->received;
    if ((uint32_t)length > remaining) return UPDATE_ERROR_OVERRUN;

    // Every chunk but the last is a whole number of EEPROM pages, so that a
    // chunk can be written straight to the medium with no assembly buffer
    // in between. The last chunk carries whatever is left and its final
    // page is zero-padded on the way out.
    if ((uint32_t)length < remaining &&
        (length % UPDATE_EEPROM_PAGE_SIZE) != 0) {
        return UPDATE_ERROR_CHUNK;
    }

    return UPDATE_ERROR_NONE;
}

uint8_t updateFinishIsAllowed(const updateState_t *state, uint8_t target)
{
    if (state == NULL) return UPDATE_ERROR_SEQUENCE;
    if (target != state->target) return UPDATE_ERROR_TARGET;
    if (state->phase != UPDATE_PHASE_RECEIVING) return UPDATE_ERROR_SEQUENCE;
    if (state->received != state->length) return UPDATE_ERROR_SHORT;

    return UPDATE_ERROR_NONE;
}

int updateIsInProgress(const updateState_t *state)
{
    if (state == NULL) return 0;

    return (state->phase == UPDATE_PHASE_RECEIVING ||
            state->phase == UPDATE_PHASE_WRITING ||
            state->phase == UPDATE_PHASE_VERIFYING) ? 1 : 0;
}

int updateImageIsPlausible(const uint8_t *first, uint32_t firstLength,
                           uint32_t totalLength)
{
    if (first == NULL) return 0;
    if (firstLength < 2) return 0;
    if (totalLength < UPDATE_IMAGE_MINIMUM_LENGTH) return 0;
    if (totalLength > UPDATE_EEPROM_SIZE) return 0;

    if (first[0] != UPDATE_IMAGE_SIGNATURE_0) return 0;
    if (first[1] != UPDATE_IMAGE_SIGNATURE_1) return 0;

    return 1;
}

uint8_t updateEepromSlaveAddress(uint32_t address)
{
    const uint32_t bank = address / UPDATE_EEPROM_SLAVE_SIZE;

    return (uint8_t)(UPDATE_EEPROM_SLAVE_BASE | ((bank & 0x07u) << 1));
}

uint16_t updateEepromSlaveOffset(uint32_t address)
{
    return (uint16_t)(address % UPDATE_EEPROM_SLAVE_SIZE);
}

uint16_t updateEepromWriteSpan(uint32_t address, uint32_t remaining)
{
    uint32_t toPageEnd;
    uint32_t span;

    if (remaining == 0) return 0;

    // A page write that runs off the end of a page wraps to the start of
    // the same page rather than continuing into the next one, so the page
    // boundary is a hard limit and not a performance hint. The slave
    // boundary is a multiple of the page size, so stopping at the page end
    // stops at the slave end too.
    toPageEnd = UPDATE_EEPROM_PAGE_SIZE -
                (address % UPDATE_EEPROM_PAGE_SIZE);

    span = (remaining < toPageEnd) ? remaining : toPageEnd;

    return (uint16_t)span;
}

uint16_t updateEepromReadSpan(uint32_t address, uint32_t remaining,
                              uint16_t bufferSize)
{
    uint32_t toSlaveEnd;
    uint32_t span;

    if (remaining == 0 || bufferSize == 0) return 0;

    // Reads have no page structure — the EEPROM's internal address counter
    // rolls through the whole slave — so only the slave boundary matters.
    toSlaveEnd = UPDATE_EEPROM_SLAVE_SIZE -
                 (address % UPDATE_EEPROM_SLAVE_SIZE);

    span = (remaining < toSlaveEnd) ? remaining : toSlaveEnd;
    if (span > (uint32_t)bufferSize) span = bufferSize;

    return (uint16_t)span;
}

uint32_t updateEepromPadToPage(uint32_t length)
{
    if (length == 0) return 0;

    return ((length + UPDATE_EEPROM_PAGE_SIZE - 1u) /
            UPDATE_EEPROM_PAGE_SIZE) * UPDATE_EEPROM_PAGE_SIZE;
}
