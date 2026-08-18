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
    one slave or one flash page and one erased sector, the checksum the
    FPGA's boot block carries, and the packet encoding the host's progress
    display is made of. An off-by-one in any of it leaves a device that will
    not boot, and a bench is a poor place to find that out.

    The transports in update-agent.c and epcs-flash.c have no coverage here
    and cannot have any — one is an EEPROM and the other is four links of
    SPI ending in a flash, and the only test for either is a board.

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

    // The one word that is not reserved. Decoding it is not the same as
    // being allowed to use it — that is testFactoryTarget's business — and
    // the two are kept apart so a decoder change cannot quietly widen what
    // may be written.
    makeBeginPacket(packet, 4096u, 0u, UPDATE_FLAG_FACTORY_WRITE);
    check(updateBeginDecode(packet, UPDATE_BEGIN_LENGTH, &begin),
          "a BEGIN packet carrying the factory-write word decodes");
    checkNumber(begin.flags, UPDATE_FLAG_FACTORY_WRITE,
                "the factory-write word survives decoding");

    // A near miss is not a hit. One bit out is a host that has computed the
    // word rather than copied it, and computing it is not something any
    // host should be doing.
    makeBeginPacket(packet, 4096u, 0u, UPDATE_FLAG_FACTORY_WRITE ^ 1u);
    check(!updateBeginDecode(packet, UPDATE_BEGIN_LENGTH, &begin),
          "a flag word one bit off the factory-write word is refused");
}

// Target 2, which writes the image a board falls back to.
//
// The rules this pins are the ones that keep an ordinary update away from
// the factory region: the unlock word is required, it is required *only*
// there, and the region ends at the boot block rather than at the end of
// the device.
static void testFactoryTarget(void)
{
    updateState_t state;
    const uint32_t factoryCapacity =
        UPDATE_EPCS_BOOT_BLOCK_ADDRESS - UPDATE_EPCS_FACTORY_ADDRESS;

    updateStateReset(&state);

    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EPCS_FACTORY, 215200u,
                                     UPDATE_FLAG_FACTORY_WRITE, 0),
                UPDATE_ERROR_NONE,
                "a factory image with the unlock word is admitted");

    // The guard, and the whole reason the word exists: a host that meant
    // target 1 and sent a 2 carries a zero flags word.
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EPCS_FACTORY, 215200u,
                                     UPDATE_FLAGS_NONE, 0),
                UPDATE_ERROR_TARGET,
                "a factory write without the unlock word is refused");

    // And the other way round, because a host that thinks it is writing the
    // factory region and is not has misunderstood which image it is
    // replacing.
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EPCS, 215200u,
                                     UPDATE_FLAG_FACTORY_WRITE, 0),
                UPDATE_ERROR_TARGET,
                "an application write carrying the unlock word is refused");
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM, 65536u,
                                     UPDATE_FLAG_FACTORY_WRITE, 0),
                UPDATE_ERROR_TARGET,
                "a firmware write carrying the unlock word is refused");

    // A capture still wins, exactly as for the other two targets: the
    // unlock word says what may be written, not when.
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EPCS_FACTORY, 215200u,
                                     UPDATE_FLAG_FACTORY_WRITE, 1),
                UPDATE_ERROR_BUSY,
                "a factory write is refused while a capture runs");

    // The ceiling is the boot block. An image that reached it would erase
    // the record of where the application image is while claiming to be a
    // bring-up, which is the one way this target could break a working
    // board that the rest of the design does not already prevent.
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EPCS_FACTORY,
                                     factoryCapacity,
                                     UPDATE_FLAG_FACTORY_WRITE, 0),
                UPDATE_ERROR_NONE,
                "a factory image exactly filling its region is admitted");
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EPCS_FACTORY,
                                     factoryCapacity + 1u,
                                     UPDATE_FLAG_FACTORY_WRITE, 0),
                UPDATE_ERROR_LENGTH,
                "a factory image reaching the boot block is refused");

    // Which is a tighter bound than the application region's, and that is
    // the point of asking the target rather than assuming one region.
    check(!updateGatewareIsPlausible(UPDATE_TARGET_EPCS_FACTORY,
                                     UPDATE_EPCS_SIZE -
                                     UPDATE_EPCS_APPLICATION_ADDRESS),
          "an image sized for the application region is too big for the factory one");

    // Where each target's bytes land, which is the arithmetic every write,
    // erase and readback in the agent is built on.
    checkNumber(updateEpcsTargetBase(UPDATE_TARGET_EPCS_FACTORY),
                UPDATE_EPCS_FACTORY_ADDRESS,
                "the factory target writes from the bottom of the flash");
    checkNumber(updateEpcsTargetBase(UPDATE_TARGET_EPCS),
                UPDATE_EPCS_APPLICATION_ADDRESS,
                "the gateware target writes from the application address");

    // The default is the application address rather than zero, so that a
    // dispatch bug writes somewhere a later update repairs rather than over
    // the image a board falls back to.
    checkNumber(updateEpcsTargetBase(UPDATE_TARGET_EEPROM),
                UPDATE_EPCS_APPLICATION_ADDRESS,
                "a target that is not on the flash defaults away from the factory image");
    checkNumber(updateEpcsTargetBase(99u), UPDATE_EPCS_APPLICATION_ADDRESS,
                "an unknown target defaults away from the factory image too");

    check(updateTargetIsEpcs(UPDATE_TARGET_EPCS), "target 1 is on the flash");
    check(updateTargetIsEpcs(UPDATE_TARGET_EPCS_FACTORY),
          "target 2 is on the flash");
    check(!updateTargetIsEpcs(UPDATE_TARGET_EEPROM),
          "target 0 is not on the flash");
    check(!updateGatewareIsPlausible(UPDATE_TARGET_EEPROM, 215200u),
          "the EEPROM target is not a flash region at all");
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

    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM, 65536u, UPDATE_FLAGS_NONE, 0),
                UPDATE_ERROR_NONE, "an ordinary firmware update is admitted");

    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM, 65536u, UPDATE_FLAGS_NONE, 1),
                UPDATE_ERROR_BUSY, "an update is refused while a capture runs");

    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EPCS, 65536u, UPDATE_FLAGS_NONE, 0),
                UPDATE_ERROR_NONE, "an ordinary gateware update is admitted");
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EPCS, 65536u, UPDATE_FLAGS_NONE, 1),
                UPDATE_ERROR_BUSY, "a gateware update is refused while a capture runs");

    // Refused rather than accepted and ignored, so a host built against a
    // later firmware finds out before it streams a megabyte.
    checkNumber(updateBeginIsAllowed(&state, 7u, 65536u, UPDATE_FLAGS_NONE, 0),
                UPDATE_ERROR_TARGET, "an unknown target is refused");

    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM, 0u, UPDATE_FLAGS_NONE, 0),
                UPDATE_ERROR_LENGTH, "a zero-length payload is refused");
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM,
                                     UPDATE_IMAGE_MINIMUM_LENGTH - 1u, UPDATE_FLAGS_NONE, 0),
                UPDATE_ERROR_LENGTH, "a payload too short to be an image is refused");
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM,
                                     UPDATE_EEPROM_SIZE + 1u, UPDATE_FLAGS_NONE, 0),
                UPDATE_ERROR_LENGTH, "a payload larger than the EEPROM is refused");
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM,
                                     UPDATE_EEPROM_SIZE, UPDATE_FLAGS_NONE, 0),
                UPDATE_ERROR_NONE, "a payload that exactly fills the EEPROM is admitted");

    // The gateware's bound is the region above the application address, not
    // the whole device. An image that ran past the end would be written
    // until the address wrapped, which on this medium means over the factory
    // image — the one thing on the device a field update may never touch.
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EPCS,
                                     UPDATE_EPCS_SIZE -
                                     UPDATE_EPCS_APPLICATION_ADDRESS, UPDATE_FLAGS_NONE, 0),
                UPDATE_ERROR_NONE, "a gateware image filling the region is admitted");
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EPCS,
                                     (UPDATE_EPCS_SIZE -
                                      UPDATE_EPCS_APPLICATION_ADDRESS) + 1u, UPDATE_FLAGS_NONE, 0),
                UPDATE_ERROR_LENGTH, "a gateware image past the end of the device is refused");
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EPCS,
                                     UPDATE_GATEWARE_MINIMUM_LENGTH - 1u, UPDATE_FLAGS_NONE, 0),
                UPDATE_ERROR_LENGTH, "a gateware image too short to be one is refused");

    // A second BEGIN during a transfer is refused rather than restarting it.
    makeReceiving(&state, 65536u);
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM, 65536u, UPDATE_FLAGS_NONE, 0),
                UPDATE_ERROR_SEQUENCE, "a second BEGIN during a transfer is refused");

    // A failed update does not block the retry that follows it.
    updateStateReset(&state);
    updateStateFail(&state, UPDATE_ERROR_STREAM_DIGEST);
    checkNumber(updateBeginIsAllowed(&state, UPDATE_TARGET_EEPROM, 65536u, UPDATE_FLAGS_NONE, 0),
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

    // The alignment is the page size of the medium the chunk is going to,
    // and the EPCS's page is four times the EEPROM's. A chunk of two EEPROM
    // pages is a legal EEPROM chunk and an illegal gateware one.
    makeReceiving(&state, 4096u);
    state.target = UPDATE_TARGET_EPCS;
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EPCS, 0u, 128u),
                UPDATE_ERROR_CHUNK, "a mid-transfer gateware chunk of two EEPROM pages is refused");
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EPCS, 0u,
                                     UPDATE_EPCS_PAGE_SIZE),
                UPDATE_ERROR_NONE, "a mid-transfer gateware chunk of one flash page is admitted");
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EPCS, 0u, 2048u),
                UPDATE_ERROR_NONE, "the advertised chunk size suits both media");
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EEPROM, 0u, 2048u),
                UPDATE_ERROR_TARGET, "a firmware chunk during a gateware update is refused");

    // The last chunk carries whatever is left, on either medium.
    makeReceiving(&state, 300u);
    state.target = UPDATE_TARGET_EPCS;
    checkNumber(updateChunkIsAllowed(&state, UPDATE_TARGET_EPCS, 0u, 300u),
                UPDATE_ERROR_NONE, "the final gateware chunk need not be page-aligned");
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

static void testGatewarePlausibility(void)
{
    // A raw EPCS byte stream carries no signature, so length is all there
    // is to check — and it is checked rather than nothing being checked.
    check(updateGatewareIsPlausible(UPDATE_TARGET_EPCS, 368000u),
          "an image the size of a real gateware is plausible");
    check(!updateGatewareIsPlausible(UPDATE_TARGET_EPCS, 0u), "an empty gateware image is refused");
    check(!updateGatewareIsPlausible(UPDATE_TARGET_EPCS, UPDATE_GATEWARE_MINIMUM_LENGTH - 1u),
          "a gateware image shorter than a flash page is refused");
    check(updateGatewareIsPlausible(UPDATE_TARGET_EPCS, UPDATE_EPCS_SIZE -
                                    UPDATE_EPCS_APPLICATION_ADDRESS),
          "an image exactly filling the application region is plausible");
    check(!updateGatewareIsPlausible(UPDATE_TARGET_EPCS, (UPDATE_EPCS_SIZE -
                                      UPDATE_EPCS_APPLICATION_ADDRESS) + 1u),
          "an image one byte past the end of the device is refused");
}

static void testEpcsDeviceIdentification(void)
{
    checkNumber(updateEpcsCapacity(UPDATE_EPCS_ID_EPCS64), 0x800000u,
                "an EPCS64 holds eight megabytes");
    checkNumber(updateEpcsCapacity(UPDATE_EPCS_ID_EPCS16), 0x200000u,
                "an EPCS16 holds two megabytes");

    // The two readings that mean nothing is there. SPI has no
    // acknowledgement, so a bridge writing into nothing returns whatever the
    // line carries — all ones if it floats, all zeros if it is held down —
    // and neither is an identifier however much it looks like one.
    checkNumber(updateEpcsCapacity(0x00u), 0u, "an all-zero answer is not a device");
    checkNumber(updateEpcsCapacity(0xFFu), 0u, "an all-ones answer is not a device");

    check(updateEpcsDeviceIsUsable(UPDATE_EPCS_ID_EPCS64,
                                   UPDATE_EPCS_APPLICATION_ADDRESS, 368000u),
          "a real gateware image fits an EPCS64 at the application address");
    check(!updateEpcsDeviceIsUsable(0x00u, UPDATE_EPCS_APPLICATION_ADDRESS, 1024u),
          "a device that did not identify itself is not written to");

    // A board carrying the smaller flash of an earlier revision is a real
    // thing to meet, and the honest answer to one is "this does not fit".
    check(!updateEpcsDeviceIsUsable(UPDATE_EPCS_ID_EPCS16,
                                    UPDATE_EPCS_APPLICATION_ADDRESS, 368000u),
          "an image that does not fit the device is refused");
    check(updateEpcsDeviceIsUsable(UPDATE_EPCS_ID_EPCS64,
                                   UPDATE_EPCS_APPLICATION_ADDRESS,
                                   0x800000u - UPDATE_EPCS_APPLICATION_ADDRESS),
          "an image exactly filling the device is accepted");
    check(!updateEpcsDeviceIsUsable(UPDATE_EPCS_ID_EPCS64,
                                    UPDATE_EPCS_APPLICATION_ADDRESS,
                                    (0x800000u - UPDATE_EPCS_APPLICATION_ADDRESS) + 1u),
          "an image one byte too large for the device is refused");
}

static void testEpcsGeometry(void)
{
    // A page program that runs past the end of its page wraps to the start
    // of the same page, exactly as the EEPROM's does.
    checkNumber(updateEpcsWriteSpan(UPDATE_EPCS_APPLICATION_ADDRESS, 4096u),
                UPDATE_EPCS_PAGE_SIZE, "a program is capped at one page");
    checkNumber(updateEpcsWriteSpan(UPDATE_EPCS_APPLICATION_ADDRESS, 10u), 10u,
                "a program shorter than a page is not padded here");
    checkNumber(updateEpcsWriteSpan(UPDATE_EPCS_APPLICATION_ADDRESS + 64u, 4096u),
                UPDATE_EPCS_PAGE_SIZE - 64u,
                "a program from mid-page stops at the page boundary");
    checkNumber(updateEpcsWriteSpan(UPDATE_EPCS_APPLICATION_ADDRESS, 0u), 0u,
                "nothing remaining is nothing to program");

    // The layout the factory image and the boot block encoder agree on. An
    // application image that did not start on a sector boundary could not be
    // erased without erasing something else.
    check(updateEpcsSectorStartsHere(UPDATE_EPCS_APPLICATION_ADDRESS),
          "the application region starts on a sector boundary");
    check(updateEpcsSectorStartsHere(UPDATE_EPCS_BOOT_BLOCK_ADDRESS),
          "the boot block has a sector of its own");
    check(!updateEpcsSectorStartsHere(UPDATE_EPCS_APPLICATION_ADDRESS + 1u),
          "a byte into a sector is not the start of one");
    check(updateEpcsSectorStartsHere(UPDATE_EPCS_APPLICATION_ADDRESS +
                                     UPDATE_EPCS_SECTOR_SIZE),
          "the next sector starts one sector along");

    checkNumber(updateEpcsSectorBase(UPDATE_EPCS_APPLICATION_ADDRESS + 1u),
                UPDATE_EPCS_APPLICATION_ADDRESS, "an address maps to its own sector");
    checkNumber(updateEpcsSectorBase(UPDATE_EPCS_APPLICATION_ADDRESS +
                                     UPDATE_EPCS_SECTOR_SIZE - 1u),
                UPDATE_EPCS_APPLICATION_ADDRESS, "the last byte of a sector maps to its base");

    // The application region must not overlap the boot block, and the boot
    // block must not overlap the factory image. Checked here because all
    // three addresses are constants that somebody could plausibly adjust.
    check(UPDATE_EPCS_BOOT_BLOCK_ADDRESS < UPDATE_EPCS_APPLICATION_ADDRESS,
          "the boot block sits below the application image");
    check((UPDATE_EPCS_BOOT_BLOCK_ADDRESS + UPDATE_EPCS_SECTOR_SIZE) <=
          UPDATE_EPCS_APPLICATION_ADDRESS,
          "the boot block's sector does not reach the application image");
    check(UPDATE_BOOT_BLOCK_LENGTH <= UPDATE_EPCS_PAGE_SIZE,
          "the boot block is one page program");
}

// Walk a whole gateware image the way the agent does, checking that the
// programs cover it exactly once and that each sector is erased exactly once
// and always before anything in it is written.
//
// This is the property the individual span tests cannot state, and the one
// that matters: an erase issued a page too late destroys what was just
// written to that sector, and the symptom is a unit that quietly boots its
// factory image days later.
static void testGatewareImageCoverage(void)
{
    const uint32_t length = 368011u;
    uint32_t offset = 0u;
    uint32_t erases = 0u;
    uint32_t programs = 0u;
    int crossedPage = 0;
    int wroteBeforeErase = 0;
    uint32_t erasedThrough = UPDATE_EPCS_APPLICATION_ADDRESS;

    while (offset < length) {
        const uint32_t address = UPDATE_EPCS_APPLICATION_ADDRESS + offset;
        const uint32_t remaining = length - offset;
        const uint16_t span = updateEpcsWriteSpan(address, remaining);

        if (span == 0u) {
            printf("FAIL: the program span stalled at %lu\n", (unsigned long)offset);
            failures++;
            return;
        }

        if (updateEpcsSectorStartsHere(address)) {
            erases++;
            erasedThrough = address + UPDATE_EPCS_SECTOR_SIZE;
        }

        if ((address + span) > erasedThrough) {
            wroteBeforeErase = 1;
        }

        if ((address % UPDATE_EPCS_PAGE_SIZE) + span > UPDATE_EPCS_PAGE_SIZE) {
            crossedPage = 1;
        }

        offset += span;
        programs++;
    }

    check(!crossedPage, "no program in a whole image crosses a page boundary");
    check(!wroteBeforeErase, "nothing is programmed into a sector that has not been erased");
    checkNumber(offset, length, "the programs cover the image exactly");
    checkNumber(erases, (length + UPDATE_EPCS_SECTOR_SIZE - 1u) /
                UPDATE_EPCS_SECTOR_SIZE, "one erase per sector the image occupies");
    checkNumber(programs, (length + UPDATE_EPCS_PAGE_SIZE - 1u) /
                UPDATE_EPCS_PAGE_SIZE, "one program per page of the image");
}

static void testCrc32(void)
{
    static const uint8_t check_vector[9] = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };
    const uint8_t zero = 0x00u;
    uint32_t crc;
    uint32_t split;

    // The published check value for CRC-32, which is what pins this
    // implementation to the one zlib computes in make-boot-block.py and the
    // one fpga/factory/crc32.v computes in the factory image's fabric.
    // Three implementations of one checksum in three languages, and this is
    // the number that says they are the same checksum.
    crc = updateCrc32Final(updateCrc32Update(UPDATE_CRC32_INITIAL,
                                             check_vector, 9u));
    checkNumber(crc, 0xCBF43926u, "the CRC-32 check value is the published one");

    checkNumber(updateCrc32Final(UPDATE_CRC32_INITIAL), 0u,
                "the CRC of nothing at all is zero");

    // Folded a byte at a time or all at once, it is the same number — which
    // is what lets the agent accumulate one over a flash it reads back in
    // whatever spans the buffer allows.
    split = UPDATE_CRC32_INITIAL;
    split = updateCrc32Update(split, check_vector, 4u);
    split = updateCrc32Update(split, check_vector + 4u, 5u);
    checkNumber(updateCrc32Final(split), 0xCBF43926u,
                "a CRC folded in two pieces matches one folded whole");

    check(updateCrc32Final(updateCrc32Update(UPDATE_CRC32_INITIAL, &zero, 1u)) !=
          updateCrc32Final(UPDATE_CRC32_INITIAL),
          "a zero byte changes the CRC");
}

static void testBootBlockEncoding(void)
{
    // The twenty-four bytes fpga/make-boot-block.py produces for a
    // thousand-byte image of ascending values at the application address,
    // field for field and byte for byte. The encoder on the host and the
    // encoder on the device write the same block or a unit boots the wrong
    // half of its flash.
    static const uint8_t expected[UPDATE_BOOT_BLOCK_LENGTH] = {
        0x44u, 0x44u, 0x42u, 0x42u,             // "DDBB"
        0x01u, 0x00u,                           // layout version 1
        0x00u, 0x00u,                           // reserved
        0x00u, 0x00u, 0x20u, 0x00u,             // application at 0x200000
        0x00u, 0x04u, 0x00u, 0x00u,             // 1024 bytes
        0x26u, 0x4Cu, 0x0Bu, 0xB7u,             // the image's CRC-32
        0x28u, 0x12u, 0xD1u, 0xB9u,             // the block's own CRC-32
    };

    uint8_t image[1024];
    uint8_t block[UPDATE_BOOT_BLOCK_LENGTH];
    uint32_t imageCrc;
    uint32_t index;

    for (index = 0u; index < sizeof(image); index++) {
        image[index] = (uint8_t)(index & 0xFFu);
    }

    imageCrc = updateCrc32Final(updateCrc32Update(UPDATE_CRC32_INITIAL, image,
                                                  (uint32_t)sizeof(image)));
    checkNumber(imageCrc, 0xB70B4C26u, "the image CRC matches the host encoder's");

    updateBootBlockEncode(block, UPDATE_EPCS_APPLICATION_ADDRESS,
                          (uint32_t)sizeof(image), imageCrc);

    for (index = 0u; index < UPDATE_BOOT_BLOCK_LENGTH; index++) {
        if (block[index] != expected[index]) {
            printf("FAIL: boot block byte %u is 0x%02X, expected 0x%02X\n",
                   (unsigned)index, block[index], expected[index]);
            failures++;
        }
    }

    // The block's own checksum covers the twenty bytes before it, which is
    // what distinguishes a block half written by an interrupted update from
    // an intact block describing a damaged image.
    checkNumber(updateCrc32Final(updateCrc32Update(UPDATE_CRC32_INITIAL, block,
                                                   UPDATE_BOOT_BLOCK_HEADER_LENGTH)),
                0xB9D11228u, "the header CRC covers the first twenty bytes");

    // A different image gives a different block, which is the whole reason
    // the block is rewritten on every update rather than left alone.
    updateBootBlockEncode(block, UPDATE_EPCS_APPLICATION_ADDRESS,
                          (uint32_t)sizeof(image) - 1u, imageCrc);
    check(block[12] != expected[12] || block[13] != expected[13],
          "a different length reaches the encoded block");
}

int main(void)
{
    testBeginDecode();
    testStatusEncoding();
    testBeginAdmission();
    testFactoryTarget();
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
    testGatewarePlausibility();
    testEpcsDeviceIdentification();
    testEpcsGeometry();
    testGatewareImageCoverage();
    testCrc32();
    testBootBlockEncoding();

    if (failures != 0) {
        printf("update-protocol-test: FAIL (%d failures)\n", failures);
        return 1;
    }

    printf("update-protocol-test: PASS\n");
    return 0;
}
