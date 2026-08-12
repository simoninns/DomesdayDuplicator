/*
 ## Cypress FX3 Core Library Header (cyfx3_api.h)
 ## ===========================
 ##
 ##  Copyright Cypress Semiconductor Corporation, 2010-2023,
 ##  All Rights Reserved
 ##  UNPUBLISHED, LICENSED SOFTWARE.
 ##
 ##  CONFIDENTIAL AND PROPRIETARY INFORMATION
 ##  WHICH IS THE PROPERTY OF CYPRESS.
 ##
 ##  Use of this file is governed
 ##  by the license agreement included in the file
 ##
 ##     <install>/license/license.txt
 ##
 ##  where <install> is the Cypress software
 ##  installation root directory path.
 ##
 ## ===========================
 */

#ifndef _INCLUDED_CYFX3_API_H_
#define _INCLUDED_CYFX3_API_H_

/** \file cyfx3_api.h
    \brief Defines the APIs provided by the FX3 core library.

    <B>Description</B>\n
    The FX3 Core library provides a minimal set of API which operate at the device level, and
    are restricted for IP protection. This header file defines the set of APIs provided by the
    core library.
 */

#include <cyu3types.h>
#include "cyu3externcstart.h"

/**********************************************************************************************
 *                                     Type definitions                                       *
 **********************************************************************************************/

/** \brief Enumeration of EZ-USB FX3 part numbers.

    <B>Description</B>\n
    There are multiple EZ-USB FX3 parts which support varying feature sets. Please refer
    to the device data sheets or the Cypress device catalogue for information on the
    features supported by each FX3 part.
 
    This enumerated type lists the various valid part numbers in the EZ-USB FX3 family.

    **\see
    *\see CyU3PDeviceGetPartNumber
 */
typedef enum CyU3PPartNumber_t
{
    CYPART_USB3014 = 0, /**< CYUSB3014: 512 KB RAM; GPIF can be 32 bit; OTG and USB host supported */
    CYPART_USB3012,     /**< CYUSB3012: 256 KB RAM; GPIF can be 32 bit */
    CYPART_USB3013,     /**< CYUSB3013: 512 KB RAM; GPIF - 16 bit bus only */
    CYPART_USB3011,     /**< CYUSB3011: 256 KB RAM; GPIF - 16 bit bus only */
    CYPART_USB3035,     /**< CYUSB3035: 512 KB RAM; GPIF - 16 bit bus; Supports two SD/eMMC/SDIO ports */
    CYPART_USB3034,     /**< CYUSB3034: 512 KB RAM; GPIF - 16 bit bus; Supports two SD/eMMC/SDIO ports */
    CYPART_USB3033,     /**< CYUSB3033: 512 KB RAM; GPIF - 16 bit bus; Supports single SD/eMMC/SDIO port */
    CYPART_USB3032,     /**< CYUSB3032: 256 KB RAM; GPIF - 16 bit bus; No USB host/OTG; Two storage ports */
    CYPART_USB3031,     /**< CYUSB3031: 256 KB RAM; GPIF - 16 bit bus; No USB host/OTG; Single storage port */
    CYPART_WB0263,      /**< CYWB0263 (Benicia): 512 KB RAM; USB 3.0; GPIF - 16 bit; Supports two SD/eMMC/SDIO ports */
    CYPART_WB0163,      /**< CYWB0163 (Bay): 512 KB RAM; USB 2.0 only; GPIF - 16 bit; Supports two SD/eMMC/SDIO ports */
    CYPART_USB2035,     /**< CYUSB2035: 512 KB RAM; USB 2.0 only; GPIF - 16 bit; Supports two SD/eMMC/SDIO ports */
    CYPART_USB2034,     /**< CYUSB2034: 512 KB RAM; USB 2.0 only; GPIF - 16 bit; Supports two SD/eMMC/SDIO ports */
    CYPART_USB2033,     /**< CYUSB2033: 512 KB RAM; USB 2.0 only; GPIF - 16 bit; Supports single SD/eMMC/SDIO port */
    CYPART_USB2032,     /**< CYUSB2032: 256 KB RAM; USB 2.0 only; GPIF - 16 bit; No USB host/OTG; Two storage ports */
    CYPART_USB2031,     /**< CYUSB2031: 256 KB RAM; USB 2.0 only; GPIF - 16 bit; No USB host/OTG; Single storage port */
    CYPART_USB3025,     /**< CYUSB3025: 512 KB RAM; No GPIF port; USB host/OTG; Two storage ports */
    CYPART_USB3024,     /**< CYUSB3024: 512 KB RAM; No GPIF port; USB host/OTG; Two storage ports */
    CYPART_USB3023,     /**< CYUSB3023: 512 KB RAM; No GPIF port; No USB host/OTG; Single storage port */
    CYPART_USB3021,     /**< CYUSB3021: 256 KB RAM; No GPIF port; No USB host/OTG; Single storage port */
    CYPART_USB2025,     /**< CYUSB2025: 512 KB RAM; USB 2.0 only; No GPIF port; USB host/OTG; Two storage ports */
    CYPART_USB2024,     /**< CYUSB2024: 512 KB RAM; USB 2.0 only; No GPIF port; USB host/OTG; Two storage ports */
    CYPART_USB2023,     /**< CYUSB2023: 512 KB RAM; USB 2.0 only; No GPIF port; No USB host/OTG; Single storage port */
    CYPART_USB3061,     /**< CYUSB3061: 256 KB RAM; Fixed Function GPIF; 1 MIPI-CSI lane */
    CYPART_USB3062,     /**< CYUSB3062: 512 KB RAM; Fixed Function GPIF; 1 MIPI-CSI lane */
    CYPART_USB3063,     /**< CYUSB3063: 256 KB RAM; Fixed Function GPIF; 2 MIPI-CSI lanes */
    CYPART_USB3064,     /**< CYUSB3064: 512 KB RAM; Fixed Function GPIF; 2 MIPI-CSI lanes */
    CYPART_USB3065,     /**< CYUSB3065: 512 KB RAM; Fixed Function GPIF; 4 MIPI-CSI lanes */
    CYPART_USB2064,     /**< CYUSB2064: 512 KB RAM; USB 2.0 only; Fixed Function GPIF; 2 MIPI-CSI lanes */
    CYPART_USB2014,     /**< CYUSB2014: 512 KB RAM; USB 2.0 only; GPIF can be 32 bit; OTG and USB host supported */
    CYPART_USB2013,     /**< CYUSB2013: 512 KB RAM; USB 2.0 only; GPIF - 16 bit; OTG and USB host supported */
    CYPART_USB2011,     /**< CYUSB2011: 256 KB RAM; USB 2.0 only; GPIF - 16 bit; No USB host/OTG */
    CYPART_USB3075,     /**< CYUSB3075: 512 KB RAM; 4 MIPI-DSI lanes. */
    CYPART_LASTDEV      /**< Unknown device type */
} CyU3PPartNumber_t;

/** \brief Defines the enumerations for LPP IO line configurations.

    <B>Description</B>\n
    Most of the IO pins on the FX3 device are multi-purpose with the specific
    configuration being selected at system start-up. This enumeration lists the
    various IO operating modes relating to Serial peripheral interfaces.

    The I2C interface is always available. If the GPIF data bus is configured as
    32-bit wide, only the UART and I2S interfaces are available (SPI is not available).
    In this case, the configuration chosen should be CY_U3P_IO_MATRIX_LPP_DEFAULT.

    If the GPIF data bus is 8, 16 or 24 bits wide; it is possible to use all of
    the SPI, UART and I2S interfaces. However, the peripheral pins can be relocated
    in this case. Refer to the FX3 datasheet for more details and choose the desired
    configuration accordingly.

    In the case of the FX3S device, none of the UART, SPI and I2S interfaces are
    available if the second storage port (S1) is used in 8-bit mode. In this case,
    the configuration chosen should CY_U3P_IO_MATRIX_LPP_NONE.

    **\see
    *\see CyU3PDeviceConfigureIOMatrix
    *\see CyU3PSPortMode_t
 */
typedef enum CyU3PIoMatrixLppMode_t
{
    CY_U3P_IO_MATRIX_LPP_DEFAULT = 0,   /**< Default LPP mode where all peripherals are enabled. */
    CY_U3P_IO_MATRIX_LPP_UART_ONLY,     /**< LPP layout with GPIF 16-bit and UART only. */
    CY_U3P_IO_MATRIX_LPP_SPI_ONLY,      /**< LPP layout with GPIF 16-bit and SPI only. */
    CY_U3P_IO_MATRIX_LPP_I2S_ONLY,      /**< LPP layout with GPIF 16-bit and I2S only. */
    CY_U3P_IO_MATRIX_LPP_NONE           /**< FX3S specific configuration where UART, SPI and I2S are disabled. */
} CyU3PIoMatrixLppMode_t;

/** \brief Define the various operating modes for the storage ports on the FX3S device.

    <B>Description</B>\n
    The FX3S device supports two storage ports which can be connected to SD/eMMC/SDIO
    peripherals. Each of these ports can be configured in a variety of modes. This enumeration
    lists the possible IO configurations for each of these storage ports.

    The selected storage port IO configuration has some implications on the selection
    of other IO configurations like lppMode. Please see the FX3S device datasheet to identify
    the supported combinations.

    **\see
    *\see CyU3PDeviceConfigureIOMatrix
    *\see CyU3PIoMatrixLppMode_t
 */
typedef enum CyU3PSPortMode_t
{
    CY_U3P_SPORT_INACTIVE = 0,          /**< Storage port not in use. Pins are available for other interfaces. */
    CY_U3P_SPORT_4BIT,                  /**< SD/MMC interface with four bit data bus. Upper half of data bus is
                                             available for other interfaces. */
    CY_U3P_SPORT_8BIT,                  /**< SD/MMC interface with eight bit data bus. All pins (CLK, CMD, D7-D0)
                                             are part of storage interface. */
    CY_U3P_SPORT_1BIT                   /**< SD/MMC interface with a single bit data bus. D1-D3 pins can be
                                             used as GPIO. */
} CyU3PSPortMode_t;

/** \brief IO matrix configuration parameters.

    <B>Description</B>\n
    The EZ-USB FX3 and FX3S devices have a flexible IO architecture that allows each IO Pin to serve
    multiple functions. The desired IO configuration for all of the pins needs to be specified before
    any of the FX3 internal blocks such as USB, GPIF or UART are powered on.

    This structure captures the desired IO configuration for the FX3/FX3S device as a whole.

    <B>Note</B>\n
    A common structure including the storage port configuration is used for both FX3 and FX3S
    devices, in order to maintain a common API interface. The s0Mode and s1Mode fields should
    be set to CY_U3P_SPORT_INACTIVE when using the EZ-USB FX3 device.

    **\see
    *\see CyU3PIoMatrixLppMode_t
    *\see CyU3PSPortMode_t
    *\see CyU3PDeviceConfigureIOMatrix
 */
typedef struct CyU3PIoMatrixConfig_t
{
    CyBool_t               isDQ32Bit; /**< Whether the GPIF data bus is 32 bits wide. */
    CyBool_t               useUart;   /**< Whether pins are to be reserved for the UART interface. */
    CyBool_t               useI2C;    /**< Whether pins are to be reserved for the I2C interface. */
    CyBool_t               useI2S;    /**< Whether pins are to be reserved for the I2S interface. */
    CyBool_t               useSpi;    /**< Whether pins are to be reserved for the SPI interface. */
    CyU3PSPortMode_t       s0Mode;    /**< Interface mode for the S0 storage port (where available). */
    CyU3PSPortMode_t       s1Mode;    /**< Interface mode for the S1 storage port (where available). */
    CyU3PIoMatrixLppMode_t lppMode;   /**< LPP IO configuration to be used. */
    uint32_t gpioSimpleEn[2];         /**< Bitmap that identifies pins that should be configured as simple GPIOs. */
    uint32_t gpioComplexEn[2];        /**< Bitmap that identifies pins that should be configured as complex GPIOs. */
} CyU3PIoMatrixConfig_t;

/**********************************************************************************************
 *                                    Function Prototypes                                     *
 **********************************************************************************************/

/** \brief Store the CPU clock frequency for use in BusyWait delay calculation.

    <B>Description</B>\n
    The CyFx3BusyWait delay function uses CPU instructions to obtain the delay required by the
    user. The actual delay obtained will depend on the CPU clock frequency. This function stores
    the CPU clock frequency, so that the BusyWait implementation can be calibrated.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyFx3BusyWait
 */
extern void
CyFx3SetCpuFreq (
        uint32_t cpuFreq                /** CPU clock frequency in Hertz. */
        );

/** \brief Delay function.

    <B>Description</B>\n
    This function provides delays in multiples of 1 us. The ARM CPU is kept busy by this
    function, and this should not be used in cases where large delays are required.

    <B>Return Value</B>\n
    * None

    **\see
    *\see CyFx3SetCpuFreq
 */
extern void
CyFx3BusyWait (
        uint16_t usWait                 /**< Delay required in micro-seconds. */
        );

/** \brief Setup the default page tables for the FX3 device.

    <B>Description</B>\n
    The MMU on the FX3/FX3S device needs to be turned on for the data cache to work.
    This function sets up a default set of page tables that map each valid physical address
    to the corresponding virtual address, so that the device can operate with the MMU
    turned on.

    <B>Return Value</B>\n
    * None
 */
extern void
CyFx3DevInitPageTables (
        void);

/** \brief Clear a FX3 device specific software interrupt.

    <B>Description</B>\n
    This function clears a device specific software interrupt. The software interrupt
    mechanism is not used in the SDK.

    <B>Return Value</B>\n
    * None
 */
extern void
CyFx3DevClearSwInterrupt (
        void);

/** \brief Get the current FX3/FX3S device part number.

    <B>Description</B>\n
    This function gets the current FX3/FX3S part number by querying the device identification
    registers. This function can be used by the code to avoid calling functions that would
    cause failures because the Silicon does not support them.

    <B>Return Value</B>\n
    * Device part number
 */
extern CyU3PPartNumber_t
CyFx3DevIdentifyPart (
        void);

/** \brief Check if the part being used supports the USB 3.0 peripheral functionality.

    <B>Description</B>\n
    Some low cost devices in the FX3 family (such as SD2 or FX2G2) do not support the USB 3.0
    peripheral functionality, while retaining all of the other capabilities such as the ARM9
    core, the high-performance distributed DMA architecture, the GPIF II port and the peripheral
    support. This function checks whether the active part supports the USB 3.0 function.

    <B>Return Value</B>\n
    * CyTrue if USB 3.0 is supported.
    * CyFalse if USB 3.0 is not supported.
 */
extern CyBool_t
CyFx3DevIsUsb3Supported (
        void);

/** \brief Check if the part being used supports the USB OTG and USB 2.0 host functionality.

    <B>Description</B>\n
    Some devices in the FX3/FX3S product family do not support the USB 2.0 host and OTG
    functionality. This function is used to check whether the part being used supports the
    host/OTG functionality.

    <B>Return Value</B>\n
    * CyTrue if USB 2.0 host and OTG is supported.
    * CyFalse if USB 2.0 host and OTG is not supported.
 */
extern CyBool_t
CyFx3DevIsOtgSupported (
        void);

/** \brief Check if the part being used supports 512 KB of System RAM.

    <B>Description</B>\n
    Some devices in the FX3/FX3S product family support only 256 KB of System RAM, and the
    firmware needs to be constrained to work with the available memory. This function is used
    to check whether the part being used supports 512 KB of System RAM.

    <B>Return Value</B>\n
    * CyTrue if the part supports 512 KB of RAM.
    * CyFalse if the part supports only 256 KB of RAM.
 */
extern CyBool_t
CyFx3DevIsRam512Supported (
        void);

/** \brief Check if the part supports the PIB/GPIF II interface.

    <B>Description</B>\n
    The FX3 SDK supports parts such as the SD3 and SD2 devices which do not provide a GPIF II
    interface. This function is used to check whether the part in use can support GPIF II
    functionality.

    <B>Return Value</B>\n
    * CyTrue if GPIF II (PIB) is supported.
    * CyFalse if GPIF II is not supported.
 */
extern CyBool_t
CyFx3DevIsGpifSupported (
        void);

/** \brief Check if the part supports a 32 bit wide GPIF II data bus.

    <B>Description</B>\n
    Some FX3 parts as well as the FX3S devices can only make use of the GPIF II interface with
    a 16 bit or narrower data bus. This function checks whether the part in use can support a
    24 bit or 32 bit wide GPIF II data bus.

    <B>Return Value</B>\n
    * CyTrue if GPIF II data bus can be 32 bits wide.
    * CyFalse if GPIF II data bus can only be 16 bits or smaller.
 */
extern CyBool_t
CyFx3DevIsGpif32Supported (
        void);

/** \brief Check if the part supports the S0 storage port.

    <B>Description</B>\n
    Some parts in the FX3 family (FX3S, SD3 and SD2) support a storage port through
    which SD cards, eMMC devices or SDIO cards can be connected. This function checks
    whether the part in use supports the S0 storage port.

    <B>Return Value</B>\n
    * CyTrue if the S0 storage port is supported.
    * CyFalse if the S0 storage port is not supported.
 */
extern CyBool_t
CyFx3DevIsSib0Supported (
        void);

/** \brief Check if the part supports the S1 storage port.

    <B>Description</B>\n
    Some parts in the FX3 family (FX3S, SD3 and SD2) support two storage ports through
    which SD cards, eMMC devices or SDIO cards can be connected. This function checks
    whether the part in use supports the S1 (second) storage port.

    <B>Return Value</B>\n
    * CyTrue if the S1 storage port is supported.
    * CyFalse if the S1 storage port is not supported.
 */
extern CyBool_t
CyFx3DevIsSib1Supported (
        void);

/** \brief Check if the part supports the I2S interface.

    <B>Description</B>\n
    Some parts in the FX3 family do not support the I2S interface. This function checks
    whether I2S is supported by the part in use.

    <B>Return Value</B>\n
    * CyTrue if I2S interface is supported.
    * CyFalse if I2S interface is not supported.
 */
extern CyBool_t
CyFx3DevIsI2sSupported (
        void);

/** \brief Check if the part supports the MIPI CSI interface.

    <B>Description</B>\n
    Some parts in the FX3 family (CX3) support a MIPI CSI interface through which an image
    sensor can be connected. This function checks whether the part in use supports the CSI
    interface.

    <B>Return Value</B>\n
    * CyTrue if the CSI interface is supported.
    * CyFalse if the CSI interface is not supported.
 */
extern CyBool_t
CyFx3DevIsMipicsiSupported (
        void);

/** \brief Get the number of CSI lanes supported by the CX3 part in use.

    <B>Description</B>\n
    Different CX3 parts can support different number of CSI lanes through image sensors
    can be connected. This API returns the number of CSI lanes that are supported by the
    CX3 part in use..

    <B>Return Value</B>\n
    * Number of CSI lanes supported.
 */
extern uint8_t
CyFx3DevGetMipiLaneCount (
        void);

/** \brief Check if I2C is enabled in the current IO configuration.

    <B>Description</B>\n
    This function checks whether the I2C interface is enabled in the current IO
    configuration of the FX3 device.

    <B>Return Value</B>\n
    * CyTrue if I2C is enabled.
    * CyFalse if I2C is disabled.
 */
extern CyBool_t
CyFx3DevIOIsI2cConfigured (
        void);

/** \brief Check if UART is enabled in the current IO configuration.

    <B>Description</B>\n
    This function checks whether the UART interface is enabled in the current IO
    configuration of the FX3 device.

    <B>Return Value</B>\n
    * CyTrue if UART is enabled.
    * CyFalse if UART is disabled.
 */
extern CyBool_t
CyFx3DevIOIsUartConfigured (
        void);

/** \brief Check if I2S is enabled in the current IO configuration.

    <B>Description</B>\n
    This function checks whether the I2S interface is enabled in the current IO
    configuration of the FX3 device.

    <B>Return Value</B>\n
    * CyTrue if I2S is enabled.
    * CyFalse if I2S is disabled.
 */
extern CyBool_t
CyFx3DevIOIsI2sConfigured (
        void);

/** \brief Check if SPI is enabled in the current IO configuration.

    <B>Description</B>\n
    This function checks whether the SPI interface is enabled in the current IO
    configuration of the FX3 device.

    <B>Return Value</B>\n
    * CyTrue if SPI is enabled.
    * CyFalse if SPI is disabled.
 */
extern CyBool_t
CyFx3DevIOIsSpiConfigured (
        void);

/** \brief Check if the SIB0 port is configured.

    <B>Description</B>\n
    Check whether the SIB0 port is enabled in the current IO configuration of the
    FX3S/SD3 device.

    <B>Return Value</B>\n
    * CyTrue if the SIB0 port is enabled in the IO configuration.
    * CyFalse if the SIB0 port is not enabled in the IO configuration.
 */
extern CyBool_t
CyFx3DevIOIsSib0Configured (
        void);

/** \brief Check if the SIB1 port is configured.

    <B>Description</B>\n
    Check whether the SIB1 port is enabled in the current IO configuration of the
    FX3S/SD3 device.

    <B>Return Value</B>\n
    * CyTrue if the SIB1 port is enabled in the IO configuration.
    * CyFalse if the SIB1 port is not enabled in the IO configuration.
 */
extern CyBool_t
CyFx3DevIOIsSib1Configured (
        void);


/** \brief Disable SIB0 port.

    <B>Description</B>\n
    Disable SIB0 port for the current IO configuration of the
    FX3S/SD3 device.
 */
extern void
CyFx3DevIODisableSib0 (
        void);



/** \brief Disable SIB1 port.

    <B>Description</B>\n
    Disable SIB1 port for the current IO configuration of the
    FX3S/SD3 device.
 */
extern void
CyFx3DevIODisableSib1 (
        void);

/** \brief Check if the specified SIB port is configured for 8 bit operation.

    <B>Description</B>\n
    The SIB ports on the FX3S/SD3 device can be configured for 1 bit, 4 bit or 8 bit
    data operation. This function checks whether the current IO configuration of the
    specified storage port allows 8 bit operation.

    <B>Return Value</B>\n
    * CyTrue if 8 bit operation is allowed.
    * CyFalse if 8 bit operation is not allowed.
 */


extern CyBool_t
CyFx3DevIOIsSib8BitWide (
        uint8_t portId                  /**< Port being queried (0 or 1). */
        );

/** \brief Check if the specified SIB port is restricted to 1 bit operation.

    <B>Description</B>\n
    The SIB ports on the FX3S/SD3 device can be configured for 1 bit, 4 bit or 8 bit
    data operation. This function checks whether the current IO configuration of the
    specified storage port is restricted to 1 bit.

    <B>Return Value</B>\n
    * CyTrue if only 1 bit operation is allowed.
    * CyFalse if data bus can be 4 or 8 bits wide.
 */
extern CyBool_t
CyFx3DevIOIsSib1BitWide (
        uint8_t portId                  /**< Port being queried (0 or 1). */
        );

/** \brief Check if the specified GPIO is configured as a GPIO.

    <B>Description</B>\n
    This function checks whether the specified FX3/FX3S GPIO pin has been configured
    as a GPIO of the specified type (simple or complex).

    <B>Return Value</B>\n
    * CyTrue if the pin is currently configured as a GPIO of the specified type.
    * CyFalse if the pin is invalid or if it is not configured as a GPIO.
 */
extern CyBool_t
CyFx3DevIOIsGpio (
        uint32_t gpioId,                /**< GPIO whose state is to be queried. */
        CyBool_t isSimple               /**< Whether to check for a simple GPIO (CyTrue) or a complex GPIO (CyFalse). */
        );

/** \brief Control the override of a IO pin as a simple or complex GPIO.

    <B>Description</B>\n
    IO pins on the FX3/FX3S interface can be temporarily over-ridden as simple or
    complex GPIOs during device operation. This API is used to place or remove
    these GPIO override modes.

    <B>Return Value</B>\n
    * None
 */
extern void
CyFx3DevIOSelectGpio (
        uint8_t  gpioId,                /**< GPIO to be modified. */
        CyBool_t enable,                /**< Whether the override is to be enabled (CyTrue) or removed (CyFalse). */
        CyBool_t isSimple               /**< Whether over-riding as a simple (CyTrue) or a complex (CyFalse) GPIO.
                                             Is don't care when enable is CyFalse. */
        );

/** \brief Configure the IO matrix on the device.
 
    <B>Description</B>\n
    The FX3 and other devices in the family have a configurable IO matrix which allows
    multiplexing of multiple functions onto a IO pin at different times. This function
    validates the IO configuration specified by the user, and applies it if valid.

    <B>Return Value</B>\n
    * CyTrue if the IO configuration is valid for the device and has been applied.
    * CyFalse if the IO configuration is illegal or inconsistent.
 */
extern CyBool_t
CyFx3DevIOConfigure (
        CyU3PIoMatrixConfig_t *cfg_p    /**< IO configuration parameters. */
        );

/** \brief Check whether the USB block is powered ON.

    <B>Description</B>\n
    Function to check whether the USB block on FX3 has been turned on.

    <B>Return value</B>\n
    * CyTrue if the USB block is powered ON.\n
    * CyFalse if the USB block is not powered ON.
 */
extern CyBool_t
CyFx3UsbIsOn (
        void);

/** \brief Power on the USB block on the FX3/FX3S device.

    <B>Description</B>\n
    A device specific sequence needs to be followed when powering the USB block on the
    FX3 device on. This function implements the sequence.

    <B>Return value</B>\n
    * None
 */
extern void
CyFx3UsbPowerOn (
        void);

/** \brief Write to a USB 3.0 PHY Control Register.

    <B>Description</B>\n
    The USB 3.0 PHY on the FX3 device has a few control registers that may need to be
    updated at various stages of device operation. This function writes a user specified
    value into one of the PHY control registers.

    <B>Return Value</B>\n
    * None
 */
extern void
CyFx3UsbWritePhyReg (
        uint16_t phyAddr,               /**< Address of the PHY register. */
        uint16_t phyVal                 /**< Value to be written. */
        );

/** \brief Configure the USB 2.0 PHY on FX3.

    <B>Description</B>\n
    This function sets up USB 2.0 PHY on the FX3 for operation. The settings made by this
    API are required for passing USB 2.0 electrical tests.

    <B>Return Value</B>\n
    * None
 */
extern void
CyFx3Usb2PhySetup (
        void);

/** \brief Configure the USB 3.0 LINK on FX3.

    <B>Description</B>\n
    This function sets up the USB 3.0 LINK on the FX3 for operation. The settings made by this
    API are required for passing USB 3.0 link layer tests.

    <B>Return Value</B>\n
    * None
 */
extern void
CyFx3Usb3LnkSetup (
        void);

/** \brief Send a USB 3.0 Transaction Packet.

    <B>Description</B>\n
    This function sends a USB 3.0 Transaction Packet (TP) with user specified data to the
    USB host.

    <B>Return Value</B>\n
    * None
 */
extern void
CyFx3Usb3SendTP (
        uint32_t *tpData                /**< Pointer to the TP data (3 dwords) to be sent. */
        );

/** \brief Enable data prefetch from the DMA sockets on the USB egress (IN) data path.

    <B>Description</B>\n
    This function enables prefetching of data from the DMA sockets on the USB egress (or IN endpoint)
    data path, so as to obtain better transfer performance. This setting is always enabled during
    USB 2.0 operation, but should be used with caution on a USB 3.0 connection.

    <B>Return Value</B>\n
    * None
 */
extern void
CyFx3UsbDmaPrefetchEnable (
        CyBool_t streamEnable           /**< Enable stream mode of data prefetch. Can cause endpoint
                                             freeze if used with endpoints that send a lot of short
                                             packets. */
        );

/** \brief Relax the USB 3.0 HP ACK Timeout period.

    <B>Description</B>\n
    In some cases, FX3 does not receive the ACK for a USB 3.0 link header in time, and gets into
    the SS.Inactive state. This API relaxes the timeout for the ACK so that this kind of error is
    prevented.

    <B>Return Value</B>\n
    * None
 */
extern void
CyFx3Usb3LnkRelaxHpTimeout (
        void);

/** \brief Check whether the PIB block is powered ON.

    <B>Description</B>\n
    This function checks whether the PIB block on FX3 has been turned ON.

    <B>Return value</B>\n
    * CyTrue if the PIB block has been turned ON.\n
    * CyFalse if the PIB block has not been turned ON.
 */
extern CyBool_t
CyFx3PibIsOn (
        void);

/** \brief Power on the PIB and GPIF II blocks on the FX3/FX3S device.

    <B>Description</B>\n
    A device specific sequence needs to be followed when powering the PIB and GPIF II
    blocks on the FX3 device on. This function implements the sequence.

    Note: The PIB clock should have been enabled before calling this function.

    <B>Return value</B>\n
    * None
 */
extern void
CyFx3PibPowerOn (
        void);

/** \brief Power off the PIB and GPIF II blocks on the FX3/FX3S device.

    <B>Description</B>\n
    A device specific sequence needs to be followed when powering the PIB and GPIF II
    blocks on the FX3 device off. This function implements the sequence.

    Note: The PIB clock should only be disabled after this function returns.

    <B>Return value</B>\n
    * None
 */
extern void
CyFx3PibPowerOff (
        void);

/** \brief Enable the DLL in the PIB block.

    <B>Description</B>\n
    The PIB block on FX3 has an embedded DLL that can be used when FX3 is acting as
    a slave device with an incoming clock signal. This function enables the DLL in the
    PIB block.

    <B>Return Value</B>\n
    * None
 */
extern void
CyFx3PibDllEnable (
        void);

/** \brief Get the current status of the PIB DLL.

    <B>Description</B>\n
    Get the locked/unlocked status of the PIB DLL.

    <B>Return Value</B>\n
    * None
 */
extern uint16_t
CyFx3PibGetDllStatus (
        void);

/** \brief Power the storage interface block on the FX3S/SD3 device on.

    <B>Description</B>\n
    This function powers the storage interface block (SIB) on the FX3S/SD3 device on.
    A common power-up sequence is used for all available storage ports. Clocks for the
    available storage ports need to be enabled before calling this function.

    <B>Return Value</B>\n
    * None
 */
extern void
CyFx3SibPowerOn (
        void);

/** \brief Power the storage interface block on the FX3S/SD3 device off.

    <B>Description</B>\n
    This function powers the storage interface block (SIB) on the FX3S/SD3 device off.
    The SIB clock(s) can be turned off after calling this function.

    <B>Return Value</B>\n
    * None
 */
extern void
CyFx3SibPowerOff (
        void);

/** \brief Check if the LPP block is powered ON.

    <B>Description</B>\n
    This function checks whether the LPP block (common hardware for all serial peripherals) is turned ON.

    <B>Return value</B>\n
    * CyTrue if the LPP block is turned ON.\n
    * CyFalse if the LPP block is turned OFF.
 */
extern CyBool_t
CyFx3LppIsOn (
        void);

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYFX3_API_H_ */

/*[]*/

