/*
 ## Cypress FX3 Core Library Header (gpio_regs.h)
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

#ifndef _INCLUDED_GPIO_REGS_H_
#define _INCLUDED_GPIO_REGS_H_

#include <cyu3types.h>

#define GPIO_BASE_ADDR                           (0xe0001000)

typedef struct
{
    struct
    {
        uvint32_t status;                         /* 0xe0001000 */
        uvint32_t timer;                          /* 0xe0001004 */
        uvint32_t period;                         /* 0xe0001008 */
        uvint32_t threshold;                      /* 0xe000100c */
    } lpp_gpio_pin[8];
    uvint32_t rsrvd0[32];
    uvint32_t lpp_gpio_simple[61];                /* 0xe0001100 */
    uvint32_t rsrvd1[119];
    uvint32_t lpp_gpio_invalue0;                  /* 0xe00013d0 */
    uvint32_t lpp_gpio_invalue1;                  /* 0xe00013d4 */
    uvint32_t rsrvd2[2];
    uvint32_t lpp_gpio_intr0;                     /* 0xe00013e0 */
    uvint32_t lpp_gpio_intr1;                     /* 0xe00013e4 */
    uvint32_t lpp_gpio_pin_intr;                  /* 0xe00013e8 */
    uvint32_t rsrvd3;
    uvint32_t lpp_gpio_id;                        /* 0xe00013f0 */
    uvint32_t lpp_gpio_power;                     /* 0xe00013f4 */
} GPIO_REGS_T, *PGPIO_REGS_T;

#define GPIO        ((PGPIO_REGS_T) GPIO_BASE_ADDR)


/*
   Configuration, mode and status of IO Pin
 */
#define CY_U3P_LPP_GPIO_PIN_STATUS_ADDRESS(n)               (0xe0001000 + ((n) * (0x0010)))
#define CY_U3P_LPP_GPIO_PIN_STATUS(n)                       (*(uvint32_t *)(0xe0001000 + ((n) * 0x0010)))
#define CY_U3P_LPP_GPIO_PIN_STATUS_DEFAULT                  (0x00000001)

/*
   Output value used for output drive (if DRIVE_EN=1)
   0: Driven Low
   1: Driven High
 */
#define CY_U3P_LPP_GPIO_OUT_VALUE                           (1u << 0) /* <0:0> RW:RW:1:No */


/*
   Present input measurement
   0: Low
   1: High
 */
#define CY_U3P_LPP_GPIO_IN_VALUE                            (1u << 1) /* <1:1> W:R:0:No */


/*
   Output driver eneable when OUT_VALUE=0
   0: Output driver is tri-stated
   1: Output-driver is active (weak/strong is determined in IO Matrix)
 */
#define CY_U3P_LPP_GPIO_DRIVE_LO_EN                         (1u << 4) /* <4:4> R:RW:0:No */


/*
   Output driver enable when OUT_VALUE=1
   0: Output driver is tri-stated
   1: Output-driver is active (weak/strong is determined in IO Matrix)
 */
#define CY_U3P_LPP_GPIO_DRIVE_HI_EN                         (1u << 5) /* <5:5> R:RW:0:No */


/*
   0: Input stage is disabled
   1: Input stage is enabled, value is readable in IN_VALUE
 */
#define CY_U3P_LPP_GPIO_INPUT_EN                            (1u << 6) /* <6:6> R:RW:0:No */


/*
   Mode/command.  Behavior is undefined when MODE>2 and TIMER_MODE=4,5,6.
   0: STATIC
   1: TOGGLE
   2: SAMPLENOW
   3: PULSENOW
   4: PULSE
   5: PWM
   6: MEASURE_LOW
   7: MEASURE_HIGH
   8: MEASURE_LOW_ONCE
   9: MEASURE_HIGH_ONCE
   10: MEASURE_NEG
   11: MEASURE_POS
   12: MEASURE_ANY
   13: MEASURE_NEG_ONCE
   14: MEASURE_POS_ONCE
   15: MEASURE_ANY_ONCE
   (for more information see Architecture Specification)
   Note that when setting TOGGLE/SAMPLENOW/PULSENOW while ENABLE=0 the behavior
   is undefined)
 */
#define CY_U3P_LPP_GPIO_MODE_MASK                           (0x00000f00) /* <8:11> RW:RW:0:No */
#define CY_U3P_LPP_GPIO_MODE_POS                            (8)


/*
   Interrupt mode
   0: No interrupt
   1: Interrupt on posedge on IN_VALUE
   2: Interrupt on negedge on IN_VALUE
   3: Interrupt on any edge on IN_VALUE
   4: Interrupt when IN_VALUE is low
   5: Interrupt when IN_VALUE is high
   6: Interrupt on TIMER = THRESHOLD
   7: Interrupt on TIMER = 0
 */
#define CY_U3P_LPP_GPIO_INTRMODE_MASK                       (0x07000000) /* <24:26> R:RW:0:No */
#define CY_U3P_LPP_GPIO_INTRMODE_POS                        (24)


/*
   Registers edge triggered interrupt condition.  Only relevant when INTRMODE=1,2,3,6,7.
    When INTRMODE=4,5 pin status is fed directly to interrupt controller;
   condition can be observed through IN_VALUE in this case.
 */
#define CY_U3P_LPP_GPIO_INTR                                (1u << 27) /* <27:27> RW1S:RW1C:0:No */


/*
   0: Shutdown GPIO_TIMER
   1: Use high frequency (e.g. 50MHz)
   2: Use low frequency (e.g. 1MHz)
   3: Use stanby frequency (32KHz)
   4: Use posedge (sampled using high frequency)
   5: Use negedge (sampled using high frequency)
   6: Use any edge (sampled using high frequency)
   7: Reserved
   Note: Changing TIMER_MODE when ENABLE=1 will result in undefined behavior.
 */
#define CY_U3P_LPP_GPIO_TIMER_MODE_MASK                     (0x70000000) /* <28:30> R:RW:0:No */
#define CY_U3P_LPP_GPIO_TIMER_MODE_POS                      (28)


/*
   Enable GPIO logic for this pin.
 */
#define CY_U3P_LPP_GPIO_ENABLE                              (1u << 31) /* <31:31> R:RW:0:No */



/*
   Timer/counter for pulse and measurement modes
 */
#define CY_U3P_LPP_GPIO_PIN_TIMER_ADDRESS(n)                (0xe0001004 + ((n) * (0x0010)))
#define CY_U3P_LPP_GPIO_PIN_TIMER(n)                        (*(uvint32_t *)(0xe0001004 + ((n) * 0x0010)))
#define CY_U3P_LPP_GPIO_PIN_TIMER_DEFAULT                   (0x00000000)

/*
   32-bit timer-counter value.  Use MODE=SAMPLE_NOW to sample the timer into
   PIN_THRESHOLD. When TIMER reaches GPIO_PERIOD it resets to 0.
 */
#define CY_U3P_LPP_GPIO_TIMER_MASK                          (0xffffffff) /* <0:31> RW:RW:0:Yes */
#define CY_U3P_LPP_GPIO_TIMER_POS                           (0)



/*
   Period length for revolving counter GPIO_TIMER
 */
#define CY_U3P_LPP_GPIO_PIN_PERIOD_ADDRESS(n)               (0xe0001008 + ((n) * (0x0010)))
#define CY_U3P_LPP_GPIO_PIN_PERIOD(n)                       (*(uvint32_t *)(0xe0001008 + ((n) * 0x0010)))
#define CY_U3P_LPP_GPIO_PIN_PERIOD_DEFAULT                  (0xffffffff)

/*
   32-bit period for GPIO_TIMER (counter resets to 0 when PERIOD=TIMER)
 */
#define CY_U3P_LPP_GPIO_PERIOD_MASK                         (0xffffffff) /* <0:31> R:RW:0xFFFFFFFF:No */
#define CY_U3P_LPP_GPIO_PERIOD_POS                          (0)



/*
   Threshold or measurement register
 */
#define CY_U3P_LPP_GPIO_PIN_THRESHOLD_ADDRESS(n)            (0xe000100c + ((n) * (0x0010)))
#define CY_U3P_LPP_GPIO_PIN_THRESHOLD(n)                    (*(uvint32_t *)(0xe000100c + ((n) * 0x0010)))
#define CY_U3P_LPP_GPIO_PIN_THRESHOLD_DEFAULT               (0x00000000)

/*
   32-bit threshold or measurement for counter
 */
#define CY_U3P_LPP_GPIO_THRESHOLD_MASK                      (0xffffffff) /* <0:31> RW:RW:0:No */
#define CY_U3P_LPP_GPIO_THRESHOLD_POS                       (0)



/*
   Simple General Purpose IO Register (one pin)
 */
#define CY_U3P_LPP_GPIO_SIMPLE_ADDRESS(n)                   (0xe0001100 + ((n) * (0x0004)))
#define CY_U3P_LPP_GPIO_SIMPLE(n)                           (*(uvint32_t *)(0xe0001100 + ((n) * 0x0004)))
#define CY_U3P_LPP_GPIO_SIMPLE_DEFAULT                      (0x00000001)

/*
   Output value used for output drive (if DRIVE_EN=1)
   0: Driven Low
   1: Driven High
 */
#define CY_U3P_LPP_GPIO_OUT_VALUE                           (1u << 0) /* <0:0> R:RW:1:No */


/*
   Present input measurement
   0: Low
   1: High
 */
#define CY_U3P_LPP_GPIO_IN_VALUE                            (1u << 1) /* <1:1> W:R:0:No */


/*
   Output driver eneable when OUT_VALUE=0
   0: Output driver is tri-stated
   1: Output-driver is active (weak/strong is determined in IO Matrix)
 */
#define CY_U3P_LPP_GPIO_DRIVE_LO_EN                         (1u << 4) /* <4:4> R:RW:0:No */


/*
   Output driver enable when OUT_VALUE=1
   0: Output driver is tri-stated
   1: Output-driver is active (weak/strong is determined in IO Matrix)
 */
#define CY_U3P_LPP_GPIO_DRIVE_HI_EN                         (1u << 5) /* <5:5> R:RW:0:No */


/*
   0: Input stage is disabled
   1: Input stage is enabled, value is readable in IN_VALUE
 */
#define CY_U3P_LPP_GPIO_INPUT_EN                            (1u << 6) /* <6:6> R:RW:0:No */


/*
   Interrupt mode
   0: No interrupt
   1: Interrupt on posedge
   2: Interrupt on negedge
   3: Interrupt on any edge
   4: Interrupt when pin is low
   5: Interrupt when pin is high
   6-7: Reserved
 */
#define CY_U3P_LPP_GPIO_INTRMODE_MASK                       (0x07000000) /* <24:26> R:RW:0:No */
#define CY_U3P_LPP_GPIO_INTRMODE_POS                        (24)


/*
   Registers edge triggered interrupt condition.  Only relevant when INTRMODE=1,2,3,6,7.
    When INTRMODE=4,5 pin status is fed directly to interrupt controller;
   condition can be observed through IN_VALUE in this case.
 */
#define CY_U3P_LPP_GPIO_INTR                                (1u << 27) /* <27:27> RW1S:RW1C:0:No */


/*
   Enable GPIO logic for this pin.
 */
#define CY_U3P_LPP_GPIO_ENABLE                              (1u << 31) /* <31:31> R:RW:0:No */



/*
   GPIO Input Value Vector
 */
#define CY_U3P_LPP_GPIO_INVALUE0_ADDRESS                    (0xe00013d0)
#define CY_U3P_LPP_GPIO_INVALUE0                            (*(uvint32_t *)(0xe00013d0))
#define CY_U3P_LPP_GPIO_INVALUE0_DEFAULT                    (0x00000000)

/*
   If bit <x> is set, IN_VALUE is active for GPIO <x>
 */
#define CY_U3P_LPP_GPIO_INVALUE0_MASK                       (0xffffffff) /* <0:31> W:R:0:No */
#define CY_U3P_LPP_GPIO_INVALUE0_POS                        (0)



/*
   GPIO Input Value Vector
 */
#define CY_U3P_LPP_GPIO_INVALUE1_ADDRESS                    (0xe00013d4)
#define CY_U3P_LPP_GPIO_INVALUE1                            (*(uvint32_t *)(0xe00013d4))
#define CY_U3P_LPP_GPIO_INVALUE1_DEFAULT                    (0x00000000)

/*
   If bit <x> is set, IN_VALUE is active for GPIO <x>
 */
#define CY_U3P_LPP_GPIO_INVALUE1_MASK                       (0x1fffffff) /* <0:28> W:R:0:No */
#define CY_U3P_LPP_GPIO_INVALUE1_POS                        (0)



/*
   GPIO Interrupt Vector
 */
#define CY_U3P_LPP_GPIO_INTR0_REG_ADDRESS                   (0xe00013e0)
#define CY_U3P_LPP_GPIO_INTR0_REG                           (*(uvint32_t *)(0xe00013e0))
#define CY_U3P_LPP_GPIO_INTR0_REG_DEFAULT                   (0x00000000)

/*
   If bit <x> is set, INTR is active for GPIO <x>
 */
#define CY_U3P_LPP_GPIO_INTR0_MASK                          (0xffffffff) /* <0:31> W:R:0:No */
#define CY_U3P_LPP_GPIO_INTR0_POS                           (0)



/*
   GPIO Interrupt Vector
 */
#define CY_U3P_LPP_GPIO_INTR1_REG_ADDRESS                   (0xe00013e4)
#define CY_U3P_LPP_GPIO_INTR1_REG                           (*(uvint32_t *)(0xe00013e4))
#define CY_U3P_LPP_GPIO_INTR1_REG_DEFAULT                   (0x00000000)

/*
   If bit <x> is set, INTR is active for GPIO <x>
 */
#define CY_U3P_LPP_GPIO_INTR1_MASK                          (0x1fffffff) /* <0:28> W:R:0:No */
#define CY_U3P_LPP_GPIO_INTR1_POS                           (0)



/*
   GPIO Interrupt Vector for PINs
 */
#define CY_U3P_LPP_GPIO_PIN_INTR_ADDRESS                    (0xe00013e8)
#define CY_U3P_LPP_GPIO_PIN_INTR                            (*(uvint32_t *)(0xe00013e8))
#define CY_U3P_LPP_GPIO_PIN_INTR_DEFAULT                    (0x00000000)

/*
   If bit <x> is set, INTR is active for GPIO.PIN[x]
 */
#define CY_U3P_LPP_GPIO_INTR0                               (1u << 0) /* <0:0> W:R:0:No */


/*
   If bit <x> is set, INTR is active for GPIO.PIN[x]
 */
#define CY_U3P_LPP_GPIO_INTR1                               (1u << 1) /* <1:1> W:R:0:No */


/*
   If bit <x> is set, INTR is active for GPIO.PIN[x]
 */
#define CY_U3P_LPP_GPIO_INTR2                               (1u << 2) /* <2:2> W:R:0:No */


/*
   If bit <x> is set, INTR is active for GPIO.PIN[x]
 */
#define CY_U3P_LPP_GPIO_INTR3                               (1u << 3) /* <3:3> W:R:0:No */


/*
   If bit <x> is set, INTR is active for GPIO.PIN[x]
 */
#define CY_U3P_LPP_GPIO_INTR4                               (1u << 4) /* <4:4> W:R:0:No */


/*
   If bit <x> is set, INTR is active for GPIO.PIN[x]
 */
#define CY_U3P_LPP_GPIO_INTR5                               (1u << 5) /* <5:5> W:R:0:No */


/*
   If bit <x> is set, INTR is active for GPIO.PIN[x]
 */
#define CY_U3P_LPP_GPIO_INTR6                               (1u << 6) /* <6:6> W:R:0:No */


/*
   If bit <x> is set, INTR is active for GPIO.PIN[x]
 */
#define CY_U3P_LPP_GPIO_INTR7                               (1u << 7) /* <7:7> W:R:0:No */



/*
   Block Identification and Version Number
 */
#define CY_U3P_LPP_GPIO_ID_ADDRESS                          (0xe00013f0)
#define CY_U3P_LPP_GPIO_ID                                  (*(uvint32_t *)(0xe00013f0))
#define CY_U3P_LPP_GPIO_ID_DEFAULT                          (0x00010004)

/*
   A unique number identifying the IP in the memory space.
 */
#define CY_U3P_LPP_GPIO_BLOCK_ID_MASK                       (0x0000ffff) /* <0:15> :R:0x0004:No */
#define CY_U3P_LPP_GPIO_BLOCK_ID_POS                        (0)


/*
   Version number for the IP.
 */
#define CY_U3P_LPP_GPIO_BLOCK_VERSION_MASK                  (0xffff0000) /* <16:31> :R:0x0001:No */
#define CY_U3P_LPP_GPIO_BLOCK_VERSION_POS                   (16)



/*
   Power, clock and reset control
 */
#define CY_U3P_LPP_GPIO_POWER_ADDRESS                       (0xe00013f4)
#define CY_U3P_LPP_GPIO_POWER                               (*(uvint32_t *)(0xe00013f4))
#define CY_U3P_LPP_GPIO_POWER_DEFAULT                       (0x00000000)

/*
   For blocks that must perform initialization after reset before becoming
   operational, this signal will remain de-asserted until initialization
   is complete.  In other words reading active=1 indicates block is initialized
   and ready for operation.
 */
#define CY_U3P_LPP_GPIO_ACTIVE                              (1u << 0) /* <0:0> W:R:0:Yes */


/*
   The block core clock is bypassed with the ATE clock supplied on TCK pin.
   Block operates normally. This mode is designed to allow functional vectors
   to be used during production test.
 */
#define CY_U3P_LPP_GPIO_PLL_BYPASS                          (1u << 29) /* <29:29> R:RW:0:No */


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
#define CY_U3P_LPP_GPIO_RESETN                              (1u << 31) /* <31:31> R:RW:0:No */



#endif /* _INCLUDED_GPIO_REGS_H_ */

/*[]*/

