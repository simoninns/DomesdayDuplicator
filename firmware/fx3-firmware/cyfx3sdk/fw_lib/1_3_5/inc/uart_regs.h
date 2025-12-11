/*
 ## Cypress FX3 Core Library Header (uart_regs.h)
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

#ifndef _INCLUDED_UART_REGS_H_
#define _INCLUDED_UART_REGS_H_

#include <cyu3types.h>

#define UART_BASE_ADDR                           (0xe0000800)

typedef struct
{
    uvint32_t lpp_uart_config;                    /* 0xe0000800 */
    uvint32_t lpp_uart_status;                    /* 0xe0000804 */
    uvint32_t lpp_uart_intr;                      /* 0xe0000808 */
    uvint32_t lpp_uart_intr_mask;                 /* 0xe000080c */
    uvint32_t lpp_uart_egress_data;               /* 0xe0000810 */
    uvint32_t lpp_uart_ingress_data;              /* 0xe0000814 */
    uvint32_t lpp_uart_socket;                    /* 0xe0000818 */
    uvint32_t lpp_uart_rx_byte_count;             /* 0xe000081c */
    uvint32_t lpp_uart_tx_byte_count;             /* 0xe0000820 */
    uvint32_t rsrvd0[243];
    uvint32_t lpp_uart_id;                        /* 0xe0000bf0 */
    uvint32_t lpp_uart_power;                     /* 0xe0000bf4 */
} UART_REGS_T, *PUART_REGS_T;

#define UART        ((PUART_REGS_T) UART_BASE_ADDR)


/*
   UART Configuration and mode
 */
#define CY_U3P_LPP_UART_CONFIG_ADDRESS                      (0xe0000800)
#define CY_U3P_LPP_UART_CONFIG                              (*(uvint32_t *)(0xe0000800))
#define CY_U3P_LPP_UART_CONFIG_DEFAULT                      (0x00001002)

/*
   0: Receiver disabled, ignore incoming data
   1: Receive enabled
 */
#define CY_U3P_LPP_UART_RX_ENABLE                           (1u << 0) /* <0:0> R:RW:0:Yes */


/*
   0: Transmitter disable, do not transmit data
   1: Transmitter enabled
 */
#define CY_U3P_LPP_UART_TX_ENABLE                           (1u << 1) /* <1:1> R:RW:1:Yes */


/*
   0: No effect
   1: Connect the value being transmitted to the receive buffer. Disable
   external transmit and receive.
 */
#define CY_U3P_LPP_UART_LOOP_BACK                           (1u << 2) /* <2:2> R:RW:0:Yes */


/*
   0: No parity
   1: Include parity bit
 */
#define CY_U3P_LPP_UART_PARITY                              (1u << 3) /* <3:3> R:RW:0:No */


/*
   Only relevant when PARITY=1 and PARITY_STICKY=0
   0: Even Parity
   1: Odd parity.
 */
#define CY_U3P_LPP_UART_PARITY_ODD                          (1u << 4) /* <4:4> R:RW:0:No */


/*
   Only relevant when PARITY=1
   0: Computed parity
   1: Sticky parity
 */
#define CY_U3P_LPP_UART_PARITY_STICKY                       (1u << 5) /* <5:5> R:RW:0:No */


/*
   Append this value at parity bit if sticky parity is effective.
 */
#define CY_U3P_LPP_UART_TX_STICKY_BIT                       (1u << 6) /* <6:6> R:RW:0:No */


/*
   If sticky parity is effective, log interrupt request if the received bit
   does not match this value.
 */
#define CY_U3P_LPP_UART_RX_STICKY_BIT                       (1u << 7) /* <7:7> R:RW:0:No */


/*
   00: Reserved
   01: 1 Stop bit.
   10: 2 Stop bits
   11: Reserved for 1.5 stop bits (not supported)
   Note: STOP_BITS=2 is supported only when PARITY=0.  Behavior is undefined
   otherwise.
 */
#define CY_U3P_LPP_UART_STOP_BITS_MASK                      (0x00000300) /* <8:9> R:RW:0:No */
#define CY_U3P_LPP_UART_STOP_BITS_POS                       (8)


/*
   0: Register-based transfers
   1: DMA-based transfers
 */
#define CY_U3P_LPP_UART_DMA_MODE                            (1u << 10) /* <10:10> R:RW:0:No */


/*
   When RX HW flow control is disabled, the software may use this bit to
   perform software flow control. 1 would signal the transmitter that we
   are ready to receive data. (Request to send).
   Modify only when ENABLE=0.
 */
#define CY_U3P_LPP_UART_RTS                                 (1u << 12) /* <12:12> R:RW:1:No */


/*
   1: Enable HW flow control for TX data
 */
#define CY_U3P_LPP_UART_TX_FLOW_CTRL_ENBL                   (1u << 13) /* <13:13> R:RW:0:No */


/*
   1: Enable HW flow control for RX data
 */
#define CY_U3P_LPP_UART_RX_FLOW_CTRL_ENBL                   (1u << 14) /* <14:14> R:RW:0:No */


/*
   0: Default Behavior
   1: Wait for the currently transmitting byte to complete (incl stop bit)
   and then transmit 0's indefinitely until this bit is cleared.  Do not
   transmit other bytes or discard any data while BREAK is being transmitted.
   CDT52575.
 */
#define CY_U3P_LPP_UART_TX_BREAK                            (1u << 15) /* <15:15> R:RW:0:Yes */


/*
   Set timing when to sample for RX input:
   0 : Sample on bits 0,1,2
   1 : Sample on bits 1,2,3
   2 : Sample on bits 2,3,4
   3 : Sample on bits 3,4,5
   4 : Sample on bits 4,5,6
   5 : Sample on bits 5,6,7
 */
#define CY_U3P_LPP_UART_RX_POLL_MASK                        (0x000f0000) /* <16:19> R:RW:0:No */
#define CY_U3P_LPP_UART_RX_POLL_POS                         (16)


/*
   0: Do nothing
   1: Clear receive FIFO
   (s/w must wait for RX_DATA=0 before clearing this bit again)
 */
#define CY_U3P_LPP_UART_RX_CLEAR                            (1u << 29) /* <29:29> R:RW:0:No */


/*
   Use only when ENABLE=0; behavior undefined when ENABLE=1
   0: Do nothing
   1: Clear transmit FIFO
   (once TX_CLEAR is set, s/w must wait for TX_DONE before clearing it)
 */
#define CY_U3P_LPP_UART_TX_CLEAR                            (1u << 30) /* <30:30> R:RW:0:Yes */


/*
   Enable block here, but only after all the configuration is set.  Do not
   set this bit to 1 while chaning any other value in this register. This
   bit will be synchronized to the core clock.
   Setting this bit to 0 will complete transmission of current sample.  When
   DMA_MODE=1 any remaining samples in the pipelineare discarded.  When DMA_MODE=0
   no samples are lost.
 */
#define CY_U3P_LPP_UART_ENABLE                              (1u << 31) /* <31:31> R:RW:0:Yes */



/*
   UART Status Register
 */
#define CY_U3P_LPP_UART_STATUS_ADDRESS                      (0xe0000804)
#define CY_U3P_LPP_UART_STATUS                              (*(uvint32_t *)(0xe0000804))
#define CY_U3P_LPP_UART_STATUS_DEFAULT                      (0x0f000030)

/*
   Indicates receive operation completed.  Only relevant when DMA_MODE=1).
    Receive operation is complete when transfer size bytes in socket have
   been received. Non sticky. Does not need softawre intervntion to clear
   it.
 */
#define CY_U3P_LPP_UART_RX_DONE                             (1u << 0) /* <0:0> W:R:0:Yes */


/*
   Indicates data is available in the RX FIFO (only relevant when DMA_MODE=0).
   This bit is updated immediately after reads from INGRESS_DATA register.
   Non sticky
 */
#define CY_U3P_LPP_UART_RX_DATA                             (1u << 1) /* <1:1> W:R:0:No */


/*
   Indicates that the RX FIFO is at least half full (only relevant when DMA_MODE=0).
   This bit can be used to create burst based interrupts. This bit is updated
   immediately after reads from INGRESS_DATA register. Non sticky
 */
#define CY_U3P_LPP_UART_RX_HALF                             (1u << 2) /* <2:2> W:R:0:No */


/*
   Indicates no more data is available for transmission. Non sticky.
   If DMA_MODE=0 this is defined as TX FIFO empty and shift register empty.
    If DMA_MODE=1 this is defined as BYTE_COUNT=0 and shift register empty.
    Note that this field will only assert after a transmission was started
   - it's power up state is 0.
 */
#define CY_U3P_LPP_UART_TX_DONE                             (1u << 3) /* <3:3> W:R:0:Yes */


/*
   Indicates space is available in the TX FIFO. This bit is updated immediately
   after writes to EGRESS_DATA register. Non sticky.
 */
#define CY_U3P_LPP_UART_TX_SPACE                            (1u << 4) /* <4:4> W:R:1:No */


/*
   Indicates that the TX FIFO is at least half empty.  This bit can be used
   to create burst-based interrupts.  This bit is updated immediately after
   writes to EGRESS_DATA register. Non sticky.
 */
#define CY_U3P_LPP_UART_TX_HALF                             (1u << 5) /* <5:5> W:R:1:No */


/*
   CTS Status, polarity inverted from the pin.  (CTS pin 0 = CTS_STAT  1
   means FX3 can transmit) Non sticky
 */
#define CY_U3P_LPP_UART_CTS_STAT                            (1u << 6) /* <6:6> W:R:0:Yes */


/*
   Set when CTS toggles.
 */
#define CY_U3P_LPP_UART_CTS_TOGGLE                          (1u << 7) /* <7:7> RW1S:RW1C:0:Yes */


/*
   Break condition has been detected. Non sticky.
 */
#define CY_U3P_LPP_UART_BREAK                               (1u << 8) /* <8:8> W:R:0:Yes */


/*
   A protocol error has occurred with cause ERROR_CODE.  Must be cleared
   by software. Sticky
 */
#define CY_U3P_LPP_UART_ERROR                               (1u << 9) /* <9:9> RW1S:RW1C:0:Yes */


/*
   Error code, only relevant when ERROR=1.  ERROR logs only the FIRST error
   to occur and will never change value as long as ERROR=1.
   0: Missing Stop bit
   1: RX Parity error (received parity bit does not match RX_STICKY_BIT in
   sticky partity mode, or the computed parity in non-sticky mode.)
   12: Write to TX FIFO when FIFO full
   13: Read from RX FIFO when FIFO empty
   14: RX FIFO overflow or DMA Socket Overflow
   15: No error
 */
#define CY_U3P_LPP_UART_ERROR_CODE_MASK                     (0x0f000000) /* <24:27> W:R:0xF:No */
#define CY_U3P_LPP_UART_ERROR_CODE_POS                      (24)


/*
   Indicates the block is busy transmitting data.  This field may remain
   asserted after the block is suspended and must be polled before changing
   any configuration values.
 */
#define CY_U3P_LPP_UART_BUSY                                (1u << 28) /* <28:28> W:R:0:Yes */



/*
   UART Interrupt Request Register
 */
#define CY_U3P_LPP_UART_INTR_ADDRESS                        (0xe0000808)
#define CY_U3P_LPP_UART_INTR                                (*(uvint32_t *)(0xe0000808))
#define CY_U3P_LPP_UART_INTR_DEFAULT                        (0x00000000)

/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_UART_RX_DONE                             (1u << 0) /* <0:0> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_UART_RX_DATA                             (1u << 1) /* <1:1> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_UART_RX_HALF                             (1u << 2) /* <2:2> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_UART_TX_DONE                             (1u << 3) /* <3:3> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_UART_TX_SPACE                            (1u << 4) /* <4:4> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_UART_TX_HALF                             (1u << 5) /* <5:5> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_UART_CTS_STAT                            (1u << 6) /* <6:6> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_UART_CTS_TOGGLE                          (1u << 7) /* <7:7> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_UART_BREAK                               (1u << 8) /* <8:8> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_UART_ERROR                               (1u << 9) /* <9:9> RW1S:RW1C:0:No */



/*
   UART Interrupt Mask Register
 */
#define CY_U3P_LPP_UART_INTR_MASK_ADDRESS                   (0xe000080c)
#define CY_U3P_LPP_UART_INTR_MASK                           (*(uvint32_t *)(0xe000080c))
#define CY_U3P_LPP_UART_INTR_MASK_DEFAULT                   (0x00000000)

/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_UART_RX_DONE                             (1u << 0) /* <0:0> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_UART_RX_DATA                             (1u << 1) /* <1:1> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_UART_RX_HALF                             (1u << 2) /* <2:2> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_UART_TX_DONE                             (1u << 3) /* <3:3> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_UART_TX_SPACE                            (1u << 4) /* <4:4> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_UART_TX_HALF                             (1u << 5) /* <5:5> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_UART_CTS_STAT                            (1u << 6) /* <6:6> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_UART_CTS_TOGGLE                          (1u << 7) /* <7:7> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_UART_BREAK                               (1u << 8) /* <8:8> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_UART_ERROR                               (1u << 9) /* <9:9> R:RW:0:No */



/*
   UART Egress Data Register
 */
#define CY_U3P_LPP_UART_EGRESS_DATA_ADDRESS                 (0xe0000810)
#define CY_U3P_LPP_UART_EGRESS_DATA                         (*(uvint32_t *)(0xe0000810))
#define CY_U3P_LPP_UART_EGRESS_DATA_DEFAULT                 (0x00000000)

/*
   Data byte to be written to the peripheral in registered mode.
 */
#define CY_U3P_LPP_UART_DATA_MASK                           (0x000000ff) /* <0:7> R:W:0:No */
#define CY_U3P_LPP_UART_DATA_POS                            (0)



/*
   UART Ingress Data
 */
#define CY_U3P_LPP_UART_INGRESS_DATA_ADDRESS                (0xe0000814)
#define CY_U3P_LPP_UART_INGRESS_DATA                        (*(uvint32_t *)(0xe0000814))
#define CY_U3P_LPP_UART_INGRESS_DATA_DEFAULT                (0x00000000)

/*
   Data byte read from the peripheral when DMA_MODE=0
 */
#define CY_U3P_LPP_UART_DATA_MASK                           (0x000000ff) /* <0:7> RW:R:0:No */
#define CY_U3P_LPP_UART_DATA_POS                            (0)



/*
   UART Socket Register
 */
#define CY_U3P_LPP_UART_SOCKET_ADDRESS                      (0xe0000818)
#define CY_U3P_LPP_UART_SOCKET                              (*(uvint32_t *)(0xe0000818))
#define CY_U3P_LPP_UART_SOCKET_DEFAULT                      (0x00000000)

/*
   Socket number for egress data
   0-7: supported
   8-..: reserved
 */
#define CY_U3P_LPP_UART_EGRESS_SOCKET_MASK                  (0x000000ff) /* <0:7> R:RW:0:No */
#define CY_U3P_LPP_UART_EGRESS_SOCKET_POS                   (0)


/*
   Socket number for ingress data
   0-7: supported
   8-..: reserved
 */
#define CY_U3P_LPP_UART_INGRESS_SOCKET_MASK                 (0x0000ff00) /* <8:15> R:RW:0:No */
#define CY_U3P_LPP_UART_INGRESS_SOCKET_POS                  (8)



/*
   UART Receive Byte Count Register
 */
#define CY_U3P_LPP_UART_RX_BYTE_COUNT_ADDRESS               (0xe000081c)
#define CY_U3P_LPP_UART_RX_BYTE_COUNT                       (*(uvint32_t *)(0xe000081c))
#define CY_U3P_LPP_UART_RX_BYTE_COUNT_DEFAULT               (0xffffffff)

/*
   Number of bytes left to receive
   0xFFFFFFFF indicates infinite (counter will not decrement)
 */
#define CY_U3P_LPP_UART_BYTE_COUNT_MASK                     (0xffffffff) /* <0:31> RW:RW:0xFFFFFFFF:Yes */
#define CY_U3P_LPP_UART_BYTE_COUNT_POS                      (0)



/*
   UART Transmit Byte Count Register
 */
#define CY_U3P_LPP_UART_TX_BYTE_COUNT_ADDRESS               (0xe0000820)
#define CY_U3P_LPP_UART_TX_BYTE_COUNT                       (*(uvint32_t *)(0xe0000820))
#define CY_U3P_LPP_UART_TX_BYTE_COUNT_DEFAULT               (0xffffffff)

/*
   Number of bytes left to transmit
   0xFFFFFFFF indicates infinite (counter will not decrement)
 */
#define CY_U3P_LPP_UART_BYTE_COUNT_MASK                     (0xffffffff) /* <0:31> RW:RW:0xFFFFFFFF:Yes */
#define CY_U3P_LPP_UART_BYTE_COUNT_POS                      (0)



/*
   Block Identification and Version Number
 */
#define CY_U3P_LPP_UART_ID_ADDRESS                          (0xe0000bf0)
#define CY_U3P_LPP_UART_ID                                  (*(uvint32_t *)(0xe0000bf0))
#define CY_U3P_LPP_UART_ID_DEFAULT                          (0x00010002)

/*
   A unique number identifying the IP in the memory space.
 */
#define CY_U3P_LPP_UART_BLOCK_ID_MASK                       (0x0000ffff) /* <0:15> :R:0x0002:No */
#define CY_U3P_LPP_UART_BLOCK_ID_POS                        (0)


/*
   Version number for the IP.
 */
#define CY_U3P_LPP_UART_BLOCK_VERSION_MASK                  (0xffff0000) /* <16:31> :R:0x0001:No */
#define CY_U3P_LPP_UART_BLOCK_VERSION_POS                   (16)



/*
   Power, clock and reset control
 */
#define CY_U3P_LPP_UART_POWER_ADDRESS                       (0xe0000bf4)
#define CY_U3P_LPP_UART_POWER                               (*(uvint32_t *)(0xe0000bf4))
#define CY_U3P_LPP_UART_POWER_DEFAULT                       (0x00000000)

/*
   For blocks that must perform initialization after reset before becoming
   operational, this signal will remain de-asserted until initialization
   is complete.  In other words reading active=1 indicates block is initialized
   and ready for operation.
 */
#define CY_U3P_LPP_UART_ACTIVE                              (1u << 0) /* <0:0> W:R:0:Yes */


/*
   The block core clock is bypassed with the ATE clock supplied on TCK pin.
   Block operates normally. This mode is designed to allow functional vectors
   to be used during production test.
 */
#define CY_U3P_LPP_UART_PLL_BYPASS                          (1u << 29) /* <29:29> R:RW:0:No */


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
#define CY_U3P_LPP_UART_RESETN                              (1u << 31) /* <31:31> R:RW:0:No */



#endif /* _INCLUDED_UART_REGS_H_ */

/*[]*/

