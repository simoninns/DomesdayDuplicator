/*
 ## Cypress FX3 Core Library Header (cyu3system.h)
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

#ifndef _INCLUDED_CYU3P_SYSTEM_H_
#define _INCLUDED_CYU3P_SYSTEM_H_

#include <cyu3types.h>
#include <cyfx3_api.h>
#include <cyu3dma.h>
#include "cyu3externcstart.h"

/** \file cyu3system.h
    \brief This file defines the device initialization and configuration functions
    associated with the FX3 SDK.
 */

/**************************************************************************
 ******************************* Macros ***********************************
 **************************************************************************/

/** \def CY_U3P_SYS_PPORT_WAKEUP_SRC
    \brief Bit mask for selecting the GPIF (P-port) source to wake FX3 from suspend / standby.
 */
#define CY_U3P_SYS_PPORT_WAKEUP_SRC             (0x01)

/** \def CY_U3P_SYS_UART_WAKEUP_SRC
    \brief Bit mask for selecting the UART CTS as a source to wake FX3 from suspend / standby.
 */
#define CY_U3P_SYS_UART_WAKEUP_SRC              (0x02)

/** \def CY_U3P_SYS_USB_VBUS_WAKEUP_SRC
    \brief Bit mask for selecting the VBus signal as a source to wake FX3 from suspend / standby.
 */
#define CY_U3P_SYS_USB_VBUS_WAKEUP_SRC          (0x04)

/** \def CY_U3P_SYS_USB_BUS_ACTVTY_WAKEUP_SRC
    \brief Bit mask for selecting USB bus activity as a source to wake FX3 from suspend.
 */
#define CY_U3P_SYS_USB_BUS_ACTVTY_WAKEUP_SRC    (0x08)

/** \def CY_U3P_SYS_USB_OTGID_WAKEUP_SRC
    \brief Bit mask for selecting the USB OTG ID pin as a source to wake FX3 from suspend.
 */
#define CY_U3P_SYS_USB_OTGID_WAKEUP_SRC         (0x10)

/** \def CY_U3P_DEBUG_DMA_BUFFER_SIZE
    \brief Buffer size used on the DMA channel used to output debug log messages.
 */
#define CY_U3P_DEBUG_DMA_BUFFER_SIZE            (0x100)

/** \def CY_U3P_DEBUG_DMA_BUFFER_COUNT
    \brief Buffer count used on the DMA channel used to output debug log messages.
 */
#define CY_U3P_DEBUG_DMA_BUFFER_COUNT           (8)

/**************************************************************************
 ******************************* Data Types *******************************
 **************************************************************************/

/** \brief List of Low Performance (Serial) Peripherals supported by the
    FX3 device.

    <B>Description</B>\n
    This enumeration lists the various serial (low performance) peripherals supported
    by the FX3 device. This type is used to notify the LPP driver about the initialization
    of individual peripheral modules.

    **\see
    *\see CyU3PIsLppIOConfigured
 */
typedef enum CyU3PLppModule_t
{
    CY_U3P_LPP_I2C = 0,                 /**< I2C Master Module. */
    CY_U3P_LPP_I2S,                     /**< I2S Master Module. */
    CY_U3P_LPP_SPI,                     /**< SPI Master Module. */
    CY_U3P_LPP_UART,                    /**< UART Module. */
    CY_U3P_LPP_GPIO                     /**< GPIO Block. */
} CyU3PLppModule_t;

/** \brief Drive Strength configuration for various FX3 IOs.

    <B>Description</B>\n
    IOs on the FX3 device are grouped based on function into multiple interfaces
    (GPIF, I2C, I2S, SPI, UART). The IO drive strength for each of these groups
    can be separately selected from this list.

    **\see
    *\see CyU3PSetPportDriveStrength
    *\see CyU3PSetI2cDriveStrength
    *\see CyU3PSetGpioDriveStrength
    *\see CyU3PSetSerialIoDriveStrength
 */
typedef enum CyU3PDriveStrengthState_t
{
    CY_U3P_DS_QUARTER_STRENGTH = 0,     /**< The drive strength is one-fourth the maximum */
    CY_U3P_DS_HALF_STRENGTH,            /**< The drive strength is half the maximum */
    CY_U3P_DS_THREE_QUARTER_STRENGTH,   /**< The drive strength is three-fourth the maximum */
    CY_U3P_DS_FULL_STRENGTH             /**< The drive strength is the maximum */
} CyU3PDriveStrengthState_t;

/** \brief Clock source for a peripheral block.

    <B>Description</B>\n
    Each peripheral block on FX3 can take various clock values based on the system
    clock supplied. All clocks are derived from the SYS_CLK_PLL value which depends
    on the input clock or crystal connected to the device.
   
    The FX3 device identifies the input clock frequency based on the FSLC pins, and
    these pins should be configured correctly for proper device operation.

    The SYS_CLK_PLL value is 416 MHz if the input clock frequency is 26 MHz or 52 MHz;
    and it is 384 MHz if the input clock frequency is 19.2 MHz or 38.4 MHz. The
    SYS_CLK_PLL frequency can be changed to 403.2 MHz from 384 MHz by passing appropriate
    parameters to the CyU3PDeviceInit function.

    **\see
    *\see CyU3PSysClockConfig_t
    *\see CyU3PDeviceGetSysClkFreq
    *\see CyU3PDeviceInit
 */
typedef enum CyU3PSysClockSrc_t
{
    CY_U3P_SYS_CLK_BY_16 = 0,           /**< SYS_CLK divided by 16. */
    CY_U3P_SYS_CLK_BY_4,                /**< SYS_CLK divided by 4. */
    CY_U3P_SYS_CLK_BY_2,                /**< SYS_CLK divided by 2. */
    CY_U3P_SYS_CLK,                     /**< SYS_CLK frequency. */
    CY_U3P_NUM_CLK_SRC                  /**< Number of clock source enumerations. */
} CyU3PSysClockSrc_t;

/** \brief Clock configuration for FX3 CPU, DMA and register access.

    <B>Description</B>\n
    This structure holds information to set the clock divider for CPU, DMA and
    internal register access. The DMA and register (MMIO) clocks are derived from
    the CPU clock.
   
    There is an additional condition: DMA clock = N * MMIO clock, where N is a positive
    integer.
  
    The useStandbyClk parameter specifies whether a 32KHz clock has been supplied
    on the CLKIN_32 pin of the device. This clock is the standby clock for the device.
    If this pin is not connected, then the device internally generates a clock from
    the SYS_CLK.
  
    The setSysClk400 parameter specifies whether the FX3 device's master clock is to
    be set to a frequency greater than 400 MHz. By default, the FX3 master clock is set
    to 384 MHz when using a 19.2 MHz crystal or clock source. This frequency setting
    may lead to DMA overflow errors on the GPIF, if the GPIF is configured as 32-bit
    wide and is running at 100 MHz. Setting this parameter will switch the master
    clock frequency to 403.2 MHz during the CyU3PDeviceInit call.

    This structure is passed as parameter to CyU3PDeviceInit call.

    **\see
    *\see CyU3PSysClockSrc_t
    *\see CyU3PDeviceInit
    *\see CyU3PDeviceGetSysClkFreq
 */
typedef struct CyU3PSysClockConfig_t
{
    CyBool_t           setSysClk400;    /**< Whether the FX3 master (System) clock is to be set to a frequency
                                             greater than 400 MHz. This is required to be set to True if the
                                             GPIF is running in 32-bit mode at 100 MHz. */
    uint8_t            cpuClkDiv;       /**< CPU clock divider from clkSrc. Valid value ranges from 2 - 16. */
    uint8_t            dmaClkDiv;       /**< DMA clock divider from CPU clock. Valid value ranges from 2 - 16. */
    uint8_t            mmioClkDiv;      /**< MMIO clock divider from CPU clock. Valid value ranges from 2 - 16. */
    CyBool_t           useStandbyClk;   /**< Whether the 32 KHz standby clock is supplied. */
    CyU3PSysClockSrc_t clkSrc;          /**< Clock source for CPU clocking. */
} CyU3PSysClockConfig_t;

/** \brief FX3 API library thread information.

    <B>Description</B>\n
    This enumeration holds the thread IDs used by various FX3 API library threads.
    The thread ID is defined by the first two characters of the thread name provided
    during thread creation. For example, the DMA thread name is "01_DMA_THREAD".

    Thread IDs 0-15 are reserved by the library. Thread IDs from 16 onwards can be
    used by FX3 application. All threads created in FX3 application are expected
    to have the first two characters as the thread ID. Please note that this is a
    convention used for readability of debug logs output through the CyU3PDebugLog
    function, and is not enforced otherwise.

    **\see
    *\see CyU3PDebugEnable
    *\see CyU3PDebugDisable
    *\see CyU3PDebugLog
 */
typedef enum CyU3PSysThreadId_t
{
    CY_U3P_THREAD_ID_INT    = 0,        /**< Interrupt context. */
    CY_U3P_THREAD_ID_DMA,               /**< Library DMA module thread. */
    CY_U3P_THREAD_ID_SYSTEM,            /**< Library system module thread. */
    CY_U3P_THREAD_ID_PIB,               /**< Library p-port module thread. */
    CY_U3P_THREAD_ID_UIB,               /**< Library USB module thread. */
    CY_U3P_THREAD_ID_LPP,               /**< Library low performance peripheral module thread. */
    CY_U3P_THREAD_ID_DEBUG   = 8,       /**< Library debug module thread. */
    CY_U3P_THREAD_ID_LIB_MAX = 15       /**< Max library reserved thread ID. */
} CyU3PSysThreadId_t;

/** \brief FX3 debug logger data type.

    <B>Description</B>\n
    The structure defines the header data type sent out by the debug logger function.
    For CyU3PDebugPrint function, the preamble will be the same with msg = 0xFFFF
    and param specifying the size of the message in bytes.

    **\see
    *\see CyU3PDebugLog
    *\see CyU3PDebugPrint
 */
typedef struct CyU3PDebugLog_t
{
    uint8_t priority;                   /**< Priority of the message */
    uint8_t threadId;                   /**< Id of thread sending the message. */
    uint16_t msg;                       /**< Message Index. 0xFFFF in case of a CyU3PDebugPrint output. */
    uint32_t param;                     /**< 32 bit message parameter. */
} CyU3PDebugLog_t;

/** \cond FX3SYS_INTERNAL
 */

/**************************************************************************
 ********************** Global variable declarations **********************
 **************************************************************************/

extern uint32_t glSysClkFreq;       /* This holds the SYS_CLK frequency. This
                                       variable is updated and used by API and
                                       should not be used by the application. */
extern CyBool_t glIsICacheEnabled;  /* Internal variable holding information
                                       about I-cache state. */
extern CyBool_t glIsDCacheEnabled;  /* Internal variable holding information
                                       about D-cache state. */

extern uint8_t glDebugTraceLevel;   /* Trace level for the debug messages.
                                       This variable must be changed only using
                                       the CyU3PDebugSetTraceLevel API. */

extern uint16_t glDebugLogMask;     /* Debug log enable mask per library thread.
                                       Each bit corresponds to threadId. This
                                       variable must be changed only using the
                                       CyU3PDebugEnable call. */

/** \endcond
 */

/**************************************************************************
 *************************** Function prototypes **************************
 **************************************************************************/

/** \brief This is the entry routine for the firmware.

    <B>Description</B>\n
    This function should be set as the entry routine for the firmware by
    choosing the appropriate linker settings. This function is defined by the
    FX3 API library and sets up the runtime stacks that are required for the
    proper C/C++ language code to execute.

    Once all of the initial CPU/memory initialization is completed, this
    function will call the user define CyU3PToolChainInit function to return
    control to the user application.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PToolChainInit
 */
extern void
CyU3PFirmwareEntry (
        void);

/** \brief This is the normal entry routine provided by the tool-chain.

    <B>Description</B>\n
    This routine maps to the start-up code created by the compiler tool-chain.
    The FX3 firmware will jump to this function after completing the initial
    CPU, memory and device initialization.

    The FX3 SDK includes a version of this function for the GNU ARM tool-chain.
    This implementation only clears the BSS segment and then jumps to the main
    function.

    If the user application requires a heap (uses malloc / new), it should be
    initialized in this function.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PFirmwareEntry
 */
extern void
CyU3PToolChainInit (
        void);

/** \brief This function initializes the device.

    <B>Description</B>\n
    This function initializes the FX3 device by setting up the clocks for
    the CPU, DMA and register accesses; and then initializing the Vectored
    Interrupt Controller (VIC). This function also restores the state of the
    GPIO block if it is called as part of the resumption from low power
    standby mode.

    This function is expected to be the first one called from the main ()
    function, and should be called only once.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the call is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the parameters are invalid.

    **\see
    *\see CyU3PSysClockConfig_t
 */
extern CyU3PReturnStatus_t
CyU3PDeviceInit (
        CyU3PSysClockConfig_t *clkCfg     /**< The clock configuration for CPU, DMA and register (MMIO) access.
                                               The default configuration (CPU divider = 2, DMA divider = 2,
                                               MMIO divider = 2, setSysClk400 = CyFalse, useStandbyClk = CyTrue)
                                               is selected if the clkCfg parameter is set to NULL. */
        );

/** \brief This function initializes the caches on the ARM926 core.

    <B>Description</B>\n
    The function is expected to be called immediately after the CyU3PDeviceInit API.
    The function controls the cache handling in the system. By default (if this API is
    not called), both Instruction and Data caches are disabled.
  
    It should be noted that once D-cache is enabled, all buffers used for DMA in the
    system has to be cache line aligned. This means that all DMA buffers have to be
    32 byte aligned and 32 byte multiples. This is ensured by the CyU3PDmaBufferAlloc
    function. Any buffer allocated outside of this function has to follow the 32 byte
    alignment / multiple rule. This rule is also applicable for all DMA buffer pointers
    passed to library APIs including the USB descriptor buffers assigned using
    CyU3PUsbSetDesc, data pointers provided to CyU3PUsbSendEP0Data, CyU3PUsbGetEP0Data,
    CyU3PUsbHostSendSetupRqt etc.
  
    The isDmaHandleDCache determines whether the DMA APIs perform cache cleans
    and cache flushes internally. If this is CyFalse, then all cache cleans and
    flushes for buffers used with DMA APIs have to be done explicitly by the user.
    If this is CyTrue, then the DMA APIs will clean the cache lines for buffers used
    to send out data from the device and will flush the cache lines for buffers used
    to receive data.

    It is recommended that the isDmaHandleDCache parameter be set to CyTrue if the
    Data cache is being enabled.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the call is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if isDmaHandleDCache is set to true without enabling the D-cache.

    **\see
    *\see CyU3PDeviceInit
 */
extern CyU3PReturnStatus_t
CyU3PDeviceCacheControl (
        CyBool_t isICacheEnable,        /**< Whether to enable the I-cache. */
        CyBool_t isDCacheEnable,        /**< Whether to enable the D-cache. */
        CyBool_t isDmaHandleDCache      /**< Whether the DMA APIs should internally take care of cache handling. */
        );

/** \brief This is the FX3 application definition function.

    <B>Description</B>\n
    This function is called by the FX3 RTOS once all of the OS and driver initialization
    is completed. The RTOS objects and threads required by the application can be created
    in this function.

    This function needs to be defined by the FX3 application firmware. At-least one thread
    must be created for meaningful operation. This function should not be explicitly called.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PKernelEntry
 */
extern void
CyFxApplicationDefine (
        void);

/** \brief This function returns the API library version.

    <B>Description</B>\n
    This function is used to identify the version information for the FX3 API
    library. It is expected that the cyfxversion.h file corresponding to the
    actual library version will be used by the application.

    Valid pointers need to be passed for each of the return parameters, only if
    the corresponding return value is required. Otherwise, NULL can be passed.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the call is successful.
 */
extern CyU3PReturnStatus_t
CyU3PSysGetApiVersion (
        uint16_t *majorVersion,      /**< Major version number for the release */
        uint16_t *minorVersion,      /**< Minor version number for the release */
        uint16_t *patchNumer,        /**< Patch version for the release */
        uint16_t *buildNumer         /**< The build number for the release */
        );

/** \brief This function returns the current SYS_CLK frequency.

    <B>Description</B>\n
    This function can be used to identify the current SYS_CLK frequency on
    the FX3 device. This should only be called after the CyU3PDeviceInit API
    has been called.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the call succeeds.\n
    * CY_U3P_ERROR_NULL_POINTER - if NULL pointer was passed as parameter.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the device is not initialized.

    **\see
    *\see CyU3PSysClockConfig_t
 */
extern CyU3PReturnStatus_t
CyU3PDeviceGetSysClkFreq (
        uint32_t *freq                  /**< Pointer to return the current SYS_CLK frequency. */
        );

/** \brief This function resets the FX3 device.

    <B>Description</B>\n
    This function is used to reset the FX3 device as a whole. Only a whole
    system reset is supported, as a CPU only reset will leave hardware blocks
    within the device in an inconsistent state.
   
    If the warm boot option is selected, the firmware in the FX3 RAM shall be
    maintained; otherwise it shall be discarded and freshly loaded. If the warm
    boot option is used, the firmware should be capable of initializing any
    global variables explicitly.

    <B>Note</B>\n
    It is recommended that all of the FX3 blocks are de-initialized before
    triggering a warm boot operation.

    <B>Return value</B>\n
    * None as this function does not return.
 */
extern void
CyU3PDeviceReset (
        CyBool_t isWarmReset            /**< Whether this should be a warm reset or a cold reset. */
        );


/** \brief Verify FX3 device is ready to enter in low power suspend mode.

    <B>Description</B>\n
    The function will verify whether FX3 device is ready for putting into low power
    suspend mode, as well as prepare device low power state entry plan.
   
    This function needs to be called prior to calling CyU3PSysEnterSuspendMode, for making 
    device enter into suspend mode.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the call was successful.\n
    * CY_U3P_ERROR_INVALID_CALLER - if called from interrupt context.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the wakeup sources specified are invalid.\n
    * CY_U3P_ERROR_ABORTED - if one of the wakeup source is already triggering.

    **\see
    *\see CyU3PSysEnterSuspendMode
 */
extern CyU3PReturnStatus_t
CyU3PSysCheckSuspendParams (
        uint16_t wakeupFlags,
        uint16_t polarity
        );

/** \brief Wait for P-Port to receive last mailbox response from FX3 device prior
     to entering in suspend mode.

    <B>Description</B>\n
    This function will make FX3 device to wait for 100 mseconds, after sending mailbox
    response to P-Port indicating that system is entering into suspend mode.
   
    This function needs to be called prior to calling CyU3PSysEnterSuspendMode, if any external
    processor is waiting for enter suspend mailbox response.

    **\see
    *\see CyU3PSysEnterSuspendMode
 */
extern void
CyU3PSysSendEnterSuspendStatus (
        void
        );

/** \brief Places the FX3 device in low power suspend mode.

    <B>Description</B>\n
    The function can be called only after initializing the device completely.
    The device will enter into the suspended mode until any of the wakeup sources
    are triggered. This function does not return until the device has already
    resumed normal operation.
   
    The CPU stops running and the device enters a low power state. Any combination
    of CY_U3P_SYS_PPORT_WAKEUP_SRC_EN, CY_U3P_SYS_USB_WAKEUP_SRC_EN and
    CY_U3P_SYS_UART_WAKEUP_SRC_EN can be used as the wakeup trigger.

    This function does not affect the state of the USB PHY on the FX3 device. If
    the USB PHY is to be powered off while in the low power mode, the caller should
    explicitly call the CyU3PConnectState API to do this.

    <B>Note</B>\n
    If the watchdog is enabled with an external 32 KHz clock input to the FX3
    device, the watchdog timer will still be operational when the FX3 device is
    in the suspend mode with clocks disabled. In this case, the FX3 device will
    get reset if the device stays in suspend mode for longer than the reset
    period specified. If the 32 KHz clock input is not provided, or the
    useStandbyClk parameter to CyU3PDeviceInit is set to CyFalse, the watchdog
    timer will be off when the device is in suspend mode. In such cases, the
    device will not get reset while it is in the suspend mode.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the call was successful.\n
    * CY_U3P_ERROR_INVALID_CALLER - if called from interrupt context.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the wakeup sources specified are invalid.\n
    * CY_U3P_ERROR_ABORTED - if one of the wakeup source is already triggering.

    **\see
    *\see CyU3PSysEnterStandbyMode
    *\see CyU3PDeviceInit
    *\see CyU3PSysWatchDogConfigure
 */
extern CyU3PReturnStatus_t
CyU3PSysEnterSuspendMode (
        uint16_t wakeupFlags,           /**< Bit mask representing the wakeup sources that are allowed to
                                             bring FX3 out of suspend mode. */
        uint16_t polarity,              /**< Polarity for each of the Wakeup Sources. This field is valid only for the
                                             CY_U3P_SYS_UART_WAKEUP_SRC and CY_U3P_SYS_USB_VBUS_WAKEUP_SRC wakeup
                                             sources.\n
                                             0 - Wakeup when the corresponding source goes low.\n
                                             1 - Wakeup when the corresponding source goes high. */
        uint16_t *wakeupSource          /**< Output parameter indicating the source responsible for waking
                                             the FX3 from the Suspend mode. */
        );

/** \brief Verify FX3 device is ready to enter in low power standby mode.

    <B>Description</B>\n
    The function will verify whether FX3 device is ready for putting into low power
    standby mode, by verifying whether wakeup sources configured are correct and all
    the peripheral blocks have been disabled.
   
    This function can be called to check whether the device can be placed in the
    low power standby state. If it is not called by the user, this will be called
    internally within the CyU3PSysEnterSuspendMode API.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the call was successful.\n
    * CY_U3P_ERROR_INVALID_CALLER - if called from interrupt context.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the wakeup sources specified are invalid.\n
    * CY_U3P_ERROR_ABORTED - if one of the wakeup source is already triggering.

    **\see
    *\see CyU3PSysEnterSuspendMode
 */
extern CyU3PReturnStatus_t
CyU3PSysCheckStandbyParam (
        uint16_t wakeupFlags,
        uint16_t polarity,
        uint8_t  *bkp_buff_p);

/** \brief Places the FX3 device in low power standby mode.

    <B>Description</B>\n
    This function places the FX3 device in low power standby mode where the power consumption on the
    device is lowest. As the power to all the device blocks is removed when in standby mode; this
    function can only be called after shutting down (de-initializing) all FX3 blocks such as USB, GPIF,
    UART, I2C etc. Only the GPIO block can be left on, and its state will be restored when the device
    wakes up.

    On wakeup from standby, the device starts firmware execution from the original entry point. In this
    sense, the entry into and wakeup from standby mode is similar to a warm reset being applied to the
    FX3 device.

    Only P-Port, UART CTS and VBus based wakeup sources are supported to trigger a wake up of the FX3 device from
    standby mode. While the FX3 System RAM retains its content, the TCM blocks lose power while the device
    is in standby. This API backs up the content of the I-TCM region into a user specified buffer location
    before entering standby mode. The I-TCM contents are then automatically restored during re-initialization
    of the FX3 device.

    <B>Return value</B>\n
    * No return if the device is actually placed in standby mode.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the wakeup sources, polarity or buffer pointer provided is invalid.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE if this API is called while any of the FX3 blocks is still active.\n
    * CY_U3P_ERROR_STANDBY_FAILED if the specified wakeup sources are already active.

    **\see
    *\see CyU3PSysEnterSuspendMode
 */
extern CyU3PReturnStatus_t
CyU3PSysEnterStandbyMode (
        uint16_t  wakeupFlags,          /**< List of selected wakeup sources from standby. */
        uint16_t  polarity,             /**< Polarity of the wakeup source which causes the device to wakeup. */
        uint8_t  *bkp_buff_p            /**< Pointer to buffer where the I-TCM content can be backed up.
                                             Should be located in the SYSMEM and be able to hold 18 KB of data. */
        );

/** \brief Configure the watchdog reset control.

    <B>Description</B>\n
    The FX3 device implements a watchdog timer that can be used to reset the
    device when it is not responsive. This function is used to enable the watchdog
    feature and to set the period for this watchdog timer.

    <B>Note</B>\n
    If the watchdog is enabled with an external 32 KHz clock input to the FX3
    device, the watchdog timer will still be operational when the FX3 device is
    in the suspend mode with clocks disabled. In this case, the FX3 device will
    get reset if the device stays in suspend mode for longer than the reset
    period specified. If the 32 KHz clock input is not provided, or the
    useStandbyClk parameter to CyU3PDeviceInit is set to CyFalse, the watchdog
    timer will be off when the device is in suspend mode. In such cases, the
    device will not get reset while it is in the suspend mode.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PSysWatchDogClear
    *\see CyU3PDeviceInit
    *\see CyU3PSysEnterSuspendMode
 */
extern void
CyU3PSysWatchDogConfigure (
        CyBool_t enable,                /**< Whether the watch dog should be enabled. */
        uint32_t period                 /**< Period in milliseconds. Is valid only for enable calls. */
        );

/** \brief Clear the watchdog timer to prevent a device reset.

    <B>Description</B>\n
    This function is used to clear the watchdog timer so as to prevent it from
    resetting the FX3 device. This function should be called more frequently than
    the specified watchdog timer period to avoid unexpected resets.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PSysWatchDogConfigure
 */
extern void
CyU3PSysWatchDogClear (
        void);

/** \brief Configures the IO matrix for the device.

    <B>Description</B>\n
    The FX3/FX3S devices have upto 61 IO pins that can be configured to function in a
    variety of modes. The functional mode for each of these pins (or groups of
    pins) should be set based on the desired system level functionality.

    The processor port (P-port) of the device can be configured as a GPIF
    interface. The mode selection for this interface should be based on the means
    of connecting the external device / processor. Some pins may not be used depending
    upon the configuration used. The free pins can be used as GPIOs.

    If a 16 bit GPIF interface is used, then the DQ[15:0], CTL[4:0] and PMODE pins
    are reserved for their default usage. CTL[12:5] are not locked and can be
    configured as GPIO. If any of the CTL[12:5] lines are used for GPIF interface
    they should not be configured as GPIOs. Similarly if some lines of CTL[4:0]
    are not used for GPIF they can be overridden to be GPIOs using the
    CyU3PDeviceGpioOverride call.

    If 32 bit GPIF interface is used, all of DQ[31:0] is reserved for the GPIF interface.

    Some devices in the family, such as the CYUSB3011 and CYUSB3013 devices, as well as
    the FX3S device; support only a 16 bit wide GPIF interface. The isDQ32Bit field should
    be set to false when using these devices.

    The FX3S device can support upto two storage ports that can be connected to SD/MMC/SDIO
    peripherals. The first storage port (S0) is always available and can be configured in
    a 4 bit SD mode or in a 8 bit MMC mode. The second storage port (S1) shares pins with the
    serial peripheral interfaces (UART, SPI and I2S).

    The serial peripheral interfaces have various configurations that can be used
    depending on the type of interfaces used. The default mode of operation has
    all the low performance (serial) peripherals enabled. I2C lines are not
    multiplexed and are available in all configurations except when used as GPIOs.
    If a peripheral is marked as not used (CyFalse), then those IO lines can be
    configured as GPIO.

    IOs that are not configured for peripheral interfaces can be used as GPIOs. This
    selection has to be explicitly made. Otherwise these lines will be configured as
    per their default function.

    The GPIOs available are further classified as simple (set / get a value or receive
    a GPIO interrupt); or as complex (PWM, timer, counter etc). The selection of this
    is made via four 32-bit bitmasks where each IO is represented with (1 << IO number).

    Please refer to the device data sheet for the complete list of supported modes on
    each of the device interfaces.

    It is recommended that this function be called immediately after the CyU3PDeviceInit
    call from the main () function; and that the IO matrix not be dynamically changed.
    However, this API can be invoked when the peripherals affected are disabled.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the IO configuration is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if some configuration value is invalid.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if the part in use does not support any of the selected features.

     **\see
     *\see CyU3PIoMatrixConfig_t
     *\see CyU3PDeviceGpioOverride
     *\see CyU3PDeviceGpioRestore
 */
extern CyU3PReturnStatus_t
CyU3PDeviceConfigureIOMatrix (
        CyU3PIoMatrixConfig_t *cfg_p    /**< Pointer to Configuration parameters. */
        );

/** \brief This function is used to override an IO as a GPIO.

    <B>Description</B>\n
    This is an override mechanism and can be used to enable any IO line as
    simple / complex GPIO. This should be done with caution as a wrong setting
    can cause damage to hardware.

    The API is expected to be invoked before the corresponding peripheral module
    is enabled. The specific GPIO configuration has to be still done after this call.

    The CyU3PDeviceConfigureIOMatrix API checks for validity of the configuration,
    whereas this API does not. Use this API with great caution.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the GPIO specified is invalid.

    **\see
    *\see CyU3PDeviceConfigureIOMatrix
    *\see CyU3PDeviceGpioRestore
 */
extern CyU3PReturnStatus_t
CyU3PDeviceGpioOverride (
        uint8_t gpioId,         /**< The IO to be converted to GPIO. */
        CyBool_t isSimple       /**< CyTrue: simple GPIO, CyFalse: complex GPIO */
        );

/** \brief This function is used to restore IO to normal mode.

    <B>Description</B>\n
    IOs that are overridden to be GPIO can be restored to normal mode of
    operation by invoking this API.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the GPIO specified is invalid.

    **\see
    *\see CyU3PDeviceGpioOverride
    *\see CyU3PDeviceConfigureIOMatrix
 */
extern CyU3PReturnStatus_t
CyU3PDeviceGpioRestore (
        uint8_t gpioId          /**< The GPIO to be restored. */
        );

/** \brief Set the IO drive strength for UART, SPI and I2S interfaces.

    <B>Description</B>\n
    This function is used to update the drive strength for the UART, SPI and
    I2S interface signals. This is set to CY_U3P_DS_THREE_QUARTER_STRENGTH by
    default.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS            - If the operation is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - If the Drive strength requested is invalid.

    **\see
    *\see CyU3PDriveStrengthState_t
    *\see CyU3PSetI2cDriveStrength
 */
extern CyU3PReturnStatus_t
CyU3PSetSerialIoDriveStrength (
        CyU3PDriveStrengthState_t driveStrength         /**< Desired serial IO drive strength. */
        );

/** \brief Check whether a specific pin has been selected as a simple GPIO.

    <B>Description</B>\n
    This function checks whether a specific FX3 pin has been selected to function as
    a simple GPIO.

    <B>Return value</B>\n
    * CyTrue if the pin is selected as a simple GPIO.\n
    * CyFalse is the pin is not selected as a simple GPIO.

    **\see
    *\see CyU3PDeviceConfigureIOMatrix
    *\see CyU3PIsGpioComplexIOConfigured
 */
extern CyBool_t
CyU3PIsGpioSimpleIOConfigured (
        uint32_t gpioId                                 /**< Pin number to be queried. */
        );

/** \brief Check whether a specific pin has been selected as a complex GPIO.

    <B>Description</B>\n
    This function checks whether a specific FX3 pin has been selected to function as
    a complex GPIO.

    <B>Return value</B>\n
    * CyTrue if the pin is selected as a complex GPIO.\n
    * CyFalse is the pin is not selected as a complex GPIO.

    **\see
    *\see CyU3PDeviceConfigureIOMatrix
    *\see CyU3PIsGpioSimpleIOConfigured
 */
extern CyBool_t
CyU3PIsGpioComplexIOConfigured (
        uint32_t gpioId                                 /**< Pin number to be queried. */
        );

/** \brief Check whether a specified GPIO ID is valid on the current FX3 part.

    <B>Description</B>\n
    Different parts in the FX3 family support different numbers of GPIOs. This
    function is used to check whether a specific GPIO number is valid on the current
    part.

    <B>Return value</B>\n
    * CyTrue if the GPIO number is valid and usable.\n
    * CyFalse if the GPIO number is invalid.
 */
extern CyBool_t
CyU3PIsGpioValid (
        uint8_t gpioId                                  /**< GPIO number to be checked for validity. */
        );

/** \brief Check whether a specific serial peripheral has been enabled in the IO Matrix.

    <B>Description</B>\n
    This function checks whether a specific serial peripheral interface has been enabled
    in the IO matrix.

    <B>Return value</B>\n
    * CyTrue if the specified LPP block is enabled.\n
    * CyFalse if the specified LPP block is not enabled.

    **\see
    *\see CyU3PDeviceConfigureIOMatrix
    *\see CyU3PLppModule_t
 */
extern CyBool_t
CyU3PIsLppIOConfigured (
        CyU3PLppModule_t lppModule                      /**< Serial interface to be queried. */
        );

/** \brief This function is used to get the part number of the FX3 device in use.

    <B>Description</B>\n
    The EZ-USB FX3 family has multiple parts which support various sets of features.
    This function can be used to query the part number of the current device so as to
    check whether specific functionality is supported or not.

    <B>Return value</B>\n
    * Part number of the FX3 device in use.

    **\see
    *\see CyU3PPartNumber_t
 */
extern CyU3PPartNumber_t
CyU3PDeviceGetPartNumber (
        void);

/** \brief Initialize the firmware logging functionality.

    <B>Description</B>\n
    This function initializes the firmware logging functionality and creates a DMA
    channel to send the log traces out through the selected DMA socket.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS  - when successfully initialized.\n
    * CY_U3P_ERROR_ALREADY_STARTED - when the logger is already initialized.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - when bad socket ID is passed in as parameter.\n
    * Other DMA specific error codes are also returned.

    **\see
    *\see CyU3PDebugDeInit
    *\see CyU3PDebugSetTimeout
 */
extern CyU3PReturnStatus_t
CyU3PDebugInit (
        CyU3PDmaSocketId_t destSckId,           /**< Socket through which the logs are to be output. */
        uint8_t            traceLevel           /**< Priority level beyond which logs should be output. */
        );

/** \brief Set the timeout period for a debug log function.

    <B>Description</B>\n
    When multiple log messages are output back-to-back through the CyU3PDebugPrint or CyU3PDebugLog
    functions, it is possible that the log function is blocked by non-availability of data buffers.
    This normally only gets cleared up when all previously logged messages have been sent out of the
    FX3 device (printed through UART or read out from other socket). The default behavior of the
    Debug API is to block forever until a buffer is available.

    This function is used to set a limit on the amount of time the CyU3PDebugPrint, CyU3PDebugLog or
    CyU3PDebugFlush API will wait for the data to be drained. If a timeout occurs, all current debug
    messages in the pipe will be cleared; so that new messages can be output.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS  - if parameters are set properly.\n
    * CY_U3P_ERROR_NOT_STARTED - if the debug module has not been initialized.

    **\see
    *\see CyU3PDebugPrint
    *\see CyU3PDebugLog
    *\see CyU3PDebugFlush
 */
extern CyU3PReturnStatus_t
CyU3PDebugSetTimeout (
        uint32_t timeout                        /**< Timeout duration. Can be CYU3P_WAIT_FOREVER or CYU3P_NO_WAIT
                                                     as well. */
        );

/** \brief De-initialize the logging function.

    <B>Description</B>\n
    This function de-initializes the firmware logging function and destroys the
    DMA channel created to output the logs.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - When successful.\n
    * CY_U3P_ERROR_NOT_STARTED - When the logger is not started.

    **\see
    *\see CyU3PDebugInit
 */
extern CyU3PReturnStatus_t
CyU3PDebugDeInit (
        void);

/** \brief Initializes the SYS_MEM based logger facility.

    <B>Description</B>\n
    This function cannot be used when the CyU3PDebugInit is invoked. The SYS_MEM
    logging is faster as the writes are done directly to the system memory. But
    the limitation is that only CyU3PDebugLog function can be used. The log buffer
    can be cleared by calling CyU3PDebugLogClear.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when successfully initialized.\n
    * CY_U3P_ERROR_ALREADY_STARTED - The logger is already started.\n
    * CY_U3P_ERROR_NULL_POINTER - If NULL buffer pointer is passed as argument.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - If any of the arguments passed is invalid.

    **\see
    *\see CyU3PDebugSysMemDeInit
    *\see CyU3PDebugLog
    *\see CyU3PDebugLogClear
 */
extern CyU3PReturnStatus_t
CyU3PDebugSysMemInit (
        uint8_t *buffer,        /**< The buffer memory to be used for writing the log. */
        uint16_t size,          /**< The size of memory available for logging. */
        CyBool_t isWrapAround,  /**< CyTrue - wrap up and start logging from beginning;
                                     CyFalse - Stop logging on reaching the last memory address. */
        uint8_t traceLevel      /**< Priority level beyond which logs should be output. */
        );

/** \brief Disables the SYS_MEM logger.

    <B>Description</B>\n
    The function de-initializes the SYS_MEM logger. The normal logger can now be
    initialized using CyU3PDebugInit call.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when successfully initialized.\n
    * CY_U3P_ERROR_NOT_STARTED - when the logger is not yet started.

    **\see
    *\see CyU3PDebugSysMemInit
 */
extern CyU3PReturnStatus_t
CyU3PDebugSysMemDeInit (
        void);

/** \brief Inhibits the preamble bytes preceding a verbose log message.

    <B>Description</B>\n
    The CyU3PDebugPrint function internally sends 8 byte preamble data preceding
    the actual message. This preamble data can be used to identify the source and
    context information for the message.

    This function can be called to enable and disable sending of the preamble data.
    This is useful in the case where the debug data is visually interpreted (and
    not decoded by a tool).

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PDebugPrint
*/
extern void
CyU3PDebugPreamble (
        CyBool_t sendPreamble    /**< CyFalse: Do not send preamble; CyTrue: Send preamble. */
        );

/** \brief Free form message logging function.

    <B>Description</B>\n
    This function can be used by the application code to log free form message
    strings, and causes the output message to be written to the UART immediately.

    The function supports a subset of the output conversion specifications
    supported by the printf function. The 'c', 'd', 'u', 'x' and 's' conversion
    specifications are supported. There is no support for float or double formats,
    or for flags, precision or type modifiers.

    <B>Note</B>\n
    If the full functionality supported by printf such as all conversion specifications,
    flags, precision, type modifiers etc. are required; please use the standard C library
    functions to print the formatted message into a string, and then use this function
    to send the string out through the UART port.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the Debug print is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - if the debug module has not been initialized.\n
    * CY_U3P_ERROR_INVALID_CALLER - if called from an interrupt.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the total length of the formatted string exceeds the debug buffer size.

    **\see
    *\see CyU3PDebugInit
    *\see CyU3PDebugPreamble
 */
extern CyU3PReturnStatus_t
CyU3PDebugPrint (
        uint8_t priority,               /**< Priority level for this message. */
        char   *message,                /**< Format specifier string. */
        ...                             /**< Variable argument list. */
        );

/** \brief Miniature version of snprintf routine.

    <B>Description</B>\n
    This is a mini version of the snprintf routine, which prints formatted debug information
    into a user provided buffer. The constraints mentioned for the CyU3PDebugPrint function
    apply for this function as well.

    This function only supports the 'c', 'd', 'u', 'x' and 's' conversion specifications.
    It also does not support precision or type modifiers.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the print to buffer is successful.
    * CY_U3P_ERROR_BAD_ARGUMENT - if the total length of the formatted string exceeds maxLength.

    **\see
    *\see CyU3PDebugPrint
 */
extern CyU3PReturnStatus_t
CyU3PDebugStringPrint (
        uint8_t *buffer,                /**< Buffer into which the data is to be printed. */
        uint16_t maxLength,             /**< Buffer size. */
        char    *fmt,                   /**< Format specifier string. */
        ...                             /**< Variable argument list. */
        );

/** \brief Log a codified message.

    <B>Description</B>\n
    This function is used to output a codified log message which contains a two
    byte message ID and a four byte parameter. The message ID is expected to be
    in the range 0x0000 to 0xFFFE.

    The log messages are written to a log buffer. If CyU3PDebugSysMemInit has been
    used, the logs can be cleared using the CyU3PDebugLogClear function.
    
    If CyU3PDebugInit has been used, the log buffer will be written out to the
    debug socket when it is full. The CyU3PDebugLogFlush can be used to flush
    the contents of the log buffer to the debug socket. The CyU3PDebugLogClear can be
    used to drop the current contents of the log buffer.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the Debug log is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - if the debug module has not been initialized.\n
    * CY_U3P_ERROR_FAILURE - if the logging fails.

    **\see
    *\see CyU3PDebugInit
    *\see CyU3PDebugSysMemInit
    *\see CyU3PDebugLogFlush
    *\see CyU3PDebugLogClear
 */
extern CyU3PReturnStatus_t
CyU3PDebugLog (
        uint8_t priority,               /**< Priority level for the message. */
        uint16_t message,               /**< Message ID for the message. */
        uint32_t parameter              /**< Parameter associated with the message. */
        );

/** \brief Flush any pending logs out of the internal buffers.

    <B>Description</B>\n
    All log messages are collected into internal memory buffers on the
    FX3 device, and then sent out to the target when the buffer is full
    or when a verbose message is committed.
    
    This function is used to force the logger to send out any pending log
    messages to the target and flush its internal buffers.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the Debug log flush is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - if the debug module has not been initialized.\n
    * CY_U3P_ERROR_INVALID_CALLER - if called from an interrupt.

    **\see
    *\see CyU3PDebugLog
    *\see CyU3PDebugLogClear
 */
extern CyU3PReturnStatus_t
CyU3PDebugLogFlush (
        void);

/** \brief Drop any pending logs in the internal buffers.

    <B>Description</B>\n
    This function is used to ask the logger to drop any pending logs in its
    internal buffers.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the Debug log flush is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - if the debug module has not been initialized.\n
    * CY_U3P_ERROR_INVALID_CALLER - if called from an interrupt.

    **\see
    *\see CyU3PDebugLog
    *\see CyU3PDebugLogFlush
 */
extern CyU3PReturnStatus_t
CyU3PDebugLogClear (
        void);

/** \brief This function sets the priority threshold above which debug traces will be logged.

    <B>Description</B>\n
    The logger can suppress log messages that have a priority level lower than a user
    specified threshold. This function is used to set the priority threshold below
    which logs will be suppressed.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PDebugPrint
    *\see CyU3PDebugLog
 */
extern void
CyU3PDebugSetTraceLevel (
        uint8_t level                   /**< Lowest debug trace priority level that is enabled. */
        );

/** \brief Enable log messages from specified threads.

    <B>Description</B>\n
    Thread ID of 0 means interrupt context. Only threadIds 1 - 15, and interrupt
    context can be controlled by this API call. This call is intended for only
    controlling logs from library threads. By default, logs from all library threads
    are disabled.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PDebugDisable
 */
extern void
CyU3PDebugEnable (
        uint16_t threadMask             /**< ThreadID mask whose logs are to be enabled. 1 means enabled. */
        );

/** \brief Disable log messages from all library threads.

    <B>Description</B>\n
    The API returns the previous threadMask set, so that it can be used for re-enabling the logs.

    <B>Return value</B>\n
    * The previous threadMask value.

    **\see
    *\see CyU3PDebugEnable
 */
extern uint16_t
CyU3PDebugDisable (
        void);

/** \brief Internal function used to register driver threads.

    <B>Description</B>\n
    This API is used by SDK internal drivers to register all the driver threads.
    Please do not call this API from user code.
 */
extern void
CyU3PSystemRegisterDriver (
        CyU3PThread *thread_p);

/** \brief Get the average CPU load from start of firmware operation.

    <B>Description</B>\n
    This API returns the average CPU load from start of firmware operation in terms
    of percentage points. The load is calculated using profiling information provided
    by the ThreadX RTOS, and is available only in profiling enabled builds of the
    firmware.

    <B>Return value</B>\n
    * CPU load in percentage, if profiling is enabled.
    * 0 if profiling is disabled.
 */
extern uint32_t
CyU3PDeviceGetCpuLoad (
        void);

/** \brief Get the CPU loading due to a given thread.

    <B>Description</B>\n
    This API returns the average CPU loading due to a given thread, from start of
    firmware operation. The load is calculated using profiling information provided
    by the ThreadX RTOS, and is available only in profiling enabled builds of the
    firmware.

    <B>Return value</B>\n
    * Thread CPU load in percentage, if profiling is enabled.
    * 0 if profiling is disabled.
 */
extern uint32_t
CyU3PDeviceGetThreadLoad (
        CyU3PThread *thread_p);

/** \brief Get the CPU loading due to SDK internal driver threads.

    <B>Description</B>\n
    This API returns the average CPU load due to all of the FX3 SDK internal driver
    threads. The load is calculated using profiling information provided
    by the ThreadX RTOS, and is available only in profiling enabled builds of the
    firmware.

    <B>Return value</B>\n
    * Driver CPU load in percentage, if profiling is enabled.
    * 0 if profiling is disabled.
 */
extern uint32_t
CyU3PDeviceGetDriverLoad (
        void);

/** \brief Jump to the specified instruction address.

    <B>Description</B>\n
    This function transfers control to the specified instruction address, without
    updating the runtime stack or the link register. This is intended to be used
    as the transition to a different application firmware binary. It is expected that
    the user would have disabled all interrupts and shut down the device blocks
    prior to calling this function.
 */
extern void
CyU3PJumpToAddress (
        uint32_t address                /**< Address to jump to. */
        );

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3P_SYSTEM_H_ */

/*[]*/

