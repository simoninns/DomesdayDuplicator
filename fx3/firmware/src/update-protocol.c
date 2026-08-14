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

    if (target != UPDATE_TARGET_EEPROM && target != UPDATE_TARGET_EPCS) {
        return UPDATE_ERROR_TARGET;
    }

    if (captureRunning) return UPDATE_ERROR_BUSY;

    // An update already under way is not restarted by a second BEGIN. Two
    // hosts updating one device is not a case to arbitrate between.
    if (updateIsInProgress(state)) return UPDATE_ERROR_SEQUENCE;

    if (target == UPDATE_TARGET_EPCS) {
        if (!updateGatewareIsPlausible(length)) return UPDATE_ERROR_LENGTH;
        return UPDATE_ERROR_NONE;
    }

    if (length < UPDATE_IMAGE_MINIMUM_LENGTH) return UPDATE_ERROR_LENGTH;
    if (length > UPDATE_EEPROM_SIZE) return UPDATE_ERROR_LENGTH;

    return UPDATE_ERROR_NONE;
}

// The alignment every chunk but the last has to respect, which is the page
// size of the medium the chunk is going to.
//
// It is a constraint on the host rather than on the firmware because the
// alternative is an assembly buffer the size of a chunk between the wire
// and the medium, and the host is the cheapest place for the alignment to
// hold. A host that takes the advertised chunk size and rounds it down to a
// multiple of the larger of the two satisfies both targets for any
// advertised size, which is what the capture application does.
static uint32_t updateTargetPageSize(uint8_t target)
{
    return (target == UPDATE_TARGET_EPCS) ? UPDATE_EPCS_PAGE_SIZE
                                          : UPDATE_EEPROM_PAGE_SIZE;
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

    // Every chunk but the last is a whole number of the medium's pages, so
    // that a chunk can be written straight to it with no assembly buffer in
    // between. The last chunk carries whatever is left: on the EEPROM its
    // final page is zero-padded on the way out, and on the EPCS nothing
    // beyond the payload is written at all, because erased flash is already
    // what an unwritten byte should be.
    if ((uint32_t)length < remaining &&
        ((uint32_t)length % updateTargetPageSize(target)) != 0) {
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

int updateGatewareIsPlausible(uint32_t totalLength)
{
    if (totalLength < UPDATE_GATEWARE_MINIMUM_LENGTH) return 0;

    // The application region is everything above its start address. An
    // image that ran past the end of the device would be written until the
    // address wrapped, which on this medium means over the factory image.
    if (totalLength > (UPDATE_EPCS_SIZE - UPDATE_EPCS_APPLICATION_ADDRESS)) {
        return 0;
    }

    return 1;
}

uint32_t updateEpcsCapacity(uint8_t siliconId)
{
    switch (siliconId) {
        case UPDATE_EPCS_ID_EPCS16:  return 0x200000u;
        case UPDATE_EPCS_ID_EPCS64:  return 0x800000u;
        case UPDATE_EPCS_ID_EPCS128: return 0x1000000u;
        default:                     return 0u;
    }
}

int updateEpcsDeviceIsUsable(uint8_t siliconId, uint32_t address,
                             uint32_t bytes)
{
    const uint32_t capacity = updateEpcsCapacity(siliconId);

    if (capacity == 0u) return 0;

    // Bounded rather than assumed. A DE0-Nano carrying the smaller flash of
    // an earlier board revision is a real thing to meet, and the honest
    // answer to one is "this image does not fit" rather than a write that
    // runs off the end of the device.
    if (address > capacity) return 0;
    if (bytes > (capacity - address)) return 0;

    return 1;
}

uint16_t updateEpcsWriteSpan(uint32_t address, uint32_t remaining)
{
    uint32_t toPageEnd;
    uint32_t span;

    if (remaining == 0) return 0;

    toPageEnd = UPDATE_EPCS_PAGE_SIZE - (address % UPDATE_EPCS_PAGE_SIZE);

    span = (remaining < toPageEnd) ? remaining : toPageEnd;

    return (uint16_t)span;
}

int updateEpcsSectorStartsHere(uint32_t address)
{
    return ((address % UPDATE_EPCS_SECTOR_SIZE) == 0u) ? 1 : 0;
}

uint32_t updateEpcsSectorBase(uint32_t address)
{
    return address - (address % UPDATE_EPCS_SECTOR_SIZE);
}

uint32_t updateCrc32Update(uint32_t crc, const uint8_t *data, uint32_t length)
{
    uint32_t index;
    uint32_t bit;

    if (data == NULL) return crc;

    // Bit at a time, with no lookup table. The table would be a kilobyte of
    // the FX3's memory to save time in a loop that is already three orders
    // of magnitude faster than the flash reads feeding it - the bytes this
    // folds arrive over a bit-banged SPI link at tens of microseconds each.
    for (index = 0u; index < length; index++) {
        crc ^= (uint32_t)data[index];

        for (bit = 0u; bit < 8u; bit++) {
            if ((crc & 1u) != 0u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

uint32_t updateCrc32Final(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFu;
}

void updateBootBlockEncode(uint8_t *out, uint32_t address, uint32_t length,
                           uint32_t imageCrc)
{
    uint32_t headerCrc;

    if (out == NULL) return;

    // The magic is the one field stored most significant byte first, so that
    // it reads as "DDBB" in a dump. Everything after it is little-endian,
    // which is the byte order of both readers.
    out[0] = UPDATE_BOOT_BLOCK_MAGIC_0;
    out[1] = UPDATE_BOOT_BLOCK_MAGIC_1;
    out[2] = UPDATE_BOOT_BLOCK_MAGIC_2;
    out[3] = UPDATE_BOOT_BLOCK_MAGIC_3;

    updateWriteLittleEndian16(out + 4, (uint16_t)UPDATE_BOOT_BLOCK_LAYOUT_VERSION);
    updateWriteLittleEndian16(out + 6, 0u);

    updateWriteLittleEndian32(out + 8, address);
    updateWriteLittleEndian32(out + 12, length);
    updateWriteLittleEndian32(out + 16, imageCrc);

    // The block carries its own checksum as well as the image's, so that a
    // block half written by an interrupted update is distinguishable from an
    // intact block describing a damaged image. Both mean "stay in the
    // factory image", and they mean different things to whoever is
    // diagnosing it.
    headerCrc = updateCrc32Final(
        updateCrc32Update(UPDATE_CRC32_INITIAL, out,
                          UPDATE_BOOT_BLOCK_HEADER_LENGTH));

    updateWriteLittleEndian32(out + UPDATE_BOOT_BLOCK_HEADER_LENGTH, headerCrc);
}
