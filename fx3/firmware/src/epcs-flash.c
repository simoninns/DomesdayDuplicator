/************************************************************************

    epcs-flash.c

    Reaching the FPGA's configuration flash through the gateware's bridge
    DomesdayDuplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3error.h"

#include "epcs-flash.h"
#include "fpga-registers.h"

// How long to wait for an erase or a program to finish, in milliseconds.
//
// The longest thing that happens here is a 64 KiB sector erase, specified
// at three seconds worst case. Eight is that with room to spare, and it is
// a bound rather than a patience setting: an erase that has not finished in
// eight seconds is a fault, and a loop with no bound at all would hang the
// thread that answers endpoint 0.
#define EPCS_READY_TIMEOUT_MS       (8000u)

// How long to sleep between polls of the status register. One millisecond
// costs a thousandth of an erase and saves several thousand SPI
// transactions spent asking a flash that is obviously still busy.
#define EPCS_POLL_INTERVAL_MS       (1u)

// The command buffer: an opcode, three address bytes and a page of data.
// Static because it is handed to the register transport whole, and because
// 260 bytes is more than this thread's stack should be asked for.
static uint8_t glEpcsCommand[4u + UPDATE_EPCS_PAGE_SIZE];

// Unlock the bridge.
//
// Four separate transactions rather than one burst, and it has to be: the
// register address post-increments, so four bytes written to 0x20 in one
// frame would land in 0x20, 0x21, 0x22 and 0x23 - which is exactly the
// accident the four-byte sequence exists to make impossible.
//
// The unlock is held for a whole flash operation rather than for each of
// the framed commands inside one. Locking the bridge tri-states the active
// serial pins, so a lock taken between the write-enable and the page
// program it enables would leave the flash's chip select floating in the
// middle of an operation. Between operations it is high and the flash is
// idle, which is the state the device sits in whenever it is not updating.
static CyBool_t epcsUnlock(void)
{
    if (!fpgaRegistersHasFlashBridge()) return CyFalse;

    if (!fpgaRegistersWrite(FPGA_REGISTER_BRIDGE_UNLOCK, FPGA_BRIDGE_UNLOCK_0)) return CyFalse;
    if (!fpgaRegistersWrite(FPGA_REGISTER_BRIDGE_UNLOCK, FPGA_BRIDGE_UNLOCK_1)) return CyFalse;
    if (!fpgaRegistersWrite(FPGA_REGISTER_BRIDGE_UNLOCK, FPGA_BRIDGE_UNLOCK_2)) return CyFalse;
    if (!fpgaRegistersWrite(FPGA_REGISTER_BRIDGE_UNLOCK, FPGA_BRIDGE_UNLOCK_3)) return CyFalse;

    return CyTrue;
}

// Deassert chip select and lock the bridge, in that order.
//
// The order matters more than the return value, which is why there is not
// one: chip select is driven high while the pins are still driven, and only
// then are they released. The other way round would leave the flash
// deselected by a pull-up rather than by the device that selected it.
//
// Runs on the way out of a failed operation as well as a successful one.
static void epcsLock(void)
{
    (void)fpgaRegistersWrite(FPGA_REGISTER_BRIDGE_CONTROL, 0x00u);
    (void)fpgaRegistersWrite(FPGA_REGISTER_BRIDGE_UNLOCK, FPGA_BRIDGE_LOCK);
}

// Chip select, within an unlocked bridge. Framing for one flash command.
static CyBool_t epcsAssert(void)
{
    return fpgaRegistersWrite(FPGA_REGISTER_BRIDGE_CONTROL, FPGA_BRIDGE_SELECT);
}

static CyBool_t epcsRelease(void)
{
    return fpgaRegistersWrite(FPGA_REGISTER_BRIDGE_CONTROL, 0x00u);
}

// Shift one byte out and return the byte that arrived in its place.
//
// Two register transactions, and no wait between them. The bridge's shift
// takes eight of its 10 MHz clock periods - under a microsecond - while a
// register transaction over the bit-banged link takes tens of them, so the
// byte has been latched long before the read that collects it begins.
static CyBool_t epcsTransfer(uint8_t send, uint8_t *received)
{
    if (!fpgaRegistersWrite(FPGA_REGISTER_BRIDGE_DATA, send)) return CyFalse;

    if (received == NULL) return CyTrue;

    return fpgaRegistersRead(FPGA_REGISTER_BRIDGE_DATA, received, 1u);
}

// Shift a run of bytes out, discarding what comes back. One transaction,
// because BRIDGE_DATA is a port and does not auto-increment.
static CyBool_t epcsSend(const uint8_t *data, uint16_t length)
{
    return fpgaRegistersWriteBurst(FPGA_REGISTER_BRIDGE_DATA, data, length);
}

// Set the write enable latch, which every erase and every program needs and
// which the flash clears by itself when the operation completes.
//
// A framed command of its own: the latch is set by the rising edge of chip
// select at the end of it, so an enable sharing a frame with the command it
// enables would never take effect.
static CyBool_t epcsWriteEnable(void)
{
    const uint8_t command = UPDATE_EPCS_WRITE_ENABLE;

    if (!epcsAssert()) return CyFalse;
    if (!epcsSend(&command, 1u)) return CyFalse;

    return epcsRelease();
}

// Wait for the write in progress bit to clear.
//
// The read-status command may be left running: the flash returns its status
// register for as long as the master keeps clocking, so this is one framed
// command rather than one per poll. Chip select stays asserted across the
// sleeps, which is what the part expects.
static CyBool_t epcsWaitReady(void)
{
    const uint8_t command = UPDATE_EPCS_READ_STATUS;
    uint32_t waited = 0u;
    uint8_t status = UPDATE_EPCS_STATUS_BUSY;

    if (!epcsAssert()) return CyFalse;
    if (!epcsSend(&command, 1u)) return CyFalse;

    while (waited < EPCS_READY_TIMEOUT_MS) {
        if (!epcsTransfer(0xFFu, &status)) return CyFalse;

        if ((status & UPDATE_EPCS_STATUS_BUSY) == 0u) {
            return epcsRelease();
        }

        CyU3PThreadSleep(EPCS_POLL_INTERVAL_MS);
        waited += EPCS_POLL_INTERVAL_MS;
    }

    CyU3PDebugPrint(4, "epcsWaitReady(): the flash was still busy after %d ms\r\n",
                    EPCS_READY_TIMEOUT_MS);
    return CyFalse;
}

// Fill in an opcode and its three address bytes, most significant first.
static void epcsCommand(uint8_t opcode, uint32_t address)
{
    glEpcsCommand[0] = opcode;
    glEpcsCommand[1] = (uint8_t)((address >> 16) & 0xFFu);
    glEpcsCommand[2] = (uint8_t)((address >> 8) & 0xFFu);
    glEpcsCommand[3] = (uint8_t)(address & 0xFFu);
}

CyBool_t epcsFlashIdentify(uint8_t *siliconId)
{
    uint8_t received = 0x00u;
    uint8_t index;

    if (siliconId == NULL) return CyFalse;

    *siliconId = 0x00u;

    if (!epcsUnlock()) return CyFalse;

    if (!epcsAssert()) {
        epcsLock();
        return CyFalse;
    }

    glEpcsCommand[0] = UPDATE_EPCS_READ_SILICON_ID;
    for (index = 0u; index < UPDATE_EPCS_ID_DUMMY_BYTES; index++) {
        glEpcsCommand[1u + index] = 0x00u;
    }

    if (!epcsSend(glEpcsCommand, (uint16_t)(1u + UPDATE_EPCS_ID_DUMMY_BYTES)) ||
        !epcsTransfer(0xFFu, &received)) {
        epcsLock();
        return CyFalse;
    }

    epcsLock();

    *siliconId = received;

    // 0x00 and 0xFF are what a line with nothing driving it reads as, so
    // neither is an identifier however much it looks like one.
    return (received != 0x00u && received != 0xFFu) ? CyTrue : CyFalse;
}

CyBool_t epcsFlashEraseSector(uint32_t address)
{
    const uint32_t base = updateEpcsSectorBase(address);

    if (!epcsUnlock()) return CyFalse;

    epcsCommand(UPDATE_EPCS_ERASE_SECTOR, base);

    // The erase begins when chip select rises at the end of the command, so
    // the wait that follows is a framed command of its own.
    if (!epcsWriteEnable() ||
        !epcsAssert() ||
        !epcsSend(glEpcsCommand, 4u) ||
        !epcsRelease() ||
        !epcsWaitReady()) {
        epcsLock();
        CyU3PDebugPrint(4, "epcsFlashEraseSector(): the sector at %d did not erase\r\n",
                        base);
        return CyFalse;
    }

    epcsLock();
    return CyTrue;
}

CyBool_t epcsFlashProgramPage(uint32_t address, const uint8_t *data,
                              uint16_t length)
{
    uint16_t index;

    if (data == NULL || length == 0u || length > UPDATE_EPCS_PAGE_SIZE) {
        return CyFalse;
    }

    if (!epcsUnlock()) return CyFalse;

    epcsCommand(UPDATE_EPCS_PAGE_PROGRAM, address);

    for (index = 0u; index < length; index++) {
        glEpcsCommand[4u + index] = data[index];
    }

    // Command, address and payload in one framed transaction, which is what
    // makes this affordable: the alternative is one register transaction per
    // byte over a link that costs tens of microseconds for each of them.
    if (!epcsWriteEnable() ||
        !epcsAssert() ||
        !epcsSend(glEpcsCommand, (uint16_t)(4u + length)) ||
        !epcsRelease() ||
        !epcsWaitReady()) {
        epcsLock();
        CyU3PDebugPrint(4, "epcsFlashProgramPage(): the page at %d did not program\r\n",
                        address);
        return CyFalse;
    }

    epcsLock();
    return CyTrue;
}

CyBool_t epcsFlashRead(uint32_t address, uint8_t *data, uint16_t length)
{
    uint16_t index;

    if (data == NULL || length == 0u) return CyFalse;

    if (!epcsUnlock()) return CyFalse;

    epcsCommand(UPDATE_EPCS_READ_BYTES, address);

    if (!epcsAssert() || !epcsSend(glEpcsCommand, 4u)) {
        epcsLock();
        return CyFalse;
    }

    // One byte at a time, and this is the slow half of a gateware update.
    // Each byte costs a write to shift it out of the flash and a read to
    // collect what came back, because the register bank drives zeros on
    // MISO during a write transaction and the two therefore cannot share a
    // frame. The flash's own address counter runs on for as long as it is
    // clocked, so the whole read is one command however long it is.
    for (index = 0u; index < length; index++) {
        if (!epcsTransfer(0xFFu, &data[index])) {
            epcsLock();
            return CyFalse;
        }
    }

    epcsLock();
    return CyTrue;
}

CyBool_t epcsFlashReconfigureFpga(void)
{
    if (!fpgaRegistersHasFlashBridge()) return CyFalse;

    // A plain register write, and the last thing this firmware asks of this
    // gateware: the FPGA reconfigures out from under the link within a few
    // milliseconds of it.
    if (!fpgaRegistersWrite(FPGA_REGISTER_RECONFIG_CONTROL,
                            FPGA_RECONFIG_TRIGGER)) {
        return CyFalse;
    }

    // What is running on the other end of the link is now a different image
    // from the one the last probe read, and which image it is is the whole
    // question this operation exists to change. Forgetting the probe makes
    // the application thread look again a couple of seconds later rather
    // than answering from an identity block that has been superseded.
    fpgaRegistersForgetProbe();

    return CyTrue;
}
