/************************************************************************

    fpga-registers.c

    Reaching the FPGA register bank over a bit-banged SPI link
    DomesdayDuplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3error.h"
#include "cyu3gpio.h"
#include "cyu3utils.h"

#include "fpga-registers.h"

// Pin allocation. See the header, and the "FPGA register interface" page of
// the documentation site.
#define FPGA_SPI_SCLK_GPIO      (22)
#define FPGA_SPI_MOSI_GPIO      (23)
#define FPGA_SPI_MISO_GPIO      (24)
#define FPGA_SPI_CS_GPIO        (25)
#define FPGA_SPI_RESERVED_GPIO  (26)

// Half a clock period, in microseconds.
//
// This is a floor rather than the resulting rate: each edge is a register
// write through the GPIO block, which takes its own time, so the link runs
// slower than these numbers alone suggest. Somewhere around 100 kHz in
// practice, against a slave that tolerates 2 MHz — and the margin is the
// point, because nothing here is on a path where speed matters. A twelve-byte
// identity read costs under a millisecond either way.
#define FPGA_SPI_HALF_BIT_US    (2)

// Chip select setup, hold, and the gap between transfers. The specification
// asks for a microsecond; two costs nothing.
#define FPGA_SPI_SELECT_US      (2)

// How long to wait between probe attempts. How many attempts to make is the
// caller's choice — see FPGA_STARTUP_PROBE_ATTEMPTS in the header.
#define FPGA_PROBE_INTERVAL_MS  (20)

static CyBool_t glFpgaPresent = CyFalse;

// What the last successful probe read. Kept so that "does this gateware
// carry the flash bridge" and "which image is running" are answered from
// the identity block that was actually read rather than by reading the
// registers again on every question.
static uint8_t glFpgaIdentity[FPGA_IDENTITY_LENGTH];

// One transfer at a time.
//
// The application thread writes the LED register as capture state changes,
// and the USB setup callback runs register reads and writes on the driver's
// own thread. Those are two threads with no other relationship, and a
// half-finished transfer interleaved with another is the kind of fault that
// presents as an occasional wrong byte months later.
static CyU3PMutex glFpgaLock;
static CyBool_t glFpgaLockReady = CyFalse;

// Configure one pin as a plain push-pull output at a known level
static CyU3PReturnStatus_t configureOutput(uint8_t gpio, CyBool_t initialValue)
{
    CyU3PGpioSimpleConfig_t gpioConfig;
    CyU3PReturnStatus_t status;

    status = CyU3PDeviceGpioOverride(gpio, CyTrue);
    if (status != CY_U3P_SUCCESS) {
        return status;
    }

    CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
    gpioConfig.outValue = initialValue;
    gpioConfig.driveLowEn = CyTrue;
    gpioConfig.driveHighEn = CyTrue;
    gpioConfig.inputEn = CyFalse;
    gpioConfig.intrMode = CY_U3P_GPIO_NO_INTR;

    return CyU3PGpioSetSimpleConfig(gpio, &gpioConfig);
}

// Configure one pin as a plain input.
//
// No interrupt: MISO is sampled when the master clocks it and at no other
// time, so an interrupt here would fire on every bit of every transfer for
// nothing.
static CyU3PReturnStatus_t configureInput(uint8_t gpio)
{
    CyU3PGpioSimpleConfig_t gpioConfig;
    CyU3PReturnStatus_t status;

    status = CyU3PDeviceGpioOverride(gpio, CyTrue);
    if (status != CY_U3P_SUCCESS) {
        return status;
    }

    CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
    gpioConfig.outValue = CyFalse;
    gpioConfig.driveLowEn = CyFalse;
    gpioConfig.driveHighEn = CyFalse;
    gpioConfig.inputEn = CyTrue;
    gpioConfig.intrMode = CY_U3P_GPIO_NO_INTR;

    return CyU3PGpioSetSimpleConfig(gpio, &gpioConfig);
}

CyU3PReturnStatus_t fpgaRegistersInitialise(void)
{
    CyU3PReturnStatus_t status;

    // Chip select idles high, and is configured first so that it is already
    // deasserted before the clock line is driven. The other order would
    // present the slave with clock edges while it believed a transfer was in
    // progress.
    status = configureOutput(FPGA_SPI_CS_GPIO, CyTrue);
    if (status != CY_U3P_SUCCESS) {
        return status;
    }

    // Mode 0, so the clock idles low
    status = configureOutput(FPGA_SPI_SCLK_GPIO, CyFalse);
    if (status != CY_U3P_SUCCESS) {
        return status;
    }

    status = configureOutput(FPGA_SPI_MOSI_GPIO, CyFalse);
    if (status != CY_U3P_SUCCESS) {
        return status;
    }

    status = configureInput(FPGA_SPI_MISO_GPIO);
    if (status != CY_U3P_SUCCESS) {
        return status;
    }

    // Wired and reserved for a future out-of-band signal. Driven rather than
    // left floating, because the FPGA reads it.
    return configureOutput(FPGA_SPI_RESERVED_GPIO, CyFalse);
}

CyU3PReturnStatus_t fpgaRegistersStart(void)
{
    uint32_t status;

    if (glFpgaLockReady) {
        return CY_U3P_SUCCESS;
    }

    status = CyU3PMutexCreate(&glFpgaLock, CYU3P_NO_INHERIT);
    if (status != CY_U3P_SUCCESS) {
        return (CyU3PReturnStatus_t)status;
    }

    glFpgaLockReady = CyTrue;
    return CY_U3P_SUCCESS;
}

// Clock one byte out and one byte in.
//
// SPI mode 0: the clock idles low, the master presents a bit while it is low
// and both ends sample on the rising edge. The slave changes MISO on the
// falling edge, so by the time the master raises the clock the bit it is
// reading has had a full half period to settle.
static uint8_t transferByte(uint8_t send)
{
    uint8_t received = 0u;
    int bit;

    for (bit = 7; bit >= 0; bit--) {
        CyU3PGpioSimpleSetValue(FPGA_SPI_MOSI_GPIO,
            ((send >> bit) & 0x01u) != 0u ? CyTrue : CyFalse);
        CyU3PBusyWait(FPGA_SPI_HALF_BIT_US);

        CyU3PGpioSimpleSetValue(FPGA_SPI_SCLK_GPIO, CyTrue);

        {
            CyBool_t value = CyFalse;
            if (CyU3PGpioSimpleGetValue(FPGA_SPI_MISO_GPIO, &value) == CY_U3P_SUCCESS) {
                if (value == CyTrue) {
                    received |= (uint8_t)(1u << bit);
                }
            }
        }

        CyU3PBusyWait(FPGA_SPI_HALF_BIT_US);
        CyU3PGpioSimpleSetValue(FPGA_SPI_SCLK_GPIO, CyFalse);
    }

    return received;
}

static void selectFpga(void)
{
    CyU3PGpioSimpleSetValue(FPGA_SPI_CS_GPIO, CyFalse);
    CyU3PBusyWait(FPGA_SPI_SELECT_US);
}

static void deselectFpga(void)
{
    CyU3PBusyWait(FPGA_SPI_SELECT_US);
    CyU3PGpioSimpleSetValue(FPGA_SPI_CS_GPIO, CyTrue);
    CyU3PBusyWait(FPGA_SPI_SELECT_US);
}

static CyBool_t takeLock(void)
{
    if (!glFpgaLockReady) {
        return CyFalse;
    }

    return (CyU3PMutexGet(&glFpgaLock, CYU3P_WAIT_FOREVER) == CY_U3P_SUCCESS)
        ? CyTrue : CyFalse;
}

static void releaseLock(void)
{
    if (glFpgaLockReady) {
        CyU3PMutexPut(&glFpgaLock);
    }
}

CyBool_t fpgaRegistersRead(uint8_t address, uint8_t *buffer, uint8_t length)
{
    uint8_t index;

    if (buffer == NULL || length == 0u || address > FPGA_REGISTER_ADDRESS_MAX) {
        return CyFalse;
    }

    if (!takeLock()) {
        return CyFalse;
    }

    selectFpga();

    // Command byte: the read bit, then the address. Nothing meaningful comes
    // back during it — the slave drives MISO low until the address is known —
    // so the returned byte is discarded and the register contents start with
    // the next one.
    (void)transferByte((uint8_t)(0x80u | address));

    for (index = 0u; index < length; index++) {
        buffer[index] = transferByte(0x00u);
    }

    deselectFpga();
    releaseLock();

    return CyTrue;
}

CyBool_t fpgaRegistersWrite(uint8_t address, uint8_t value)
{
    if (address > FPGA_REGISTER_ADDRESS_MAX) {
        return CyFalse;
    }

    if (!takeLock()) {
        return CyFalse;
    }

    selectFpga();
    (void)transferByte(address);            // read bit clear
    (void)transferByte(value);
    deselectFpga();

    releaseLock();

    return CyTrue;
}

CyBool_t fpgaRegistersWriteBurst(uint8_t address, const uint8_t *data,
                                 uint16_t length)
{
    uint16_t index;

    if (data == NULL || length == 0u || address > FPGA_REGISTER_ADDRESS_MAX) {
        return CyFalse;
    }

    if (!takeLock()) {
        return CyFalse;
    }

    selectFpga();
    (void)transferByte(address);            // read bit clear

    for (index = 0u; index < length; index++) {
        (void)transferByte(data[index]);
    }

    deselectFpga();
    releaseLock();

    return CyTrue;
}

CyBool_t fpgaRegistersSetLeds(uint8_t pattern)
{
    if (!glFpgaPresent) {
        return CyFalse;
    }

    return fpgaRegistersWrite(FPGA_REGISTER_LED, pattern);
}

CyBool_t fpgaRegistersPresent(void)
{
    return glFpgaPresent;
}

CyBool_t fpgaRegistersHasFlashBridge(void)
{
    if (!glFpgaPresent) {
        return CyFalse;
    }

    return fpgaIdentityHasFlashBridge(glFpgaIdentity) ? CyTrue : CyFalse;
}

uint8_t fpgaRegistersImageRole(void)
{
    if (!glFpgaPresent) {
        return FPGA_IMAGE_ROLE_APPLICATION;
    }

    return fpgaIdentityImageRole(glFpgaIdentity);
}

void fpgaRegistersForgetProbe(void)
{
    glFpgaPresent = CyFalse;
}

CyBool_t fpgaRegistersProbe(uint8_t attempts)
{
    uint8_t *const identity = glFpgaIdentity;
    char commit[FPGA_COMMIT_LENGTH + 1];
    uint8_t attempt;

    glFpgaPresent = CyFalse;

    for (attempt = 0u; attempt < attempts; attempt++) {
        if (fpgaRegistersRead(FPGA_REGISTER_ID, identity, FPGA_IDENTITY_LENGTH) &&
            fpgaIdentityIsValid(identity)) {
            glFpgaPresent = CyTrue;
            break;
        }

        if ((attempt + 1u) < attempts) {
            CyU3PThreadSleep(FPGA_PROBE_INTERVAL_MS);
        }
    }

    if (!glFpgaPresent) {
        // Only the deliberate search says so. The recheck the application
        // thread makes runs every couple of seconds for as long as no FPGA
        // answers, and a console line each time would bury everything else.
        //
        // Reported at a level that is on by default, because every other
        // symptom of this is silence: the device enumerates, captures and
        // says nothing about why its LEDs are dark and its version is
        // unknown.
        if (attempts > 1u) {
            CyU3PDebugPrint(4, "fpgaRegistersProbe(): no FPGA register bank found after %d attempts; "
                "gateware version and status LEDs are unavailable\r\n", attempts);
        }
        return CyFalse;
    }

    fpgaIdentityCommitText(identity, commit, sizeof(commit));

    CyU3PDebugPrint(4, "fpgaRegistersProbe(): FPGA register map version %d, commit %s%s\r\n",
        fpgaIdentityMapVersion(identity),
        (commit[0] != '\0') ? commit : "unknown",
        fpgaIdentityIsDirty(identity) ? " (dirty)" : "");

    // Worth a line of its own, because it is the difference between a unit
    // that can capture and one that cannot. A device in the factory image
    // answers everything here perfectly well and has no capture path at all.
    if (fpgaIdentityHasFlashBridge(identity) &&
        fpgaIdentityImageRole(identity) == FPGA_IMAGE_ROLE_FACTORY) {
        CyU3PDebugPrint(4, "fpgaRegistersProbe(): the FPGA is running its factory image - "
            "this unit is in gateware recovery and cannot capture until the gateware "
            "is reinstalled\r\n");
    }

    if (fpgaIdentityMapVersion(identity) != FPGA_IDENTITY_MAP_VERSION) {
        CyU3PDebugPrint(4, "fpgaRegistersProbe(): gateware implements register map version %d, "
            "this firmware was built against %d\r\n",
            fpgaIdentityMapVersion(identity), FPGA_IDENTITY_MAP_VERSION);
    }

    return CyTrue;
}
