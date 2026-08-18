/************************************************************************

    fpga-registers.h

    Reaching the FPGA register bank over a bit-banged SPI link
    DomesdayDuplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    The transport. What the registers mean, and every decision that can be
    made without touching hardware, is in fpga-register-map.h — which is
    where the host-testable half of this lives.

    Four of the five former configuration lines carry the link:

        GPIO 22  CTL_05  SCLK  output
        GPIO 23  CTL_06  MOSI  output
        GPIO 24  CTL_07  MISO  input
        GPIO 25  CTL_08  CS_N  output, idle high

    GPIO 26 (CTL_09) is left over, driven low and reserved.

    Bit-banged rather than using the FX3's SPI block, which sits on LPP pins
    the board does not route to the FPGA. Every line is push-pull and
    point-to-point, so there is nothing to arbitrate and no pull-up to wait
    for; the master generates every clock edge, so a transfer cannot hang and
    nothing here needs a timeout.

************************************************************************/

#ifndef _FPGA_REGISTERS_H_
#define _FPGA_REGISTERS_H_

#include "cyu3externcstart.h"
#include "cyu3types.h"

#include "fpga-register-map.h"

// How hard to look for the FPGA at start-up. The Cyclone IV loads from EPCS
// in well under a second and the FX3 has already enumerated by the time this
// runs, so ten attempts twenty milliseconds apart is generous rather than
// marginal.
#define FPGA_STARTUP_PROBE_ATTEMPTS (10)

// How often to look again when nothing answered, counted in passes of the
// application thread's loop. That loop sleeps for 10 ms, so this is about two
// seconds.
#define FPGA_RECHECK_LOOPS          (200)

// Claim and configure the four SPI pins, and the reserved fifth.
//
// Called from main() with the other GPIO setup, before the RTOS starts, so it
// must not touch any kernel object.
CyU3PReturnStatus_t fpgaRegistersInitialise(void);

// Create the lock that keeps the application thread and the USB setup
// callback out of each other's transfers. Must be called once the kernel is
// running and before anything else here is used.
CyU3PReturnStatus_t fpgaRegistersStart(void);

// Look for the register bank and remember whether it answered.
//
// attempts is how many times to try before giving up. The probe at start-up
// is generous, because the FPGA loads its configuration from EPCS while the
// FX3 boots and neither waits for the other; the periodic recheck the
// application thread makes passes 1, because it will come round again.
//
// A failure is not fatal: the device stays enumerated and captures normally,
// and only the version report and the LEDs are lost.
CyBool_t fpgaRegistersProbe(uint8_t attempts);

// Did the last probe find a register bank? This is what the vendor requests
// refuse on, so that a host request never waits on retries.
CyBool_t fpgaRegistersPresent(void);

// Does the gateware the last probe found carry the flash bridge?
//
// The gateware update path refuses on this rather than discovering it by
// writing to registers that are not there. A gateware predating map version
// 2 answers everything the capture path needs and has no bridge at all, so
// the writes would go nowhere and the readback would be whatever the flash
// happened to hold — which is precisely the failure mode that must not be
// reported as a successful update.
CyBool_t fpgaRegistersHasFlashBridge(void);

// Which of the two gateware images answered the last probe. Meaningless
// unless fpgaRegistersHasFlashBridge() is true.
uint8_t fpgaRegistersImageRole(void);

// Forget what the last probe found, so the application thread looks again.
//
// Called when the FPGA has been told to reconfigure: everything above is
// answered from the identity block read at the last probe, and reloading the
// FPGA is precisely the operation that changes which image that block came
// from. The thread's periodic recheck picks the new one up a couple of
// seconds later.
void fpgaRegistersForgetProbe(void);

// Read length bytes starting at address. The address auto-increments in the
// gateware, so the identity block is one transfer.
CyBool_t fpgaRegistersRead(uint8_t address, uint8_t *buffer, uint8_t length);

// Write one byte to one register.
CyBool_t fpgaRegistersWrite(uint8_t address, uint8_t value);

// Write a run of bytes in one framed transaction.
//
// The address auto-increments in the gateware exactly as it does for a read,
// so this writes consecutive registers — with one exception that is the
// reason this exists at all. BRIDGE_DATA at 0x22 does *not* increment: it is
// a port rather than a location, and each byte written to it shifts one byte
// out to the EPCS. A flash page program is therefore one transaction of 260
// bytes rather than 260 transactions of one, which halves the cost of the
// slowest operation in a gateware update.
//
// Safe at any length: the bridge's own shift takes eight of its 10 MHz clock
// periods, which is less than a microsecond, and the next byte of this
// transaction cannot arrive for tens of microseconds.
CyBool_t fpgaRegistersWriteBurst(uint8_t address, const uint8_t *data,
                                 uint16_t length);

// Drive the status LEDs. A no-op returning CyFalse when no bank was found.
CyBool_t fpgaRegistersSetLeds(uint8_t pattern);

#include <cyu3externcend.h>

#endif // _FPGA_REGISTERS_H_
