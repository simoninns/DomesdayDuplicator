/************************************************************************

    update-agent.c

    Rewriting the device's two flash memories, commanded from the host
    DomesdayDuplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3error.h"
#include "cyu3i2c.h"
#include "cyu3utils.h"

#include "epcs-flash.h"
#include "fpga-registers.h"
#include "update-agent.h"
#include "vendor/sha-256.h"

// The I2C bit rate. 400 kHz is the M24M02's fast mode and is what the
// Cypress flash programmer uses over the same bus, so the timing is not
// new: it is the timing every EEPROM on every one of these kits has already
// been written at.
#define UPDATE_I2C_BIT_RATE         (400000u)

// How many times to poll the EEPROM for the end of an internal page write
// before giving up. The part specifies 5 ms; each poll is a full I2C
// address cycle, so two hundred is a long way past patient.
#define UPDATE_I2C_ACK_RETRIES      (200u)

// How many times to retry a transfer the slave did not acknowledge at all.
// One retry, because a NAK on the address phase means the device is busy
// with the previous write, and anything beyond that is a wiring fault that
// retrying will not fix.
#define UPDATE_I2C_NAK_RETRIES      (1u)

// The readback buffer. Sized to a whole number of EEPROM pages so a read
// never straddles the boundary the write span was chosen around, and kept
// small because it is static and this processor has 512 KiB of RAM in total.
#define UPDATE_READBACK_SIZE        (512u)

// Whether the I2C block came up. Everything here refuses with
// UPDATE_ERROR_HARDWARE when it did not, rather than calling into a block
// that was never initialised.
static CyBool_t glUpdateI2cReady = CyFalse;

static updateState_t glUpdateState;

// Set by UPDATE_FINISH, cleared by the application thread when it picks the
// work up. Volatile because it is written by one thread and read by another
// with nothing between them that would force a reload.
static volatile CyBool_t glUpdateVerifyPending = CyFalse;

// The first EEPROM page, held back until everything else is written and
// verified.
//
// It carries the 'CY' signature the boot ROM looks for, so an EEPROM whose
// first page has not been written is an EEPROM the boot ROM rejects — and a
// kit whose EEPROM the boot ROM rejects falls back to the USB bootloader.
// That is the whole of the interrupted-update safety story, and it is one
// buffer and one deferred write.
static uint8_t glUpdateFirstPage[UPDATE_EEPROM_PAGE_SIZE]
    __attribute__ ((aligned (32)));

// Readback staging. Handed to the I2C block, so it needs the same 32-byte
// alignment the register buffer does.
static uint8_t glUpdateReadback[UPDATE_READBACK_SIZE]
    __attribute__ ((aligned (32)));

// One page, for the zero-padded tail of an image that does not end on a
// page boundary. The padding is outside the payload the digest covers, so
// it is written to the medium and never read back into a comparison.
static uint8_t glUpdatePagePad[UPDATE_EEPROM_PAGE_SIZE]
    __attribute__ ((aligned (32)));

// SHA-256 over the chunk stream as it arrives (integrity link 5) and over
// the readback from the medium (link 6). Two contexts because the second
// starts while the first is still needed for the comparison.
static struct Sha_256 glUpdateStreamHash;
static uint8_t glUpdateStreamDigest[UPDATE_DIGEST_LENGTH];

static struct Sha_256 glUpdateMediumHash;
static uint8_t glUpdateMediumDigest[UPDATE_DIGEST_LENGTH];

// The boot block, built once the application image has been read back and
// proved, and written as the last act of a gateware update.
static uint8_t glUpdateBootBlock[UPDATE_BOOT_BLOCK_LENGTH];

// Build the preamble for an EEPROM access: slave address for the bank, then
// the sixteen-bit byte address within it.
static void updateEepromPreamble(CyU3PI2cPreamble_t *preamble, uint32_t address,
                                 CyBool_t isRead)
{
    const uint8_t slave = updateEepromSlaveAddress(address);
    const uint16_t offset = updateEepromSlaveOffset(address);

    preamble->buffer[0] = slave;
    preamble->buffer[1] = (uint8_t)(offset >> 8);
    preamble->buffer[2] = (uint8_t)(offset & 0xFFu);

    if (isRead) {
        // A read is a write of the address followed by a repeated start and
        // the same slave in read mode. ctrlMask bit 2 is what puts that
        // second start condition after the third preamble byte.
        preamble->buffer[3] = (uint8_t)(slave | 0x01u);
        preamble->length = 4;
        preamble->ctrlMask = 0x0004;
    } else {
        preamble->length = 3;
        preamble->ctrlMask = 0x0000;
    }
}

// Wait for the EEPROM to finish its internal write cycle.
//
// The part acknowledges its address again as soon as the cycle is over, so
// polling for an acknowledgement is both the specified way to do this and
// faster than the fixed 5 ms delay it replaces: a page usually completes in
// rather less.
static CyU3PReturnStatus_t updateEepromWaitReady(uint32_t address)
{
    CyU3PI2cPreamble_t preamble;

    preamble.buffer[0] = updateEepromSlaveAddress(address);
    preamble.length = 1;
    preamble.ctrlMask = 0x0000;

    return CyU3PI2cWaitForAck(&preamble, UPDATE_I2C_ACK_RETRIES);
}

// Write one page, or part of one. The caller guarantees the span does not
// cross a page or a slave boundary — that is updateEepromWriteSpan()'s job,
// and it is tested on the host.
static CyU3PReturnStatus_t updateEepromWritePage(uint32_t address,
                                                 uint8_t *data,
                                                 uint16_t length)
{
    CyU3PI2cPreamble_t preamble;
    CyU3PReturnStatus_t status;

    updateEepromPreamble(&preamble, address, CyFalse);

    status = CyU3PI2cTransmitBytes(&preamble, data, length,
                                   UPDATE_I2C_NAK_RETRIES);
    if (status != CY_U3P_SUCCESS) return status;

    return updateEepromWaitReady(address);
}

static CyU3PReturnStatus_t updateEepromRead(uint32_t address, uint8_t *data,
                                            uint16_t length)
{
    CyU3PI2cPreamble_t preamble;

    updateEepromPreamble(&preamble, address, CyTrue);

    return CyU3PI2cReceiveBytes(&preamble, data, length,
                                UPDATE_I2C_NAK_RETRIES);
}

CyU3PReturnStatus_t updateAgentStart(void)
{
    CyU3PI2cConfig_t i2cConfig;
    CyU3PReturnStatus_t status;

    updateStateReset(&glUpdateState);
    glUpdateVerifyPending = CyFalse;

    status = CyU3PI2cInit();
    if (status != CY_U3P_SUCCESS) return status;

    CyU3PMemSet((uint8_t *)&i2cConfig, 0, sizeof(i2cConfig));
    i2cConfig.bitRate = UPDATE_I2C_BIT_RATE;

    // Register mode rather than DMA. The transfers here are at most a page
    // at a time and the calls are blocking, which is what makes the write
    // loop something that can be read straight down the page; a DMA-mode
    // I2C block would need a callback and a completion event for no gain at
    // these sizes.
    i2cConfig.isDma = CyFalse;

    // No timeouts. The SDK is explicit that they are to be left disabled in
    // the default mode of operation, and the EEPROM is a point-to-point
    // slave with nothing to arbitrate against.
    i2cConfig.busTimeout = 0xFFFFFFFFu;
    i2cConfig.dmaTimeout = 0xFFFFu;

    status = CyU3PI2cSetConfig(&i2cConfig, NULL);
    if (status != CY_U3P_SUCCESS) return status;

    glUpdateI2cReady = CyTrue;

    return CY_U3P_SUCCESS;
}

const updateState_t *updateAgentState(void)
{
    return &glUpdateState;
}

CyBool_t updateAgentInProgress(void)
{
    return updateIsInProgress(&glUpdateState) ? CyTrue : CyFalse;
}

void updateAgentStatus(uint8_t *out)
{
    updateStatusEncode(&glUpdateState, out);
}

CyBool_t updateAgentVerifyPending(void)
{
    return glUpdateVerifyPending;
}

// Is the medium this target lives on in a state to be written?
//
// Asked before the transfer opens rather than discovered part way through
// it, and it is a different question for each target: the EEPROM needs the
// I2C block to have come up, and the EPCS needs a gateware with a flash
// bridge and a flash that answers through it.
//
// Returns UPDATE_ERROR_NONE, or the error to report.
static uint8_t updateAgentMediumIsReady(uint8_t target, uint32_t length)
{
    uint8_t siliconId = 0x00u;

    if (target == UPDATE_TARGET_EEPROM) {
        return glUpdateI2cReady ? UPDATE_ERROR_NONE : UPDATE_ERROR_HARDWARE;
    }

    // Deliberately not refused when the factory image is the one answering.
    // A unit running its factory gateware is a unit in recovery, and this
    // is the path that repairs it — the factory image carries the bridge
    // for exactly that reason.
    if (!fpgaRegistersHasFlashBridge()) {
        CyU3PDebugPrint(4, "updateAgentBegin(): no gateware with a flash bridge is "
            "answering; the EPCS cannot be reached from here\r\n");
        return UPDATE_ERROR_HARDWARE;
    }

    // The one thing that can be established before a sector is erased: that
    // there is a serial flash on the far end of four links, and that it is
    // large enough for what is about to be written.
    if (!epcsFlashIdentify(&siliconId)) {
        CyU3PDebugPrint(4, "updateAgentBegin(): the EPCS did not identify itself\r\n");
        return UPDATE_ERROR_HARDWARE;
    }

    if (!updateEpcsDeviceIsUsable(siliconId, UPDATE_EPCS_APPLICATION_ADDRESS,
                                  length)) {
        CyU3PDebugPrint(4, "updateAgentBegin(): the flash answered with silicon id %d, "
            "which will not hold %d bytes at the application address\r\n",
            siliconId, length);
        return UPDATE_ERROR_LENGTH;
    }

    return UPDATE_ERROR_NONE;
}

CyBool_t updateAgentBegin(uint8_t target, const uint8_t *data, uint16_t length,
                          CyBool_t captureRunning)
{
    updateBegin_t begin;
    uint8_t refusal;
    uint32_t index;

    if (!updateBeginDecode(data, length, &begin)) {
        if (updateIsInProgress(&glUpdateState)) return CyFalse;
        updateStateReset(&glUpdateState);
        updateStateFail(&glUpdateState, UPDATE_ERROR_LENGTH);
        return CyFalse;
    }

    refusal = updateBeginIsAllowed(&glUpdateState, target, begin.length,
                                   captureRunning ? 1 : 0);

    // The medium is only asked about once the request itself is admissible.
    // Reaching for the flash to answer a request that was going to be
    // refused anyway would drive the bridge on behalf of a host that had
    // asked for something impossible.
    if (refusal == UPDATE_ERROR_NONE) {
        refusal = updateAgentMediumIsReady(target, begin.length);
    }
    if (refusal != UPDATE_ERROR_NONE) {
        // A second BEGIN arriving during an update is refused by stalling
        // and nothing more. Recording it as a failure would let a stray
        // request from a second host abort a transfer that is going
        // perfectly well.
        if (updateIsInProgress(&glUpdateState)) return CyFalse;

        // Otherwise the previous result is cleared first, so the error the
        // host reads is this request's and not the last one's.
        updateStateReset(&glUpdateState);
        updateStateFail(&glUpdateState, refusal);
        return CyFalse;
    }

    updateStateReset(&glUpdateState);

    glUpdateState.phase = UPDATE_PHASE_RECEIVING;
    glUpdateState.target = target;
    glUpdateState.length = begin.length;
    for (index = 0; index < UPDATE_DIGEST_LENGTH; index++) {
        glUpdateState.digest[index] = begin.digest[index];
    }

    for (index = 0; index < UPDATE_EEPROM_PAGE_SIZE; index++) {
        glUpdateFirstPage[index] = 0;
    }

    sha_256_init(&glUpdateStreamHash, glUpdateStreamDigest);

    CyU3PDebugPrint(4, "updateAgentBegin(): update opened for target %d, %d bytes\r\n",
                    target, glUpdateState.length);

    return CyTrue;
}

// Write one chunk of a gateware image to the EPCS, erasing as it goes.
//
// The address arithmetic is all in update-protocol.c and tested on the
// host; what is here is the order things happen in. Each sector is erased
// at the moment the write first enters it, which works because the image is
// written strictly in address order and costs one erase per sector rather
// than erasing a device that is mostly factory image.
//
// No sector is erased speculatively and none is erased at UPDATE_BEGIN. An
// update that is abandoned before its first chunk therefore leaves the
// previous gateware intact and running.
static CyBool_t updateAgentDataEpcs(uint8_t *data, uint16_t length)
{
    uint32_t consumed = 0u;

    while (consumed < (uint32_t)length) {
        const uint32_t imageOffset = glUpdateState.received + consumed;
        const uint32_t address = UPDATE_EPCS_APPLICATION_ADDRESS + imageOffset;
        const uint32_t remaining = (uint32_t)length - consumed;
        const uint16_t span = updateEpcsWriteSpan(address, remaining);

        if (span == 0u) {
            updateStateFail(&glUpdateState, UPDATE_ERROR_WRITE);
            return CyFalse;
        }

        if (updateEpcsSectorStartsHere(address)) {
            // Seconds, and the host is waiting on the control transfer that
            // delivered this chunk for all of them. That is why the protocol
            // puts no deadline on UPDATE_DATA.
            if (!epcsFlashEraseSector(address)) {
                updateStateFail(&glUpdateState, UPDATE_ERROR_WRITE);
                return CyFalse;
            }
        }

        if (!epcsFlashProgramPage(address, data + consumed, span)) {
            updateStateFail(&glUpdateState, UPDATE_ERROR_WRITE);
            return CyFalse;
        }

        consumed += span;
        glUpdateState.written = imageOffset + span;
    }

    return CyTrue;
}

CyBool_t updateAgentData(uint8_t target, uint16_t index, uint8_t *data,
                         uint16_t length)
{
    uint8_t refusal;
    uint32_t offset;
    uint32_t consumed;

    if (data == NULL) return CyFalse;

    refusal = updateChunkIsAllowed(&glUpdateState, target, index, length);
    if (refusal != UPDATE_ERROR_NONE) {
        updateStateFail(&glUpdateState, refusal);
        return CyFalse;
    }

    // The first chunk carries the image's own signature, so this is the
    // earliest moment a payload that is not an FX3 boot image can be
    // refused — and it is before anything has been written.
    //
    // There is no equivalent for the gateware target and none is invented.
    // A raw EPCS byte stream carries no signature at all, so the only thing
    // that could be checked here is a length, which UPDATE_BEGIN already
    // checked. What stands in its place is the readback digest, which
    // catches the wrong file just as surely — only later, and after the
    // boot block has already been left invalid.
    if (glUpdateState.target == UPDATE_TARGET_EEPROM &&
        glUpdateState.received == 0 &&
        !updateImageIsPlausible(data, length, glUpdateState.length)) {
        updateStateFail(&glUpdateState, UPDATE_ERROR_IMAGE);
        return CyFalse;
    }

    // Integrity link 5: hash what arrived, in the order it arrived, so that
    // a stream that does not match what UPDATE_BEGIN promised is caught at
    // UPDATE_FINISH and never reaches the commit record.
    sha_256_write(&glUpdateStreamHash, data, length);

    if (glUpdateState.target == UPDATE_TARGET_EPCS) {
        if (!updateAgentDataEpcs(data, length)) return CyFalse;

        glUpdateState.received += length;
        glUpdateState.nextChunk++;

        return CyTrue;
    }

    offset = glUpdateState.received;
    consumed = 0;

    // The first page is held back rather than written. Everything from the
    // second page on goes straight to the medium.
    while (consumed < (uint32_t)length && offset < UPDATE_EEPROM_PAGE_SIZE) {
        glUpdateFirstPage[offset] = data[consumed];
        offset++;
        consumed++;
    }

    while (consumed < (uint32_t)length) {
        const uint32_t remaining = (uint32_t)length - consumed;
        const uint16_t span = updateEepromWriteSpan(offset, remaining);
        uint16_t writeLength = span;
        CyU3PReturnStatus_t status;

        if (span == 0) {
            updateStateFail(&glUpdateState, UPDATE_ERROR_WRITE);
            return CyFalse;
        }

        // The last page of the image is zero-padded up to a whole page,
        // because a page is the smallest thing an EEPROM can be written in.
        // The padding is outside the payload the digest covers, so it is
        // never read back into the comparison.
        if ((offset + span) == glUpdateState.length &&
            (span % UPDATE_EEPROM_PAGE_SIZE) != 0) {
            uint16_t pad;

            writeLength = (uint16_t)(UPDATE_EEPROM_PAGE_SIZE -
                                     (offset % UPDATE_EEPROM_PAGE_SIZE));

            for (pad = 0; pad < span; pad++) {
                glUpdatePagePad[pad] = data[consumed + pad];
            }
            for (pad = span; pad < writeLength; pad++) {
                glUpdatePagePad[pad] = 0;
            }

            status = updateEepromWritePage(offset, glUpdatePagePad,
                                           writeLength);
        } else {
            status = updateEepromWritePage(offset, data + consumed, span);
        }

        if (status != CY_U3P_SUCCESS) {
            CyU3PDebugPrint(4, "updateAgentData(): EEPROM write failed at %d, "
                            "error code = %d\r\n", offset, status);
            updateStateFail(&glUpdateState, UPDATE_ERROR_WRITE);
            return CyFalse;
        }

        offset += span;
        consumed += span;
        glUpdateState.written = offset;
    }

    glUpdateState.received += length;
    glUpdateState.nextChunk++;

    return CyTrue;
}

CyBool_t updateAgentFinish(uint8_t target)
{
    uint8_t refusal;
    uint32_t index;

    refusal = updateFinishIsAllowed(&glUpdateState, target);
    if (refusal != UPDATE_ERROR_NONE) {
        updateStateFail(&glUpdateState, refusal);
        return CyFalse;
    }

    sha_256_close(&glUpdateStreamHash);

    for (index = 0; index < UPDATE_DIGEST_LENGTH; index++) {
        if (glUpdateStreamDigest[index] != glUpdateState.digest[index]) {
            CyU3PDebugPrint(4, "updateAgentFinish(): stream digest mismatch; "
                            "nothing committed\r\n");
            updateStateFail(&glUpdateState, UPDATE_ERROR_STREAM_DIGEST);
            return CyFalse;
        }
    }

    // Handed to the application thread rather than done here. The readback
    // is tens of seconds of I2C, or a minute or two of SPI through the flash
    // bridge, and this runs in the USB driver's setup callback where a
    // control request that took that long would be abandoned by the host
    // before it was answered.
    glUpdateState.phase = UPDATE_PHASE_VERIFYING;
    glUpdateState.verified = 0;
    glUpdateVerifyPending = CyTrue;

    return CyTrue;
}

// Read the application image back off the EPCS, prove it, and only then
// write the boot block that makes the FPGA boot it.
//
// Two accumulators over one pass of the flash, because the two readers of
// what is written need different numbers about the same bytes. SHA-256 is
// integrity link 6, compared against what UPDATE_BEGIN promised. CRC-32 is
// what goes into the boot block, for the factory image to check at every
// power-on — a weaker checksum, deliberately, because it is computed in
// fabric that can never be updated and it defends against corruption only.
static void updateAgentVerifyEpcs(void)
{
    uint32_t offset = 0u;
    uint32_t index;
    uint32_t imageCrc = UPDATE_CRC32_INITIAL;

    CyU3PDebugPrint(4, "updateAgentVerify(): reading %d bytes back from the "
                    "EPCS\r\n", glUpdateState.length);

    sha_256_init(&glUpdateMediumHash, glUpdateMediumDigest);

    while (offset < glUpdateState.length) {
        const uint32_t remaining = glUpdateState.length - offset;
        const uint16_t span = (remaining < UPDATE_READBACK_SIZE)
            ? (uint16_t)remaining : (uint16_t)UPDATE_READBACK_SIZE;

        if (!epcsFlashRead(UPDATE_EPCS_APPLICATION_ADDRESS + offset,
                           glUpdateReadback, span)) {
            CyU3PDebugPrint(4, "updateAgentVerify(): EPCS read failed at %d\r\n",
                            offset);
            updateStateFail(&glUpdateState, UPDATE_ERROR_READ);
            return;
        }

        sha_256_write(&glUpdateMediumHash, glUpdateReadback, span);
        imageCrc = updateCrc32Update(imageCrc, glUpdateReadback, span);

        offset += span;
        glUpdateState.verified = offset;
    }

    sha_256_close(&glUpdateMediumHash);

    for (index = 0; index < UPDATE_DIGEST_LENGTH; index++) {
        if (glUpdateMediumDigest[index] != glUpdateState.digest[index]) {
            CyU3PDebugPrint(4, "updateAgentVerify(): medium digest mismatch; "
                            "boot block not written\r\n");
            updateStateFail(&glUpdateState, UPDATE_ERROR_MEDIUM_DIGEST);
            return;
        }
    }

    // Everything the boot block will describe is now known to be right, so
    // the block may be written. This is the commit, and it is one sector:
    // before it the unit boots the factory image and reports itself in
    // recovery, after it the unit boots the gateware that has just been
    // proved.
    glUpdateState.phase = UPDATE_PHASE_WRITING;

    updateBootBlockEncode(glUpdateBootBlock, UPDATE_EPCS_APPLICATION_ADDRESS,
                          glUpdateState.length, updateCrc32Final(imageCrc));

    if (!epcsFlashEraseSector(UPDATE_EPCS_BOOT_BLOCK_ADDRESS) ||
        !epcsFlashProgramPage(UPDATE_EPCS_BOOT_BLOCK_ADDRESS, glUpdateBootBlock,
                              UPDATE_BOOT_BLOCK_LENGTH)) {
        CyU3PDebugPrint(4, "updateAgentVerify(): the boot block could not be "
                        "written\r\n");
        updateStateFail(&glUpdateState, UPDATE_ERROR_WRITE);
        return;
    }

    // And read it back, because the commit record is the one write nothing
    // downstream would catch in time: a boot block that is wrong sends the
    // unit to the factory image at the next power-on with no explanation,
    // long after the host has said the update succeeded.
    glUpdateState.phase = UPDATE_PHASE_VERIFYING;

    if (!epcsFlashRead(UPDATE_EPCS_BOOT_BLOCK_ADDRESS, glUpdateReadback,
                       UPDATE_BOOT_BLOCK_LENGTH)) {
        updateStateFail(&glUpdateState, UPDATE_ERROR_READ);
        return;
    }

    for (index = 0; index < UPDATE_BOOT_BLOCK_LENGTH; index++) {
        if (glUpdateReadback[index] != glUpdateBootBlock[index]) {
            updateStateFail(&glUpdateState, UPDATE_ERROR_MEDIUM_DIGEST);
            return;
        }
    }

    glUpdateState.written = glUpdateState.length;
    glUpdateState.verified = glUpdateState.length;
    glUpdateState.phase = UPDATE_PHASE_COMPLETE;

    CyU3PDebugPrint(4, "updateAgentVerify(): gateware update complete; reload "
                    "the FPGA to run it\r\n");
}

void updateAgentVerify(void)
{
    uint32_t address;
    uint32_t index;
    CyU3PReturnStatus_t status;

    glUpdateVerifyPending = CyFalse;

    if (glUpdateState.phase != UPDATE_PHASE_VERIFYING) return;

    if (glUpdateState.target == UPDATE_TARGET_EPCS) {
        updateAgentVerifyEpcs();
        return;
    }

    CyU3PDebugPrint(4, "updateAgentVerify(): reading %d bytes back from the "
                    "EEPROM\r\n", glUpdateState.length);

    sha_256_init(&glUpdateMediumHash, glUpdateMediumDigest);

    // Integrity link 6, first part: the held-back page. It is the one span
    // of the image that is not on the medium yet, and hashing the copy that
    // is about to be written is the only thing that can be done about that.
    // It is also the span the host will confirm by other means, because the
    // signature page is what decides whether the device boots this image at
    // all.
    sha_256_write(&glUpdateMediumHash, glUpdateFirstPage,
                  UPDATE_EEPROM_PAGE_SIZE);
    glUpdateState.verified = UPDATE_EEPROM_PAGE_SIZE;

    address = UPDATE_EEPROM_PAGE_SIZE;
    while (address < glUpdateState.length) {
        const uint32_t remaining = glUpdateState.length - address;
        const uint16_t span = updateEepromReadSpan(address, remaining,
                                                   UPDATE_READBACK_SIZE);

        if (span == 0) {
            updateStateFail(&glUpdateState, UPDATE_ERROR_READ);
            return;
        }

        status = updateEepromRead(address, glUpdateReadback, span);
        if (status != CY_U3P_SUCCESS) {
            CyU3PDebugPrint(4, "updateAgentVerify(): EEPROM read failed at %d, "
                            "error code = %d\r\n", address, status);
            updateStateFail(&glUpdateState, UPDATE_ERROR_READ);
            return;
        }

        sha_256_write(&glUpdateMediumHash, glUpdateReadback, span);

        address += span;
        glUpdateState.verified = address;
    }

    sha_256_close(&glUpdateMediumHash);

    for (index = 0; index < UPDATE_DIGEST_LENGTH; index++) {
        if (glUpdateMediumDigest[index] != glUpdateState.digest[index]) {
            CyU3PDebugPrint(4, "updateAgentVerify(): medium digest mismatch; "
                            "signature page not written\r\n");
            updateStateFail(&glUpdateState, UPDATE_ERROR_MEDIUM_DIGEST);
            return;
        }
    }

    // Everything else on the medium is now known to be right, so the page
    // that makes the boot ROM accept it may be written. This is the commit,
    // and it is one page: before it the kit falls back to the USB
    // bootloader, after it the kit boots the new firmware.
    glUpdateState.phase = UPDATE_PHASE_WRITING;

    status = updateEepromWritePage(0, glUpdateFirstPage,
                                   UPDATE_EEPROM_PAGE_SIZE);
    if (status != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "updateAgentVerify(): signature page write failed, "
                        "error code = %d\r\n", status);
        updateStateFail(&glUpdateState, UPDATE_ERROR_WRITE);
        return;
    }

    // And read it back, because the commit record is the one write nothing
    // downstream would catch: an image whose signature page is wrong is an
    // image the boot ROM silently declines, and the device would come back
    // in the bootloader with no explanation.
    glUpdateState.phase = UPDATE_PHASE_VERIFYING;

    status = updateEepromRead(0, glUpdateReadback, UPDATE_EEPROM_PAGE_SIZE);
    if (status != CY_U3P_SUCCESS) {
        updateStateFail(&glUpdateState, UPDATE_ERROR_READ);
        return;
    }

    for (index = 0; index < UPDATE_EEPROM_PAGE_SIZE; index++) {
        if (glUpdateReadback[index] != glUpdateFirstPage[index]) {
            updateStateFail(&glUpdateState, UPDATE_ERROR_MEDIUM_DIGEST);
            return;
        }
    }

    glUpdateState.written = updateEepromPadToPage(glUpdateState.length);
    glUpdateState.verified = glUpdateState.length;
    glUpdateState.phase = UPDATE_PHASE_COMPLETE;

    CyU3PDebugPrint(4, "updateAgentVerify(): update complete; reset the device "
                    "to run it\r\n");
}

CyBool_t updateAgentReconfigureFpga(void)
{
    // Refused while a transfer is open, for the same reason a capture is:
    // reconfiguring the FPGA part way through writing its own configuration
    // flash would pull the bridge out from under the write in progress.
    if (updateIsInProgress(&glUpdateState)) return CyFalse;

    CyU3PDebugPrint(4, "updateAgentReconfigureFpga(): host asked the FPGA to "
                    "reload itself\r\n");

    return epcsFlashReconfigureFpga();
}

void updateAgentResetDevice(void)
{
    // A cold reset, so the FX3 re-reads its boot source rather than
    // restarting the image already in RAM. It also closes a long-standing
    // gap: until now the host had no way to reboot the device at all.
    CyU3PDeviceReset(CyFalse);
}
