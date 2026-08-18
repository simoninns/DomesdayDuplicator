/************************************************************************

    epcs-flash.h

    Reaching the FPGA's configuration flash through the gateware's bridge
    DomesdayDuplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    The second medium the update agent writes, and the more roundabout of
    the two. The FX3 has no electrical path to the FPGA's configuration
    circuitry at all - no JTAG, no active-serial pins, no nCONFIG - so the
    only route to the EPCS is through the FPGA fabric: the bit-banged SPI
    register link to the gateware, then the flash bridge at registers 0x20
    to 0x22, then the Cyclone IV's asmiblock, then the flash.

    Four links, and this file is the only place that knows there are four.
    Everything above it asks for a sector to be erased or a page to be
    programmed; everything below it shifts bytes and knows nothing about
    what they mean.

    The command set is the flash's own and is written down in
    update-protocol.h beside the geometry, because that is the half of this
    that can be compiled and tested on a build host. What is here is only
    the sequencing, which cannot be tested anywhere but a bench.

    Two properties worth stating, because both are load-bearing:

      * the bridge is unlocked for the duration of one flash command and
        locked again afterwards. The registers behind it are reachable by
        anything that can send a 0xB8 register write and the flash holds the
        only copy of the gateware, so leaving it unlocked between commands
        would widen the one hole this design deliberately keeps shut;

      * a page program is a single framed register transaction. BRIDGE_DATA
        does not auto-increment, so 260 bytes written to it in one frame
        shift 260 bytes to the flash - which is what makes programming a
        350 KB image take tens of seconds rather than minutes.

    Reads cost twice what writes do and there is no way around it from this
    side: latching the byte that came back needs a write to shift it and a
    read to collect it, and they cannot share a frame because the register
    bank drives zeros on MISO during a write transaction. Whether that
    matters is a measurement rather than an argument - it is verification
    item V6 in TESTING.md - and if it does, the answer is a change to the
    bridge rather than to this file.

************************************************************************/

#ifndef _EPCS_FLASH_H_
#define _EPCS_FLASH_H_

#include "cyu3externcstart.h"
#include "cyu3types.h"

#include "update-protocol.h"

// Read the flash's silicon identifier.
//
// The one sanity check that can be made before anything is erased: it
// proves there is a serial flash on the far end of four links rather than a
// bridge writing into nothing. Returns CyFalse if the register bank is
// absent, if the gateware has no bridge, or if the answer is one of the two
// readings that mean nothing is there.
CyBool_t epcsFlashIdentify(uint8_t *siliconId);

// Erase the 64 KiB sector containing this address, and wait for it.
//
// Seconds, not milliseconds: the part specifies a typical erase around one
// second and a maximum of three, and this blocks for all of it. That is why
// the host imposes no deadline on an UPDATE_DATA chunk.
CyBool_t epcsFlashEraseSector(uint32_t address);

// Program up to one page, and wait for it. The caller guarantees the span
// does not cross a page boundary - that is updateEpcsWriteSpan()'s job, and
// it is tested on the host.
CyBool_t epcsFlashProgramPage(uint32_t address, const uint8_t *data,
                              uint16_t length);

// Read bytes back off the flash. No page structure applies to a read: the
// device's address counter runs on for as long as it is clocked.
CyBool_t epcsFlashRead(uint32_t address, uint8_t *data, uint16_t length);

// Ask the FPGA to reload itself.
//
// It reconfigures to the factory image, which then makes the same boot
// decision it makes at every power-on - so an update ends with the device
// booting the way it always boots, rather than the way a special case
// thought it should. Reconfiguration stops the clock underneath the capture
// path, so the caller resets the FX3 afterwards rather than leaving it
// holding a data path whose clock has gone away.
CyBool_t epcsFlashReconfigureFpga(void);

#include <cyu3externcend.h>

#endif // _EPCS_FLASH_H_
