/*
 ## Cypress FX3 Core Library Header (gpif_regs.h)
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

#ifndef _INCLUDED_GPIF_REGS_H_
#define _INCLUDED_GPIF_REGS_H_

#include <cyu3types.h>


/*
   GPIF Configuration Register
 */
#define CY_U3P_PIB_GPIF_CONFIG_ADDRESS                      (0xe0014000)
#define CY_U3P_PIB_GPIF_CONFIG                              (*(uvint32_t *)(0xe0014000))
#define CY_U3P_PIB_GPIF_CONFIG_DEFAULT                      (0x00000220)

/*
   1: Enable the control Comparator
   0: Disable it.
 */
#define CY_U3P_GPIF_CONF_CTRL_COMP_ENABLE                   (1u << 0) /* <0:0> R:RW:0:No */


/*
   1: Enable the address Comparator
   0: Disable it.
 */
#define CY_U3P_GPIF_CONF_ADDR_COMP_ENABLE                   (1u << 1) /* <1:1> R:RW:0:No */


/*
   1: Enable the data Comparator
   0: Disable it.
 */
#define CY_U3P_GPIF_CONF_DATA_COMP_ENABLE                   (1u << 2) /* <2:2> R:RW:0:No */


/*
   0: Normal clock polarity (clock on posedge)
   1: Inverted clock polarity (clock on negedge)
 */
#define CY_U3P_GPIF_CONF_CLK_INVERT                         (1u << 3) /* <3:3> R:RW:0:No */


/*
   Indicates whether clock is sourced from GCTL or pin.  Has no effect in
   PMMC mode.
   0: External clock input on CLK pin
   1: Internal clock generated from GCTL
 */
#define CY_U3P_GPIF_CONF_CLK_SOURCE                         (1u << 4) /* <4:4> R:RW:0:No */


/*
   Indicates whether to drive CLK pin with GPIF clock.  No effect when CLK_SOURCE=0
   or PMMC mode.
   0: Do not output clock on CLK pin (typical of async mode)
   1: Output clock on CLK pin (typical of sync master mode)
 */
#define CY_U3P_GPIF_CONF_CLK_OUT                            (1u << 5) /* <5:5> R:RW:1:No */


/*
   0: Select 1X clock as the core clock
   1: Select 2X clock as the core clock
 */
#define CY_U3P_GPIF_CONF_DDR_MODE                           (1u << 6) /* <6:6> R:RW:0:No */


/*
   1: Use update_dout (alpha) to also trigger rq_pop (which is normally beta)
   0: rq_pop is a separate beta
 */
#define CY_U3P_GPIF_CONF_DOUT_POP_EN                        (1u << 7) /* <7:7> R:RW:0:No */


/*
   1: Operate in synchronous mode.
   0: Operate in asynchornous mode.
   These mode have different clocking structures as defined in PAS.
   GPIF_CONFIG.SYNC should be set to indicate synchronous or Asynchronous
   MODE before programming GPIF_BUS_CONFIG.DLE* and GPIF_BUS_CONFIG.ALE*
 */
#define CY_U3P_GPIF_CONF_SYNC                               (1u << 8) /* <8:8> R:RW:0:No */


/*
   This parameter influences the GPIF timing and pipeline (see arch spec
   for more details)
   0: <50MHz
   1: 50-100Mz
 */
#define CY_U3P_GPIF_CONF_SYNC_SPEED                         (1u << 9) /* <9:9> R:RW:1:No */


/*
   Endianness of interface when PP_MODE==0
   0: Little Endian
   1: Big Endian
 */
#define CY_U3P_GPIF_CONF_ENDIAN                             (1u << 10) /* <10:10> R:RW:0:No */


/*
   1: Comparator outputs true when any of the unmasked bits change value.
   0: Comparator outputs true when bits match a target value.
 */
#define CY_U3P_GPIF_CONF_ADDR_COMP_TOGGLE                   (1u << 11) /* <11:11> R:RW:0:No */


/*
   1: Comparator outputs true when any of the unmasked bits change value.
   0: Comparator outputs true when bits match a target value.
 */
#define CY_U3P_GPIF_CONF_CTRL_COMP_TOGGLE                   (1u << 12) /* <12:12> R:RW:0:No */


/*
   1: Comparator outputs true when any of the unmasked bits change value.
   0: Comparator outputs true when bits match a target value.
 */
#define CY_U3P_GPIF_CONF_DATA_COMP_TOGGLE                   (1u << 13) /* <13:13> R:RW:0:No */


/*
   1: DQ[15] becomes serial out, DQ[14] serial in. DQ[13:6] carry framing
   signals. These are pushed and popped one bit at a time using push and
   pop signals.Overrides data-width setting. Each serial direction can be
   further configured and disabled in dedicated registers. No other data
   exchange is possible.
   0: Normal parallel mode
 */
#define CY_U3P_GPIF_CONF_SERIAL_MODE                        (1u << 14) /* <14:14> R:RW:0:No */


/*
   0: Normal operation
   1: The thread number for an operation comes from the state description
   rather than the THREAD_CONFIG register (see GPIF_Modes for more information)
 */
#define CY_U3P_GPIF_CONF_THREAD_IN_STATE                    (1u << 15) /* <15:15> R:RW:0:No */


/*
   Overrides the use of AIN[7] to enable selection of register versus DMA
   access on different pins in PP_MODE=1.  If A7OVERRIDE=1, register accesses
   are determined by beta(pp_access) instead.
 */
#define CY_U3P_GPIF_CONF_A7OVERRIDE                         (1u << 19) /* <19:19> R:RW:0:No */


/*
   Main operating mode of PP registers
   0: Master mode or simple slave mode - PP* registers are not honored
   1: P-Port mode - PP* registers are used to setup transfers
 */
#define CY_U3P_GPIF_CONF_PP_MODE                            (1u << 30) /* <30:30> R:RW:0:No */


/*
   Enables entire GPIF block.
 */
#define CY_U3P_GPIF_CONF_ENABLE                             (1u << 31) /* <31:31> R:RW:0:Yes */



/*
   Bus Configuration Register
 */
#define CY_U3P_PIB_GPIF_BUS_CONFIG_ADDRESS                  (0xe0014004)
#define CY_U3P_PIB_GPIF_BUS_CONFIG                          (*(uvint32_t *)(0xe0014004))
#define CY_U3P_PIB_GPIF_BUS_CONFIG_DEFAULT                  (0x00000000)

/*
   Number of pins allocated to GPIF interface - needs to be consistent with
   GCTL_IOMATRIX:
   0: 47-pin interface
   1: 43-pin interface (no S1)
   2: 35-pin interface (no S0)
   3: 31-pin interface (no S0/S1)
 */
#define CY_U3P_GPIF_PIN_COUNT_MASK                          (0x00000003) /* <0:1> R:RW:0:No */
#define CY_U3P_GPIF_PIN_COUNT_POS                           (0)


/*
   0: DQ is 8b wide
   1: DQ is 16b wide
   2: DQ is 24b wide
   3: DQ is 32b wide
 */
#define CY_U3P_GPIF_BUS_WIDTH_MASK                          (0x0000000c) /* <2:3> R:RW:0:No */
#define CY_U3P_GPIF_BUS_WIDTH_POS                           (2)


/*
   1: Address is sampled from DQ, time multiplexed with data.
   0: Address and data are separate lines.
 */
#define CY_U3P_GPIF_MUX_MODE                                (1u << 4) /* <4:4> R:RW:0:No */


/*
   Number of control lines overridden by address lines.  Control signals
   CTRL[15] to CTRL[16-ADR_CTRL] are not connected to pins.  Instead those
   pins are designated as address signals. Which address signals depends
   on the other mode fields above; see architecture spec for details.  In
   other words:  if ADR_CTRL=0 all CTRL lines are connected to pins, if ADR_CTRL=1,
   CTRL[15] is not connected and so on.
 */
#define CY_U3P_GPIF_ADR_CTRL_MASK                           (0x000001e0) /* <5:8> R:RW:0:No */
#define CY_U3P_GPIF_ADR_CTRL_POS                            (5)


/*
   CTRL[0] is CE and should be used to disable DQ drivers
 */
#define CY_U3P_GPIF_CE_PRESENT                              (1u << 9) /* <9:9> R:RW:0:No */


/*
   CTRL[1] is WE and should be used to disable DQ drivers
   Can be used together with DLE_PRESENT
 */
#define CY_U3P_GPIF_WE_PRESENT                              (1u << 10) /* <10:10> R:RW:0:No */


/*
   CTRL[1] is DLE and should be used to latch data from DQ lines
   Can be used together with WE_PRESENT
 */
#define CY_U3P_GPIF_DLE_PRESENT                             (1u << 11) /* <11:11> R:RW:0:No */


/*
   CTRL[2] is OE and should be used to tri-state DQ lines.
   If WE_PRESENT=1 also, then OE will take precedence over WE.  In other
   words, when WE is asserted, then output drivers are off, regardless of
   value of OE input.
 */
#define CY_U3P_GPIF_OE_PRESENT                              (1u << 12) /* <12:12> R:RW:0:No */


/*
   CTRL[4] is directly influenced by CTRL[3]/DACK as defined by DRQ_MODE
   This setting also overrides the CTRL_BUS_SELECT selection for this pin,
   in favor of betas 'assert drq' and 'deassert_drq'
 */
#define CY_U3P_GPIF_DRQ_PRESENT                             (1u << 13) /* <13:13> R:RW:0:No */


/*
   CTRL[7] is to be treated as IO that is driven out when the alpha specified
   in FIO0_CONF is asserted (ignore CTRL_BUS_DIRECTION)
 */
#define CY_U3P_GPIF_FIO0_PRESENT                            (1u << 14) /* <14:14> R:RW:0:No */


/*
   CTRL[8] is to be treated as IO that is driven out when the alpha specified
   in FIO1_CONF is asserted (ignore CTRL_BUS_DIRECTION)
 */
#define CY_U3P_GPIF_FIO1_PRESENT                            (1u << 15) /* <15:15> R:RW:0:No */


/*
   CTRL[9] is connected to the selected control counter bit instead of a
   control signal
 */
#define CY_U3P_GPIF_CNTR_PRESENT                            (1u << 16) /* <16:16> R:RW:0:No */


/*
   CTRL[10] is ALE used to latch address from the DQ lines.
 */
#define CY_U3P_GPIF_ALE_PRESENT                             (1u << 17) /* <17:17> R:RW:0:No */


/*
   0: Assert DRQ on deassertion of DACK
   1: Assert DRQ on assertion of DACK
   2: Deassert DRQ on deassertion of DACK
   3: Deassert DRQ on assertion of DACK
 */
#define CY_U3P_GPIF_DRQ_MODE_MASK                           (0x000c0000) /* <18:19> R:RW:0:No */
#define CY_U3P_GPIF_DRQ_MODE_POS                            (18)


/*
   1: Assert DRQ on rising edge of DMA_READY. Typical case, DRQ_MODE=2, this
   bit 1.
   0: Do nothing
 */
#define CY_U3P_GPIF_DRQ_ASSERT_MODE                         (1u << 20) /* <20:20> R:RW:0:No */


/*
   0: Normal operation of INT pin
   1: Override INT pin and connect to CTRL[15]
 */
#define CY_U3P_GPIF_INT_CTRL                                (1u << 22) /* <22:22> R:RW:0:No */


/*
   0: Normal operation
   1: CTRL[0] is CE; it should be used to clock gate most of GPIF when deasserted
 */
#define CY_U3P_GPIF_CE_CLKSTOP                              (1u << 23) /* <23:23> R:RW:0:No */


/*
   Designates control to be used to enable output drivers of FIO0 (CTRL[7])
   0 to 3: Use the alpha 4 to 7 to switch FIO0 direction.
   8-11: Use beta 0-3.
 */
#define CY_U3P_GPIF_FIO0_CONF_MASK                          (0x0f000000) /* <24:27> R:RW:0:No */
#define CY_U3P_GPIF_FIO0_CONF_POS                           (24)


/*
   Designates control to be used to enable output drivers of FIO1 (CTRL[8])
   0 to 3: Use the alpha  4-7 to switch FIO1 direction.
   8-11: Use beta 0-3.
 */
#define CY_U3P_GPIF_FIO1_CONF_MASK                          (0xf0000000) /* <28:31> R:RW:0:No */
#define CY_U3P_GPIF_FIO1_CONF_POS                           (28)



/*
   Bus Configuration Register #2
 */
#define CY_U3P_PIB_GPIF_BUS_CONFIG2_ADDRESS                 (0xe0014008)
#define CY_U3P_PIB_GPIF_BUS_CONFIG2                         (*(uvint32_t *)(0xe0014008))
#define CY_U3P_PIB_GPIF_BUS_CONFIG2_DEFAULT                 (0x00000000)

/*
   0: Normal operation
   1,2,3: STATE7 indicates Lambda number to be used for state number bit
   7
   2,3: STATE6 indicates Lambda number to be used for state number bit 6
   3: STATE5 indicates Lambda number to be used for state number bit 5
 */
#define CY_U3P_GPIF_STATE_FROM_CTRL_MASK                    (0x00000007) /* <0:2> R:RW:0:No */
#define CY_U3P_GPIF_STATE_FROM_CTRL_POS                     (0)


/*
   Lambda number to be used for state number bit 5
 */
#define CY_U3P_GPIF_STATE5_MASK                             (0x00001f00) /* <8:12> R:RW:0:No */
#define CY_U3P_GPIF_STATE5_POS                              (8)


/*
   Lambda number to be used for state number bit 6
 */
#define CY_U3P_GPIF_STATE6_MASK                             (0x001f0000) /* <16:20> R:RW:0:No */
#define CY_U3P_GPIF_STATE6_POS                              (16)


/*
   Lambda number to be used for state number bit 7
 */
#define CY_U3P_GPIF_STATE7_MASK                             (0x1f000000) /* <24:28> R:RW:0:No */
#define CY_U3P_GPIF_STATE7_POS                              (24)



/*
   Address/Data configuration register
 */
#define CY_U3P_PIB_GPIF_AD_CONFIG_ADDRESS                   (0xe001400c)
#define CY_U3P_PIB_GPIF_AD_CONFIG                           (*(uvint32_t *)(0xe001400c))
#define CY_U3P_PIB_GPIF_AD_CONFIG_DEFAULT                   (0x00000000)

/*
   Data lines are
   0: Always outputs
   1: Always inputs
   2: Direction controlled by the alpha: "dq_oen"
   3: Reserved
   Note that CTRL[2] can be OE, controlling the data output drivers directly,
   overriding what's specified here (this field should be set to 0 if that's
   used)
 */
#define CY_U3P_GPIF_DQ_OEN_CFG_MASK                         (0x00000003) /* <0:1> R:RW:0:No */
#define CY_U3P_GPIF_DQ_OEN_CFG_POS                          (0)


/*
   Address lines (if there are any) are
   0: Always outputs
   1: Always inputs
   2: Controlled by the beta: "a_oen"
   3: Reserved
 */
#define CY_U3P_GPIF_A_OEN_CFG_MASK                          (0x0000000c) /* <2:3> R:RW:0:No */
#define CY_U3P_GPIF_A_OEN_CFG_POS                           (2)


/*
   0: Connect AIN to the active socket number of thread specified by AIN_DATA
   1: Connect AIN to INGRESS_ADDRESS register
   2: Connect AIN to the socket pointed to by ADDRESS_THREAD register
 */
#define CY_U3P_GPIF_AIN_SELECT_MASK                         (0x00000030) /* <4:5> R:RW:0:No */
#define CY_U3P_GPIF_AIN_SELECT_POS                          (4)


/*
   0:  Connect AOUT to ADDR_COUNTER
   1: Connect AOUT to EGRESS_ADDRESS register
   2: Connect AOUT to the socket pointed to by ADDRESS_THREAD register
 */
#define CY_U3P_GPIF_AOUT_SELECT_MASK                        (0x000000c0) /* <6:7> R:RW:0:No */
#define CY_U3P_GPIF_AOUT_SELECT_POS                         (6)


/*
   0: Connect DOUT to the socket pointed to by AIN_DATA or EGRESS_DATA register
   (as determined by beta 'register_access')
   1: Connect DOUT to DATA_COUNTER
 */
#define CY_U3P_GPIF_DOUT_SELECT                             (1u << 8) /* <8:8> R:RW:0:No */


/*
   This field determines which thread number to use for data accesses:
   0: specified by A1:A0.
   1: specified by DATA_THREAD
   If AIN_SELECT=0 this field also determines the thread number for which
   to change the active socket on awq_push:
   0: The active socket of thread A1:A0 is changed (3 bits only)
   1: The active socket of thread DATA_THREAD is changed (full 5 bits)
 */
#define CY_U3P_GPIF_AIN_DATA                                (1u << 9) /* <9:9> R:RW:0:No */


/*
   Thread number to be used for addresses;only relevant when AIN_SELECT!=0
   When this register is used to select the thread it must have been initialized
   by firmware.  When both are used, ADDRESS_THREAD must be different from
   DATA_THREAD.
 */
#define CY_U3P_GPIF_ADDRESS_THREAD_MASK                     (0x00030000) /* <16:17> R:RW:0:Yes */
#define CY_U3P_GPIF_ADDRESS_THREAD_POS                      (16)


/*
   Thread number to be used for data; only relevant when AIN_DATA=1
   When this register is used to select the thread it must have been initialized
   by firmware.   When both are used, ADDRESS_THREAD must be different from
   DATA_THREAD.
 */
#define CY_U3P_GPIF_DATA_THREAD_MASK                        (0x000c0000) /* <18:19> R:RW:0:Yes */
#define CY_U3P_GPIF_DATA_THREAD_POS                         (18)



/*
   GPIF Status Register
 */
#define CY_U3P_PIB_GPIF_STATUS_ADDRESS                      (0xe0014010)
#define CY_U3P_PIB_GPIF_STATUS                              (*(uvint32_t *)(0xe0014010))
#define CY_U3P_PIB_GPIF_STATUS_DEFAULT                      (0x000f0000)

/*
   1: GPIF has reached the DONE state.  Non sticky.
 */
#define CY_U3P_GPIF_STATUS_GPIF_DONE                        (1u << 0) /* <0:0> RW:R:0:Yes */


/*
   Indicates that GPIF state machine has raised an interrupt.
 */
#define CY_U3P_GPIF_STATUS_GPIF_INTR                        (1u << 1) /* <1:1> RW:R:0:Yes */


/*
   Indicates that the SWITCH_TIMEOUT was reached (see WAVEFORM_SWITCH).
   This bit clears when a new WAVEFORM_SWTICH is initiated.
 */
#define CY_U3P_GPIF_STATUS_SWITCH_TIMEOUT                   (1u << 2) /* <2:2> RW:R:0:Yes */


/*
   Indicates that an incorrect CRC was received
 */
#define CY_U3P_GPIF_STATUS_CRC_ERROR                        (1u << 3) /* <3:3> RW:R:0:Yes */


/*
   Address counter is at limit
 */
#define CY_U3P_GPIF_STATUS_ADDR_COUNT_HIT                   (1u << 4) /* <4:4> RW:R:0:Yes */


/*
   Data counter is at limit
 */
#define CY_U3P_GPIF_STATUS_DATA_COUNT_HIT                   (1u << 5) /* <5:5> RW:R:0:Yes */


/*
   Control counter is at limit
 */
#define CY_U3P_GPIF_STATUS_CTRL_COUNT_HIT                   (1u << 6) /* <6:6> RW:R:0:Yes */


/*
   Address comparator hits
 */
#define CY_U3P_GPIF_STATUS_ADDR_COMP_HIT                    (1u << 7) /* <7:7> RW:R:0:Yes */


/*
   Data comparator hits
 */
#define CY_U3P_GPIF_STATUS_DATA_COMP_HIT                    (1u << 8) /* <8:8> RW:R:0:Yes */


/*
   Control comparator hits
 */
#define CY_U3P_GPIF_STATUS_CTRL_COMP_HIT                    (1u << 9) /* <9:9> RW:R:0:Yes */


/*
   CPU tried to access waveform memory w/o clearing WAVEFORM_VALID
 */
#define CY_U3P_GPIF_STATUS_WAVEFORM_BUSY                    (1u << 10) /* <10:10> RW:R:0:Yes */


/*
   Indicates corresponding EGRESS_DATA register is empty
 */
#define CY_U3P_GPIF_STATUS_EG_DATA_EMPTY_MASK               (0x000f0000) /* <16:19> RW:R:0xf:Yes */
#define CY_U3P_GPIF_STATUS_EG_DATA_EMPTY_POS                (16)


/*
   Indicates corresponding INGRESS_DATA register is full.
 */
#define CY_U3P_GPIF_STATUS_IN_DATA_VALID_MASK               (0x00f00000) /* <20:23> RW:R:0:Yes */
#define CY_U3P_GPIF_STATUS_IN_DATA_VALID_POS                (20)


/*
   State that raised the interrupt through GPIF_INTR. Pl. note that these
   bits do not have individual interrupt and mask bits in GPIF_INTR and GPIF_MASK
 */
#define CY_U3P_GPIF_STATUS_INTERRUPT_STATE_MASK             (0xff000000) /* <24:31> RW:R:0:Yes */
#define CY_U3P_GPIF_STATUS_INTERRUPT_STATE_POS              (24)



/*
   GPIF Interrupt Request Register
 */
#define CY_U3P_PIB_GPIF_INTR_ADDRESS                        (0xe0014014)
#define CY_U3P_PIB_GPIF_INTR                                (*(uvint32_t *)(0xe0014014))
#define CY_U3P_PIB_GPIF_INTR_DEFAULT                        (0x00000000)

/*
   Interrupt request corresponding to same bit in GPIF_STATUS
 */
#define CY_U3P_GPIF_INTR_GPIF_DONE                          (1u << 0) /* <0:0> RW1S:RW1C:0:No */


/*
   Interrupt request corresponding to same bit in GPIF_STATUS
 */
#define CY_U3P_GPIF_INTR_GPIF_INTR                          (1u << 1) /* <1:1> RW1S:RW1C:0:No */


/*
   Interrupt request corresponding to same bit in GPIF_STATUS
 */
#define CY_U3P_GPIF_INTR_SWITCH_TIMEOUT                     (1u << 2) /* <2:2> RW1S:RW1C:0:No */


/*
   Interrupt request corresponding to same bit in GPIF_STATUS
 */
#define CY_U3P_GPIF_INTR_CRC_ERROR                          (1u << 3) /* <3:3> RW1S:RW1C:0:No */


/*
   Interrupt request corresponding to same bit in GPIF_STATUS
 */
#define CY_U3P_GPIF_INTR_ADDR_COUNT_HIT                     (1u << 4) /* <4:4> RW1S:RW1C:0:No */


/*
   Interrupt request corresponding to same bit in GPIF_STATUS
 */
#define CY_U3P_GPIF_INTR_DATA_COUNT_HIT                     (1u << 5) /* <5:5> RW1S:RW1C:0:No */


/*
   Interrupt request corresponding to same bit in GPIF_STATUS
 */
#define CY_U3P_GPIF_INTR_CTRL_COUNT_HIT                     (1u << 6) /* <6:6> RW1S:RW1C:0:No */


/*
   Interrupt request corresponding to same bit in GPIF_STATUS
 */
#define CY_U3P_GPIF_INTR_ADDR_COMP_HIT                      (1u << 7) /* <7:7> RW1S:RW1C:0:No */


/*
   Interrupt request corresponding to same bit in GPIF_STATUS
 */
#define CY_U3P_GPIF_INTR_DATA_COMP_HIT                      (1u << 8) /* <8:8> RW1S:RW1C:0:No */


/*
   Interrupt request corresponding to same bit in GPIF_STATUS
 */
#define CY_U3P_GPIF_INTR_CTRL_COMP_HIT                      (1u << 9) /* <9:9> RW1S:RW1C:0:No */


/*
   Interrupt request corresponding to same bit in GPIF_STATUS
 */
#define CY_U3P_GPIF_INTR_WAVEFORM_BUSY                      (1u << 10) /* <10:10> RW1S:RW1C:0:No */


/*
   Interrupt request corresponding to same bit in GPIF_STATUS
 */
#define CY_U3P_GPIF_INTR_EG_DATA_EMPTY_MASK                 (0x000f0000) /* <16:19> RW1S:RW1C:0:No */
#define CY_U3P_GPIF_INTR_EG_DATA_EMPTY_POS                  (16)


/*
   Interrupt request corresponding to same bit in GPIF_STATUS
 */
#define CY_U3P_GPIF_INTR_IN_DATA_VALID_MASK                 (0x00f00000) /* <20:23> RW1S:RW1C:0:No */
#define CY_U3P_GPIF_INTR_IN_DATA_VALID_POS                  (20)



/*
   GPIF Interrupt Mask Register
 */
#define CY_U3P_PIB_GPIF_INTR_MASK_ADDRESS                   (0xe0014018)
#define CY_U3P_PIB_GPIF_INTR_MASK                           (*(uvint32_t *)(0xe0014018))
#define CY_U3P_PIB_GPIF_INTR_MASK_DEFAULT                   (0x00000000)

/*
   Mask bit that controls reporting of corresponding bit in GPIF_INTR
 */
#define CY_U3P_GPIF_INTR_GPIF_DONE                          (1u << 0) /* <0:0> R:RW:0:No */


/*
   Mask bit that controls reporting of corresponding bit in GPIF_INTR
 */
#define CY_U3P_GPIF_INTR_GPIF_INTR                          (1u << 1) /* <1:1> R:RW:0:No */


/*
   Mask bit that controls reporting of corresponding bit in GPIF_INTR
 */
#define CY_U3P_GPIF_INTR_SWITCH_TIMEOUT                     (1u << 2) /* <2:2> R:RW:0:No */


/*
   Mask bit that controls reporting of corresponding bit in GPIF_INTR
 */
#define CY_U3P_GPIF_INTR_CRC_ERROR                          (1u << 3) /* <3:3> R:RW:0:No */


/*
   Mask bit that controls reporting of corresponding bit in GPIF_INTR
 */
#define CY_U3P_GPIF_INTR_ADDR_COUNT_HIT                     (1u << 4) /* <4:4> R:RW:0:No */


/*
   Mask bit that controls reporting of corresponding bit in GPIF_INTR
 */
#define CY_U3P_GPIF_INTR_DATA_COUNT_HIT                     (1u << 5) /* <5:5> R:RW:0:No */


/*
   Mask bit that controls reporting of corresponding bit in GPIF_INTR
 */
#define CY_U3P_GPIF_INTR_CTRL_COUNT_HIT                     (1u << 6) /* <6:6> R:RW:0:No */


/*
   Mask bit that controls reporting of corresponding bit in GPIF_INTR
 */
#define CY_U3P_GPIF_INTR_ADDR_COMP_HIT                      (1u << 7) /* <7:7> R:RW:0:No */


/*
   Mask bit that controls reporting of corresponding bit in GPIF_INTR
 */
#define CY_U3P_GPIF_INTR_DATA_COMP_HIT                      (1u << 8) /* <8:8> R:RW:0:No */


/*
   Mask bit that controls reporting of corresponding bit in GPIF_INTR
 */
#define CY_U3P_GPIF_INTR_CTRL_COMP_HIT                      (1u << 9) /* <9:9> R:RW:0:No */


/*
   Mask bit that controls reporting of corresponding bit in GPIF_INTR
 */
#define CY_U3P_GPIF_INTR_WAVEFORM_BUSY                      (1u << 10) /* <10:10> R:RW:0:No */


/*
   Mask bit that controls reporting of corresponding bit in GPIF_INTR
 */
#define CY_U3P_GPIF_INTR_EG_DATA_EMPTY_MASK                 (0x000f0000) /* <16:19> R:RW:0:No */
#define CY_U3P_GPIF_INTR_EG_DATA_EMPTY_POS                  (16)


/*
   Mask bit that controls reporting of corresponding bit in GPIF_INTR
 */
#define CY_U3P_GPIF_INTR_IN_DATA_VALID_MASK                 (0x00f00000) /* <20:23> R:RW:0:No */
#define CY_U3P_GPIF_INTR_IN_DATA_VALID_POS                  (20)



/*
   GPIF Serial In Configuration Register
 */
#define CY_U3P_PIB_GPIF_SERIAL_IN_CONFIG_ADDRESS            (0xe001401c)
#define CY_U3P_PIB_GPIF_SERIAL_IN_CONFIG                    (*(uvint32_t *)(0xe001401c))
#define CY_U3P_PIB_GPIF_SERIAL_IN_CONFIG_DEFAULT            (0x00000082)

/*
   1: Enable Serial input in SERIAL_MODE
   0: Disable serial input
 */
#define CY_U3P_GPIF_SERIN_ENABLE                            (1u << 0) /* <0:0> R:RW:0:No */


/*
   1: Socket number is specified in THREAD_NUMBER.
   0: Socket number is controlled by the beta "Thread Number" and the active
   socket for the corresponding thread (THREAD_CONFIG.THREAD_SOCK).
 */
#define CY_U3P_GPIF_SERIN_SOCKET_MODE                       (1u << 1) /* <1:1> R:RW:1:No */


/*
   Push the deserialized data in this socket.
 */
#define CY_U3P_GPIF_SERIN_THREAD_NUMBER_MASK                (0x0000000c) /* <2:3> R:RW:0:No */
#define CY_U3P_GPIF_SERIN_THREAD_NUMBER_POS                 (2)


/*
   Packing is only done when FRAME_MODE=0
   1: If words are 1, 2, 4, 8 or 16 bits long, they are to be packed into
   a 32-bit field before comitting to socket. Other word sizes are zero-padded
   at the MSB to the next byte or half-word  (16-bit) and then packed to
   a 32-bit value.
   0: Push individual words as soon as they are formed, with MSBs zero padded.
 */
#define CY_U3P_GPIF_SERIN_PACK                              (1u << 7) /* <7:7> R:RW:1:No */


/*
   0: WORD_SIZE based.
   1: sof to sof
   2: sof to eof (allows for null/garbage data between frames)
   Use of early and late variants for framing is not suported.
 */
#define CY_U3P_GPIF_SERIN_FRAME_MODE_MASK                   (0x00000300) /* <8:9> R:RW:0:No */
#define CY_U3P_GPIF_SERIN_FRAME_MODE_POS                    (8)


/*
   WORD_SIZE-1 for the serial interface. For example 0: Means 1 bit is to
   be formed as word, 9 means 10 bits make a word and so on.
 */
#define CY_U3P_GPIF_SERIN_WORD_SIZE_MASK                    (0x00007c00) /* <10:14> R:RW:0:No */
#define CY_U3P_GPIF_SERIN_WORD_SIZE_POS                     (10)


/*
   00: No Parity
   01: Even Parity
   10: Odd Parity
   11: Sticky.
   Parity errors are reported to PP_EVENT register. This register can be
   written by the AP through the mailbox write mechanism.
 */
#define CY_U3P_GPIF_SERIN_PARITY_MASK                       (0x00018000) /* <15:16> R:RW:0:No */
#define CY_U3P_GPIF_SERIN_PARITY_POS                        (15)


/*
   Parity bit in the sticky mode. Parity bit is always the last received
   bit in a word in sticky mode.
 */
#define CY_U3P_GPIF_SERIN_STICKY                            (1u << 17) /* <17:17> R:RW:0:No */



/*
   GPIF Serial Out Configuration Register
 */
#define CY_U3P_PIB_GPIF_SERIAL_OUT_CONFIG_ADDRESS           (0xe0014020)
#define CY_U3P_PIB_GPIF_SERIAL_OUT_CONFIG                   (*(uvint32_t *)(0xe0014020))
#define CY_U3P_PIB_GPIF_SERIAL_OUT_CONFIG_DEFAULT           (0x00000782)

/*
   1: Enable Serial output in SERIAL_MODE
   0: Disable serial output
 */
#define CY_U3P_GPIF_SEROUT_ENABLE                           (1u << 0) /* <0:0> R:RW:0:No */


/*
   1: Socket number is specified in THREAD_NUMBER.
   0: Socket number is controlled by the beta "Thread Number" and the active
   socket for the corresponding thread (THREAD_CONFIG.THREAD_SOCK).
 */
#define CY_U3P_GPIF_SEROUT_SOCKET_MODE                      (1u << 1) /* <1:1> R:RW:1:No */


/*
   Pop the data to be serialized from this socket.
 */
#define CY_U3P_GPIF_SEROUT_THREAD_NUMBER_MASK               (0x0000000c) /* <2:3> R:RW:0:No */
#define CY_U3P_GPIF_SEROUT_THREAD_NUMBER_POS                (2)


/*
   1: If words are 1, 2, 4, 8 or 16 bits long, they are receivved packed
   into a 32-bit field before comitting to socket. Other word sizes are received
   zero-padded at the MSBs to the next byte or half-word  (16-bit) and then
   packed to a 32-bit value.
   0: Every 32-bit value has only one word regardless of size. MSBs are zero
   padded.
 */
#define CY_U3P_GPIF_SEROUT_PACK                             (1u << 7) /* <7:7> R:RW:1:No */


/*
   WORD_SIZE-1 for the serial interface. For example 0: Means 1 bit is to
   be formed as word; 9 means 10 bits make a word and so on. At the first
   bit of a word an sof is generated, at last bit, an eof is generated by
   internal hardware. SOF, EOF and their offset variants are connected to
   DQ lines.
 */
#define CY_U3P_GPIF_SEROUT_WORD_SIZE_MASK                   (0x00001f00) /* <8:12> R:RW:7:No */
#define CY_U3P_GPIF_SEROUT_WORD_SIZE_POS                    (8)


/*
   Generate a late sof after so many cycles. Sof and late sof are distinct
   signals. Value 0 would make them co-incident, 1 would delay late sof by
   1 cycle and so on. Offset > WORD_SIZE will lead to unpredictable behavior.
 */
#define CY_U3P_GPIF_SEROUT_SOF_OFFSET_MASK                  (0x0003e000) /* <13:17> R:RW:0:No */
#define CY_U3P_GPIF_SEROUT_SOF_OFFSET_POS                   (13)


/*
   Generate an learly eof  so many cycles in advance of eof. Eof and late
   eof are distinct signals. Value 0 would make them co-incident, 1 would
   advance early eof by 1 cycle and so on. Offset > WORD size will lead to
   unpredictable behaviour.
 */
#define CY_U3P_GPIF_SEROUT_EOF_OFFSET_MASK                  (0x007c0000) /* <18:22> R:RW:0:No */
#define CY_U3P_GPIF_SEROUT_EOF_OFFSET_POS                   (18)



/*
   Control Bus in/out direction
 */
#define CY_U3P_PIB_GPIF_CTRL_BUS_DIRECTION_ADDRESS          (0xe0014024)
#define CY_U3P_PIB_GPIF_CTRL_BUS_DIRECTION                  (*(uvint32_t *)(0xe0014024))
#define CY_U3P_PIB_GPIF_CTRL_BUS_DIRECTION_DEFAULT          (0x00000000)

/*
   Bit at (bit_number/2) has following direction:
   00: Input
   01:Output
   10: Bidirectional IO.
   11: Open drain IO.
 */
#define CY_U3P_GPIF_DIRECTION_MASK                          (0xffffffff) /* <0:31> R:RW:0:No */
#define CY_U3P_GPIF_DIRECTION_POS                           (0)



/*
   Control bus default values
 */
#define CY_U3P_PIB_GPIF_CTRL_BUS_DEFAULT_ADDRESS            (0xe0014028)
#define CY_U3P_PIB_GPIF_CTRL_BUS_DEFAULT                    (*(uvint32_t *)(0xe0014028))
#define CY_U3P_PIB_GPIF_CTRL_BUS_DEFAULT_DEFAULT            (0x00000000)

/*
   One bit for each CTRL signal indicating default value
   0:  Asserted (see POLARITY)
   1:  De-asserted (see POLARITY)
 */
#define CY_U3P_GPIF_DEFAULT_MASK                            (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_GPIF_DEFAULT_POS                             (0)



/*
   Control bus signal polarity
 */
#define CY_U3P_PIB_GPIF_CTRL_BUS_POLARITY_ADDRESS           (0xe001402c)
#define CY_U3P_PIB_GPIF_CTRL_BUS_POLARITY                   (*(uvint32_t *)(0xe001402c))
#define CY_U3P_PIB_GPIF_CTRL_BUS_POLARITY_DEFAULT           (0x00000000)

/*
   One bit for each CTRL signal indicating polarity
   0: Asserted when 1
   1: Asserted when 0
 */
#define CY_U3P_GPIF_POLARITY_MASK                           (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_GPIF_POLARITY_POS                            (0)



/*
   Control bus output toggle mode
 */
#define CY_U3P_PIB_GPIF_CTRL_BUS_TOGGLE_ADDRESS             (0xe0014030)
#define CY_U3P_PIB_GPIF_CTRL_BUS_TOGGLE                     (*(uvint32_t *)(0xe0014030))
#define CY_U3P_PIB_GPIF_CTRL_BUS_TOGGLE_DEFAULT             (0x00000000)

/*
   One bit for each CTRL signal indicating toggle mode
   0: Normal mode, set value from alpha/beta
   1: Toggle mode, toggle value when alpha/beta is 1, do nothing when 0
 */
#define CY_U3P_GPIF_TOGGLE_MASK                             (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_GPIF_TOGGLE_POS                              (0)



/*
   Control bus connection matrix register
 */
#define CY_U3P_PIB_GPIF_CTRL_BUS_SELECT_ADDRESS(n)          (0xe0014034 + ((n) * (0x0004)))
#define CY_U3P_PIB_GPIF_CTRL_BUS_SELECT(n)                  (*(uvint32_t *)(0xe0014034 + ((n) * 0x0004)))
#define CY_U3P_PIB_GPIF_CTRL_BUS_SELECT_DEFAULT             (0x00000000)

/*
   For each omega, 5-bits specify what is driven at to the output:
   0-3: Connect to alpha 0-3
   8-11: Connect to beta 0-3
   16-19: Empty/Full flags for thread 0-3
   20-23: Partial Flag for thread 0-3
   24: Empty/Full flag for current thread
   25: Partial Flag for current thread
   26: PP_DRQR5 signal (see PP_DRQR5MASK)
   27-31: Connected to logic 0 (cannot be used together with CTRL_BUS_TOGGLE)
 */
#define CY_U3P_GPIF_OMEGA_INDEX_MASK                        (0x0000001f) /* <0:4> R:RW:0:No */
#define CY_U3P_GPIF_OMEGA_INDEX_POS                         (0)



/*
   Control counter configuration
 */
#define CY_U3P_PIB_GPIF_CTRL_COUNT_CONFIG_ADDRESS           (0xe0014074)
#define CY_U3P_PIB_GPIF_CTRL_COUNT_CONFIG                   (*(uvint32_t *)(0xe0014074))
#define CY_U3P_PIB_GPIF_CTRL_COUNT_CONFIG_DEFAULT           (0x00000006)

/*
   0: This counter is not used.
   1: This counter is used.
 */
#define CY_U3P_GPIF_CC_ENABLE                               (1u << 0) /* <0:0> R:RW:0:Yes */


/*
   0: Down Count
   1: Up Count
 */
#define CY_U3P_GPIF_CC_DOWN_UP                              (1u << 1) /* <1:1> R:RW:1:No */


/*
   0: Saturate on reaching the limit
   1: Reload on reaching the limit
 */
#define CY_U3P_GPIF_CC_RELOAD                               (1u << 2) /* <2:2> R:RW:1:No */


/*
   1: SW writes one to reset/load the counter
   0: HW write 0 to signal that counter has reset
 */
#define CY_U3P_GPIF_CC_SW_RESET                             (1u << 3) /* <3:3> RW0C:RW1S:0:Yes */


/*
   Connect the specified bit of this counter to CTRL[9]
 */
#define CY_U3P_GPIF_CC_CONNECT_MASK                         (0x000000f0) /* <4:7> R:RW:0:No */
#define CY_U3P_GPIF_CC_CONNECT_POS                          (4)



/*
   Control counter reset register
 */
#define CY_U3P_PIB_GPIF_CTRL_COUNT_RESET_ADDRESS            (0xe0014078)
#define CY_U3P_PIB_GPIF_CTRL_COUNT_RESET                    (*(uvint32_t *)(0xe0014078))
#define CY_U3P_PIB_GPIF_CTRL_COUNT_RESET_DEFAULT            (0x00000000)

/*
   Reset counter to this value. Reload to this value when limit is reached
   if specified.
 */
#define CY_U3P_GPIF_CC_RESET_LOAD_MASK                      (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_GPIF_CC_RESET_LOAD_POS                       (0)



/*
   Control counter limit register
 */
#define CY_U3P_PIB_GPIF_CTRL_COUNT_LIMIT_ADDRESS            (0xe001407c)
#define CY_U3P_PIB_GPIF_CTRL_COUNT_LIMIT                    (*(uvint32_t *)(0xe001407c))
#define CY_U3P_PIB_GPIF_CTRL_COUNT_LIMIT_DEFAULT            (0x0000ffff)

/*
   Stop counting when counter reaches this value
 */
#define CY_U3P_GPIF_CC_LIMIT_MASK                           (0x0000ffff) /* <0:15> R:RW:0xFFFF:No */
#define CY_U3P_GPIF_CC_LIMIT_POS                            (0)



/*
   Address counter configuration
 */
#define CY_U3P_PIB_GPIF_ADDR_COUNT_CONFIG_ADDRESS           (0xe0014080)
#define CY_U3P_PIB_GPIF_ADDR_COUNT_CONFIG                   (*(uvint32_t *)(0xe0014080))
#define CY_U3P_PIB_GPIF_ADDR_COUNT_CONFIG_DEFAULT           (0x0000010a)

/*
   0: This counter is not used.
   1: This counter is used.
 */
#define CY_U3P_GPIF_AC_ENABLE                               (1u << 0) /* <0:0> R:RW:0:Yes */


/*
   0: Saturate on reaching the limit
   1: Reload on reaching the limit
 */
#define CY_U3P_GPIF_AC_RELOAD                               (1u << 1) /* <1:1> R:RW:1:No */


/*
   1: SW writes one to reset/load the counter
   0: HW write 0 to signal that counter has reset
 */
#define CY_U3P_GPIF_AC_SW_RESET                             (1u << 2) /* <2:2> RW0C:RW1S:0:Yes */


/*
   0: Down Count
   1: Up Count
 */
#define CY_U3P_GPIF_AC_DOWN_UP                              (1u << 3) /* <3:3> R:RW:1:No */


/*
    8-bit quantity to be added/subtracted to the counter on each clock
 */
#define CY_U3P_GPIF_AC_INCREMENT_MASK                       (0x0000ff00) /* <8:15> R:RW:1:No */
#define CY_U3P_GPIF_AC_INCREMENT_POS                        (8)



/*
   Address counter reset register
 */
#define CY_U3P_PIB_GPIF_ADDR_COUNT_RESET_ADDRESS            (0xe0014084)
#define CY_U3P_PIB_GPIF_ADDR_COUNT_RESET                    (*(uvint32_t *)(0xe0014084))
#define CY_U3P_PIB_GPIF_ADDR_COUNT_RESET_DEFAULT            (0x00000000)

/*
   Reset counter to this value. Reload to this value when limit is reached
   if specified.
 */
#define CY_U3P_GPIF_AC_RESET_LOAD_MASK                      (0xffffffff) /* <0:31> R:RW:0:No */
#define CY_U3P_GPIF_AC_RESET_LOAD_POS                       (0)



/*
   Address counter limit register
 */
#define CY_U3P_PIB_GPIF_ADDR_COUNT_LIMIT_ADDRESS            (0xe0014088)
#define CY_U3P_PIB_GPIF_ADDR_COUNT_LIMIT                    (*(uvint32_t *)(0xe0014088))
#define CY_U3P_PIB_GPIF_ADDR_COUNT_LIMIT_DEFAULT            (0x0000ffff)

/*
   Stop counting when counter reaches this value
 */
#define CY_U3P_GPIF_AC_LIMIT_MASK                           (0xffffffff) /* <0:31> R:RW:0xFFFF:No */
#define CY_U3P_GPIF_AC_LIMIT_POS                            (0)



/*
   State counter configuration
 */
#define CY_U3P_PIB_GPIF_STATE_COUNT_CONFIG_ADDRESS          (0xe001408c)
#define CY_U3P_PIB_GPIF_STATE_COUNT_CONFIG                  (*(uvint32_t *)(0xe001408c))
#define CY_U3P_PIB_GPIF_STATE_COUNT_CONFIG_DEFAULT          (0x00000000)

/*
   0: This counter is not used.
   1: This counter is used.
 */
#define CY_U3P_GPIF_SC_ENABLE                               (1u << 0) /* <0:0> R:RW:0:Yes */


/*
   1: SW writes one to reset/load the counter
   0: HW write 0 to signal that counter has reset
 */
#define CY_U3P_GPIF_SC_SW_RESET                             (1u << 1) /* <1:1> RW0C:RW1S:0:Yes */



/*
   State counter limit register
 */
#define CY_U3P_PIB_GPIF_STATE_COUNT_LIMIT_ADDRESS           (0xe0014090)
#define CY_U3P_PIB_GPIF_STATE_COUNT_LIMIT                   (*(uvint32_t *)(0xe0014090))
#define CY_U3P_PIB_GPIF_STATE_COUNT_LIMIT_DEFAULT           (0x0000ffff)

/*
   Generate an output tick, reset and start counting again if enabled when
   this limit is reached..
 */
#define CY_U3P_GPIF_SC_LIMIT_MASK                           (0x0000ffff) /* <0:15> R:RW:0xFFFF:No */
#define CY_U3P_GPIF_SC_LIMIT_POS                            (0)



/*
   Data counter configuration
 */
#define CY_U3P_PIB_GPIF_DATA_COUNT_CONFIG_ADDRESS           (0xe0014094)
#define CY_U3P_PIB_GPIF_DATA_COUNT_CONFIG                   (*(uvint32_t *)(0xe0014094))
#define CY_U3P_PIB_GPIF_DATA_COUNT_CONFIG_DEFAULT           (0x0000010a)

/*
   0: This counter is not used.
   1: This counter is used.
 */
#define CY_U3P_GPIF_DC_ENABLE                               (1u << 0) /* <0:0> R:RW:0:Yes */


/*
   0: Saturate on reaching the limit
   1: Reload on reaching the limit
 */
#define CY_U3P_GPIF_DC_RELOAD                               (1u << 1) /* <1:1> R:RW:1:No */


/*
   1: SW writes one to reset/load the counter
   0: HW write 0 to signal that counter has reset
 */
#define CY_U3P_GPIF_DC_SW_RESET                             (1u << 2) /* <2:2> RW0C:RW1S:0:Yes */


/*
   0: Down Count
   1: Up Count
 */
#define CY_U3P_GPIF_DC_DOWN_UP                              (1u << 3) /* <3:3> R:RW:1:No */


/*
    8-bit quantity to be added/subtracted to the counter on each clock
 */
#define CY_U3P_GPIF_DC_INCREMENT_MASK                       (0x0000ff00) /* <8:15> R:RW:1:No */
#define CY_U3P_GPIF_DC_INCREMENT_POS                        (8)



/*
   Data counter reset register
 */
#define CY_U3P_PIB_GPIF_DATA_COUNT_RESET_ADDRESS            (0xe0014098)
#define CY_U3P_PIB_GPIF_DATA_COUNT_RESET                    (*(uvint32_t *)(0xe0014098))
#define CY_U3P_PIB_GPIF_DATA_COUNT_RESET_DEFAULT            (0x00000000)

/*
   Reset counter to this value. Relload to this value when limit it reached
   if specified.
 */
#define CY_U3P_GPIF_DC_RESET_LOAD_MASK                      (0xffffffff) /* <0:31> R:RW:0:No */
#define CY_U3P_GPIF_DC_RESET_LOAD_POS                       (0)



/*
   Data counter limit register
 */
#define CY_U3P_PIB_GPIF_DATA_COUNT_LIMIT_ADDRESS            (0xe001409c)
#define CY_U3P_PIB_GPIF_DATA_COUNT_LIMIT                    (*(uvint32_t *)(0xe001409c))
#define CY_U3P_PIB_GPIF_DATA_COUNT_LIMIT_DEFAULT            (0x0000ffff)

/*
   Reload data counter if this limit is reached and reload is enabled.
 */
#define CY_U3P_GPIF_DC_LIMIT_MASK                           (0xffffffff) /* <0:31> R:RW:0xFFFF:No */
#define CY_U3P_GPIF_DC_LIMIT_POS                            (0)



/*
   Control comparator value
 */
#define CY_U3P_PIB_GPIF_CTRL_COMP_VALUE_ADDRESS             (0xe00140a0)
#define CY_U3P_PIB_GPIF_CTRL_COMP_VALUE                     (*(uvint32_t *)(0xe00140a0))
#define CY_U3P_PIB_GPIF_CTRL_COMP_VALUE_DEFAULT             (0x00000000)

/*
   Output true when CTRL bus matches this value
 */
#define CY_U3P_GPIF_CC_VALUE_MASK                           (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_GPIF_CC_VALUE_POS                            (0)



/*
   Control comparator mask
 */
#define CY_U3P_PIB_GPIF_CTRL_COMP_MASK_ADDRESS              (0xe00140a4)
#define CY_U3P_PIB_GPIF_CTRL_COMP_MASK                      (*(uvint32_t *)(0xe00140a4))
#define CY_U3P_PIB_GPIF_CTRL_COMP_MASK_DEFAULT              (0x00000000)

/*
   1: Bit at this bit position in the CTRL bus is to be used in comparison
   0: Bit at this bit position is a don't-care for comparison
 */
#define CY_U3P_GPIF_CC_MASK_MASK                            (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_GPIF_CC_MASK_POS                             (0)



/*
   Data comparator value
 */
#define CY_U3P_PIB_GPIF_DATA_COMP_VALUE_ADDRESS             (0xe00140a8)
#define CY_U3P_PIB_GPIF_DATA_COMP_VALUE                     (*(uvint32_t *)(0xe00140a8))
#define CY_U3P_PIB_GPIF_DATA_COMP_VALUE_DEFAULT             (0x00000000)

/*
   Output true when Data  bus matches this value
 */
#define CY_U3P_GPIF_DC_VALUE_MASK                           (0xffffffff) /* <0:31> R:RW:0:No */
#define CY_U3P_GPIF_DC_VALUE_POS                            (0)



/*
   Data comparator mask
 */
#define CY_U3P_PIB_GPIF_DATA_COMP_MASK_ADDRESS              (0xe00140ac)
#define CY_U3P_PIB_GPIF_DATA_COMP_MASK                      (*(uvint32_t *)(0xe00140ac))
#define CY_U3P_PIB_GPIF_DATA_COMP_MASK_DEFAULT              (0x00000000)

/*
   1: Bit at this bit position in the Data bus is to be used in comparision
   0: Bit at this bit position is a don't-care for comparision
 */
#define CY_U3P_GPIF_DC_MASK_MASK                            (0xffffffff) /* <0:31> R:RW:0:No */
#define CY_U3P_GPIF_DC_MASK_POS                             (0)



/*
   Address comparator value
 */
#define CY_U3P_PIB_GPIF_ADDR_COMP_VALUE_ADDRESS             (0xe00140b0)
#define CY_U3P_PIB_GPIF_ADDR_COMP_VALUE                     (*(uvint32_t *)(0xe00140b0))
#define CY_U3P_PIB_GPIF_ADDR_COMP_VALUE_DEFAULT             (0x00000000)

/*
   Output true when Data  bus matches this value
 */
#define CY_U3P_GPIF_AC_VALUE_MASK                           (0xffffffff) /* <0:31> R:RW:0:No */
#define CY_U3P_GPIF_AC_VALUE_POS                            (0)



/*
   Address comparator mask
 */
#define CY_U3P_PIB_GPIF_ADDR_COMP_MASK_ADDRESS              (0xe00140b4)
#define CY_U3P_PIB_GPIF_ADDR_COMP_MASK                      (*(uvint32_t *)(0xe00140b4))
#define CY_U3P_PIB_GPIF_ADDR_COMP_MASK_DEFAULT              (0x00000000)

/*
   1: Bit at this bit position in the CTRL bus is to be used in comparison
   0: Bit at this bit position is a don't-care for comparison
 */
#define CY_U3P_GPIF_AC_MASK_MASK                            (0xffffffff) /* <0:31> R:RW:0:No */
#define CY_U3P_GPIF_AC_MASK_POS                             (0)



/*
   Data Control Register
 */
#define CY_U3P_PIB_GPIF_DATA_CTRL_ADDRESS                   (0xe00140b8)
#define CY_U3P_PIB_GPIF_DATA_CTRL                           (*(uvint32_t *)(0xe00140b8))
#define CY_U3P_PIB_GPIF_DATA_CTRL_DEFAULT                   (0x00000000)

/*
   Indicates data available in INGRESS_DATA.  Cleared by s/w when data processed.
 */
#define CY_U3P_GPIF_IN_DATA_VALID_MASK                      (0x0000000f) /* <0:3> RW1S:RW1C:0:Yes */
#define CY_U3P_GPIF_IN_DATA_VALID_POS                       (0)


/*
   Software writes 1 to indicate a valid word is present in the address register.
    Hardware writes 0 to indicate that the data is used and new word can
   be written.
 */
#define CY_U3P_GPIF_EG_DATA_VALID_MASK                      (0x000000f0) /* <4:7> RW0C:RW1S:0:Yes */
#define CY_U3P_GPIF_EG_DATA_VALID_POS                       (4)


/*
   Indicates address available in INGRESS_ADDRESS.  Cleared by s/w when address
   processed.
 */
#define CY_U3P_GPIF_IN_ADDR_VALID_MASK                      (0x00000f00) /* <8:11> RW1S:RW1C:0:Yes */
#define CY_U3P_GPIF_IN_ADDR_VALID_POS                       (8)


/*
   Software writes 1 to indicate a valid word is present in the address register.
    Hardware writes 0 to indicate that the data is used and new word can
   be written.
 */
#define CY_U3P_GPIF_EG_ADDR_VALID_MASK                      (0x0000f000) /* <12:15> RW0C:RW1S:0:Yes */
#define CY_U3P_GPIF_EG_ADDR_VALID_POS                       (12)



/*
   Socket Ingress Data
 */
#define CY_U3P_PIB_GPIF_INGRESS_DATA_ADDRESS(n)             (0xe00140bc + ((n) * (0x0004)))
#define CY_U3P_PIB_GPIF_INGRESS_DATA(n)                     (*(uvint32_t *)(0xe00140bc + ((n) * 0x0004)))
#define CY_U3P_PIB_GPIF_INGRESS_DATA_DEFAULT                (0x00000000)

/*
   Ingress Data.  This register will hold only 1 word of GPIF_BUS_CONFIG.BUS_WIDTH.
    No packing/unpacking is done.  MSB's will be 0.
 */
#define CY_U3P_GPIF_DATA_MASK                               (0xffffffff) /* <0:31> RW:R:0:No */
#define CY_U3P_GPIF_DATA_POS                                (0)



/*
   Socket Egress Data
 */
#define CY_U3P_PIB_GPIF_EGRESS_DATA_ADDRESS(n)              (0xe00140cc + ((n) * (0x0004)))
#define CY_U3P_PIB_GPIF_EGRESS_DATA(n)                      (*(uvint32_t *)(0xe00140cc + ((n) * 0x0004)))
#define CY_U3P_PIB_GPIF_EGRESS_DATA_DEFAULT                 (0x00000000)

/*
   Egress Data.  This register will hold only 1 word of GPIF_BUS_CONFIG.BUS_WIDTH.
    No packing/unpacking is done.  MSB's are ignored.
 */
#define CY_U3P_GPIF_DATA_MASK                               (0xffffffff) /* <0:31> R:RW:0:No */
#define CY_U3P_GPIF_DATA_POS                                (0)



/*
   Thread Ingress Address
 */
#define CY_U3P_PIB_GPIF_INGRESS_ADDRESS_ADDRESS(n)          (0xe00140dc + ((n) * (0x0004)))
#define CY_U3P_PIB_GPIF_INGRESS_ADDRESS(n)                  (*(uvint32_t *)(0xe00140dc + ((n) * 0x0004)))
#define CY_U3P_PIB_GPIF_INGRESS_ADDRESS_DEFAULT             (0x00000000)

/*
   Ingress Address
 */
#define CY_U3P_GPIF_ADDRESS_MASK                            (0xffffffff) /* <0:31> RW:R:0:No */
#define CY_U3P_GPIF_ADDRESS_POS                             (0)



/*
   Thread Egress Address
 */
#define CY_U3P_PIB_GPIF_EGRESS_ADDRESS_ADDRESS(n)           (0xe00140ec + ((n) * (0x0004)))
#define CY_U3P_PIB_GPIF_EGRESS_ADDRESS(n)                   (*(uvint32_t *)(0xe00140ec + ((n) * 0x0004)))
#define CY_U3P_PIB_GPIF_EGRESS_ADDRESS_DEFAULT              (0x00000000)

/*
   Egress Address
 */
#define CY_U3P_GPIF_ADDRESS_MASK                            (0xffffffff) /* <0:31> R:RW:0:No */
#define CY_U3P_GPIF_ADDRESS_POS                             (0)



/*
   Thread Configuration Register
 */
#define CY_U3P_PIB_GPIF_THREAD_CONFIG_ADDRESS(n)            (0xe00140fc + ((n) * (0x0004)))
#define CY_U3P_PIB_GPIF_THREAD_CONFIG(n)                    (*(uvint32_t *)(0xe00140fc + ((n) * 0x0004)))
#define CY_U3P_PIB_GPIF_THREAD_CONFIG_DEFAULT               (0x00010400)

/*
   Active Socket Number for this thread.
   Can be written by software for fixed socket assignment.
   Can be modified by h/w as result of PP_DMA_XFER accesses
   Can be modified by h/w as result of alpha 'sample AIN'
 */
#define CY_U3P_GPIF_THREAD_SOCK_MASK                        (0x0000001f) /* <0:4> RW:RW:0:Yes */
#define CY_U3P_GPIF_THREAD_SOCK_POS                         (0)


/*
   0: Assert partial flag when number of samples (remaining available for
   reading/writing) is less than or equal to the watermark.
   1: Assert when samples are more than the watermark.
 */
#define CY_U3P_GPIF_WM_CFG                                  (1u << 7) /* <7:7> R:RW:0:No */


/*
   Log2 of burst size ( 1: 2 words, 2: 4 words, etc).  The system must always
   transfer an entire burst before responding to a change in a partial flag
   (see architecture spec for more information). Burst size must equal or
   exceed the delay in partial flag creation when partial flag is used. 
   Minimum value depends on desired semantics and latency of flags vs other
   interface signals.  Maximum value is 14.  When value is >0, any socket
   switching must occur on 8 byte boundaries.
 */
#define CY_U3P_GPIF_BURST_SIZE_MASK                         (0x00000f00) /* <8:11> R:RW:4:No */
#define CY_U3P_GPIF_BURST_SIZE_POS                          (8)


/*
   Watermark position. Indicates number words from end of usable space/data.
    For normal full buffer transfers this is the size of the buffer.  For
   partial buffer transfers this is the value of DMA_SIZE rounded up to an
   integral number of BURSTSIZE quanta.
 */
#define CY_U3P_GPIF_WATERMARK_MASK                          (0x3fff0000) /* <16:29> R:RW:1:No */
#define CY_U3P_GPIF_WATERMARK_POS                           (16)


/*
   Enables the threadcontroller for operation.  Can be set by firmware after
   initializing THEAD_SOCK and other fields.  Will be set by h/w when THREAD_SOCK
   is written to by h/w.
 */
#define CY_U3P_GPIF_ENABLE                                  (1u << 31) /* <31:31> RW:RW:0:Yes */



/*
   Lambda Status Register
 */
#define CY_U3P_PIB_GPIF_LAMBDA_STAT_ADDRESS                 (0xe001410c)
#define CY_U3P_PIB_GPIF_LAMBDA_STAT                         (*(uvint32_t *)(0xe001410c))
#define CY_U3P_PIB_GPIF_LAMBDA_STAT_DEFAULT                 (0x10000000)

/*
   Current value of the Lambda Inputs
 */
#define CY_U3P_GPIF_LAMBDA_MASK                             (0xffffffff) /* <0:31> RW:R:0x10000000:Yes */
#define CY_U3P_GPIF_LAMBDA_POS                              (0)



/*
   Alpha Status Register
 */
#define CY_U3P_PIB_GPIF_ALPHA_STAT_ADDRESS                  (0xe0014110)
#define CY_U3P_PIB_GPIF_ALPHA_STAT                          (*(uvint32_t *)(0xe0014110))
#define CY_U3P_PIB_GPIF_ALPHA_STAT_DEFAULT                  (0x00000000)

/*
   Current value of the Alpha signals
 */
#define CY_U3P_GPIF_ALPHA_MASK                              (0x000000ff) /* <0:7> RW:R:0:Yes */
#define CY_U3P_GPIF_ALPHA_POS                               (0)



/*
   Beta Status Register
 */
#define CY_U3P_PIB_GPIF_BETA_STAT_ADDRESS                   (0xe0014114)
#define CY_U3P_PIB_GPIF_BETA_STAT                           (*(uvint32_t *)(0xe0014114))
#define CY_U3P_PIB_GPIF_BETA_STAT_DEFAULT                   (0x00000000)

/*
   Current value of the Beta signals
 */
#define CY_U3P_GPIF_BETA_MASK                               (0xffffffff) /* <0:31> RW:R:0:Yes */
#define CY_U3P_GPIF_BETA_POS                                (0)



/*
   Waveform program control
 */
#define CY_U3P_PIB_GPIF_WAVEFORM_CTRL_STAT_ADDRESS          (0xe0014118)
#define CY_U3P_PIB_GPIF_WAVEFORM_CTRL_STAT                  (*(uvint32_t *)(0xe0014118))
#define CY_U3P_PIB_GPIF_WAVEFORM_CTRL_STAT_DEFAULT          (0x00000000)

/*
   1:  The waveform memory is consistent and valid.
   0: Waveforms are no longer valid, stop operation and return outputs to
   default state
 */
#define CY_U3P_GPIF_WAVEFORM_VALID                          (1u << 0) /* <0:0> R:RW:0:Yes */


/*
   Write 1 here to pause GPIF. 0 to resume where left off.
 */
#define CY_U3P_GPIF_PAUSE                                   (1u << 1) /* <1:1> R:RW:0:Yes */


/*
   0: Waveform is not valid (Initial state or WAVEFORM_VALID is cleared)
   1: <unused>
   2: GPIF is armed (WAVEFORM_VALID is set)
   3: GPIF is running (using WAVEFORM_SWITCH)
   4: GPIF is done (encountered DONE_STATE)
   5: GPIF is paused (PAUSE=0)
   6: GPIF is switching (waiting for timeout/terminal state)
   7: An error occurred
 */
#define CY_U3P_GPIF_GPIF_STAT_MASK                          (0x00000700) /* <8:10> RW:R:0:Yes */
#define CY_U3P_GPIF_GPIF_STAT_POS                           (8)


/*
   Visible to the state machine as lambda 30.
 */
#define CY_U3P_GPIF_CPU_LAMBDA                              (1u << 11) /* <11:11> R:RW:0:Yes */


/*
   Initial values for alpha outputs.  These are loaded into the alpha registers
   when GPIF execution starts (first WAVEFORM_SWITCH) is set.
 */
#define CY_U3P_GPIF_ALPHA_INIT_MASK                         (0x00ff0000) /* <16:23> R:RW:0:No */
#define CY_U3P_GPIF_ALPHA_INIT_POS                          (16)


/*
   Current state of GPIF. Always updated.
 */
#define CY_U3P_GPIF_CURRENT_STATE_MASK                      (0xff000000) /* <24:31> RW:R:0:No */
#define CY_U3P_GPIF_CURRENT_STATE_POS                       (24)



/*
   Waveform switch control
 */
#define CY_U3P_PIB_GPIF_WAVEFORM_SWITCH_ADDRESS             (0xe001411c)
#define CY_U3P_PIB_GPIF_WAVEFORM_SWITCH                     (*(uvint32_t *)(0xe001411c))
#define CY_U3P_PIB_GPIF_WAVEFORM_SWITCH_DEFAULT             (0x00000000)

/*
   SW sets this bit after programming the switch register. HW clears it after
   the switch is complete.
 */
#define CY_U3P_GPIF_WAVEFORM_SWITCH                         (1u << 0) /* <0:0> RW0C:RW1S:0:Yes */


/*
   1: Enable checking for DONE_STATE and generation of GPIF_DONE.
 */
#define CY_U3P_GPIF_DONE_ENABLE                             (1u << 1) /* <1:1> R:RW:0:No */


/*
   1: Do not wait for TERMINAL_STATE, switch right away
 */
#define CY_U3P_GPIF_SWITCH_NOW                              (1u << 2) /* <2:2> R:RW:0:Yes */


/*
   0: Timeout disable
   1: Timeout for reaching TERMINAL_STATE.  Interrupt on timeout
   2: Timeout for reaching DONE_STATE.  Interrupt on timeout
   3: Timeout for reaching TERMINAL STATE.  Force switch on timeout.
   4: Timeout for hanging in current state. Timer resets on each transition.
 */
#define CY_U3P_GPIF_TIMEOUT_MODE_MASK                       (0x00000038) /* <3:5> R:RW:0:No */
#define CY_U3P_GPIF_TIMEOUT_MODE_POS                        (3)


/*
   Indicates that timeout was reached since last WAVEFORM_SWITCH
 */
#define CY_U3P_GPIF_TIMEOUT_REACHED                         (1u << 6) /* <6:6> RW:R:0:Yes */


/*
   Indicates that the TERMINAL_STATE was reached since last WAVEFORM_SWITCH
 */
#define CY_U3P_GPIF_TERMINATED                              (1u << 7) /* <7:7> RW:R:0:Yes */


/*
   State from which to initiate the switch. Corresponds to idle states of
   waveforms.
 */
#define CY_U3P_GPIF_TERMINAL_STATE_MASK                     (0x0000ff00) /* <8:15> R:RW:0:No */
#define CY_U3P_GPIF_TERMINAL_STATE_POS                      (8)


/*
   State to jump to, may be the initial state of the new wavform.
 */
#define CY_U3P_GPIF_DESTINATION_STATE_MASK                  (0x00ff0000) /* <16:23> R:RW:0:No */
#define CY_U3P_GPIF_DESTINATION_STATE_POS                   (16)


/*
   Signal GPIF_DONE upon reaching this state.
 */
#define CY_U3P_GPIF_DONE_STATE_MASK                         (0xff000000) /* <24:31> R:RW:0:No */
#define CY_U3P_GPIF_DONE_STATE_POS                          (24)



/*
   Waveform timeout register
 */
#define CY_U3P_PIB_GPIF_WAVEFORM_SWITCH_TIMEOUT_ADDRESS     (0xe0014120)
#define CY_U3P_PIB_GPIF_WAVEFORM_SWITCH_TIMEOUT             (*(uvint32_t *)(0xe0014120))
#define CY_U3P_PIB_GPIF_WAVEFORM_SWITCH_TIMEOUT_DEFAULT     (0x00000000)

/*
   Timeout value
 */
#define CY_U3P_GPIF_RESET_LOAD_MASK                         (0xffffffff) /* <0:31> R:RW:0:No */
#define CY_U3P_GPIF_RESET_LOAD_POS                          (0)



/*
   CRC Configuration Register
 */
#define CY_U3P_PIB_GPIF_CRC_CONFIG_ADDRESS                  (0xe0014124)
#define CY_U3P_PIB_GPIF_CRC_CONFIG                          (*(uvint32_t *)(0xe0014124))
#define CY_U3P_PIB_GPIF_CRC_CONFIG_DEFAULT                  (0x00000000)

/*
   A CRC value received on the data inputs by the state machine
 */
#define CY_U3P_GPIF_CRC_RECEIVED_MASK                       (0x0000ffff) /* <0:15> RW:R:0:Yes */
#define CY_U3P_GPIF_CRC_RECEIVED_POS                        (0)


/*
   Indicates the order in which the bits in each byte are brought through
   the CRC shift register.
   0: LSb first
   1: MSb first
 */
#define CY_U3P_GPIF_BIT_ENDIAN                              (1u << 20) /* <20:20> R:RW:0:No */


/*
   Indicates the order in which bytes in a 32b word are brought through the
   CRC shift register. This is independent from the endianness of the interface.
   0: LSB first
   1: MSB first
 */
#define CY_U3P_GPIF_BYTE_ENDIAN                             (1u << 21) /* <21:21> R:RW:0:No */


/*
   A CRC was loaded into CRC_RECEIVED that is different from CRC_VALUE
 */
#define CY_U3P_GPIF_CRC_ERROR                               (1u << 22) /* <22:22> RW:R:0:Yes */


/*
   Enables CRC calulation
 */
#define CY_U3P_GPIF_ENABLE                                  (1u << 31) /* <31:31> R:RW:0:No */



/*
   CRC Data Register
 */
#define CY_U3P_PIB_GPIF_CRC_DATA_ADDRESS                    (0xe0014128)
#define CY_U3P_PIB_GPIF_CRC_DATA                            (*(uvint32_t *)(0xe0014128))
#define CY_U3P_PIB_GPIF_CRC_DATA_DEFAULT                    (0x00000000)

/*
   Initial CRC Value
 */
#define CY_U3P_GPIF_INITIAL_VALUE_MASK                      (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_GPIF_INITIAL_VALUE_POS                       (0)


/*
   Current result of CRC Calculation
 */
#define CY_U3P_GPIF_CRC_VALUE_MASK                          (0xffff0000) /* <16:31> RW:R:0:Yes */
#define CY_U3P_GPIF_CRC_VALUE_POS                           (16)



/*
   Beta Deassert Register
 */
#define CY_U3P_PIB_GPIF_BETA_DEASSERT_ADDRESS               (0xe001412c)
#define CY_U3P_PIB_GPIF_BETA_DEASSERT                       (*(uvint32_t *)(0xe001412c))
#define CY_U3P_PIB_GPIF_BETA_DEASSERT_DEFAULT               (0x00000001)

/*
   1: BETA_DEASSERT from the waveform descriptor applies to this beta. This
   is not honored for external betas which always behave as if apply_deassert=0
   0: BETA_DEASSERT does not apply. Betas remain asserted throughout the
   state.
 */
#define CY_U3P_GPIF_APPLY_DEASSERT_MASK                     (0xffffffff) /* <0:31> R:RW:1:No */
#define CY_U3P_GPIF_APPLY_DEASSERT_POS                      (0)



/*
   Transition Function Registers
 */
#define CY_U3P_PIB_GPIF_FUNCTION_ADDRESS(n)                 (0xe0014130 + ((n) * (0x0004)))
#define CY_U3P_PIB_GPIF_FUNCTION(n)                         (*(uvint32_t *)(0xe0014130 + ((n) * 0x0004)))
#define CY_U3P_PIB_GPIF_FUNCTION_DEFAULT                    (0x00000000)

/*
   Truth table for transition function. Bit position X contains output when
   the 4 inputs constitute the value X in binary. For example bit 2 = 1 means
   in3=0, in2=0, in1=1 and in0=0 will evaluate true for this function.
 */
#define CY_U3P_GPIF_FUNCTION_MASK                           (0x0000ffff) /* <0:15> R:RW:0:No */
#define CY_U3P_GPIF_FUNCTION_POS                            (0)



/*
   Left edge waveform memory
 */
#define CY_U3P_PIB_GPIF_LEFT_WAVEFORM0_ADDRESS(n)           (0xe0015000 + ((n) * (0x0010)))
#define CY_U3P_PIB_GPIF_LEFT_WAVEFORM0(n)                   (*(uvint32_t *)(0xe0015000 + ((n) * 0x0010)))
#define CY_U3P_PIB_GPIF_LEFT_WAVEFORM0_DEFAULT              (0x00000000)

/*
   Next state on left transition
 */
#define CY_U3P_GPIF0_NEXT_STATE_MASK                        (0x000000ff) /* <0:7> R:RW:0:No */
#define CY_U3P_GPIF0_NEXT_STATE_POS                         (0)


/*
   Index to select the first input for transition fuctions out of 32 choices.
 */
#define CY_U3P_GPIF0_FA_MASK                                (0x00001f00) /* <8:12> R:RW:0:No */
#define CY_U3P_GPIF0_FA_POS                                 (8)


/*
   Second input index.
 */
#define CY_U3P_GPIF0_FB_MASK                                (0x0003e000) /* <13:17> R:RW:0:No */
#define CY_U3P_GPIF0_FB_POS                                 (13)


/*
   Third input index.
 */
#define CY_U3P_GPIF0_FC_MASK                                (0x007c0000) /* <18:22> R:RW:0:No */
#define CY_U3P_GPIF0_FC_POS                                 (18)


/*
   Fourth input index
 */
#define CY_U3P_GPIF0_FD_MASK                                (0x0f800000) /* <23:27> R:RW:0:No */
#define CY_U3P_GPIF0_FD_POS                                 (23)


/*
   Index to select the first transition function from a choice of 32 functions.
   Truth-tables for the 32 4-bit functions are defined using the GPIF_FUNCTION
   registers.
 */
#define CY_U3P_GPIF0_F0_L_MASK                              (0xf0000000) /* <28:31> R:RW:0:No */
#define CY_U3P_GPIF0_F0_L_POS                               (28)



/*
   Left edge waveform memory
 */
#define CY_U3P_PIB_GPIF_LEFT_WAVEFORM1_ADDRESS(n)           (0xe0015004 + ((n) * (0x0010)))
#define CY_U3P_PIB_GPIF_LEFT_WAVEFORM1(n)                   (*(uvint32_t *)(0xe0015004 + ((n) * 0x0010)))
#define CY_U3P_PIB_GPIF_LEFT_WAVEFORM1_DEFAULT              (0x00000000)

/*
   Index to select the first transition function from a choice of 32 functions.
   Truth-tables for the 32 4-bit functions are defined using the GPIF_FUNCTION
   registers.
 */
#define CY_U3P_GPIF1_F0_H                                   (1u << 0) /* <0:0> R:RW:0:No */


/*
   Index to select the second transition function from a choice of 32 functions.
   Truth-tables for the 32 4-bit functions are defined using the GPIF_FUNCTION
   registers.
 */
#define CY_U3P_GPIF1_F1_MASK                                (0x0000003e) /* <1:5> R:RW:0:No */
#define CY_U3P_GPIF1_F1_POS                                 (1)


/*
   Primary outputs for the left edge of the next state.
 */
#define CY_U3P_GPIF1_ALPHA_LEFT_MASK                        (0x00003fc0) /* <6:13> R:RW:0:No */
#define CY_U3P_GPIF1_ALPHA_LEFT_POS                         (6)


/*
   For the right edge
 */
#define CY_U3P_GPIF1_ALPHA_RIGHT_MASK                       (0x003fc000) /* <14:21> R:RW:0:No */
#define CY_U3P_GPIF1_ALPHA_RIGHT_POS                        (14)


/*
   32 Secondary Outputs
 */
#define CY_U3P_GPIF1_BETA_L_MASK                            (0xffc00000) /* <22:31> R:RW:0:No */
#define CY_U3P_GPIF1_BETA_L_POS                             (22)



/*
   Left edge waveform memory
 */
#define CY_U3P_PIB_GPIF_LEFT_WAVEFORM2_ADDRESS(n)           (0xe0015008 + ((n) * (0x0010)))
#define CY_U3P_PIB_GPIF_LEFT_WAVEFORM2(n)                   (*(uvint32_t *)(0xe0015008 + ((n) * 0x0010)))
#define CY_U3P_PIB_GPIF_LEFT_WAVEFORM2_DEFAULT              (0x00000000)

/*
   32 Secondary Outputs
 */
#define CY_U3P_GPIF2_BETA_H_MASK                            (0x003fffff) /* <0:21> R:RW:0:No */
#define CY_U3P_GPIF2_BETA_H_POS                             (0)


/*
   Number of times to stay in this state -1
 */
#define CY_U3P_GPIF2_REPEAT_COUNT_MASK                      (0x3fc00000) /* <22:29> R:RW:0:No */
#define CY_U3P_GPIF2_REPEAT_COUNT_POS                       (22)


/*
   0: Keep betas asserted throughout the state
   1: De-assert after asserting for exactly one clock cycle, irrespective
   of how many cycles the state is active.
   The normal (de-assert) state of user defined betas is defined in GPIF_CTRL_BUS_DEFAULT.
   The normal state of internal betas is fixed by hardware. This function
   is applied only to betas selected in GPIF_BETA_DEASSERT.
 */
#define CY_U3P_GPIF2_BETA_DEASSERT                          (1u << 30) /* <30:30> R:RW:0:No */


/*
   1: This entry is valid.
   0: Entry not valid. (Not programmed or edge does not exist)
 */
#define CY_U3P_GPIF2_VALID                                  (1u << 31) /* <31:31> R:RW:0:No */



/*
   Left edge waveform memory
 */
#define CY_U3P_PIB_GPIF_LEFT_WAVEFORM3_ADDRESS(n)           (0xe001500c + ((n) * (0x0010)))
#define CY_U3P_PIB_GPIF_LEFT_WAVEFORM3(n)                   (*(uvint32_t *)(0xe001500c + ((n) * 0x0010)))
#define CY_U3P_PIB_GPIF_LEFT_WAVEFORM3_DEFAULT              (0x00000000)

/*
   This entry is unused and does not map to RAM in the current implementation.
   Writes are ignored and reads will return 0 in all cases.
 */
#define CY_U3P_GPIF3_UNUSED_MASK                            (0xffffffff) /* <0:31> R:R:0:No */
#define CY_U3P_GPIF3_UNUSED_POS                             (0)



/*
   Right edge waveform memory
 */
#define CY_U3P_PIB_GPIF_RIGHT_WAVEFORM0_ADDRESS(n)          (0xe0016000 + ((n) * (0x0010)))
#define CY_U3P_PIB_GPIF_RIGHT_WAVEFORM0(n)                  (*(uvint32_t *)(0xe0016000 + ((n) * 0x0010)))
#define CY_U3P_PIB_GPIF_RIGHT_WAVEFORM0_DEFAULT             (0x00000000)

/*
   Next state on left transition
 */
#define CY_U3P_GPIF0_NEXT_STATE_MASK                        (0x000000ff) /* <0:7> R:RW:0:No */
#define CY_U3P_GPIF0_NEXT_STATE_POS                         (0)


/*
   Index to select the first input for transition fuctions out of 32 choices.
 */
#define CY_U3P_GPIF0_FA_MASK                                (0x00001f00) /* <8:12> R:RW:0:No */
#define CY_U3P_GPIF0_FA_POS                                 (8)


/*
   Second input index.
 */
#define CY_U3P_GPIF0_FB_MASK                                (0x0003e000) /* <13:17> R:RW:0:No */
#define CY_U3P_GPIF0_FB_POS                                 (13)


/*
   Third input index.
 */
#define CY_U3P_GPIF0_FC_MASK                                (0x007c0000) /* <18:22> R:RW:0:No */
#define CY_U3P_GPIF0_FC_POS                                 (18)


/*
   Fourth input index
 */
#define CY_U3P_GPIF0_FD_MASK                                (0x0f800000) /* <23:27> R:RW:0:No */
#define CY_U3P_GPIF0_FD_POS                                 (23)


/*
   Index to select the first transition function from a choice of 32 functions.
   Truth-tables for the 32 4-bit functions are defined using the GPIF_FUNCTION
   registers.
 */
#define CY_U3P_GPIF0_F0_L_MASK                              (0xf0000000) /* <28:31> R:RW:0:No */
#define CY_U3P_GPIF0_F0_L_POS                               (28)



/*
   Right edge waveform memory
 */
#define CY_U3P_PIB_GPIF_RIGHT_WAVEFORM1_ADDRESS(n)          (0xe0016004 + ((n) * (0x0010)))
#define CY_U3P_PIB_GPIF_RIGHT_WAVEFORM1(n)                  (*(uvint32_t *)(0xe0016004 + ((n) * 0x0010)))
#define CY_U3P_PIB_GPIF_RIGHT_WAVEFORM1_DEFAULT             (0x00000000)

/*
   Index to select the first transition function from a choice of 32 functions.
   Truth-tables for the 32 4-bit functions are defined using the GPIF_FUNCTION
   registers.
 */
#define CY_U3P_GPIF1_F0_H                                   (1u << 0) /* <0:0> R:RW:0:No */


/*
   Index to select the second transition function from a choice of 32 functions.
   Truth-tables for the 32 4-bit functions are defined using the GPIF_FUNCTION
   registers.
 */
#define CY_U3P_GPIF1_F1_MASK                                (0x0000003e) /* <1:5> R:RW:0:No */
#define CY_U3P_GPIF1_F1_POS                                 (1)


/*
   Primary outputs for the left edge of the next state.
 */
#define CY_U3P_GPIF1_ALPHA_LEFT_MASK                        (0x00003fc0) /* <6:13> R:RW:0:No */
#define CY_U3P_GPIF1_ALPHA_LEFT_POS                         (6)


/*
   For the right edge
 */
#define CY_U3P_GPIF1_ALPHA_RIGHT_MASK                       (0x003fc000) /* <14:21> R:RW:0:No */
#define CY_U3P_GPIF1_ALPHA_RIGHT_POS                        (14)


/*
   32 Secondary Outputs
 */
#define CY_U3P_GPIF1_BETA_L_MASK                            (0xffc00000) /* <22:31> R:RW:0:No */
#define CY_U3P_GPIF1_BETA_L_POS                             (22)



/*
   Right edge waveform memory
 */
#define CY_U3P_PIB_GPIF_RIGHT_WAVEFORM2_ADDRESS(n)          (0xe0016008 + ((n) * (0x0010)))
#define CY_U3P_PIB_GPIF_RIGHT_WAVEFORM2(n)                  (*(uvint32_t *)(0xe0016008 + ((n) * 0x0010)))
#define CY_U3P_PIB_GPIF_RIGHT_WAVEFORM2_DEFAULT             (0x00000000)

/*
   32 Secondary Outputs
 */
#define CY_U3P_GPIF2_BETA_H_MASK                            (0x003fffff) /* <0:21> R:RW:0:No */
#define CY_U3P_GPIF2_BETA_H_POS                             (0)


/*
   Number of times to stay in this state -1
 */
#define CY_U3P_GPIF2_REPEAT_COUNT_MASK                      (0x3fc00000) /* <22:29> R:RW:0:No */
#define CY_U3P_GPIF2_REPEAT_COUNT_POS                       (22)


/*
   0: Keep betas asserted throughout the state
   1: De-assert after asserting for exactly one clock cycle, irrespective
   of how many cycles the state is active.
   The normal (de-assert) state of user defined betas is defined in GPIF_CTRL_BUS_DEFAULT.
   The normal state of internal betas is fixed by hardware. This function
   is applied only to betas selected in GPIF_BETA_DEASSERT.
 */
#define CY_U3P_GPIF2_BETA_DEASSERT                          (1u << 30) /* <30:30> R:RW:0:No */


/*
   1: This entry is valid.
   0: Entry not valid. (Not programmed or edge does not exist)
 */
#define CY_U3P_GPIF2_VALID                                  (1u << 31) /* <31:31> R:RW:0:No */



/*
   Right edge waveform memory
 */
#define CY_U3P_PIB_GPIF_RIGHT_WAVEFORM3_ADDRESS(n)          (0xe001600c + ((n) * (0x0010)))
#define CY_U3P_PIB_GPIF_RIGHT_WAVEFORM3(n)                  (*(uvint32_t *)(0xe001600c + ((n) * 0x0010)))
#define CY_U3P_PIB_GPIF_RIGHT_WAVEFORM3_DEFAULT             (0x00000000)

/*
   This entry is unused and does not map to RAM in the current implementation.
   Writes are ignored and reads will return 0 in all cases.
 */
#define CY_U3P_GPIF3_UNUSED_MASK                            (0xffffffff) /* <0:31> R:R:0:No */
#define CY_U3P_GPIF3_UNUSED_POS                             (0)



#endif /* _INCLUDED_GPIF_REGS_H_ */

/*[]*/

