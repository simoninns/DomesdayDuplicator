/************************************************************************

    update-agent.h

    Rewriting the FX3's own boot EEPROM, commanded from the host
    DomesdayDuplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    The transport half of the device update mechanism. What the protocol
    means, and every decision that can be made without touching hardware,
    is in update-protocol.h — which is where the host-testable half lives.

    The FX3 boots from an I2C EEPROM on the Explorer Kit. Its I2C pins are
    dedicated — not shared with the 16-bit GPIF bus or the UART — so the
    running firmware can bring up the I2C block and rewrite its own boot
    source with no jumper, no second cable and no personality change. That
    is the whole of the mechanism: this file is the flasher, and the host
    is only the thing that hands it bytes.

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

// UPDATE_DATA. The chunk is hashed and written straight to the EEPROM,
// except for the first page, which is held back until everything else has
// been written and verified.
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

// Read the written region back off the EEPROM, check it against the digest
// UPDATE_BEGIN carried, and only then write the signature page that makes
// the image bootable.
//
// Runs on the application thread and takes tens of seconds. The commit
// ordering is the safety mechanism: until the last write here, the EEPROM
// holds an image the boot ROM rejects, so an update interrupted anywhere
// before it leaves a device that falls back to the USB bootloader — a
// state the capture application recognises and can repair from.
void updateAgentVerify(void);

// Cold reset, so the device re-reads its boot source and comes back
// running whatever is now in the EEPROM. Does not return.
void updateAgentResetDevice(void);

#include <cyu3externcend.h>

#endif // _UPDATE_AGENT_H_
