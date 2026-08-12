/*
 ## Cypress FX3 Core Library Header (cyu3vic.h)
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

#ifndef _INCLUDED_CYU3PVIC_H_
#define _INCLUDED_CYU3PVIC_H_

#include "cyu3types.h"
#include "cyu3externcstart.h"

/** \file cyu3vic.h
    \brief Internal functions for managing the VIC and interrupts on the FX3 device.
 */

/**************************************************************************
 ******************************* Data types *******************************
 **************************************************************************/

/** \brief Vector numbers for different interrupt sources.

    <B>Description</B>\n
    The following enumeration lists the interrupt vector numbers on the FX3 device.
 */
typedef enum
{
    CY_U3P_VIC_GCTL_CORE_VECTOR = 0,            /**< 0: Low power mode entry/exit interrupt. */
    CY_U3P_VIC_SWI_VECTOR,                      /**< 1: Software interrupt. */
    CY_U3P_VIC_DEBUG_RX_VECTOR,                 /**< 2: Unused debug vector. */
    CY_U3P_VIC_DEBUG_TX_VECTOR,                 /**< 3: Unused debug vector. */
    CY_U3P_VIC_WDT_VECTOR,                      /**< 4: Watchdog timer interrupt. */
    CY_U3P_VIC_BIAS_CORRECT_VECTOR,             /**< 5: Timer for PVT Bias correction. */
    CY_U3P_VIC_PIB_DMA_VECTOR,                  /**< 6: GPIF (PIB) DMA interrupt. */
    CY_U3P_VIC_PIB_CORE_VECTOR,                 /**< 7: GPIF (PIB) Core interrupt. */
    CY_U3P_VIC_UIB_DMA_VECTOR,                  /**< 8: USB DMA interrupt. */
    CY_U3P_VIC_UIB_CORE_VECTOR,                 /**< 9: USB core interrupt. */
    CY_U3P_VIC_UIB_CONTROL_VECTOR,              /**< 10: Unused interrupt vector. */
    CY_U3P_VIC_SIB_DMA_VECTOR,                  /**< 11: Storage port DMA interrupt (FX3S only). */
    CY_U3P_VIC_SIB0_CORE_VECTOR,                /**< 12: Storage port 0 core interrupt (FX3S only). */
    CY_U3P_VIC_SIB1_CORE_VECTOR,                /**< 13: Storage port 1 core interrupt (FX3S only). */
    CY_U3P_VIC_RESERVED_15_VECTOR,              /**< 14: Unused interrupt vector. */
    CY_U3P_VIC_I2C_CORE_VECTOR,                 /**< 15: I2C block interrupt. */
    CY_U3P_VIC_I2S_CORE_VECTOR,                 /**< 16: I2S block interrupt. */
    CY_U3P_VIC_SPI_CORE_VECTOR,                 /**< 17: SPI block interrupt. */
    CY_U3P_VIC_UART_CORE_VECTOR,                /**< 18: UART block interrupt. */
    CY_U3P_VIC_GPIO_CORE_VECTOR,                /**< 19: GPIO block interrupt. */
    CY_U3P_VIC_LPP_DMA_VECTOR,                  /**< 20: Serial peripheral (I2C, I2S, SPI and UART) DMA interrupt. */
    CY_U3P_VIC_GCTL_PWR_VECTOR,                 /**< 21: VBus detect interrupt. */
    CY_U3P_VIC_NUM_VECTORS                      /**< Number of valid FX3 interrupt vectors. */
} CyU3PVicVector_t;

/** \cond VIC_INTERNAL
 */

/**************************************************************************
 ********************** Global variable declarations **********************
 **************************************************************************/

extern uint16_t glOsTimerInterval;  /* Internal variable for OS timer configuration. Should not be
                                       modified from outside the library. */

/** \endcond
 */

/**************************************************************************
 *************************** Function prototypes **************************
 **************************************************************************/

/** \brief Enable the interrupt for the specified vector number.

    <B>Description</B>\n
    Enable the interrupt with the specified vector number. All FX3 interrupts
    are enabled/disabled at appropriate times by the corresponding drivers in
    the FX3 firmware. This function should not be directly used by the user
    application.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PVicDisableInt
    *\see CyU3PVicClearInt
 */
extern void
CyU3PVicEnableInt(
        uint32_t vectorNum      /**< Vector number to be enabled (0 - CY_U3P_VIC_NUM_VECTORS). */
        );

/** \brief Disable the interrupt for the specified vector number.

    <B>Description</B>\n
    Disable the interrupt with the specified vector number. All FX3 interrupts
    are enabled/disabled at appropriate times by the corresponding drivers in
    the FX3 firmware. This function should not be directly used by the user
    application.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PVicEnableInt
    *\see CyU3PVicClearInt
 */
extern void
CyU3PVicDisableInt (
        uint32_t vectorNum      /**< Interrupt vector number (0 - CY_U3P_VIC_NUM_VECTORS) to be disabled. */
        );

/** \brief Clear any interrupt that has occurred.

    <B>Description</B>\n
    This function marks the current interrupt cleared at the VIC level, so that the
    VIC can raise the next pending interrupt. The actual cause of the interrupt has
    to be cleared or masked out before doing this.

    This function should not be used directly by the user application.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PVicEnableInt
    *\see CyU3PVicDisableInt
 */
extern void
CyU3PVicClearInt (
        void);

/** \brief Get the raw interrupt status.

    <B>Description</B>\n
    Get a bit vector that represents the current status of all FX3 device interrupts.
    The bit vector will report an interrupt even if it is currently disabled at the
    VIC level.

    <B>Return value</B>\n
    * Bit vector reporting the current status of all FX3 interrupts.

    **\see
    *\see CyU3PVicIRQGetStatus
 */
extern uint32_t
CyU3PVicIntGetStatus (
        void);

/** \brief Get the IRQ interrupt status.

    <B>Description</B>\n
    This function returns a bit vector that reports the interrupt vectors that are
    both active and enabled.

    <B>Return value</B>\n
    * Bit vector representing the active and enabled interrupts.

    **\see
    *\see CyU3PVicIntGetStatus
 */
extern uint32_t
CyU3PVicIRQGetStatus (
        void);

/** \brief Set the priority level of the interrupt vector.

    <B>Description</B>\n
    Set the priority level for a specified interrupt vector. This function is not
    used in the library and is not expected to be called. Interrupts in FX3 firmware
    are split into two groups: a high priority group which is not pre-emptable; and
    a low priority group that is pre-emptable.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PVicIntGetPriority
 */
extern void
CyU3PVicIntSetPriority (
        uint32_t vectorNum,     /**< Interrupt vector number (0 - CY_U3P_VIC_NUM_VECTORS). */
        uint32_t priority       /**< Priority level to be set (0 - 15) */
        );

/** \brief Get the priority level of the interrupt vector.

    <B>Description</B>\n
    Returns the currently programmed priority level for the specified interrupt
    vector. 0 is the highest priority level, and 15 is the lowest priority level.

    <B>Return value</B>\n
    * The priority level for the specified interrupt vector.

    **\see
    *\see CyU3PVicIntSetPriority
 */
extern uint32_t
CyU3PVicIntGetPriority (
        uint32_t vectorNum              /**< Interrupt vector number. */
        );

/** \brief The function initializes the VIC.

    <B>Description</B>\n
    This function initializes the PL192 Vectored Interrupt Controller on the FX3
    device and sets up the interrupt vector addresses for all FX3 device interrupts.

    This function is called internally during device initialization and should not
    be called directly.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PVicInit (
        void);

/** \brief The function initializes the interrupt vector table.

    <B>Description</B>\n
    This function sets up the interrupt vector address table for the FX3 device, and
    is called internally by the library during device initialization.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PVicInit
 */
extern void
CyU3PVicSetupIntVectors (
        void);

/** \brief This function disables all FX3 interrupts at the VIC level.

    <B>Description</B>\n
    This function can be used to disable all FX3 interrupts at the VIC level.
    The function returns a mask that represent the interrupts that were enabled
    before this function was called. It is expected that this mask would be used
    to re-enable the interrupts using the CyU3PVicEnableInterrupts function.

    <B>Return value</B>\n
    * A mask that represents the interrupts that were enabled before this call.

    **\see
    *\see CyU3PVicEnableInterrupts
 */
extern uint32_t
CyU3PVicDisableAllInterrupts (
        void);

/** \brief This function enables the specified interrupts at the VIC level.

    <B>Description</B>\n
    This function can be used to re-enable interrupts that were previously
    disabled through the CyU3PVicDisableAllInterrupts function. These two
    functions can be used together to save and restore interrupt states
    across critical sections of code.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PVicDisableAllInterrupts
 */
extern void
CyU3PVicEnableInterrupts (
        uint32_t mask           /**< Bit-mask representing interrupts to be enabled. */
        );

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3PVIC_H_ */

/*[]*/

