/************************************************************************

    update-protocol-test.c

    T1 unit test for the device update protocol's decisions
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    Compiled and run on the build host, not the FX3, which is possible at
    all only because update-protocol.c deliberately does not include the
    Cypress SDK.

    What is covered here is everything that decides whether a byte gets
    written and where: the state machine that admits or refuses each
    request, the paging arithmetic that keeps a write inside one page and
    one slave, and the packet encoding the host's progress display is made
    of. An off-by-one in any of it leaves a device that will not boot, and
    a bench is a poor place to find that out.

    The I2C transport in update-agent.c has no coverage here and cannot
    have any — it is an EEPROM, and the only test for it is a board.

************************************************************************/

#include <stdio.h>
#include <string.h>

#include "update-protocol.h"

static int failures = 0;

static void check(int condition, const char *what)
{
    if (!condition) {
        printf("FAIL: %s\n", what);
        failures++;
    }
}

static void checkNumber(unsigned long got, unsigned long want, const char *what)
{
    if (got != want) {
        printf("FAIL: %s: got %lu, expected %lu\n", what, got, want);
        failures++;
    }
}

// A state with an update open and receiving, which is the precondition for
// most of what is tested below.
static void makeReceiving(updateState_t *state, uint32_t length)
{
    updateStateReset(state);
    state->phase = UPDATE_PHASE_RECEIVING;
    state->target = UPDATE_TARGET_EEPROM;
    state->length = length;
}

// Build an UPDATE_BEGIN data stage the way a host would.
static void makeBeginPacket(uint8_t *packet, uint32_t length, uint8_t digestFill,
                            uint32_t flags)
{
    size_t index;

    memset(packet, 0, UPDATE_BEGIN_LENGTH);

    packet[0] = (uint8_t)(length & 0xFFu);
    packet[1] = (uint8_t)((length >> 8) & 0xFFu);
    packet[2] = (uint8_t)((length >> 16) & 0xFFu);
    packet[3] = (uint8_t)((length >> 24) & 0xFFu);

    for (index = 0u; index < UPDATE_DIGEST_LENGTH; index++) {
        packet[4 + index] = (uint8_t)(digestFill + index);
    }

    packet[36] = (uint8_t)(flags & 0xFFu);
    packet[37] = (uint8_t)((flags >> 8) & 0xFFu);
    packet[38] = (uint8_t)((flags >> 16) & 0xFFu);
    packet[39] = (uint8_t)((flags >> 24) & 0xFFu);
}

static void testBeginDecode(void)
{
    uint8_t packet[UPDATE_BEGIN_LENGTH];
    updateBegin_t begin;
    size_t index;

    makeBeginPacket(packet, 0x00030201u, 0x10u, 0u);

    check(updateBeginDecode(packet, UPDATE_BEGIN_LENGTH, &begin),
          "a well-formed BEGIN packet decodes");
    checkNumber(begin.length, 0x00030201u, "the length is little-endian");

    for (index = 0u; index < UPDATE_DIGEST_LENGTH; index++) {
        if (begin.digest[index] != (uint8_t)(0x10u + index)) {
            printf("FAIL: digest byte %u is wrong\n", (unsigned)index);
            failures++;
            break;
        }
    }

    check(!updateBeginDecode(packet, UPDATE_BEGIN_LENGTH - 1u, &begin),
          "a short BEGIN packet is refused");
    check(!updateBeginDecode(packet, UPDATE_BEGIN_LENGTH + 1u, &begin),
          "a long BEGIN packet is refused");

    // Reserved bits are required to be zero rather than ignored: a host that
    // sets one is asking for behaviour this firmware does not have.
    makeBeginPacket(packet, 4096u, 0u, 0x00000001u);
    check(!updateBeginDecode(packet, UPDATE_BEGIN_LENGTH, &begin),
          "a BEGIN packet with a reserved flag set is refused");
}

static void testStatusEncoding(void)
{
    updateState_t state;
    uint8_t packet[UPDATE_STATUS_LENGTH];

    updateStateReset(&state);
    state.phase = UPDATE_PHASE_VERIFYING;
    state.error = UPDATE_ERROR_NONE;
    state.received = 0x11223344u;
    state.written = 0x00000100u;
    state.verified = 0x000000FFu;

    updateStatusEncode(&state, packet);

    checkNumber(packet[0], UPDATE_PHASE_VERIFYING, "the phase is at offset 0");
    checkNumber(packet[1], UPDATE_ERROR_NONE, "the error is at offset 1");
    checkNumber((unsigned long)packet[2] | ((unsigned long)packet[3] << 8),
                UPDATE_MAX_CHUNK, "the chunk size is at offset 2");

    checkNumber(packet[4], 0x44u, "bytes received are little-endian at offset 4");
    checkNumber(packet[7], 0x11u, "bytes received are little-endian at offset 4");
    checkNumber(packet[9], 0x01u, "bytes written are at offset 8");
    checkNumber(packet[12], 0xFFu, "bytes verified are at offset 12");

    // Answerable before anything has ever been started, which is how a host
    // discovers the chunk size rather than assuming one.
    updateStateReset(&state);
    updateStatusEncode(&state, packet);
    checkNumber(packet[0], UPDATE_PHASE_IDLE, "an untouched agent reports idle");
    checkNumber((unsigned long)packet[2] | ((unsigned long)packet[3] << 8),
                UPDATE_MAX_CHUNK, "an untouched agent still reports its chunk size");
}

static void testBeginAdmission(void)
{
    updateState_t state;

    updateStateReset(&state);

    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM, 65536u, 0),
                UPDATE_ERROR_NONE, "an ordinary firmware update is admitted");

    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM, 65536u, 1),
                UPDATE_ERROR_BUSY, "an update is refused while a capture runs");

    // The gateware target arrives with the flash bridge. Refused rather than
    // accepted and ignored, so a host built against a later firmware finds
    // out before it streams a megabyte.
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EPCS, 65536u, 0),
                UPDATE_ERROR_TARGET, "the EPCS target is refused by this firmware");
    checkNumber(updateBeginIsAllowed(&state, 7u, 65536u, 0),
                UPDATE_ERROR_TARGET, "an unknown target is refused");

    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM, 0u, 0),
                UPDATE_ERROR_LENGTH, "a zero-length payload is refused");
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM,
                                     UPDATE_IMAGE_MINIMUM_LENGTH - 1u, 0),
                UPDATE_ERROR_LENGTH, "a payload too short to be an image is refused");
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM,
                                     UPDATE_EEPROM_SIZE + 1u, 0),
                UPDATE_ERROR_LENGTH, "a payload larger than the EEPROM is refused");
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM,
                                     UPDATE_EEPROM_SIZE, 0),
                UPDATE_ERROR_NONE, "a payload that exactly fills the EEPROM is admitted");

    // A second BEGIN during a transfer is refused rather than restarting it.
    makeReceiving(&state, 65536u);
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM, 65536u, 0),
                UPDATE_ERROR_SEQUENCE, "a second BEGIN during a transfer is refused");

    // A failed update does not block the retry that follows it.
    updateStateReset(&state);
    updateStateFail(&state, UPDATE_ERROR_STREAM_DIGEST);
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM, 65536u, 0),
                UPDATE_ERROR_NONE, "a retry after a failure is admitted");
}

static void testChunkAdmission(void)
{
    updateState_t state;

    makeReceiving(&state, 4096u);

    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EEPROM, 0u, 2048u),
                UPDATE_ERROR_NONE, "the first full chunk is admitted");

    // A chunk that arrives out of order fails the transfer rather than being
    // buffered: the host is a program and not a network, so a gap means
    // something has gone wrong that reordering would hide.
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EEPROM, 1u, 2048u),
                UPDATE_ERROR_SEQUENCE, "a chunk that skips ahead is refused");

    state.nextChunk = 1u;
    state.received = 2048u;
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EEPROM, 0u, 2048u),
                UPDATE_ERROR_SEQUENCE, "a repeated chunk is refused");
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EEPROM, 1u, 2048u),
                UPDATE_ERROR_NONE, "the chunk that follows is admitted");
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EPCS, 1u, 2048u),
                UPDATE_ERROR_TARGET, "a chunk for another target is refused");

    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EEPROM, 1u, 0u),
                UPDATE_ERROR_CHUNK, "an empty chunk is refused");

    // More data than UPDATE_BEGIN promised, in a chunk that is otherwise
    // perfectly well formed. This is the case that would run off the end of
    // the image the host said it was sending.
    makeReceiving(&state, 2560u);
    state.nextChunk = 1u;
    state.received = 2048u;
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EEPROM, 1u, 1024u),
                UPDATE_ERROR_OVERRUN, "a chunk past the promised length is refused");
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EEPROM, 1u, 512u),
                UPDATE_ERROR_NONE, "the chunk that exactly finishes the image is admitted");

    // Every chunk but the last is a whole number of EEPROM pages, so a chunk
    // can go straight to the medium with no assembly buffer in between.
    makeReceiving(&state, 4096u);
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EEPROM, 0u, 100u),
                UPDATE_ERROR_CHUNK, "a mid-transfer chunk that is not whole pages is refused");
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EEPROM, 0u, 128u),
                UPDATE_ERROR_NONE, "a mid-transfer chunk of two pages is admitted");

    // The last chunk carries whatever is left, page-aligned or not.
    makeReceiving(&state, 100u);
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EEPROM, 0u, 100u),
                UPDATE_ERROR_NONE, "the final short chunk is admitted");

    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EEPROM, 0u,
                                     UPDATE_MAX_CHUNK + 1u),
                UPDATE_ERROR_CHUNK, "a chunk larger than the advertised maximum is refused");

    // Nothing is accepted outside the receiving phase.
    updateStateReset(&state);
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EEPROM, 0u, 64u),
                UPDATE_ERROR_SEQUENCE, "a chunk with no transfer open is refused");

    makeReceiving(&state, 4096u);
    state.phase = UPDATE_PHASE_VERIFYING;
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EEPROM, 0u, 64u),
                UPDATE_ERROR_SEQUENCE, "a chunk during verification is refused");
}

static void testFinishAdmission(void)
{
    updateState_t state;

    makeReceiving(&state, 4096u);
    state.received = 4095u;
    checkNumber(updateFinishIsAllowed(&state, UPDATE_TARGET_EEPROM),
                UPDATE_ERROR_SHORT, "FINISH before the last byte is refused");

    state.received = 4096u;
    checkNumber(updateFinishIsAllowed(&state, UPDATE_TARGET_EEPROM),
                UPDATE_ERROR_NONE, "FINISH after the last byte is admitted");
    checkNumber(updateFinishIsAllowed(&state, UPDATE_TARGET_EPCS),
                UPDATE_ERROR_TARGET, "FINISH for another target is refused");

    updateStateReset(&state);
    checkNumber(updateFinishIsAllowed(&state, UPDATE_TARGET_EEPROM),
                UPDATE_ERROR_SEQUENCE, "FINISH with no transfer open is refused");
}

static void testMutualExclusion(void)
{
    updateState_t state;

    updateStateReset(&state);
    check(!updateIsInProgress(&state), "an idle agent is not in progress");

    state.phase = UPDATE_PHASE_RECEIVING;
    check(updateIsInProgress(&state), "receiving counts as in progress");
    state.phase = UPDATE_PHASE_WRITING;
    check(updateIsInProgress(&state), "writing counts as in progress");
    state.phase = UPDATE_PHASE_VERIFYING;
    check(updateIsInProgress(&state), "verifying counts as in progress");

    // Both ends of an update release the capture path again.
    state.phase = UPDATE_PHASE_COMPLETE;
    check(!updateIsInProgress(&state), "a completed update is not in progress");
    state.phase = UPDATE_PHASE_FAILED;
    check(!updateIsInProgress(&state), "a failed update is not in progress");
}

static void testFailureIsSticky(void)
{
    updateState_t state;

    makeReceiving(&state, 4096u);
    state.received = 2048u;

    updateStateFail(&state, UPDATE_ERROR_WRITE);
    checkNumber(state.phase, UPDATE_PHASE_FAILED, "a failure reaches the failed phase");
    checkNumber(state.error, UPDATE_ERROR_WRITE, "the failure is recorded");
    checkNumber(state.received, 2048u, "how far it got is kept");

    // The first error wins. A write that fails and then cannot be read back
    // is one fault, and reporting the second would name the symptom.
    updateStateFail(&state, UPDATE_ERROR_READ);
    checkNumber(state.error, UPDATE_ERROR_WRITE, "the first error is the one reported");
}

static void testImagePlausibility(void)
{
    uint8_t image[4] = { UPDATE_IMAGE_SIGNATURE_0, UPDATE_IMAGE_SIGNATURE_1, 0x1Cu, 0xB0u };

    check(updateImageIsPlausible(image, sizeof(image), 4096u),
          "an image carrying the CY signature is plausible");

    image[0] = 'X';
    check(!updateImageIsPlausible(image, sizeof(image), 4096u),
          "an image without the CY signature is refused");

    image[0] = UPDATE_IMAGE_SIGNATURE_0;
    check(!updateImageIsPlausible(image, 1u, 4096u),
          "a first chunk too short to hold the signature is refused");
    check(!updateImageIsPlausible(image, sizeof(image), 8u),
          "an image shorter than any bootable one is refused");
    check(!updateImageIsPlausible(image, sizeof(image), UPDATE_EEPROM_SIZE + 1u),
          "an image larger than the EEPROM is refused");
}

static void testEepromAddressing(void)
{
    // Four 64 KiB slaves, and the bank lives in bits 2:1 of the address byte.
    checkNumber(updateEepromSlaveAddress(0u), 0xA0u, "the first bank is 0xA0");
    checkNumber(updateEepromSlaveAddress(65535u), 0xA0u, "the last byte of bank 0 is 0xA0");
    checkNumber(updateEepromSlaveAddress(65536u), 0xA2u, "bank 1 is 0xA2");
    checkNumber(updateEepromSlaveAddress(131072u), 0xA4u, "bank 2 is 0xA4");
    checkNumber(updateEepromSlaveAddress(196608u), 0xA6u, "bank 3 is 0xA6");

    checkNumber(updateEepromSlaveOffset(0u), 0u, "the offset within bank 0 starts at zero");
    checkNumber(updateEepromSlaveOffset(65536u), 0u, "the offset resets at each bank");
    checkNumber(updateEepromSlaveOffset(65537u), 1u, "the offset counts within the bank");
    checkNumber(updateEepromSlaveOffset(131071u), 65535u, "the offset reaches the bank's end");
}

static void testEepromWriteSpans(void)
{
    // A page write that runs off the end of a page wraps to the start of the
    // same page, so the page boundary is a hard limit and not a hint.
    checkNumber(updateEepromWriteSpan(0u, 4096u), UPDATE_EEPROM_PAGE_SIZE,
                "a write is capped at one page");
    checkNumber(updateEepromWriteSpan(0u, 10u), 10u,
                "a write shorter than a page is not padded here");
    checkNumber(updateEepromWriteSpan(32u, 4096u), 32u,
                "a write from mid-page stops at the page boundary");
    checkNumber(updateEepromWriteSpan(0u, 0u), 0u, "nothing remaining is nothing to write");

    // The slave boundary is a multiple of the page size, so stopping at the
    // page end stops at the slave end too. Checked rather than assumed,
    // because a write that crossed a slave boundary would land in the wrong
    // 64 KiB of a device that is about to be booted from.
    checkNumber(updateEepromWriteSpan(UPDATE_EEPROM_SLAVE_SIZE -
                                      UPDATE_EEPROM_PAGE_SIZE, 4096u),
                UPDATE_EEPROM_PAGE_SIZE, "the last page of a bank is a whole page");
    check((UPDATE_EEPROM_SLAVE_SIZE % UPDATE_EEPROM_PAGE_SIZE) == 0u,
          "the slave size is a whole number of pages");
}

static void testEepromReadSpans(void)
{
    // Reads have no page structure — the EEPROM's address counter rolls
    // through the whole slave — so only the slave boundary and the caller's
    // buffer matter.
    checkNumber(updateEepromReadSpan(0u, 4096u, 512u), 512u,
                "a read is capped at the buffer");
    checkNumber(updateEepromReadSpan(0u, 100u, 512u), 100u,
                "a read is capped at what remains");
    checkNumber(updateEepromReadSpan(65536u - 100u, 4096u, 512u), 100u,
                "a read stops at the bank boundary");
    checkNumber(updateEepromReadSpan(0u, 0u, 512u), 0u, "nothing remaining reads nothing");
    checkNumber(updateEepromReadSpan(0u, 4096u, 0u), 0u, "no buffer reads nothing");
}

static void testPagePadding(void)
{
    checkNumber(updateEepromPadToPage(0u), 0u, "nothing pads to nothing");
    checkNumber(updateEepromPadToPage(1u), UPDATE_EEPROM_PAGE_SIZE,
                "one byte costs a whole page");
    checkNumber(updateEepromPadToPage(UPDATE_EEPROM_PAGE_SIZE),
                UPDATE_EEPROM_PAGE_SIZE, "an aligned length is unchanged");
    checkNumber(updateEepromPadToPage(UPDATE_EEPROM_PAGE_SIZE + 1u),
                2u * UPDATE_EEPROM_PAGE_SIZE, "one byte over rounds up");
    checkNumber(updateEepromPadToPage(129412u), 129472u,
                "a real firmware image rounds up to a page");
}

// Walk a whole transfer the way the agent does, checking that the paging
// arithmetic covers the image exactly once with no gap and no overlap.
//
// This is the property the individual span tests cannot state: each of them
// checks one call, and what actually bricks a device is a sequence of calls
// that between them skip a page.
static void testFullImageCoverage(void)
{
    const uint32_t length = 129412u;
    uint32_t address = UPDATE_EEPROM_PAGE_SIZE;
    uint32_t writes = 0u;
    int crossedPage = 0;
    int crossedSlave = 0;

    while (address < length) {
        const uint32_t remaining = length - address;
        const uint16_t span = updateEepromWriteSpan(address, remaining);

        if (span == 0u) {
            printf("FAIL: the write span stalled at %lu\n", (unsigned long)address);
            failures++;
            return;
        }

        if ((address % UPDATE_EEPROM_PAGE_SIZE) + span > UPDATE_EEPROM_PAGE_SIZE) {
            crossedPage = 1;
        }
        if (updateEepromSlaveAddress(address) !=
            updateEepromSlaveAddress(address + span - 1u)) {
            crossedSlave = 1;
        }

        address += span;
        writes++;
    }

    check(!crossedPage, "no write in a whole image crosses a page boundary");
    check(!crossedSlave, "no write in a whole image crosses a slave boundary");
    checkNumber(address, length, "the writes cover the image exactly");
    checkNumber(writes, (length - UPDATE_EEPROM_PAGE_SIZE +
                         UPDATE_EEPROM_PAGE_SIZE - 1u) / UPDATE_EEPROM_PAGE_SIZE,
                "the image takes one write per page after the held-back first");
}

int main(void)
{
    testBeginDecode();
    testStatusEncoding();
    testBeginAdmission();
    testChunkAdmission();
    testFinishAdmission();
    testMutualExclusion();
    testFailureIsSticky();
    testImagePlausibility();
    testEepromAddressing();
    testEepromWriteSpans();
    testEepromReadSpans();
    testPagePadding();
    testFullImageCoverage();

    if (failures != 0) {
        printf("update-protocol-test: FAIL (%d failures)\n", failures);
        return 1;
    }

    printf("update-protocol-test: PASS\n");
    return 0;
}
