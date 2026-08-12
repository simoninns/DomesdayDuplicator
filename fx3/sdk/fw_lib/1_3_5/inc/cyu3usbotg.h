/*
 ## Cypress FX3 Core Library Header (cyu3usbotg.h)
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

#ifndef _INCLUDED_CYU3USBOTG_H_
#define _INCLUDED_CYU3USBOTG_H_

#include <cyu3types.h>
#include <cyu3externcstart.h>

/** \file cyu3usbotg.h
    \brief The FX3 device supports USB 2.0 On-The-Go (OTG) functionality which
    allows the device to identify whether it should function as a USB host or
    a peripheral. This file defines the data types and APIs provided by the USB
    driver on FX3 for OTG management.

    <B>Description</B>\n
    When the FX3 USB port is in the device mode of operation, it can function
    as per USB 3.0 specification, and function at SuperSpeed, Hi-Speed or
    full speed depending upon the host capabilities.

    When configured as a USB host, the port supports USB 2.0 operations at Hi-Speed,
    full speed or low speed according to the peripheral capabilities. In host mode
    of operation, only a single peripheral can be attached at a time and HUB devices
    are not supported.

    The OTG port on FX3 is also capable of ACA charger detection based on the
    USB Battery  Charging specification 1.1 or Motorola EMU specification depending
    on the selected configuration. The peripheral type detection is based on the ID
    pin state. The OTG port supports D+ pulsing based SRP and also supports Host
    Negotiation Protocol (HNP).
 */

/**************************************************************************
 ******************************* Data types *******************************
 **************************************************************************/

/** \def CY_U3P_OTG_SRP_MAX_REPEAT_INTERVAL
    \brief  Maximum value in ms that can be used for the SRP repeat interval.
 */
#define CY_U3P_OTG_SRP_MAX_REPEAT_INTERVAL              (10000)

/** \brief OTG modes of operation.

    <B>Description</B>\n
    The FX3 device has a single USB port which can function in multiple modes.
    It can act as a USB 2.0 host or as a USB 3.0 device. It can also route the
    USB 2.0 lines (D+/D-) to the UART block for car-kit mode of operation.

    This enumeration lists the various modes of operation allowed for the
    device. The default mode of operation is CY_U3P_OTG_MODE_DEVICE_ONLY.

    **\see
    *\see CyU3POtgConfig_t
 */
typedef enum CyU3POtgMode_t
{
    CY_U3P_OTG_MODE_DEVICE_ONLY = 0,    /**< USB port acts in device only mode. The ID pin value is ignored, and
                                             charger detection is disabled. */
    CY_U3P_OTG_MODE_HOST_ONLY,          /**< USB port acts in host only mode. The ID pin value is ignored, and
                                             charger detection is disabled. */
    CY_U3P_OTG_MODE_OTG,                /**< USB port acts in OTG mode and identifies the mode of operation based
                                             on the state of the ID pin. This mode also enables charger detection
                                             based on the ID pin. */
    CY_U3P_OTG_MODE_CARKIT_PPORT,       /**< The D+/D- lines are routed to the P-Port pads PIB_CTL11 and PIB_CTL12.
                                             Charger detection is disabled. */
    CY_U3P_OTG_MODE_CARKIT_UART,        /**< The D+/D- lines are routed to the FX3 UART lines. FX3 UART will not
                                             function in this mode. Charger detection is disabled. */
    CY_U3P_OTG_NUM_MODES                /**< Number of OTG modes. */
} CyU3POtgMode_t;

/** \brief Various charger detection methods supported.

    <B>Description</B>\n
    The FX3 device can detect chargers based on standard ACA requirements
    as well as Motorola EMU (enhanced mini USB) requirements. This enumeration
    lists the various charger detect modes available on FX3.

    Charger detection is based on the state of the OTG ID pin and so is available
    only in CY_U3P_OTG_MODE_OTG mode of operation.

    **\see
    *\see CyU3POtgMode_t
    *\see CyU3POtgConfig_t
 */
typedef enum CyU3POtgChargerDetectMode_t
{
    CY_U3P_OTG_CHARGER_DETECT_ACA_MODE = 0, /**< Charger detection is based on standard ACA charger mode. This is
                                                 the default mode. */
    CY_U3P_OTG_CHARGER_DETECT_MOT_EMU,      /**< Charger detection is based on Motorola Enhanced Mini USB (EMU)
                                                 requirements. */
    CY_U3P_OTG_CHARGER_DETECT_NUM_MODES     /**< Number of charger detection modes. */
} CyU3POtgChargerDetectMode_t;

/** \brief List of OTG events.

    <B>Description</B>\n
    The enumeration lists the various OTG events that the USB driver provides to
    the user application.

    **\see
    *\see CyU3POtgEventCallback_t
    *\see CyU3POtgPeripheralType_t
 */
typedef enum CyU3POtgEvent_t
{
    CY_U3P_OTG_PERIPHERAL_CHANGE = 0,   /**< The OTG peripheral attached to FX3 has changed (removed or new device
                                             connected). The parameter to the event callback identifies the type
                                             of peripheral attached. */
    CY_U3P_OTG_SRP_DETECT,              /**< The remote device has initiated an SRP request. On receiving this
                                             request, the user is expected to turn on the VBus. */
    CY_U3P_OTG_VBUS_VALID_CHANGE        /**< Notifies that VBus state has changed. The parameter is CyTrue if
                                             VBus is active and CyFalse if VBus is not active. */
} CyU3POtgEvent_t;

/** \brief List of OTG peripheral types.

    <B>Description</B>\n
    This enumeration lists the various types of OTG peripherals that can be
    detected by the FX3.

    **\see
    *\see CyU3POtgEventCallback_t
    *\see CyU3POtgEvent_t
 */
typedef enum CyU3POtgPeripheralType_t
{
    CY_U3P_OTG_TYPE_DISABLED = 0,       /**< Device type not identified because the OTG mode detection is disabled. */
    CY_U3P_OTG_TYPE_A_CABLE,            /**< OTG A-type peripheral cable connected to FX3. FX3 is expected to behave
                                             as an OTG host. This mode is specified based on the ID pin state alone,
                                             and does not guarantee that a device has been connected. The FX3
                                             device can either enable the VBus at this stage or wait for the peripheral
                                             to initiate an SRP request. */
    CY_U3P_OTG_TYPE_B_CABLE,            /**< OTG B-type peripheral cable connected to FX3. FX3 is expected to act as
                                             an OTG device by default. The FX3 device is expected to wait for the VBus
                                             to be valid. If this does not happen, then CyU3POtgSrpStart API can be
                                             invoked for initiating SRP. */
    CY_U3P_OTG_TYPE_ACA_A_CHG,          /**< ACA RID_A_CHG charger. FX3 is expected to behave as OTG host.
                                             VBus is already available from the charger. */
    CY_U3P_OTG_TYPE_ACA_B_CHG,          /**< ACA RID_B_CHG charger. FX3 can charge and initiate SRP. The remote
                                             host is not asserting VBus or is absent. */
    CY_U3P_OTG_TYPE_ACA_C_CHG,          /**< ACA RID_C_CHG charger. FX3 device can charge and can connect but
                                             cannot initiate SRP as VBus is already asserted by the remote host. */
    CY_U3P_OTG_TYPE_MOT_MPX200,         /**< Motorola MPX.200 VPA */
    CY_U3P_OTG_TYPE_MOT_CHG,            /**< Motorola non-intelligent charger. */
    CY_U3P_OTG_TYPE_MOT_MID,            /**< Motorola mid rate charger. */
    CY_U3P_OTG_TYPE_MOT_FAST            /**< Motorola fast charger. */
} CyU3POtgPeripheralType_t;

/** \brief OTG event callback function.

    <B>Description</B>\n
    This type defines the prototype of the event callback function that is called
    by the USB driver to notify the application about OTG events.
 
    The input to the callback is dependant on the actual event. For the
    CY_U3P_OTG_PERIPHERAL_CHANGE event, this specifies the type of peripheral
    detected (CyU3POtgPeripheralType_t). For the VBus change event, it specifies
    the status of VBus.

    **\see
    *\see CyU3POtgConfig_t
    *\see CyU3POtgEvent_t
    *\see CyU3POtgPeripheralType_t
 */
typedef void (*CyU3POtgEventCallback_t) (
        CyU3POtgEvent_t event,          /**< OTG event type. */
        uint32_t        input           /**< Event specific input parameter. */
        );

/** \brief OTG configuration information.

    <B>Description</B>\n
    This structure encapsulates all of the OTG configuration information, and
    is taken in as an argument by the CyU3POtgStart function.

    **\see
    *\see CyU3POtgMode_t
    *\see CyU3POtgEventCallback_t
    *\see CyU3POtgStart
 */
typedef struct CyU3POtgConfig_t
{
    CyU3POtgMode_t              otgMode;        /**< USB port mode of operation. */
    CyU3POtgChargerDetectMode_t chargerMode;    /**< Charger detect mode. */
    CyU3POtgEventCallback_t     cb;             /**< OTG event callback function. */
} CyU3POtgConfig_t;

/**************************************************************************
 ******************************* Functions ********************************
 **************************************************************************/

/** \brief Check whether the OTG module has been started.

    <B>Description</B>\n
    This API can be used to check whether the OTG module in the FX3 firmware
    has been started.

    <B>Return value</B>\n
    * CyTrue - OTG module has been started.\n
    * CyFalse - OTG module is not running.

    **\see
    *\see CyU3POtgStart
    *\see CyU3POtgStop
 */
extern CyBool_t
CyU3POtgIsStarted (
        void);

/** \brief Identify the type of USB peripheral attached to FX3.

    <B>Description</B>\n
    This function returns the type of USB peripheral attached to FX3. This
    function can return the correct value only if there is a valid VBus or VBatt.

    <B>Return value</B>\n
    * Type of peripheral attached (CyU3POtgPeripheralType_t)

    **\see
    *\see CyU3POtgPeripheralType_t
 */
extern CyU3POtgPeripheralType_t
CyU3POtgGetPeripheralType (
        void);

/** \brief Retrieve the currently selected OTG operating mode.

    <B>Description</B>\n
    This function retrieves the active OTG operation mode.

    <B>Return value</B>\n
    * OTG mode selected at the time CyU3POtgStart call.

    **\see
    *\see CyU3POtgMode_t
    *\see CyU3POtgStart
 */
extern CyU3POtgMode_t
CyU3POtgGetMode (
        void);

/** \brief Check whether USB device (peripheral) mode operation is permitted.

    <B>Description</B>\n
    This function determines the mode of OTG operation and checks whether the FX3
    can initiate the device (peripheral) mode of operation.

    <B>Return value</B>\n
    * CyTrue - Device mode of operation is allowed.\n
    * CyFalse - Device mode of operation is not allowed.

    **\see
    *\see CyU3POtgStart
    *\see CyU3PUsbStart
 */
extern CyBool_t
CyU3POtgIsDeviceMode (
        void);

/** \brief Check whether USB host mode operation is permitted.

    <B>Description</B>\n
    This function determines the mode of OTG operation and checks whether the FX3
    can initiate host mode operation.

    <B>Return value</B>\n
    * CyTrue - Host mode of operation is allowed.\n
    * CyFalse - Host mode of operation is not allowed.

    **\see
    *\see CyU3POtgStart
    *\see CyU3PUsbHostStart
 */
extern CyBool_t
CyU3POtgIsHostMode (
        void);

/** \brief Initialize the OTG module.

    <B>Description</B>\n
    The function initializes the OTG functionality in the FX3 USB block. At this point,
    the device is ready to detect any device/host/charger connection based on the state
    of the ID pin. Once the mode of operation is detected by the USB driver and notified
    through the OTG event callback, the corresponding start API needs to be invoked.
    
    Carkit mode allows the USB 2.0 D+ and D- lines to be routed to either P-Port IOs
    or the UART TX/RX IO lines. These IOs cannot be used for the normal function when
    the carkit mode is active. Also, USB connections cannot be enabled when the carkit
    mode is active. The carkit mode can be disabled by invoking the CyU3POtgStop API.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the configuration was successful.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if the FX3 device in use does not support the OTG feature.\n
    * CY_U3P_ERROR_NULL_POINTER - if the input parameter is NULL.\n
    * CY_U3P_ERROR_ALREADY_STARTED - the module was already started.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - the CY_U3P_OTG_MODE_DEVICE_ONLY mode was already started.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - some input parameters are invalid.

    **\see
    *\see CyU3POtgConfig_t
    *\see CyU3POtgStop
    *\see CyU3POtgIsStarted
    *\see CyU3PUsbStart
    *\see CyU3PUsbHostStart
 */
extern CyU3PReturnStatus_t
CyU3POtgStart (
        CyU3POtgConfig_t *cfg           /**< OTG configuration information. */
        );

/** \brief Disable the OTG module.

    <B>Description</B>\n
    This function disables OTG mode detection on the FX3 device. This expects
    that the USB host/device mode operation has been shut down by calling the
    corresponding stop function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The API call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The module has not been started yet.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - The device or host stack is running.

    **\see
    *\see CyU3PUsbStop
    *\see CyU3PUsbHostStop
    *\see CyU3POtgStart
    *\see CyU3POtgIsStarted
 */
extern CyU3PReturnStatus_t
CyU3POtgStop (
        void);

/** \brief Initiate an SRP request.

    <B>Description</B>\n
    This API is valid only when the FX3 is functioning as a USB 2.0 device,
    and is used to start the SRP request. This is expected to cause the USB
    host to enable the VBus voltage.

    The CY_U3P_OTG_VBUS_VALID_CHANGE  event is sent when a valid VBus voltage
    is detected. The OTG module will automatically repeat the SRP request until
    the VBus voltage is valid, or until CyU3POtgStpAbort() has been called.

    The SRP repetition interval in milli-seconds is set through a parameter to
    this API.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The module is not yet started.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - Input parameter is invalid.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - Host / device stack is still active or not in the correct mode.

    **\see
    *\see CyU3POtgStart
    *\see CyU3POtgSrpAbort
 */
extern CyU3PReturnStatus_t
CyU3POtgSrpStart (
        uint32_t repeatInterval         /**< SRP repeat interval in ms. The valid range is from 500 ms to 10s. */
        );

/** \brief Abort SRP request.

    <B>Description</B>\n
    The USB driver repeats the SRP request until VBus is detected. This function
    instructs the driver to abort the periodic SRP requests.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The module is not started.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - Wrong mode or host stack is still running.

    **\see
    *\see CyU3POtgStart
    *\see CyU3POtgSrpStart
 */
extern CyU3PReturnStatus_t
CyU3POtgSrpAbort (
        void);

/** \brief Set the HNP request bit in the device status word.

    <B>Description</B>\n
    This API is valid only in device mode of operation. To initiate HNP, the
    device need to set the session request bit in the device status word. If
    USB control requests are being handled in user callback, this needs to be
    handled by the user.

    Since this is a role change request, the flag is not cleared until
    CyU3PUsbStop or CyU3POtgRequestHnp (CyFalse) is called, or there is an
    OTG peripheral change. In case of a change in the OTG peripheral attached,
    the library will automatically clear the session request flag.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.
    * CY_U3P_ERROR_NOT_SUPPORTED - Not in OTG mode.
    * CY_U3P_ERROR_NOT_STARTED - Device stack is not started.

    **\see
    *\see CyU3POtgStart
    *\see CyU3POtgHnpEnable
 */
extern CyU3PReturnStatus_t
CyU3POtgRequestHnp (
        CyBool_t isEnable               /**< Whether to set or clear the host request flag. */
        );

/** \brief Initiate a HNP role change.

    <B>Description</B>\n
    This API initiates a OTG role change. This should only be called when
    both the host and device mode USB stacks in FX3 firmware are disabled.

    If the isEnable parameter is true, the following sequence is performed:\n

    If this API is called when in 'A' session, the API assumes that the
    remote device wants to get host role and will allow subsequent
    CyU3PUsbStart call to go through. If this API is called when in 'B'
    session, the API assumes that the remote host wants to relinqush control
    and will allow subsequent CyU3PUsbHostStart call to go through. It
    should be noted that the previous configuration has to be stopped
    before invoking this call.

    If the isEnable parameter is false, the API allows the devices to revert
    to their original roles. The previous configuration has to be stopped before
    calling this API.

    The following sequence should be followed when FX3 is default USB host:\n
    1. The FX3 hosts sends down the SetFeature request for a_hnp_support
       indicating to the remote device that the host can do a role change.
       This is required for only legacy peripherals.\n
    2. Remote devices request for an HNP via the session request bit.\n
    3. Finish / abort all on-going transfers.\n
    4. FX3 host sends down the SetFeature request for b_hnp_enable.
       This is to indicate to the remote device that the host is ready
       to initiate a role change.\n
    5. The CyU3PUsbHostPortSuspend() API should be used to suspend the port.\n
    6. Once the port is suspended, FX3 should wait for the remote device
       to get disconnected (CY_U3P_USB_HOST_EVENT_DISCONNECT host event).
       If this does not happen within the spec defined time, then the
       host can either resume host mode operation or it can end the session.\n
    7. If the remote device got disconnected, FX3 should then call
       CyU3PUsbHostStop () to stop the host mode of operation.\n
    8. Enable role change by calling CyU3POtgHnpEnable (CyTrue).\n
    9. Call CyU3PUsbStart () to start the device mode stack.
 
    The following sequence should be followed when FX3 is default USB device:\n
    1. When FX3 requires a role change, it should first respond to a
       OTG GetStatus request with the session request flag set.
       When using fast enumeration mode, this can be done using
       CyU3POtgRequestHnp (CyTrue) call.\n
    2. FX3 should wait until the remote host enables HNP by issuing
       SetFeature request for b_hnp_enable.\n
    3. Once the SetFeature request is received, FX3 should wait for
       the bus to be suspended within the spec defined time. If this
       does not happen, then the device can either disconnect and end
       session or it can remain connected until the remote host resumes
       operation.\n
    4. If the bus is suspended, then disconnect from the USB bus by
       calling CyU3PConnectState (CyFalse, CyFalse).\n
    5. Now FX3 should stop the device mode stack by cleaning up all DMA
       channels and then finally calling CyU3PUsbStop ().\n
    6. FX3 should then start the role change by calling
       CyU3POtgHnpEnable (CyTrue).\n
    7. FX3 should then start the host mode operation by calling CyU3PUsbHostStart ().

    <B>Note</B>\n
    As HNP is role reversal, it has to be explicitly disabled. This only gets
    disabled if the user calls CyU3POtgStop() or CyU3POtgHnpEnable(CyFalse); or
    if the OTG peripheral is disconnected.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - Not in OTG mode.\n
    * CY_U3P_ERROR_NOT_STARTED - The module is not stated.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - Device or host stack is still active.

    **\see
    *\see CyU3POtgStart
    *\see CyU3POtgRequestHnp
    *\see CyU3POtgStop
    *\see CyU3PUsbStart
    *\see CyU3PUsbStop
    *\see CyU3PUsbHostStart
    *\see CyU3PUsbHostStop
    *\see CyU3PUsbHostPortSuspend
 */
extern CyU3PReturnStatus_t
CyU3POtgHnpEnable (
        CyBool_t isEnable               /**< Whether to initiate or reverse the HNP role change. */
        );

/** \brief Check if a role reversal mode is active or not.

    <B>Description</B>\n
    This API checks whether HNP role reversal is currently requested by the user.

    <B>Return value</B>\n
    * CyTrue - HNP role change is active.\n
    * CyFalse - HNP role change is not active.

    **\see
    *\see CyU3POtgStart
    *\see CyU3POtgEventCallback_t
 */
extern CyBool_t
CyU3POtgIsHnpEnabled (
        void);

/** \brief Check if a valid VBus is available.

    <B>Description</B>\n
    The API can be used to determine the state of the VBus. Since the USB module
    can function properly only with the VBus enabled, this can be used to determine
    when to start the device / host stacks.
   
    Notification of VBus state change can be received through the registered OTG
    event callback function as well.

    <B>Return value</B>\n
    * CyTrue - VBus is valid.\n
    * CyFalse - A valid VBus is not available.

    **\see
    *\see CyU3POtgStart
    *\see CyU3POtgEventCallback_t
 */
extern CyBool_t
CyU3POtgIsVBusValid (
        void);

#include <cyu3externcend.h>

#endif /* _INCLUDED_CYU3USBOTG_H_ */

/*[]*/
