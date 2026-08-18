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
#define FPGA_REGISTER_IMAGE_ROLE        (0x0Bu)
#define FPGA_REGISTER_TEST_MODE         (0x10u)
#define FPGA_REGISTER_LED               (0x11u)
#define FPGA_REGISTER_DECIMATION        (0x12u)

// Decimation factors. The register holds the factor rather than a flag, so
// reading it back says what the capture path is doing rather than echoing what
// was asked for.
#define FPGA_DECIMATION_EVERY_SAMPLE    (0x01u)
#define FPGA_DECIMATION_HALF_RATE       (0x02u)

// Map version 2's flash bridge and reconfiguration control. These are the
// only registers whose writes have an effect outside the register bank:
// through them the firmware reaches the EPCS configuration flash and asks
// the FPGA to reload itself.
#define FPGA_REGISTER_BRIDGE_UNLOCK     (0x20u)
#define FPGA_REGISTER_BRIDGE_CONTROL    (0x21u)
#define FPGA_REGISTER_BRIDGE_DATA       (0x22u)
#define FPGA_REGISTER_RECONFIG_CONTROL  (0x23u)

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

// The map version this firmware was written against.
//
// Version 2 is the one that carries the flash bridge, so it is not only a
// version this firmware knows about: it is the version a device has to
// implement before its gateware can be updated at all. A device reporting
// version 1 is a device that predates the two-image model and needs the
// bench procedure once.
#define FPGA_IDENTITY_MAP_VERSION       (0x02u)

// The oldest map version whose gateware carries the flash bridge
#define FPGA_MAP_VERSION_WITH_BRIDGE    (0x02u)

// Identity block: signature, map version, build flags, eight commit
// characters and the image role, contiguous so that one transaction fetches
// all of it.
#define FPGA_IDENTITY_LENGTH            (12u)
#define FPGA_COMMIT_LENGTH              (8u)

// Bits of FPGA_REGISTER_BUILD_FLAGS
#define FPGA_BUILD_FLAG_DIRTY           (0x01u)
#define FPGA_BUILD_FLAG_COMMIT          (0x02u)

// What FPGA_REGISTER_IMAGE_ROLE reports. Only meaningful from map version 2;
// a version 1 gateware has no such register and reads 0x00 for it, which is
// why the role is believed only when the map version says it exists.
#define FPGA_IMAGE_ROLE_FACTORY         (0x00u)
#define FPGA_IMAGE_ROLE_APPLICATION     (0x01u)

// The four bytes that unlock the flash bridge, written one per transaction
// to FPGA_REGISTER_BRIDGE_UNLOCK. Any other write there locks it again.
//
// Four bytes rather than one magic value because the register address
// post-increments, so each byte is a transaction of its own and no run of
// bytes across the map can produce the sequence as a side effect. The lock
// is what stands between a stray register write and an unbootable board.
#define FPGA_BRIDGE_UNLOCK_0            (0x44u)
#define FPGA_BRIDGE_UNLOCK_1            (0x44u)
#define FPGA_BRIDGE_UNLOCK_2            (0x55u)
#define FPGA_BRIDGE_UNLOCK_3            (0xAAu)

// Anything at all relocks it; zero by convention.
#define FPGA_BRIDGE_LOCK                (0x00u)

// FPGA_REGISTER_BRIDGE_UNLOCK reads the lock state rather than the position
// in the sequence.
#define FPGA_BRIDGE_UNLOCKED            (0x01u)

// Bits of FPGA_REGISTER_BRIDGE_CONTROL: write bit 0 to assert the flash's
// chip select; read bit 0 back as that state and bit 1 as a byte shift in
// progress.
#define FPGA_BRIDGE_SELECT              (0x01u)
#define FPGA_BRIDGE_BUSY                (0x02u)

// Bits of FPGA_REGISTER_RECONFIG_CONTROL. Writing bit 1 is what makes the
// FPGA reload itself after a gateware update: it returns to the factory
// image, which then makes the same boot decision it makes at every power-on
// with whatever the flash now holds.
#define FPGA_RECONFIG_TICKLE            (0x01u)
#define FPGA_RECONFIG_TRIGGER           (0x02u)

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

// Which of the two gateware images answered, or FPGA_IMAGE_ROLE_APPLICATION
// for a map version that predates the question.
//
// The charitable default is deliberate: a version 1 gateware is a single
// image and it captures, so reporting it as a factory image would put a
// working device into a recovery state that does not exist for it.
uint8_t fpgaIdentityImageRole(const uint8_t *identity);

// Does this gateware carry the flash bridge, and can it therefore have its
// own configuration flash rewritten from here?
int fpgaIdentityHasFlashBridge(const uint8_t *identity);

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
