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

// Read length bytes starting at address. The address auto-increments in the
// gateware, so the identity block is one transfer.
CyBool_t fpgaRegistersRead(uint8_t address, uint8_t *buffer, uint8_t length);

// Write one byte to one register.
CyBool_t fpgaRegistersWrite(uint8_t address, uint8_t value);

// Drive the status LEDs. A no-op returning CyFalse when no bank was found.
CyBool_t fpgaRegistersSetLeds(uint8_t pattern);

#include <cyu3externcend.h>

#endif // _FPGA_REGISTERS_H_
