/*
 ## Cypress FX3 Core Library Header (cyu3usb.h)
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

#ifndef _INCLUDED_CYU3USB_H_
#define _INCLUDED_CYU3USB_H_

#include "cyu3types.h"
#include "cyu3usbconst.h"
#include "cyu3externcstart.h"

/** \file cyu3usb.h
    \brief The USB device mode driver on the FX3 device is responsible for handling
    USB 3.0 and 2.0 connections, and providing the API hooks for configuring device
    operations.

    \section UsbEnumModes Enumeration Modes
    \brief The FX3 USB driver supports two different modes of enumeration, one of
    which is optimized for good performance and simplified application code; whereas
    the other allows for greater flexibility.

    <B>Description</B>\n
    Two different modes of enumeration control are supported by the USB driver
    on the FX3 device.

    **1. Fast enumeration**: In this case, all of the descriptors for all USB speeds
    of interest are registered with the USB driver through the CyU3PUsbSetDesc
    API call. The USB driver then uses this information to handle all of the
    standard USB setup requests. The driver is responsible for identifying the
    current connection speed and using the appropriate set of USB descriptors.

    In this case, only a single configuration descriptor can be used because
    only one config descriptor for each connection speed can be registered with
    the USB driver. The application will also need to disconnect the USB link
    and re-enumerate every time it wants to change the descriptors.

    **2. User enumeration**: In this case, all USB setup requests are forwarded to the
    user application logic through a callback function. It is the responsibility
    of the application code to handle these requests and to send the appropriate
    responses to the USB host. The application also has to track the current
    USB connection speed and use this to identify the specific descriptor to send
    up to the host.

    In this case, the application can change the descriptor data at will and
    has maximum flexibility in terms of implementing multiple configurations
    and alternate settings.

    <B>Note</B>\n
    In either form of enumeration, any non-standard (Class or vendor specific)
    setup request will trigger a callback function. The user application is
    expected to handle these requests in all cases.

    \section UsbDevFuncs USB Device Mode Functions
    \brief The USB device APIs are used to configure the USB device functionality
    and to perform USB data transfers.

    <B>Description</B>\n
    The FX3 device supports programmable USB peripheral implementation at
    USB-SS, USB-HS and USB-FS speeds. The type of USB device to be implemented as
    well as the endpoint pipes to be used can be configured through a set of USB
    device mode APIs. The USB device mode APIs also provide the capability to
    manage the endpoint states such as STALL and NAK as well as to perform data
    transfers on the control endpoint (EP0). Data transfers on the other USB endpoints
    will be controlled through the DMA APIs.
 */

/**************************************************************************
 ****************************** Data Types ********************************
 **************************************************************************/

/** \brief List of USB related events that are raised by the USB manager.

    <B>Description</B>\n
    The USB manager or driver continually runs on a thread on the FX3 device
    and performs the necessary operations to handle various USB requests from
    the host. The USB driver can notify the user application about various USB
    specific events by calling a callback function that is registered with it.
    This type lists the various USB event types that are notified to the
    application logic.

    **\see
    *\see CyU3PUSBEventCb_t
    *\see CyU3PUsbEnableITPEvent
 */
typedef enum CyU3PUsbEventType_t
{
    CY_U3P_USB_EVENT_CONNECT = 0,       /**< USB Connect event. The evData parameter is an integer value which
                                             indicates whether it is a 3.0 connection (evData = 1) or a 2.0
                                             connection (evData = 0). */
    CY_U3P_USB_EVENT_DISCONNECT,        /**< USB Disconnect event. The evData parameter is not used and will
                                             be NULL. */
    CY_U3P_USB_EVENT_SUSPEND,           /**< USB Suspend event for both USB 2.0 and 3.0 connections. The evData
                                             parameter is not used and will be NULL. */
    CY_U3P_USB_EVENT_RESUME,            /**< USB Resume event. The evData parameter is not used and will be
                                             NULL. */
    CY_U3P_USB_EVENT_RESET,             /**< USB Reset event. The evData parameter is an integer value which
                                             indicates whether it is a 3.0 connection (evData = 1) or a 2.0
                                             connection (evData = 0). */
    CY_U3P_USB_EVENT_SETCONF,           /**< USB Set Configuration event. evData provides the configuration
                                             number that is selected by the host. */
    CY_U3P_USB_EVENT_SPEED,             /**< USB Speed Change event. The evData parameter is not used and will
                                             always be set to 1. */
    CY_U3P_USB_EVENT_SETINTF,           /**< USB Set Interface event. The evData parameter provides the interface
                                             number and the selected alternate setting values.
                                             Bits  7 - 0: LSB of wValue field from setup request.
                                             Bits 15 - 8: LSB of wIndex field from setup request. */
    CY_U3P_USB_EVENT_SET_SEL,           /**< USB 3.0 Set System Exit Latency event. The event data
                                             parameter will be zero. The CyU3PUsbGetDevProperty API
                                             can be used to get the SEL values specified by the host. */
    CY_U3P_USB_EVENT_SOF_ITP,           /**< SOF/ITP event notification. The evData parameter is not used and
                                             will be NULL. Since these events are very frequent, the event
                                             notification needs to be separately enabled using the
                                             CyU3PUsbEnableITPEvent() API call. */
    CY_U3P_USB_EVENT_EP0_STAT_CPLT,     /**< Event notifying completion of the status phase of a control request.
                                             This event is only generated for control requests that are handled
                                             by the user's application; and not for requests that are handled
                                             internally by the USB driver. */
    CY_U3P_USB_EVENT_VBUS_VALID,        /**< Vbus power detected on FX3's USB port. */
    CY_U3P_USB_EVENT_VBUS_REMOVED,      /**< Vbus power removed from FX3's USB port. */
    CY_U3P_USB_EVENT_HOST_CONNECT,      /**< USB host mode connect event. */
    CY_U3P_USB_EVENT_HOST_DISCONNECT,   /**< USB host mode disconnect event. */
    CY_U3P_USB_EVENT_OTG_CHANGE,        /**< USB OTG-ID Pin state change. */
    CY_U3P_USB_EVENT_OTG_VBUS_CHG,      /**< VBus made valid by OTG host. */
    CY_U3P_USB_EVENT_OTG_SRP,           /**< SRP signalling detected from OTG device. */
    CY_U3P_USB_EVENT_EP_UNDERRUN,       /**< Indicates that a data underrun error has been detected on one
                                             of the USB endpoints. The event data will provide the endpoint
                                             number. */
    CY_U3P_USB_EVENT_LNK_RECOVERY,      /**< Indicates that the USB link has entered recovery. This event
                                             will not be raised if the recovery is entered as part of a
                                             transition from Ux to U0. This event is raised from the interrupt
                                             context, and the event handler should not invoke any blocking
                                             operations. */
    CY_U3P_USB_EVENT_USB3_LNKFAIL,      /**< Indicates that the USB 3.0 link from FX3 to the USB host has
                                             failed. This event is only generated if the USB 2.0 operation
                                             of FX3 is disabled; and should be used to trigger a USB 2.0
                                             connection from the external controller. */
    CY_U3P_USB_EVENT_SS_COMP_ENTRY,     /**< Indicates that the USB 3.0 link has entered the compliance state. */
    CY_U3P_USB_EVENT_SS_COMP_EXIT,      /**< Indicates that the USB 3.0 link has exited the compliance state. */
    CY_U3P_USB_EVENT_RESERVED_1,        /**< Reserved event code. No handling required. */
    CY_U3P_USB_EVENT_LMP_EXCH_FAIL      /**< Indicates that USB 3.0 connection failed due to LPM handshake failure. */
} CyU3PUsbEventType_t;

/** \brief List of USB device manager states.

    <B>Description</B>\n
    This USB driver (manager) implements a state machine that tracks the current state
    of the USB connection and uses this to identify the appropriate response to USB
    requests. This type lists the various possible states for the USB driver state
    machine. Some of these states are software defined and have no connection with
    any USB bus conditions.
 */
typedef enum CyU3PUsbMgrStates_t
{
    CY_U3P_USB_INACTIVE = 0,            /**< USB module not started. */
    CY_U3P_USB_STARTED,                 /**< USB module started and waiting for configuration. */
    CY_U3P_USB_WAITING_FOR_DESC,        /**< USB module waiting for a configuration descriptor. */
    CY_U3P_USB_CONFIGURED,              /**< USB module completely configured. */
    CY_U3P_USB_VBUS_WAIT,               /**< USB module waiting for Vbus to connect to USB host. */
    CY_U3P_USB_CONNECTED,               /**< Waiting for USB connection. */
    CY_U3P_USB_ESTABLISHED,             /**< USB connection active. */
    CY_U3P_USB_MS_ACTIVE,               /**< Mass storage function active. */
    CY_U3P_USB_MAX_STATE                /**< Sentinel state. */
} CyU3PUsbMgrStates_t;

/** \brief Virtual descriptor types to be used to set descriptors.

    <B>Description</B>\n
    The user application can register different device and configuration descriptors with the
    USB driver, to be used at various USB connection speeds. To support this, the CyU3PUsbSetDesc
    call accepts a descriptor type parameter that is different from the USB standard
    descriptor type value that is embedded in the descriptor itself. This enumerated type
    is to be used by the application to register various descriptors with the USB driver.

    **\see
    *\see CyU3PUsbSetDesc
 */
typedef enum CyU3PUSBSetDescType_t
{
    CY_U3P_USB_SET_SS_DEVICE_DESCR,     /**< Device descriptor for SuperSpeed. */
    CY_U3P_USB_SET_HS_DEVICE_DESCR,     /**< Device descriptor for Hi-Speed and Full Speed. */
    CY_U3P_USB_SET_DEVQUAL_DESCR,       /**< Device Qualifier descriptor. */
    CY_U3P_USB_SET_FS_CONFIG_DESCR,     /**< Configuration descriptor for Full Speed. */
    CY_U3P_USB_SET_HS_CONFIG_DESCR,     /**< Configuration descriptor for Hi-Speed. */
    CY_U3P_USB_SET_STRING_DESCR,        /**< String Descriptor */
    CY_U3P_USB_SET_SS_CONFIG_DESCR,     /**< Configuration descriptor for SuperSpeed. */
    CY_U3P_USB_SET_SS_BOS_DESCR,        /**< BOS descriptor. */
    CY_U3P_USB_SET_OTG_DESCR            /**< OTG descriptor. */
} CyU3PUSBSetDescType_t;

/** \brief List of supported USB connection speeds.
 */
typedef enum CyU3PUSBSpeed_t
{
    CY_U3P_NOT_CONNECTED = 0x00,        /**< USB device not connected. */
    CY_U3P_FULL_SPEED,                  /**< USB full speed. */
    CY_U3P_HIGH_SPEED,                  /**< High speed. */
    CY_U3P_SUPER_SPEED                  /**< Super speed. */
} CyU3PUSBSpeed_t;

/** \brief List of USB 3.0 link operating modes.

    <B>Description</B>\n
    List of USB 3.0 link operating modes. Only the active and low power modes are listed here
    as the rest of the link modes are not visible to software.

    **\see
    *\see CyU3PUsbLPMReqCb_t
 */
typedef enum CyU3PUsbLinkPowerMode
{
    CyU3PUsbLPM_U0 = 0,                 /**< U0 (active) power state. */
    CyU3PUsbLPM_U1,                     /**< U1 power state. */
    CyU3PUsbLPM_U2,                     /**< U2 power state. */
    CyU3PUsbLPM_U3,                     /**< U3 (suspend) power state. */
    CyU3PUsbLPM_COMP,                   /**< Compliance state. */
    CyU3PUsbLPM_Unknown                 /**< The link is not in any of the Ux states. This normally
                                             happens while the link training is ongoing. */
} CyU3PUsbLinkPowerMode;

/** \brief List of USB endpoint events.

    <B>Description</B>\n
    The USB API can relay a set of endpoint related events to the firmware application
    for monitoring and control purposes. This enumerated data type lists the various endpoint
    events that can be generated by the API library.

    **\see
    *\see CyU3PUsbEpEvtCb_t
    *\see CyU3PUsbRegisterEpEvtCallback
    *\see CyU3PUsbEpEvtControl
    *\see CyU3PUsbSetXfer
 */
typedef enum CyU3PUsbEpEvtType
{
    CYU3P_USBEP_NAK_EVT = 1,            /**< The endpoint sent NAK in response to a host transfer request.
                                             In the case of a USB 3.0 connection, this callback only indicates
                                             that the endpoint has gone into a flowcontrol state due to not
                                             being ready for data transfer. This does not necessarily mean that
                                             the device has sent an NRDY response to the USB host on that
                                             endpoint. */
    CYU3P_USBEP_ZLP_EVT = 2,            /**< A Zero Length Packet was sent/received on the endpoint. */
    CYU3P_USBEP_SLP_EVT = 4,            /**< A Short Length Packet was sent/received on the endpoint.
                                             Note that the SLP event will only on USB 2.0 connections if a non-zero
                                             transfer size has been set on the endpoint using CyU3PUsbSetXfer. */
    CYU3P_USBEP_ISOERR_EVT = 8,         /**< Out of sequence ISO PIDs or ZLP received on ISO endpoint. */
    CYU3P_USBEP_SS_RETRY_EVT = 16,      /**< An ACK TP with the RETRY bit was received on the IN endpoint. */
    CYU3P_USBEP_SS_SEQERR_EVT = 32,     /**< Sequence number error detected during endpoint data transfer. */
    CYU3P_USBEP_SS_STREAMERR_EVT = 64,  /**< A Bulk-Stream protocol error occured on the endpoint. */
    CYU3P_USBEP_SS_BTERM_EVT = 128,     /**< The USB 3.0 host pre-maturely terminated a burst transfer. */
    CYU3P_USBEP_SS_RESET_EVT = 256      /**< USB 3.0 endpoint reset event. A stall recovery is required. */
} CyU3PUsbEpEvtType;

/** \brief Callback function type used for notification of USB endpoint events.

    <B>Description</B>\n
    This callback function type is used by the firmware application to
    receive notifications of USB endpoint events of interest.

    **\see
    *\see CyU3PUsbEpEvtType
    *\see CyU3PUsbRegisterEpEvtCallback
 */
typedef void (*CyU3PUsbEpEvtCb_t)(
        CyU3PUsbEpEvtType evType,       /**< Type of event being notified. */
        CyU3PUSBSpeed_t   usbSpeed,     /**< Current USB connection speed. */
        uint8_t           epNum         /**< Endpoint number with direction. */
        );

/** \brief USB event callback type.

    <B>Description</B>\n
    Type of callback function that should be registered with the USB driver
    to get notifications of USB specific events.

    The evtype parameter specifies the type of event and evdata contains event
    specific data.

    **\see
    *\see CyU3PUsbEventType_t
    *\see CyU3PUsbRegisterEventCallback
 */
typedef void (*CyU3PUSBEventCb_t)(
        CyU3PUsbEventType_t evtype,     /**< The event type. */
        uint16_t            evdata      /**< Event specific data. */
        );

/** \def CY_U3P_USB_REQUEST_TYPE_MASK
    \brief Mask to get the bmRequestType field from the USB setup request.
 */
#define CY_U3P_USB_REQUEST_TYPE_MASK                  (0x000000FF)

/** \def CY_U3P_USB_REQUEST_TYPE_POS
    \brief Position of the bmRequestType field in the USB setup request.
 */
#define CY_U3P_USB_REQUEST_TYPE_POS                   (0)

/** \def CY_U3P_USB_REQUEST_MASK
    \brief Mask to get the bRequest field from the USB setup request.
 */
#define CY_U3P_USB_REQUEST_MASK                       (0x0000FF00)

/** \def CY_U3P_USB_REQUEST_POS
    \brief Position of the bRequest field in the USB setup request.
 */
#define CY_U3P_USB_REQUEST_POS                        (8)

/** \def CY_U3P_USB_VALUE_MASK
    \brief Mask to get the wValue field from the USB setup request.
 */
#define CY_U3P_USB_VALUE_MASK                         (0xFFFF0000)

/** \def CY_U3P_USB_VALUE_POS
    \brief Position of the wValue field in the USB setup request.
 */
#define CY_U3P_USB_VALUE_POS                          (16)

/** \def CY_U3P_USB_INDEX_MASK
    \brief Mask to get the wIndex field from the USB setup request.
 */
#define CY_U3P_USB_INDEX_MASK                         (0x0000FFFF)

/** \def CY_U3P_USB_INDEX_POS
    \brief Position of the wIndex field in the USB setup request.
 */
#define CY_U3P_USB_INDEX_POS                          (0)

/** \def CY_U3P_USB_LENGTH_MASK
    \brief Mask to get the wLength field from the USB setup request.
 */
#define CY_U3P_USB_LENGTH_MASK                        (0xFFFF0000)

/** \def CY_U3P_USB_LENGTH_POS
    \brief Position of the wLength field in the USB setup request.
 */
#define CY_U3P_USB_LENGTH_POS                         (16)

/** \brief USB setup request handler type.

    <B>Description</B>\n
    Type of callback function that is invoked to handle USB setup requests.
    The function shall return CyTrue (1) if it has handled the setup request.
    If it is unable to handle this specific request, it shall return CyFalse
    and the USB driver will perform the default actions corresponding to the
    setup request.

    The handling of each setup request will involve one and only one of the
    following API calls:\n
    <B>CyU3PUsbSendEP0Data</B>: Called to perform any IN-data phase associated
    with the request. The status handshake for the request will be completed
    as soon as all of the data requested has been sent to the host.\n
    <B>CyU3PUsbGetEP0Data</B>: Called to perform any OUT-data phase associated
    with the request. The length specified should match the transfer length
    specified by the host. The status handshake for the request will be
    completed as soon as all of the data has been received from the host.\n
    <B>CyU3PUsbAckSetup</B>: Called to complete the status handshake (ACK) phase
    of a request that does not involve any data transfer.\n
    <B>CyU3PUsbStall</B>: Called to stall EP0 to fail the setup request. The ep
    number can be specified as 0x00 or 0x80, and the API will take care of
    stalling the ep in the appropriate direction.\n

    These API calls can be made from the Setup callback directly or in a delayed
    fashion in a bottom half that is scheduled from the Setup callback. In either
    case, the Setup callback should return CyTrue to let the API know that the
    default action (stalling EP0 for any non-standard request) should not be
    performed.

    <B>Note</B>\n
    Calling both CyU3PUsbSendEP0Data/CyU3PUsbGetEP0Data and CyU3PUsbAckSetup
    or calling CyU3PUsbAckSetup multiple times in response to a control request
    can result in errors due to the subsequent request being ACKed prematurely.

    **\see
    *\see CyU3PUsbRegisterSetupCallback
    *\see CyU3PUsbSendEP0Data
    *\see CyU3PUsbGetEP0Data
    *\see CyU3PUsbAckSetup
    *\see CyU3PUsbStall
 */
typedef CyBool_t (*CyU3PUSBSetupCb_t)(
        uint32_t    setupdat0,          /**< Lower 4 bytes of the setup packet. */
        uint32_t    setupdat1           /**< Upper 4 bytes of the setup packet. */
        );

/** \brief Event callback raised on host request to switch USB link to a low power state.

    <B>Description</B>\n
    The USB 3.0 host may request the FX3 device to switch the USB link to a low power
    state when the host has no more activity to perform. This event callback is raised
    when a request from the host to switch the link to a U1/U2 state is received.
    The link_mode parameter indicates the power state that the host is requesting for.
    The return value from this function call is expected to indicate whether the
    low power state entry request should be accepted. A return of CyTrue will cause
    FX3 to accept entry into U1/U2 state and a return of CyFalse will cause FX3 to
    reject the request.

    **\see
    *\see CyU3PUsbLinkPowerMode
    *\see CyU3PUsbRegisterLPMRequestCallback
 */
typedef CyBool_t (*CyU3PUsbLPMReqCb_t) (
        CyU3PUsbLinkPowerMode link_mode /**< Link Power Mode requested by the USB 3.0 host. */
        );

/** \brief Endpoint configuration information.

    <B>Description</B>\n
    This structure holds all the properties of a USB endpoint. This structure
    is used to provide information about the desired configuration of various
    endpoints in the system.

    **\see
    *\see CyU3PSetEpConfig
 */
typedef struct CyU3PEpConfig_t
{
    CyBool_t          enable;           /**< Endpoint status - enabled or not. */
    CyU3PUsbEpType_t  epType;           /**< The endpoint type - Isochronous = 1, Bulk = 2, Interrupt = 3.
                                             See USB specification for type values. */
    uint16_t          streams;          /**< Number of bulk streams used by the endpoint. This needs to be specified
                                             as the number of streams and not as "stream count - 1" as in the case of
                                             the Super Speed Companion descriptor. */
    uint16_t          pcktSize;         /**< Maximum packet size for the endpoint. Can be set to 0 if the maximum
                                             packet size should be set to the maximum value consistent with the USB
                                             specification. */
    uint8_t           burstLen;         /**< Maximum burst length in packets. This needs to be specified as the
                                             number of packets per burst and not as "burst length - 1" as in the case
                                             of the Super Speed Companion descriptor. */
    uint8_t           isoPkts;          /**< Number of packets per micro-frame for ISO endpoints. */
} CyU3PEpConfig_t;

/** \def CY_U3P_MAX_STRING_DESC_INDEX
    \brief Number of string descriptors that are supported by the USB driver.

    <B>Description</B>\n
    The USB driver is capable of storing pointers to and handling a number
    of string descriptors.  This constant represents the number of strings
    that the driver is capable of handling.

    **\see
    *\see CyU3PUsbSetDesc
 */
#define CY_U3P_MAX_STRING_DESC_INDEX                    (16)

/** \brief List of USB device properties that can be queried.

    <B>Description</B>\n
    This is the list of USB device properties that can be queried by the application through
    the CyU3PUsbGetDevProperty API. Each property value is specified as a 32 bit integer value.

    **\see
    *\see CyU3PUsbGetDevProperty
 */
typedef enum CyU3PUsbDevProperty
{
    CY_U3P_USB_PROP_DEVADDR = 0,        /**< The USB device address assigned to FX3. The 7 bit device address
                                             value is returned as the LS bits of a 32 bit word. */
    CY_U3P_USB_PROP_FRAMECNT,           /**< USB 2.0 frame count value. Returned as a 32 bit unsigned integer
                                             value. */
    CY_U3P_USB_PROP_LINKSTATE,          /**< Current state of the USB 3.0 Link. Returned as a 32 bit unsigned
                                             integer value. See CyU3PUsbLinkState_t for state encoding values. */
    CY_U3P_USB_PROP_ITPINFO,            /**< The Isochronous timestamp field from the last ITP received.
                                             Returned as a 32 bit unsigned integer value. */
    CY_U3P_USB_PROP_SYS_EXIT_LAT        /**< The System Exit Latency value specified by the USB host. The six
                                             byte SEL data is copied into the data buffer. The pointer passed
                                             in this case should correspond to a six byte (or longer) buffer. */
} CyU3PUsbDevProperty;

/** \cond USB_LOGLIST
 */

/*
    USB event log values.
 */
#define CYU3P_USB_LOG_VBUS_OFF          (0x01u) /* Indicates VBus turned off. */
#define CYU3P_USB_LOG_VBUS_ON           (0x02u) /* Indicates VBus turned on. */
#define CYU3P_USB_LOG_USB2_PHY_OFF      (0x03u) /* Indicates that the 2.0 PHY has been turned off. */
#define CYU3P_USB_LOG_USB3_PHY_OFF      (0x04u) /* Indicates that the 3.0 PHY has been turned off. */
#define CYU3P_USB_LOG_USB2_PHY_ON       (0x05u) /* Indicates that the 2.0 PHY has been turned on. */
#define CYU3P_USB_LOG_USB3_PHY_ON       (0x06u) /* Indicates that the 3.0 PHY has been turned on. */
#define CYU3P_USB_LOG_USBSS_DISCONNECT  (0x10u) /* Indicates that the USB 3.0 link has been disabled. */
#define CYU3P_USB_LOG_USBSS_RESET       (0x11u) /* Indicates that a USB 3.0 reset (warm/hot) has happened. */
#define CYU3P_USB_LOG_USBSS_CONNECT     (0x12u) /* Indicates that USB 3.0 Rx Termination has been detected. */
#define CYU3P_USB_LOG_USBSS_CTRL        (0x14u) /* Indicates that a USB 3.0 control request has been received. */
#define CYU3P_USB_LOG_USBSS_STATUS      (0x15u) /* Indicates completion of status stage for a 3.0 control request. */
#define CYU3P_USB_LOG_USBSS_ACKSETUP    (0x16u) /* Indicates that the CyU3PUsbAckSetup API has been called. */
#define CYU3P_USB_LOG_USBSS_SETCONF     (0x17u) /* Indicates that USB configuration has been selected. */
#define CYU3P_USB_LOG_LGO_U1            (0x21u) /* Indicates that a LGO_U1 command has been received. */
#define CYU3P_USB_LOG_LGO_U2            (0x22u) /* Indicates that a LGO_U2 command has been received. */
#define CYU3P_USB_LOG_LGO_U3            (0x23u) /* Indicates that a LGO_U3 command has been received. */
#define CYU3P_USB_LOG_PORTCAP_TIMEOUT   (0x24u) /* Indicates that a PORT_CAP or PORT_CFG timeout occured. */
#define CYU3P_USB_LOG_PORTCAP_DONE      (0x25u) /* Indicates that PORT_CAP handshake is completed. */
#define CYU3P_USB_LOG_PORTCFG_DONE      (0x26u) /* Indicates that PORT_CFG handshake is completed. */
#define CYU3P_USB_LOG_USB2_SUSP         (0x40u) /* Indicates that a USB 2.0 suspend condition has been detected. */
#define CYU3P_USB_LOG_USB2_RESET        (0x41u) /* Indicates that a USB 2.0 bus reset has been detected. */
#define CYU3P_USB_LOG_USB2_HSGRANT      (0x42u) /* Indicates that the USB High-Speed handshake has been completed. */
#define CYU3P_USB_LOG_USB2_CTRL         (0x44u) /* Indicates that a FS/HS control request has been received. */
#define CYU3P_USB_LOG_USB2_STATUS       (0x45u) /* Indicates completion of status stage for a FS/HS control transfer. */
#define CYU3P_USB_LOG_USB2_SETCONF      (0x46u) /* Indicates that USB configuration has been selected. */
#define CYU3P_USB_LOG_USB_FALLBACK      (0x50u) /* Indicates that the USB connection is dropping from 3.0 to 2.0 */
#define CYU3P_USB_LOG_USBSS_ENABLE      (0x51u) /* Indicates that a USB 3.0 connection is being attempted again. */
#define CYU3P_USB_LOG_USBSS_LNKERR      (0x52u) /* The number of link errors has crossed the threshold. */
#define CYU3P_USB_LOG_LTSSM_CHG         (0x80u) /* Base of values that indicate a USB 3.0 LTSSM state change. */
#define CYU3P_USB_LOG_EPM_RESET         (0xABu) /* Endpoint Memory Block Reset. */
#define CYU3P_USB_LOG_USB_HP_TIMEOUT    (0xACu) /* USB 3.0 link header acknowledgement timeout. */
#define CYU3P_USB_LOG_USBSS_LNKFAIL     (0xADu) /* USB 3.0 link failure. */
#define CYU3P_USB_LOG_USBSS_INACTIVE    (0xAEu) /* USB 3.0 link inactive state. */
#define CYU3P_USB_LOG_EP_UNDERRUN       (0xAFu) /* USB Endpoint Underrun. */
#define CYU3P_USB_LOG_PHY_ON_DELAY      (0xF0u) /* Phy Enable Delay is enabled */


/** \endcond
 */

/**************************************************************************
 *************************** Function Prototypes **************************
 **************************************************************************/

/** \brief Register a USB event callback function.

    <B>Description</B>\n
    This function registers a USB event callback function with the USB driver.
    This function will be invoked by the driver every time an USB event of
    interest happens.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PUSBEventCb_t
    *\see CyU3PUsbEventType_t
    *\see CyU3PUsbStart
 */
extern void
CyU3PUsbRegisterEventCallback (
        CyU3PUSBEventCb_t callback      /**< Event callback function pointer. */
        );

/** \brief Register a USB setup request handler.

    <B>Description</B>\n
    This function is used to register a USB setup request handler with the USB
    driver. The fastEnum parameter specifies whether this setup handler should
    be used only for unknown setup requests or for all USB setup requests.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PUSBSetupCb_t
    *\see CyU3PUsbStart
 */
extern void
CyU3PUsbRegisterSetupCallback (
        CyU3PUSBSetupCb_t callback,     /**< Setup request handler function. */
        CyBool_t fastEnum               /**< Select fast enumeration mode by setting CyTrue. */
        );

/** \brief Register a USB 3.0 LPM request handler callback.

    <B>Description</B>\n
    This function is used to register a callback that handles Link Power state requests
    from the USB host. In the absence of a registered callback, all U1/U2 LPM entry
    requests will be failed by the FX3 USB driver.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PUsbLPMReqCb_t
    *\see CyU3PUsbSetLinkPowerState
    *\see CyU3PUsbGetLinkPowerState
    *\see CyU3PUsbLPMDisable
    *\see CyU3PUsbLPMEnable
 */
extern void
CyU3PUsbRegisterLPMRequestCallback (
        CyU3PUsbLPMReqCb_t cb           /**< Callback function pointer. */
        );

/** \brief Register a callback function for notification of USB endpoint events.

    <B>Description</B>\n
    This function is used to register a callback function that will be invoked for notification of
    USB endpoint events during device operation.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PUsbEpEvtCb_t
    *\see CyU3PUsbEpEvtType
    *\see CyU3PUsbEpEvtControl
 */
extern void
CyU3PUsbRegisterEpEvtCallback (
        CyU3PUsbEpEvtCb_t cbFunc,       /**< Callback function pointer. */
        uint32_t          eventMask,    /**< Bitmap variable representing the events that should be enabled. */
        uint16_t          outEpMask,    /**< Bitmap variable representing the OUT endpoints whose events are
                                             to be enabled. Bit 1 represents EP 1-OUT, 2 represents EP 2-OUT
                                             and so on. */
        uint16_t          inEpMask      /**< Bitmap variable representing the IN endpoints whose events are to
                                             be enabled. */
        );

/** \brief Set the expected data transfer size on a USB 2.0 endpoint.

    <B>Description</B>\n
    This function is used to define an expected transfer size on a USB 2.0 endpoint (does not
    work for USB 3.0). The hardware will only generate the SLP event if a short packet is received
    on the endpoint while it has a non-zero transfer count.

    <B>Return value</B>\n
    * None
 */
extern CyU3PReturnStatus_t
CyU3PUsbSetXfer (
        uint8_t ep,                     /**< Endpoint number and direction. */
        uint16_t count                  /**< Expected transfer size. Please note that the transfer count is only
                                             used to generate the SLP event on the endpoint, and does not affect
                                             the ability of the endpoint to actually transfer more data. */
        );

/** \brief Enable or disable USB endpoint specific interrupts.

    <B>Description</B>\n
    The CyU3PUsbRegisterEpEvtCallback function is used to register a handler for and to enable
    endpoint specific events. A limitation of the function is that it enables the same events
    on all endpoints specified. There may be cases where the user is interested in different
    events on different endpoints. The CyU3PUsbEpEvtControl allows the user to selectively
    enable/disable events on specific endpoints.

    All events corresponding to the bits set in eventMask will be enabled and all other events
    will be disabled for the endpoint epNum.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the endpoint events are updated as requested.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the endpoint specified is invalid or has not been enabled.\n
    * CY_U3P_ERROR_NOT_STARTED if the USB block is not enabled, or an EP event callback is not registered.

    **\see
    *\see CyU3PUsbRegisterEpEvtCallback
    *\see CyU3PUsbEpEvtType
 */
extern CyU3PReturnStatus_t
CyU3PUsbEpEvtControl (
        uint8_t  epNum,                         /**< Endpoint to be updated. */
        uint32_t eventMask                      /**< Bit mask representing the events to be enabled.
                                                     See CyU3PUsbEpEvtType for the bit definitions. */
        );

/** \brief This function returns whether the USB device module has been started.

    <B>Description</B>\n
    Since there can be various modes of USB operations this API returns
    whether CyU3PUsbStart was invoked.

    <B>Return value</B>\n
    * CyTrue if the USB module has been started.\n
    * CyFalse if the USB module is not running.\n

    **\see
    *\see CyU3PUsbStart
    *\see CyU3PUsbStop
 */
CyBool_t
CyU3PUsbIsStarted (void);

/** \brief Start the USB driver.

    <B>Description</B>\n
    This function is used to start the USB device mode driver in the FX3 device
    and also to create the DMA channels required for the control endpoint.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_ALREADY_STARTED - when the USB driver has already been started.\n
    * CY_U3P_ERROR_CHANNEL_CREATE_FAILED - when the DMA channel creation for the control endpoint fails.\n
    * CY_U3P_ERROR_NO_REENUM_REQUIRED - if the USB block has been left running by the boot firmware image..

    **\see
    *\see CyU3PUsbStop
    *\see CyU3PUsbIsStarted
    *\see CyU3PUsbVBattEnable
    *\see CyU3PUsbRegisterEventCallback
    *\see CyU3PUsbRegisterSetupCallback
    *\see CyU3PUsbRegisterEpEvtCallback
    *\see CyU3PUsbRegisterLPMRequestCallback
    *\see CyU3PUsbSetDesc
    *\see CyU3PConnectState
 */
extern CyU3PReturnStatus_t
CyU3PUsbStart (
             void);

/** \brief Stop the USB driver.

    <B>Description</B>\n
    This function stops the USB driver on the FX3 device. It also
    frees up the DMA channels created for the control endpoint.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - when the USB driver has not been started.\n
    * Other DMA error codes - when the control endpoint DMA channel destroy fails.

    **\see
    *\see CyU3PUsbStart
    *\see CyU3PUsbIsStarted
 */
extern CyU3PReturnStatus_t
CyU3PUsbStop (
             void);

/** \brief Register a USB descriptor with the driver.

    <B>Description</B>\n
    This function is used to register a USB descriptor with the USB driver.
    The driver is capable of remembering one descriptor each of the various
    supported types as well as upto 16 different string descriptors.

    The driver only stores the descriptor pointers that are passed in to this
    function, and does not make copies of the descriptors. The caller therefore
    should not free up these descriptor buffers while the USB driver is active.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - when the USB driver has not been started.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - when the buffer pointer is invalid.\n
    * CY_U3P_ERROR_BAD_DESCRIPTOR_TYPE - When the descriptor type is not valid.\n
    * CY_U3P_ERROR_BAD_INDEX - if the string descriptor index exceeds 16.

    **\see
    *\see CyU3PUSBSetDescType_t
    *\see CyU3PUsbRegisterSetupCallback
 */
extern CyU3PReturnStatus_t
CyU3PUsbSetDesc (
        CyU3PUSBSetDescType_t desc_type,        /**< Type of descriptor to register. */
        uint8_t desc_index,                     /**< Descriptor index: Used only for string descriptors. */
        uint8_t* desc                           /**< Pointer to buffer containing the descriptor. */
        );

/** \brief Enable / disable the USB connection.

    <B>Description</B>\n
    This function is used to enable or disable the USB PHYs on the FX3 device
    and to control the connection to the USB host in that manner.
    Re-enumeration can be achieved by disabling and then enabling the USB
    connection.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - when the USB driver has not been started.\n
    * CY_U3P_ERROR_ALREADY_STARTED - if the connection is already enabled.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if all of the necessary configuration has not been completed.

    **\see
    *\see CyU3PUsbStart
    *\see CyU3PUsbStop
    *\see CyU3PUsbSetDesc
    *\see CyU3PGetConnectState
 */
extern CyU3PReturnStatus_t
CyU3PConnectState (
        CyBool_t connect,            /**< CyTrue to enable connection, CyFalse to disable connection. */
        CyBool_t ssEnable            /**< Whether to enumerate as a SS device or FS/HS device*/
        );

/** \brief Set or clear the stall status of an endpoint.

    <B>Description</B>\n
    This function is to set or clear the stall status of a given endpoint.
    This function is to be used in response to SET_FEATURE and CLEAR_FEATURE
    requests from the host as well as for interface specific error handling.
    When the stall condition is being cleared, the data toggles for the
    endpoint can also be cleared. While an option is provided to leave the
    data toggles unmodified, this should only be used under specific conditions
    as recommended by Cypress.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - when the USB driver has not been started.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - when the endpoint is invalid.

    **\see
    *\see CyU3PSetEpConfig
    *\see CyU3PUsbGetEpCfg
 */
extern CyU3PReturnStatus_t
CyU3PUsbStall (
        uint8_t ep,                     /**< Endpoint number to be modified. */
        CyBool_t stall,                 /**< CyTrue: Set the stall condition, CyFalse: Clear the stall */
        CyBool_t toggle                 /**< CyTrue: Clear the data toggles in a Clear Stall call. Note
                                             that the toggle parameter is ignored when the stall parameter
                                             is set to CyTrue. */
        );

/** \brief Retrieve the current state of the specified endpoint.

    <B>Description</B>\n
    This function retrieves the current NAK and STALL status of the specified
    endpoint. The isNak return value will be CyTrue if the endpoint is forced
    to NAK all requests. The isStall return value will be CyTrue if the endpoint
    is currently stalled.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - when the USB driver has not been started.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the EP specified is not valid.

    **\see
    *\see CyU3PUsbSetEpNak
    *\see CyU3PUsbStall
 */
extern CyU3PReturnStatus_t
CyU3PUsbGetEpCfg (
        uint8_t ep,                     /**< Endpoint number to query. */
        CyBool_t *isNak,                /**< Return parameter which will be filled with the NAK status. */
        CyBool_t *isStall               /**< Return parameter which will be filled with the STALL status. */
        );

/** \brief Force or clear the NAK status on the specified endpoint.

    <B>Description</B>\n
    All bulk and interrupt endpoints on the FX3 device can be forced to NAK
    any host request while the corresponding function driver is not prepared for
    data transfers. This function is used to force the specified endpoint to NAK
    all requests, or to clear the NAK status and allow data transfers to proceed.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - when the USB driver has not been started.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the EP specified is not valid.

    **\see
    *\see CyU3PUsbGetEpCfg
 */
extern CyU3PReturnStatus_t
CyU3PUsbSetEpNak (
        uint8_t  ep,                    /**< Endpoint to be updated. */
        CyBool_t nak                    /**< CyTrue to force NAK, CyFalse to clear NAK. */
        );


/** \brief Check whether the FX3 device is currently connected to a host.

    <B>Description</B>\n
    This function checks whether the the FX3 device is connected to a USB host.
    This depends on the detection of a valid Vbus input to the device.

    <B>Return value</B>\n
    * CyTrue: The USB PHY on FX3 has been turned ON.
    * CyFalse: The USB PHY on FX3 is turned OFF.

    **\see
    *\see CyU3PUsbStart
    *\see CyU3PUsbStop
    *\see CyU3PConnectState
 */
extern CyBool_t
CyU3PGetConnectState (
        void);

/** \brief Complete the status handshake of a USB control request.

    <B>Description</B>\n
    This function is used to complete the status handshake of a USB control
    request that does not involve any data transfer. If there is a need for
    OUT or IN data transfers to process the control request, the CyU3PUsbGetEP0Data
    and CyU3PUsbSendEP0Data calls should be used instead.

    This function should only be used if a positive ACK is to be sent to the
    USB host. To indicate an error condition, the CyU3PUsbStall call should be
    used to stall the endpoint EP0-OUT or EP0-IN.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PUSBSetupCb_t
    *\see CyU3PUsbRegisterSetupCallback
    *\see CyU3PUsbSendEP0Data
    *\see CyU3PUsbGetEP0Data
 */
extern void
CyU3PUsbAckSetup (
        void);

/** \brief Configure a USB endpoint

    <B>Description</B>\n
    The FX3 device has 30 user configurable endpoints (1-OUT to 15-OUT and
    1-IN to 15-IN) which can be separately selected and configured with desired
    properties such as endpoint type, the maximum packet size, amount of desired
    buffering. For bulk endpoints under super-speed, the number of bulk streams
    supported on that endpoint also needs to be specified.

    All of these endpoints are kept disabled by default. This function is used
    to enable and set the properties for a specified endpoint. Separate calls
    need to be made to enable and configure each endpoint that needs to be used.
    The same function can be called to disable the endpoints after use.

    <B>Note</B>\n
    Please note that a mult (isoPkts) setting of greater than 0 (multiple packets
    or bursts in a micro-frame) is only supported for endpoints 3 and 7 in either
    direction.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - when the USB driver has not been started.\n
    * CY_U3P_NULL_POINTER - when the epinfo pointer is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - when the packet size or the endpoint number is invalid.\n
    * CY_U3P_ERROR_INVALID_CONFIGURATION - when a non-zero isoPkts value is used with endpoints other than 3 and 7.

    **\see
    *\see CyU3PEpConfig_t
 */
extern CyU3PReturnStatus_t
CyU3PSetEpConfig (
        uint8_t ep,                     /**< Endpoint number to be configured. */
        CyU3PEpConfig_t *epinfo         /**< Properties to associate with the endpoint. */
        );

/** \brief Set the maximum packet size for a USB endpoint.

    <B>Description</B>\n
    The DMA channels associated with USB endpoints on the FX3 device can be
    configured to have buffers of any user specified size (up to 64 KB). The FX3
    device will allow multiple packets worth of data to be collected into a
    single DMA buffer as long as all of the packets are full packets. Any short
    packet arriving from USB will cause a DMA buffer to be wrapped up and committed
    to the consumer.

    This API is used to set the maximum packet size that applies to the current
    endpoint. This needs to be set by the application based on the active USB
    connection speed, if there is a need to collect multiple data packets into a
    buffer. By default, this is set to the maximum packet size computed based on
    the user specified CyU3PEpConfig_t.pcktSize value, the USB connection speed and
    endpoint type.

    <B>Note</B>\n
    Please note that the maximum packet size will be automatically adjusted to the
    connection speed when the USB connection is first detected. If this value is to
    be over-ridden, this should be done after the enumeration is complete. One way
    to do this is by calling this function on receiving a SET_CONFIGURATION setup
    callback or CY_U3P_USB_EVENT_SETCONF event.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the endpoint or size specified is invalid.

    **\see
    *\see CyU3PSetEpConfig
    *\see CyU3PUsbSetEpPktMode
 */
CyU3PReturnStatus_t
CyU3PSetEpPacketSize (
        uint8_t ep,                     /**< Endpoint number to be configured. */
        uint16_t maxPktSize             /**< Maximum packet size for endpoint in bytes. */
        );

/** \brief Select whether a OUT endpoint will function in packet (one buffer per packet) mode or not.

    <B>Description</B>\n
    USB OUT endpoints on the FX3 device are setup by default to collect multiple full data packets
    into a single DMA buffer (size permitting). The DMA buffer into which data is received is wrapped
    up and made available to the consumer only if it is filled up, or if a short packet is received
    on the endpoint. The endpoint can also be configured for packet mode, where each DMA buffer will
    only hold one packet worth of data (regardless of the actual packet size) and gets wrapped up
    as soon as any data packet is received.

    This API is used to switch the USB OUT endpoint between packet mode and transfer mode as required.
    This API only operates on non-control OUT endpoints.

    **\see
    *\see CyU3PSetEpPacketSize

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the endpoint mode was updated as requested.\n
    * CY_U3P_ERROR_NOT_STARTED if the USB driver has not been started.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the endpoint specified is invalid.
 */
CyU3PReturnStatus_t
CyU3PUsbSetEpPktMode (
        uint8_t  ep,                    /**< Endpoint number to be configured. */
        CyBool_t pktMode                /**< Whether the endpoint will function in packet mode or not. */
        );

/** \brief Send data to the USB host via EP0-IN.

    <B>Description</B>\n
    This function is used to respond to USB control requests with an
    associated IN data transfer phase. The data in the buffer will be
    sent to the host in multiple packets of appropriate size as per the
    USB connection speed. Multiple calls of this function can be made
    to respond to a single control request as long as each call sends
    an integral number of full packets to the host. Any send call that
    results in a short packet will terminate the control transfer.

    If the data in the buffer ends in a full packet, a zero length packet
   (ZLP) should be sent to the host to terminate the control transfer.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - when the USB driver has not been started.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the buffer passed is NULL or a DMA failure occurs.\n
    * CY_U3P_XFER_CANCELLED - is returned if the transaction has already been cancelled.\n
    * CY_U3P_ERROR_TIMEOUT - if the scheduled data transfer is not completed within 5 seconds.\n
    * CY_U3P_ERROR_ABORTED - if the data transfer is aborted due to a USB reset or another control transfer.\n
    * CY_U3P_ERROR_DMA_FAILURE - if the data transfer fails due to a DMA error.

    **\see
    *\see CyU3PUSBSetupCb_t
    *\see CyU3PUsbGetEP0Data
    *\see CyU3PUsbAckSetup
    *\see CyU3PUsbStall
 */
extern CyU3PReturnStatus_t
CyU3PUsbSendEP0Data (
        uint16_t count,                 /**< The amount of data in bytes. */
        uint8_t *buffer                 /**< Pointer to buffer containing the data. */
        );

/** \brief Read data associated with a control-OUT transfer.

    <B>Description</B>\n
    This function is used to get the OUT data associated with a USB control
    transfer. The caller is responsible for ensuring that all of the data
    being sent by the host is retrieved. If the caller is not able to do
    this, the control request should be stalled using the CyU3PUsbStall API.

    Multiple calls of this function can be made to fetch all of the data
    associated with a single control transfer, as long as each of the partial
    calls fetches an integral number of full data packets from the host.

    If the control request is to be failed with a STALL handshake, the stall
    call has to be made before all of the OUT data has been read. The request
    will be completed with a positive ACK as soon as all of the OUT data has
    been received by the device.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - when the USB driver has not been started.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the buffer passed is NULL or a DMA failure occurs.\n
    * CY_U3P_XFER_CANCELLED - is returned if the transaction has already been cancelled.\n
    * CY_U3P_ERROR_TIMEOUT - if the scheduled data transfer is not completed within 5 seconds.\n
    * CY_U3P_ERROR_ABORTED - if the data transfer is aborted due to a USB reset or another control transfer.\n
    * CY_U3P_ERROR_DMA_FAILURE - if the data transfer fails due to a DMA error.

    **\see
    *\see CyU3PUsbSendEP0Data
    *\see CyU3PUsbAckSetup
    *\see CyU3PUsbStall
 */
extern CyU3PReturnStatus_t
CyU3PUsbGetEP0Data (
        uint16_t count,                 /**< The length of data to be read in bytes. This should not be greater than
                                             the size requested by the host. While all of the data sent by the host
                                             needs to be read out completely, it is possible to break the read into
                                             multiple CyU3PUsbGetEP0Data API calls. In such a case, the count should
                                             be a multiple of the EP0 max. packet size (64 bytes for USB 2.0 and
                                             512 bytes for USB 3.0). */
        uint8_t *buffer,                /**< Pointer to buffer where the data should be placed. */
        uint16_t *readCount             /**< Output parameter which will be filled with the actual size
                                             of data read. */
        );

/** \brief Clear (flush) all data buffers associated with a specified endpoint.

    <B>Description</B>\n
    This function is used to clear the contents of all data buffers associated with
    a specified endpoint. This functionality is typically used to get rid of stale
    data when handling a USB bus reset or recovering an error/stall condition on the
    endpoint.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - when the USB driver has not been started.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if invalid parameter is passed to function.

    **\see
    *\see CyU3PUsbStall
    *\see CyU3PUsbResetEp
    *\see CyU3PUsbResetEndpointMemories
 */
extern CyU3PReturnStatus_t
CyU3PUsbFlushEp (
        uint8_t ep                      /**< Endpoint number to be cleared. */
        );

/** \brief Resets the specified endpoint.

    <B>Description</B>\n
    This function is used to reset USB endpoints while functioning at Super speed. The
    reset clears error conditions on the endpoint logic and prepares the endpoing for data
    transfer.

    This function does nothing if called while in a USB 2.0 connection.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - when the endpoint number is invalid.

    **\see
    *\see CyU3PUsbStall
    *\see CyU3PUsbFlushEp
    *\see CyU3PUsbResetEndpointMemories
 */
extern CyU3PReturnStatus_t
CyU3PUsbResetEp (
        uint8_t ep                      /**< Endpoint number to be cleared. */
        );

/** \brief Map a socket to stream of the specified endpoint.

    <B>Description</B>\n
    This function is used to map the stream of the specified endpoint to
    the socket passed as a parameter, this can be done if the socket is
    not already mapped.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - when the USB driver has not been started.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if invalid parameter is passed to function.

    **\see
    *\see CyU3PSetEpConfig
    *\see CyU3PUsbChangeMapping
 */
extern CyU3PReturnStatus_t
CyU3PUsbMapStream (
        uint8_t ep,                     /**< Endpoint number for which the stream is to be mapped. */
        uint8_t socketNum,              /**< Socket number for mapping the stream. */
        uint16_t streamId               /**< StreamId to be mapped. */
                   );

/** \brief Map a socket to stream of the specified endpoint.

    <B>Description</B>\n
    This function is used to map the stream of the specified endpoint to the socket passed as a parameter,
    this can be done if the socket is not already mapped.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - when the USB driver has not been started.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if invalid parameter is passed to function.

    **\see
    *\see CyU3PSetEpConfig
    *\see CyU3PUsbMapStream
 */
extern CyU3PReturnStatus_t
CyU3PUsbChangeMapping (
        uint8_t ep,                     /**< Endpoint number to which the stream was mapped */
        uint8_t socketNum,              /**< Socket number to which the stream was mapped */
        CyBool_t remap,                 /**< CyTrue: if remapping is to be done */
        uint16_t newstreamId,           /**< Stream id of the new stream to be mapped to the socket */
        uint8_t newep                   /**< Endpoint number of the new stream to be mapped to the socket */
        );

/** \brief Get the connection speed at which USB is operating.

    <B>Description</B>\n
    This function is used to get the operating speed of the USB connection.

    <B>Return value</B>\n
    * Current USB connection speed.

    **\see
    * CyU3PUSBSpeed_t
    * CyU3PConnectState
    * CyU3PGetConnectState
 */
extern CyU3PUSBSpeed_t
CyU3PUsbGetSpeed (
        void);

/** \brief Configure USB block on FX3 to work off Vbatt power instead of Vbus.

    <B>Description</B>\n
    The USB block on the FX3 device can be configured to work off Vbus power or Vbatt power, with the
    Vbus power being the default setting. This function is used to enable/disable the Vbatt power input
    to the USB block.

    This API needs to be called before the CyU3PConnectState API is called.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - when the USB driver has not been started.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - when the API is called after CyU3PConnectState has been called.

    **\see
    *\see CyU3PUsbStart
    *\see CyU3PConnectState
 */
extern CyU3PReturnStatus_t
CyU3PUsbVBattEnable (
        CyBool_t enable                 /**< CyTrue: Work off Vbatt, CyFalse: Work off Vbus. */
        );

/** \brief Function that returns some properties of the USB device.

    <B>Description</B>\n
    This function is used to retrieve properties of the USB device such as Device address,
    current ITP timestamp or frame number etc.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - when the USB driver has not been started.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - when the USB device connection is not active.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - when the property type being queried is invalid.

    **\see
    *\see CyU3PUsbDevProperty
 */
extern CyU3PReturnStatus_t
CyU3PUsbGetDevProperty (
        CyU3PUsbDevProperty  type,              /**< The type of property to be queried. */
        uint32_t            *buf                /**< Buffer into which the property value is copied. The format of
                                                     the data in the buffer depends on the property being queried.
                                                     See CyU3PUsbDevProperty for details on how the property data
                                                     is returned. */
        );

/** \brief Function to send an ERDY TP to a USB 3.0 host.

    <B>Description</B>\n
    This function allows the firmware to generate an ERDY TP for a specified endpoint. Under
    normal operation, ERDY TPs are automatically generated by the FX3 device as and when
    required. This function is provided to support exceptional cases where the firmware
    application needs to generate a specific ERDY TP.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - if the USB driver has not been started.\n
    * CY_U3P_ERROR_OPERN_DISABLED - if the USB connection is currently not in USB 3.0 mode.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the endpoint specified is invalid.

    **\see
    *\see CyU3PUsbSendNrdy
 */
extern CyU3PReturnStatus_t
CyU3PUsbSendErdy (
        uint8_t  ep,                    /**< Endpoint for which the ERDY TP is to be generated. Bit 7 of the endpoint
                                             indicates direction. */
        uint16_t bulkStream             /**< Stream number for which the ERDY TP is to be generated. Is only valid
                                             for stream enabled bulk endpoints and is ignored otherwise. */
        );

/** \brief Function to send an NRDY TP to a USB 3.0 host.

    <B>Description</B>\n
    This function allows the firmware to generate an NRDY TP for a specified endpoint. Under
    normal operation, NRDY TPs are automatically generated by the FX3 device as and when
    required. This function is provided to support exceptional cases where the firmware
    application needs to generate a specific NRDY TP.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - if the USB driver has not been started.\n
    * CY_U3P_ERROR_OPERN_DISABLED - if the USB connection is currently not in USB 3.0 mode.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the endpoint specified is invalid.

    **\see
    *\see CyU3PUsbSendErdy
 */
extern CyU3PReturnStatus_t
CyU3PUsbSendNrdy (
        uint8_t  ep,                    /**< Endpoint for which the NRDY TP is to be generated. Bit 7 of the endpoint
                                             indicates direction. */
        uint16_t bulkStream             /**< Stream number for which the NRDY TP is to be generated. Is only valid
                                             for stream enabled bulk endpoints and is ignored otherwise. */
        );

/** \brief Function to send an ACK TP to a USB 3.0 host.

    <B>Description</B>\n
    This function allows the firmware to generate an ACK TP for the specified endpoint. This function
    is only provided for debug and testing purposes, and should be used with caution.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the call is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - if the USB driver has not been started.\n
    * CY_U3P_ERROR_OPERN_DISABLED - if the USB connection is currently not in USB 3.0 mode.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the endpoint specified is invalid.

    **\see
    *\see CyU3PUsbSendErdy
 */
extern CyU3PReturnStatus_t
CyU3PUsbSendAckTP (
        uint8_t  ep,                    /**< Endpoint for which the ACK TP is to be generated. Bit 7 of the endpoint
                                             indicates direction. */
        uint8_t  nump,                  /**< NUMP (Number of Packets) value to associate with the TP. Please refer
                                             to USB specification for meaning and interpretation. */
        uint16_t bulkStream             /**< Stream number for which the ACK TP is to be generated. Is only valid
                                             for stream enabled bulk endpoints and is ignored otherwise. */
        );

/** \brief Function to get the current power state of the USB 3.0 link.

    <B>Description</B>\n
    This function is used to get the current power state of the USB 3.0 link. The actual
    link state will be returned through the mode_p parameter if the link is in any of the
    Ux states. If the link is in any of the other LTSSM states, CyU3PUsbLPM_Unknown will
    be returned. An error will be returned if the USB 3.0 connection is not enabled.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - the link state has been successfully retrieved.\n
    * CY_U3P_ERROR_NOT_STARTED - if the USB driver has not been started.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the USB connection is not active.\n
    * CY_U3P_ERROR_OPERN_DISABLED - if the USB connection is not in 3.0 mode.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if a null pointer is passed in as the mode_p parameter.

    **\see
    *\see CyU3PUsbLinkPowerMode
    *\see CyU3PUsbSetLinkPowerState
 */
extern CyU3PReturnStatus_t
CyU3PUsbGetLinkPowerState (
        CyU3PUsbLinkPowerMode *mode_p   /**< Return parameter that will be filled in with the current power state. */
        );

/** \brief Function to request a device entry into one of the U0, U1 or U2 link power modes.

    <B>Description</B>\n
    This function is used to request the FX3 USB driver to place the USB 3.0 link in a
    desired (U0, U1 or U2) power state. The U3 power state is not supported because U3
    entry can only be triggered by the host. The request to move into U1 or U2 will only
    be allowed while the link is currently in the U0 state.  The request to move into U0
    will be allowed while the link is in the U1, U2 or U3 states.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - when the requested link power state switch request has been initiated.\n
    * CY_U3P_ERROR_NOT_STARTED - if the USB driver has not been started.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the USB connection is not active.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the link state specified is invalid.\n
    * CY_U3P_ERROR_OPERN_DISABLED - if the USB connection is not in USB 3.0 mode or if the current
    link power state does not allow this transition.

    **\see
    *\see CyU3PUsbLinkPowerMode
    *\see CyU3PUsbGetLinkPowerState
    *\see CyU3PUsbLPMEnable
    *\see CyU3PUsbLPMDisable
 */
extern CyU3PReturnStatus_t
CyU3PUsbSetLinkPowerState (
        CyU3PUsbLinkPowerMode link_mode         /**< Desired link power state. */
        );

/** \brief Function to enable/disable notification of SOF/ITP events to the application.

    <B>Description</B>\n
    Start Of Frame (SOF) and Isochronous Timestamp Packet (ITP) packets are sent by a USB host to connected devices
    on a periodic basis (every 125 us for high speed and super speed USB connections). While some applications may
    require notifications when these packets are received, they may cause unnecessary overhead in many other cases.
    This function is used to enable/disable notifications of SOF/ITP events from the FX3 firmware library to the
    application. These events are kept disabled by default and are sent through the regular USB event callback
    function.

    <B>Note</B>\n
    The SOF/ITP event notification is sent to the application in interrupt context so as to reduce latencies.
    Performing time consuming operations in these callbacks is therefore not recommended.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the event change was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - if the USB driver has not been started.

    **\see
    *\see CyU3PUsbEventType_t
    *\see CyU3PUSBEventCb_t
 */
extern CyU3PReturnStatus_t
CyU3PUsbEnableITPEvent (
        CyBool_t enable                 /**< Whether SOF/ITP event notification should be enabled. */
        );

/** \brief Trigger USB 2.0 Remote Wakeup signalling to the host.

    <B>Description</B>\n
    Self powered USB 2.0 devices which have been suspended can notify the host that they would like
    to be active again, by using remote wakeup signalling. This API call can be used to trigger remote
    wakeup signalling on the USB 2.0 pins of the FX3 device.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the remote wakeup signalling was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - if the USB driver has not been started.\n
    * CY_U3P_ERROR_OPERN_DISABLED - if the USB connection is not 2.0, is not suspended or remote wakeup is disabled.
 */
extern CyU3PReturnStatus_t
CyU3PUsbDoRemoteWakeup (
        void);

/** \brief Function to force the USB 2.0 device connection to full speed.

    <B>Description</B>\n
    When the FX3 is functioning as a USB device, the speed selection between super speed and other (2.0) speeds
    can be done through the CyU3PConnectState API call. On a 2.0 connection, the device uses the highest speed
    that the host supports; and this will be high speed in most cases. This API can be used to force the device
    to connect as a full-speed device instead of a high-speed device, and also to re-enable high speed operation.

    <B>Note</B>\n
    This API only prevents FX3 from connecting as a high speed device. If the ssEnable parameter to the
    CyU3PConnectState() API is set to True, the device may still connect as a super-speed device.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the control operation was completed successfully.\n
    * CY_U3P_ERROR_NOT_STARTED - if the USB driver has not been started.\n
    * CY_U3P_ERROR_OPERN_DISABLED - if the USB connection has already been enabled.
 */
extern CyU3PReturnStatus_t
CyU3PUsbForceFullSpeed (
        CyBool_t enable                 /**< Whether to enable/disable the forced full speed operation. */
        );

/** \brief Function to send a DEV_NOTIFICATION Transaction Packet to the host.

    <B>Description</B>\n
    This API allows the user to send DEV_NOTIFICATION Transaction packets to the USB 3.0 host.
    The Notification Type and Notification Type Specific fields are accepted as parameters. None
    of the parameters are validated by the API, so the caller needs to ensure validity of these
    values.

    <B>Note</B>\n
    This API has to be called by the user after ensuring that the link is in U0 state. If the
    link is in U1/U2 and a TP has to be sent, the CyU3PUsbSetLinkPowerState API should be called
    first to trigger a recovery to U0.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the DEV_NOTIFICATION TP was successfully send to the host.\n
    * CY_U3P_ERROR_NOT_STARTED - if the USB driver has not been started.\n
    * CY_U3P_ERROR_OPERN_DISABLED - if the USB connection is not in Super Speed mode or if the link is not in U0 state.
 */
extern CyU3PReturnStatus_t
CyU3PUsbSendDevNotification (
        uint8_t  notificationType,      /**< Notification type - No validation is performed by the API. */
        uint32_t param0,                /**< Notification Type Specific Data - DWORD1[31:8]. */
        uint32_t param1                 /**< Notification Type Specific Data - DWORD2[31:0]. */
        );

/** \brief Function to prevent FX3 from entering U1/U2 states.

    <B>Description</B>\n
    The USB driver in FX3 firmware runs a state machine that determines whether entry to U1/U2 low power states
    is to be allowed. At most times, this decision is made automatically by the FX3 device itself based on whether
    it has any pending packets to go out. At other times, the decision is made by the firmware based on whether a
    Force_LINKPM_ACCEPT command has been received, and also the amount of time that has elapsed since the last exit
    from U1/U2.

    This function allows the user to override the state machine, and ensure that entry to U1/U2 states will be
    systematically denied by FX3 until the CyU3PUsbLPMEnable call is made.

    This call also disables the acceptance of LPM-L1 entry requests when FX3 is functioning as a Hi-Speed or
    a full speed device.

    <B>Note</B>\n
    Indiscriminate use of this function can result in USB compliance test failures. Please ensure that the
    LPM functionality is enabled every time an USB connect or reset event is received. This can be disabled
    again after ensuring that the device is functioning in a performance critical mode.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - if the USB driver has not been started.\n
    * CY_U3P_ERROR_OPERN_DISABLED - if the USB connection has not been enabled.

    **\see
    *\see CyU3PUsbLPMEnable

 */
extern CyU3PReturnStatus_t
CyU3PUsbLPMDisable (
        void);

/** \brief Function to re-enable automated handling of U1/U2 state entry.

    <B>Description</B>\n
    This function is used to re-enable the USB driver state machine that governs the handling of U1/U2 requests
    from the USB host. This function removes the override that is specified using the CyU3PUsbLPMDisable call.

    This function also enables the acceptance of LPM-L1 entry requests when FX3 is functioning as a Hi-Speed or
    a full speed device.

    <B>Note</B>\n
    It is expected that this call is made every time the application receives a USB connect or reset event, if a
    CyU3PUsbLPMDisable call has been made previously.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the operation is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - if the USB driver has not been started.\n
    * CY_U3P_ERROR_OPERN_DISABLED - if the USB connection has not been enabled.

    **\see
    *\see CyU3PUsbLPMDisable
 */
extern CyU3PReturnStatus_t
CyU3PUsbLPMEnable (
        void);

/** \brief Function to enable/disable switching control back to the FX3 2-stage bootloader.

    <B>Description</B>\n
    This function is used to enabled/disable the switching of control back to the FX3
    2-stage bootloader. The function is expected to be called prior to CyU3PUsbJumpBackToBooter().

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PUsbJumpBackToBooter
    *\see CyU3PUsbStart
 */
extern void
CyU3PUsbSetBooterSwitch (
        CyBool_t enable         /**< CyTrue - Enable switching to booter; CyFalse - Disable switching to booter. */
        );

/** \brief Function to check the version of the boot firmware that transferred control to this firmware image.

    <B>Description</B>\n
    The full FX3 firmware image could be loaded directly by the FX3 ROM boot-loader, or by a firmware
    application using the boot firmware library. This API can be used to fetch the version number of
    the boot firmware library that loaded this firmware image.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the boot firmware version could be identified and returned.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if valid pointers are not provided for the Returns.\n
    * CY_U3P_ERROR_FAILURE if the boot firmware version could not be identified. This can happen if the
    firmware application was loaded directly by the boot loader, or if the noReenum option was not used.
 */
extern CyU3PReturnStatus_t
CyU3PUsbGetBooterVersion (
        uint8_t *major_p,               /**< Return parameter to be filled with major version of boot firmware. */
        uint8_t *minor_p,               /**< Return parameter to be filled with minor version of boot firmware. */
        uint8_t *patch_p                /**< Return parameter to be filled with patch level of boot firmware. */
        );

/** \brief Function to transfer control back to the FX3 2-stage bootloader.

    <B>Description</B>\n
    This function is used to transfer control back to the FX3 2-stage bootloader.
    The caller is expected to do cleanup of modules other than USB prior to this call.
    D-Cache needs to be cleaned before calling this function. The entry address for
    the booter firmware can be obtained by using the verbose option of the elf2img
    converter tool.

    It is expected that all Serial Peripheral blocks are turned OFF before this function
    is called. Only the GPIO block can be left ON, if the booter had previously been
    configured to leave the GPIO block ON.

    <B>Return value</B>\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - If this function is called before the serial peripherals are turned off.\n
    * CY_U3P_ERROR_OPERN_DISABLED - If the FX3 2-stage booter doesn't support switching back.\n
    * Otherwise this is a non-returning call.

    **\see
    *\see CyU3PUsbSetBooterSwitch
 */
extern CyU3PReturnStatus_t
CyU3PUsbJumpBackToBooter (
        uint32_t address        /**< Address of the FX3 2-stage bootloader's entry point. */
        );

/** \brief Function to initiate logging of USB state changes into a circular buffer.

    <B>Description</B>\n
    While debugging USB connection/transfer failures with FX3, it is useful to have a
    detailed log of the USB related state changes and events. This function is used to
    register a memory buffer into which the state change information will be logged by
    the USB driver. The buffer would be treated as a circular buffer into which the
    data is continuously logged.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PUsbAddToEventLog
 */
extern void
CyU3PUsbInitEventLog (
        uint8_t  *buffer,               /**< Pointer to memory buffer into which events are to be logged. */
        uint32_t  bufSize               /**< Size of memory buffer in bytes. */
        );

/** \brief Get the current event log buffer index.

    <B>Description</B>\n
    This function is used to get the current write location within the USB event log
    buffer. This can be used to identify the most recent values that have been added
    to the buffer.

    <B>Return value</B>\n
    * Current buffer index

    **\see
    *\see CyU3PUsbInitEventLog
 */
extern uint16_t
CyU3PUsbGetEventLogIndex (
        void);

/** \brief Prepare all enabled USB device endpoints for data transfer.

    <B>Description</B>\n
    This function prepares all of the enabled USB endpoints on the FX3 device for data transfer
    at the current link operating speed. This sets the maximum packet size for the endpoint
    based on the curSpeed parameter, and clears the sequence numbers, data toggled and any STALL
    conditions associated with these endpoints. This function also sets up the endpoint memory
    block on the FX3 device to remove the possibility of data underruns.

    This API is called internally by the USB driver in response to USB connect, reset and
    SET_CONFIGURATION events; and does not generally need to be called directly by the user
    application.

    <B>Note</B>\n
    The caller of this function is expected to determine the current link operating speed,
    and then call this API with the appropriate parameter.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PUsbGetSpeed
    *\see CyU3PSetEpPacketSize
    *\see CyU3PUsbStall
 */
extern void
CyU3PUsbEpPrepare (
        CyU3PUSBSpeed_t curSpeed        /**< Current USB connection speed. */
        );

/** \brief Reset and re-initialize the endpoint memory block on FX3.

    <B>Description</B>\n
    Some transfer error conditions can leave the endpoint memories on FX3 in a bad state. This API
    is used to reset reset and re-initialize the memory block, so that USB data transfers can resume.
    This API should only be used when the application is recovering from an error condition, such
    as a transfer stall due to backflow from the USB host.

    Please note that this API leaves the sequence numbers of all endpoints unaffected. If this is being
    done as part of a EP reset handling, the sequence number should be explicitly cleared using
    the CyU3PUsbStall API.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS in case of successful reset and re-initialization.\n
    * CY_U3P_ERROR_NOT_STARTED if the USB block has not been started.
 */
extern CyU3PReturnStatus_t
CyU3PUsbResetEndpointMemories (
        void);

/** \brief Configure the USB DMA interface to continuously pre-fetch data.

    <B>Description</B>\n
    Applications that use multiple USB IN endpoints and have high data rate may run into
    data corruption errors due to an underrun of data on the RAM to USB endpoint path. This
    API is used to configure the USB DMA interface to pre-fetch the data from RAM more
    aggressively, so as to prevent this kind of data corruption.

    Receiving a CY_U3P_USB_EVENT_EP_UNDERRUN event from the USB driver is an indication
    that this API should be called by the application.

    <B>Return value</B>\n
    * None

    **\see
    *\see CY_U3P_USB_EVENT_EP_UNDERRUN
 */
extern void
CyU3PUsbEnableEPPrefetch (
        void);

/** \brief Function to get the number of USB 3.0 PHY and LINK error counts detected by FX3.

    <B>Description</B>\n
    The FX3 device keeps track of the number of USB 3.0 PHY and LINK errors encountered
    during device operation. This API can be used to query the number of PHY and LINK
    errors detected since the last query was completed. Both error counts are cleared to
    zero whenever a query is performed.

    The PHY errors tracked by FX3 are:\n
    * 8b/10b Decode errors.\n
    * Elastic buffer overflow/underflow errors.\n
    * CRC errors.\n
    * Training sequence errors.\n
    * PHY lock loss.

    The LINK errors tracked by FX3 are:\n
    * Header packet ACK timeout.\n
    * Link credit timeout.\n
    * Missing LGOOD_x / LCRD_x error.\n
    * Tx/Rx header sequence number errors.\n
    * Link power management timeout (missing LAU/LXU).\n
    * Link header sequence number / credit advertisement timeout.\n
    * Link header or LGO_x received before sequence number / credit advertisement.

    Please note that both error counters saturate at a value of 0xFFFF.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the query API is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if valid pointers are not provided.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE if the 3.0 connection is not active
 */
extern CyU3PReturnStatus_t
CyU3PUsbGetErrorCounts (
        uint16_t *phy_err_cnt,          /**< Return parameter to be filled with phy error count. */
        uint16_t *lnk_err_cnt           /**< Return parameter to be filled with link error count. */
        );

/** \brief Function to enable burst mode operation for a USB endpoint.

    <B>Description</B>\n
    Under normal operation, the FX3 device will only send data from one DMA buffer as part of
    one burst transaction on an USB IN endpoint. In some cases, this constraint may limit the
    data transfer performance achieved. This limitation can be fixed by configuring the endpoints
    for burst mode, where the device will be combine data from multiple DMA buffers into a single
    burst transaction. This API is used to enable/disable burst mode on an USB endpoint.

    <B>Note</B>\n
    Setting up burst mode is not recommended for endpoints which are likely to transfer
    a significant number of ZLPs or short packets. This should ideally be used for cases where
    most of the packets transferred will be full packets.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the configuration was updated as required.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the endpoint specified is invalid.
 */
extern CyU3PReturnStatus_t
CyU3PUsbEPSetBurstMode (
        uint8_t  ep,                    /**< The endpoint to be configured. */
        CyBool_t burstEnable            /**< Whether burst mode is to be enabled or not. */
        );

/** \brief Set the Tx amplitude range for the USB 3.0 signals.

    <B>Description</B>\n
    This API sets the Tx amplitude used by FX3 on the USB 3.0 interface. Please use this API with caution.
    The device has only been tested to work properly under the default swing setting of 0.9V (swing value
    set to 90). This API is expected to be called before calling the CyU3PConnectState() API to enable USB
    connections.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the setting is updated as expected.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the swing value specified is out of range.
 */
extern CyU3PReturnStatus_t
CyU3PUsbSetTxSwing (
        uint32_t swing                  /**< TX Amplitude swing in 10 mV units. Should be less than 1.28V. */
        );

/** \brief Set the Tx de-emphasis setting for the USB 3.0 signals.

    <B>Description</B>\n
    This API sets the Tx de-emphasis level used by FX3 on the USB 3.0 interface. Please use this API with caution.
    This API is expected to be called before calling the CyU3PConnectState() API to enable USB connections.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the setting is updated as expected.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the value specified is out of range.
 */
extern CyU3PReturnStatus_t
CyU3PUsbSetTxDeemphasis (
        uint32_t value                  /**< TX De-emphasis value. Should be less than 0x1F. Default value is 0x11. */
        );

/** \brief Disable Spread-Spectrum Clocking support in the USB block.

    <B>Description</B>\n
    The USB 3.0 block on FX3 has Spread-Spectrum Clocking support enabled by default. This API is used to
    disable SSC support on the FX3 device.
 */
extern void
CyU3PUsbSSCDisable (
        void);

/** \brief Get the current sequence number for an endpoint.

    <B>Description</B>\n
    This function is used to query the current sequence number for a USB endpoint. This API is only
    valid while the device is functioning at USB 3.0.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the query was successful and the sequence number is returned.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the ep or the seqnum_p parameter is invalid.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE if the USB link is not at SuperSpeed.

    **\see
    *\see CyU3PUsbSetEpSeqNum
 */
extern CyU3PReturnStatus_t
CyU3PUsbGetEpSeqNum (
        uint8_t  ep,                    /**< Endpoint to be queried. */
        uint8_t *seqnum_p               /**< Return parameter to be filled with sequence number. */
        );

/** \brief Set the active sequence number for an endpoint.

    <B>Description</B>\n
    This function is used to set the active sequence number for a USB 3.0 endpoint to a desired
    value.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the update operation is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the ep or seqnum parameter is invalid.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE if the USB link is not at SuperSpeed.

    **\see
    *\see CyU3PUsbGetEpSeqNum
 */
extern CyU3PReturnStatus_t
CyU3PUsbSetEpSeqNum (
        uint8_t ep,                     /**< Endpoint to be updated. */
        uint8_t seqnum                  /**< Sequence number to be selected. */
        );

/** \brief Enable/Disable VBus detection in FX3 firmware.

    <B>Description</B>\n
    When the FX3 device is functioning as a USB device, it performs Vbus detection by default;
    and will connect to the host only when VBus voltage is seen to be above 4.1 V. This API is
    used to configure the FX3 device to ignore the VBus input.
   
    In some cases, the VBus voltage may be connected to the VBatt input of FX3 through a regulator.
    The firmware can be configured to automatically detect the VBatt input and enable/disable the
    USB connectivity accordingly. The useVbatt parameter can be set to select this mode of operation.
   
    If the VBus signal from the USB connector is not connected to FX3 or connected to a GPIO,
    the user is expected to enable/disable the USB connection at appropriate times. In this case;
    both the enable and useVbatt parameters should be set to CyFalse, and the CyU3PConnectState
    API should be called directly by the user application.

    <B>Note</B>\n
    The USB block on the FX3 device draws power from the VBus supply by default. If the voltage source
    connected on the VBus pin is unable to provide enough power for this, the user should use the
    CyU3PUsbVBattEnable API to have the device draw power from VBatt instead.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the configuration is updated properly.\n
    * CY_U3P_ERROR_NOT_STARTED if the USB block is not started.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE if the USB connection is already enabled.

    **\see
    *\see CyU3PConnectState
    *\see CyU3PUsbVBattEnable
 */
extern CyU3PReturnStatus_t
CyU3PUsbControlVBusDetect (
        CyBool_t enable,                /**< Whether VBus detection in firmware is enabled or disabled. */
        CyBool_t useVbatt               /**< Whether VBatt should be used as an indication of VBus validity. This
                                             parameter is only relevant when the enable parameter is set to false. */
        );

/** \brief Enable/Disable USB 2.0 device operation on FX3 device.

    <B>Description</B>\n
    By default, the FX3 device is configured to attempt both USB 3.0 and 2.0 connections to a USB
    host. The USB driver in the firmware automatically attempts a hi-speed or full speed connection,
    if the SuperSpeed connection fails to go through link training.

    Some designs may use the FX3 device in USB 3.0 mode alone, and require that an external controller
    be used in USB 2.0 mode. This API can be used to modify the driver behavior to suppress the USB 2.0
    device operation on FX3. USB events are provided to notify the user that the 3.0 connection has
    failed, and a 2.0 connection needs to be attempted. The CyU3PConnectState API should be called
    repeatedly to retry the USB 3.0 connection.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the configuration is updated properly.\n
    * CY_U3P_ERROR_NOT_STARTED if the USB block has not been started.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE if the USB connection has already been enabled.
 */
extern CyU3PReturnStatus_t
CyU3PUsbControlUsb2Support (
        CyBool_t enable                 /**< Whether USB 2.0 support on FX3 is enabled (default = enabled). */
        );

/** \brief Resume USB 2.0 link from LPM-L1 suspend.

    <B>Description</B>\n
    The USB host can request the USB Hi-Speed link to be placed in the LPM-L1 state during device
    operation. The FX3 device can initiate an exit from the L1 state using resume signaling, when
    it is ready with data. This function is used to initate L1 exit from the FX3 side.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the resume is initiated, or if the link is already in L0 state.\n
    * CY_U3P_ERROR_NOT_STARTED if the USB block has not been started.\n
    * CY_U3P_ERROR_NOT_CONFIGURED if the USB connection has not been enabled.\n
    * CY_U3P_ERROR_OPERN_DISABLED if a USB 3.0 connection is active.

    **\see
    *\see CyU3PUsbLPMDisable
 */
extern CyU3PReturnStatus_t
CyU3PUsb2Resume (
        void);

/** \brief Set the number of maximum sized packets that an OUT endpoint buffer can accommodate.

    <B>Description</B>\n
    Some USB endpoints (isochronous or interrupt) may have maximum packet sizes that are not aligned
    to 512 or 1024 bytes. When one such OUT endpoint makes use of DMA buffers that can hold multiple
    packets worth of data, the space availability may not be calculated correctly by the FX3 device.\n

    For example, if there is an ISOCHRONOUS endpoint with maximum packet size of 128 bytes and DMA
    buffers of 384 bytes are used with this endpoint; each buffer can actually accommodate 3 packets
    of data. However, FX3 will calculate this as 2 packets only and may end up dropping the third
    packet if all three packets are received in a single micro-frame.\n

    In such cases, the count of packets that can be accommodated in one DMA buffer can be statically
    programmed by the user. This API allows the user to set this configuration. The value specified
    here should be calculated based on the DMA buffer size and the maximum packet size for the endpoint.
    This should be called once the SET_CONFIGURATION or SET_INTERFACE request that enables the interface
    has been received.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the configuration is updated properly.
    * CY_U3P_ERROR_NOT_STARTED if the USB block has not been started.
    * CY_U3P_ERROR_BAD_ARGUMENT if the endpoint specified is not valid, or has not been enabled.

    **\see
    *\see CyU3PSetEpConfig
    *\see CyU3PSetEpPacketSize
 */
extern CyU3PReturnStatus_t
CyU3PUsbEpSetPacketsPerBuffer (
        uint8_t epNum,                          /**< Endpoint to be updated. Should be a non-zero OUT endpoint. */
        uint8_t numPkts                         /**< Number of full packets that a DMA buffer can hold. Should be
                                                     in the 0-31 range. Setting numPkts to 0 will cause FX3 to
                                                     revert to the default buffer size calculation. */
        );

/** \brief Select endpoints that should not be suspended to allow EP0 transfers to complete.

    <B>Description</B>\n
    When FX3 is operating in High Speed mode, other IN endpoints are suspended while any EP0-IN data
    transfers are being handled. This scheme is implemented as a work-around to ensure that no data
    corruption occurs. However, this work-around can affect the on-time data transfer on periodic
    endpoints. This API can be used to specify that some IN endpoints should be exempted from the
    implementation of this suspend work-around.

    <B>Note</B>\n
    This list of endpoints for whom DMA suspend is disabled is cleared when FX3 goes through a USB
    bus reset. Therefore, the setting needs to be done afresh every time the device goes through
    SET_CONFIGURATION.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the configuration is updated.
    * CY_U3P_ERROR_NOT_STARTED if the USB block has not been started.

    **\see
    *\see CyU3PUsbSendEP0Data
 */
extern CyU3PReturnStatus_t
CyU3PUsbSetEpSuspDisableMask (
        uint16_t noSuspMask                     /**< Bitmask representing IN endpoints that should not be suspended
                                                     while handling EP0-IN data transfers. */
        );

/** \brief Function to check whether the USB connection is still active.

    <B>Description</B>\n
    This function checks whether the USB connection is active by looking for SOF or ITP events.
    This will only work if the user ensures that the USB link is not in a low power mode.

    <B>Return value</B>\n
    * CyTrue if the connection is active.
    * CyFalse if the connection is not active.
 */
CyBool_t
CyU3PUibCheckConnection (
        void);

/** \brief Disable USB 3.0 electrical compliance mode support.

    <B>Description</B>\n
    The USB spec requires USB devices to check for USB 3.0 support on the upstream device/hub by
    checking for a valid termination on the SSTX (far side RX) pins. The spec defines a valid
    termination impedance of under 30 Ohms and an invalid (no USB 3.0 support) termination impedance
    above 25 KOhms.
    
    FX3 has a receiver detection threshold of approximately 150 Ohms, and will enter USB 3.0
    electrical compliance test mode if a host/hub offers impedance under 150 Ohms while USB 3.0
    is disabled. This behavior can be modified by calling this API to disable compliance test
    mode support. If compliance mode is disabled, FX3 will enter USB 2.0 link mode if the Polling.LFPS
    handshake times out.

    <B>Return value</B>\n
    * None
 */
extern void
CyU3PUsbLinkComplianceControl (
        CyBool_t isEnabled                      /**< Whether link compliance test mode is enabled. */
        );

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3USB_H_ */

/*[]*/

