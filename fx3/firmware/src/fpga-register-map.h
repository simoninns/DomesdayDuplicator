/************************************************************************

    fpga-register-map.h

    The FPGA register map, and the decisions about it that need no hardware
    DomesdayDuplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    Deliberately free of the FX3 SDK. Everything here is arithmetic over
    bytes the host sent or the FPGA returned, so it compiles and runs on a
    build machine and is tested there — which makes it the only part of this
    firmware that has automated coverage at all. The transport that carries
    the bytes is in fpga-registers.h, and cannot be tested off hardware.

    The map itself is specified on the "FPGA register interface" page of
    the documentation site. This header is the firmware's copy of it, not
    its definition.

************************************************************************/

#ifndef _FPGA_REGISTER_MAP_H_
#define _FPGA_REGISTER_MAP_H_

#include <stddef.h>
#include <stdint.h>

// Register addresses
#define FPGA_REGISTER_ID                (0x00u)
#define FPGA_REGISTER_MAP_VERSION       (0x01u)
#define FPGA_REGISTER_BUILD_FLAGS       (0x02u)
#define FPGA_REGISTER_COMMIT            (0x03u)
#define FPGA_REGISTER_TEST_MODE         (0x10u)
#define FPGA_REGISTER_LED               (0x11u)

// The command byte carries a seven-bit address, so this is the whole space
#define FPGA_REGISTER_ADDRESS_MAX       (0x7Fu)

// The signature at FPGA_REGISTER_ID.
//
// Neither 0x00 nor 0xFF, deliberately. SPI has no acknowledgement, so an
// absent or unconfigured FPGA does not fail a transfer — it returns whatever
// the MISO line happens to carry, which is all-ones if the pin floats and
// all-zeros if it is held down. This value is the only thing that
// distinguishes a real register bank from either.
#define FPGA_IDENTITY_VALUE             (0x44u)

// The map version this firmware was written against
#define FPGA_IDENTITY_MAP_VERSION       (0x01u)

// Identity block: signature, map version, build flags, eight commit characters
#define FPGA_IDENTITY_LENGTH            (11u)
#define FPGA_COMMIT_LENGTH              (8u)

// Bits of FPGA_REGISTER_BUILD_FLAGS
#define FPGA_BUILD_FLAG_DIRTY           (0x01u)
#define FPGA_BUILD_FLAG_COMMIT          (0x02u)

// The largest read a single vendor request may ask for
#define FPGA_REGISTER_READ_MAX          (64u)

// LED patterns.
//
// The gateware no longer generates a pattern of its own, so what the eight
// LEDs mean is entirely this firmware's choice. Three states are worth
// distinguishing at a glance across a room, and the reset value the gateware
// shows before the firmware ever writes here — a single lit LED — is a fourth.
#define FPGA_LED_READY                  (0x81u)     // enumerated, register link up
#define FPGA_LED_CAPTURING              (0xFFu)     // the host is collecting
#define FPGA_LED_BUFFER_ERROR           (0x55u)     // the FPGA reported an overflow
#define FPGA_LED_UPDATING               (0x18u)     // rewriting the boot EEPROM

// Does this identity block come from a gateware register bank?
int fpgaIdentityIsValid(const uint8_t *identity);

// The register map version the gateware implements, or zero if the block is
// not one this firmware recognises.
uint8_t fpgaIdentityMapVersion(const uint8_t *identity);

// Was the gateware built from a tree with uncommitted changes?
int fpgaIdentityIsDirty(const uint8_t *identity);

// The commit the gateware was built from, as a NUL-terminated string.
//
// Empty when the block names no commit, which covers a gateware built outside
// a checkout as well as a block that was never read. Characters that are not
// hex digits end the string, so a misread link cannot put arbitrary bytes into
// the debug log.
void fpgaIdentityCommitText(const uint8_t *identity, char *text, size_t size);

// May the host write this register?
//
// Separate from whether the gateware would accept the write, which it would
// for any read/write register. This is firmware policy about who owns what.
int fpgaRegisterIsHostWritable(uint8_t address);

// Should a 0xB7 register-read request be honoured?
int fpgaReadRequestIsValid(uint16_t address, uint16_t length);

// Should a 0xB8 register-write request be honoured? The address is the high
// byte of wValue and the value to write is the low byte.
int fpgaWriteRequestIsValid(uint16_t value);

#endif // _FPGA_REGISTER_MAP_H_
