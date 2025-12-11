/*
 ## Cypress FX3 Core Library Header (cyu3usbhost.h)
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

#ifndef _INCLUDED_CYU3P_USB_HOST_H_
#define _INCLUDED_CYU3P_USB_HOST_H_

#include <cyu3types.h>
#include <cyu3usbconst.h>
#include <cyu3externcstart.h>

/** \file cyu3usbhost.h
    \brief The FX3 device supports programmable USB host implementation for a single
    USB host port at USB-HS, USB-FS and USB-LS speeds. The control pipe as well as
    the data pipes to be used can be configured through a set of USB host mode APIs.
    The USB host mode APIs also provide the capability to manage the host port.
 */

/**************************************************************************
 ******************************* Data Types *******************************
 **************************************************************************/

/** \brief Mode of data transfer.

    <B>Description</B>\n
    This type lists the various data transfer modes for USB endpoints on the
    FX3 host. The NORMAL mode is to be used for all endpoints other than the
    control endpoint. For the control (EP0) endpoint, the mode needs to be
    set on a per-transfer basis depending on the type of the control request.

    **\see
    *\see CyU3PUsbHostEpSetXfer
 */
typedef enum CyU3PUsbHostEpXferType_t
{
    CY_U3P_USB_HOST_EPXFER_NORMAL = 0,          /**< Normal transfer. All non-EP0 tranfers. */
    CY_U3P_USB_HOST_EPXFER_SETUP_OUT_DATA,      /**< EP0 setup packet with OUT data phase. */
    CY_U3P_USB_HOST_EPXFER_SETUP_IN_DATA,       /**< EP0 setup packet with IN data phase. */
    CY_U3P_USB_HOST_EPXFER_SETUP_NO_DATA        /**< EP0 setup packet with no data phase. */

} CyU3PUsbHostEpXferType_t;

/** \brief Speed of operation for the USB host port.

    <B>Description</B>\n
    The USB host on the FX3 device supports Low, Full and Hi-Speed
    operation. This type defines named constants for each of these
    connection types.

    **\see
    *\see CyU3PUsbHostGetPortStatus
 */
typedef enum CyU3PUsbHostOpSpeed_t
{
    CY_U3P_USB_HOST_LOW_SPEED = 0,              /**< Host port is operating in low speed mode. */
    CY_U3P_USB_HOST_FULL_SPEED,                 /**< Host port is operating in full speed mode. */
    CY_U3P_USB_HOST_HIGH_SPEED                  /**< Host port is operating in high speed mode. */
} CyU3PUsbHostOpSpeed_t;

/** \brief List of USB host mode events.

    <B>Description</B>\n
    This type lists the possible USB host mode related events that are reported
    to the application via the event callback function.

    **\see
    *\see CyU3PUsbHostEventCb_t
 */
typedef enum CyU3PUsbHostEventType_t
{
    CY_U3P_USB_HOST_EVENT_CONNECT = 0,          /**< USB Connect event. */
    CY_U3P_USB_HOST_EVENT_DISCONNECT            /**< USB Disconnect event. */
} CyU3PUsbHostEventType_t;


/** \brief Host mode endpoint status.

    <B>Description</B>\n
    At the end of each data transfer, the transfer scheduler on the FX3 device
    returns a 32-bit status value. This status value has a number of fields
    that report the status of the data transfer.

    **\see
    *\see CyU3PUsbHostXferCb_t
 */
typedef uint32_t CyU3PUsbHostEpStatus_t;

/** \def CY_U3P_USB_HOST_EPS_EPNUM_POS
    \brief Position of endpoint number field in the status word.
 */
#define CY_U3P_USB_HOST_EPS_EPNUM_POS              (0)

/** \def CY_U3P_USB_HOST_EPS_EPNUM_MASK
    \brief Mask for the endpoint number field in the status word.
 */
#define CY_U3P_USB_HOST_EPS_EPNUM_MASK             (0x0000000F)

/** \def CY_U3P_USB_HOST_EPS_EPDIR
    \brief Endpoint direction bit (1 - OUT, 0 - IN).
 */
#define CY_U3P_USB_HOST_EPS_EPDIR                  (0x00000010)

/** \def CY_U3P_USB_HOST_EPS_ACTIVE
    \brief Endpoint active bit. Indicates whether the EP is still active.
 */
#define CY_U3P_USB_HOST_EPS_ACTIVE                 (0x00000020)

/** \def CY_U3P_USB_HOST_EPS_HALT
    \brief Endpoint halt bit. Set if the endpoint is halted (stalled).
 */
#define CY_U3P_USB_HOST_EPS_HALT                   (0x00000040)

/** \def CY_U3P_USB_HOST_EPS_OVER_UNDER_RUN
    \brief This bit is set if an overrun or underrun has happened on this endpoint.
 */
#define CY_U3P_USB_HOST_EPS_OVER_UNDER_RUN         (0x00000080)

/** \def CY_U3P_USB_HOST_EPS_BABBLE
    \brief This bit is set if a babble was detected during the transfer.
 */
#define CY_U3P_USB_HOST_EPS_BABBLE                 (0x00000100)

/** \def CY_U3P_USB_HOST_EPS_XACT_ERROR
    \brief This bit is set if there was a transfer error.
 */
#define CY_U3P_USB_HOST_EPS_XACT_ERROR             (0x00000200)

/** \def CY_U3P_USB_HOST_EPS_PING
    \brief This bit is set if there was a PING packet.
 */
#define CY_U3P_USB_HOST_EPS_PING                   (0x00000400)

/** \def CY_U3P_USB_HOST_EPS_PHY_ERROR
    \brief This bit is set if there was a PHY error during the transfer.
 */
#define CY_U3P_USB_HOST_EPS_PHY_ERROR              (0x00000800)

/** \def CY_U3P_USB_HOST_EPS_PID_ERROR
    \brief This bit is set if there was a PID error during the transfer.
 */
#define CY_U3P_USB_HOST_EPS_PID_ERROR              (0x00001000)

/** \def CY_U3P_USB_HOST_EPS_TIMEOUT_ERROR
    \brief This bit is set if there was a timeout error during the transfer.
 */
#define CY_U3P_USB_HOST_EPS_TIMEOUT_ERROR          (0x00002000)

/** \def CY_U3P_USB_HOST_EPS_ISO_ORUN_ERROR
    \brief This bit is set if there was an ISO overrun error during the transfer.
 */
#define CY_U3P_USB_HOST_EPS_ISO_ORUN_ERROR         (0x00004000)

/** \def CY_U3P_USB_HOST_EPS_IOC_INT
    \brief This bit is set if there was an IOC interrupt.
 */
#define CY_U3P_USB_HOST_EPS_IOC_INT                (0x00008000)

/** \def CY_U3P_USB_HOST_EPS_BYTE_COUNT_POS
    \brief Position of the remaining byte count field.
 */
#define CY_U3P_USB_HOST_EPS_BYTE_COUNT_POS         (16)

/** \def CY_U3P_USB_HOST_EPS_BYTE_COUNT_MASK
    \brief Mask for the remaining byte count field.
 */
#define CY_U3P_USB_HOST_EPS_BYTE_COUNT_MASK        (0xFFFF0000)

/** \brief Host mode port status information.

    <B>Description</B>\n
    The port status returned by the library is a 16-bit value with multiple fields.

    **\see
    *\see CyU3PUsbHostGetPortStatus
 */
typedef uint16_t CyU3PUsbHostPortStatus_t;

/** \def CY_U3P_USB_HOST_PORT_STAT_CONNECTED
    \brief This bit is set if a peripheral is connected downstream.
 */
#define CY_U3P_USB_HOST_PORT_STAT_CONNECTED        (0x0001)

/** \def CY_U3P_USB_HOST_PORT_STAT_ENABLED
    \brief This bit is set if the host port has been enabled by the user.
 */
#define CY_U3P_USB_HOST_PORT_STAT_ENABLED          (0x0002)

/** \def CY_U3P_USB_HOST_PORT_STAT_ACTIVE
    \brief Mask to identify whether the port is active. A port is active if it is enabled
    and connected to a downstream peripheral.
 */
#define CY_U3P_USB_HOST_PORT_STAT_ACTIVE           (0x0003)

/** \def CY_U3P_USB_HOST_PORT_STAT_SUSPENDED
    \brief This bit is set if the port has been suspended.
 */
#define CY_U3P_USB_HOST_PORT_STAT_SUSPENDED        (0x0004)

/** \brief Host mode endpoint configuration structure.

    <B>Description</B>\n
    The structure holds the information for configuring an endpoint when the
    FX3 is acting as a USB host.

    **\see
    *\see CyU3PUsbHostEpAdd
 */
typedef struct CyU3PUsbHostEpConfig_t
{
    CyU3PUsbEpType_t type;      /**< Type of endpoint to be created. */
    uint8_t mult;               /**< Number of packets to be transferred per micro-frame. This should be 1
                                     for bulk, control and interrupt endpoints; and 1, 2, or 3 for isochronous
                                     endpoints. */
    uint16_t maxPktSize;        /**< The maximum packet size for the endpoint. Valid range is as defined in the
                                     USB specification. */
    uint8_t pollingRate;        /**< Rate at which the endpoint has to be polled in ms. Zero will indicate
                                     that polling will be done every micro-frame and any non-zero value will
                                     specify the polling rate. It should be noted that pollingRate is valid only
                                     when the request itself is larger than a single packet. This setting is
                                     valid for synchronous endpoints. For asynchronous endpoints, this should be
                                     set to zero. */
    uint16_t fullPktSize;       /**< This is used for DMA packet boundary identification. If the DMA
                                     buffer allocated is larger than the maxPktSize specified, this
                                     field determines when a buffer is wrapped up. A DMA buffer is
                                     wrapped up by the hardware when it sees a SLP or ZLP. So, as long as the
                                     data received is a multiple of fullPktSize, the buffer is not
                                     wrapped up. fullPktSize cannot be smaller than the maxPktSize. */
    CyBool_t isStreamMode;      /**< Enable stream mode. This means that the EP is always active. This setting
                                     is valid only for an IN EP, and should be CyFalse for EP0 and OUT endpoints.
                                     When the flag is set, an IN EP will send IN tokens and collect data whenever
                                     there is a free buffer on the DMA channel. */
} CyU3PUsbHostEpConfig_t;

/** \brief Host mode event callback function.

    <B>Description</B>\n
    The event callback function is used to notify the user application about peripheral
    connect and disconnect events.

    **\see
    *\see CyU3PUsbHostStart
 */
typedef void (*CyU3PUsbHostEventCb_t) (
        CyU3PUsbHostEventType_t evType,         /**< The event type. */
        uint32_t                evData          /**< Event specific data. */
        );

/** \brief Host mode endpoint transfer complete callback function.

    <B>Description</B>\n
    The transfer callback function is registered when the USB host stack is started,
    and is used by the driver to notify the application about transfer completion.

    **\see
    *\see CyU3PUsbHostStart
    *\see CyU3PUsbHostEpSetXfer
 */
typedef void (*CyU3PUsbHostXferCb_t)(
        uint8_t                ep,      /* The endpoint number. */
        CyU3PUsbHostEpStatus_t epStatus /* Endpoint status. */
        );

/** \brief USB host mode configuration information.

    <B>Description</B>\n
    The FX3 host mode driver takes the following configuration parameters,
    which are set when starting the stack. These settings cannot be changed
    dynamically while the stack is active.

    **\see
    *\see CyU3PUsbHostStart
 */
typedef struct CyU3PUsbHostConfig_t
{
    CyBool_t ep0LowLevelControl;        /**< Whether to enable EP0 low level control. If CyFalse, the EP0
                                             DMA is handled by firmware. Only the CyU3PUsbHostSendSetupRqt
                                             API needs to be called. If CyTrue, EP0 DMA transfers need to
                                             be handled by the application. This allows fine control over
                                             setup, data and status phase. This mode should be used only if
                                             the EP0 data needs to be routed to a different path out of the
                                             FX3 device. The maxPktSize and fullPktSize for EP0 should be
                                             the same of this setting is set to true. */
    CyU3PUsbHostEventCb_t eventCb;      /**< Event callback function for USB host stack. */
    CyU3PUsbHostXferCb_t  xferCb;       /**< EP transfer completion callback for USB host stack. */
} CyU3PUsbHostConfig_t;

/**************************************************************************
 *************************** Function prototypes **************************
 **************************************************************************/

/** \brief Check whether the USB host stack has been started.

    <B>Description</B>\n
    This function is used to check whether the USB host stack has been started.

    <B>Return value</B>\n
    * CyTrue - USB host module started.\n
    * CyFalse - USB host module not started.

    **\see
    *\see CyU3PUsbHostStart
    *\see CyU3PUsbHostStop
 */
extern CyBool_t
CyU3PUsbHostIsStarted (
        void);

/** \brief This function initializes the USB 2.0 host stack.

    <B>Description</B>\n
    This function enables the USB block and configures it to function as a
    host. The configuration parameters cannot be changed unless the stack is
    stopped and re-started.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if the current FX3 device does not support the USB 2.0 host.\n
    * CY_U3P_ERROR_NULL_POINTER - If NULL pointer is passed in as parameter.\n
    * CY_U3P_ERROR_ALREADY_STARTED - The host stack is already running.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - FX3 is in the wrong OTG mode or USB device stack is running.

    **\see
    *\see CyU3PUsbHostConfig_t
    *\see CyU3PUsbHostStop
    *\see CyU3PUsbHostIsStarted
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostStart (
        CyU3PUsbHostConfig_t *hostCfg   /**< Pointer to the host configuration information. */
        );

/** \brief This function de-initializes the USB host stack.

    <B>Description</B>\n
    This function disables the USB host mode operation and de-initializes
    the USB host stack.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The host mode stack is not running.

    **\see
    *\see CyU3PUsbHostStart
    *\see CyU3PUsbHostIsStarted
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostStop (
        void);

/** \brief This function enables the USB host port on the FX3 device.

    <B>Description</B>\n
    The USB host port on the FX3 is kept disabled when the host stack is started.
    The driver uses the event callback to notify that a downstream peripheral has
    been connected, and then the application can call this API to enable the host
    port.

    This function enables the port and sets it to the correct speed of operation.
    When the down-stream peripheral is disconnected, the port automatically gets
    disabled.

    The FX3 device has only a single usb host port which can support a single
    peripheral device. Hubs are not supported.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The host stack is not running.\n
    * CY_U3P_ERROR_ALREADY_STARTED - The port is already enabled.\n
    * CY_U3P_ERROR_FAILURE - No downstream peripheral attached.

    **\see
    *\see CyU3PUsbHostPortDisable
    *\see CyU3PUsbHostGetPortStatus
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostPortEnable (
        void);

/** \brief Disable the USB host port.

    <B>Description</B>\n
    This function disables the USB host port on the FX3 device. The device will still
    be able to detect a newly connected peripheral after the port is disabled.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The port is not enabled.

    **\see
    *\see CyU3PUsbHostPortEnable
    *\see CyU3PUsbHostGetPortStatus
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostPortDisable (
        void);

/** \brief This function retrieves the current port status.

    <B>Description</B>\n
    This function retrieves the current state and speed of operation of the
    USB host port on the FX3 device.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The host stack is not running.

    **\see
    *\see CyU3PUsbHostPortEnable
    *\see CyU3PUsbHostPortDisable
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostGetPortStatus (
        CyU3PUsbHostPortStatus_t *portStatus, /* Return parameter which will be filled with the port status. */
        CyU3PUsbHostOpSpeed_t    *portSpeed   /* Return parameter which will be filled with the port speed. */
        );

/** \brief This function resets the USB host port.

    <B>Description</B>\n
    This function resets the USB host port so that the peripheral connected can be
    re-enumerated. Calling this function is equivalent to a port disable call followed
    by a port enable call.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The host stack is not running.

    **\see
    *\see CyU3PUsbHostPortEnable
    *\see CyU3PUsbHostPortDisable
    *\see CyU3PUsbHostGetPortStatus
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostPortReset (
        void);

/** \brief Suspend the USB host port.

    <B>Description</B>\n
    This function suspends the USB host port. Please note that the FX3 device
    does not support remote wakeup and the port resumption has to be done by
    the firmware application.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The port is not active.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - There is an active data transfer.

    **\see
    *\see CyU3PUsbHostPortResume
    *\see CyU3PUsbHostGetPortStatus
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostPortSuspend (
        void);

/** \brief Resume the previously suspended USB host port.

    <B>Description</B>\n
    This function resumes the USB host port, after it was previously suspended
    through the CyU3PUsbHostPortSuspend API.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The port is not enabled or device got disconnected.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - The port is not suspended.\n

    **\see
    *\see CyU3PUsbHostPortSuspend
    *\see CyU3PUsbHostGetPortStatus
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostPortResume (
        void);

/** \brief Get the current frame number to be used.

    <B>Description</B>\n
    This function gets the current USB frame number. The frame number query is
    not synchronized with the scheduler and only returns the number at the time
    of the API call.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NULL_POINTER - The input pointer was NULL.\n
    * CY_U3P_ERROR_NOT_STARTED - The host port is not enabled.

    **\see
    *\see CyU3PUsbHostStart
    *\see CyU3PUsbHostPortEnable
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostGetFrameNumber (
        uint32_t *frameNumber           /**< Pointer to load the frame number. */
        );

/** \brief Get the current downstream peripheral address.

    <B>Description</B>\n
    This function returns the device address that has been assigned by FX3 to the
    downstream peripheral. A return value of zero indicates that the peripheral
    has been disconnected.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NULL_POINTER - The pointer provided is NULL.\n
    * CY_U3P_ERROR_NOT_STARTED - The host port is not enabled.

    **\see
    *\see CyU3PUsbHostSetDeviceAddress
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostGetDeviceAddress (
        uint8_t *devAddr                /**< Pointer to load the current device address. */
        );

/** \brief Set (update) the downstream peripheral address.

    <B>Description</B>\n
    This function sets (updates) the device address that should be used by
    the FX3 to talk to the downstream peripheral. This address is initialized
    to zero whenever the port is enabled. The application should set the address
    after completing a SET_ADDRESS command successfully.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - The address parameter is invalid.\n
    * CY_U3P_ERROR_NOT_STARTED - The host port is not enabled.

    **\see
    *\see CyU3PUsbHostGetDeviceAddress
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostSetDeviceAddress (
        uint8_t devAddr                 /**< Device address to be set. */
        );

/** \brief Add an endpoint to the scheduler.

    <B>Description</B>\n
    The USB host block on the FX3 maintains a schedule based on which data
    transfers on various endpoints are initiated. The firmware application
    should identify the active set of endpoints on the downstream peripheral,
    and add the corresponding endpoints to the execution schedule.

    The schedule parameters that are passed to this function depend on the
    values reported by the peripheral in the endpoint descriptors.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NULL_POINTER - The input pointer was NULL.\n
    * CY_U3P_ERROR_NOT_STARTED - The port is not enabled.\n
    * CY_U3P_ERROR_ALREADY_STARTED - The endpoint is already added.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - One or more of the input parameter fields is invalid.

    **\see
    *\see CyU3PUsbHostEpRemove
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostEpAdd (
        uint8_t ep,                     /**< Endpoint to be added to the transfer schedule. */
        CyU3PUsbHostEpConfig_t *cfg     /**< Endpoint configuration parameters. */
        );

/** \brief Removes an endpoint from the scheduler.

    <B>Description</B>\n
    This function can be called to remove an endpoint from the transfer schedule, once
    it is no longer in use by the application.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - The endpoint number is invalid.\n
    * CY_U3P_ERROR_NOT_STARTED - The port is not enabled.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - The endpoint is not yet added.

    **\see
    *\see CyU3PUsbHostEpAdd
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostEpRemove (
        uint8_t ep                      /**< Endpoint to be removed from scheduler. */
        );

/** \brief Reset an endpoint.

    <B>Description</B>\n
    This function flushes the internal buffers and resets the data toggles for
    an endpoint. This should only be called when there is no active transfer on
    the endpoint.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The port is not enabled.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - The endpoint number is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - The endpoint is not added.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - The endpoint is active.

    **\see
    *\see CyU3PUsbHostEpAdd
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostEpReset (
        uint8_t ep                      /**< Endpoint to be reset. */
        );

/** \brief Perform a USB control (EP0) transfer.

    <B>Description</B>\n
    This function performs all of the operations associated with a USB control (EP0)
    transfer. This is only valid when the ep0LowLevelControl setting is false.

    The API initiates the transfer of setup packet, but does not wait for completion.
    The setup, data and status phases of the transfer will be handled by the driver;
    and the transfer complete callback shall be called when all of these are done.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - EP0 Low level control enabled.\n
    * CY_U3P_ERROR_NOT_STARTED - The port is not enabled.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - The endpoint is already active.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - The endpoint is not added.\n
    * CY_U3P_ERROR_DMA_FAILURE - Error in setting internal DMA channels.

    **\see
    *\see CyU3PUsbHostStart
    *\see CyU3PUsbHostEpAbort
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostSendSetupRqt (
        uint8_t *setupPkt,      /**< Pointer to buffer containing the 8 byte setup packet. */
        uint8_t *buf_p          /**< Buffer for the data phase. The buffer should be big enough to
                                     handle the complete data phase, and should already contain the data in
                                     the case of an OUT data transfer.. */
        );

/** \brief Start a non-EP0 data transfer.

    <B>Description</B>\n
    This function will enable the endpoint and setup the data transfer for all
    EPs except for EP0. This can also be used for EP0 when the low level control
    flag has been set to true.

    In the case of a low level EP0 transfer, the 8 byte setup packet needs to
    be queued on the EP0 egress DMA channel. If there is a data phase, that also
    needs to be queued on the EP0 ingress or egress DMA channel. Once the DMA channels
    have been setup, the EP0 transfer can be kicked off by calling the
    CyU3PUsbHostEp0BeginXfer API.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The port is not enabled.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - One or more input parameters is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - The endpoint is not added.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - The endpoint is already active.

    **\see
    *\see CyU3PUsbHostEp0BeginXfer
    *\see CyU3PUsbHostEpAbort
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostEpSetXfer (
        uint8_t ep,                             /**< Endpoint to configure. */
        CyU3PUsbHostEpXferType_t type,          /**< Type of transfer. This is meaningful only for EP0. */
        uint32_t count                          /**< Size of data to be transferred. */
        );

/** \brief Kick off a low level control endpoint transfer.

    <B>Description</B>\n
    This function enables the endpoint and starts the transfer for EP0. The
    setup packet must be queued on EP0 egress socket before this function is
    called.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The port is not enabled.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - The endpoint is not configured or the transfer is not setup.

    **\see
    *\see CyU3PUsbHostEpSetXfer
    *\see CyU3PUsbHostEpAbort
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostEp0BeginXfer (
        void);

/** \brief Abort pending transfers on the selected endpoint.

    <B>Description</B>\n
    This function aborts any ongoing data transfer on the selected endpoint
    and deactivates it. The DMA channels are not touched by the API, and they
    need to be separately reset.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The port is not enabled.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - The endpoint number is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - The endpoint is not added.

    **\see
    *\see CyU3PUsbHostEpSetXfer
    *\see CyU3PUsbHostEp0BeginXfer
 */
extern CyU3PReturnStatus_t
CyU3PUsbHostEpAbort (
        uint8_t ep                      /**< Endpoint to abort. */
        );

/** \brief Wait for the current endpoint transfer to complete.

    <B>Description</B>\n
    The function waits for the transfer to complete or get aborted. If this does
    not happen within the timeout specified, it returns CY_U3P_ERROR_TIMEOUT.

    A timeout error does not necessarily mean that it has failed, and it is possible
    that another call to this API returns successfully.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - The call was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - The port is not enabled.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - The endpoint number is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - The endpoint is not added.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - No active transfer.\n
    * CY_U3P_ERROR_TIMEOUT - The transfer is not complete at the end of the specified timeout duration.\n
    * CY_U3P_ERROR_STALLED - The transfer was stalled by the USB peripheral.

    **\see
    *\see CyU3PUsbHostEpSetXfer
    *\see CyU3PUsbHostEp0BeginXfer
 */
CyU3PReturnStatus_t
CyU3PUsbHostEpWaitForCompletion (
        uint8_t ep,                             /**< Endpoint to wait on. */
        CyU3PUsbHostEpStatus_t *epStatus,       /**< Output parameter to be filled in with transfer status. */
        uint32_t waitOption                     /**< Timeout duration to wait for. */
        );

#include <cyu3externcend.h>

#endif /* _INCLUDED_CYU3P_USB_HOST_H_ */

/*[]*/

