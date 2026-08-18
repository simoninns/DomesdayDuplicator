/************************************************************************

    update-agent.h

    Rewriting the device's two flash memories, commanded from the host
    DomesdayDuplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    The transport half of the device update mechanism. What the protocol
    means, and every decision that can be made without touching hardware,
    is in update-protocol.h — which is where the host-testable half lives.

    One agent, two targets, and they are unalike in everything except the
    protocol that reaches them.

    **Target 0, the FX3's boot EEPROM.** The FX3 boots from an I2C EEPROM on
    the Explorer Kit. Its I2C pins are dedicated — not shared with the
    16-bit GPIF bus or the UART — so the running firmware can bring up the
    I2C block and rewrite its own boot source with no jumper, no second
    cable and no personality change. The commit is the first page, which
    carries the 'CY' signature, and it is written last.

    **Target 1, the FPGA's EPCS configuration flash.** No wire runs from the
    FX3 to the FPGA's configuration circuitry, so this goes the long way
    round: the SPI register link to the gateware, the flash bridge, the
    asmiblock and then the flash. epcs-flash.h holds that route; what is
    here is the sequencing above it. The commit is the boot block, and it is
    written last.

    Both are the same shape on purpose. Everything is written and verified
    before the one write that makes it count, so an update interrupted
    anywhere leaves a device in a rescue state the capture application
    recognises — the USB bootloader for the FX3, the factory gateware for
    the FPGA — and never one that half works.

    Two threads reach this file, and which one may act is decided by the
    phase rather than by a lock:

      * the USB setup callback runs UPDATE_BEGIN, UPDATE_DATA and
        UPDATE_FINISH, and does the page writes inline. Each of those is
        refused unless the phase says it is that thread's turn;
      * the application thread runs the readback verification, which takes
        tens of seconds and may only run in the verifying phase — a phase
        in which every request the setup callback would honour is refused.

    A mutex would be the obvious alternative and would be worse. The
    verification holds the medium for the whole of its run, so a lock
    around it would block endpoint 0 — including the status request that
    the host's progress bar is made of, which is precisely the thing a user
    is watching while it happens.

    UPDATE_STATUS is therefore deliberately lock-free: it reads counters
    another thread is incrementing. Each is a naturally aligned 32-bit word
    on this processor, so a reader sees an old value or a new one and never
    half of each, and a progress counter one poll behind is not a defect.

************************************************************************/

#ifndef _UPDATE_AGENT_H_
#define _UPDATE_AGENT_H_

#include "cyu3externcstart.h"
#include "cyu3types.h"

#include "update-protocol.h"

// Bring up the I2C block.
//
// Called from the application thread once the kernel is running, because
// CyU3PI2cInit() allocates. A failure is not fatal: the device enumerates
// and captures normally, and only the ability to update itself is lost —
// which the host is told about through UPDATE_ERROR_HARDWARE rather than
// through a device that answers nothing.
CyU3PReturnStatus_t updateAgentStart(void);

// The state the status request reports. Never null.
const updateState_t *updateAgentState(void);

// Is an update under way? This is what the capture start request refuses
// on, and it is the other half of the mutual exclusion that
// updateBeginIsAllowed() enforces from the update side.
CyBool_t updateAgentInProgress(void);

// UPDATE_BEGIN, with its data stage exactly as it arrived. Decodes it,
// decides whether it may be accepted now, and if it may, opens the
// transfer. captureRunning is the host's collection flag, and refusing on
// it is one half of the capture/update mutual exclusion.
//
// Returns true if the request was accepted. A false return has already
// recorded why in the state, so the host reads the reason with
// UPDATE_STATUS rather than inferring it from a stall.
CyBool_t updateAgentBegin(uint8_t target, const uint8_t *data, uint16_t length,
                          CyBool_t captureRunning);

// UPDATE_DATA. The chunk is hashed and written straight to the medium as
// it arrives, with no assembly buffer in between.
//
// On the EEPROM the first page is held back until everything else has been
// written and verified. On the EPCS nothing is held back — the commit is a
// separate boot block rather than a page of the image — but each sector is
// erased as the write first enters it, which is why one chunk in thirty-two
// takes about a second longer than its neighbours.
CyBool_t updateAgentData(uint8_t target, uint16_t index, uint8_t *data,
                         uint16_t length);

// UPDATE_FINISH. Checks the stream digest and hands the transfer to the
// application thread for readback verification; it does not block.
//
// The verification is deliberately not done here. It reads the whole
// written region back off the medium, which takes tens of seconds, and a
// control request that took tens of seconds to answer would be abandoned
// by the host long before it did.
CyBool_t updateAgentFinish(uint8_t target);

// Write the 16-byte status packet. Answerable at any time, including when
// no update has ever been started, and it is how the host discovers the
// chunk size rather than assuming one.
void updateAgentStatus(uint8_t *out);

// Does the application thread have verification work to do?
CyBool_t updateAgentVerifyPending(void);

// Read the written region back off the medium, check it against the digest
// UPDATE_BEGIN carried, and only then write the record that makes the image
// count.
//
// Runs on the application thread and takes tens of seconds for the EEPROM
// and a minute or two for the EPCS. The commit ordering is the safety
// mechanism in both cases: until the last write here, the medium holds
// something its reader rejects — an image the boot ROM will not accept, or
// a boot block whose checksum does not describe what is beside it — so an
// update interrupted anywhere before it leaves a device in a rescue state
// the capture application recognises and can repair from.
void updateAgentVerify(void);

// UPDATE_FPGA_RECONFIG. Ask the FPGA to reload itself, which is how a
// gateware update takes effect without a power cycle.
//
// Refused when no gateware with a flash bridge answered, because
// acknowledging it would tell a host that a reconfiguration had happened.
CyBool_t updateAgentReconfigureFpga(void);

// Cold reset, so the device re-reads its boot source and comes back
// running whatever is now in the EEPROM. Does not return.
void updateAgentResetDevice(void);

#include <cyu3externcend.h>

#endif // _UPDATE_AGENT_H_
