/*
 ## Cypress FX3 Core Library Header (pp_regs.h)
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

#ifndef _INCLUDED_P_PORT_H_
#define _INCLUDED_P_PORT_H_

/*
   P-Port Device ID Register
 */
#define CY_U3P_PP_ID_ADDRESS                                (0x00000080)
#define CY_U3P_PP_ID                                        (*(uvint32_t *)(0x00000080))
#define CY_U3P_PP_ID_DEFAULT                                (0x00000000)

/*
   Provides a device ID as defined in IROS.
 */
#define CY_U3P_DEVICE_ID_MASK                               (0x0000ffff) /* <0:15> RW:R:0:No */
#define CY_U3P_DEVICE_ID_POS                                (0)



/*
   P-Port reset and power control
 */
#define CY_U3P_PP_INIT_ADDRESS                              (0x00000081)
#define CY_U3P_PP_INIT                                      (*(uvint32_t *)(0x00000081))
#define CY_U3P_PP_INIT_DEFAULT                              (0x00000c01)

/*
   Indicates system woke up through a power-on-reset or RESET# pin reset
   sequence. If firmware does not clear this bit it will stay 1 even through
   software reset, standby and suspend sequences.  This bit is a shadow bit
   of GCTL_CONTROL.
 */
#define CY_U3P_POR                                          (1u << 0) /* <0:0> RW:R:1:No */


/*
   Indicates system woke up from a s/w induced hard reset sequence (from
   GCTL_CONTROL.HARD_RESET_N or PP_INIT.HARD_RESET_N).  If firmware does
   not clear this bit it will stay 1 even through standby and suspend sequences.
   This bit is a shadow bit of GCTL_CONTROL.
 */
#define CY_U3P_SW_RESET                                     (1u << 1) /* <1:1> RW:R:0:No */


/*
   Indicates system woke up from a watchdog timer induced hard reset (see
   GCTL_WATCHDOG_CS).  If firmware does not clear this bit it will stay 1
   even through standby and suspend sequences. This bit is a shadow bit of
   GCTL_CONTROL.
 */
#define CY_U3P_WDT_RESET                                    (1u << 2) /* <2:2> RW:R:0:No */


/*
   Indicates system woke up from standby mode (see architecture spec for
   details). If firmware does not clear this bit it will stay 1 even through
   suspend sequences. This bit is a shadow bit of GCTL_CONTROL.
 */
#define CY_U3P_WAKEUP_PWR                                   (1u << 3) /* <3:3> RW:R:0:No */


/*
   Indicates system woke up from suspend state (see architecture spec for
   details). If firmware does not clear this bit it will stay 1 even through
   standby sequences. This bit is a shadow bit of GCTL_CONTROL.
 */
#define CY_U3P_WAKEUP_CLK                                   (1u << 4) /* <4:4> RW:R:0:No */


/*
   Software clears this bit to effect a CPU reset (aka reboot).  No other
   blocks or registers are affected.  The CPU will enter the boot ROM, that
   will use the WARM_BOOT flag to determine whether to reload firmware.
   Unlike the same bit in GCTL_CONTROL, the software needs to explicitly
   clear and then set this bit to bring the internal CPU out of reset.  It
   is permissible to keep the ARM CPU in reset for an extended period of
   time (although not advisable).
 */
#define CY_U3P_CPU_RESET_N                                  (1u << 10) /* <10:10> R:RW:1:No */


/*
   Software clears this bit to effect a global hard reset (all blocks, all
   flops).  This is equivalent to toggling the RESET pin on the device. 
   This function is also available to internal firmware in GCTL_CONTROL.
 */
#define CY_U3P_HARD_RESET_N                                 (1u << 11) /* <11:11> R:RW0C:1:No */


/*
   0: P-Port is Little Endian
   1: P-Port is Big Endian
 */
#define CY_U3P_BIG_ENDIAN                                   (1u << 15) /* <15:15> R:RW:0:No */



/*
   P-Port Configuration Register
 */
#define CY_U3P_PP_CONFIG_ADDRESS                            (0x00000082)
#define CY_U3P_PP_CONFIG                                    (*(uvint32_t *)(0x00000082))
#define CY_U3P_PP_CONFIG_DEFAULT                            (0x0000004f)

/*
   Size of DMA bursts; only relevant when DRQMODE=1.
   0-14:  DMA burst size is 2BURSTSIZE words
   15: DMA burst size is infinite (DRQ de-asserts on last cycle of transfer)
 */
#define CY_U3P_BURSTSIZE_MASK                               (0x0000000f) /* <0:3> R:RW:15:No */
#define CY_U3P_BURSTSIZE_POS                                (0)


/*
   Initialization Mode
   0: Normal operation mode.
   1: Initialization mode.
   This bit is cleared to “0” by firmware, by writing 0 to PIB_CONFIG.PP_CFGMODE,
   after completing the initialization process.  Specific usage of this bit
   is described in the software architecure.  This bit is mirrored directly
   by HW in PIB_CONFIG.
 */
#define CY_U3P_CFGMODE                                      (1u << 6) /* <6:6> RW:RW:1:No */


/*
   DMA signaling mode. See DMA section for more information.
   0: Pulse mode, DRQ will de-assert when DACK de-asserts and will remain
   de-asserted for a specified time (see EROS).  After that DRQ may re-assert
   depending on other settings.
   1: Burst mode, DRQ will de-assert when BURSTSIZE words are transferred
   and will not re-assert until DACK is de-asserted.
 */
#define CY_U3P_DRQMODE                                      (1u << 7) /* <7:7> R:RW:0:No */


/*
   0: No override
   1: INTR signal is forced to INTR_VALUE
   This bit is used directly in HW to generate INT signal.
 */
#define CY_U3P_INTR_OVERRIDE                                (1u << 9) /* <9:9> R:RW:0:No */


/*
   0: INTR is de-asserted when INTR_OVERRIDE=1
   1: INTR is asserted on override when INTR_OVERRIDE=1
   This bit is used directly in HW to generate INT signal.
 */
#define CY_U3P_INTR_VALUE                                   (1u << 10) /* <10:10> R:RW:0:No */


/*
   0: INTR is active low
   1: INTR is active high
   This bit is used directly in HW to generate INT signal.
 */
#define CY_U3P_INTR_POLARITY                                (1u << 11) /* <11:11> R:RW:0:No */


/*
   0: No override
   1: DRQ signal is forced to DRQ_VALUE
 */
#define CY_U3P_DRQ_OVERRIDE                                 (1u << 12) /* <12:12> R:RW:0:No */


/*
   0: DRQ is de-asserted when DRQ_OVERRIDE=1
   1: DRQ is asserted when DRQ_OVERRIDE=1
 */
#define CY_U3P_DRQ_VALUE                                    (1u << 13) /* <13:13> R:RW:0:No */


/*
   0: DRQ is active low
   1: DRQ is active high
 */
#define CY_U3P_DRQ_POLARITY                                 (1u << 14) /* <14:14> R:RW:0:No */


/*
   0: DACK is active low
   1: DACK is active high
 */
#define CY_U3P_DACK_POLARITY                                (1u << 15) /* <15:15> R:RW:0:No */



/*
   P-port PMMC socket write config
 */
#define CY_U3P_PP_SOCK_WR_CONFIG0_ADDRESS                   (0x00000086)
#define CY_U3P_PP_SOCK_WR_CONFIG0                           (*(uvint32_t *)(0x00000086))
#define CY_U3P_PP_SOCK_WR_CONFIG0_DEFAULT                   (0x00000000)

/*
   Socket configuration, one bit per socketl. This config bit indicates which
   socket may send partially valid  data over fixed size blocks.
   0: All data in the block is forwarded
   1: Only DATA_SIZE bytes of a block are accepted during write operation.
   .For NoTA configuration the lower 16 bits are used
 */
#define CY_U3P_SOCK_WR_CFG_L_MASK                           (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_SOCK_WR_CFG_L_POS                            (0)



/*
   P-port PMMC socket write config
 */
#define CY_U3P_PP_SOCK_WR_CONFIG1_ADDRESS                   (0x00000087)
#define CY_U3P_PP_SOCK_WR_CONFIG1                           (*(uvint32_t *)(0x00000087))
#define CY_U3P_PP_SOCK_WR_CONFIG1_DEFAULT                   (0x00000000)

/*
   Socket configuration, one bit per socketl. This config bit indicates which
   socket may send partially valid  data over fixed size blocks.
   0: All data in the block is forwarded
   1: Only DATA_SIZE bytes of a block are accepted during write operation.
   .For NoTA configuration the lower 16 bits are used
 */
#define CY_U3P_SOCK_WR_CFG_H_MASK                           (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_SOCK_WR_CFG_H_POS                            (0)



/*
   P-Port Interrupt Mask Register
 */
#define CY_U3P_PP_INTR_MASK_ADDRESS                         (0x00000088)
#define CY_U3P_PP_INTR_MASK                                 (*(uvint32_t *)(0x00000088))
#define CY_U3P_PP_INTR_MASK_DEFAULT                         (0x00002000)

/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_SOCK_AGG_AL                                  (1u << 0) /* <0:0> R:RW:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_SOCK_AGG_AH                                  (1u << 1) /* <1:1> R:RW:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_SOCK_AGG_BL                                  (1u << 2) /* <2:2> R:RW:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_SOCK_AGG_BH                                  (1u << 3) /* <3:3> R:RW:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_GPIF_INT                                     (1u << 4) /* <4:4> R:RW:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_PIB_ERR                                      (1u << 5) /* <5:5> R:RW:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_MMC_ERR                                      (1u << 6) /* <6:6> R:RW:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_GPIF_ERR                                     (1u << 7) /* <7:7> R:RW:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_DMA_WMARK_EV                                 (1u << 11) /* <11:11> R:RW:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_DMA_READY_EV                                 (1u << 12) /* <12:12> R:RW:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_RD_MB_FULL                                   (1u << 13) /* <13:13> R:RW:1:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_WR_MB_EMPTY                                  (1u << 14) /* <14:14> R:RW:0:No */


/*
   1: Forward EVENT onto INT line
 */
#define CY_U3P_WAKEUP                                       (1u << 15) /* <15:15> R:RW:0:No */



/*
   P-Port DRQ/R5 Mask Register
 */
#define CY_U3P_PP_DRQR5_MASK_ADDRESS                        (0x00000089)
#define CY_U3P_PP_DRQR5_MASK                                (*(uvint32_t *)(0x00000089))
#define CY_U3P_PP_DRQR5_MASK_DEFAULT                        (0x00002000)

/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_SOCK_AGG_AL                                  (1u << 0) /* <0:0> R:RW:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_SOCK_AGG_AH                                  (1u << 1) /* <1:1> R:RW:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_SOCK_AGG_BL                                  (1u << 2) /* <2:2> R:RW:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_SOCK_AGG_BH                                  (1u << 3) /* <3:3> R:RW:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_GPIF_INT                                     (1u << 4) /* <4:4> R:RW:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_PIB_ERR                                      (1u << 5) /* <5:5> R:RW:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_MMC_ERR                                      (1u << 6) /* <6:6> R:RW:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_GPIF_ERR                                     (1u << 7) /* <7:7> R:RW:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_DMA_WMARK_EV                                 (1u << 11) /* <11:11> R:RW:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_DMA_READY_EV                                 (1u << 12) /* <12:12> R:RW:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_RD_MB_FULL                                   (1u << 13) /* <13:13> R:RW:1:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_WR_MB_EMPTY                                  (1u << 14) /* <14:14> R:RW:0:No */


/*
   1: Forward EVENT onto DRQ line
 */
#define CY_U3P_WAKEUP                                       (1u << 15) /* <15:15> R:RW:0:No */



/*
   P-Port Socket Mask Register
 */
#define CY_U3P_PP_SOCK_MASK0_ADDRESS                        (0x0000008a)
#define CY_U3P_PP_SOCK_MASK0                                (*(uvint32_t *)(0x0000008a))
#define CY_U3P_PP_SOCK_MASK0_DEFAULT                        (0x00000000)

/*
   For socket <x>, bit <x> indicates:
   0: Socket does not affect SOCK_AGG_A/B
   1: Socket does affect SOCK_AGG_A/B
 */
#define CY_U3P_SOCK_MASK_L_MASK                             (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_SOCK_MASK_L_POS                              (0)



/*
   P-Port Socket Mask Register
 */
#define CY_U3P_PP_SOCK_MASK1_ADDRESS                        (0x0000008b)
#define CY_U3P_PP_SOCK_MASK1                                (*(uvint32_t *)(0x0000008b))
#define CY_U3P_PP_SOCK_MASK1_DEFAULT                        (0x00000000)

/*
   For socket <x>, bit <x> indicates:
   0: Socket does not affect SOCK_AGG_A/B
   1: Socket does affect SOCK_AGG_A/B
 */
#define CY_U3P_SOCK_MASK_H_MASK                             (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_SOCK_MASK_H_POS                              (0)



/*
   P-Port Error Indicator Register
 */
#define CY_U3P_PP_ERROR_ADDRESS                             (0x0000008c)
#define CY_U3P_PP_ERROR                                     (*(uvint32_t *)(0x0000008c))
#define CY_U3P_PP_ERROR_DEFAULT                             (0x00000000)

/*
   Mirror of corresponding field in PIB_ERROR
 */
#define CY_U3P_PIB_ERR_CODE_MASK                            (0x0000003f) /* <0:5> RW:R:0:No */
#define CY_U3P_PIB_ERR_CODE_POS                             (0)


/*
   Mirror of corresponding field in PIB_ERROR
 */
#define CY_U3P_MMC_ERR_CODE_MASK                            (0x000003c0) /* <6:9> RW:R:0:No */
#define CY_U3P_MMC_ERR_CODE_POS                             (6)


/*
   Mirror of corresponding field in PIB_ERROR
 */
#define CY_U3P_GPIF_ERR_CODE_MASK                           (0x00007c00) /* <10:14> RW:R:0:No */
#define CY_U3P_GPIF_ERR_CODE_POS                            (10)



/*
   P-Port DMA Transfer Register
 */
#define CY_U3P_PP_DMA_XFER_ADDRESS                          (0x0000008e)
#define CY_U3P_PP_DMA_XFER                                  (*(uvint32_t *)(0x0000008e))
#define CY_U3P_PP_DMA_XFER_DEFAULT                          (0x00000000)

/*
   Processor specified socket number for data transfer
 */
#define CY_U3P_DMA_SOCK_MASK                                (0x000000ff) /* <0:7> R:RW:0:No */
#define CY_U3P_DMA_SOCK_POS                                 (0)


/*
   0: Disable ongoing transfer. If no transfer is ongoing ignore disable
   1: Enable data transfer
 */
#define CY_U3P_DMA_ENABLE                                   (1u << 8) /* <8:8> RW:RW:0:No */


/*
   0: Read (Transfer from FX3 - Egress direction)
   1: Write (Transfer to FX3 - Ingress direction)
 */
#define CY_U3P_DMA_DIRECTION                                (1u << 9) /* <9:9> R:RW:0:No */


/*
   0: Short Transfer (DMA_ENABLE clears at end of buffer
   1: Long Transfer (DMA_ENABLE must be cleared by AP at end of transfer)
 */
#define CY_U3P_LONG_TRANSFER                                (1u << 10) /* <10:10> R:RW:0:No */


/*
   Indicates that DMA_SIZE value is valid and corresponds to the socket selected
   in PP_DMA_XFER.  SIZE_VALID will be 0 for a short period after PP_DMA_XFER
   is written into.  AP shall poll SIZE_VALID or DMA_READY before reading
   DMA_SIZE
 */
#define CY_U3P_SIZE_VALID                                   (1u << 12) /* <12:12> RW:R:0:No */


/*
   Indicates that link controller is busy processing a transfer. A zero length
   transfer would cause DMA_READY to never assert.
   0: No DMA is in progress
   1: DMA is busy
 */
#define CY_U3P_DMA_BUSY                                     (1u << 13) /* <13:13> W:R:0:No */


/*
   0: No errors
   1: DMA transfer error
   This bit is set when a DMA error occurs and cleared when the next transfer
   is started using DMA_ENABLE=1.
 */
#define CY_U3P_DMA_ERROR                                    (1u << 14) /* <14:14> W:R:0:No */


/*
   Indicates that the link controller is ready to exchange data.
   0: Socket not ready for transfer
   1: Socket ready for transfer; SIZE_VALID is also guaranteed 1
 */
#define CY_U3P_DMA_READY                                    (1u << 15) /* <15:15> W:R:0:No */



/*
   P-Port DMA Transfer Size Register
 */
#define CY_U3P_PP_DMA_SIZE_ADDRESS                          (0x0000008f)
#define CY_U3P_PP_DMA_SIZE                                  (*(uvint32_t *)(0x0000008f))
#define CY_U3P_PP_DMA_SIZE_DEFAULT                          (0x00000000)

/*
   Size of DMA transfer. Number of bytes available for read/write when read,
   number of bytes to be read/written when written.
 */
#define CY_U3P_DMA_SIZE_MASK                                (0x0000ffff) /* <0:15> RW:RW:0:No */
#define CY_U3P_DMA_SIZE_POS                                 (0)



/*
   P-Port Write (Ingress) Mailbox Registers
 */
#define CY_U3P_PP_WR_MAILBOX0_ADDRESS                       (0x00000090)
#define CY_U3P_PP_WR_MAILBOX0                               (*(uvint32_t *)(0x00000090))
#define CY_U3P_PP_WR_MAILBOX0_DEFAULT                       (0x00000000)

/*
   Write mailbox message from AP
 */
#define CY_U3P_WR_MAILBOX_L_MASK                            (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_WR_MAILBOX_L_POS                             (0)



/*
   P-Port Write (Ingress) Mailbox Registers
 */
#define CY_U3P_PP_WR_MAILBOX1_ADDRESS                       (0x00000091)
#define CY_U3P_PP_WR_MAILBOX1                               (*(uvint32_t *)(0x00000091))
#define CY_U3P_PP_WR_MAILBOX1_DEFAULT                       (0x00000000)

/*
   Write mailbox message from AP
 */
#define CY_U3P_WR_MAILBOX_H_L_MASK                          (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_WR_MAILBOX_H_L_POS                           (0)



/*
   P-Port Write (Ingress) Mailbox Registers
 */
#define CY_U3P_PP_WR_MAILBOX2_ADDRESS                       (0x00000092)
#define CY_U3P_PP_WR_MAILBOX2                               (*(uvint32_t *)(0x00000092))
#define CY_U3P_PP_WR_MAILBOX2_DEFAULT                       (0x00000000)

/*
   Write mailbox message from AP
 */
#define CY_U3P_WR_MAILBOX_H_L_MASK                          (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_WR_MAILBOX_H_L_POS                           (0)



/*
   P-Port Write (Ingress) Mailbox Registers
 */
#define CY_U3P_PP_WR_MAILBOX3_ADDRESS                       (0x00000093)
#define CY_U3P_PP_WR_MAILBOX3                               (*(uvint32_t *)(0x00000093))
#define CY_U3P_PP_WR_MAILBOX3_DEFAULT                       (0x00000000)

/*
   Write mailbox message from AP
 */
#define CY_U3P_WR_MAILBOX_H_MASK                            (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_WR_MAILBOX_H_POS                             (0)



/*
   P-Port MMIO Address Registers
 */
#define CY_U3P_PP_MMIO_ADDR0_ADDRESS                        (0x00000094)
#define CY_U3P_PP_MMIO_ADDR0                                (*(uvint32_t *)(0x00000094))
#define CY_U3P_PP_MMIO_ADDR0_DEFAULT                        (0x00000000)

/*
   Address in MMIO register space to be used for access.
 */
#define CY_U3P_MMIO_ADDR_L_MASK                             (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_MMIO_ADDR_L_POS                              (0)



/*
   P-Port MMIO Address Registers
 */
#define CY_U3P_PP_MMIO_ADDR1_ADDRESS                        (0x00000095)
#define CY_U3P_PP_MMIO_ADDR1                                (*(uvint32_t *)(0x00000095))
#define CY_U3P_PP_MMIO_ADDR1_DEFAULT                        (0x00000000)

/*
   Address in MMIO register space to be used for access.
 */
#define CY_U3P_MMIO_ADDR_H_MASK                             (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_MMIO_ADDR_H_POS                              (0)



/*
   P-Port MMIO Data Registers
 */
#define CY_U3P_PP_MMIO_DATA0_ADDRESS                        (0x00000096)
#define CY_U3P_PP_MMIO_DATA0                                (*(uvint32_t *)(0x00000096))
#define CY_U3P_PP_MMIO_DATA0_DEFAULT                        (0x00000000)

/*
   32-bit data word for read or write transaction
 */
#define CY_U3P_MMIO_DATA_L_MASK                             (0x0000ffff) /* <0:15> RW:RW:0:No */
#define CY_U3P_MMIO_DATA_L_POS                              (0)



/*
   P-Port MMIO Data Registers
 */
#define CY_U3P_PP_MMIO_DATA1_ADDRESS                        (0x00000097)
#define CY_U3P_PP_MMIO_DATA1                                (*(uvint32_t *)(0x00000097))
#define CY_U3P_PP_MMIO_DATA1_DEFAULT                        (0x00000000)

/*
   32-bit data word for read or write transaction
 */
#define CY_U3P_MMIO_DATA_H_MASK                             (0x0000ffff) /* <0:15> RW:RW:0:No */
#define CY_U3P_MMIO_DATA_H_POS                              (0)



/*
   P-Port MMIO Control Registers
 */
#define CY_U3P_PP_MMIO_ADDRESS                              (0x00000098)
#define CY_U3P_PP_MMIO                                      (*(uvint32_t *)(0x00000098))
#define CY_U3P_PP_MMIO_DEFAULT                              (0x00000000)

/*
   0: No action
   1: Initiate read from MMIO_ADDR, place data in MMIO_DATA
 */
#define CY_U3P_MMIO_RD                                      (1u << 0) /* <0:0> RW0C:RW1S:0:No */


/*
   0: No action
   1: Initiate write of MMIO_DATA to MMIO_ADDR
 */
#define CY_U3P_MMIO_WR                                      (1u << 1) /* <1:1> RW0C:RW1S:0:No */


/*
   0: No MMIO operation is pending
   1: MMIO operation is being executed
 */
#define CY_U3P_MMIO_BUSY                                    (1u << 2) /* <2:2> RW:R:0:No */


/*
   0: Normal
   1: MMIO operation was not executed because of security contraints (PIB_CONFIG.MMIO_ENABLE=0)
 */
#define CY_U3P_MMIO_FAIL                                    (1u << 3) /* <3:3> RW:R:0:No */



/*
   P-Port Event Register
 */
#define CY_U3P_PP_EVENT_ADDRESS                             (0x00000099)
#define CY_U3P_PP_EVENT                                     (*(uvint32_t *)(0x00000099))
#define CY_U3P_PP_EVENT_DEFAULT                             (0x00004000)

/*
   0 : SOCK_STAT_A[7:0] is all zeroes
   1: At least one bit set in SOCK_STAT_A[7:0]
 */
#define CY_U3P_SOCK_AGG_AL                                  (1u << 0) /* <0:0> RW:R:0:No */


/*
   0 : SOCK_STAT_A[15:8] is all zeroes
   1: At least one bit set in SOCK_STAT_A[15:8]
 */
#define CY_U3P_SOCK_AGG_AH                                  (1u << 1) /* <1:1> RW:R:0:No */


/*
   0 : SOCK_STAT_B[7:0] is all zeroes
   1: At least one bit set in SOCK_STAT_B[7:0]
 */
#define CY_U3P_SOCK_AGG_BL                                  (1u << 2) /* <2:2> RW:R:0:No */


/*
   0 : SOCK_STAT_B[15:8] is all zeroes
   1: At least one bit set in SOCK_STAT_B[15:8]
 */
#define CY_U3P_SOCK_AGG_BH                                  (1u << 3) /* <3:3> RW:R:0:No */


/*
   1: State machine raised host interrupt
 */
#define CY_U3P_GPIF_INT                                     (1u << 4) /* <4:4> W1S:RW1C:0:No */


/*
   The socket based link controller encountered an error and needs attention.
    FW clears this bit after handling the error.  The error code is indicated
   in PP_ERROR.PIB_ERR_CODE
 */
#define CY_U3P_PIB_ERR                                      (1u << 5) /* <5:5> W1S:RW1C:0:No */


/*
   An unrecoverable error occurred in the PMMC controller. FW clears this
   bit after handling the eror. The error code is indicated in PP_ERROR.MMC_ERR_CODE
 */
#define CY_U3P_MMC_ERR                                      (1u << 6) /* <6:6> W1S:RW1C:0:No */


/*
   An error occurred in the GPIF.  FW clears this bit after handling the
   error.  The error code is indicated in PP_ERROR.GPIF_ERR_CODE
 */
#define CY_U3P_GPIF_ERR                                     (1u << 7) /* <7:7> W1S:RW1C:0:No */


/*
   Usage of DMA_WMARK is explained in PAS.
   0: P-Port has fewer than <watermark> words left (can be 0)
   1: P-Port is ready for transfer and at least <watermark> words remain
 */
#define CY_U3P_DMA_WMARK_EV                                 (1u << 11) /* <11:11> RW:R:0:No */


/*
   Usage of DMA_READY is explained in PAS.
   0: P-port not ready for data transfer
   1: P-port ready for data transfer
 */
#define CY_U3P_DMA_READY_EV                                 (1u << 12) /* <12:12> RW:R:0:No */


/*
   1: RD Mailbox is full - message must be read
 */
#define CY_U3P_RD_MB_FULL                                   (1u << 13) /* <13:13> W1S:RW1C:0:Yes */


/*
   1: WR Mailbox is empty - message can be written
   This field is cleared by PIB when message is written to MBX, but can also
   be cleared by AP when used as interrupt.  This field is set by PIB only
   once when MBX is emptied by firmware.
 */
#define CY_U3P_WR_MB_EMPTY                                  (1u << 14) /* <14:14> RW:RW1C:1:Yes */


/*
   0: No wakeup event
   1: FX3 returned from standby mode, signal must be cleared by software
 */
#define CY_U3P_WAKEUP                                       (1u << 15) /* <15:15> W1S:RW1C:0:Yes */



/*
   P-Port Read (Egress) Mailbox Registers
 */
#define CY_U3P_PP_RD_MAILBOX0_ADDRESS                       (0x0000009a)
#define CY_U3P_PP_RD_MAILBOX0                               (*(uvint32_t *)(0x0000009a))
#define CY_U3P_PP_RD_MAILBOX0_DEFAULT                       (0x00000000)

/*
   Read mailbox message to AP
 */
#define CY_U3P_RD_MAILBOX_L_MASK                            (0x0000ffff) /* <0:15> RW:R:0:No */
#define CY_U3P_RD_MAILBOX_L_POS                             (0)



/*
   P-Port Read (Egress) Mailbox Registers
 */
#define CY_U3P_PP_RD_MAILBOX1_ADDRESS                       (0x0000009b)
#define CY_U3P_PP_RD_MAILBOX1                               (*(uvint32_t *)(0x0000009b))
#define CY_U3P_PP_RD_MAILBOX1_DEFAULT                       (0x00000000)

/*
   Read mailbox message to AP
 */
#define CY_U3P_RD_MAILBOX_H_L_MASK                          (0x0000ffff) /* <0:15> RW:R:0:No */
#define CY_U3P_RD_MAILBOX_H_L_POS                           (0)



/*
   P-Port Read (Egress) Mailbox Registers
 */
#define CY_U3P_PP_RD_MAILBOX2_ADDRESS                       (0x0000009c)
#define CY_U3P_PP_RD_MAILBOX2                               (*(uvint32_t *)(0x0000009c))
#define CY_U3P_PP_RD_MAILBOX2_DEFAULT                       (0x00000000)

/*
   Read mailbox message to AP
 */
#define CY_U3P_RD_MAILBOX_H_L_MASK                          (0x0000ffff) /* <0:15> RW:R:0:No */
#define CY_U3P_RD_MAILBOX_H_L_POS                           (0)



/*
   P-Port Read (Egress) Mailbox Registers
 */
#define CY_U3P_PP_RD_MAILBOX3_ADDRESS                       (0x0000009d)
#define CY_U3P_PP_RD_MAILBOX3                               (*(uvint32_t *)(0x0000009d))
#define CY_U3P_PP_RD_MAILBOX3_DEFAULT                       (0x00000000)

/*
   Read mailbox message to AP
 */
#define CY_U3P_RD_MAILBOX_H_MASK                            (0x0000ffff) /* <0:15> RW:R:0:No */
#define CY_U3P_RD_MAILBOX_H_POS                             (0)



/*
   P-Port Socket Status Register
 */
#define CY_U3P_PP_SOCK_STAT0_ADDRESS                        (0x0000009e)
#define CY_U3P_PP_SOCK_STAT0                                (*(uvint32_t *)(0x0000009e))
#define CY_U3P_PP_SOCK_STAT0_DEFAULT                        (0x00000000)

/*
   For socket <x>, bit <x> indicates:
   0: Socket has no active descriptor or descriptor is not available (empty
   for write, occupied for read)
   1: Socket is available for reading or writing
 */
#define CY_U3P_SOCK_STAT_L_MASK                             (0x0000ffff) /* <0:15> W:R:0:No */
#define CY_U3P_SOCK_STAT_L_POS                              (0)



/*
   P-Port Socket Status Register
 */
#define CY_U3P_PP_SOCK_STAT1_ADDRESS                        (0x0000009f)
#define CY_U3P_PP_SOCK_STAT1                                (*(uvint32_t *)(0x0000009f))
#define CY_U3P_PP_SOCK_STAT1_DEFAULT                        (0x00000000)

/*
   For socket <x>, bit <x> indicates:
   0: Socket has no active descriptor or descriptor is not available (empty
   for write, occupied for read)
   1: Socket is available for reading or writing
 */
#define CY_U3P_SOCK_STAT_H_MASK                             (0x0000ffff) /* <0:15> W:R:0:No */
#define CY_U3P_SOCK_STAT_H_POS                              (0)



#endif /* _INCLUDED_P_PORT_H_ */

/*[]*/

