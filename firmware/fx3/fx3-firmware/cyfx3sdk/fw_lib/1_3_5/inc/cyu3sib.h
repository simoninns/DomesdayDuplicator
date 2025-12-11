/*
 ## Cypress FX3 Core Library Header (cyu3sib.h)
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

#ifndef _INCLUDED_CYU3SIB_H_
#define _INCLUDED_CYU3SIB_H_

#include <cyu3os.h>
#include <cyu3dma.h>
#include <cyu3types.h>

#include "cyu3externcstart.h"

/** \file cyu3sib.h
    \brief This file defines the data types and APIs that support SD/MMC/SDIO
    peripheral access on the EZ-USB FX3S device.

    <B>Description</B>\n
    The EZ-USB FX3S device is an extension to the FX3 device, which has the capability
    to talk to SD/MMC/SDIO peripherals.

    The storage module in FX3S firmware manages accesses to SD, MMC and SDIO devices.
    The storage module is composed of a storage driver and convenience API; and is
    compiled into a separate static library. The storage library (cyu3sport.a) only
    needs to be linked in by FX3S applications that make use of the SD/MMC/SDIO
    interfaces.

    <B>Note</B>\n
    The storage driver internally makes use of GPIO functions. Therefore, applications
    linking with the storage library will also need to link with the serial peripheral
    library (cyu3lpp.a).
 */

/************************************************************************************/
/*********************************** Data Types *************************************/
/************************************************************************************/

/** \def CY_U3P_SIB0_DETECT_GPIO
    \brief GPIO used for card detection on Storage port 0.
 */
#define CY_U3P_SIB0_DETECT_GPIO         (44)

/** \def CY_U3P_SIB0_WRPROT_GPIO
    \brief GPIO used for write protection detection on Storage port 0.
 */
#define CY_U3P_SIB0_WRPROT_GPIO         (43)

/** \def CY_U3P_SIB1_WRPROT_GPIO
    \brief GPIO used for write protection detection on Storage port 1.
 */
#define CY_U3P_SIB1_WRPROT_GPIO         (52)

/** \brief List of storage ports

    <B>Description</B>\n
    The FX3S device has two storage ports which support SD/MMC/SDIO cards to be
    attached. These storage ports can be accessed independently of each other.
 */
typedef enum CyU3PSibPortId
{
    CY_U3P_SIB_PORT_0 = 0,              /**< Storage Port 0 */
    CY_U3P_SIB_PORT_1,                  /**< Storage Port 1 */
    CY_U3P_SIB_NUM_PORTS                /**< Maximum number of storage ports supported */
} CyU3PSibPortId;

/** \brief List of card detection methods.

    <B>Description</B>\n
    The card insertion and removal detection can be based on either GPIO card detect or
    based on signal changes on the DAT3 line.
 */
typedef enum CyU3PSibCardDetect
{
    CY_U3P_SIB_DETECT_NONE = 0,         /**< Don't use any card detection mechanism */
    CY_U3P_SIB_DETECT_GPIO,             /**< Use a GPIO connected to a micro-switch on the socket. This is
                                             only supported for the S0 storage port. */
    CY_U3P_SIB_DETECT_DAT_3             /**< Use voltage changes on the DAT[3] pin for card detection. */
} CyU3PSibCardDetect;

/** \brief List the Storage Device types supported by FX3S.

    <B>Description</B>\n
    This enumeration lists the various storage device types supported by the
    storage module in FX3S firmware.
*/
typedef enum CyU3PSibDevType
{
    CY_U3P_SIB_DEV_NONE = 0,            /**< No device */
    CY_U3P_SIB_DEV_MMC,                 /**< MMC/eMMC device */
    CY_U3P_SIB_DEV_SD,                  /**< SD Memory card */
    CY_U3P_SIB_DEV_SDIO,                /**< SDIO IO only Device.*/
    CY_U3P_SIB_DEV_SDIO_COMBO           /**< SDIO Combo Device. Data transfers to the combo card are not supported. */
} CyU3PSibDevType;

/** \brief List of SD/MMC operating voltage ranges.

    <B>Description</B>\n
    The storage host driver supports storage functionality at both the normal voltage (3.3V)
    and low voltage (1.8V). Low voltage is necessary for the use of the SDR50 signaling rate
    on the SD 3.0 interface.
*/
typedef enum CyU3PSibIntfVoltage
{
    CY_U3P_SIB_VOLTAGE_NORMAL = 0,      /**< Normal voltage, 3.3 V. */
    CY_U3P_SIB_VOLTAGE_LOW              /**< Low voltage, 1.8 V. */
} CyU3PSibIntfVoltage;

/** \brief List of storage related events.

    <B>Description</B>\n
    The storage driver propagates events of interest such as card insertion/removal
    and data transfer completion through a callback function.
 */
typedef enum CyU3PSibEventType
{
    CY_U3P_SIB_EVENT_INSERT = 0,        /**< Card Insert Event */
    CY_U3P_SIB_EVENT_REMOVE,            /**< Card Remove Event */
    CY_U3P_SIB_EVENT_XFER_CPLT,         /**< Data transfer completion. The status field will indicate
                                             success/failure. */
    CY_U3P_SIB_EVENT_SDIO_INTR,         /**< SDIO interrupt event. */
    CY_U3P_SIB_EVENT_RELEASE,           /**< Notification that device is free for access: Not supported. */
    CY_U3P_SIB_EVENT_DATA_ERROR,        /**< Errors like CRC16, Data Timeout and Data Error handler event. */
    CY_U3P_SIB_EVENT_ABORT              /**< Transaction aborted event. */
} CyU3PSibEventType;

/** \brief List of logical unit (LUN) types.

    <B>Description</B>\n
    The storage devices can be partitioned to store boot images as well as for
    storing user content. The boot partitions are only accessible under boot or
    boot update modes.
*/
typedef enum CyU3PSibLunType
{
    CY_U3P_SIB_LUN_RSVD = 0xAB,         /**< LUN type reserved for future use. */
    CY_U3P_SIB_LUN_BOOT = 0xBB,         /**< LUN holding FX3S Boot code */
    CY_U3P_SIB_LUN_DATA = 0xDA          /**< LUN holding user data */
} CyU3PSibLunType;

/** \brief SD register types

    <B>Description</B>\n
    List of SD registers that can be read.
 */
typedef enum CyU3PSibDevRegType
{
    CY_U3P_SD_REG_OCR = 0,              /**< OCR register. */
    CY_U3P_SD_REG_CID,                  /**< CID register. */
    CY_U3P_SD_REG_CSD                   /**< CSD register. */
} CyU3PSibDevRegType;

/** \brief List of logical unit locations.

    <B>Description</B>\n
    MMC devices can support one or more boot partitions in addition to the user data area, and a
    given storage LUN may be located in the boot partition or in the user data area. This enumerated
    type lists the possible location of a logical unit.
 */
typedef enum CyU3PSibLunLocation
{
    CY_U3P_SIB_LOCATION_USER = 0,       /**< The LUN is located in the user data area. */
    CY_U3P_SIB_LOCATION_BOOT1,          /**< The LUN is located in the first boot partition. */
    CY_U3P_SIB_LOCATION_BOOT2           /**< The LUN is located in the second boot partition. */
} CyU3PSibLunLocation;

/** \brief List of storage operating frequencies.

    <B>Description</B>\n
    This enumerated type lists the various operating frequencies supported on the storage port by the
    FX3S device. This type is used to set a constraint on the maximum operating frequency for the
    storage devices.
 */
typedef enum CyU3PSibOpFreq
{
    CY_U3P_SIB_FREQ_400KHZ,             /**< 400 KHz. Used for device initialization only. Cannot be set as the
                                             maximum frequency. */
    CY_U3P_SIB_FREQ_20MHZ,              /**< 20 MHz SDR. */
    CY_U3P_SIB_FREQ_26MHZ,              /**< 26 MHz SDR. */
    CY_U3P_SIB_FREQ_52MHZ,              /**< 52 MHz SDR. */
    CY_U3P_SIB_FREQ_104MHZ              /**< Max. frequency possible: 104 MHz SDR or 52 MHz DDR. */
} CyU3PSibOpFreq;

/** \brief Storage interface control parameters.

    <B>Description</B>\n
    This structure encapsulates the desired operating conditions for the storage ports.
*/
typedef struct CyU3PSibIntfParams
{
    uint8_t        resetGpio;           /**< GPIO to be used for controlling power/reset to the storage device.
                                             Set to 255 if no GPIO is to be used. */
    CyBool_t       rstActHigh;          /**< Whether the reset GPIO (if present) is active high or active low. */
    uint8_t        cardDetType;         /**< Card detection method that should be used for this S port. */
    uint8_t        writeProtEnable;     /**< Indicates whether device specified write protection is allowed
                                             for this S port. */
    uint8_t        lowVoltage;          /**< Enable/disable low voltage operation on the port. */
    uint8_t        voltageSwGpio;       /**< FX3 GPIO to be used for voltage switching. Set to 255 if no GPIO is
                                             to be used. */
    CyBool_t       lvGpioState;         /**< State of the GPIO that sets the S-port to low voltage:
                                             CyTrue : GPIO=0 => 3.3 V, GPIO=1 => 1.8 V.
                                             CyFalse: GPIO=1 => 3.3 V, GPIO=0 => 1.8 V. */
    uint8_t        useDdr;              /**< Whether to enable DDR clock for the S port. */
    CyU3PSibOpFreq maxFreq;             /**< Maximum operating frequency for thr S port. */
    uint8_t        cardInitDelay;       /**< Initialization delay (in ms) to allow card to stabilize. Some
                                             storage devices appear to need some time to start responding after
                                             initial power up. */
} CyU3PSibIntfParams_t;

/** \brief Logical unit properties.

    <B>Description</B>\n
    The following structure stores information about each logical unit (partition) on the storage
    devices connected on each storage port. Each boot partition is counted as a separate logical
    unit. The logical units can be virtual partitions managed by the firmware, in the case where
    the storage device used does not support native partitions. A maximum of four logical units
    can be supported on each storage device.

    **\see
    *\see CyU3PSibLunType
    *\see CyU3PSibLunLocation
*/
typedef struct CyU3PSibLunInfo
{
    uint32_t            startAddr;      /**< Starting address for the logical unit within the device. */
    uint32_t            numBlocks;      /**< Size of the partition in blocks. */
    uint32_t            blockSize;      /**< Block size in bytes for this logical unit. This will be the same as
                                             the device block size. */
    uint8_t             valid;          /**< Whether this partition exists on the storage device.*/
    uint8_t             writeable;      /**< Whether the unit is write enabled. The application can define separate
                                             write permissions for each unit. */
    CyU3PSibLunLocation location;       /**< Location of the logical unit. */
    CyU3PSibLunType     type;           /**< Identifies the type of the logical unit. */
} CyU3PSibLunInfo_t;

/** \brief Storage (SD/MMC) device properties.

    <B>Description</B>\n
    The following structure stores information about the storage device attached to the
    FX3S's storage ports. Please note that most of these fields are relevant only for
    SD and MMC devices. SDIO specific query APIs should be used to get the properties of
    an SDIO device.

    **\see
    *\see CyU3PSibDevType
*/
typedef struct CyU3PSibDevInfo
{
    CyU3PSibDevType cardType;           /**< Type of storage device connected on the S port. (Can be none if
                                             no device detected). */
    uint32_t        clkRate;            /**< Current operating clock frequency for the device. */
    uint32_t        numBlks;            /**< Number of blocks of the storage device. */
    uint32_t        eraseSize;          /**< The erase unit size in bytes for this device. */
    uint16_t        blkLen;             /**< Current block size setting for the device. */
    uint16_t        ccc;                /**< Card command classes (CCC) from the CSD register. */
    uint8_t         removable;          /**< Indicates if the storage device is a removable device. */
    uint8_t         writeable;          /**< Whether the storage device is write enabled. */
    uint8_t         locked;             /**< Identifies whether the storage device is password locked. */
    uint8_t         ddrMode;            /**< Whether DDR clock mode is being used for this device. */
    uint8_t         opVoltage;          /**< Current operating voltage setting for the device. */
    uint8_t         busWidth;           /**< Current bus width setting for the device. */
    uint8_t         numUnits;           /**< Number of boot LUNs & User LUNs present on this device. */
} CyU3PSibDevInfo_t;

/** \brief Storage event callback function type.

    <B>Description</B>\n
    This function type defines a callback for the storage events as well as for the
    status of the storage read/write operations.

    **\see
    *\see CyU3PSibEventType
    *\see CyU3PSibRegisterCbk
*/
typedef void (*CyU3PSibEvtCbk_t)(
        uint8_t portId,                 /**< Port Id on which the event occured. */
        CyU3PSibEventType evt,          /**< Storage event type */
        CyU3PReturnStatus_t status      /**< Status in case of a transfer complete event. */
        );

/** \brief SDIO Card Generic Information and CCCR registers.

    <B>Description</B>\n
    The following structure stores information about the SDIO card attached to a storage port
    of the FX3S.
*/
typedef struct CyU3PSdioCardRegs
{
    uint8_t isMemoryPresent;            /**< Is Memory is present on the SDIO. 1 in case of a Combo card,
                                             0 for I/O only cards. */
    uint8_t numberOfFunctions;          /**< Number of I/O Functions present on the card */
    uint8_t CCCRVersion;                /**< CCCR Format Version Number as defined by SDIO spec */
    uint8_t sdioVersion;                /**< Lower 4 bits define SDIO Version supported by the card.
                                             Upper 4 bits define SD Physical Layer Spec supported by the card. */
    uint8_t cardCapability;             /**< Sdio Card Capability register from the CCCR */
    uint8_t cardSpeed;                  /**< Sdio card speed information (Low Speed, Full Speed or High Speed Card) */
    uint16_t fn0BlockSize;              /**< Function 0 Block Size */
    uint32_t addrCIS;                   /**< Pointer to card's common Card Information Structure (CIS) */
    uint16_t manufacturerId;            /**< Manufacturer ID */
    uint16_t manufacturerInfo;          /**< Manufacturer information */
    uint8_t uhsSupport;                 /**< UHS-I support byte from CCCR for SDIO3.0 cards */
    uint8_t supportsAsyncIntr;          /**< Interrupt Extension byte from CCCR SDIO 3.0 Asynchronous Interrupt
                                             support information. */
} CyU3PSdioCardRegs_t;

/** \brief Erase command mode.

    <B>Description</B>\n
    This enumeration lists the possible modes for the erase command. These modes are only
    applicable to eMMC media, and only the standard erase mode is supported for SD cards.
 */
typedef enum CyU3PSibEraseMode
{
    CY_U3P_SIB_ERASE_STANDARD = 0,      /**< Standard erase mode. */
    CY_U3P_SIB_ERASE_SECURE,            /**< Secure erase mode. Only supported for eMMC. */
    CY_U3P_SIB_ERASE_TRIM_STEP1,        /**< Secure trim STEP1. Mark the blocks for secure erase. */
    CY_U3P_SIB_ERASE_TRIM_STEP2         /**< Secure trim STEP2. Erase blocks marked in trim STEP1. */
} CyU3PSibEraseMode;

/************************************************************************************/
/*********************************** Functions **************************************/
/************************************************************************************/

/** \brief Define the interface parameters for a storage port.

    <B>Description</B>\n
    This function is used to define the interface parameters such as low voltage operation, DDR clocking support,
    card detection and write protection support for one of the FX3S storage ports. This function can be called
    before the storage driver is started up, so that these parameters are effective when CyU3PSibStart is
    called.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the parameters were successfully updated.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the parameters passed are incorrect.

    **\see
    *\see CyU3PSibStart
    *\see CyU3PSibIntfParams_t
 */
extern CyU3PReturnStatus_t
CyU3PSibSetIntfParams (
        uint8_t               portId,           /**< Storage port to be updated. */
        CyU3PSibIntfParams_t *intfParams        /**< Parameters to be selected for the storage interface. */
        );

/** \brief Start the storage driver and try to initialize any storage devices connected.

    <B>Description</B>\n
    This function starts the storage driver on the FX3S device, and initializes any storage devices
    that are connected on the two storage ports.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completed successfully.\n
    * CY_U3P_ERROR_NOT_SUPPORTED if the part in use does not support the SD/MMC interface.\n
    * CY_U3P_ERROR_NOT_CONFIGURED if the IO Matrix confiuration is not correct for the storage operation.\n
    * CY_U3P_ERROR_SIB_INIT if DMA channel creation associated with the storage port fails.

    **\see
    *\see CyU3PSibStop
    *\see CyU3PSibInit
 */
extern CyU3PReturnStatus_t
CyU3PSibStart (
        void);

/** \brief Stop the storage driver on the FX3S device.

    <B>Description</B>\n
    De-initializes any connected storage devices and stops the storage driver.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PSibStart
    *\see CyU3PSibDeInit
 */
extern void
CyU3PSibStop (
        void);

/** \brief Initialize the SD/MMC/SDIO device on the specified port.

    <B>Description</B>\n
    This function initializes any storage device connected on the specified port. Also creates the DMA Channels
    and locks required for the driver to access the device safely.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completed successfully.\n
    * CY_U3P_ERROR_SIB_INIT if the DMA channel creation associated with the storage port fails.

    **\see
    *\see CyU3PSibDeInit
    *\see CyU3PSibStart
 */
extern CyU3PReturnStatus_t
CyU3PSibInit (
        uint8_t portId  /* Port Identifier, indicates the port which has to be initialized. */
        );

/** \brief De-Initialize the storage device on the specified port.

    <B>Description</B>\n
    Request to selectively de-initialize the devices connected on one of the storage ports.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PSibInit
    *\see CyU3PSibStop
 */
extern void
CyU3PSibDeInit (
        uint8_t portId                          /**< The storage port which has to be uninitialized. */
        );

/** \brief Register a function for notification of storage related events.

    <B>Description</B>\n
    This function is used to register a callback function which will be called to provide
    notification of storage related events.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completed successfully.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the parameter is not valid.

    **\see
    *\see CyU3PSibEvtCbk_t
 */
extern CyU3PReturnStatus_t
CyU3PSibRegisterCbk (
    CyU3PSibEvtCbk_t sibEvtCbk                  /**< The event callback function to be called. */
    );

/** \brief Query storage device information.

    <B>Description</B>\n
    This function returns the storage device information for the specified port.
    If the operation completes successfully the structure will be populated with
    the storage device information.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completes successfully.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the portId or devInfo_p arguments are bad.\n
    * CY_U3P_ERROR_INVALID_DEV if the device is not valid.

    **\see
    *\see CyU3PSibDevInfo_t
 */
extern CyU3PReturnStatus_t
CyU3PSibQueryDevice (
        uint8_t            portId,              /**< The storage device to be queried. */
        CyU3PSibDevInfo_t *devInfo_p            /**< Pointer to the storage device info structure. */
        );

/** \brief Query storage partition information.

    <B>Description</B>\n
    This function returns the storage partition (LUN) information for the specified port
    and unit number. If the operation completes successfully, the structure will be populated with
    the storage unit information.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completes successfully.\n
    * CY_U3P_ERROR_INVALID_DEV if the device is not valid.\n
    * CY_U3P_ERROR_INVALID_UNIT if the unit is not valid.

    **\see
    *\see CyU3PSibLunInfo_t
 */
extern CyU3PReturnStatus_t
CyU3PSibQueryUnit (
        uint8_t            portId,              /**< The storage device to be queried. */
        uint8_t            unitId,              /**< The storage unit to be queried. */
        CyU3PSibLunInfo_t *unitInfo_p           /**< Pointer to the storage unit info structure. */
        );

/** \brief Get the CSD register content for an SD/MMC card connected.

    <B>Description</B>\n
    This function is used to access the CSD register data of a card on a particular port.
    This function only returns the CSD value that is read during device initialization.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the query function is successful.\n
    * CY_U3P_ERROR_NOT_STARTED  if the storage driver has not been started.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the portId or csd_buffer argument is invalid.\n
    * CY_U3P_ERROR_INVALID_DEV if the storage device does not exist.
 */
extern CyU3PReturnStatus_t
CyU3PSibGetCSD (
        uint8_t  portId,                        /**< Port on which to query the CSD. */
        uint8_t *csd_buffer                     /**< Buffer to be filled with CSD content. */
        );

/** \brief Read the Extended CSD register from an MMC card.

    <B>Description</B>\n
   This function reads the extended CSD register content from an MMC device connected to
   FX3S.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the read is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the portId or buffer_p argument is invalid.\n
    * CY_U3P_ERROR_INVALID_DEV if the device addressed is not an MMC card.\n
    * CY_U3P_ERROR_DEVICE_BUSY if the storage device is performing a data transfer.

    **\see
    *\see CyU3PSibSendSwitchCommand
 */
extern CyU3PReturnStatus_t
CyU3PSibGetMMCExtCsd (
        uint8_t  portId,        /**< Port on which to read the Ext. CSD register. */
        uint8_t *buffer_p       /**< Buffer to read the register value into. Should be able to hold 512 bytes. */
        );

/** \brief Send a SWITCH command to update the Ext. CSD register on an MMC device.

    <B>Description</B>\n
    This API can be used to send a SWITCH command to update a Ext. CSD register byte on an MMC device.
    While SD cards support the SWITCH command, their usage model is different; and this API does not
    support them.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the switch command was sent successfully.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if an invalid port ID is specified.\n
    * CY_U3P_ERROR_INVALID_DEV if the addressed device is not an MMC device.\n
    * CY_U3P_ERROR_DEVICE_BUSY if the MMC device is busy performing data transfer.

    **\see
    *\see CyU3PSibGetMMCExtCsd
 */
extern CyU3PReturnStatus_t
CyU3PSibSendSwitchCommand (
        uint8_t   portId,       /**< Port on which to send the Switch command. */
        uint32_t  argument,     /**< Switch command argument. No validation is done on this parameter. */
        uint32_t *resp_p        /**< Buffer to read the Switch command response into (optional, can be NULL). */
        );

/** \brief Define the block length to be used for storage data transfers.

    <B>Description</B>\n
    This function is used to configure the block length to be used for storage data
    transfers through the FX3S device. Note that this API sets the block length
    only in the FX3S device registers. If a corresponding SD/MMC command is to be
    sent to the storage device, that needs to be done separately through the
    CyU3PSibVendorAccess API.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PSibVendorAccess
 */
extern void
CyU3PSibSetBlockLen (
        uint8_t  portId,                /**< Port id on which to change the block length. */
        uint16_t blkLen                 /**< Desired block length in bytes. */
        );

/** \brief Modify default storage write timer interval for data transfers.

    <B>Description</B>\n
    This function is used to modify the default timer interval i.e. 5 seconds used for storage write data
    transfers through the FX3S device.
    
    This function should be called every time SIB is initialized, if expecting storage write transactions
    ranging more than 5 seconds. Storage write timer resets to default interval every time SIB gets de-initialized.
 
    <B>Return value</B>\n
    * None

    **\see
 */
extern void
CyU3PSibWriteTimerModify (
        uint32_t newTimerVal            /**< New Timer Interval to be used for current transaction. */
        );

/** \brief Initiates a read/write data request.

    <B>Description</B>\n
    This function is used to initiate a read/write request to a storage device. The open-ended
    read/write commands (CMD18 and CMD25) are used in all cases. The CY_U3P_SIB_EVENT_XFER_CPLT
    event will be sent to the caller through the register storage callback function when the
    specified amount of data has been completely transferred.
 
    This function internally initiates the DMA channel for transferring the specified amount
    of data. It is therefore expected that the DMA channel is left in the idle state when
    calling this API.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completed successfully.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the port or unit number is out of range.\n
    * CY_U3P_ERROR_INVALID_DEV if the device specified is not present.\n
    * CY_U3P_ERROR_CARD_LOCKED if the storage device being accessed is password locked.\n
    * CY_U3P_ERROR_WRITE_PROTECTED if the storage device is write protected.\n
    * CY_U3P_ERROR_INVALID_UNIT if the LUN specified is not a valid LUN.\n
    * CY_U3P_ERROR_INVALID_ADDR if the address specified is not valid.\n
    * CY_U3P_ERROR_DEVICE_BUSY if the storage device is already busy processing another request.

    **\see
    *\see CyU3PSibCommitReadWrite
    *\see CyU3PSibSetWriteCommitSize
    *\see CyU3PSibWriteTimerModify
 */
extern CyU3PReturnStatus_t
CyU3PSibReadWriteRequest (
        CyBool_t isRead,                        /**< Read or a write operation */
        uint8_t  portId,                        /**< Storage port number to access. */
        uint8_t  unitId,                        /**< Storage partition (unit) number to access. */
        uint16_t numBlks,                       /**< Number of blocks to be read or written. */
        uint32_t blkAddr,                       /**< Start address of the operation */
        uint8_t  socket                         /**< The SIB socket on the which the operation is to happen. */
        );

/** \brief Commits any pending read/write transaction.

    <B>Description</B>\n
    Function to commit a pending read/write operation to the SD/MMC storage; by making use
    of the STOP_TRANSMISSION command.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation was committed successfully.\n
    * CY_U3P_ERROR_NOT_STARTED if the storage driver has not been started.\n
    * CY_U3P_ERROR_INVALID_DEV if there is no device connected to the port.

    **\see
    *\see CyU3PSibReadWriteRequest
    *\see CyU3PSibSetWriteCommitSize
 */
extern CyU3PReturnStatus_t
CyU3PSibCommitReadWrite (
        uint8_t portId                          /**< Port on which the operation is to be committed. */
        );

/** \brief Set the sector granularity at which write operations are committed to the SD/MMC storage.

    <B>Description</B>\n
    Committing write data to SD/MMC storage in small chunks can lead to poor write transfer performance.
    The write performance to these devices can be improved by combining write operations to sequential
    addresses, and performing a single commit (STOP_TRANSMISSION) operation.
 
    This function is used to set the granularity at which write operations are committed to the storage
    device. By default, this value is set to 1, meaning that a write of one sector or more will be
    immediately committed to the storage device.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the write granularity is set properly.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the portId is invalid or the numSectors value is set to 0.

    **\see
    *\see CyU3PSibReadWriteRequest
    *\see CyU3PSibCommitReadWrite
 */
extern CyU3PReturnStatus_t
CyU3PSibSetWriteCommitSize (
        uint8_t  portId,                        /**< Storage port being configured. */
        uint32_t numSectors                     /**< Number of sectors after which a write operation should be
                                                     committed to the SD/MMC storage. */
        );

/** \brief Abort an ongoing transaction.

    <B>Description</B>\n
    This function is used to abort any ongoing transactions on the specified port. Prior to calling this
    function, the corresponding DMA channel should be reset.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completed successfully.
 */
extern CyU3PReturnStatus_t
CyU3PSibAbortRequest (
        uint8_t portId                  /**< Port on which the transaction is to be aborted. */
        );

/** \brief Erase blocks on the storage card.

    <B>Description</B>\n
    This function is used to erase the specified number of erase units on an SD card or eMMC device.
    Please see the SD/eMMC specifications for the definition of an erase unit and its computation.

    On an eMMC device, this API can be used to erase the contents of the boot partition(s) as well.
    Please use the CyU3PSibSendSwitchCommand() API to select the partition to be erased, prior to
    calling this API to initiate the erase operation.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the operation completed successfully.\n
    * CY_U3P_ERROR_NOT_STARTED - if the storage driver has not been started.\n
    * CY_U3P_ERROR_INVALID_DEV - if no storage device is found on the specified port.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if this operation is requested on a device other than SD card.\n
    * CY_U3P_ERROR_INVALID_ADDR - if the address given is not valid.\n
    * CY_U3P_ERROR_TIMEOUT - if the operation timed out while erasing the blocks.\n
    * CY_U3P_ERROR_DEVICE_BUSY - if the device is processing another request.

    **\see
    *\see CyU3PSibEraseMode
    *\see CyU3PSibSendSwitchCommand
 */
extern CyU3PReturnStatus_t
CyU3PSibEraseBlocks (
        uint8_t           portId,               /**< Port Id on which the erase operation is to be carried out. */
        uint16_t          numEraseUnits,        /**< Number of Erase Units to be erased. */
        uint32_t          startAddr,            /**< Starting address from which the blocks are to be erased. */
        CyU3PSibEraseMode mode                  /**< Type of erase operation to be performed. */
        );

/** \brief Function to get current status of the SD/MMC card.

    <B>Description</B>\n
    This function sends the CMD13 (SEND_STATUS) command to the SD/MMC device connected on the
    specified storage port and provides the card status response that is received. This API can
    be used to check whether the storage device is accessible and is in the TRAN state where
    it can receive new commands.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if a R1 response was received from the storage device.\n
    * CY_U3P_ERROR_NOT_STARTED - if the storage driver has not been started.\n
    * CY_U3P_ERROR_INVALID_DEV - if no storage device is found on the specified port.\n
    * CY_U3P_ERROR_TIMEOUT - if there is a response timeout or the card is signalling busy condition.\n
    * CY_U3P_ERROR_DEVICE_BUSY - if the device is processing another request.
 */
extern CyU3PReturnStatus_t
CyU3PSibGetCardStatus (
        uint8_t   portId,               /**< Port id on which the card is connected. */
        uint32_t *status_p              /**< Return parameter through which the card status is retrieved. */
        );

/** \brief Partition a storage device into multiple logical units.

    <B>Description</B>\n
    The available storage on a SD/MMC device connected to FX3S can be partitioned into multiple
    logical units which can be separately accessed. This function is used to setup the partitions
    as required. The number of partitions to be created, the sizes for all except the last
    partition; and the types associated with each partition are specified as parameters.
    The size of the last partition is inferred as the number of remaining blocks available
    on the storage device, and does not need to be specified.

    As a custom partitioning scheme is used by the firmware, partitions created by FX3S cannot be
    identified by another SD/MMC host controller. Therefore, this feature should only be used
    in cases where there is no need for the storage device to be read through another controller.
 
    <B>Note</B>\n
    The FX3S boot loader will only be able to boot from a storage partition at the top
    of the available storage. Therefore, it is not useful to set any of the partitions other
    than 0, as CY_U3P_SIB_LUN_BOOT partitions.

    **\see
    *\see CyU3PSibLunInfo_t
    *\see CyU3PSibLunType
    *\see CyU3PSibQueryUnit
    *\see CyU3PSibRemovePartitions

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if all partitions were created successfully.\n
    * CY_U3P_ERROR_NOT_STARTED if the storage driver has not been started.\n
    * CY_U3P_ERROR_INVALID_DEV if the storage device specified does not exist.\n
    * CY_U3P_ERROR_ALREADY_PARTITIONED if the storage device has already been partitioned.\n
    * CY_U3P_ERROR_INVALID_ADDR if the partition sizes specified cannot be setup.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the partition count or types specified are invalid.\n
    * CY_U3P_ERROR_DEVICE_BUSY if the device is busy processing another request.
 */
extern CyU3PReturnStatus_t
CyU3PSibPartitionStorage (
        uint8_t   portId,                       /**< Storage port to do the partitioning on. */
        uint8_t   partCount,                    /**< Total number of partitions required. Should be less than or equal
                                                     to 4. */
        uint32_t *partSizes_p,                  /**< Sizes for each of the first (N - 1) partitions. The remaining
                                                     storage on the device will be assigned to the last partition. */
        uint8_t  *partType_p                    /**< Type for each partition (boot or data partition). */
        );

/** \brief Read the contents of an internal register on the SD/eMMC device connected.

    <B>Description</B>\n
    This function returns the contents of an internal register on the SD/eMMC device connected
    to the FX3S or SD3 device. Since these internal registers can only be read at init time,
    the API returns the values that would have been read and cached during the init process.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the read is successful.\n
    * CY_U3P_ERROR_NOT_STARTED if the storage driver has not been started.\n
    * CY_U3P_ERROR_INVALID_DEV if the device specified does not exist.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the register specified is invalid.

    **\see
    *\see CyU3PSibDevRegType
 */
extern CyU3PReturnStatus_t
CyU3PSibReadRegister (
        uint8_t             portId,
        CyU3PSibDevRegType  regType,
        uint8_t            *dataBuffer,
        uint8_t            *dataLen
        );

/** \brief Remove all partitions and re-unify a storage device.

    <B>Description</B>\n
    This function is used to remove all partitions created using the CyU3PSibPartitionStorage API,
    and re-unify the complete storage available on a device as a single data partition (volume).

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the partitions were successfully removed.\n
    * CY_U3P_ERROR_NOT_STARTED if the storage driver has not been started.\n
    * CY_U3P_ERROR_INVALID_DEV if the device specified does not exist.\n
    * CY_U3P_ERROR_NOT_PARTITIONED if the device has not been partitioned.\n
    * CY_U3P_ERROR_DEVICE_BUSY if the device is busy processing another request.

    **\see
    *\see CyU3PSibPartitionStorage
 */
extern CyU3PReturnStatus_t
CyU3PSibRemovePartitions (
        uint8_t portId                          /**< Storage port to remove partitions on. */
        );

/** \brief Lock/Unlock an SD Card.

    <B>Description</B>\n
    This function is used to lock or unlock an SD Card on the specified port.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completed successfully.\n
    * CY_U3P_ERROR_CARD_LOCK_UNLOCK if the operation to lock/unlock the card fails.
 */
extern CyU3PReturnStatus_t
CyU3PSibLockUnlockCard (
        uint8_t   portId,               /**< Port on which the lock/unlock operation is to be done. */
        CyBool_t  lockCard,             /**< Specifies whether to lock (1) or unlock (0) the card. */
        CyBool_t  clrPasswd,            /**< Used along with the Unlock card to clear the password. */
        uint8_t  *passwd,               /**< The current password set in the card. */
        uint8_t   passwdLen             /**< The current password length in bytes. */
        );

/** \brief Set/replace password on an SD Card.

    <B>Description</B>\n
    This function is used to set or replace the password on an SD Card on the specified port.
    The card can also be locked while setting the password by setting the lockCard variable.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completed successfully.\n
    * CY_U3P_ERROR_CARD_LOCK_UNLOCK if setting the password failed.
 */
extern CyU3PReturnStatus_t
CyU3PSibSetPasswd (
        uint8_t   portId,               /**< Port on which the set/replace password operation is to be done. */
        CyBool_t  lockCard,             /**< Specifies whether to lock the card while setting the password.  */
        uint8_t  *passwd,               /**< The password to be set on the card. In case the password is being
                                             replaced this indicates the current password in use. */
        uint8_t   passwdLen,            /**< The length of the password in bytes. In case the password is being
                                             replaced this indicates the current password length in bytes. */
        uint8_t  *newPasswd,            /**< The new password to be set on the card. */
        uint8_t   newPasswdLen          /**< The new password length. This should be set to zero in case the
                                             password is not being replaced. */
        );

/** \brief Clear the password on an SD Card.

    <B>Description</B>\n
    This function is used to clear the password on an SD Card on the specified port.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completed successfully.\n
    * CY_U3P_ERROR_CARD_LOCK_UNLOCK if the operation failed while removing password.
 */
extern CyU3PReturnStatus_t
CyU3PSibRemovePasswd (
        uint8_t  portId,                /**< Port on which the clear password operation is to be done. */
        uint8_t *passwd,                /**< The current password set in the card. */
        uint8_t  passwdLen              /**< The current password length in bytes. */
        );

/** \brief Force erase the user content and the lock/unlock related information on an SD Card.

    <B>Description</B>\n
    This function is used force erase the user content on the SD Card and also clear the
    current password and unlock the card.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completed successfully.\n
    * CY_U3P_ERROR_CARD_FORCE_ERASE if the operation failed while force erasing.
 */
extern CyU3PReturnStatus_t
CyU3PSibForceErase (
        uint8_t portId                  /**< Port on which the force erase is to be done. */
        );

/** \brief Vendor SD/MMC command access.

    <B>Description</B>\n
    This function is used to send general SD/MMC commands to storage device/cards attached
    to FX3S device and receive the corresponding response. Custom commands can be implemented
    using this function.
 
    The response data will be placed in the output parameter respData along with the CRC7 and
    end bits. respLen parameter indicates the expected response length (in bits) not including
    the start, transmit, CRC7 and end bits.
 
    If the command being executed has an associated data phase; the numBlks, socket and pChHandle
    parameters can be used to manage the data transfer.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completed successfully.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the parameters passed to the function are not valid.\n
    * CY_U3P_ERROR_TIMEOUT timed out while waiting for the response from the card.\n
    * CY_U3P_ERROR_DEVICE_BUSY if the device is busy processing another request.

    **\see
    *\see CyU3PSibSetBlockLen
*/
extern CyU3PReturnStatus_t
CyU3PSibVendorAccess (
        uint8_t   portId,               /**< Storage port on which command is to be executed. */
        uint8_t   cmd,                  /**< SD/MMC Commande to be sent */
        uint32_t  cmdArg,               /**< Command argument */
        uint8_t   respLen,              /**< Length of response in bits */
        uint8_t  *respData,             /**< Pointer to response data */
        uint16_t  numBlks,              /**< Length of data to be transferred in blocks. Set to 0 if there is
                                             no data phase. */
        CyBool_t  isRead,               /**< Direction of data transfer, in case there is a data phase. */
        uint8_t   socket                /**< S-Port socket to be used. */
        );

/** \brief Reset an SDIO card.

    <B>Description</B>\n
    This function resets an I/O only SDIO card (or the I/O portion of a combo card) by
    writing 1 to the RES bit in the CCCR. This function does not reset the memory portion
    in case of a combo card.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT if a timeout occurs during the operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED if the device on the addressed port is not an SDIO device.\n
    * CY_U3P_ERROR_CARD_NOT_ACTIVE if card has not been intialized or is in Standby or Inactive sates.\n
    * CY_U3P_ERROR_UNKNOWN if a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_ILLEGAL_CMD if the command is not legal for the card state.\n
    * CY_U3P_ERROR_CRC if the CRC check of the previous command failed.
  */
extern CyU3PReturnStatus_t
CyU3PSdioCardReset (
        uint8_t portId                  /**< Port ID for card to be Reset. */
        );

/** \brief Fetch SDIO card information and Card common register information.

    <B>Description</B>\n
    This function returns an CYU3PSdioCardRegs_t object which contains card common register
    information from an initialized SDIO card on the port specified by portId.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completes successfully.\n
    * CY_U3P_ERROR_NOT_SUPPORTED if the device on the addressed port is not an SDIO device.

    **\see
    *\see CyU3PSdioCardRegs_t
*/
extern CyU3PReturnStatus_t
CyU3PSdioQueryCard (
        uint8_t portId,                 /**< Port id on which SDIO card is connected. */
        CyU3PSdioCardRegs_t *data       /**< Output parameter to be filled with SDIO register information. */
        );

/** \brief Perform a Direct I/O operation (Single Byte) to an SDIO card using CMD52.

    <B>Description</B>\n
    This function is used to read or write a single byte of data from/to a register on
    the SDIO card. When the readAfterWrite flag is set, the data from the register is
    read back after the write has been completed.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the I/O operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT if a timeout occurs during the I/O operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED if the device does not support the operation.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if function number or register address is out of range.\n
    * CY_U3P_ERROR_CARD_NOT_ACTIVE if card has not been intialized or is in Standby or Inactive states.\n
    * CY_U3P_ERROR_BAD_CMD_ARG if the command argument was out of the allowed range for the card.\n
    * CY_U3P_ERROR_INVALID_FUNCTION if an invalid function number was requested.\n
    * CY_U3P_ERROR_UNKNOWN if a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_ILLEGAL_CMD if the command is not legal for the card state.\n
    * CY_U3P_ERROR_CRC if the CRC check of the previous command failed.
 */
extern CyU3PReturnStatus_t
CyU3PSdioByteReadWrite (
        uint8_t portId,                 /**< Port ID for card on which CMD52 operation is to be executed. */
        uint8_t functionNumber,         /**< SDIO Card Function number (0-7)  for the Byte IO operation. */
        uint8_t isWrite,                /**< CY_U3P_SDIO_WRITE indicates Write a operation.
                                             CY_U3P_SDIO_READ indicates a Read operation. */
        uint8_t readAfterWriteFlag,     /**< CY_U3P_SDIO_READ_AFTER_WRITE indicates request for Read back of
                                             register after Write. */
        uint32_t registerAddr,          /**< Register address to which IO operation is to be performed. */
        uint8_t *data                   /**< In case of Write operation, the byte of data to be written to the card.
                                             In case of Read (or Write with readAfterWriteFlag=1) the data read
                                             from the SDIO card will be returned through the same parameter. */
        );

/** \brief Perform Extended I/O operation (Multi-Byte/Block) to/from SDIO card using CMD53.

    <B>Description</B>\n
    This API reads or writes multiple bytes of data from/to the address specified.
    The length of data to be written needs to be specified as the parameter. The
    transfer size can be specified in the form of a byte count or a block count.
    The API provides the option of auto-incrementing the address after writing or
    reading each data byte/block. The data is read/written to/from the dma channel
    associated with the socket ID passed in for the operation. This operation should
    use one of the SIB Sockets as producer (for read) or consumer (for write).
 
    Transferring infinite blocks of data (by setting transferCount to 0 in block mode)
    is not supported on the FX3S devices. If transferCount is set to 0, FX3S will transfer
    512 bytes of data in Multi-Byte transfer mode, but will throw an error in Multi-Block
    mode.
 
    The following table shows the number of bytes and blocks transferred based on
    transferCount values:\n

    \verbatim
     ___________________________________________________________________________________
    |  transferCount Value       |  0x000      |   0x001     |   0x002  | ... |  0x1FF  |
    |____________________________|_____________|_____________|__________|_____|_________|
    |  Bytes Transfered          |   512       |    1        |    2     |     |   511   |
    |  Blocks Transferred        | illegal     |    1        |    2     |     |   511   |
    |____________________________|_____________|_____________|__________|_____|_________|
    \endverbatim

    This API will fail with error code INVALID_FUNCTION if data transfer length is less then 16 bytes. 
    CyU3PSdioDirectReadWrite API should be called in place of current API.
 
    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the IO operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT if a timeout occurs during the operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED if the device does not support CMD53 operations.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the function number or register address is out of range.\n
    * CY_U3P_ERROR_CARD_NOT_ACTIVE if card has not been intialized or is in Standby or Inactive states.\n
    * CY_U3P_ERROR_BAD_CMD_ARG if the command argument was out of the allowed range for the card.\n
    * CY_U3P_ERROR_INVALID_FUNCTION if an invalid function number or data transfer length less than 16 byte was requested.\n
    * CY_U3P_ERROR_UNKNOWN if a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_ILLEGAL_CMD if the command is not legal for the card state.\n
    * CY_U3P_ERROR_CRC if the CRC check of the previous command failed.
 */
extern CyU3PReturnStatus_t
CyU3PSdioExtendedReadWrite (
        uint8_t portId,                 /**< Port ID for card on which CMD53 operation is to be executed. */
        uint8_t functionNumber,         /**< SDIO Card Function number (0-7)  for the Extended IO operation. */
        uint8_t isWrite,                /**< CY_U3P_SDIO_WRITE indicates a Write operation.
                                             CY_U3P_SDIO_READ indicates a Read operation. */
        uint8_t blockMode,              /**< CY_U3P_SDIO_RW_BYTE_MODE indicates multi-byte transfer.
                                             CY_U3P_SDIO_RW_BLOCK_MODE indicates multi-block (1 or more) transfer. */
        uint8_t opCode,                 /**< CY_U3P_SDIO_RW_ADDR_FIXED indicates all I/O to a single address (FIFO
                                             access); CY_U3P_SDIO_RW_ADDR_AUTO_INCREMENT indicates I/O to
                                             incrementing addresses (RAM like data buffer). */
        uint32_t registerAddr,          /**< Register address to which I/O operation is to be performed. This will
                                             be the starting address in case of auto-increment mode. */
        uint16_t transferCount,         /**< Number of Bytes or Blocks to be transferred. */
        uint16_t sckId                  /**< DMA Socket to be used to produce/consume data for the I/O operation. */
        );


/** \brief Perform a Direct I/O operation (Single Byte) to an SDIO card using CMD52.

    <B>Description</B>\n
    This function is used to read or write less than 16 bytes of data from/to a register on
    the SDIO card. This API should be called in place of CyU3PSdioExtendedReadWrite () API 
    for data transfer length of less then 16 bytes.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the I/O operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT if a timeout occurs during the I/O operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED if the device does not support the operation.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if function number or register address is out of range.\n
    * CY_U3P_ERROR_CARD_NOT_ACTIVE if card has not been intialized or is in Standby or Inactive states.\n
    * CY_U3P_ERROR_BAD_CMD_ARG if the command argument was out of the allowed range for the card.\n
    * CY_U3P_ERROR_INVALID_FUNCTION if an invalid function number was requested.\n
    * CY_U3P_ERROR_UNKNOWN if a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_ILLEGAL_CMD if the command is not legal for the card state.\n
    * CY_U3P_ERROR_CRC if the CRC check of the previous command failed.
 */
extern CyU3PReturnStatus_t
CyU3PSdioDirectReadWrite (
        uint8_t  portId,          /**< Port ID for card on which CMD52 operation is to be executed. */
        uint8_t  functionNumber,  /**< SDIO Card Function number (0-7)  for the Multi Byte IO operation. */               
        uint8_t  isWrite,         /**< CY_U3P_SDIO_WRITE indicates a Write operation.
                                       CY_U3P_SDIO_READ indicates a Read operation. */
        uint8_t  opCode,          /**< CY_U3P_SDIO_RW_ADDR_FIXED indicates all I/O to a single address (FIFO
                                       access); CY_U3P_SDIO_RW_ADDR_AUTO_INCREMENT indicates I/O to
                                       incrementing addresses (RAM like data buffer). */
        uint8_t  *data_p,         /**< In case of Write operation, the byte of data to be written to the card.
                                       In case of Read (or Write with readAfterWriteFlag=1) the data read
                                       from the SDIO card will be returned through the same parameter. */
        uint32_t registerAddr,    /**< Register address to which I/O operation is to be performed. This will
                                       be the starting address in case of auto-increment mode. */     
        uint16_t transferCount    /**< Number of Bytes to be transferred. */
        );


/** \brief Suspend or Resume SDIO I/O Function.

    <B>Description</B>\n
    This function is used to suspend or resume the specified SDIO Function. Suspend
    and resume needs to be supported by the card to which these calls are being
    addressed. Support for suspend and resume can be ascertained by looking at the
    CY_U3P_SDIO_CARD_CAPABLITY_SBS bit of the card's card-capability register. When
    the function is resumed, it also checks if any data is available.

    <B>Note</B>\n
    This function is not supported in the SDK 1.3 Beta release.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS \n
    * CY_U3P_ERROR_BAD_ARGUMENT \n
    * CY_U3P_ERROR_NOT_SUPPORTED \n
    * CY_U3P_ERROR_RESUME_FAILED
 */
extern CyU3PReturnStatus_t
CyU3PSdioSuspendResumeFunction (
        uint8_t portId,                 /**< Port ID for card on which suspend/resume operation is to be executed. */
        uint8_t functionNumber,         /**< SDIO Card Function number to be suspended. */
        uint8_t operation,              /**< CY_U3P_SDIO_SUSPEND indicates a request to Suspend the function.
                                             CY_U3P_SDIO_RESUME indicates a request to Resume the function. */
        uint8_t *isDataAvailable        /**< Non-zero if data is available after resuming the function */
        );


/** \brief Abort an ongoing extended read or extended write operation.

    <B>Description</B>\n
    This function is used abort an ongoing extended I/O operation.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS \n
    * CY_U3P_ERROR_BAD_ARGUMENT
 */
extern CyU3PReturnStatus_t
CyU3PSdioAbortFunctionIO (
        uint8_t portId,                 /**< Port ID for card on which I/O abort is to be executed. */
        uint8_t functionNumber          /**< SDIO Card Function number on which I/O is to be aborted. */
        );

/** \brief Enable or Disable Interrupts on the SDIO Card.

    <B>Description</B>\n
    This function is used to enable or disable interrupts on the SDIO card. A request to
    enable interrupts for an SDIO Function (1-7) will automatically enable the card's
    Interrupt Master.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the register update is successful.\n
    * CY_U3P_ERROR_TIMEOUT if a timeout occurs during the register update.\n
    * CY_U3P_ERROR_NOT_SUPPORTED if the device does not support this operation.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the function number is out of range.\n
    * CY_U3P_ERROR_CARD_NOT_ACTIVE if card has not been intialized or is in Standby or Inactive states.\n
    * CY_U3P_ERROR_BAD_CMD_ARG if the command argument was out of the allowed range for the card.\n
    * CY_U3P_ERROR_INVALID_FUNCTION if an invalid function number was requested.\n
    * CY_U3P_ERROR_UNKNOWN if a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_ILLEGAL_CMD if the command is not legal for the card state.\n
    * CY_U3P_ERROR_CRC if the CRC check of the previous command failed.

    **\see
    *\see CyU3PSdioCheckInterruptEnableStatus
*/
extern CyU3PReturnStatus_t
CyU3PSdioInterruptControl (
        uint8_t portId,         /**< Port number for card on which interrupt control operation is to be executed. */
        uint8_t functionNumber, /**< Function number (1-7) or CY_U3P_SDIO_INT_MASTER (for card interrupt master),
                                     for which interrupt control operation is requested */
        uint8_t operation,      /**< CY_U3P_SDIO_DISABLE_INT indicates interrupt for function (or global) needs
                                     to be disabled. CY_U3P_SDIO_ENABLE_INT indicates interrupt for function (or
                                     global) needs to be enabled. CY_U3P_SDIO_CHECK_INT_ENABLE_REG indicates a
                                     request to read and return the interrupt enable register from the card's
                                     common registers. */
        uint8_t* intEnReg       /**< Value of card's Interrupt Enable register post execution of enable / disable /
                                     check operation. Status for current function can be checked using
                                     CyU3PSdioCheckInterruptEnableStatus. */
        );


/** \brief Get Address for the CIS for the specified card function.

    <B>Description</B>\n
    This function gets the Card Information Structure base address for the specified function
    on the SDIO card.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT if a timeout occurs while reading the CIS address bytes.\n
    * CY_U3P_ERROR_NOT_SUPPORTED if the device does not support this operation.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the function number is out of range.\n
    * CY_U3P_ERROR_CARD_NOT_ACTIVE if card has not been intialized or is in Standby or Inactive states.\n
    * CY_U3P_ERROR_INVALID_FUNCTION if an invalid function number was requested.\n
    * CY_U3P_ERROR_UNKNOWN if a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_ILLEGAL_CMD if the command is not legal for the card state.\n
    * CY_U3P_ERROR_CRC if the CRC check of the previous command failed.
 */
extern CyU3PReturnStatus_t
CyU3PSdioGetCISAddress (
        uint8_t portId,         /**< Port number for the card on which operation is to be executed. */
        uint8_t functionNumber, /**< Function number whose CIS address is to be fetched. */
        uint32_t* addressCIS    /**< Value of the Function's 17-Bit CIS Address */
        );

/** \brief Read data for a CIS Tuple into a buffer.

    <B>Description</B>\n
    This function reads out a CIS Tuple from the card's CIS area into a buffer.
    The size of the tuple is returned in the tupleSize parameter. The buffer memory
    should be initialized before being provided to this call. As per the SDIO
    specification, no SDIO tuple can have more than 255 bytes of data.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT if a timeout occurs during the operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED if the device does not support this operation.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the function number is out of range.\n
    * CY_U3P_ERROR_CARD_NOT_ACTIVE if card has not been intialized or is in Standby or Inactive states.\n
    * CY_U3P_ERROR_INVALID_FUNCTION if an invalid function number was requested.\n
    * CY_U3P_ERROR_UNKNOWN if a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_ILLEGAL_CMD if the command is not legal for the card state.\n
    * CY_U3P_ERROR_CRC if the CRC check of the previous command failed.
*/
extern CyU3PReturnStatus_t
CyU3PSdioGetTuples(
        uint8_t portId,         /**< Port number for the card on which operation is to be executed. */
        uint8_t funcNo,         /**< Function whose Tuple Data is to be fetched. */
        uint8_t tupleId,        /**< TUPLE_CODE for which data is to be fetched. */
        uint32_t addrCIS,       /**< Address to the Function's CIS. */
        uint8_t* buffer,        /**< Pointer for buffer into which data will be read into. */
        uint8_t* tupleSize      /**< Size of tuple body as read from the TUPLE_LINK */
        );

/** \brief Set the block size for an IO function.

    <B>Description</B>\n
    This function sets the block size for a function. The block size set should be
    lower than the maximum block size value read from the CISTPL_FUNCE CIS tuple for
    the function. The value set to the card using this API is saved in the driver and
    is used as the transfer block size in case of extended transfers.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the IO operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT if a timeout occurs during the operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED if the device does not support the operation.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the function number is out of range.\n
    * CY_U3P_ERROR_CARD_NOT_ACTIVE if card has not been intialized or is in Standby or Inactive states.\n
    * CY_U3P_ERROR_BAD_CMD_ARG if the command's argument was out of the allowed range for the card.\n
    * CY_U3P_ERROR_INVALID_FUNCTION if an invalid function number was requested.\n
    * CY_U3P_ERROR_UNKNOWN if a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_ILLEGAL_CMD if the command is not legal for the card state.\n
    * CY_U3P_ERROR_CRC if the CRC check of the previous command failed.
*/
extern CyU3PReturnStatus_t
CyU3PSdioSetBlockSize(
        uint8_t portId,         /**< Port number for the card on which operation is to be executed. */
        uint8_t funcNo,         /**< Function whose block size is to be set. */
        uint16_t blockSize      /**< Block size to be set for the function. */
        );

/** \brief Get block size which has been set for IO function to the device.

    <B>Description</B>\n
    This function gets the block size for a function by reading the block size registers
    for the function. The API also updates the saved transfer block size in the driver
    which is used in case of extended transfers.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the IO operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT if a timeout occurs during the operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED if the device does not support the operation.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the function number is out of range.\n
    * CY_U3P_ERROR_CARD_NOT_ACTIVE if card has not been intialized or is in Standby or Inactive states.\n
    * CY_U3P_ERROR_BAD_CMD_ARG if the command's argument was out of the allowed range for the card.\n
    * CY_U3P_ERROR_INVALID_FUNCTION if an invalid function number was requested.\n
    * CY_U3P_ERROR_UNKNOWN if a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_ILLEGAL_CMD if the command is not legal for the card state.\n
    * CY_U3P_ERROR_CRC if the CRC check of the previous command failed.
*/
extern CyU3PReturnStatus_t
CyU3PSdioGetBlockSize(
        uint8_t portId,                 /**< Port number for the card on which operation is to be executed. */
        uint8_t funcNo,                 /**< Function whose block size is to be set. */
        uint16_t *blockSize             /**< Pointer for buffer through which the block size will be returned. */
        );


/** \brief Enable Read Wait on the SDIO BUS.

    <B>Description</B>\n
    This function triggers Read-Wait on card which supports the feature using the DAT[2]
    of the SDIO Data bus.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completes successfully.\n
    * CY_U3P_ERROR_NOT_SUPPORTED if the device does not support the operation.
*/
extern CyU3PReturnStatus_t
CyU3PSdioReadWaitEnable(
        uint8_t portId,         /**< Port number for the card on which operation is to be executed. */

        uint8_t isRdWtEnable    /**< 1 to Trigger Read-Wait on the Data Bus, 0 to restart I/O on the Data Bus */
        );

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3SIB_H_ */

/*[]*/

