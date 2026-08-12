/*
 ## Cypress FX3 Core Library Header (cyu3gpio.h)
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

#ifndef _INCLUDED_CYU3_GPIO_H_
#define _INCLUDED_CYU3_GPIO_H_

#include "cyu3types.h"
#include "cyu3system.h"
#include "cyu3lpp.h"
#include "cyu3externcstart.h"

/** \file cyu3gpio.h
    \brief The GPIO manager module is responsible for handling general purpose
    IO pins on the FX3 device.

    <B>Description</B>\n
    Almost any pin of the FX3 device can be used as a General Purpose I/O (GPIO),
    subject to the condition that it is not being used for any other purpose.
    The FX3 device supports two classes of GPIOs - Simple and Complex.

    Simple GPIOs provide software controlled and observable input and output
    capability only. The only additional functionality supported on these are
    interrupt detection on edge or signal level. Any unused pin of the FX3 can
    be mapped and used as a simple GPIO.

    Complex GPIOs are value-added blocks that support additional functions like
    PWM output, pulsing, time measurements and timers. A maximum of 8 pins on the
    FX3 can be configured as complex GPIOs, and each of these pins should have
    a different remainder module 8.

    As each GPIO is controlled through a separate set of registers on the FX3 device,
    they can only be configured individually and multiple GPIOs cannot be updated
    simultaneously.
*/

/**************************************************************************
******************************* Data Types *******************************
**************************************************************************/

/** \brief Enumerated list of all GPIO complex modes.

    <B>Description</B>\n
    This enumeration lists the complex GPIO modes supported by the FX3 device.

    Of these, the CY_U3P_GPIO_MODE_SAMPLE_NOW, CY_U3P_GPIO_MODE_PULSE_NOW,
  CY_U3P_GPIO_MODE_PULSE, CY_U3P_GPIO_MODE_MEASURE_LOW_ONCE,
  CY_U3P_GPIO_MODE_MEASURE_HIGH_ONCE, CY_U3P_GPIO_MODE_MEASURE_NEG_ONCE,
  CY_U3P_GPIO_MODE_MEASURE_POS_ONCE and CY_U3P_GPIO_MODE_MEASURE_ANY_ONCE
    modes cannot be directly selected. To use any of these modes, the complex
    GPIO should be configured with CY_U3P_GPIO_MODE_STATIC configuration; and
    then special GPIO APIs need to be used for completing the desired operations.

    **\see
    *\see CyU3PGpioTimerMode_t
    *\see CyU3PGpioIntrMode_t
  */
typedef enum CyU3PGpioComplexMode_t
{
    CY_U3P_GPIO_MODE_STATIC = 0,        /**< Drives simple static values on GPIO. */
    CY_U3P_GPIO_MODE_TOGGLE,            /**< Toggles the output when timer = threshold. */
    CY_U3P_GPIO_MODE_SAMPLE_NOW,        /**< Read current timer value into threshold. This is a one time
                                             operation and resets mode to static. This mode should not be
                                             set by the configure function. This will be done by the
                                           CyU3PGpioComplexSampleNow API. */
    CY_U3P_GPIO_MODE_PULSE_NOW,         /**< Mode used for driving a pulse out on a specified GPIO pin at
                                             a specified time. This mode should not be set directly by the
                                             configure function. This will be done by the CyU3PGpioComplexPulseNow
                                             API. */
    CY_U3P_GPIO_MODE_PULSE,             /**< This is similar to the PULSE_NOW mode, and is used to drive a
                                             pulse out when the timer associated with the GPIO reaches 0.
                                             This mode should not be set by the configure function and will be
                                             done by the CyU3PGpioComplexPulse API. */
    CY_U3P_GPIO_MODE_PWM,               /**< Drive a Pulse Width Modulated (PWM) waveform out on the specified
                                             GPIO. The ON and OFF times for the signal are defined by the timer
                                             period and the threshold value. This is a continuous operation. */
    CY_U3P_GPIO_MODE_MEASURE_LOW,       /**< This mode is used to measure the length of time for which a specified
                                             input pin is low. This is a continuous operation which provides
                                             the interval duration every time there is a positive edge on the pin. */
    CY_U3P_GPIO_MODE_MEASURE_HIGH,      /**< This mode is similar to the MEASURE_LOW mode and is used to measure
                                             the duration for which an input pin is driver high. */
    CY_U3P_GPIO_MODE_MEASURE_LOW_ONCE,  /**< This mode is similar to the MEASURE_LOW mode, except that the pulse
                                             duration is measured only once. The mode of the pin will revert to
                                             STATIC at the end of the first low pulse. This mode cannot be directly
                                             selected and is used internally by the CyU3PGpioComplexMeasureOnce API. */
    CY_U3P_GPIO_MODE_MEASURE_HIGH_ONCE, /**< This mode is similar to the MEASURE_LOW_ONCE mode, and is used to
                                             do a single duration measurement for a high pulse. This mode is also
                                             used internally by the CyU3PGpioComplexMeasureOnce API. */
    CY_U3P_GPIO_MODE_MEASURE_NEG,       /**< This mode is used to measure the delay until the specified GPIO input
                                             goes low. This is a continuous operation, where the Threshold value
                                             is updated with the current timer value at the point where the signal
                                             goes low. */
    CY_U3P_GPIO_MODE_MEASURE_POS,       /**< This mode is similar to the MEASURE_NEG mode, and is a continuous mode
                                             which can be used to measure the delay until the GPIO input is driven
                                             high. */
    CY_U3P_GPIO_MODE_MEASURE_ANY,       /**< This mode is similar to the MEASURE_NEG and MEASURE_POS modes; and
                                             is used to measure the delay until there is any change in the GPIO
                                             input signal. */
    CY_U3P_GPIO_MODE_MEASURE_NEG_ONCE,  /**< This mode is similar to the MEASURE_NEG mode, except that only a
                                             single delay measurement is performed. This mode cannot be directly
                                             selected, and is used internally by the CyU3PGpioComplexMeasureOnce
                                             API. */
    CY_U3P_GPIO_MODE_MEASURE_POS_ONCE,  /**< This mode is similar to the MEASURE_POS mode, except that only a
                                             single delay measurement is performed. This mode cannot be directly
                                             selected, and is used internally by the CyU3PGpioComplexMeasureOnce
                                             API. */
    CY_U3P_GPIO_MODE_MEASURE_ANY_ONCE   /**< This mode is similar to the MEASURE_POS mode, except that only a
                                             single delay measurement is performed. This mode cannot be directly
                                             selected, and is used internally by the CyU3PGpioComplexMeasureOnce
                                             API. */
} CyU3PGpioComplexMode_t;

/** \brief List of possible clock sources for GPIO timers.

    <B>Description</B>\n
    The timer blocks associated with Complex GPIOs can be clocked internally, or
    based on the input signal connected to the corresponding pin. This enumerated
    type lists the various clocking options for these GPIO timers.

    **\see
    *\see CyU3PGpioClock_t
    *\see CyU3PGpioComplexMode_t
    *\see CyU3PGpioIntrMode_t
  */
typedef enum CyU3PGpioTimerMode_t
{
    CY_U3P_GPIO_TIMER_SHUTDOWN = 0,	/**< Timer not in use (no clock applied). */
    CY_U3P_GPIO_TIMER_HIGH_FREQ,	/**< Use GPIO fast clock. */
    CY_U3P_GPIO_TIMER_LOW_FREQ,		/**< Use GPIO slow clock. */
    CY_U3P_GPIO_TIMER_STANDBY_FREQ,	/**< Use FX3 back-up clock (32 KHz). */
    CY_U3P_GPIO_TIMER_POS_EDGE,		/**< Timer updates on positive edge of input. */
    CY_U3P_GPIO_TIMER_NEG_EDGE,		/**< Timer updates on negative edge of input. */
    CY_U3P_GPIO_TIMER_ANY_EDGE,		/**< Timer updates on either edge of input. */
    CY_U3P_GPIO_TIMER_RESERVED		/**< Reserved for future use. */
} CyU3PGpioTimerMode_t;

/** \brief List of interrupt modes supported by FX3 GPIOs.

    <B>Description</B>\n
    GPIOs on the FX3 device can be configured to interrupt the firmware under
    various conditions. For simple GPIOs, the interrupt conditions are based on
    specific input levels or edge detection. Complex GPIOs support additional
    interrupts based on the timer period and the threshold value.

    This enumerated type lists the various interrupt modes supported by FX3 GPIOs.

    **\see
    *\see CyU3PGpioComplexMode_t
    *\see CyU3PGpioTimerMode_t
  */
typedef enum CyU3PGpioIntrMode_t
{
    CY_U3P_GPIO_NO_INTR = 0,        /**< GPIO interrupt is unused. */
    CY_U3P_GPIO_INTR_POS_EDGE,      /**< Interrupt is triggered for positive edge of input. */
    CY_U3P_GPIO_INTR_NEG_EDGE,      /**< Interrupt is triggered for negative edge of input. */
    CY_U3P_GPIO_INTR_BOTH_EDGE,     /**< Interrupt is triggered on either edge of input. */
    CY_U3P_GPIO_INTR_LOW_LEVEL,     /**< Interrupt is triggered when the input value is low. */
    CY_U3P_GPIO_INTR_HIGH_LEVEL,    /**< Interrupt is triggered when the input value is high. */
    CY_U3P_GPIO_INTR_TIMER_THRES,   /**< Interrupt is triggered when the timer crosses the
                                         threshold. Valid only for complex GPIO pins. */
    CY_U3P_GPIO_INTR_TIMER_ZERO     /**< Interrupt is triggered when the timer reaches zero.
                                         Valid only for complex GPIO pins. */
} CyU3PGpioIntrMode_t;

/** \brief Configuration information for simple GPIOs.

    <B>Description</B>\n
    Simple GPIOs on the FX3 have configurable properties such as I/O direction,
    output value and interrupt mode. This structure holds all of the information
    required to configure a simple GPIO on the FX3 device.

    The input and output stages are configured seperately. Care should be
    taken that the output stage is only turned on where desired, so that external
    devices are not damaged.

    For use as a normal output pin, both driveLowEn and driveHighEn need to be
    CyTrue and inputEn needs to be CyFalse. Similarly for use as a normal input pin,
    inputEn must be CyTrue and both driveLowEn and driveHighEn should be CyFalse.

    When output stage is enabled, the outValue field contains the initial state
    of the pin. CyTrue means high and CyFalse means low.

    **\see
    *\see CyU3PGpioIntrMode_t
    *\see CyU3PGpioSetSimpleConfig
  */
typedef struct CyU3PGpioSimpleConfig_t
{
    CyBool_t outValue;              /**< Initial output on the GPIO if configured as output: CyFalse = 0, CyTrue = 1. */
    CyBool_t driveLowEn;            /**< When set true, the output driver is enabled for outValue = CyFalse,
                                         otherwise tristated. */
    CyBool_t driveHighEn;           /**< When set true, the output driver is enabled for outValue = CyTrue,
                                         otherwise tristated. */
    CyBool_t inputEn;               /**< When set true, the input stage on the pin is enabled. */
    CyU3PGpioIntrMode_t intrMode;   /**< Interrupt mode for the GPIO. */
} CyU3PGpioSimpleConfig_t;

/** \brief Configuration information for complex GPIO pins.

    <B>Description</B>\n
    Complex GPIOs on FX3 can be configured to behave in different ways.
    All of the parameters that are used for configuring simple GPIOs are
    also applicable for complex GPIOs.

    In addition to these parameters, the pinMode parameter is used to specify
    the specific complex GPIO functionality desired. The timerMode, timer,
    period and threshold parameters are used to configure the complex GPIO
    timer for this pin; in cases where the timer is required.

    **\see
    *\see CyU3PGpioSetComplexConfig
    *\see CyU3PGpioSimpleConfig_t
    *\see CyU3PGpioComplexMode_t
    *\see CyU3PGpioTimerMode_t
    *\see CyU3PGpioIntrMode_t
  */

typedef struct CyU3PGpioComplexConfig_t
{
    CyBool_t outValue;                  /**< Initial value for the GPIO if configured as output:
                                             CyFalse = 0, CyTrue = 1. */
    CyBool_t driveLowEn;                /**< When set true, the output driver is enabled for outValue = CyFalse(0),
                                             otherwise tristated. */
    CyBool_t driveHighEn;               /**< When set true, the output driver is enabled for outValue = CyTrue(1),
                                             otherwise tristated. */
    CyBool_t inputEn;                   /**< When set true, the input state is enabled. */
    CyU3PGpioComplexMode_t pinMode;     /**< Complex GPIO operating mode. */
    CyU3PGpioIntrMode_t    intrMode;    /**< Interrupt mode for the GPIO. */
    CyU3PGpioTimerMode_t   timerMode;   /**< Timer mode. */
    uint32_t timer;                     /**< Timer initial value. */
    uint32_t period;                    /**< Timer period. */
    uint32_t threshold;                 /**< Timer threshold value. */
} CyU3PGpioComplexConfig_t;

/** \brief GPIO callback function type.

    <B>Description</B>\n
    The GPIO manager in FX3 firmware supports event notifications when a GPIO related
    interrupt is detected. This data type defines the signature of the callback function
    that can be registered to receive the event notifications.

    **\see
    *\see CyU3PRegisterGpioCallBack
*/
typedef void (*CyU3PGpioIntrCb_t)(
        uint8_t gpioId             /**< Indicates the GPIO pin id that triggered the interrupt. */
        );

/**************************************************************************
*************************** Function prototypes **************************
**************************************************************************/

/** \brief Initializes the GPIO Manager.

    <B>Description</B>\n
    This function initializes the GPIO manager in the FX3 firmware and powers
    up the GPIO block on the FX3 device. All other GPIO functions in the SDK
    can be used only after this function is called.

    The clock parameters for the GPIO block are defined through this function.
    Three different clocks are associated with the FX3 GPIO clock.

    The FAST clock is derived from the FX3 system clock, and its frequency is
    determined by the clkSrc and fastClkDiv parameters. The SLOW clock is derived
    from the FAST clock and its frequency is determined by the slowClkDiv value.
    The operating clock for various complex GPIO timers can be selected from
    among the FAST clock, the SLOW clock and a fixed 32 KHz frequency.

    All simple GPIOs function (are updated or sampled) based on a separate clock
    which is also derived from the FAST clock. The frequency of this clock is
    determined by the simpleDiv value.

    This function also allows for a GPIO interrupt callback to be registered.
    This function will be called when any GPIO related interrupt is raised by
    the device, and will receive the GPIO ID as a parameter. Please note that
    this interrupt call is made in interrupt context, which means that any blocking
    API calls cannot be made from this callback.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS               - If the GPIO initialization is successful.\n
    * CY_U3P_ERROR_ALREADY_STARTED - If the GPIO has already been initialized.\n
    * CY_U3P_ERROR_NULL_POINTER    - If clk_p is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT    - If an incorrect / invalid clock value is passed.

    **\see
    *\see CyU3PGpioClock_t
    *\see CyU3PGpioIntrCb_t
    *\see CyU3PGpioDeInit
 */
extern CyU3PReturnStatus_t
CyU3PGpioInit (
        CyU3PGpioClock_t *clk_p,                /**< Clock settings for the GPIO block. */
        CyU3PGpioIntrCb_t irq                   /**< GPIO interrupt callback function. */
        );

/** \brief De-initializes the GPIO Manager.

    <B>Description</B>\n
    De-initialize the GPIO manager in FX3 firmware and power off the GPIO block
    on the FX3 device.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS           - If the GPIO de-init is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - If the GPIO block was not previously initialized.

    **\see
    *\see CyU3PGpioInit
 */
extern CyU3PReturnStatus_t
CyU3PGpioDeInit (
        void);

/** \brief Configures a simple GPIO.

    <B>Description</B>\n
    This function configures and enables a simple GPIO pin. The pin being
    configured needs to have been selected as a simple GPIO through the
    CyU3PDeviceConfigureIOMatrix or CyU3PDeviceGpioOverride API calls.

    The operating parameters for the GPIO pin as passed in as parameters.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the GPIO block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT   - If any of the parameters are incorrect/invalid.\n
    * CY_U3P_ERROR_NULL_POINTER   - If cfg_p is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - If the pin has not been selected as a GPIO in the IO matrix configuration.

    **\see
    *\see CyU3PGpioSimpleConfig_t
    *\see CyU3PDeviceConfigureIOMatrix
    *\see CyU3PDeviceGpioOverride
    *\see CyU3PGpioDisable
    *\see CyU3PGpioSimpleSetValue
    *\see CyU3PGpioSimpleGetValue
    *\see CyU3PGpioSetValue
    *\see CyU3PGpioGetValue
 */
extern CyU3PReturnStatus_t
CyU3PGpioSetSimpleConfig (
       uint8_t gpioId,                  /**< Id of the pin being selected as a GPIO. */
       CyU3PGpioSimpleConfig_t *cfg_p   /**< Desired GPIO configuration. */
       );

/** \brief Configures a complex GPIO pin.

    <B>Description</B>\n
   This function is used to configure and enable a complex GPIO pin. This
   function needs to be called before using the complex GPIO pin.

    A maximum of 8 complex GPIOs can be used on FX3, and the assignment of
    pin to complex GPIO is done on a MODULO 8 basis. i.e., only one of the
    pins 0, 8, 16, .. 56 can be used as complex GPIO and so on.

    It is possible to use the timer associated with a complex GPIO without
    actually configuring or using the corresponding pin as a GPIO. In this
    case, all of the inputEn, driveLowEn and driveHighEn values can be set
    to CyFalse.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the GPIO block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT   - If any of the parameters are incorrect/invalid.\n
    * CY_U3P_ERROR_NULL_POINTER   - If cfg_p is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - If the pin has not been selected as a complex GPIO in the IO matrix.

    **\see
    *\see CyU3PGpioComplexMode_t
    *\see CyU3PGpioComplexConfig_t
    *\see CyU3PGpioSetSimpleConfig
    *\see CyU3PGpioDisable
    *\see CyU3PGpioComplexUpdate
    *\see CyU3PGpioComplexGetThreshold
    *\see CyU3PGpioComplexWaitForCompletion
    *\see CyU3PGpioComplexSampleNow
    *\see CyU3PGpioComplexPulseNow
    *\see CyU3PGpioComplexPulse
    *\see CyU3PGpioComplexMeasureOnce
 */
extern CyU3PReturnStatus_t
CyU3PGpioSetComplexConfig (
       uint8_t gpioId,              	        /**< GPIO id to be modified */
       CyU3PGpioComplexConfig_t *cfg_p          /**< Desired GPIO configuration. */
       );

/** \brief Disables a GPIO pin.

    <B>Description</B>\n
    This function is used to disable a GPIO pin which was previously configured
    as a simple or a complex GPIO.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS            - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED  - If the GPIO block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - If the GPIO id is invalid.

    **\see
    *\see CyU3PGpioSetSimpleConfig
    *\see CyU3PGpioSetComplexConfig
 */
extern CyU3PReturnStatus_t
CyU3PGpioDisable (
        uint8_t gpioId		                /**< GPIO ID to be disabled */
	);

/** \brief Query the state of GPIO input.

    <B>Description</B>\n
    Get the current state of an input pin configured as a simple or a complex GPIO.
    This function can also retrieve the last updated outValue for an output pin.

    This API can be used to get the current state of an input pin, even if it is
    configured as an interrupt source.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the GPIO block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT   - If the GPIO id is invalid.\n
    * CY_U3P_ERROR_NULL_POINTER   - If value_p is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - If the pin is not selected as a GPIO in the IOMATRIX.

    **\see
    *\see CyU3PGpioSetSimpleConfig
    *\see CyU3PGpioSetComplexConfig
    *\see CyU3PGpioSimpleGetValue
    *\see CyU3PGpioSetValue
    *\see CyU3PGpioGetIOValues
 */
extern CyU3PReturnStatus_t
CyU3PGpioGetValue (
        uint8_t  gpioId,                /**< GPIO id to be queried. */
        CyBool_t *value_p               /**< Output parameter that will be filled with the GPIO value. */
        );

/** \brief Query the state of simple GPIO input.

    <B>Description</B>\n
    Get the current signal state of a simple GPIO configured as an input signal.
    The CyU3PGpioGetValue API supports all kinds of GPIOs and has a fair amount
    of conditions checks that slow the operation down. This dedicated API works
    only on simple GPIOs and will give better GPIO access performance.

    This API can be used to get the current state of an input pin, even if it is
    configured as an interrupt source.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the GPIO block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT   - If the GPIO id is invalid.\n
    * CY_U3P_ERROR_NULL_POINTER   - If value_p is NULL.

    **\see
    *\see CyU3PGpioSetSimpleConfig
    *\see CyU3PGpioSimpleSetValue
    *\see CyU3PGpioGetValue
    *\see CyU3PGpioGetIOValues
 */
extern CyU3PReturnStatus_t
CyU3PGpioSimpleGetValue (
        uint8_t  gpioId,                /**< GPIO id to be queried. */
        CyBool_t *value_p               /**< Output parameter that will be filled with the GPIO value. */
        );

/** \brief Update the state of a GPIO output.

    <B>Description</B>\n
    This function updates the state of a GPIO output pin. The driveLowEn and driveHighEn
    values set at the time of GPIO configuration will determine whether the pin is actually
    driven with the desired value or tri-stated.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the GPIO block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT   - If the GPIO id is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - If the pin is not selected as a GPIO in the IOMATRIX.

    **\see
    *\see CyU3PGpioSetSimpleConfig
    *\see CyU3PGpioSetComplexConfig
    *\see CyU3PGpioSimpleSetValue
    *\see CyU3PGpioGetValue
 */
extern CyU3PReturnStatus_t
CyU3PGpioSetValue (
        uint8_t  gpioId,                /**< GPIO id to be modified. */
        CyBool_t  value                 /**< Value to set on the GPIO pin. */
        );

/** \brief Update the state of a simple GPIO output pin.

    <B>Description</B>\n
    This function updates the state of a simple GPIO output pin. This API works
    only for simple GPIOs and will be faster than the more generic CyU3PGpioSetValue
    API call.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the GPIO block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT   - If the GPIO id is invalid.

    **\see
    *\see CyU3PGpioSetSimpleConfig
    *\see CyU3PGpioSetValue
    *\see CyU3PGpioSimpleGetValue
 */
extern CyU3PReturnStatus_t
CyU3PGpioSimpleSetValue (
        uint8_t  gpioId,                /**< GPIO id to be modified. */
        CyBool_t  value                 /**< Value to set on the GPIO pin. */
        );

/** \brief Get the current state of all GPIOs.

    <B>Description</B>\n
    This API returns the state of all FX3 IO pins. The state information
    is returned in the form of a bit vector, where each bit represents the
    state of the corresponding GPIO pin. The retrieved information is valid
    only for pins configured as GPIOs.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the GPIO block has not been initialized.

    **\see
    *\see CyU3PGpioGetValue
    *\see CyU3PGpioSimpleGetValue
 */
extern CyU3PReturnStatus_t
CyU3PGpioGetIOValues (
        uint32_t *gpioVal0_p,           /**< Bit vector that represents the states of GPIO 31 : 00. */
        uint32_t *gpioVal1_p            /**< Bit vector that represents the states of GPIO 60 : 32. The
                                             upper three bits are unused and reserved. */
        );

/** \brief Update the timer period and threshold associated with a complex GPIO.

    <B>Description</B>\n
    The function updates the timer period and threshold value associated with a
    complex GPIO. This operation is only permitted if the corresponding is configured
    in the PWM or STATIC modes.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the GPIO block has not been initialized.\n
   * CY_U3P_ERROR_BAD_ARGUMENT   - If the IO pin is invalid.

    **\see
    *\see CyU3PGpioSetComplexConfig
    *\see CyU3PGpioComplexGetThreshold
 */
extern CyU3PReturnStatus_t
CyU3PGpioComplexUpdate (
        uint8_t gpioId,                 /**< The complex GPIO to sample. */
        uint32_t threshold,             /**< New timer threshold value to be updated. */
        uint32_t period                 /**< New timer period value to be updated. */
        );

/** \brief Read the threshold value associated with a complex GPIO.

    <B>Description</B>\n
    This function reads the current threshold value associated with a complex
    GPIO. This function is used in the various MEASURE modes of the GPIO to get
    the measured pulse/signal duration.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the GPIO block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT   - If the IO specified is not valid.\n
    * CY_U3P_ERROR_NULL_POINTER   - If the threshold_p is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - If the GPIO is not enabled.

    **\see
    *\see CyU3PGpioSetComplexConfig
    *\see CyU3PGpioComplexUpdate
    *\see CyU3PGpioComplexMeasureOnce
 */
extern CyU3PReturnStatus_t
CyU3PGpioComplexGetThreshold (
        uint8_t gpioId,                 /**< The complex GPIO to sample. */
        uint32_t *threshold_p           /**< Returns the current threshold value for the GPIO. */
        );

/** \brief Sample the GPIO timer value at an instant.

    <B>Description</B>\n
    This function is used to sample the current value of the GPIO timer at
    an instant. This can be used to estimate the elapsed time between multiple
    events processed by the firmware.

    To use this feature, the GPIO should be configured as CY_U3P_GPIO_MODE_STATIC
    and the timer should be operational.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the GPIO block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT   - If the IO is invalid.\n
    * CY_U3P_ERROR_NULL_POINTER   - If the value_p is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - If the GPIO is not enabled.\n
   * CY_U3P_ERROR_NOT_SUPPORTED  - Invalid configuration.

    **\see
    *\see CyU3PGpioSetComplexConfig
 */
extern CyU3PReturnStatus_t
CyU3PGpioComplexSampleNow (
        uint8_t gpioId,                 /**< The complex GPIO to sample. */
        uint32_t *value_p               /**< Returns the current value of the timer. */
        );

/** \brief Drive a pulse on the specified complex GPIO output immediately.

    <B>Description</B>\n
    This function is used to drive a pulse out on the specified complex GPIO,
    starting immediately. The timer value associated with the GPIO is set to 0
    at the beginning of the pulse. The pulse duration is determined by the
    threshold value and the signal is automatically driven low once the threshold
    is reached.

    This API does not wait until the end of the pulse, but returns as soon as
    the pulse has been started. The mode for the GPIO is reverted to STATIC as
    soon as the pulse is completed.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the GPIO block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT   - If the IO or threshold is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - If the IO is not enabled.\n
    * CY_U3P_ERROR_NOT_SUPPORTED  - If the GPIO is not configured correctly for this operation.

    **\see
    *\see CyU3PGpioSetComplexConfig
    *\see CyU3PGpioComplexPulse
 */
extern CyU3PReturnStatus_t
CyU3PGpioComplexPulseNow (
        uint8_t gpioId,                 /**< The complex GPIO to pulse. */
        uint32_t threshold              /**< Updates the threshold value used for setting the pulse duration. */
        );

/** \brief Configures a complex GPIO to drive an output pulse.

    <B>Description</B>\n
    This API is a delayed version of the CyU3PGpioComplexPulseNow where
    the start of the pulse output happens when the GPIO timer reaches 0.

    The pulse duration is again determined by the threshold value, and
    this function returns as soon as it has configured the GPIO to drive
    the pulse out.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the GPIO block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT   - If the IO or threshold is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - If the IO is not enabled.\n
    * CY_U3P_ERROR_NOT_SUPPORTED  - If the GPIO is not configured correctly for this operation.

    **\see
    *\see CyU3PGpioSetComplexConfig
    *\see CyU3PGpioComplexPulseNow
 */
extern CyU3PReturnStatus_t
CyU3PGpioComplexPulse (
        uint8_t gpioId,                 /**< The complex GPIO to pulse. */
        uint32_t threshold              /**< Updates the threshold value used for setting the pulse duration. */
        );

/** \brief Initiate an input signal duration measurement.

    <B>Description</B>\n
    The complex GPIOs on FX3 are capable of measuring various duration parameters
    on the input signal.

    CY_U3P_GPIO_MODE_MEASURE_LOW_ONCE  : Low period for input.\n
    CY_U3P_GPIO_MODE_MEASURE_HIGH_ONCE : High period for input.\n
    CY_U3P_GPIO_MODE_MEASURE_NEG_ONCE  : Time to negative edge.\n
    CY_U3P_GPIO_MODE_MEASURE_POS_ONCE  : Time to positive edge.\n
    CY_U3P_GPIO_MODE_MEASURE_ANY_ONCE  : Time to the next edge (transition).

    This API is used to initiate a single measurement of the desired timing parameter
    on a specified GPIO. This API can only be used when the GPIO is in the
    CY_U3P_GPIO_MODE_STATIC mode and the timer is operational.

    The API does not wait for the transition that ends the measurement, but returns
    immediately. The CyU3PGpioComplexWaitForCompletion API can be used to
    wait until the condition that ends the measurement is reached.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the GPIO block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT   - If IO or the mode specified is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - If the IO is not enabled.\n
    * CY_U3P_ERROR_NOT_SUPPORTED  - If the GPIO is not configured correctly for this operation.

    **\see
    *\see CyU3PGpioComplexMode_t
    *\see CyU3PGpioSetComplexConfig
 */
extern CyU3PReturnStatus_t
CyU3PGpioComplexMeasureOnce (
        uint8_t gpioId,                         /**< The complex GPIO to measure. */
        CyU3PGpioComplexMode_t pinMode          /**< Type of measurement to be performed. */
        );

/** \brief Wait for the completion of MeasureOnce, Pulse and PulseNow operations.

    <B>Description</B>\n
    This function checks or wait for the completion of a previously initiated
    CyU3PGpioComplexPulse, CyU3PGpioComplexPulseNow or CyU3PGpioComplexMeasureOnce
    operation.

    If the isWait option is set to false, the API only checks for completion and
    returns a TIMEOUT error if it is not complete. If the isWait option is set to
    true, the API enters a busy-wait loop that is only terminated when the GPIO
    operation is complete.

    <B>Note</B>\n
    A busy wait is used to wait for the completion of the GPIO operation. Please
    use this with care, as this can affect the operation of other blocks and
    drivers in the FX3 firmware.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED    - If the GPIO block has not been initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT   - If the Drive strength requested is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - If the IO is not enabled.\n
    * CY_U3P_ERROR_TIMEOUT        - If the operation is not complete at the time of checking.\n
    * CY_U3P_ERROR_NOT_SUPPORTED  - If the GPIO is not configured correctly for this operation.

    **\see
    *\see CyU3PGpioComplexPulse
    *\see CyU3PGpioComplexPulseNow
    *\see CyU3PGpioComplexMeasureOnce
 */
extern CyU3PReturnStatus_t
CyU3PGpioComplexWaitForCompletion (
        uint8_t gpioId,                 /**< The complex GPIO to wait for completion. */
        uint32_t *threshold_p,          /**< The current threshold value after completion. */
        CyBool_t isWait                 /**< Whether to wait or just check for completion. */
        );

/** \brief Register a callback for notification of GPIO events.

    <B>Description</B>\n
    This function registers a callback function for notification of GPIO related
    interrupts. The callback can also be registered through the CyU3PGpioInit API
    call itself.

    <B>Return value</B>\n
   * None

    **\see
    *\see CyU3PGpioIntrCb_t
    *\see CyU3PGpioInit
 */
extern void
CyU3PRegisterGpioCallBack (
        CyU3PGpioIntrCb_t gpioIntrCb    /**< Callback function pointer. */
        );

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3_UART_H_ */

/*[]*/
