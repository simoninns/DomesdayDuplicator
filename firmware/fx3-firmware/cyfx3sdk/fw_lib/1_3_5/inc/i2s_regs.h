/*
 ## Cypress FX3 Core Library Header (i2s_regs.h)
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

#ifndef _INCLUDED_I2S_REGS_H_
#define _INCLUDED_I2S_REGS_H_

#include <cyu3types.h>

#define I2S_BASE_ADDR                            (0xe0000000)

typedef struct
{
    uvint32_t lpp_i2s_config;                     /* 0xe0000000 */
    uvint32_t lpp_i2s_status;                     /* 0xe0000004 */
    uvint32_t lpp_i2s_intr;                       /* 0xe0000008 */
    uvint32_t lpp_i2s_intr_mask;                  /* 0xe000000c */
    uvint32_t lpp_i2s_egress_data_left;           /* 0xe0000010 */
    uvint32_t lpp_i2s_egress_data_right;          /* 0xe0000014 */
    uvint32_t lpp_i2s_counter;                    /* 0xe0000018 */
    uvint32_t lpp_i2s_socket;                     /* 0xe000001c */
    uvint32_t rsrvd0[244];
    uvint32_t lpp_i2s_id;                         /* 0xe00003f0 */
    uvint32_t lpp_i2s_power;                      /* 0xe00003f4 */
} I2S_REGS_T, *PI2S_REGS_T;

#define I2S        ((PI2S_REGS_T) I2S_BASE_ADDR)


/*
   I2S Configuration and Mode Register
 */
#define CY_U3P_LPP_I2S_CONFIG_ADDRESS                       (0xe0000000)
#define CY_U3P_LPP_I2S_CONFIG                               (*(uvint32_t *)(0xe0000000))
#define CY_U3P_LPP_I2S_CONFIG_DEFAULT                       (0x00000102)

/*
   Pause transmission, transmit 0s.
   Setting this bit to 1 will not discard any samples.  Later clearing this
   bit will resume output at exact same spot.  In paused mode the I2S clock
   will continue to transmit 0-value samples.
   A small, integral, but undefined number of samples will be transmitted
   after this bit is set to 1 (to ensure no hanging samples).
   When one of the descriptors is modified in socket, no samples from the
   old descriptor will be output (all FIFO's will be cleared).
 */
#define CY_U3P_LPP_I2S_PAUSE                                (1u << 0) /* <0:0> R:RW:0:Yes */


/*
   Discard the value read from the DMA and transmit zeros instead.  Continue
   to read input samples at normal rate.
 */
#define CY_U3P_LPP_I2S_MUTE                                 (1u << 1) /* <1:1> R:RW:1:Yes */


/*
   0: MSB First
   1:LSB First
 */
#define CY_U3P_LPP_I2S_ENDIAN                               (1u << 2) /* <2:2> R:RW:0:No */


/*
   I2S_MODE:
   0: WS=0 shall denote the left Channel
   1: WS=0 shall denote the right channel.
   In left/right justified modes:
   1: WS=0 shall denote the left Channel
   0: WS=0 shall denote the right channel.
 */
#define CY_U3P_LPP_I2S_WSMODE                               (1u << 3) /* <3:3> R:RW:0:No */


/*
   0: SCK = 16*WS, 32*WS or 64* WS for 8-bit, 16-bit and 32-bit width, 64*WS
   otherwise.
   1: SCK=64*WS
 */
#define CY_U3P_LPP_I2S_FIXED_SCK                            (1u << 4) /* <4:4> R:RW:0:No */


/*
   0: Stereo
   1: Mono> Read samples from the left channel and send in out on both the
   channels.
 */
#define CY_U3P_LPP_I2S_MONO                                 (1u << 5) /* <5:5> R:RW:0:No */


/*
   0: Register-based transfers
   1: DMA-based transfers
 */
#define CY_U3P_LPP_I2S_DMA_MODE                             (1u << 6) /* <6:6> R:RW:0:No */


/*
   0:8-bit
   1:16-bit
   2:18-bit
   3:24-bit
   4:32-bit
   5-7: Reserved
 */
#define CY_U3P_LPP_I2S_BIT_WIDTH_MASK                       (0x00000700) /* <8:10> R:RW:1:No */
#define CY_U3P_LPP_I2S_BIT_WIDTH_POS                        (8)


/*
   0,3: I2S Mode
   1: Left Justified Mode
   2: Right Justified Mode. See PAS for timing diagrams.
 */
#define CY_U3P_LPP_I2S_MODE_MASK                            (0x00001800) /* <11:12> R:RW:0:No */
#define CY_U3P_LPP_I2S_MODE_POS                             (11)


/*
   Use only when ENABLE=0; behavior undefined when ENABLE=1
   0: Do nothing
   1: Clear transmit FIFO
   (once TX_CLEAR is set, s/w must wait for TX*_DONE before clearing it)
 */
#define CY_U3P_LPP_I2S_TX_CLEAR                             (1u << 30) /* <30:30> R:RW:0:Yes */


/*
   Enable block here, but only after all the configuration is set.  Do not
   set this bit to 1 while chaning any other value in this register. This
   bit will be synchronized to the core clock.
   Setting this bit to 0 will complete transmission of current sample.  When
   DMA_MODE=1 any remaining samples in the pipelineare discarded.  When DMA_MODE=0
   no samples are lost.
 */
#define CY_U3P_LPP_I2S_ENABLE                               (1u << 31) /* <31:31> R:RW:0:Yes */



/*
   I2S Status Register
 */
#define CY_U3P_LPP_I2S_STATUS_ADDRESS                       (0xe0000004)
#define CY_U3P_LPP_I2S_STATUS                               (*(uvint32_t *)(0xe0000004))
#define CY_U3P_LPP_I2S_STATUS_DEFAULT                       (0x0f000036)

/*
   Indicates no more data is available for transmission on left channel.
   Non sticky.  If DMA_MODE=0 this is defined as TX FIFO empty and shift
   register empty but will assert only when ENABLE=0.  If DMA_MODE=1 this
   is defined as socket is EOT and shift register empty.  Note that this
   field will only assert after a transmission was started - it's power up
   state is 0.
 */
#define CY_U3P_LPP_I2S_TXL_DONE                             (1u << 0) /* <0:0> W:R:0:Yes */


/*
   Indicates space is available in the left TX FIFO. This bit is updated
   immediately after writes to EGRESS_DATA_LEFT register. Only relevant when
   DMA_MODE=0. Non sticky.
 */
#define CY_U3P_LPP_I2S_TXL_SPACE                            (1u << 1) /* <1:1> W:R:1:No */


/*
   Indicates that the left TX FIFO is at least half empty.  This bit can
   be used to create burst-based interrupts.  This bit is updated immediately
   after writes to EGRESS_DATA_LEFT register. Only relevant when DMA_MODE=0.
   Non sticky.
 */
#define CY_U3P_LPP_I2S_TXL_HALF                             (1u << 2) /* <2:2> W:R:1:No */


/*
   Indicates no more data is available for transmission on right channel.
   Non sticky.  If DMA_MODE=0 this is defined as TX FIFO empty and shift
   register empty but  will assert only when ENABLE=0.  If DMA_MODE=1 this
   is defined as socket is EOT and shift register empty.  Note that this
   field will only assert after a transmission was started - it's power up
   state is 0.
 */
#define CY_U3P_LPP_I2S_TXR_DONE                             (1u << 3) /* <3:3> W:R:0:Yes */


/*
   Indicates space is available in the right TX FIFO. This bit is updated
   immediately after writes to EGRESS_DATA_RIGHT register. Only relevant
   when DMA_MODE=0. Non sticky.
 */
#define CY_U3P_LPP_I2S_TXR_SPACE                            (1u << 4) /* <4:4> W:R:1:No */


/*
   Indicates that the right TX FIFO is at least half empty.  This bit can
   be used to create burst-based interrupts.  This bit is updated immediately
   after writes to EGRESS_DATA_RIGHT register. Only relevant when DMA_MODE=0.
   Non sticky.
 */
#define CY_U3P_LPP_I2S_TXR_HALF                             (1u << 5) /* <5:5> W:R:1:No */


/*
   Output is paused (PAUSE has taken effect). Non sticky
 */
#define CY_U3P_LPP_I2S_PAUSED                               (1u << 6) /* <6:6> W:R:0:Yes */


/*
   No data is currently available for ouput, but socket does not indicate
   empty. Only relevant when DMA_MODE=1.  Non sticky.
 */
#define CY_U3P_LPP_I2S_NO_DATA                              (1u << 7) /* <7:7> W:R:0:Yes */


/*
   An internal error has occurred with cause ERROR_CODE.  Must be cleared
   by software. Sticky
 */
#define CY_U3P_LPP_I2S_ERROR                                (1u << 8) /* <8:8> RW1S:RW1C:0:Yes */


/*
   Error code, only relevant when ERROR=1.  ERROR logs only the FIRST error
   to occur and will never change value as long as ERROR=1.
   11: Left TX FIFO/DMA socket underflow
   12: Right TX FIFO/DMA socket underflow
   13: Write to left TX FIFO when FIFO full
   14: Write to right TX FIFO when FIFO full
   15: No error
 */
#define CY_U3P_LPP_I2S_ERROR_CODE_MASK                      (0x0f000000) /* <24:27> W:R:0xF:No */
#define CY_U3P_LPP_I2S_ERROR_CODE_POS                       (24)


/*
   Indicates the block is busy transmitting data.  This field may remain
   asserted after the block is suspended and must be polled before changing
   any configuration values.
 */
#define CY_U3P_LPP_I2S_BUSY                                 (1u << 28) /* <28:28> W:R:0:Yes */



/*
   I2S Interrupt Request Register
 */
#define CY_U3P_LPP_I2S_INTR_ADDRESS                         (0xe0000008)
#define CY_U3P_LPP_I2S_INTR                                 (*(uvint32_t *)(0xe0000008))
#define CY_U3P_LPP_I2S_INTR_DEFAULT                         (0x00000000)

/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2S_TXL_DONE                             (1u << 0) /* <0:0> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2S_TXL_SPACE                            (1u << 1) /* <1:1> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2S_TXL_HALF                             (1u << 2) /* <2:2> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2S_TXR_DONE                             (1u << 3) /* <3:3> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2S_TXR_SPACE                            (1u << 4) /* <4:4> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2S_TXR_HALF                             (1u << 5) /* <5:5> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2S_PAUSED                               (1u << 6) /* <6:6> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2S_NO_DATA                              (1u << 7) /* <7:7> RW1S:RW1C:0:No */


/*
   Set by h/w when corresponding STATUS asserts, cleared by s/w.
 */
#define CY_U3P_LPP_I2S_ERROR                                (1u << 8) /* <8:8> RW1S:RW1C:0:No */



/*
   I2S Interrupt Mask Register
 */
#define CY_U3P_LPP_I2S_INTR_MASK_ADDRESS                    (0xe000000c)
#define CY_U3P_LPP_I2S_INTR_MASK                            (*(uvint32_t *)(0xe000000c))
#define CY_U3P_LPP_I2S_INTR_MASK_DEFAULT                    (0x00000000)

/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2S_TXL_DONE                             (1u << 0) /* <0:0> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2S_TXL_SPACE                            (1u << 1) /* <1:1> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2S_TXL_HALF                             (1u << 2) /* <2:2> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2S_TXR_DONE                             (1u << 3) /* <3:3> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2S_TXR_SPACE                            (1u << 4) /* <4:4> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2S_TXR_HALF                             (1u << 5) /* <5:5> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2S_PAUSED                               (1u << 6) /* <6:6> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2S_NO_DATA                              (1u << 7) /* <7:7> R:RW:0:No */


/*
   1: Report interrupt to CPU
 */
#define CY_U3P_LPP_I2S_ERROR                                (1u << 8) /* <8:8> R:RW:0:No */



/*
   I2S Egress Data Register (Left)
 */
#define CY_U3P_LPP_I2S_EGRESS_DATA_LEFT_ADDRESS             (0xe0000010)
#define CY_U3P_LPP_I2S_EGRESS_DATA_LEFT                     (*(uvint32_t *)(0xe0000010))
#define CY_U3P_LPP_I2S_EGRESS_DATA_LEFT_DEFAULT             (0x00000000)

/*
   Sample to be written to the peripheral in registered mode. Number of bits
   taken depends on sample size (see I2S_CONFIG), other bits are ignored.
 */
#define CY_U3P_LPP_I2S_DATA_MASK                            (0xffffffff) /* <0:31> R:W:0:No */
#define CY_U3P_LPP_I2S_DATA_POS                             (0)



/*
   I2S Egress Data Register (Right)
 */
#define CY_U3P_LPP_I2S_EGRESS_DATA_RIGHT_ADDRESS            (0xe0000014)
#define CY_U3P_LPP_I2S_EGRESS_DATA_RIGHT                    (*(uvint32_t *)(0xe0000014))
#define CY_U3P_LPP_I2S_EGRESS_DATA_RIGHT_DEFAULT            (0x00000000)

/*
   Sample to be written to the peripheral in registered mode. Number of bits
   taken depends on sample size (see I2S_CONFIG), other bits are ignored.
 */
#define CY_U3P_LPP_I2S_DATA_MASK                            (0xffffffff) /* <0:31> R:W:0:No */
#define CY_U3P_LPP_I2S_DATA_POS                             (0)



/*
   I2S Sample Counter Register
 */
#define CY_U3P_LPP_I2S_COUNTER_ADDRESS                      (0xe0000018)
#define CY_U3P_LPP_I2S_COUNTER                              (*(uvint32_t *)(0xe0000018))
#define CY_U3P_LPP_I2S_COUNTER_DEFAULT                      (0x00000000)

/*
   Counter increments by one for every sample written on output.  Counts
   L+R as one sample in stereo mode.  This counter is more reliable to implement
   a s/w PLL because of more periodic behavior.
   This counter will be reset to 0 when I2S_CONFIG.ENABLE=0
 */
#define CY_U3P_LPP_I2S_COUNTER_MASK                         (0xffffffff) /* <0:31> RW:R:0:Yes */
#define CY_U3P_LPP_I2S_COUNTER_POS                          (0)



/*
   I2S Socket Register
 */
#define CY_U3P_LPP_I2S_SOCKET_ADDRESS                       (0xe000001c)
#define CY_U3P_LPP_I2S_SOCKET                               (*(uvint32_t *)(0xe000001c))
#define CY_U3P_LPP_I2S_SOCKET_DEFAULT                       (0x00000000)

/*
   Socket number for left data samples
   0-7: supported
   8-..: reserved
 */
#define CY_U3P_LPP_I2S_LEFT_SOCKET_MASK                     (0x000000ff) /* <0:7> R:RW:0:No */
#define CY_U3P_LPP_I2S_LEFT_SOCKET_POS                      (0)


/*
   Socket number for right data samples
   0-7: supported
   8-..: reserved
 */
#define CY_U3P_LPP_I2S_RIGHT_SOCKET_MASK                    (0x0000ff00) /* <8:15> R:RW:0:No */
#define CY_U3P_LPP_I2S_RIGHT_SOCKET_POS                     (8)



/*
   Block Identification and Version Number
 */
#define CY_U3P_LPP_I2S_ID_ADDRESS                           (0xe00003f0)
#define CY_U3P_LPP_I2S_ID                                   (*(uvint32_t *)(0xe00003f0))
#define CY_U3P_LPP_I2S_ID_DEFAULT                           (0x00010000)

/*
   A unique number identifying the IP in the memory space.
 */
#define CY_U3P_LPP_I2S_BLOCK_ID_MASK                        (0x0000ffff) /* <0:15> :R:0x0000:No */
#define CY_U3P_LPP_I2S_BLOCK_ID_POS                         (0)


/*
   Version number for the IP.
 */
#define CY_U3P_LPP_I2S_BLOCK_VERSION_MASK                   (0xffff0000) /* <16:31> :R:0x0001:No */
#define CY_U3P_LPP_I2S_BLOCK_VERSION_POS                    (16)



/*
   Power, clock and reset control
 */
#define CY_U3P_LPP_I2S_POWER_ADDRESS                        (0xe00003f4)
#define CY_U3P_LPP_I2S_POWER                                (*(uvint32_t *)(0xe00003f4))
#define CY_U3P_LPP_I2S_POWER_DEFAULT                        (0x00000000)

/*
   For blocks that must perform initialization after reset before becoming
   operational, this signal will remain de-asserted until initialization
   is complete.  In other words reading active=1 indicates block is initialized
   and ready for operation.
 */
#define CY_U3P_LPP_I2S_ACTIVE                               (1u << 0) /* <0:0> W:R:0:Yes */


/*
   The block core clock is bypassed with the ATE clock supplied on TCK pin.
   Block operates normally. This mode is designed to allow functional vectors
   to be used during production test.
 */
#define CY_U3P_LPP_I2S_PLL_BYPASS                           (1u << 29) /* <29:29> R:RW:0:No */


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
#define CY_U3P_LPP_I2S_RESETN                               (1u << 31) /* <31:31> R:RW:0:No */



#endif /* _INCLUDED_I2S_REGS_H_ */

/*[]*/

