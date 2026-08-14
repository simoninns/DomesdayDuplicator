/************************************************************************

    update-protocol.h

    The device update protocol, and the decisions about it that need no
    hardware
    DomesdayDuplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    Deliberately free of the FX3 SDK, for the same reason
    fpga-register-map.h is: everything here is arithmetic over bytes the
    host sent, so it compiles and runs on a build machine and is tested
    there. The transport — I2C, EEPROM timing, SHA-256 over what came back
    off the medium — is in update-agent.h and cannot be tested off
    hardware.

    An off-by-one in the paging arithmetic below writes past a page or a
    slave boundary and leaves a device that will not boot, which is not
    something to discover on a bench.

    The protocol itself is specified on the "Device update mechanism" page
    of the documentation site. This header is the firmware's copy of it,
    not its definition.

************************************************************************/

#ifndef _UPDATE_PROTOCOL_H_
#define _UPDATE_PROTOCOL_H_

#include <stddef.h>
#include <stdint.h>

// Vendor requests. 0xA0 belongs to the Cypress boot ROM, 0xB0/0xBA/0xBB to
// the Cypress flash-programmer personality and 0xB5-0xB8 to this firmware's
// capture and register interface, so the update agent starts at 0xD0.
#define UPDATE_REQUEST_STATUS           (0xD0u)
#define UPDATE_REQUEST_BEGIN            (0xD1u)
#define UPDATE_REQUEST_DATA             (0xD2u)
#define UPDATE_REQUEST_FINISH           (0xD3u)
#define UPDATE_REQUEST_RESET            (0xD4u)
#define UPDATE_REQUEST_FPGA_RECONFIG    (0xD5u)

// wIndex selects the target throughout. Target 1 is the FPGA's EPCS
// configuration flash, reached through the gateware's flash bridge; the
// firmware in this phase answers for target 0 only and refuses target 1
// with UPDATE_ERROR_TARGET rather than pretending.
#define UPDATE_TARGET_EEPROM            (0u)
#define UPDATE_TARGET_EPCS              (1u)

// Sizes fixed by the protocol.
#define UPDATE_STATUS_LENGTH            (16u)
#define UPDATE_BEGIN_LENGTH             (40u)
#define UPDATE_DIGEST_LENGTH            (32u)

// The largest UPDATE_DATA chunk the device accepts, and what it reports in
// the status packet. Matches the flash programmer's transfer size, which is
// what the host tooling has always used.
#define UPDATE_MAX_CHUNK                (2048u)

// I2C EEPROM geometry. A write must be a whole number of pages and must not
// cross a slave boundary; past 64 KiB the slave address increments.
//
// These are a deliberate second copy of the constants in
// fx3/programmer/src/fx3-paging.h (AGENTS.md §2). Two programs on two
// processors write the same EEPROM, and sharing a header between them would
// be a cross-component include.
#define UPDATE_EEPROM_PAGE_SIZE         (64u)
#define UPDATE_EEPROM_SLAVE_SIZE        (65536u)

// The I2C address of the first EEPROM slave. Bits 2:1 carry the 64 KiB bank,
// so the four banks of an M24M02 are 0xA0, 0xA2, 0xA4 and 0xA6.
#define UPDATE_EEPROM_SLAVE_BASE        (0xA0u)

// The largest image the boot EEPROM can hold: 2 Mbit across four slaves.
#define UPDATE_EEPROM_SIZE              (4u * UPDATE_EEPROM_SLAVE_SIZE)

// The first two bytes of an FX3 boot image. The boot ROM looks for these, so
// an image that does not carry them is not a thing this device could ever
// boot from — and is refused before a single byte is written.
#define UPDATE_IMAGE_SIGNATURE_0        (0x43u)     // 'C'
#define UPDATE_IMAGE_SIGNATURE_1        (0x59u)     // 'Y'

// The smallest plausible boot image: the signature page plus something to
// run. Anything shorter is a truncated download, not an image.
#define UPDATE_IMAGE_MINIMUM_LENGTH     (128u)

// Update phases, as reported at offset 0 of the status packet.
#define UPDATE_PHASE_IDLE               (0u)
#define UPDATE_PHASE_RECEIVING          (1u)
#define UPDATE_PHASE_WRITING            (2u)
#define UPDATE_PHASE_VERIFYING          (3u)
#define UPDATE_PHASE_COMPLETE           (4u)
#define UPDATE_PHASE_FAILED             (5u)

// Why an update stopped, as reported at offset 1 of the status packet.
//
// The host shows these to a user, so each one names a distinct thing that
// went wrong rather than a place in the code. Two of them are the integrity
// chain's own links and are kept apart on purpose: "the bytes that arrived
// are not the bytes you promised" and "the bytes on the medium are not the
// bytes I wrote" are different faults with different remedies.
#define UPDATE_ERROR_NONE               (0u)
#define UPDATE_ERROR_BUSY               (1u)    // a capture is running
#define UPDATE_ERROR_TARGET             (2u)    // no such target on this firmware
#define UPDATE_ERROR_LENGTH             (3u)    // payload length impossible for the medium
#define UPDATE_ERROR_SEQUENCE           (4u)    // a request in the wrong phase, or a chunk out of order
#define UPDATE_ERROR_CHUNK              (5u)    // chunk size the medium cannot be written in
#define UPDATE_ERROR_OVERRUN            (6u)    // more data than UPDATE_BEGIN promised
#define UPDATE_ERROR_SHORT              (7u)    // UPDATE_FINISH before all the data arrived
#define UPDATE_ERROR_STREAM_DIGEST      (8u)    // integrity link 5: the stream is not what was promised
#define UPDATE_ERROR_MEDIUM_DIGEST      (9u)    // integrity link 6: the readback is not what was written
#define UPDATE_ERROR_WRITE              (10u)   // the medium refused a write
#define UPDATE_ERROR_READ               (11u)   // the medium refused a read
#define UPDATE_ERROR_IMAGE              (12u)   // not an image this device could boot
#define UPDATE_ERROR_HARDWARE           (13u)   // the I2C block did not come up

// Everything the agent knows about the update in progress.
//
// Held as a plain struct with no pointers so that the whole of the state
// machine below is a pure function of it. The agent adds the medium.
typedef struct
{
    uint8_t phase;
    uint8_t error;
    uint8_t target;

    // The payload length and digest UPDATE_BEGIN promised.
    uint32_t length;
    uint8_t digest[UPDATE_DIGEST_LENGTH];

    // The three counters the status packet reports. They move at very
    // different speeds — receiving is seconds, writing is minutes on the
    // EPCS — so a host that drove one progress bar from one of them would
    // be showing a still picture for most of the update.
    uint32_t received;
    uint32_t written;
    uint32_t verified;

    // The chunk index the next UPDATE_DATA must carry.
    uint16_t nextChunk;
} updateState_t;

// What UPDATE_BEGIN's data stage carries.
typedef struct
{
    uint32_t length;
    uint8_t digest[UPDATE_DIGEST_LENGTH];
    uint32_t flags;
} updateBegin_t;

// Put the state back to idle with no error. The counters are cleared, so a
// status read after this reports an update that has not started rather than
// the tail of the previous one.
void updateStateReset(updateState_t *state);

// Record a failure: the phase becomes failed and the error is remembered
// until the next UPDATE_BEGIN. The counters are left alone, because how far
// an update got before it stopped is the most useful thing about a failure.
//
// The first error wins. A write that fails and then cannot be read back is
// one fault, and reporting the second would name the symptom.
void updateStateFail(updateState_t *state, uint8_t error);

// Decode UPDATE_BEGIN's data stage. Returns non-zero if the buffer is the
// right length and the reserved flags are clear, zero otherwise.
//
// Reserved bits are required to be zero rather than ignored. A host that
// sets one is asking for behaviour this firmware does not have, and the
// only honest answer to that is a refusal.
int updateBeginDecode(const uint8_t *data, uint16_t length, updateBegin_t *out);

// Write the 16-byte status packet.
void updateStatusEncode(const updateState_t *state, uint8_t *out);

// May an UPDATE_BEGIN for this target and length be accepted right now?
//
// Returns UPDATE_ERROR_NONE if it may, and the error to report if it may
// not. captureRunning is the mutual exclusion the "Device update mechanism"
// page requires: an update is refused while a capture is running, and a
// capture is refused while an update is in progress, by state rather than
// by convention.
uint8_t updateBeginIsAllowed(const updateState_t *state, uint8_t target,
                             uint32_t length, int captureRunning);

// May this UPDATE_DATA chunk be accepted?
//
// Returns UPDATE_ERROR_NONE, or the error to report. A chunk that arrives
// out of order fails the transfer rather than being buffered: the host is a
// program and not a network, so a gap in the sequence means something has
// gone wrong that reordering would hide.
uint8_t updateChunkIsAllowed(const updateState_t *state, uint8_t target,
                             uint16_t index, uint16_t length);

// May UPDATE_FINISH be accepted?
uint8_t updateFinishIsAllowed(const updateState_t *state, uint8_t target);

// Is an update in progress — that is, would starting a capture now collide
// with one? True from UPDATE_BEGIN until the update completes or fails.
int updateIsInProgress(const updateState_t *state);

// Does this look like an image the FX3 boot ROM would accept?
//
// Checked against the first bytes of the payload, before anything is
// written, so that a bundle carrying the wrong file is refused rather than
// flashed. It is not a substitute for the digest — it proves nothing about
// the rest of the image — but it is the one check that can be made before
// the first page is committed.
int updateImageIsPlausible(const uint8_t *first, uint32_t firstLength,
                           uint32_t totalLength);

// The I2C slave address covering this byte address.
uint8_t updateEepromSlaveAddress(uint32_t address);

// The byte address within that slave.
uint16_t updateEepromSlaveOffset(uint32_t address);

// The largest write that may start at this address: at most a page, and
// never across a slave boundary. Returns 0 when nothing remains.
//
// A page write that crosses a page boundary wraps to the start of the same
// page in every EEPROM this device has ever shipped with, so the caller
// cannot be trusted to "just write less next time" — the limit has to be
// computed, and computed here where it is tested.
uint16_t updateEepromWriteSpan(uint32_t address, uint32_t remaining);

// The largest read that may start at this address, given a buffer of
// bufferSize bytes. Reads have no page limit, only the slave boundary.
uint16_t updateEepromReadSpan(uint32_t address, uint32_t remaining,
                              uint16_t bufferSize);

// Round a length up to a whole number of EEPROM pages. The final page of an
// image is zero-padded, because a page is the smallest thing that can be
// written.
uint32_t updateEepromPadToPage(uint32_t length);

#endif // _UPDATE_PROTOCOL_H_
