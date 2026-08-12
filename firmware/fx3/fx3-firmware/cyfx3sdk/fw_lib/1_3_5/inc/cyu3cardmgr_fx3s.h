/*
 ## Cypress FX3 Core Library Header (cyu3cardmgr_fx3s.h)
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

#ifndef _INCLUDED_CYU3CARDMGR_FX3S_H_
#define _INCLUDED_CYU3CARDMGR_FX3S_H_

#include <cyu3os.h>
#include <cyu3types.h>
#include <cyu3sib.h>

#include "cyu3externcstart.h"

/** \file cyu3cardmgr_fx3s.h
    \brief This file contains the provides the low level SD/MMC/SDIO driver related macros
    and definitions.
*/

/** @cond DOXYGEN_HIDE */

/** \section SDMMC_Command SD and MMC card commands
    \brief The following macros define the various SD and MMC commands that are defined
    in the SD and MMC device specifications.
 */

#define CY_U3P_SD_MMC_CMD0_GO_IDLE_STATE                 (0x00)         /**< Go-idle state command. */
#define CY_U3P_MMC_CMD1_SEND_OP_COND                     (0x01)         /**< Send-Op Condition command (MMC) */
#define CY_U3P_SD_MMC_CMD2_ALL_SEND_CID                  (0x02)         /**< All Send CID command. */
#define CY_U3P_SD_CMD3_SEND_RELATIVE_ADDR                (0x03)         /**< Send relative address command (SD) */
#define CY_U3P_MMC_CMD3_SET_RELATIVE_ADDR                (0x03)         /**< Set relative address command (MMC) */
#define CY_U3P_SD_MMC_CMD4_SET_DSR                       (0x04)         /**< Set DSR command. */
#define CY_U3P_MMC_CMD5_SLEEP_AWAKE                      (0x05)         /**< Sleep/Wake command (MMC) */
#define CY_U3P_SD_MMC_CMD5_IO_SEND_OP_COND               (0x05)         /**< IO Send Op Condition command (SDIO) */
#define CY_U3P_SD_MMC_CMD6_SWITCH                        (0x06)         /**< Switch command */
#define CY_U3P_SD_MMC_CMD7_SELECT_DESELECT_CARD          (0x07)         /**< Select/Deselect card command */
#define CY_U3P_SD_CMD8_SEND_IF_COND                      (0x08)         /**< Send */
#define CY_U3P_MMC_CMD8_SEND_EXT_CSD                     (0x08)         /**< */
#define CY_U3P_SD_MMC_CMD9_SEND_CSD                      (0x09)         /**< */
#define CY_U3P_SD_MMC_CMD10_SEND_CID                     (0x0A)         /**< */
#define CY_U3P_MMC_CMD11_READ_DAT_UNTIL_STOP             (0x0B)         /**< */
#define CY_U3P_SD_CMD11_VOLTAGE_SWITCH                   (0x0B)         /**< */
#define CY_U3P_SD_MMC_CMD12_STOP_TRANSMISSION            (0x0C)         /**< */
#define CY_U3P_SD_MMC_CMD13_SEND_STATUS                  (0x0D)         /**< */
#define CY_U3P_MMC_CMD14_BUSTEST_R                       (0x0E)         /**< */
#define CY_U3P_SD_MMC_CMD15_GO_INACTIVE_STATE            (0x0F)         /**< */
#define CY_U3P_SD_MMC_CMD16_SET_BLOCKLEN                 (0x10)         /**< */
#define CY_U3P_SD_MMC_CMD17_READ_SINGLE_BLOCK            (0x11)         /**< */
#define CY_U3P_SD_MMC_CMD18_READ_MULTIPLE_BLOCK          (0x12)         /**< */
#define CY_U3P_MMC_CMD19_BUSTEST_W                       (0x13)         /**< */
#define CY_U3P_SD_CMD19_SEND_TUNING_PATTERN              (0x13)         /**< */
#define CY_U3P_MMC_CMD20_WRITE_DAT_UNTIL_STOP            (0x14)         /**< */
#define CY_U3P_SD_MMC_CMD21_RESERVED_21                  (0x15)         /**< */
#define CY_U3P_SD_MMC_CMD22_RESERVED_22                  (0x16)         /**< */
#define CY_U3P_MMC_CMD23_SET_BLOCK_COUNT                 (0x17)         /**< */
#define CY_U3P_SD_MMC_CMD24_WRITE_BLOCK                  (0x18)         /**< */
#define CY_U3P_SD_MMC_CMD25_WRITE_MULTIPLE_BLOCK         (0x19)         /**< */
#define CY_U3P_MMC_CMD26_PROGRAM_CID                     (0x1A)         /**< */
#define CY_U3P_SD_MMC_CMD27_PROGRAM_CSD                  (0x1B)         /**< */
#define CY_U3P_SD_MMC_CMD28_SET_WRITE_PROT               (0x1C)         /**< */
#define CY_U3P_SD_MMC_CMD29_CLR_WRITE_PROT               (0x1D)         /**< */
#define CY_U3P_SD_MMC_CMD30_SEND_WRITE_PROT              (0x1E)         /**< */
#define CY_U3P_SD_MMC_CMD31_RESERVED                     (0x1F)         /**< */
#define CY_U3P_SD_CMD32_ERASE_WR_BLK_START               (0x20)         /**< */
#define CY_U3P_SD_CMD33_ERASE_WR_BLK_END                 (0x21)         /**< */
#define CY_U3P_SD_MMC_CMD34_RESERVED                     (0x22)         /**< */
#define CY_U3P_MMC_CMD35_ERASE_GROUP_START               (0x23)         /**< */
#define CY_U3P_MMC_CMD36_ERASE_GROUP_END                 (0x24)         /**< */
#define CY_U3P_SD_MMC_CMD37_RESERVED                     (0x25)         /**< */
#define CY_U3P_SD_MMC_CMD38_ERASE                        (0x26)         /**< */
#define CY_U3P_MMC_CMD39_FAST_IO                         (0x27)         /**< */
#define CY_U3P_MMC_CMD40_GO_IRQ_STATE                    (0x28)         /**< */
#define CY_U3P_SD_MMC_CMD41_RESERVED                     (0x29)         /**< */
#define CY_U3P_SD_MMC_CMD42_LOCK_UNLOCK                  (0x2A)         /**< */
#define CY_U3P_SD_MMC_CMD43_RESERVED                     (0x2B)         /**< */
#define CY_U3P_SD_MMC_CMD44_RESERVED                     (0x2C)         /**< */
#define CY_U3P_SD_MMC_CMD45_RESERVED                     (0x2D)         /**< */
#define CY_U3P_SD_MMC_CMD46_RESERVED                     (0x2E)         /**< */
#define CY_U3P_SD_MMC_CMD47_RESERVED                     (0x2F)         /**< */
#define CY_U3P_SD_MMC_CMD48_RESERVED                     (0x30)         /**< */
#define CY_U3P_SD_MMC_CMD49_RESERVED                     (0x31)         /**< */
#define CY_U3P_SD_MMC_CMD50_RESERVED                     (0x32)         /**< */
#define CY_U3P_SD_MMC_CMD51_RESERVED                     (0x33)         /**< */
#define CY_U3P_SD_MMC_CMD52_IO_RW_DIRECT                 (0x34)         /**< */
#define CY_U3P_SD_MMC_CMD53_IO_RW_EXTENDED               (0x35)         /**< */
#define CY_U3P_SD_MMC_CMD54_RESERVED                     (0x36)         /**< */
#define CY_U3P_SD_MMC_CMD55_APP_CMD                      (0x37)         /**< */
#define CY_U3P_SD_MMC_CMD56_GEN_CMD                      (0x38)         /**< */
#define CY_U3P_SD_MMC_CMD57_RESERVED                     (0x39)         /**< */
#define CY_U3P_SD_MMC_CMD58_RESERVED                     (0x3A)         /**< */
#define CY_U3P_SD_MMC_CMD59_RESERVED                     (0x3B)         /**< */
#define CY_U3P_SD_MMC_CMD60_RW_MULTIPLE_REGISTER         (0x3C)         /**< */
#define CY_U3P_SD_MMC_CMD61_RW_MULTIPLE_BLOCK            (0x3D)         /**< */

#define CY_U3P_SD_ACMD6_SET_BUS_WIDTH                    (0x06)         /**< */
#define CY_U3P_SD_ACMD13_SD_STATUS                       (0x0D)         /**< */
#define CY_U3P_SD_ACMD17_RESERVED                        (0x11)         /**< */
#define CY_U3P_SD_ACMD18_RESERVED                        (0x12)         /**< */
#define CY_U3P_SD_ACMD19_RESERVED                        (0x13)         /**< */
#define CY_U3P_SD_ACMD20_RESERVED                        (0x14)         /**< */
#define CY_U3P_SD_ACMD21_RESERVED                        (0x15)         /**< */
#define CY_U3P_SD_ACMD22_SEND_NUM_WR_BLOCKS              (0x16)         /**< */
#define CY_U3P_SD_ACMD23_SET_WR_BLK_ERASE_COUNT          (0x17)         /**< */
#define CY_U3P_SD_ACMD24_RESERVED                        (0x18)         /**< */
#define CY_U3P_SD_ACMD25_RESERVED                        (0x19)         /**< */
#define CY_U3P_SD_ACMD26_RESERVED                        (0x1A)         /**< */
#define CY_U3P_SD_ACMD38_RESERVED                        (0x26)         /**< */
#define CY_U3P_SD_ACMD39_RESERVED                        (0x27)         /**< */
#define CY_U3P_SD_ACMD40_RESERVED                        (0x28)         /**< */
#define CY_U3P_SD_ACMD41_SD_SEND_OP_COND                 (0x29)         /**< */
#define CY_U3P_SD_ACMD42_SET_CLR_CARD_DETECT             (0x2A)         /**< */
#define CY_U3P_SD_ACMD43_RESERVED                        (0x2B)         /**< */
#define CY_U3P_SD_ACMD49_RESERVED                        (0x31)         /**< */
#define CY_U3P_SD_ACMD51_SEND_SCR                        (0x33)         /**< */

/** \section SDMMC_Response SD and MMC card response length
    \brief These macros define constants denoting the length in bits of various
    response types defined in the SD and MMC specifications. These lengths are
    excluding the Start, direction and CRC bits.
 */

#define CY_U3P_SD_MMC_R1_RESP_BITS                       (0x26)
#define CY_U3P_SD_MMC_R1B_RESP_BITS                      (0x26)
#define CY_U3P_SD_MMC_R2_RESP_BITS                       (0x7E)
#define CY_U3P_SD_MMC_R3_RESP_BITS                       (0x26)
#define CY_U3P_SD_MMC_R4_RESP_BITS                       (0x26)
#define CY_U3P_SD_MMC_R5_RESP_BITS                       (0x26)
#define CY_U3P_SD_R6_RESP_BITS                           (0x26)
#define CY_U3P_SD_R7_RESP_BITS                           (0x26)

#define CY_U3P_SD_MMC_REQD_CMD_CLASS                     (0x0005) /* Classes 0, 2 are mandatory. */
#define CY_U3P_SD_MMC_WRITE_CMD_CLASS                    (0x0010) /* Class 4. */
#define CY_U3P_SD_MMC_ERASE_CMD_CLASS                    (0x0020) /* Class 5. */

/** @endcond */

/** \def CY_U3P_SD_REG_OCR_LEN
    \brief Length of the OCR register in bytes.
 */
#define CY_U3P_SD_REG_OCR_LEN                            (4)

/** \def CY_U3P_SD_REG_CID_LEN
    \brief Length of the CID register in bytes.
 */
#define CY_U3P_SD_REG_CID_LEN                            (16)

/** \def CY_U3P_SD_REG_CSD_LEN
    \brief Length of the CSD register in bytes.
 */
#define CY_U3P_SD_REG_CSD_LEN                            (16)

/** \def CY_U3P_SDIO_SUSPEND
    \brief SDIO operation code for function suspend.
 */
#define CY_U3P_SDIO_SUSPEND                              (0x0)

/** \def CY_U3P_SDIO_RESUME
    \brief SDIO operation code for function resume.
 */
#define CY_U3P_SDIO_RESUME                               (0x1)

/** \def CY_U3P_SDIO_READ
    \brief Operation code for read from SDIO card.
 */
#define CY_U3P_SDIO_READ                                 (0x0)

/** \def CY_U3P_SDIO_WRITE
    \brief Operation code for write to SDIO card.
 */
#define CY_U3P_SDIO_WRITE                                (0x1)

/** \def CY_U3P_SDIO_RW_BYTE_MODE
    \brief Flag that indicates that the SDIO transfer requested is a byte-mode request.
 */
#define CY_U3P_SDIO_RW_BYTE_MODE                         (0x0)

/** \def CY_U3P_SDIO_RW_BLOCK_MODE
    \brief Flag that indicates that the SDIO transfer request is a block-mode request.
 */
#define CY_U3P_SDIO_RW_BLOCK_MODE                        (0x1)

/** \def CY_U3P_SDIO_RW_ADDR_FIXED
    \brief Flag that indicates that the SDIO transfer should be performed with a fixed address.
 */
#define CY_U3P_SDIO_RW_ADDR_FIXED                        (0x0)

/** \def CY_U3P_SDIO_RW_ADDR_AUTO_INCREMENT
    \brief Flag that indicates that the address for the SDIO transfer should be auto incremented.
 */
#define CY_U3P_SDIO_RW_ADDR_AUTO_INCREMENT               (0x1)

#define CY_U3P_SDIO_REG_CCCR_REVISION                   (0x00)  /**< CCCR revision register address. */
#define CY_U3P_SDIO_REG_SD_SPEC_REVISION                (0x01)  /**< SD Spec revision register address. */
#define CY_U3P_SDIO_REG_IO_ENABLE                       (0x02)  /**< IO Enable register address. */
#define CY_U3P_SDIO_REG_IO_READY                        (0x03)  /**< IO ready register address. */
#define CY_U3P_SDIO_REG_IO_INTR_ENABLE                  (0x04)  /**< IO interrupt enable register address. */
#define CY_U3P_SDIO_REG_IO_INTR_PENDING                 (0x05)  /**< IO interrupt pending register address. */
#define CY_U3P_SDIO_REG_IO_ABORT                        (0x06)  /**< IO abort register address. */
#define CY_U3P_SDIO_REG_BUS_INTERFACE_CONTROL           (0x07)  /**< SDIO bus interface control register address. */
#define CY_U3P_SDIO_REG_CARD_CAPABILITY                 (0x08)  /**< SDIO card capability register address. */
#define CY_U3P_SDIO_REG_CIS_PTR_D0                      (0x09)  /**< CIS pointer: First byte. */
#define CY_U3P_SDIO_REG_CIS_PTR_D1                      (0x0A)  /**< CIS pointer: Second byte. */
#define CY_U3P_SDIO_REG_CIS_PTR_D2                      (0x0B)  /**< CIS pointer: Third byte. */
#define CY_U3P_SDIO_REG_BUS_SUSPEND                     (0x0C)  /**< SDIO bus suspend register address. */
#define CY_U3P_SDIO_REG_FUNCTION_SELECT                 (0x0D)  /**< Function select register address. */
#define CY_U3P_SDIO_REG_EXEC_FLAGS                      (0x0E)  /**< Exec flags register address. */
#define CY_U3P_SDIO_REG_READY_FLAGS                     (0x0F)  /**< Ready flags register address. */
#define CY_U3P_SDIO_REG_FUNCTION_BLOCKSIZE_D0           (0x10)  /**< Function 0 block size: First byte. */
#define CY_U3P_SDIO_REG_FUNCTION_BLOCKSIZE_D1           (0x11)  /**< Function 0 block size: Second byte. */
#define CY_U3P_SDIO_REG_POWER_CONTROL                   (0x12)  /**< Power control register address. */
#define CY_U3P_SDIO_REG_CCCR_HIGH_SPEED                 (0x13)  /**< Bus speed select register address. */
#define CY_U3P_SDIO_REG_UHS_I_SUPPORT                   (0x14)  /**< UHS-I support register address. */
#define CY_U3P_SDIO_REG_DRIVER_STRENGTH                 (0x15)  /**< Driver strength register address. */
#define CY_U3P_SDIO_REG_INTERRUPT_EXTENSION             (0x16)  /**< Interrupt extension register address. */


#define CY_U3P_SDIO_REG_FBR_INTERFACE_CODE              (0x00)  /**< Standard function interface code. */
#define CY_U3P_SDIO_REG_FBR_EXT_INTERFACE_CODE          (0x01)  /**< Extended SDIO function interface code. */
#define CY_U3P_SDIO_REG_FBR_POWER_SELECT                (0x02)  /**< Power selection. */
#define CY_U3P_SDIO_REG_FBR_CIS_PTR_DO                  CY_U3P_SDIO_REG_CIS_PTR_D0      /**< CIS Pointer. */
#define CY_U3P_SDIO_REG_FBR_CIS_PTR_D1                  CY_U3P_SDIO_REG_CIS_PTR_D1      /**< CIS Pointer. */
#define CY_U3P_SDIO_REG_FBR_CIS_PTR_D2                  CY_U3P_SDIO_REG_CIS_PTR_D2      /**< CIS Pointer. */
#define CY_U3P_SDIO_REG_FBR_CSA_PTR_DO                  (0x0C)  /**< CSA pointer. */
#define CY_U3P_SDIO_REG_FBR_CSA_PTR_D1                  (0x0D)  /**< CSA pointer. */
#define CY_U3P_SDIO_REG_FBR_CSA_PTR_D2                  (0x0E)  /**< CSA pointer. */
#define CY_U3P_SDIO_REG_FBR_DATA_ACCESS_WINDOW          (0x0F)  /**< Data access window to CSA. */
#define CY_U3P_SDIO_REG_FBR_IO_BLOCKSIZE_D0             CY_U3P_SDIO_REG_FUNCTION_BLOCKSIZE_D0   /**< Block size. */
#define CY_U3P_SDIO_REG_FBR_IO_BLOCKSIZE_D1             CY_U3P_SDIO_REG_FUNCTION_BLOCKSIZE_D1   /**< Block size. */

#define CY_U3P_SDIO_LOW_SPEED                           (0x00)  /**< Low speed. */
#define CY_U3P_SDIO_FULL_SPEED                          (0x01)  /**< Full speed. */
#define CY_U3P_SDIO_HIGH_SPEED                          (0x02)  /**< High speed. */
#define CY_U3P_SDIO_SSDR12_SPEED                        (0x04)  /**< SDR12. */
#define CY_U3P_SDIO_SSDR25_SPEED                        (0x10)  /**< SDR25. */
#define CY_U3P_SDIO_SSDR50_SPEED                        (0x20)  /**< SDR50. */
#define CY_U3P_SDIO_SSDR104_SPEED                       (0x40)  /**< SDR104. */
#define CY_U3P_SDIO_SDDR50_SPEED                        (0x80)  /**< DDR50. */

#define CY_U3P_SDIO_CARD_CAPABLITY_SDC                  (0x01)  /**< Support direct command. */
#define CY_U3P_SDIO_CARD_CAPABLITY_SMB                  (0x02)  /**< Support Multi-Block transfer. */
#define CY_U3P_SDIO_CARD_CAPABLITY_SRW                  (0x04)  /**< Support Read Wait. */
#define CY_U3P_SDIO_CARD_CAPABLITY_SBS                  (0x08)  /**< Support Bus Control. */
#define CY_U3P_SDIO_CARD_CAPABLITY_S4MI                 (0x10)  /**< Support Block Gap Interrupt. */
#define CY_U3P_SDIO_CARD_CAPABLITY_E4MI                 (0x20)  /**< Enable Block Gap Interrupt. */
#define CY_U3P_SDIO_CARD_CAPABLITY_LSC                  (0x40)  /**< Low Speed Card. */
#define CY_U3P_SDIO_CARD_CAPABLITY_4BLS                 (0x80)  /**< Low speed card with 4-bit support. */


#define CY_U3P_SDIO_READ_AFTER_WRITE                    (0x01)  /**< Read After Write flag for CMD52 */
#define CY_U3P_SDIO_SUPPORT_HIGH_SPEED                  (0x01)  /**< High Speed support flag. */
#define CY_U3P_SDIO_ENABLE_HIGH_SPEED                   (0x02)  /**< Enable high speed flag. */
#define CY_U3P_SDIO_RESET                               (0x08)  /**< SDIO reset flag. */
#define CY_U3P_SDIO_CIA_FUNCTION                        (0x00)  /**< Function number for SDIO CIA. */

#define CY_U3P_SDIO_CISTPL_NULL                         (0x00)  /**< Starting Tuple Code */
#define CY_U3P_SDIO_CISTPL_END                          (0xFF)  /**< Tuple Chain End */
#define CY_U3P_SDIO_CISTPL_MANFID                       (0x20)  /**< Manufacturer ID */
#define CY_U3P_SDIO_CISTPL_FUNCE                        (0x22)  /**< Function Extensions. */

#define CY_U3P_SDIO_CCCR_Version_1_00                   (0x00)  /**< CCCR version 1.00 */
#define CY_U3P_SDIO_CCCR_Version_1_10                   (0x01)  /**< CCCR version 1.10 */
#define CY_U3P_SDIO_CCCR_Version_2_00                   (0x02)  /**< CCCR version 2.00 */
#define CY_U3P_SDIO_CCCR_Version_3_00                   (0x03)  /**< CCCR version 3.00 */

#define CY_U3P_SDIO_Version_1_00                        (0x00)  /**< SDIO version 1.00 */
#define CY_U3P_SDIO_Version_1_10                        (0x01)  /**< SDIO version 1.10 */
#define CY_U3P_SDIO_Version_1_20                        (0x02)  /**< SDIO version 1.20 */
#define CY_U3P_SDIO_Version_2_00                        (0x03)  /**< SDIO version 2.00 */
#define CY_U3P_SDIO_Version_3_00                        (0x04)  /**< SDIO version 3.00 */

#define CY_U3P_SDIO_SD_Version_1_00                     (0x00)  /**< SD physical layer version 1.00 */
#define CY_U3P_SDIO_SD_Version_1_10                     (0x10)  /**< SD physical layer version 1.10 */
#define CY_U3P_SDIO_SD_Version_2_00                     (0x20)  /**< SD physical layer version 2.00 */
#define CY_U3P_SDIO_SD_Version_3_00                     (0x30)  /**< SD physical layer version 3.00 */

#define CY_U3P_SDIO_UHS_SSDR50                          (0x01)  /**< SDR50 support. */
#define CY_U3P_SDIO_UHS_SSDR104                         (0x02)  /**< SDR104 support. */
#define CY_U3P_SDIO_UHS_SDDR50                          (0x04)  /**< DDR50 support. */

#define CY_U3P_SDIO_SAI                                 (0x01)  /**< Support Asynchronous Interrupt. */
#define CY_U3P_SDIO_EAI                                 (0x02)  /**< Enable Asynchronous Interrupt. */

#define CY_U3P_SDIO_INTFC_NONE          (0x00)  /**< No standard Interface supported by the function */
#define CY_U3P_SDIO_INTFC_UART          (0x01)  /**< Function supports SDIO standard UART. */
#define CY_U3P_SDIO_INTFC_BT_A          (0x02)  /**< Function supports SDIO Bluetooth Type-A standard interface. */
#define CY_U3P_SDIO_INTFC_BT_B          (0x03)  /**< Function supports SDIO Bluetooth Type-B standard interface. */
#define CY_U3P_SDIO_INTFC_GPS           (0x04)  /**< Function supports SDIO GPS standard interface. */
#define CY_U3P_SDIO_INTFC_CAM           (0x05)  /**< Function supports SDIO Camera standard interface. */
#define CY_U3P_SDIO_INTFC_PHS           (0x06)  /**< Function supports SDIO PHS standard interface. */
#define CY_U3P_SDIO_INTFC_WLAN          (0x07)  /**< Function supports SDIO WLAN standard interface. */
#define CY_U3P_SDIO_INTFC_EMBD_ATA      (0x08)  /**< Function supports Embedded SDIO-ATA standard UART. */
#define CY_U3P_SDIO_INTFC_BT_A_AMP      (0x09)  /**< Function supports SDIO Bluetooth Type-A AMP standard interface. */

#define CY_U3P_SDIO_INT_MASTER                          (0x00)  /**< Master interrupt flag. */
#define CY_U3P_SDIO_DISABLE_INT                         (0x00)  /**< Master interrupt disabled. */
#define CY_U3P_SDIO_ENABLE_INT                          (0x01)  /**< Master interrupt enabled. */
#define CY_U3P_SDIO_CHECK_INT_ENABLE_REG                (0x02)  /**< Mask to apply on interrupt enable register. */
#define CY_U3P_SDIO_ENABLE_ASYNC_INT                    (0x03)  /**< SDIO asynchronous interrupt enable. */

/** \def CyU3PSdioInterruptBit
    \brief Macro to generate interrupt enable bit mask for a SDIO function.
 */
#define CyU3PSdioInterruptBit(funcNo)   ((0x01) << (funcNo))

/** \def CyU3PSdioCheckInterruptEnableStatus
    \brief Macro to check if interrupts are enabled or disabled in the returned data
    of the CyU3PSdioInterruptControl call
 */
#define CyU3PSdioCheckInterruptEnableStatus(funcNo, ienReg) ((CyU3PSdioInterruptBit(funcNo) & (ienReg)) ? (1) : (0))

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3CARDMGR_FX3S_H_ */

/*[]*/

