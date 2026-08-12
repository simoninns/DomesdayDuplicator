/*
 ## Cypress FX3 Core Library Header (spi_regs.h)
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

#ifndef _INCLUDED_SPI_REGS_H_
#define _INCLUDED_SPI_REGS_H_

#include <cyu3types.h>

#define SPI_BASE_ADDR                            (0xe0000c00)

typedef struct
{
    uvint32_t lpp_spi_config;                     /* 0xe0000c00 */
    uvint32_t lpp_spi_status;                     /* 0xe0000c04 */
    uvint32_t lpp_spi_intr;                       /* 0xe0000c08 */
    uvint32_t lpp_spi_intr_mask;                  /* 0xe0000c0c */
    uvint32_t lpp_spi_egress_data;                /* 0xe0000c10 */
    uvint32_t lpp_spi_ingress_data;               /* 0xe0000c14 */
    uvint32_t lpp_spi_socket;                     /* 0xe0000c18 */
    uvint32_t lpp_spi_rx_byte_count;              /* 0xe0000c1c */
    uvint32_t lpp_spi_tx_byte_count;              /* 0xe0000c20 */
    uvint32_t rsrvd0[243];
    uvint32_t lpp_spi_id;                         /* 0xe0000ff0 */
    uvint32_t lpp_spi_power;                      /* 0xe0000ff4 */
} SPI_REGS_T, *PSPI_REGS_T;

#define SPI        ((PSPI_REGS_T) SPI_BASE_ADDR)


/*
   SPI Configuration and modes register
 */
#define CY_U3P_LPP_SPI_CONFIG_ADDRESS                       (0xe0000c00)
#define CY_U3P_LPP_SPI_CONFIG                               (*(uvint32_t *)(0xe0000c00))
#define CY_U3P_LPP_SPI_CONFIG_DEFAULT                       (0x00105102)

/*
   0: Receiver disabled, ignore incoming data
   1: Receive enabled
 */
#define CY_U3P_LPP_SPI_RX_ENABLE                            (1u << 0) /* <0:0> R:RW:0:Yes */


/*
   0: Transmitter disable, do not transmit data
   1: Transmitter enabled
 */
#define CY_U3P_LPP_SPI_TX_ENABLE                            (1u << 1) /* <1:1> R:RW:1:Yes */


/*
   0: Register-based transfers
   1: DMA-based transfers
 */
#define CY_U3P_LPP_SPI_DMA_MODE                             (1u << 2) /* <2:2> R:RW:0:No */


/*
   0: MSB First
   1: LSB First
 */
#define CY_U3P_LPP_SPI_ENDIAN                               (1u << 3) /* <3:3> R:RW:0:No */


/*
   This bit controls SSN behavior at the end of each received and trasnmitted
   "byte" if SSNCTRL indicate firmware SSN control. This bit is transmitted
   over SSN line *as is*  and without regards to how many cycles it is going
   out. SSPOL is applied. The FW is expected to send one word at a time in
   this mode. DESELECT bit takes precedence over this bit.
   Typically the FW shall keep this bit asserted for the entire transaction
   that can span multiple words. If SSN needs to be de-asserted between words,
   only single word transfers are possible when SSN is under firmware control.
   Modify only when ENABLE=0.
 */
#define CY_U3P_LPP_SPI_SSN_BIT                              (1u << 4) /* <4:4> R:RW:0:No */


/*
   0: No effect
   1: Send the value being transmitted to the receive buffer. Disable external
   transmit and receive.
 */
#define CY_U3P_LPP_SPI_LOOPBACK                             (1u << 5) /* <5:5> R:RW:0:No */


/*
   00: SSN is toggled by FW.
   01: SSN remains asserted between each 8-bit transfer.
   10: SSN asserts high at the end of the transfer.
   11: SSN is governed by CPHA
 */
#define CY_U3P_LPP_SPI_SSNCTRL_MASK                         (0x00000300) /* <8:9> R:RW:1:No */
#define CY_U3P_LPP_SPI_SSNCTRL_POS                          (8)


/*
   0: SCK idles low
   1: SCK idles high
 */
#define CY_U3P_LPP_SPI_CPOL                                 (1u << 10) /* <10:10> R:RW:0:No */


/*
   Transaction start mode. See section 8.8.4.2.1
 */
#define CY_U3P_LPP_SPI_CPHA                                 (1u << 11) /* <11:11> R:RW:0:No */


/*
   SSN-SCK lead time.Encodings are:  0, 0.5, 1 or 1.5 SCK cycles. Indicates
   the number of half-clock cycles SSN need to assert ahead of the first
   SCK cycle. However zero lead is not supported.
 */
#define CY_U3P_LPP_SPI_LEAD_MASK                            (0x00003000) /* <12:13> R:RW:1:No */
#define CY_U3P_LPP_SPI_LEAD_POS                             (12)


/*
   SCK-SSN lag time. Indicates the number of half-clock cycles for which
   SSN needs to remain asserted after the last SCK cycle including zero.
   Note that LAG must be >0 when CHPA=1.
 */
#define CY_U3P_LPP_SPI_LAG_MASK                             (0x0000c000) /* <14:15> R:RW:1:No */
#define CY_U3P_LPP_SPI_LAG_POS                              (14)


/*
   0: SSN is active low, else active high.
 */
#define CY_U3P_LPP_SPI_SSPOL                                (1u << 16) /* <16:16> R:RW:0:No */


/*
   SPI transaction-unit length from 4 to 32 bits. 0-3 reserved.
 */
#define CY_U3P_LPP_SPI_WL_MASK                              (0x007e0000) /* <17:22> R:RW:8:No */
#define CY_U3P_LPP_SPI_WL_POS                               (17)


/*
   1: Never assert SSN. Used to direct SPI traffic to non-default slaves
   whose SSN are connected to GPIO.
   0: Normal SSN behaviour.
 */
#define CY_U3P_LPP_SPI_DESELECT                             (1u << 23) /* <23:23> R:RW:0:No */


/*
   0: Do nothing
   1: Clear receive FIFO
   (s/w must wait for RX_DATA=0 before clearing this bit again)
 */
#define CY_U3P_LPP_SPI_RX_CLEAR                             (1u << 29) /* <29:29> R:RW:0:No */


/*
   Use only when ENABLE=0; behavior undefined when ENABLE=1
   0: Do nothing
   1: Clear transmit FIFO
   (once TX_CLEAR is set, s/w must wait for TX_DONE before clearing it)
 */
#define CY_U3P_LPP_SPI_TX_CLEAR                             (1u << 30) /* <30:30> R:RW:0:Yes */


/*
   Enable block here, but only after all the configuration is set.  Do not
   set this bit to 1 while chaning any other value in this register. This
   bit will be synchronized to the core clock.
   Setting this bit to 0 will complete transmission of current sample.  When
   DMA_MODE=1 any remaining samples in the pipelineare discarded.  When DMA_MODE=0
   no samples are lost.
 */
#define CY_U3P_LPP_SPI_ENABLE                               (1u << 31) /* <31:31> R:RW:0:Yes */



/*
   SPI Status and Error register
 */
#define CY_U3P_LPP_SPI_STATUS_ADDRESS                       (0xe0000c04)
#define CY_U3P_LPP_SPI_STATUS                               (*(uvint32_t *)(0xe0000c04))
#define CY_U3P_LPP_SPI_STATUS_DEFAULT                       (0x0f000030)

/*
   Indicates receive operation completed.  Only relevant when DMA_MODE=1).
    Receive operation is complete when transfer size bytes in socket have
   been received. Non sticky, does not require software intervention to clear
   it.
 */
#define CY_U3P_LPP_SPI_RX_DONE                              (1u << 0) /* <0:0> W:R:0:Yes */


/*
   Indicates data is available in the RX FIFO (only relevant when DMA_MODE=0).
   This bit is updated immediately after reads from INGRESS_DATA register.
   Non sticky
 */
#define CY_U3P_LPP_SPI_RX_DATA                              (1u << 1) /* <1:1> W:R:0:No */


/*
   Indicates that the RX FIFO is at least half full (only relevant when DMA_MODE=0).
   This bit can be used to create burst based interrupts. This bit is updated
   immediately after reads from INGRESS_DATA register. Non sticky
 */
#define CY_U3P_LPP_SPI_RX_HALF                              (1u << 2) /* <2:2> W:R:0:No */


/*
   Indicates no more data is available for transmission. Non sticky.
   If DMA_MODE=0 this is defined as TX FIFO empty and shift register empty.
    If DMA_MODE=1 this is defined as BYTE_COUNT=0 and shift register empty.
    Note that this field will only assert after a transmission was started
   - it's power up state is 0.
 */
#define CY_U3P_LPP_SPI_TX_DONE                              (1u << 3) /* <3:3> W:R:0:Yes */


/*
   Indicates space is available in the TX FIFO. This bit is updated immediately
   after writes to EGRESS_DATA register. Non sticky.
 */
#define CY_U3P_LPP_SPI_TX_SPACE                             (1u << 4) /* <4:4> W:R:1:No */


/*
   Indicates that the TX FIFO is at least half empty.  This bit can be used
   to create burst-based interrupts.  This bit is updated immediately after
   writes to EGRESS_DATA register. Non sticky.
 */
#define CY_U3P_LPP_SPI_TX_HALF                              (1u << 5) /* <5:5> W:R:1:No */


/*
   An internal error has occurred with cause ERROR_CODE.  Must be cleared
   by software. Sticky
 */
#define CY_U3P_LPP_SPI_ERROR                                (1u << 6) /* <6:6> RW1S:RW1C:0:Yes */


/*
   Error code, only relevant when ERROR=1.  ERROR logs only the FIRST error
   to occur and will never change value as long as ERROR=1.
   12: Write to TX FIFO when FIFO full
   13: Read from RX FIFO when FIFO empty
   15: No error
 */
#define CY_U3P_LPP_SPI_ERROR_CODE_MASK                      (0x0f000000) /* <24:27> W:R:0xF:No */
#define CY_U3P_LPP_SPI_ERROR_CODE_POS                       (24)


/*
   Indicates the block is busy transmitting data.  This field may remain
   asserted after the block is suspended and must be polled before changing
   any configuration values.
 */
#define CY_U3P_LPP_SPI_BUSY                                 (1u << 28) /* <28:28> W:R:0:Yes */



/*
   SPI Interrupt Request Register
 */
#define CY_U3P_LPP_SPI_INTR_ADDRESS                         (0xe0000c08)
#define CY_U3P_LPP_SPI_INTR                                 (*(uvint32_t *)(0xe0000c08))
#define CY_U3P_LPP_SPI_INTR_DEFAULT                         (0x00000000)

/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_SPI_RX_DONE                              (1u << 0) /* <0:0> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_SPI_RX_DATA                              (1u << 1) /* <1:1> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_SPI_RX_HALF                              (1u << 2) /* <2:2> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_SPI_TX_DONE                              (1u << 3) /* <3:3> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_SPI_TX_SPACE                             (1u << 4) /* <4:4> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_SPI_TX_HALF                              (1u << 5) /* <5:5> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_SPI_ERROR                                (1u << 6) /* <6:6> RW1S:RW1C:0:No */



/*
   SPI Interrupt Mask Register
 */
#define CY_U3P_LPP_SPI_INTR_MASK_ADDRESS                    (0xe0000c0c)
#define CY_U3P_LPP_SPI_INTR_MASK                            (*(uvint32_t *)(0xe0000c0c))
#define CY_U3P_LPP_SPI_INTR_MASK_DEFAULT                    (0x00000000)

/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_SPI_RX_DONE                              (1u << 0) /* <0:0> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_SPI_RX_DATA                              (1u << 1) /* <1:1> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_SPI_RX_HALF                              (1u << 2) /* <2:2> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_SPI_TX_DONE                              (1u << 3) /* <3:3> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_SPI_TX_SPACE                             (1u << 4) /* <4:4> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_SPI_TX_HALF                              (1u << 5) /* <5:5> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_SPI_ERROR                                (1u << 6) /* <6:6> R:RW:0:No */



/*
   SPI Egress Data Register
 */
#define CY_U3P_LPP_SPI_EGRESS_DATA_ADDRESS                  (0xe0000c10)
#define CY_U3P_LPP_SPI_EGRESS_DATA                          (*(uvint32_t *)(0xe0000c10))
#define CY_U3P_LPP_SPI_EGRESS_DATA_DEFAULT                  (0x00000000)

/*
   Data word to be written to the peripheral in registered mode.  Only the
   least significant SPI_CONF.WL bits are used.  Other bits are ignored.
 */
#define CY_U3P_LPP_SPI_DATA32_MASK                          (0xffffffff) /* <0:31> R:W:0:No */
#define CY_U3P_LPP_SPI_DATA32_POS                           (0)



/*
   SPI Ingress Data Register
 */
#define CY_U3P_LPP_SPI_INGRESS_DATA_ADDRESS                 (0xe0000c14)
#define CY_U3P_LPP_SPI_INGRESS_DATA                         (*(uvint32_t *)(0xe0000c14))
#define CY_U3P_LPP_SPI_INGRESS_DATA_DEFAULT                 (0x00000000)

/*
   Data word read from the peripheral when DMA_MODE=0.  Only the least significant
   SPI_CONF.WL bits are provided.  Other bits are set to 0.
 */
#define CY_U3P_LPP_SPI_DATA32_MASK                          (0xffffffff) /* <0:31> RW:R:0:No */
#define CY_U3P_LPP_SPI_DATA32_POS                           (0)



/*
   SPI Socket Register
 */
#define CY_U3P_LPP_SPI_SOCKET_ADDRESS                       (0xe0000c18)
#define CY_U3P_LPP_SPI_SOCKET                               (*(uvint32_t *)(0xe0000c18))
#define CY_U3P_LPP_SPI_SOCKET_DEFAULT                       (0x00000000)

/*
   Socket number for egress data
   0-7: supported
   8-..: reserved
 */
#define CY_U3P_LPP_SPI_EGRESS_SOCKET_MASK                   (0x000000ff) /* <0:7> R:RW:0:No */
#define CY_U3P_LPP_SPI_EGRESS_SOCKET_POS                    (0)


/*
   Socket number for ingress data
   0-7: supported
   8-..: reserved
 */
#define CY_U3P_LPP_SPI_INGRESS_SOCKET_MASK                  (0x0000ff00) /* <8:15> R:RW:0:No */
#define CY_U3P_LPP_SPI_INGRESS_SOCKET_POS                   (8)



/*
   SPI Receive Byte Count Register
 */
#define CY_U3P_LPP_SPI_RX_BYTE_COUNT_ADDRESS                (0xe0000c1c)
#define CY_U3P_LPP_SPI_RX_BYTE_COUNT                        (*(uvint32_t *)(0xe0000c1c))
#define CY_U3P_LPP_SPI_RX_BYTE_COUNT_DEFAULT                (0xffffffff)

/*
   Number of words left to read or write
   0xFFFFFFFF indicates infinite (counter will not decrement)
 */
#define CY_U3P_LPP_SPI_WORD_COUNT_MASK                      (0xffffffff) /* <0:31> RW:RW:0xFFFFFFFF:Yes */
#define CY_U3P_LPP_SPI_WORD_COUNT_POS                       (0)



/*
   SPI Transmit Byte Count Register
 */
#define CY_U3P_LPP_SPI_TX_BYTE_COUNT_ADDRESS                (0xe0000c20)
#define CY_U3P_LPP_SPI_TX_BYTE_COUNT                        (*(uvint32_t *)(0xe0000c20))
#define CY_U3P_LPP_SPI_TX_BYTE_COUNT_DEFAULT                (0xffffffff)

/*
   Number of words left to read or write
   0xFFFFFFFF indicates infinite (counter will not decrement)
 */
#define CY_U3P_LPP_SPI_WORD_COUNT_MASK                      (0xffffffff) /* <0:31> RW:RW:0xFFFFFFFF:Yes */
#define CY_U3P_LPP_SPI_WORD_COUNT_POS                       (0)



/*
   Block Identification and Version Number
 */
#define CY_U3P_LPP_SPI_ID_ADDRESS                           (0xe0000ff0)
#define CY_U3P_LPP_SPI_ID                                   (*(uvint32_t *)(0xe0000ff0))
#define CY_U3P_LPP_SPI_ID_DEFAULT                           (0x00010003)

/*
   A unique number identifying the IP in the memory space.
 */
#define CY_U3P_LPP_SPI_BLOCK_ID_MASK                        (0x0000ffff) /* <0:15> :R:0x0003:No */
#define CY_U3P_LPP_SPI_BLOCK_ID_POS                         (0)


/*
   Version number for the IP.
 */
#define CY_U3P_LPP_SPI_BLOCK_VERSION_MASK                   (0xffff0000) /* <16:31> :R:0x0001:No */
#define CY_U3P_LPP_SPI_BLOCK_VERSION_POS                    (16)



/*
   Power, clock and reset control
 */
#define CY_U3P_LPP_SPI_POWER_ADDRESS                        (0xe0000ff4)
#define CY_U3P_LPP_SPI_POWER                                (*(uvint32_t *)(0xe0000ff4))
#define CY_U3P_LPP_SPI_POWER_DEFAULT                        (0x00000000)

/*
   For blocks that must perform initialization after reset before becoming
   operational, this signal will remain de-asserted until initialization
   is complete.  In other words reading active=1 indicates block is initialized
   and ready for operation.
 */
#define CY_U3P_LPP_SPI_ACTIVE                               (1u << 0) /* <0:0> W:R:0:Yes */


/*
   The block core clock is bypassed with the ATE clock supplied on TCK pin.
   Block operates normally. This mode is designed to allow functional vectors
   to be used during production test.
 */
#define CY_U3P_LPP_SPI_PLL_BYPASS                           (1u << 29) /* <29:29> R:RW:0:No */


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
#define CY_U3P_LPP_SPI_RESETN                               (1u << 31) /* <31:31> R:RW:0:No */



#endif /* _INCLUDED_SPI_REGS_H_ */

/*[]*/

