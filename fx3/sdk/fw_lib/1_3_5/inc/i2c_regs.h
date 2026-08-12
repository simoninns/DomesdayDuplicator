/*
 ## Cypress FX3 Core Library Header (i2c_regs.h)
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

#ifndef _INCLUDED_I2C_REGS_H_
#define _INCLUDED_I2C_REGS_H_

#include <cyu3types.h>

#define I2C_BASE_ADDR                            (0xe0000400)

typedef struct
{
    uvint32_t lpp_i2c_config;                     /* 0xe0000400 */
    uvint32_t lpp_i2c_status;                     /* 0xe0000404 */
    uvint32_t lpp_i2c_intr;                       /* 0xe0000408 */
    uvint32_t lpp_i2c_intr_mask;                  /* 0xe000040c */
    uvint32_t lpp_i2c_timeout;                    /* 0xe0000410 */
    uvint32_t lpp_i2c_dma_timeout;                /* 0xe0000414 */
    uvint32_t lpp_i2c_preamble_ctrl;              /* 0xe0000418 */
    uvint32_t lpp_i2c_preamble_data0;             /* 0xe000041c */
    uvint32_t lpp_i2c_preamble_data1;             /* 0xe0000420 */
    uvint32_t lpp_i2c_preamble_rpt;               /* 0xe0000424 */
    uvint32_t lpp_i2c_command;                    /* 0xe0000428 */
    uvint32_t lpp_i2c_egress_data;                /* 0xe000042c */
    uvint32_t lpp_i2c_ingress_data;               /* 0xe0000430 */
    uvint32_t lpp_i2c_clock_low_count;            /* 0xe0000434 */
    uvint32_t lpp_i2c_byte_count;                 /* 0xe0000438 */
    uvint32_t lpp_i2c_bytes_transferred;          /* 0xe000043c */
    uvint32_t lpp_i2c_socket;                     /* 0xe0000440 */
    uvint32_t rsrvd0[235];
    uvint32_t lpp_i2c_id;                         /* 0xe00007f0 */
    uvint32_t lpp_i2c_power;                      /* 0xe00007f4 */
} I2C_REGS_T, *PI2C_REGS_T;

#define I2C        ((PI2C_REGS_T) I2C_BASE_ADDR)


/*
   I2C Cofiguration and Mode Register
 */
#define CY_U3P_LPP_I2C_CONFIG_ADDRESS                       (0xe0000400)
#define CY_U3P_LPP_I2C_CONFIG                               (*(uvint32_t *)(0xe0000400))
#define CY_U3P_LPP_I2C_CONFIG_DEFAULT                       (0x00000004)

/*
   0: Register-based transfers
   1: DMA-based transfers
 */
#define CY_U3P_LPP_I2C_DMA_MODE                             (1u << 0) /* <0:0> R:RW:0:No */


/*
   1: Continue trsnamission even if NACK is received. It is strongly advised
   to use this bit for debugging purposes only. This bit is overridder in
   preamble repeat feature.
 */
#define CY_U3P_LPP_I2C_CONTINUE_ON_NACK                     (1u << 1) /* <1:1> R:RW:0:No */


/*
   1: I2C is in 100KHz mode, use 5:5 ratio.
   0: Other speeds, use 4:6 High:Low ratio while generating SCLK from 10X
   clock.
 */
#define CY_U3P_LPP_I2C_I2C_100KHZ                           (1u << 2) /* <2:2> R:RW:1:No */


/*
   0: Do nothing
   1: Clear receive FIFO
   (s/w must wait for RX_DATA=0 before clearing this bit again)
 */
#define CY_U3P_LPP_I2C_RX_CLEAR                             (1u << 29) /* <29:29> R:RW:0:No */


/*
   Use only when ENABLE=0; behavior undefined when ENABLE=1
   0: Do nothing
   1: Clear transmit FIFO
   (once TX_CLEAR is set, s/w must wait for TX_DONE before clearing it)
 */
#define CY_U3P_LPP_I2C_TX_CLEAR                             (1u << 30) /* <30:30> R:RW:0:Yes */


/*
   Enable block here, but only after all the configuration is set.  Do not
   set this bit to 1 while changing any other configuration value in this
   register. This bit will be synchronized to the core clock. Disabling blocks
   resets all  I2C controller state machine and stops all transfers at the
   end of current byte. When DMA_MODE=1, data hanging in the transmit pipeline
   may be lost. Any unread data in the ingress data register is lost.
 */
#define CY_U3P_LPP_I2C_ENABLE                               (1u << 31) /* <31:31> R:RW:0:Yes */



/*
   I2C Status Register
 */
#define CY_U3P_LPP_I2C_STATUS_ADDRESS                       (0xe0000404)
#define CY_U3P_LPP_I2C_STATUS                               (*(uvint32_t *)(0xe0000404))
#define CY_U3P_LPP_I2C_STATUS_DEFAULT                       (0x0f000030)

/*
   Indicates receive operation completed.  Non sticky, Does not need software
   intervention to clear it.
 */
#define CY_U3P_LPP_I2C_RX_DONE                              (1u << 0) /* <0:0> W:R:0:Yes */


/*
   Indicates data is available in the RX FIFO (only relevant when DMA_MODE=0).
   This bit is updated immediately after reads from INGRESS_DATA register.
   Non sticky
 */
#define CY_U3P_LPP_I2C_RX_DATA                              (1u << 1) /* <1:1> W:R:0:No */


/*
   Indicates that the RX FIFO is at least half full (only relevant when DMA_MODE=0).
   This bit can be used to create burst based interrupts. This bit is updated
   immediately after reads from INGRESS_DATA register. Non sticky
 */
#define CY_U3P_LPP_I2C_RX_HALF                              (1u << 2) /* <2:2> W:R:0:No */


/*
   Indicates no more data is available for transmission. Non sticky.
   If DMA_MODE=0 this is defined as TX FIFO empty and shift register empty.
    If DMA_MODE=1 this is defined as BYTES_TARNSFERRED=BYTE_COUNT and shift
   register empty. Note that this field will only assert after a transmission
   was started - it's power up state is 0.
 */
#define CY_U3P_LPP_I2C_TX_DONE                              (1u << 3) /* <3:3> W:R:0:Yes */


/*
   Indicates space is available in the TX FIFO. This bit is updated immediately
   after writes to EGRESS_DATA register. Non sticky.
 */
#define CY_U3P_LPP_I2C_TX_SPACE                             (1u << 4) /* <4:4> W:R:1:No */


/*
   Indicates that the TX FIFO is at least half empty.  This bit can be used
   to create burst-based interrupts.  This bit is updated immediately after
   writes to EGRESS_DATA register. Non sticky.
 */
#define CY_U3P_LPP_I2C_TX_HALF                              (1u << 5) /* <5:5> W:R:1:No */


/*
   An I2C bus timeout occurred (see I2C_TIMEOUT register). Sticky
 */
#define CY_U3P_LPP_I2C_TIMEOUT                              (1u << 6) /* <6:6> RW1S:RW1C:0:Yes */


/*
   Master lost arbitration during command.  S/W is reponsible for resetting
   socket (in DMA_MODE) and re-issueing command. Sticky
 */
#define CY_U3P_LPP_I2C_LOST_ARBITRATION                     (1u << 7) /* <7:7> RW1S:RW1C:0:Yes */


/*
   An internal error has occurred with cause ERROR_CODE.  Must be cleared
   by software. Sticky
 */
#define CY_U3P_LPP_I2C_ERROR                                (1u << 8) /* <8:8> RW1S:RW1C:0:Yes */


/*
   Error code, only relevant when ERROR=1. ERROR codes 0 through 7 are relevent
   for TIMEOUT as well and are used to pin-point when timeout happened (at
   pre-amble byte X). ERROR logs only the FIRST error to occur and will never
   change value as long as ERROR=1. Values detailed in BROS
   0-7: Slave NAK-ed the corresponding byte in the preamble.
   8: Slave NAK-ed in data phase.
   9: Premable Repeat exited due to NACK or ACK.
   10: Preamble repeat-count reached without satisfying NACK.ACK conditions.
   11: TX Underflow
   12: Write to TX FIFO when FIFO full
   13: Read from RX FIFO when FIFO empty
   14: RX Overflow
   15: No error
 */
#define CY_U3P_LPP_I2C_ERROR_CODE_MASK                      (0x0f000000) /* <24:27> W:R:0xF:No */
#define CY_U3P_LPP_I2C_ERROR_CODE_POS                       (24)


/*
   Indicates the block is busy transmitting data.  This field may remain
   asserted after the block is suspended and must be polled before changing
   any configuration values.
 */
#define CY_U3P_LPP_I2C_BUSY                                 (1u << 28) /* <28:28> W:R:0:Yes */


/*
   Asserts when the block has detected that it cannot start an operation
   (TX/RX) since the bus is kept busy by another master. De-asserts and resets
   when a stop condition is detected or when the block is disabled.Please
   note that the block may not be able to
 */
#define CY_U3P_LPP_I2C_BUS_BUSY                             (1u << 29) /* <29:29> W:R:0:Yes */


/*
   Status of the SCL line.
 */
#define CY_U3P_LPP_I2C_SCL_STAT                             (1u << 30) /* <30:30> W:R:0:Yes */


/*
   Status of the SDA line.
 */
#define CY_U3P_LPP_I2C_SDA_STAT                             (1u << 31) /* <31:31> W:R:0:Yes */



/*
   I2C Interrupt Request Register
 */
#define CY_U3P_LPP_I2C_INTR_ADDRESS                         (0xe0000408)
#define CY_U3P_LPP_I2C_INTR                                 (*(uvint32_t *)(0xe0000408))
#define CY_U3P_LPP_I2C_INTR_DEFAULT                         (0x00000000)

/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2C_RX_DONE                              (1u << 0) /* <0:0> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2C_RX_DATA                              (1u << 1) /* <1:1> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2C_RX_HALF                              (1u << 2) /* <2:2> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2C_TX_DONE                              (1u << 3) /* <3:3> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2C_TX_SPACE                             (1u << 4) /* <4:4> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2C_TX_HALF                              (1u << 5) /* <5:5> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2C_TIMEOUT                              (1u << 6) /* <6:6> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2C_LOST_ARBITRATION                     (1u << 7) /* <7:7> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2C_ERROR                                (1u << 8) /* <8:8> RW1S:RW1C:0:No */



/*
   I2C Interrupt Mask Register
 */
#define CY_U3P_LPP_I2C_INTR_MASK_ADDRESS                    (0xe000040c)
#define CY_U3P_LPP_I2C_INTR_MASK                            (*(uvint32_t *)(0xe000040c))
#define CY_U3P_LPP_I2C_INTR_MASK_DEFAULT                    (0x00000000)

/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2C_RX_DONE                              (1u << 0) /* <0:0> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2C_RX_DATA                              (1u << 1) /* <1:1> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2C_RX_HALF                              (1u << 2) /* <2:2> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2C_TX_DONE                              (1u << 3) /* <3:3> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2C_TX_SPACE                             (1u << 4) /* <4:4> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2C_TX_HALF                              (1u << 5) /* <5:5> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2C_TIMEOUT                              (1u << 6) /* <6:6> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2C_LOST_ARBITRATION                     (1u << 7) /* <7:7> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2C_ERROR                                (1u << 8) /* <8:8> R:RW:0:No */



/*
   Timeout Register
 */
#define CY_U3P_LPP_I2C_TIMEOUT_REG_ADDRESS                  (0xe0000410)
#define CY_U3P_LPP_I2C_TIMEOUT_REG                          (*(uvint32_t *)(0xe0000410))
#define CY_U3P_LPP_I2C_TIMEOUT_REG_DEFAULT                  (0xffffffff)

/*
   Number of core clocks SCK can be held low by the slave byte transmission
   before triggering a timeout error.
 */
#define CY_U3P_LPP_I2C_TIMEOUT_MASK                         (0xffffffff) /* <0:31> R:RW:0xFFFFFFFF:No */
#define CY_U3P_LPP_I2C_TIMEOUT_POS                          (0)



/*
   DMA Timeout Register
 */
#define CY_U3P_LPP_I2C_DMA_TIMEOUT_ADDRESS                  (0xe0000414)
#define CY_U3P_LPP_I2C_DMA_TIMEOUT                          (*(uvint32_t *)(0xe0000414))
#define CY_U3P_LPP_I2C_DMA_TIMEOUT_DEFAULT                  (0x0000ffff)

/*
   Number of core clocks DMA has to be not ready before the condition is
   reported as error condition.
 */
#define CY_U3P_LPP_I2C_TIMEOUT16_MASK                       (0x0000ffff) /* <0:15> R:RW:0xFFFF:No */
#define CY_U3P_LPP_I2C_TIMEOUT16_POS                        (0)



/*
   I2C Preamble Control Register
 */
#define CY_U3P_LPP_I2C_PREAMBLE_CTRL_ADDRESS                (0xe0000418)
#define CY_U3P_LPP_I2C_PREAMBLE_CTRL                        (*(uvint32_t *)(0xe0000418))
#define CY_U3P_LPP_I2C_PREAMBLE_CTRL_DEFAULT                (0x00000000)

/*
   If bit <x> is 1, issue a start after byte <x> of the preamble.
 */
#define CY_U3P_LPP_I2C_START_MASK                           (0x000000ff) /* <0:7> R:RW:0:No */
#define CY_U3P_LPP_I2C_START_POS                            (0)


/*
   If bit <x> is 1, issue a stop after byte <x> of the preamble (if both
   START and STOP are set, STOP will take priority).
 */
#define CY_U3P_LPP_I2C_STOP_MASK                            (0x0000ff00) /* <8:15> R:RW:0:No */
#define CY_U3P_LPP_I2C_STOP_POS                             (8)



/*
   I2C Preamble Data Register
 */
#define CY_U3P_LPP_I2C_PREAMBLE_DATA0_ADDRESS               (0xe000041c)
#define CY_U3P_LPP_I2C_PREAMBLE_DATA0                       (*(uvint32_t *)(0xe000041c))
#define CY_U3P_LPP_I2C_PREAMBLE_DATA0_DEFAULT               (0x00000000)

/*
   Command and initial data bytes.
 */
#define CY_U3P_LPP_I2C_DATA_L_MASK                          (0xffffffff) /* <0:31> R:RW:0:No */
#define CY_U3P_LPP_I2C_DATA_L_POS                           (0)



/*
   I2C Preamble Data Register
 */
#define CY_U3P_LPP_I2C_PREAMBLE_DATA1_ADDRESS               (0xe0000420)
#define CY_U3P_LPP_I2C_PREAMBLE_DATA1                       (*(uvint32_t *)(0xe0000420))
#define CY_U3P_LPP_I2C_PREAMBLE_DATA1_DEFAULT               (0x00000000)

/*
   Command and initial data bytes.
 */
#define CY_U3P_LPP_I2C_DATA_H_MASK                          (0xffffffff) /* <0:31> R:RW:0:No */
#define CY_U3P_LPP_I2C_DATA_H_POS                           (0)



/*
   I2C Preamble Repeat Register
 */
#define CY_U3P_LPP_I2C_PREAMBLE_RPT_ADDRESS                 (0xe0000424)
#define CY_U3P_LPP_I2C_PREAMBLE_RPT                         (*(uvint32_t *)(0xe0000424))
#define CY_U3P_LPP_I2C_PREAMBLE_RPT_DEFAULT                 (0x00000002)

/*
   1: Turns on preamble repeat feature. The sequence from IDLE to preamble_complete
   will repeat in a programmable fashion. START_FIRST will be honored. Repating
   will always start at the first byte and end at the last byte. The only
   exception is if timeout is enabled and happens during byte trasnmission,
   in which case premable will stop at the end of the current byte. Data
   phase is not entered if preamble repeat feature is enabled.
 */
#define CY_U3P_LPP_I2C_RPT_ENABLE                           (1u << 0) /* <0:0> R:RW:0:No */


/*
   1: Preamble will stop repeating if the ACK is received after any byte.
 */
#define CY_U3P_LPP_I2C_STOP_ON_ACK                          (1u << 1) /* <1:1> R:RW:1:No */


/*
   1: Preamble will stop repeating if the NACK is received after any byte.
   Unless this byte is set, continue on nack if reapeat is enabled.
 */
#define CY_U3P_LPP_I2C_STOP_ON_NACK                         (1u << 2) /* <2:2> R:RW:0:No */


/*
   Preamble stops after reaching the count or earlier if other conditions
   are satisfied. The maximum number of times a preamble can repeat is FF_FFFF
 */
#define CY_U3P_LPP_I2C_COUNT_MASK                           (0xffffff00) /* <8:31> RW:RW:FF_FFFF:Yes */
#define CY_U3P_LPP_I2C_COUNT_POS                            (8)



/*
   I2C Command Register
 */
#define CY_U3P_LPP_I2C_COMMAND_ADDRESS                      (0xe0000428)
#define CY_U3P_LPP_I2C_COMMAND                              (*(uvint32_t *)(0xe0000428))
#define CY_U3P_LPP_I2C_COMMAND_DEFAULT                      (0x000000a0)

/*
   Number of bytes in preamble.  For preamble length = 1, set this to 1 etc.
   From 1 through 8. 0 and values > 8 are not supported.
 */
#define CY_U3P_LPP_I2C_PREAMBLE_LEN_MASK                    (0x0000000f) /* <0:3> R:RW:0:No */
#define CY_U3P_LPP_I2C_PREAMBLE_LEN_POS                     (0)


/*
   SW sets this bit to indicate valid preamble. HW resets it to indicate
   that it has finished transmitting the preamble so that new preamble, such
   as one for doing restarts, can be populated.
 */
#define CY_U3P_LPP_I2C_PREAMBLE_VALID                       (1u << 4) /* <4:4> RW0C:RW1S:0:Yes */


/*
   0: Send ACK on last byte of read
   1: Send NAK on last byte of read
 */
#define CY_U3P_LPP_I2C_NAK_LAST                             (1u << 5) /* <5:5> R:RW:1:No */


/*
   0: Send (repeated) START on last byte of  data phase
   1: Send STOP on last byte of data phase
 */
#define CY_U3P_LPP_I2C_STOP_LAST                            (1u << 6) /* <6:6> R:RW:0:No */


/*
   1: Send START before the first byte of preamble
   0: Do nothing before the first byte of preamble
 */
#define CY_U3P_LPP_I2C_START_FIRST                          (1u << 7) /* <7:7> R:RW:1:No */


/*
   0: Write command (data phase)
   1: Read command (data phase)
    After command, the HW will idle if no valid pre-amble exists, will play
   preamble if it does exist. Valid preamble is indicated by preamble valid
   bit. Data phase is not entered if preamble repeat feature is enabled.
 */
#define CY_U3P_LPP_I2C_READ                                 (1u << 28) /* <28:28> R:RW:0:No */


/*
   Valid only when BYTE_COUNT=FFFF_FFFF. SW turns this bit on to end infinite
   transfers.  Transfer will end at the end of current byte.
   Note: This feature is not supported on FX3.
 */
#define CY_U3P_LPP_I2C_END_INFINITE_TRANSFER                (1u << 29) /* <29:29> RW0C:RW1S:0:Yes */


/*
   00:  I2C is idle. 01: I2C is playing the preamble. 10: I2C is receiving
   data. 11: I2C is transmitting data.
 */
#define CY_U3P_LPP_I2C_I2C_STAT_MASK                        (0xc0000000) /* <30:31> W:R:0:No */
#define CY_U3P_LPP_I2C_I2C_STAT_POS                         (30)



/*
   I2C Egress Data Register
 */
#define CY_U3P_LPP_I2C_EGRESS_DATA_ADDRESS                  (0xe000042c)
#define CY_U3P_LPP_I2C_EGRESS_DATA                          (*(uvint32_t *)(0xe000042c))
#define CY_U3P_LPP_I2C_EGRESS_DATA_DEFAULT                  (0x00000000)

/*
   Data byte to be written to the peripheral in registered mode.
 */
#define CY_U3P_LPP_I2C_DATA_MASK                            (0x000000ff) /* <0:7> R:W:0:No */
#define CY_U3P_LPP_I2C_DATA_POS                             (0)



/*
   I2C Ingress Data Register
 */
#define CY_U3P_LPP_I2C_INGRESS_DATA_ADDRESS                 (0xe0000430)
#define CY_U3P_LPP_I2C_INGRESS_DATA                         (*(uvint32_t *)(0xe0000430))
#define CY_U3P_LPP_I2C_INGRESS_DATA_DEFAULT                 (0x00000000)

/*
   Data byte read from the peripheral when DMA_MODE=0
 */
#define CY_U3P_LPP_I2C_DATA_MASK                            (0x000000ff) /* <0:7> W:R:0:No */
#define CY_U3P_LPP_I2C_DATA_POS                             (0)



/*
   I2C Clock Low Count Register
 */
#define CY_U3P_LPP_I2C_CLOCK_LOW_COUNT_ADDRESS              (0xe0000434)
#define CY_U3P_LPP_I2C_CLOCK_LOW_COUNT                      (*(uvint32_t *)(0xe0000434))
#define CY_U3P_LPP_I2C_CLOCK_LOW_COUNT_DEFAULT              (0x00000000)

/*
   Indicates the low period of the last clock pulse on the I2C bus, measured
   using the I2C core clock.
 */
#define CY_U3P_LPP_I2C_CLOCK_LOW_COUNT_MASK                 (0xffffffff) /* <0:31> W:R:0:No */
#define CY_U3P_LPP_I2C_CLOCK_LOW_COUNT_POS                  (0)



/*
   I2C Byte Count Register
 */
#define CY_U3P_LPP_I2C_BYTE_COUNT_ADDRESS                   (0xe0000438)
#define CY_U3P_LPP_I2C_BYTE_COUNT                           (*(uvint32_t *)(0xe0000438))
#define CY_U3P_LPP_I2C_BYTE_COUNT_DEFAULT                   (0xffffffff)

/*
   Number of bytes in the data phase of the  transfer. Infinite tranfer mode
   perviously indicated by BYTE_COUNT = 0xFFFF_FFFFF is no longer supported.
   Please perform transfers in terms of fixed byte count and perform dummy
   transactions at the end if required to complete the byte count
 */
#define CY_U3P_LPP_I2C_BYTE_COUNT_MASK                      (0xffffffff) /* <0:31> R:RW:0xFFFFFFFF:No */
#define CY_U3P_LPP_I2C_BYTE_COUNT_POS                       (0)



/*
   Number of bytes tranferred in the data phase
 */
#define CY_U3P_LPP_I2C_BYTES_TRANSFERRED_ADDRESS            (0xe000043c)
#define CY_U3P_LPP_I2C_BYTES_TRANSFERRED                    (*(uvint32_t *)(0xe000043c))
#define CY_U3P_LPP_I2C_BYTES_TRANSFERRED_DEFAULT            (0x00000000)

/*
   Indicates number of bytes  transferred in the data phase (with ACK or
   without) so far. Does not include preamble bytes. Useful for determining
   when NACK happened in data transmission. If  data NACK error happens on
   at the end of 5 th byte, the value in this register will be 5. If timeout
   happens while transmitting the 1 st bit of the 11th byte, this value will
   be 11 and so on.
 */
#define CY_U3P_LPP_I2C_BYTE_COUNT_MASK                      (0xffffffff) /* <0:31> W:R:0:No */
#define CY_U3P_LPP_I2C_BYTE_COUNT_POS                       (0)



/*
   I2C Socket Register
 */
#define CY_U3P_LPP_I2C_SOCKET_ADDRESS                       (0xe0000440)
#define CY_U3P_LPP_I2C_SOCKET                               (*(uvint32_t *)(0xe0000440))
#define CY_U3P_LPP_I2C_SOCKET_DEFAULT                       (0x00000000)

/*
   Socket number for egress data
   0-7: supported
   8-..: reserved
 */
#define CY_U3P_LPP_I2C_EGRESS_SOCKET_MASK                   (0x000000ff) /* <0:7> R:RW:0:No */
#define CY_U3P_LPP_I2C_EGRESS_SOCKET_POS                    (0)


/*
   Socket number for ingress data
   0-7: supported
   8-..: reserved
 */
#define CY_U3P_LPP_I2C_INGRESS_SOCKET_MASK                  (0x0000ff00) /* <8:15> R:RW:0:No */
#define CY_U3P_LPP_I2C_INGRESS_SOCKET_POS                   (8)



/*
   Block Identification and Version Number
 */
#define CY_U3P_LPP_I2C_ID_ADDRESS                           (0xe00007f0)
#define CY_U3P_LPP_I2C_ID                                   (*(uvint32_t *)(0xe00007f0))
#define CY_U3P_LPP_I2C_ID_DEFAULT                           (0x00010001)

/*
   A unique number identifying the IP in the memory space.
 */
#define CY_U3P_LPP_I2C_BLOCK_ID_MASK                        (0x0000ffff) /* <0:15> :R:0x0001:No */
#define CY_U3P_LPP_I2C_BLOCK_ID_POS                         (0)


/*
   Version number for the IP.
 */
#define CY_U3P_LPP_I2C_BLOCK_VERSION_MASK                   (0xffff0000) /* <16:31> :R:0x0001:No */
#define CY_U3P_LPP_I2C_BLOCK_VERSION_POS                    (16)



/*
   Power, clock and reset control
 */
#define CY_U3P_LPP_I2C_POWER_ADDRESS                        (0xe00007f4)
#define CY_U3P_LPP_I2C_POWER                                (*(uvint32_t *)(0xe00007f4))
#define CY_U3P_LPP_I2C_POWER_DEFAULT                        (0x00000000)

/*
   For blocks that must perform initialization after reset before becoming
   operational, this signal will remain de-asserted until initialization
   is complete.  In other words reading active=1 indicates block is initialized
   and ready for operation.
 */
#define CY_U3P_LPP_I2C_ACTIVE                               (1u << 0) /* <0:0> W:R:0:Yes */


/*
   The block core clock is bypassed with the ATE clock supplied on TCK pin.
   Block operates normally. This mode is designed to allow functional vectors
   to be used during production test.
 */
#define CY_U3P_LPP_I2C_PLL_BYPASS                           (1u << 29) /* <29:29> R:RW:0:No */


/*
   Active LOW reset signal for all logic in the block.  Note that reset is
   active on all flops in the block when either system reset is asserted
   (RESET# pin or SYSTEM_POWER.RESETN is asserted) or this signal is active.
   After setting this bit to 1, firmware shall poll and wait for the ‘active’
   bit to assert.  Reading ‘1’ from ‘resetn’ does not indicate the block
   is out of reset – this may take some time depending on initialization
   tasks and clock frequencies.
   This bit must be asserted ('0') for at least 10us for effective reset.
 */
#define CY_U3P_LPP_I2C_RESETN                               (1u << 31) /* <31:31> R:RW:0:No */



#endif /* _INCLUDED_I2C_REGS_H_ */

/*[]*/

