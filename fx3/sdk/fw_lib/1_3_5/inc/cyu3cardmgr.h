/*
 ## Cypress FX3 Core Library Header (cyu3cardmgr.h)
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

#ifndef _INCLUDED_CYU3CARDMGR_H_
#define _INCLUDED_CYU3CARDMGR_H_

#include <cyu3os.h>
#include <cyu3types.h>
#include <cyu3sib.h>
#include <cyu3cardmgr_fx3s.h>
#include <cyu3externcstart.h>

/** \file cyu3cardmgr.h
    \brief This file defines the data types and APIs provided by the low level
    SD/MMC/SDIO driver for FX3S.

    <B>Description</B>\n
    The EZ-USB FX3S device is an extension to the FX3 device, which has the capability
    to talk to SD/MMC/SDIO peripherals.

    The SD/MMC/SDIO low level driver provides functions to send commands to these
    peripherals and perform data transfers. These functions are intended to be used
    internally by the FX3S storage API layer, and not expected to be called directly
    from user applications.
 */

/** \def CY_U3P_BOOT_PART_SIGN
    \brief Signature that identifies an FX3S Virtual Boot Partition.
 */
#define CY_U3P_BOOT_PART_SIGN                           (0x42575943)

/** \def CY_U3P_MMC_ECSD_REVISION_LOC
    \brief Location of MMC revision field in MMC Extended CSD register.
 */
#define CY_U3P_MMC_ECSD_REVISION_LOC                    (192)

/** \def CY_U3P_MMC_ECSD_PARTCFG_LOC
    \brief Location of Partition Config field in MMC Extended CSD register.
 */
#define CY_U3P_MMC_ECSD_PARTCFG_LOC                     (179)

/** \def CY_U3P_MMC_ECSD_BOOTSIZE_LOC
    \brief Location of Boot Partition Size field in MMC Extended CSD register.
 */
#define CY_U3P_MMC_ECSD_BOOTSIZE_LOC                    (226)

/** \def CY_U3P_CLOCK_DIVIDER_400
    \brief Clock divider value to get 400 KHz interface clock frequency.
 */
#define CY_U3P_CLOCK_DIVIDER_400                        (0x3FF)

/** \def CY_U3P_CLOCK_DIVIDER_20M
    \brief Clock divider value to get 20 MHz interface clock frequency.
 */
#define CY_U3P_CLOCK_DIVIDER_20M                        (0x14)

/** \def CY_U3P_CLOCK_DIVIDER_26M
    \brief Clock divider value to get 26 MHz interface clock frequency.
 */
#define CY_U3P_CLOCK_DIVIDER_26M                        (0x0F)

/** \def CY_U3P_CLOCK_DIVIDER_46M
    \brief Clock divider value to get 46 MHz interface clock frequency.
 */
#define CY_U3P_CLOCK_DIVIDER_46M                        (0x08)

/** \def CY_U3P_CLOCK_DIVIDER_52M
    \brief Clock divider value to get 52 MHz interface clock frequency.
 */
#define CY_U3P_CLOCK_DIVIDER_52M                        (0x07)

/** \def CY_U3P_CLOCK_DIVIDER_84M
    \brief Clock divider value to get 84 MHz interface clock frequency.
 */
#define CY_U3P_CLOCK_DIVIDER_84M                        (0x04)

/** \def CY_U3P_CLOCK_DIVIDER_104M
    \brief Clock divider value to get 104 MHz interface clock frequency.
 */
#define CY_U3P_CLOCK_DIVIDER_104M                       (0x03)

/** \def CY_U3P_SIB_DEVICE_POSTRESET_DELAY
    \brief Post reset (CMD0) delay allowed for storage devices.
 */
#define CY_U3P_SIB_DEVICE_POSTRESET_DELAY               (5)

/** \def CY_U3P_SIB_DEVICE_READY_TIMEOUT
    \brief Timeout (in ms) for the wait for device readyness.
 */
#define CY_U3P_SIB_DEVICE_READY_TIMEOUT                 (0x3FF)

/** \def CY_U3P_SIB_POLLING_DELAY
    \brief Storage driver polling delay in us.
 */
#define CY_U3P_SIB_POLLING_DELAY                        (5)

/** \def CY_U3P_SIB_SENDCMD_DELAY
    \brief Storage driver command send delay.
 */
#define CY_U3P_SIB_SENDCMD_DELAY                        (10)

/** \def CY_U3P_SIB_HOTPLUG_DELAY
    \brief Delay required from hotplug event to handler start (in ms).
 */
#define CY_U3P_SIB_HOTPLUG_DELAY                        (100)

/** \def CY_U3P_SIB_BLOCK_SIZE
    \brief Default data block size is 512 bytes.
 */
#define CY_U3P_SIB_BLOCK_SIZE                           (0x0200)

/** @cond DOXYGEN_HIDE */

/** \def CY_U3P_SIB_DSR_DEF_VAL
    \brief SD Spec 2.0 Sec 5.5 Driver Stage Register's default value.
 */
#define CY_U3P_SIB_DSR_DEF_VAL                          (0x0404)

#define CY_U3P_SIB_CS_REG_MASK                          (0xFFFFFF00)
#define CY_U3P_SIB_CS_CFG_MASK                          (0xFFFFC000)

/** \def CY_U3P_SIB_HCS_BIT
    \brief Host Capacity Bit mask in the Operating Conditions Register (OCR).
 */
#define CY_U3P_SIB_HCS_BIT                              (0x40000000)

#define CY_U3P_SIB_CCS_BIT                              CY_U3P_SIB_HCS_BIT
#define CY_U3P_SIB_SEC_MODE_ADDR_BIT                    CY_U3P_SIB_HCS_BIT

/** \def CY_U3P_SIB_XPC_BIT
    \brief XPC bit in OCR of SD 3.0 cards.
 */
#define CY_U3P_SIB_XPC_BIT                              (0x10000000)

/** \def CY_U3P_SIB_S18R_BIT
    \brief S18R and S18A bit mask in OCR of SD 3.0 cards.
 */
#define CY_U3P_SIB_S18R_BIT                             (0x01000000)

#define CY_U3P_SIB_S18A_BIT                             CY_U3P_SIB_S18R_BIT

#define CY_U3P_SIB_DEF_VOLT_RANGE                       (0x00FFFF00)
#define CY_U3P_SIB_CARD_BUSY_BIT_POS                    (0x1F)

/** @endcond */

/** \def CY_U3P_SIB_SD_CMD11_CLKSTOP_DURATION
    \brief Clock stop duration while switching operating voltage.
 */
#define CY_U3P_SIB_SD_CMD11_CLKSTOP_DURATION            (10)

/** \def CY_U3P_SIB_SD_CMD11_CLKACT_DURATION
    \brief Delay from SD clock start that is required for SD card to drive the IOs high.
 */
#define CY_U3P_SIB_SD_CMD11_CLKACT_DURATION             (2)

/** \def CY_U3P_SIB_WRITE_DATA_TIMEOUT
    \brief Write data timeout value in clock ticks.
 */
#define CY_U3P_SIB_WRITE_DATA_TIMEOUT                   (5000)

/** \def CY_U3P_SIB_IDLE_DATA_TIMEOUT
    \brief Idle Time out value.
 */
#define CY_U3P_SIB_IDLE_DATA_TIMEOUT                    (2000)

/** \def CY_U3P_SIB_SECTORCNT_TO_BYTECNT
    \brief Macro to convert a sector address/count value to a byte address/count.
 */
#define CY_U3P_SIB_SECTORCNT_TO_BYTECNT(n)              ((n) << 9)

/** \def CY_U3P_SIB_BYTECNT_TO_SECTORCNT
    \brief Macro to convert a byte address/count value to a sector address/count.
 */
#define CY_U3P_SIB_BYTECNT_TO_SECTORCNT(n)              ((n) >> 9)

#define CY_U3P_SD_SW_QUERY_FUNCTIONS                    (0x00FFFFF1) /**< Switch parameter to query device functions. */
#define CY_U3P_SD_SW_HIGHSP_PARAM                       (0x80FFFFF1) /**< Switch parameter to enable High Speed mode. */
#define CY_U3P_SD_SW_UHS1_PARAM                         (0x80FFFFF2) /**< Switch parameter to select UHS-1 mode. */

/* MMC Switch command parameters. */
#define CY_U3P_MMC_SW_PARTCFG_USER_PARAM                (0x03B30000) /**< Switch parameter to select User Data area. */
#define CY_U3P_MMC_SW_PARTCFG_BOOT1_PARAM               (0x03B30100) /**< Switch parameter to select Boot1 partition. */
#define CY_U3P_MMC_SW_PARTCFG_BOOT2_PARAM               (0x03B30200) /**< Switch parameter to select Boot2 partition. */

/** \def CY_U3P_SIB_VBP_PARTSIZE_LOCATION
    \brief Offset of partition size byte in the Virtual Partition Header.
 */
#define CY_U3P_SIB_VBP_PARTSIZE_LOCATION                (40)

/** \def CY_U3P_SIB_VBP_PARTTYPE_LOCATION
    \brief Offset of partition type byte in the Virtual Partition Header.
 */
#define CY_U3P_SIB_VBP_PARTTYPE_LOCATION                (47)

/** \def CY_U3P_SIB_VBP_CHECKSUM_LOCATION
    \brief Offset of checksum byte in the Virtual Partition Header.
 */
#define CY_U3P_SIB_VBP_CHECKSUM_LOCATION                (68)

/** \def CyU3PSibSetNumBlocks
    \brief Macro to set the number of data blocks for transfer on one SD port.
 */
#define CyU3PSibSetNumBlocks(portId, numBlks) \
    (SIB->sdmmc[(portId)].block_count = (numBlks))

/** \def CyU3PSibSetActiveSocket
    \brief Macro that sets the active socket for one SD port.
 */
#define CyU3PSibSetActiveSocket(portId, sockNum) \
{\
    SIB->sdmmc[(portId)].cs = ((SIB->sdmmc[(portId)].cs & ~CY_U3P_SIB_SOCKET_MASK) | \
                               (CyU3PDmaGetSckNum((sockNum)) << CY_U3P_SIB_SOCKET_POS));\
}

/** \def CyU3PSibResetSibCtrlr
    \brief Macro to reset the SIB Controller and then restore the operating mode.
 */
#define CyU3PSibResetSibCtrlr(portId) \
{\
    uint32_t tmp321 = SIB->sdmmc[(portId)].mode_cfg; \
    SIB->sdmmc[(portId)].cs |= CY_U3P_SIB_RSTCONT; \
    while (SIB->sdmmc[(portId)].cs & CY_U3P_SIB_RSTCONT); \
    SIB->sdmmc[(portId)].mode_cfg = tmp321; \
}

/** \def CyU3PSibClearIntr
    \brief Macro to clear all the interrupts on the given port.
 */
#define CyU3PSibClearIntr(portId) \
{ \
    SIB->sdmmc[(portId)].intr = ~(CY_U3P_SIB_SDMMC_INTR_DEFAULT); \
}

/** \def CyU3PSibConvertAddr
    \brief Macro to convert the block address to byte address if the target device is byte addressed.
 */
#define CyU3PSibConvertAddr(portId, blkAddr) \
{ \
    if (glCardCtxt[(portId)].highCapacity == CyFalse) \
    { \
        (blkAddr) <<= 9; \
    } \
}

/** \def CyU3PSdioNumberOfFunction
    \brief Get the number of functions supported by an SDIO device.
 */
#define CyU3PSdioNumberOfFunction(portId) (SIB->sdmmc[(portId)].resp_reg0>>28)&0x7

/** \def CyU3PSdioMemoryPresent
    \brief Macro to check whether an SDIO device is a combo card.
 */
#define CyU3PSdioMemoryPresent(portId) (SIB->sdmmc[(portId)].resp_reg0>>27)&0x1

/** \def CyU3PSdioOCRRegister
    \brief Get the OCR register setting for an SDIO device.
 */
#define CyU3PSdioOCRRegister(portId) ((SIB->sdmmc[(portId)].resp_reg0>>8)&0xFFFFFF)

/************************************************************************************/
/********************************DATATYPES*******************************************/
/************************************************************************************/

/** \brief SD/MMC peripheral states.

    <B>Description</B>\n
    List of SD/MMC peripheral states, as defined by the specifications.
 */
typedef enum
{
    CY_U3P_SD_MMC_IDLE = 0,             /**< Idle state. */
    CY_U3P_SD_MMC_READY,                /**< Ready state. */
    CY_U3P_SD_MMC_IDENTIFICATION,       /**< Identification state. */
    CY_U3P_SD_MMC_STANDBY,              /**< Standby state. */
    CY_U3P_SD_MMC_TRANSFER,             /**< Transfer state. */
    CY_U3P_SD_MMC_DATA,                 /**< Data state. */
    CY_U3P_SD_MMC_RECEIVE,              /**< Receive state. */
    CY_U3P_SD_MMC_PROGRAMMING,          /**< Programming state. */
    CY_U3P_SD_MMC_DISCONNECT,           /**< Disconnect state. */
    CY_U3P_MMC_BUS_TEST                 /**< Bus test state. */
} CyU3PSdMmcStates_t;

/** \brief Enumeration of SD Card Versions.
 */
typedef enum 
{
    CY_U3P_SD_CARD_VER_1_X = 0,         /**< SD 1.x cards: Byte addressed. */
    CY_U3P_SD_CARD_VER_2_0,             /**< SD 2.0 cards: Sector addressed, high capacity. */
    CY_U3P_SD_CARD_VER_3_0              /**< SD 3.0 cards: Extended capacity, UHS-1 cards. */
} CyU3PSDCardVer_t;

/** \brief Enumeration of storage peripheral bus widths.
 */
typedef enum
{
    CY_U3P_CARD_BUS_WIDTH_1_BIT = 0,    /**< Single bit mode. */
    CY_U3P_CARD_BUS_WIDTH_4_BIT,        /**< 4 bit wide mode. */
    CY_U3P_CARD_BUS_WIDTH_8_BIT         /**< 8 bit wide mode (eMMC only). */
} CyU3PCardBusWidth_t;

/** \brief Enumeration of SD Card Registers.
 */
typedef enum
{
    CY_U3P_SIB_SD_REG_OCR = 0,          /**< OCR Register */
    CY_U3P_SIB_SD_REG_CID,              /**< CID Register */
    CY_U3P_SIB_SD_REG_CSD               /**< CSD Register */
} CyU3PSibSDRegs_t;

/** \brief Enumeration of card operating modes.
 */
typedef enum
{
    CY_U3P_SIB_SD_CARD_ID_400 = 0,      /**< SD 400 KHz */
    CY_U3P_SIB_SDSC_25,                 /**< SDSC */
    CY_U3P_SIB_SDHC_DS_25,              /**< SDHC Default Speed Mode 0 - 25MHz */
    CY_U3P_SIB_SDHC_HS_50,              /**< SDHC 25MB/sec 0 - 50MHz */
    CY_U3P_SIB_UHS_I_DS,                /**< Default Speed up to 25MHz 3.3V signaling */
    CY_U3P_SIB_UHS_I_HS,                /**< High Speed up to 50MHz 3.3V signaling */
    CY_U3P_SIB_UHS_I_SDR12,             /**< SDR up to 25MHz 1.8V signaling */
    CY_U3P_SIB_UHS_I_SDR25,             /**< SDR up to 50MHz 1.8V signaling */
    CY_U3P_SIB_UHS_I_SDR50,             /**< SDR up to 100MHz 1.8V signaling */
    CY_U3P_SIB_UHS_I_SDR104,            /**< SDR up to 208MHz 1.8V signaling */
    CY_U3P_SIB_UHS_I_DDR50              /**< DDR up to 50MHz 1.8V signaling */
} CyU3PCardOpMode_t;

#ifdef CYU3P_STORAGE_SDIO_SUPPORT

/** \brief FBR register structure as defined by SDIO specification.
 */
typedef struct CyU3PSdioFbr
{
    uint8_t interfaceCode;    /**< Standard Function Interface Code */
    uint8_t extInterfaceCode; /**< Extended Standard Function Interface Code */
    uint8_t supportCSA;       /**< Function support for the CSA */
    uint8_t dataWindow;       /**< Data access window to Function Code Storage Area (CSA)  */
    uint8_t supportWakeUp;    /**< Function supports Wake up or not */
    uint8_t csaProperty;      /**< CSA Property  */
    uint16_t maxIoBlockSize;  /**< IO block size for the function */
    uint32_t addrCIS;         /**< Pointer of the CIS register for the function */
    uint32_t addrCSA;         /**< Pointer of the CSA register for the function */
    uint32_t cardPSN;         /**< Card Product serial number  */
    uint32_t csaSize;         /**< CSA Size in Bytes */
} CyU3PSdioFbr_t;

#endif /* CYU3P_STORAGE_SDIO_SUPPORT */

/** \brief Structure that holds properties of an SD/MMC peripheral.

    <B>Description</B>\n
    This structure holds status and state information about an SD/MMC peripheral connected
    to the FX3S device.
 */
typedef struct CyU3PCardCtxt
{
    uint8_t  cidRegData[CY_U3P_SD_REG_CID_LEN]; /**< CID Register data */
    uint8_t  csdRegData[CY_U3P_SD_REG_CSD_LEN]; /**< CSD Register data */
    uint32_t ocrRegData;                        /**< OCR Register data */

    uint32_t numBlks;           /**< Current block size setting for the device. */
    uint32_t eraseSize;         /**< The erase unit size in bytes for this device. */
    uint32_t dataTimeOut;       /**< Timeout value for data transactions */

    uint16_t blkLen;            /**< Current block size setting for the device. */
    uint16_t cardRCA;           /**< Relative card address */
    uint16_t ccc;               /**< Card command classes */

    uint8_t  cardType;          /**< Type of storage device connected on the S port. (Can be none if
                                     no device detected).*/
    uint8_t  removable;         /**< Indicates if the storage device is a removable device. */
    uint8_t  writeable;         /**< Whether the storage device is write enabled. */
    uint8_t  locked;            /**< Identifies whether the storage device is password locked. */
    uint8_t  ddrMode;           /**< Whether DDR clock mode is being used for this device. */
    uint8_t  clkRate;           /**< Current operating clock frequency for the device.*/
    uint8_t  opVoltage;         /**< Current operating voltage setting for the device. */
    uint8_t  busWidth;          /**< Current bus width setting for the device. */

    uint8_t  cardVer;           /**< Card version */
    uint8_t  highCapacity;      /**< Indicates if the card is a high capacity card. */
    uint8_t  uhsIOpMode;        /**< SD 3.0 Operating modes */
    uint8_t  cardSpeed;         /**< Card speed information */

#ifdef CYU3P_STORAGE_SDIO_SUPPORT

    CyBool_t isMemoryPresent;   /**< Memory is present in SDIO */
    uint8_t  numberOfFunction;  /**< Number of Function present in SDIO */
    uint8_t  CCCRVersion;       /**< Version number of CCCR register in SDIO */ 
    uint8_t  sdioVersion;       /**< SDIO Version  */
    uint8_t  cardCapability;    /**< Sdio Card Capability  */
    uint32_t addrCIS;           /**< Address of CIS */
    uint16_t manufactureId;     /**< Manufacture ID */
    uint16_t manufactureInfo;   /**< Manufacture information */	 
    uint16_t fnBlkSize [8];     /**< Array to store block size from SetBlockSize*/

    uint8_t  uhsSupport;        /**< Whether the SDIO device supports UHS-1 mode. */
    uint8_t  supportsAsyncIntr; /**< Whether the SDIO device supports asynchronous interrupts. */

#endif /* CYU3P_STORAGE_SDIO_SUPPORT */

} CyU3PCardCtxt_t;

/** \brief Global variable that holds peripheral properties.
 */
extern CyU3PCardCtxt_t glCardCtxt[CY_U3P_SIB_NUM_PORTS];

/*****************************************************************************/
/*******************************FUNCTION PROTOTYPES***************************/
/*****************************************************************************/

/** \brief Initialize a SD/MMC/SDIO storage device attached to FX3S.
  
    <B>Description</B>\n
   Initialize a SD/MMC/SDIO storage device attached to the specified FX3S storage
   port. This will return immediately if the peripheral connected to the port has
   already been initialized.
  
   <B>Return value</B>\n
   * CY_U3P_SUCCESS if the initialization is successful.\n
   * CY_U3P_ERROR_TIMEOUT if the peripheral fails to respond.
 */
extern CyU3PReturnStatus_t
CyU3PCardMgrInit (
        uint8_t portId          /**< Port on which the peripheral is connected. */
        );

/** \brief De-initialize a storage peripheral connected to FX3S.

    <B>Description</B>\n
    De-initialize a storage peripheral connected to the specified storage port.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PCardMgrDeInit (
        uint8_t portId          /**< Port on which the peripheral to be de-initialized is connected. */
        );

/** \brief Select the interface clock frequency for a storage port.

    <B>Description</B>\n
    This function updates the interface clock frequency for the specified FX3S storage port.
    The clock is stopped while the frequency is changed and then re-enabled.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PCardMgrSetClockFreq (
        uint8_t  portId,        /**< Storage port to be updated. */
        uint16_t clkDivider,    /**< Clock divider to be used. The base clock frequency is always 384 MHz. */
        CyBool_t halfDiv        /**< Whether a 0.5 factor has to be added to the selected divider. */
        );

/** \brief Initiate a data write request to an SD card or eMMC storage device.

    <B>Description</B>\n
    This function sends the write command (CMD24 or CMD25) to the SD/eMMC storage peripheral
    and prepares the FX3S block for the corresponding data transfer. The transfer completion
    will be notified through interrupt when done.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the write setup is successful.\n
    * CY_U3P_ERROR_INVALID_ADDR if the write parameter is invalid.
 */
extern CyU3PReturnStatus_t
CyU3PCardMgrSetupWrite (
        uint8_t portId,                 /**< Port on which to setup the write operation. */
        uint8_t unit,                   /**< Storage partition to write to. Used for address calculation. */
        uint8_t socket,                 /**< DMA socket to be used for the write operation. */
        uint16_t numWriteBlks,          /**< Number of block of data to be written. */
        uint32_t blkAddr                /**< Sector address to write to (within the selected partition). */
        );

/** \brief Initiate a data read request to an SD card or eMMC storage device.

    <B>Description</B>\n
    This function sends the read command (CMD17 or CMD18) to the SD/eMMC storage peripheral
    and prepares the FX3S block for the corresponding data transfer. The transfer completion
    will be notified through interrupt when done.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the read setup is successful.\n
    * CY_U3P_ERROR_INVALID_ADDR if the read parameter is invalid.
 */
extern CyU3PReturnStatus_t
CyU3PCardMgrSetupRead (
        uint8_t portId,                 /**< Port on which read operation is to be performed. */
        uint8_t unit,                   /**< Partition to read data from. */
        uint8_t socket,                 /**< DMA socket to be used for read. */
        uint16_t numReadBlks,           /**< Number of blocks of data to read. */
        uint32_t blkAddr                /**< Sector address to read data from. */
        );

/** \brief Continue reading/writing data from the address where the previous operation ended.

    <B>Description</B>\n
    Data transfer performance from/to SD/MMC devices can be improved by combining sequential
    operations into a single read/write command. This function sets up the FX3S storage port
    to continue a previously paused storage read/write operation. This is generally used only
    for write transfers.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the transfer setup is successful.\n
    * CY_U3P_ERROR_TIMEOUT if the transfer could not be resumed.
 */
extern CyU3PReturnStatus_t
CyU3PCardMgrContinueReadWrite (
        uint8_t  isRead,                /**< CyTrue = Read operation, CyFalse = Write operation. */
        uint8_t  portId,                /**< Port to do the operation on. */
        uint8_t  socket,                /**< Socket to use for data transfer. */
        uint16_t numBlks                /**< Number of additional blocks (sectors) to transfer. */
        );

/** \brief Stop an ongoing SD/MMC data transfer.

    <B>Description</B>\n
    This function is used to send a stop transmission command to the card to
    stop an ongoing data transfer.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the stop command is successful.\n
    * CY_U3P_ERROR_TIMEOUT if the card failed to respond.
 */
extern CyU3PReturnStatus_t
CyU3PCardMgrStopTransfer (
        uint8_t portId                  /**< Port on which transfer is to be stopped. */
        );

/** \brief Get the CSD register data for an SD/MMC peripheral.

    <B>Description</B>\n
    This function returns the contents of the CSD register of the SD/MMC peripheral
    connected on the specified port. As the CSD register cannot be read at all times,
    this returns values that are buffered during the peripheral initialization.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PCardMgrGetCSD (
        uint8_t  portId,                /**< Port number. */
        uint8_t *csd_buffer             /**< Return buffer to be filled with CSD data. */
        );

/** \brief Get the Extended CSD register content for an SD/MMC peripheral.

     <B>Description</B>\n
     This function reads and returns the content of the Extended CSD register on the SD
     card or MMC device connected to the specified storage device.

     <B>Return value</B>\n
     * CY_U3P_SUCCESS if the read is successful.\n
     * CY_U3P_ERROR_TIMEOUT if the read operation times out.
 */
extern CyU3PReturnStatus_t
CyU3PCardMgrReadExtCsd (
        uint8_t  portId,                /**< Port number. */
        uint8_t *buffer_p               /**< Buffer to read the register data into. Should be able to hold 512 bytes. */
        );

/** \brief Send a command to an SD/MMC/SDIO peripheral.

     <B>Description</B>\n
     This function sends the specified command to the specified SD/MMC/SDIO peripheral
     and initiates the device to receive the response (if any).

     <B>Return value</B>\n
     * CY_U3P_SUCCESS if the read is successful.\n
     * CY_U3P_ERROR_TIMEOUT if the read operation times out.
 */
extern void
CyU3PCardMgrSendCmd (
        uint8_t  portId,                /**< Port number. */
        uint8_t  cmd,                   /**< Command opcode. */
        uint8_t  respLen,               /**< Expected response length in bits. */
        uint32_t cmdArg,                /**< Command argument value. */
        uint32_t flags                  /**< Flags that indicate whether any data transfer is expected. */
        );

/** \brief Function to wait for a SIB related interrupt.

    <B>Description</B>\n
    This function is used to wait for a SIB related completion interrupt, or until
    an error or timeout is detected.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the specified interrupt is received.\n
    * CY_U3P_ERROR_CRC if a CRC error is detected.\n
    * CY_U3P_ERROR_TIMEOUT if the interrupt was not received.
 */
extern CyU3PReturnStatus_t
CyU3PCardMgrWaitForInterrupt (
        uint8_t  portId,                /**< Port number */
        uint32_t intr,                  /**< Type of interrupt to wait for. */
        uint32_t timeout                /**< Timeout to be applied in 5 us units. */
        );

/** \def CyU3PCardMgrWaitForCmdResp
    \brief Wait until a command response is received from the SD/MMC/SDIO peripheral.
 */
#define CyU3PCardMgrWaitForCmdResp(port)        \
    (CyU3PCardMgrWaitForInterrupt((port),CY_U3P_SIB_RCVDRES,0x3FFFF))

/** \def CyU3PCardMgrWaitForBlkXfer
    \brief Wait until a data transfer completion interrupt is received from the SD/MMC/SDIO peripheral.
 */
#define CyU3PCardMgrWaitForBlkXfer(port,intr)   \
    (CyU3PCardMgrWaitForInterrupt((port),(intr),glCardCtxt[(port)].dataTimeOut))

/** \brief Check the status of SD/MMC peripheral and wait until it is in the transfer state.

    <B>Description</B>\n
    This function is used to read the status (CMD13) of the SD/MMC peripheral and wait until
    it is in the transfer state. This is used to ensure that the peripheral is ready to receive
    the next data transfer command.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the card moved to the transfer state.\n
    * CY_U3P_ERROR_TIMEOUT if the card did not move to the transfer state.
 */
extern CyU3PReturnStatus_t
CyU3PCardMgrCheckStatus (
        uint8_t portId                  /**< Port on which the peripheral state is to be checked. */
        );

/** \brief Complete the initialization of an SD card after it has been unlocked.

    <B>Description</B>\n
    If an SD card is password locked, it cannot be fully initialized during the
    CyU3PCardMgrInit call. This function is used to complete the Initialization of
    the SD card after it has been unlocked.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the initialization is successful.\n
    * CY_U3P_ERROR_TIMEOUT if the initialization times out.
 */
extern CyU3PReturnStatus_t
CyU3PCardMgrCompleteSDInit (
        uint8_t portId                  /**< Port on which the card is connected. */
        );

#ifdef CYU3P_STORAGE_SDIO_SUPPORT

/** \brief Enables or disables the interrupt for the specified IO function.
 
    <B>Description</B>\n
    Enables or disables the interrupt for the specified IO function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the interrupt control is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the function specified is invalid.
 */
extern CyU3PReturnStatus_t
CyU3PSdioGlobalInterruptControl (
        uint8_t  portId,                /**< Port on which SDIO device is connected. */
        uint8_t  funcNo,                /**< SDIO function to be controlled. */
        CyBool_t enable                 /**< Whether to enable or disable the interrupt. */
        );

/** \brief Initialize the specified SDIO function.

    <B>Description</B>\n
    Initialize the specified SDIO function for data transfer.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the initialization is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the function number is invalid.\n
    * CY_U3P_ERROR_TIMEOUT if the initialization times out.
 */
extern CyU3PReturnStatus_t 
CyU3PSdioFunctionInit (
        uint8_t portId,                 /**< Port on which SDIO device is connected. */
        uint8_t funcNo                  /**< Function to be initialized. */
        );

/** \brief De-initialize the specified SDIO function.

    <B>Description</B>\n
    De-initialize the specified SDIO function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the de-initialization is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the function number is invalid.\n
 */
extern CyU3PReturnStatus_t
CyU3PCardMgrSdioFuncDeInit (
        uint8_t portId,                 /**< Port on which SDIO device is connected. */
        uint8_t funcNo                  /**< Function to be de-initialized. */
        );

#endif /* CYU3P_STORAGE_SDIO_SUPPORT */

#include <cyu3externcend.h>

#endif /* _INCLUDED_CYU3CARDMGR_H_ */

/*[]*/

