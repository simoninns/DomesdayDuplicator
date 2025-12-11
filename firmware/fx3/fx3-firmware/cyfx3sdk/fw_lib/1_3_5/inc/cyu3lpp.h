/*
 ## Cypress FX3 Core Library Header (cyu3lpp.h)
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

#ifndef _INCLUDED_CYU3_LPP_H_
#define _INCLUDED_CYU3_LPP_H_

#include <cyu3os.h>
#include <cyu3types.h>
#include <cyu3system.h>
#include "cyu3externcstart.h"

/** \file cyu3lpp.h
    \brief All of the serial peripherals and GPIOs on the FX3 device share a set
    of common hardware resources that need to be initialized before any of these
    interfaces can be used. Further, the drivers for all of the serial peripherals
    share a single RTOS thread; because the CPU load due to each of these is
    expected to be minimal. This file defines the data types and APIs that are
    used for the central management of all these serial peripheral blocks.
 */

/**************************************************************************
 ********************************Data Types********************************
 **************************************************************************/

/** \brief List of IO modes.

    <B>Description</B>\n
    The FX3 device supports internal weak pull-ups or pull-downs on its GPIOs. These
    terminations can be used even on signals that are not used as GPIOs.

    This type lists the possible internal IO modes that can be selected for a FX3 GPIO.

    **\see
    *\see CyU3PGpioSetIoMode
 */
typedef enum CyU3PGpioIoMode_t
{
    CY_U3P_GPIO_IO_MODE_NONE = 0,               /**< No internal pull-up or pull-down. Default condition. */
    CY_U3P_GPIO_IO_MODE_WPU,                    /**< A weak pull-up is provided on the IO. */
    CY_U3P_GPIO_IO_MODE_WPD                     /**< A weak pull-down is provided on the IO. */
} CyU3PGpioIoMode_t;

/** \brief Clock divider values for sampling simple GPIOs.

    <B>Description</B>\n
    Sampling and updates of simple GPIOs on the FX3 device are performed based
    on a clock that is derived from the GPIO fast clock. This type lists the
    possible divisor values that can be used to derive this clock from the fast
    clock.

    **\see
    *\see CyU3PGpioClock_t
    *\see CyU3PGpioSetClock
 */
typedef enum CyU3PGpioSimpleClkDiv_t
{
    CY_U3P_GPIO_SIMPLE_DIV_BY_2 = 0,            /**< Simple GPIO clock frequency is fast clock by 2. */
    CY_U3P_GPIO_SIMPLE_DIV_BY_4,                /**< Simple GPIO clock frequency is fast clock by 4. */
    CY_U3P_GPIO_SIMPLE_DIV_BY_16,               /**< Simple GPIO clock frequency is fast clock by 16. */
    CY_U3P_GPIO_SIMPLE_DIV_BY_64,               /**< Simple GPIO clock frequency is fast clock by 64. */
    CY_U3P_GPIO_SIMPLE_NUM_DIV                  /**< Number of divider enumerations. */
} CyU3PGpioSimpleClkDiv_t;

/** \brief Clock configuration information for the GPIO block.

    <B>Description</B>\n
    The GPIO block on the FX3 makes use of three different clocks. The master
    (fast) clock for this block is directly divided down from the SYS_CLK.
    The block also uses a slow clock which can be derived from this fast clock.
    The complex GPIOs on FX3 can be clocked on either the fast or the slow clock.

    The simple GPIOs are clocked by another clock which is separately derived
    from the fast clock.

    This structure encapsulates all of the clock parameters for the GPIO clock.

    **\see
    *\see CyU3PGpioSetClock
    *\see CyU3PSysClockSrc_t
    *\see CyU3PGpioSimpleClkDiv_t
 */
typedef struct CyU3PGpioClock_t
{
    uint8_t fastClkDiv;                 /**< Divider value for the GPIO fast clock. The min value is 2 and max
                                             value is 16. */
    uint8_t slowClkDiv;                 /**< Divider value for the GPIO slow clock. This divisor applies on top of
                                             the fast clock and can be set to zero, to disable the slow clock.
                                             The min value is 0  (1 is not supported) and max value is 64. */
    CyBool_t halfDiv;                   /**< This allows the fast clock to be divided by a non integral value.
                                             If set to true, this adds 0.5 to the fastClkDiv value. This should
                                             be used only if the slow clock is disabled. */
    CyU3PGpioSimpleClkDiv_t simpleDiv;  /**< Divider value from fast clock for sampling simple GPIOs. */
    CyU3PSysClockSrc_t clkSrc;          /**< The clock source to be used for this peripheral. */
} CyU3PGpioClock_t;


/** \brief Serial peripheral interrupt handler function type.

    <B>Description</B>\n
    Each serial peripheral (I2C, I2S, SPI, UART and GPIO) on the FX3 device can have
    interrupts associated with it. The drivers for these blocks can register an
    interrupt handler function that the FX3 firmware framework will call when an
    interrupt is received. This block specific interrupt handler is registered
   through the CyU3PLppInit function.

    **\see
    *\see CyU3PLppInit
*/
typedef void (*CyU3PLppInterruptHandler) (
        void);

/** \cond LPP_INTERNAL
 */

/**************************************************************************
 ********************************* MACROS *********************************
 **************************************************************************/

#define CY_U3P_LPP_EVENT_GPIO_INTR      (1 << 3)        /**< GPIO interrupt event mask. */

#define CY_U3P_LPP_EVENT_I2S_INTR       (1 << 4)        /**< I2S interrupt event mask. */

#define CY_U3P_LPP_EVENT_I2C_INTR       (1 << 5)        /**< I2C interrupt event mask. */

#define CY_U3P_LPP_EVENT_UART_INTR      (1 << 6)        /**< UART interrupt event mask. */

#define CY_U3P_LPP_EVENT_SPI_INTR       (1 << 7)        /**< SPI interrupt event mask. */

/** \brief Send an event to the LPP driver thread.

    <B>Description</B>\n
    This is an internal function used to notify the LPP driver thread about
    peripheral interrupts.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the event has been sent successfully.
 */
extern CyU3PReturnStatus_t
CyU3PLppEventSend (
        uint32_t eventMask                      /**< The event mask to be set. */
        );

/** \endcond
 */

/**************************************************************************
 *************************** Function prototypes **************************
 **************************************************************************/

/** \brief Check if the boot firmware has left the GPIO block powered ON.

    <B>Description</B>\n
   In systems which make use of the boot firmware, it is possible that some of the
   GPIOs have been left configured by the boot firmware. In such a case, the GPIO
   block on the FX3 device should not be reset during firmware initialization.
   This function is used to check whether the boot firmware has requested the GPIO
   block to be left powered ON across firmware initialization.

    <B>Note</B>\n
   Please note that all the pins that have been left configured by the boot firmware
   need to be selected as simple GPIOs during IO Matrix configuration. Otherwise, these
   pins will be tri-state because the pin functionality is overridden.

    <B>Return value</B>\n
    * CyTrue if the boot firmware has left GPIO on.\n
    * CyFalse if the GPIO block is turned off.

    **\see
    *\see CyU3PDeviceConfigureIOMatrix
 */
extern CyBool_t
CyU3PLppGpioBlockIsOn (
        void);

/** \brief Register the specified peripheral block as active.

    <B>Description</B>\n
   The serial peripheral blocks on the FX3 device share some resources. While the individual peripheral
   blocks (GPIO, I2C, UART, SPI and I2S) can be turned on/off at runtime; the shared resources need to
   be kept initialized while any of these blocks are on. This function is used to manage the shared
   peripheral resources and also to keep track of which peripheral blocks are on.

   This function need to be called after initializing the clock for the corresponding peripheral interface.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS                - If the call is successful.\n
    * CY_U3P_ERROR_ALREADY_STARTED  - The block is already initialized.\n
   * CY_U3P_ERROR_INVALID_SEQUENCE - If the block init is being called without turning on the corresponding clock.

    **\see
    *\see CyU3PLppDeInit
    *\see CyU3PUartSetClock
    *\see CyU3PI2cSetClock
    *\see CyU3PI2sSetClock
    *\see CyU3PSpiSetClock
 */
extern CyU3PReturnStatus_t
CyU3PLppInit (
        CyU3PLppModule_t lppModule,             /**< The peripheral block being initialized. */
        CyU3PLppInterruptHandler intrHandler    /**< Interrupt handler function for the peripheral block. */
        );

/** \brief Register that a specified peripheral block has been made inactive.

    <B>Description</B>\n
   This function registers that a specified peripheral block is no longer in use. The function
   along with CyU3PLppInit() keeps track of the peripheral blocks that are ON; and manages the
   power on/off of the shared resources for these blocks.

    The clock for the block being disabled should be turned off only after this function is called.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS             - If the call is successful.\n
   * CY_U3P_ERROR_NOT_STARTED   - If the serial peripheral being stopped has not been started.
  
    **\see
    *\see CyU3PLppInit
 */
extern CyU3PReturnStatus_t
CyU3PLppDeInit (
        CyU3PLppModule_t lppModule              /**< The peripheral block being de-initialized. */
        );

/** \brief Enable the GPIO block clocks on the FX3 device.

    <B>Description</B>\n
    The GPIO block on the FX3 device makes use of multiple clocks. This function is used
    to select the frequency for these clocks and to turn them on.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS            - If the clocks were successfully configured and enabled.\n
   * CY_U3P_ERROR_BAD_ARGUMENT - If the clock configuration specified is invalid.
  
    **\see
    *\see CyU3PGpioClock_t
    *\see CyU3PGpioStopClock
 */
extern CyU3PReturnStatus_t
CyU3PGpioSetClock(
        CyU3PGpioClock_t *clk_p                 /**< The desired clock parameters. */
        );

/** \brief Set the frequency for and enable the I2S clock.

    <B>Description</B>\n
    The I2S block on the FX3 device needs to be clocked at different rates based
    on the output audio sample rate. This function sets the I2C block clock frequency
    and enables the clock.

    The internal clock for the I2S block needs to run at 16X the desired output
    clock frequency. Further, the FX3 device only supports integer and half divisors
    for deriving the internal clock from the SYS_CLK.

    This function sets the internal clock for the I2S block to the value that
    provides the closest approximation of the desired output clock frequency.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS             - If the I2S interface clock was setup as required.\n
    * CY_U3P_ERROR_BAD_ARGUMENT  - When invalid arqument is passed to the function.

    **\see
    *\see CyU3PI2sStopClock
 */
extern CyU3PReturnStatus_t
CyU3PI2sSetClock (
        uint32_t clkRate                        /**< Desired interface clock frequency. */
        );

/** \brief Set the frequency for and enable the I2C clock.

    <B>Description</B>\n
    The clock frequency for the I2C block depends on the desired output bit rate.
    This function configures this clock frequency as required and then enables
    the clock.

    The internal clock for the I2C block needs to run at 10X the desired output
    clock frequency. Further, the FX3 device only supports integer and half divisors
    for deriving the internal clock from the SYS_CLK.

    This function sets the internal clock for the I2C block to the value that
    provides the closest approximation of the desired output bit rate.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS            - If the I2C interface clock was setup as required.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - When invalid arqument is passed to the function.

    **\see
    *\see CyU3PI2cStopClock
 */
extern CyU3PReturnStatus_t
CyU3PI2cSetClock (
        uint32_t bitRate                        /**< Desired interface clock frequency. */
        );

/** \brief Set the frequency for and enable the UART block.

    <B>Description</B>\n
    The clock frequency for the UART block depends on the desired output baud rate.
    This function configures this clock frequency as required and then enables
    the clock.

    The internal clock for the UART block needs to run at 16X the desired output
    clock frequency. Further, the FX3 device only supports integer and half divisors
    for deriving the internal clock from the SYS_CLK.

    This function sets the internal clock for the UART block to the value that
    provides the closest approximation of the desired output baud rate.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS             - If the UART clock and baud rate was setup as required.\n
    * CY_U3P_ERROR_BAD_ARGUMENT  - When invalid arqument is passed to the function.

    **\see
    *\see CyU3PUartStopClock
 */
extern CyU3PReturnStatus_t
CyU3PUartSetClock (
        uint32_t baudRate                       /**< Desired baud rate for the UART interface. */
        );

/** \brief Set the frequency for and enable the SPI block.

    <B>Description</B>\n
    The clock frequency for the SPI block depends on the desired output bit rate.
    This function configures this clock frequency as required and then enables
    the clock.

    The internal clock for the SPI block needs to run at 2X the desired output
    clock frequency. Further, the FX3 device only supports integer and half divisors
    for deriving the internal clock from the SYS_CLK.

    This function sets the internal clock for the SPI block to the value that
    provides the closest approximation of the desired output bit rate.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS             - If the SPI clock has been successfully turned on.\n
    * CY_U3P_ERROR_BAD_ARGUMENT  - When invalid arqument is passed to the function.

    **\see
    *\see CyU3PSpiStopClock
 */
extern CyU3PReturnStatus_t
CyU3PSpiSetClock (
        uint32_t clock                          /**< Desired output bit rate. */
        );

/** \brief Disable the SPI block clock.

    <B>Description</B>\n
    This function disables the SPI block clock. This should only be called after
    the SPI block has been de-initialized.

    <B>Return value</B>\n
   * CY_U3P_SUCCESS               - if the SPI clock is turned off successfully.

    **\see
    *\see CyU3PSpiSetClock
 */
extern CyU3PReturnStatus_t
CyU3PSpiStopClock (
        void);

/** \brief Disable the I2C block clock.

    <B>Description</B>\n
    This function disables the I2C block clock. This should only be called after
    the I2C block has been de-initialized.

    <B>Return value</B>\n
   * CY_U3P_SUCCESS            - If the I2C clock was successfully turned off.
  
    **\see
    *\see CyU3PI2cSetClock
 */
extern CyU3PReturnStatus_t
CyU3PI2cStopClock (
        void);

/** \brief Disable the GPIO block clocks.

    <B>Description</B>\n
    This function disables all of the clocks associated with the GPIO block. This should
    only be called after the GPIO block has been de-initialized.

    <B>Return value</B>\n
   * CY_U3P_SUCCESS            - If the GPIO clocks were successfully turned off.

    **\see
    *\see CyU3PGpioSetClock
 */
extern CyU3PReturnStatus_t
CyU3PGpioStopClock (
        void);

/** \brief Disable the I2S block clock.

    <B>Description</B>\n
    This function disables the I2S block clock. This should only be called after the
    I2S block has been de-initialized.

    <B>Return value</B>\n
   * CY_U3P_SUCCESS             - If the I2S clock is successfully turned off.

    **\see
    *\see CyU3PI2sSetClock
 */
extern CyU3PReturnStatus_t
CyU3PI2sStopClock (
        void);

/** \brief Disable the UART block clock.

    <B>Description</B>\n
    This function disables the UART block clock. This should only be called after the
    UART block has been de-initialized.

    <B>Return value</B>\n
   * CY_U3P_SUCCESS             - if the UART clock was successfully turned off.

    **\see
    *\see CyU3PUartSetClock
 */
extern CyU3PReturnStatus_t
CyU3PUartStopClock (
        void);

/** \brief Set the IO drive strength for the I2C interface.

    <B>Description</B>\n
    The function sets the IO Drive strength for the I2C interface signals.
   The default IO drive strength for I2C is set to CY_U3P_DS_THREE_QUARTER_STRENGTH.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the I2C block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT   - If the drive strength requested is invalid.
  
    **\see
    *\see CyU3PDriveStrengthState_t
 */
extern CyU3PReturnStatus_t
CyU3PSetI2cDriveStrength (
        CyU3PDriveStrengthState_t i2cDriveStrength      /**< Drive strength desired for I2C signals. */
        );

/** \brief Set IO mode for the selected GPIO.

    <B>Description</B>\n
    This function enables or disables internal pull-ups/pull-downs on the selected
    FX3 IO pin. This IO mode change will take about 5 us to become active.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS            - If the operation is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - If the gpioId or ioMode requested is invalid.

    **\see
    *\see CyU3PGpioIoMode_t
 */
extern CyU3PReturnStatus_t
CyU3PGpioSetIoMode (
        uint8_t gpioId,                         /**< GPIO Pin to be updated. */
        CyU3PGpioIoMode_t ioMode                /**< Desired pull-up/pull-down mode. */
        );

/** \brief Set the IO drive strength for all FX3 GPIOs.

    <B>Description</B>\n
    This function updates the drive strength of all FX3 GPIOs. This is an
    alternative for setting the drive strength for each port separately.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS            - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED  - If the GPIO block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - If the drive strength requested is invalid.

    **\see
    *\see CyU3PDriveStrengthState_t
 */
extern CyU3PReturnStatus_t
CyU3PSetGpioDriveStrength (
        CyU3PDriveStrengthState_t gpioDriveStrength     /**< Desired GPIO Drive strength */
        );

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3LPP_H_ */

/*[]*/
