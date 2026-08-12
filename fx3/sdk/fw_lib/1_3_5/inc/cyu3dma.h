/*
 ## Cypress FX3 Core Library Header (cyu3dma.h)
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

#ifndef _INCLUDED_CYU3DMA_H_
#define _INCLUDED_CYU3DMA_H_

#include "cyu3os.h"
#include "cyu3externcstart.h"

/** \file cyu3dma.h
    \brief DMA driver and API definitions for EZ-USB FX3.

    <B>Description</B>\n
    The FX3 device architecture includes a distributed DMA fabric that allows
    high performance data transfer between any of the external interfaces of
    the device. This DMA engine is a custom implementation that makes use of
    multiple registers and data structures for the data path management.

    The DMA driver in the FX3 firmware takes care of configuring the DMA data
    paths as desired and performs all runtime control operations such as interrupt
    handling. The FX3 library also provides a set of APIs through which the
    user application can set up and control DMA data paths.
 */

/**************************************************************************
 ******************************* Data types *******************************
 **************************************************************************/

/** \brief DMA socket IDs for all sockets in the device.

    <B>Description</B>\n
    This is a software representation of all sockets on the device.
    The socket ID has two parts: IP number and socket number. Each
    peripheral (IP) has a fixed ID. LPP is 0, PIB is 1, Storage port
    is 2, USB egress is 3 and USB ingress is 4.
    
    Each peripheral has a number of sockets. The LPP sockets are fixed
    and have to be used as defined. The PIB sockets 0-15 can be used as
    both producer and consumer, but the PIB sockets 16-31 are strictly
    producer sockets. The UIB sockets are defined as 0-15 producer and
    0-15 consumer sockets. The CPU sockets are virtual representations
    and have no backing hardware block.

    **\see
    *\see CyU3PDmaChannelConfig_t
    *\see CyU3PDmaMultiChannelConfig_t
    *\see CyU3PDmaChannelCreate
    *\see CyU3PDmaMultiChannelCreate
 */
typedef enum CyU3PDmaSocketId_t
{
    CY_U3P_LPP_SOCKET_I2S_LEFT = 0x0000,        /**< Left channel output to I2S port. */
    CY_U3P_LPP_SOCKET_I2S_RIGHT,                /**< Right channel output to I2S port. */
    CY_U3P_LPP_SOCKET_I2C_CONS,                 /**< Outgoing data to I2C slave. */
    CY_U3P_LPP_SOCKET_UART_CONS,                /**< Outgoing data to UART peer. */
    CY_U3P_LPP_SOCKET_SPI_CONS,                 /**< Outgoing data to SPI slave. */
    CY_U3P_LPP_SOCKET_I2C_PROD,                 /**< Incoming data from I2C slave. */
    CY_U3P_LPP_SOCKET_UART_PROD,                /**< Incoming data from UART peer. */
    CY_U3P_LPP_SOCKET_SPI_PROD,                 /**< Incoming data from SPI slave. */


    CY_U3P_PIB_SOCKET_0 = 0x0100,               /**< P-port socket number 0. */
    CY_U3P_PIB_SOCKET_1,                        /**< P-port socket number 1. */
    CY_U3P_PIB_SOCKET_2,                        /**< P-port socket number 2. */
    CY_U3P_PIB_SOCKET_3,                        /**< P-port socket number 3. */
    CY_U3P_PIB_SOCKET_4,                        /**< P-port socket number 4. */
    CY_U3P_PIB_SOCKET_5,                        /**< P-port socket number 5. */
    CY_U3P_PIB_SOCKET_6,                        /**< P-port socket number 6. */
    CY_U3P_PIB_SOCKET_7,                        /**< P-port socket number 7. */
    CY_U3P_PIB_SOCKET_8,                        /**< P-port socket number 8. */
    CY_U3P_PIB_SOCKET_9,                        /**< P-port socket number 9. */
    CY_U3P_PIB_SOCKET_10,                       /**< P-port socket number 10. */
    CY_U3P_PIB_SOCKET_11,                       /**< P-port socket number 11. */
    CY_U3P_PIB_SOCKET_12,                       /**< P-port socket number 12. */
    CY_U3P_PIB_SOCKET_13,                       /**< P-port socket number 13. */
    CY_U3P_PIB_SOCKET_14,                       /**< P-port socket number 14. */
    CY_U3P_PIB_SOCKET_15,                       /**< P-port socket number 15. */
    CY_U3P_PIB_SOCKET_16,                       /**< P-port socket number 16. */
    CY_U3P_PIB_SOCKET_17,                       /**< P-port socket number 17. */
    CY_U3P_PIB_SOCKET_18,                       /**< P-port socket number 18. */
    CY_U3P_PIB_SOCKET_19,                       /**< P-port socket number 19. */
    CY_U3P_PIB_SOCKET_20,                       /**< P-port socket number 20. */
    CY_U3P_PIB_SOCKET_21,                       /**< P-port socket number 21. */
    CY_U3P_PIB_SOCKET_22,                       /**< P-port socket number 22. */
    CY_U3P_PIB_SOCKET_23,                       /**< P-port socket number 23. */
    CY_U3P_PIB_SOCKET_24,                       /**< P-port socket number 24. */
    CY_U3P_PIB_SOCKET_25,                       /**< P-port socket number 25. */
    CY_U3P_PIB_SOCKET_26,                       /**< P-port socket number 26. */
    CY_U3P_PIB_SOCKET_27,                       /**< P-port socket number 27. */
    CY_U3P_PIB_SOCKET_28,                       /**< P-port socket number 28. */
    CY_U3P_PIB_SOCKET_29,                       /**< P-port socket number 29. */
    CY_U3P_PIB_SOCKET_30,                       /**< P-port socket number 30. */
    CY_U3P_PIB_SOCKET_31,                       /**< P-port socket number 31. */


    CY_U3P_SIB_SOCKET_0 = 0x0200,               /**< S-port socket number 0. */
    CY_U3P_SIB_SOCKET_1,                        /**< S-port socket number 1. */
    CY_U3P_SIB_SOCKET_2,                        /**< S-port socket number 2. */
    CY_U3P_SIB_SOCKET_3,                        /**< S-port socket number 3. */
    CY_U3P_SIB_SOCKET_4,                        /**< S-port socket number 4. */
    CY_U3P_SIB_SOCKET_5,                        /**< S-port socket number 5. */


    CY_U3P_UIB_SOCKET_CONS_0 = 0x0300,          /**< U-port output socket number 0. */
    CY_U3P_UIB_SOCKET_CONS_1,                   /**< U-port output socket number 1. */
    CY_U3P_UIB_SOCKET_CONS_2,                   /**< U-port output socket number 2. */
    CY_U3P_UIB_SOCKET_CONS_3,                   /**< U-port output socket number 3. */
    CY_U3P_UIB_SOCKET_CONS_4,                   /**< U-port output socket number 4. */
    CY_U3P_UIB_SOCKET_CONS_5,                   /**< U-port output socket number 5. */
    CY_U3P_UIB_SOCKET_CONS_6,                   /**< U-port output socket number 6. */
    CY_U3P_UIB_SOCKET_CONS_7,                   /**< U-port output socket number 7. */
    CY_U3P_UIB_SOCKET_CONS_8,                   /**< U-port output socket number 8. */
    CY_U3P_UIB_SOCKET_CONS_9,                   /**< U-port output socket number 9. */
    CY_U3P_UIB_SOCKET_CONS_10,                  /**< U-port output socket number 10. */
    CY_U3P_UIB_SOCKET_CONS_11,                  /**< U-port output socket number 11. */
    CY_U3P_UIB_SOCKET_CONS_12,                  /**< U-port output socket number 12. */
    CY_U3P_UIB_SOCKET_CONS_13,                  /**< U-port output socket number 13. */
    CY_U3P_UIB_SOCKET_CONS_14,                  /**< U-port output socket number 14. */
    CY_U3P_UIB_SOCKET_CONS_15,                  /**< U-port output socket number 15. */


    CY_U3P_UIB_SOCKET_PROD_0 = 0x400,           /**< U-port input socket number 0. */
    CY_U3P_UIB_SOCKET_PROD_1,                   /**< U-port input socket number 1. */
    CY_U3P_UIB_SOCKET_PROD_2,                   /**< U-port input socket number 2. */
    CY_U3P_UIB_SOCKET_PROD_3,                   /**< U-port input socket number 3. */
    CY_U3P_UIB_SOCKET_PROD_4,                   /**< U-port input socket number 4. */
    CY_U3P_UIB_SOCKET_PROD_5,                   /**< U-port input socket number 5. */
    CY_U3P_UIB_SOCKET_PROD_6,                   /**< U-port input socket number 6. */
    CY_U3P_UIB_SOCKET_PROD_7,                   /**< U-port input socket number 7. */
    CY_U3P_UIB_SOCKET_PROD_8,                   /**< U-port input socket number 8. */
    CY_U3P_UIB_SOCKET_PROD_9,                   /**< U-port input socket number 9. */
    CY_U3P_UIB_SOCKET_PROD_10,                  /**< U-port input socket number 10. */
    CY_U3P_UIB_SOCKET_PROD_11,                  /**< U-port input socket number 11. */
    CY_U3P_UIB_SOCKET_PROD_12,                  /**< U-port input socket number 12. */
    CY_U3P_UIB_SOCKET_PROD_13,                  /**< U-port input socket number 13. */
    CY_U3P_UIB_SOCKET_PROD_14,                  /**< U-port input socket number 14. */
    CY_U3P_UIB_SOCKET_PROD_15,                  /**< U-port input socket number 15. */


    CY_U3P_CPU_SOCKET_CONS = 0x3F00,            /**< Socket through which the FX3 CPU receives data. */
    CY_U3P_CPU_SOCKET_PROD                      /**< Socket through which the FX3 CPU produces data. */

} CyU3PDmaSocketId_t;

/** \brief List of DMA single channel types.

    <B>Description</B>\n
    A DMA channel aggregates all of the resources required to manage a unique data
    flow through the FX3 device. A DMA channel needs to be created before any DMA
    operation can be performed.

    In the normal case, each DMA channel has one producer socket through which data
    is received on the FX3 device; and one consumer socket through which data is sent
    out of the FX3 device. The DMA channels can be classified into multiple types
    based on the amount of firmware intervention and control that is required in the
    data flow.

    This enumeration lists the different types of DMA single (one to one) channels.
    All APIs that operate on these channels have have a CyU3PDmaChannel prefix.

    **\see
    *\see CyU3PDmaChannelCreate
 */
typedef enum CyU3PDmaType_t
{
    CY_U3P_DMA_TYPE_AUTO = 0,           /**< Auto mode DMA channel. */
    CY_U3P_DMA_TYPE_AUTO_SIGNAL,        /**< Auto mode with event signalling. */
    CY_U3P_DMA_TYPE_MANUAL,             /**< Manual mode DMA channel. */
    CY_U3P_DMA_TYPE_MANUAL_IN,          /**< Manual mode producer socket to CPU. */
    CY_U3P_DMA_TYPE_MANUAL_OUT,         /**< Manual mode CPU to consumer socket. */
    CY_U3P_DMA_NUM_SINGLE_TYPES         /**< Number of single DMA channel types. */

} CyU3PDmaType_t;

/** \brief List of DMA multi-channel types.

    <B>Description</B>\n
    In some cases, a simple one-to-one DMA channel is insufficient to handle the data flowing
    through the FX3 device. For example, a user may need to use multiple PIB (GPIF) sockets to
    collect the data coming from an image sensor without any data loss.  Special handling is
    required to combine the data from these sockets into a single USB endpoint for streaming.
    
    Such special DMA handling is achieved through the use of multi-channels. Different types of
    multi-channels are supported by the DMA manager, and this enumerated type lists them.
    Special APIs with a CyU3PDmaMultiChannel prefix need to be used for creating and managing
    DMA multi-channels.

    **\see
    *\see CyU3PDmaType_t
    *\see CyU3PDmaMultiChannelCreate
    *\see CyU3PDmaEnableMulticast
 */
typedef enum CyU3PDmaMultiType_t
{
    CY_U3P_DMA_TYPE_AUTO_MANY_TO_ONE = (CY_U3P_DMA_NUM_SINGLE_TYPES), /**< Auto mode many to one interleaved
                                                                           DMA channel. */
    CY_U3P_DMA_TYPE_AUTO_ONE_TO_MANY,   /**< Auto mode one to many interleaved DMA channel. */
    CY_U3P_DMA_TYPE_MANUAL_MANY_TO_ONE, /**< Manual mode many to one interleaved DMA channel. */
    CY_U3P_DMA_TYPE_MANUAL_ONE_TO_MANY, /**< Manual mode one to many interleaved DMA channel. */
    CY_U3P_DMA_TYPE_MULTICAST,          /**< Multicast mode with one producer and multiple consumers for the same
                                             buffer. This is a manual channel. Please note that the
                                             CyU3PDmaEnableMulticast API needs to be called before any
                                             multicast DMA channels are created. */
    CY_U3P_DMA_NUM_TYPES                /**< Number of DMA channel types. */
} CyU3PDmaMultiType_t;

/** \brief List of different DMA channel states.

    <B>Description</B>\n
    The following are the different states that a DMA channel can be in.
    All states until CY_U3P_DMA_NUM_STATES are channel states, where-as
    those after it are virtual state values returned on the GetStatus calls
    only.

    **\see
    *\see CyU3PDmaChannelGetStatus
 */
typedef enum CyU3PDmaState_t
{
    CY_U3P_DMA_NOT_CONFIGURED = 0, /**< DMA channel is unconfigured. This state occurs only 
                                        when using a stale/uninitialized channel structure. This channel
                                        state is set to this when a channel is destroyed. */
    CY_U3P_DMA_CONFIGURED,         /**< DMA channel has been configured successfully. The channel
                                        reaches this state through the following conditions:
                                        1. Channel is successfully created.
                                        2. Channel is reset.
                                        3. A finite transfer has been successfully completed.
                                           A GetStatus call in this case will return a virtual
                                           CY_U3P_DMA_XFER_COMPLETED state.
                                        4. One of the override modes have completed successfully.
                                           A GetStatus call in this case will return a virtual
                                           CY_U3P_DMA_SEND_COMPLETED or CY_U3P_DMA_RECV_COMPLETED state.
                                    */
    CY_U3P_DMA_ACTIVE,             /**< Channel has active transaction going on.
                                        This state is reached when a SetXfer call is invoked and the
                                        transfer is on-going. */
    CY_U3P_DMA_PROD_OVERRIDE,      /**< The channel is working in producer socket override mode.
                                        This state is reached when a SetupSend call is invoked and the
                                        transfer is on-going. */
    CY_U3P_DMA_CONS_OVERRIDE,      /**< Channel is working in consumer socket override mode.
                                        This state is reached when a SetupRecv call is invoked and the
                                        transfer is on-going. */
    CY_U3P_DMA_ERROR,              /**< Channel has encountered an error. This state is reached when
                                        a DMA hardware error is detected. */
    CY_U3P_DMA_IN_COMPLETION,      /**< Waiting for all data to drain out. This state is reached when
                                        transfer has completed from the producer side, but waiting for
                                        the consumer to drain all data. */
    CY_U3P_DMA_ABORTED,            /**< The channel is in aborted state. This state is reached when a Abort
                                        call is made. */
    CY_U3P_DMA_NUM_STATES,         /**< Number of states. This is not a valid state and is just a place holder. */
    CY_U3P_DMA_XFER_COMPLETED,     /**< This is a virtual state returned by GetStatus function. 
                                        The actual state is CY_U3P_DMA_CONFIGURED. This is just the value
                                        returned on GetStatus call after completion of finite transfer. */
    CY_U3P_DMA_SEND_COMPLETED,     /**< This is a virtual state returned by GetStatus function.
                                        The actual state is CY_U3P_DMA_CONFIGURED. This is just the value
                                        returned on GetStatus call after completion of producer override mode. */
    CY_U3P_DMA_RECV_COMPLETED      /**< This is a virtual state returned by GetStatus function.
                                        The actual state is CY_U3P_DMA_CONFIGURED. This is just the value
                                        returned on GetStatus call after completion of consumer override mode. */
} CyU3PDmaState_t;

/** \brief List of different DMA transfer modes.

    <B>Description</B>\n
    The following are the different types of DMA transfer modes. The default mode of operation
    is byte mode. The buffer mode is useful only when there are variable data sized transfers
    and when firmware handling is required after a finite number of buffers.

    **\see
    *\see CyU3PDmaChannelConfig_t
    *\see CyU3PDmaMultiChannelConfig_t
    *\see CyU3PDmaChannelCreate
    *\see CyU3PDmaChannelUpdateMode
    *\see CyU3PDmaMultiChannelUpdateMode
 */
typedef enum CyU3PDmaMode_t
{
    CY_U3P_DMA_MODE_BYTE = 0, /**< Transfer is based on byte count. This is the default mode of operation.
                                   The transfer count is done based on number of bytes received / sent. */
    CY_U3P_DMA_MODE_BUFFER,   /**< Transfer is based on buffer count. The transfer count is based on the 
                                   number of buffers generated or consumed. This is useful only when the
                                   data size is variable but there should be finite handling after N buffers. */
    CY_U3P_DMA_NUM_MODES      /**< Count of DMA modes. This is just a place holder and not a valid mode. */
} CyU3PDmaMode_t;

/** \brief List of DMA channel callback types.

    <B>Description</B>\n
    This type lists the various callback types that are supported by the DMA manager for
    various DMA channels. The user can register the combination of these events for which
    event notifications are required at channel creation time.

    **\see
    *\see CyU3PDmaCallback_t
    *\see CyU3PDmaChannelConfig_t
    *\see CyU3PDmaMultiChannelConfig_t
    *\see CyU3PDmaChannelCreate
    *\see CyU3PDmaMultiChannelCreate
 */
typedef enum CyU3PDmaCbType_t
{
    CY_U3P_DMA_CB_XFER_CPLT  = (1 << 0), /**< Transfer has been completed. This event is generated when
                                              a finite transfer queued with SetXfer is completed. */
    CY_U3P_DMA_CB_SEND_CPLT  = (1 << 1), /**< SendBuffer call has been completed. This event is generated
                                              when the data queued with SendBuffer has been successfully sent. */
    CY_U3P_DMA_CB_RECV_CPLT  = (1 << 2), /**< ReceiverBuffer call has been completed. This event is generated
                                              when data is received successfully on the producer socket. */
    CY_U3P_DMA_CB_PROD_EVENT = (1 << 3), /**< Buffer received from producer. This event is generated when
                                              a buffer is generated by the producer socket when a transfer
                                              is queued with SetXfer. */
    CY_U3P_DMA_CB_CONS_EVENT = (1 << 4), /**< Buffer consumed by the consumer. This event is generated when
                                              a buffer is sent out by the consumer socket when a transfer
                                              is queued with SetXfer. */
    CY_U3P_DMA_CB_ABORTED    = (1 << 5), /**< This event is generated when the Abort API is invoked. */
    CY_U3P_DMA_CB_ERROR      = (1 << 6), /**< This event is generated when the hardware detects an error. */
    CY_U3P_DMA_CB_PROD_SUSP  = (1 << 7), /**< This event is generated when the producer socket is suspended. */
    CY_U3P_DMA_CB_CONS_SUSP  = (1 << 8)  /**< This event is generated when the consumer socket is suspended. */
} CyU3PDmaCbType_t;

/** \brief List of DMA socket suspend options.

    <B>Description</B>\n
    The producer and consume sockets associated with a DMA channel can be suspended
    based on various triggers. Each of these suspend triggers/options behaves differently.

    <B>Note</B>\n
    In case of multi channels, only the single socket side can be suspended.
    For a many to one channels, the producer sockets cannot be suspended; and for
    one to many channels, the consumer sockets cannot be suspended.

    **\see
    *\see CyU3PDmaChannelCreate
    *\see CyU3PDmaChannelSetSuspend
    *\see CyU3PDmaChannelResume
    *\see CyU3PDmaMultiChannelCreate
 */
typedef enum CyU3PDmaSckSuspType_t
{
    CY_U3P_DMA_SCK_SUSP_NONE = 0,        /**< Socket will not be suspended. This option is used to remove any
                                              existing suspend triggers on the selected socket. */
    CY_U3P_DMA_SCK_SUSP_EOP,             /**< Socket will be suspended after handling any buffer with the EOP
                                              bit set. Typically the EOP bit will be set on any data buffers that
                                              are wrapped up on the PIB (GPIF) side through a COMMIT action.
                                              The DMA buffer with the EOP bit set would have been handled before
                                              the socket gets suspended, meaning that it is not possible to make
                                              changes to the contents of that data packet. This option can be
                                              applied to both producer and consumer sockets, and is sticky. */ 
    CY_U3P_DMA_SCK_SUSP_CUR_BUF,         /**< Socket will be suspended after the current buffer is completed.
                                              This can be used to suspend the socket at a defined point to be
                                              resumed later. This option is is valid for only the current buffer,
                                              and the socket option will change back to CY_U3P_DMA_SCK_SUSP_NONE
                                              on resumption. */
    CY_U3P_DMA_SCK_SUSP_CONS_PARTIAL_BUF /**< This option is valid only for consumer sockets, and allows the socket
                                              to be suspended before handling any partially filled (count < size)
                                              DMA buffer. This mode allows the user to make changes to the data
                                              before sending it out. Please note that the suspend option for the
                                              socket has to be changed to CY_U3P_DMA_SCK_SUSP_CUR_BUF or
                                              CY_U3P_DMA_SCK_SUSP_NONE when resuming the channel operation. */ 
} CyU3PDmaSckSuspType_t;

/** \brief DMA buffer data structure.

    <B>Description</B>\n
    The data structure is used to describe the status of a DMA buffer. It holds the
    address, size, valid data count, and the status information. This type is used
    in DMA callbacks and APIs to identify the properties of the data buffer to be
    transferred.

    **\see
    *\see CyU3PDmaCBInput_t
    *\see CyU3PDmaChannelGetBuffer
    *\see CyU3PDmaChannelSetupSendBuffer
    *\see CyU3PDmaChannelSetupRecvBuffer
    *\see CyU3PDmaChannelWaitForRecvBuffer
    *\see CyU3PDmaMultiChannelGetBuffer
    *\see CyU3PDmaMultiChannelSetupSendBuffer
    *\see CyU3PDmaMultiChannelSetupRecvBuffer
    *\see CyU3PDmaMultiChannelWaitForRecvBuffer
 */
typedef struct CyU3PDmaBuffer_t
{
    uint8_t *buffer;    /**< Pointer to the data buffer. */
    uint16_t count;     /**< Byte count of valid data in buffer. */
    uint16_t size;      /**< Actual size of the buffer in bytes. Should be a multiple of 16. */
    uint16_t status;    /**< Buffer status. This is a four bit data field defined by 
                             CY_U3P_DMA_BUFFER_STATUS_MASK. This holds information like
                             whether the buffer is occupied, whether the buffer holds the
                             end of packet and whether the buffer encountered a DMA error. */
} CyU3PDmaBuffer_t;

/** \brief DMA channel callback input.

    <B>Description</B>\n
    This data structure is used to provide event specific information when a DMA callback
    is called. This structure is defined as a union to facilitate future updates to the
    DMA manager.

    **\see
    *\see CyU3PDmaBuffer_t
    *\see CyU3PDmaCallback_t
 */
typedef union CyU3PDmaCBInput_t
{
    CyU3PDmaBuffer_t buffer_p;  /**< Data about the DMA buffer that caused the callback to be called. */
} CyU3PDmaCBInput_t;

/** @cond DOXYGEN_HIDE */
/* Forward declaration to make compiler happy */
typedef struct CyU3PDmaChannel CyU3PDmaChannel;
typedef struct CyU3PDmaMultiChannel CyU3PDmaMultiChannel;
/** @endcond */

/** \brief DMA channel callback function type.

    <B>Description</B>\n
    The FX3 DMA manager supports a number of event notifications that can be sent to the
    user application during system operation. These event notifications are provided per
    DMA channel, and the types of events required can be selected at the time of channel
    creation.

    This type defines the prototype for the callback function which will be invoked to
    provide these event notifications.

    No blocking calls should be made from these callback functions. If data processing is
    required, it should be done outside of the callback function.

    If produce events are enabled, the application is expected to be fast enough to handle
    the incoming data rate. The input parameter can be stale if the handling of the buffer
    is not done in a timely fashion.

    In the case of produce events on AUTO_SIGNAL channels, the input parameter points to the
    latest produced buffer. If the callback handling is delayed, the producer socket may
    overwrite the buffer before the event is handled.

    In case of MANUAL or MANUAL_IN channels, the input parameter points to the first buffer
    left to be committed to the consumer socket. If the buffer is not committed before the
    next callback, then the input parameter shall be stale data.

    **\see
    *\see CyU3PDmaCBInput_t
    *\see CyU3PDmaChannelConfig_t
    *\see CyU3PDmaChannelCreate
 */
typedef void (*CyU3PDmaCallback_t) (
        CyU3PDmaChannel *handle,        /**< Handle to the DMA channel. */
        CyU3PDmaCbType_t type,          /**< The type of callback notification being generated. */
        CyU3PDmaCBInput_t *input        /**< Union that contains data related to the notification.
                                             The input parameter will be a pointer to a CyU3PDmaBuffer_t variable
                                             in the cases where the callback type is CY_U3P_DMA_CB_RECV_CPLT or
                                             CY_U3P_DMA_CB_PROD_EVENT. */
        );

/** \brief Multi socket DMA channel callback function type.

    <B>Description</B>\n
    Callback function type that is associated with DMA multi-channels. All of the usage
    restrictions and guidelines that apply to normal DMA channel callbacks (CyU3PDmaCallback_t)
    also apply to these callbacks. The only difference is in the type of the first parameter
    passed to the callback function.

    **\see
    *\see CyU3PDmaCallback_t
    *\see CyU3PDmaCBInput_t
    *\see CyU3PDmaMultiChannelConfig_t
    *\see CyU3PDmaMultiChannelCreate
 */
typedef void (*CyU3PDmaMultiCallback_t) (
        CyU3PDmaMultiChannel *handle,   /**< Handle to the multi-socket DMA channel. */
        CyU3PDmaCbType_t type,          /**< The callback type. */
        CyU3PDmaCBInput_t *input        /**< Pointer to a union that contains the callback related data.
                                             Will point to a valid CyU3PDmaBuffer_t variable if the callback
                                             type is CY_U3P_DMA_CB_RECV_CPLT or CY_U3P_DMA_CB_PROD_EVENT. */
        );

/** \brief DMA channel parameters.

    <B>Description</B>\n
    This structure encapsulates the parameters that are provided at the time of DMA
    channel creation, and specifies the resources and events that are to be associated
    with the channel.

    The size field specifies the size in bytes of each DMA buffer to be allocated for this
    DMA channel.

    The offsets prodHeader, prodFooter and consHeader are used to do header addition and
    removal. These are valid only for manual channels and should be zero for auto channels.

    The buffer address seen by the producer = (buffer + prodHeader).\n
    The buffer size seen by the producer    = (size - prodHeader - prodFooter).\n
    The buffer address seen by the consumer = (buffer + consHeader).\n
    The buffer size seen by the consumer    = (buffer - consHeader).

    For header addition to the buffer generated by the producer, the prodHeader should
    be the length of the header to be added and the other offsets should be zero.
    Once the buffer is generated, the header can be modified manually by the CPU
    and committed using the CyU3PDmaChannelCommitBuffer call.

    For footer addition to the buffer generated by the producer, the prodFooter should
    be the length of the footer to be added and the other offsets should be zero.
    Once the buffer is generated, the footer can be added and committed using the 
    CyU3PDmaChannelCommitBuffer call.

    For header deletion from the buffer generated by the producer, the consHeader should
    be the length of the header to be removed and the other offsets should be zero.
    Once the buffer is generated, the buffer can be committed to the consumer socket
    with only change to the size of the data to be transmitted using the CommitBuffer call.

    The size of the buffer as seen by the producer socket should always be a multiple of
    16 bytes; ie, (size - prodHeader - prodFooter) must be a multiple of 16 bytes.

    The prodAvailCount count should always be zero. This is used only for very specific
    use case where there should always be free buffers. Since there is no current use case
    for such a channel, this field should always be zero.

    The count field specifies the number of buffers that should be allocated for this DMA
    channel. It is possible to obtain a large buffering depth by specifying a large count
    value, subject to availability of DMA buffer space and free descriptors.

    **\see
    *\see CyU3PDmaChannelCreate
 */
typedef struct CyU3PDmaChannelConfig_t
{
    uint16_t size;                /**< The buffer size associated with the channel. */
    uint16_t count;               /**< Number of buffers to be allocated for the channel. The count can
                                       be zero for MANUAL, MANUAL_OUT and MANUAL_IN channels if the channel
                                       is intended to operate only in override mode and no buffer need to be
                                       allocated for the channel. The count cannot be zero for AUTO and
                                       AUTO_SIGNAL channels. */
    CyU3PDmaSocketId_t prodSckId; /**< The producer (ingress) socket ID. */
    CyU3PDmaSocketId_t consSckId; /**< The consumer (egress) socket ID. */
    uint16_t prodAvailCount;      /**< Minimum available empty buffers before producer is active. By default,
                                       this should be zero. A non-zero value will activate this feature.
                                       The producer socket will not receive data into memory until the specified
                                       number of free buffers are available. This feature should be used only for
                                       very specific use cases where there is a requirement that there should
                                       always be free buffers during the transfer. */
    uint16_t prodHeader;          /**< The producer socket header offset. */
    uint16_t prodFooter;          /**< The producer socket footer offset. */
    uint16_t consHeader;          /**< The consumer socket header offset. */
    CyU3PDmaMode_t dmaMode;       /**< Mode of DMA operation. */
    uint32_t notification;        /**< Registered notifications. This is a bit map based on CyU3PDmaCbType_t.
                                       This defines the events for which the callback is triggered. */
    CyU3PDmaCallback_t cb;        /**< Callback function which gets invoked on DMA events. */
} CyU3PDmaChannelConfig_t;

/** \def CY_U3P_DMA_MIN_MULTI_SCK_COUNT
    \brief Macro defining the minimum required sockets for multi socket DMA channels.
    Since normal DMA channels can be used for single socket, the minimum number
    supported is 2.
 */
#define CY_U3P_DMA_MIN_MULTI_SCK_COUNT          (2)

/** \def CY_U3P_DMA_MAX_MULTI_SCK_COUNT
    \brief Macro defining the maximum allowed socket for multi socket DMA channels.
 */
#define CY_U3P_DMA_MAX_MULTI_SCK_COUNT          (4)

/** \def CY_U3P_DMA_CHANNEL_START_SIG
    \brief Signature used to identify start of a valid DMA channel structure.
 */
#define CY_U3P_DMA_CHANNEL_START_SIG            (0x43484E4C)

/** \def CY_U3P_DMA_MULTICHN_START_SIG
    \brief Signature used to identify start of a valid DMA multi-channel structure.
 */
#define CY_U3P_DMA_MULTICHN_START_SIG           (0x4D4C4348)

/** \def CY_U3P_DMA_CHANNEL_END_SIG
    \brief Signature used to identify the end of a valid DMA channel or multi-channel structure.
 */
#define CY_U3P_DMA_CHANNEL_END_SIG              (0x454E4443)

/** \brief DMA multi-channel parameter structure.

    <B>Description</B>\n
    This structure encapsulates all the parameters required to create a DMA multi-channel.

    In the case of many to one channels, there shall be 'validSckCount' number of producer
    sockets and only one consumer socket. The producer sockets needs to be updated in the
    required order of operation. The first buffer shall be taken from the prodSckId[0],
    second from prodSckId[1] and so on. If only two producer sockets are used, then only
    prodSckId[0], prodSckId[1] and consSckId[0] shall be considered.

    In the case of one to many operations, there shall be only one producer socket and
    'validSckCount' number of consumer sockets.

    The size field is the total buffer that needs to be allocated for DMA operations.
    This field has restrictions for DMA operations.

    The size, count, prodHeader, prodFooter, consHeader and prodAvailCount fields are
    used in the same way as in the CyU3PDmaChannelConfig_t structure.

    **\see
    *\see CyU3PDmaChannelConfig_t
    *\see CyU3PDmaMultiChannelCreate
*/
typedef struct CyU3PDmaMultiChannelConfig_t
{
    uint16_t size;              /**< The buffer size associated with the channel. */
    uint16_t count;             /**< Number of buffers to be allocated for each socket of the channel. 
                                     For one to many and many to one channels, there will be twice the
                                     number of buffers as specified in the count and for multicast it
                                     will have the same number of buffers as specified in count. The
                                     count cannot be zero. */
    uint16_t validSckCount;     /**< Number of sockets in the multi-socket operation. */
    CyU3PDmaSocketId_t prodSckId[CY_U3P_DMA_MAX_MULTI_SCK_COUNT]; /**< The producer (ingress) socket ID. */
    CyU3PDmaSocketId_t consSckId[CY_U3P_DMA_MAX_MULTI_SCK_COUNT]; /**< The consumer (egress) socket ID. */
    uint16_t prodAvailCount;    /**< Minimum available empty buffers before producer is active. By default,
                                     this should be zero. A non-zero value will activate this feature.
                                     The producer socket will not receive data into memory until the specified
                                     number of free buffers are available. This feature should be used only for
                                     very specific use cases where there is a requirement that there should
                                     always be free buffers during the transfer. */
    uint16_t prodHeader;        /**< The producer socket header offset. */
    uint16_t prodFooter;        /**< The producer socket footer offset. */
    uint16_t consHeader;        /**< The consumer socket header offset. */
    CyU3PDmaMode_t dmaMode;     /**< Mode of DMA operation */
    uint32_t notification;      /**< Registered notifications. This is a bit map based on CyU3PDmaCbType_t.
                                     This defines the events for which the callback is triggered. */
    CyU3PDmaMultiCallback_t cb; /**< Callback function which gets invoked on multi socket DMA events. */
} CyU3PDmaMultiChannelConfig_t;

/** \brief DMA Channel structure.

    <B>Description</B>\n
    This structure keeps track of all the configuration and state information corresponding
    to a DMA channel. This structure is updated and maintained by the DMA driver and APIs.
    It is not recommended that the user application directly access any of this structure
    members.

    **\see
    *\see CyU3PDmaChannelConfig_t
    *\see CyU3PDmaChannelCreate
    *\see CyU3PDmaChannelDestroy
    *\see CyU3PDmaChannelUpdateMode
    *\see CyU3PDmaChannelGetStatus
    *\see CyU3PDmaChannelGetHandle
    *\see CyU3PDmaChannelSetXfer
    *\see CyU3PDmaChannelGetBuffer
    *\see CyU3PDmaChannelCommitBuffer
    *\see CyU3PDmaChannelDiscardBuffer
    *\see CyU3PDmaChannelSetupSendBuffer
    *\see CyU3PDmaChannelSetupRecvBuffer
    *\see CyU3PDmaChannelWaitForCompletion
    *\see CyU3PDmaChannelWaitForRecvBuffer
    *\see CyU3PDmaChannelSetWrapUp
    *\see CyU3PDmaChannelSetSuspend
    *\see CyU3PDmaChannelResume
    *\see CyU3PDmaChannelAbort
    *\see CyU3PDmaChannelReset
    *\see CyU3PDmaChannelCacheControl
 */
struct CyU3PDmaChannel
{
    uint32_t startSig;          /**< Channel structure start signature. */
    uint32_t state;             /**< The current state of the DMA channel */
    uint16_t type;              /**< The type of the DMA channel */
    uint16_t size;              /**< The buffer size associated with the channel */
    uint16_t count;             /**< Number of buffers for the channel */
    uint16_t prodAvailCount;    /**< Minimum available buffers before producer is active. */
    uint16_t firstProdIndex;    /**< Head for the normal descriptor chain list */
    uint16_t firstConsIndex;    /**< Head for the manual mode consumer descriptor chain list */
    uint16_t prodSckId;         /**< The producer (ingress) socket ID */
    uint16_t consSckId;         /**< The consumer (egress) socket ID */
    uint16_t overrideDscrIndex; /**< Descriptor for override modes. */
    uint16_t currentProdIndex;  /**< Producer descriptor for the latest buffer produced. */
    uint16_t currentConsIndex;  /**< Consumer descriptor for the latest buffer produced. */
    uint16_t commitProdIndex;   /**< Producer descriptor for the buffer to be consumed. */
    uint16_t commitConsIndex;   /**< Consumer descriptor for the buffer to be consumed. */
    uint16_t activeProdIndex;   /**< Active producer descriptor. */
    uint16_t activeConsIndex;   /**< Active consumer descriptor. */
    uint16_t prodHeader;        /**< The producer socket header offset */
    uint16_t prodFooter;        /**< The producer socket footer offset */
    uint16_t consHeader;        /**< The consumer socket header offset */
    uint16_t dmaMode;           /**< Mode of DMA operation */
    uint16_t prodSusp;          /**< Producer suspend option. */
    uint16_t consSusp;          /**< Consumer suspend option. */
    uint16_t usbConsSusp;       /**< USB consumer suspend mode for device work-around. */
    uint16_t discardCount;      /**< Number of pending discards. */
    uint32_t notification;      /**< Registered notifications */
    uint32_t xferSize;          /**< Current transfer size */
    CyBool_t isDmaHandleDCache; /**< Whether to do DMA cache handling for this channel. */
    CyU3PMutex lock;            /**< Lock for this channel structure. */
    CyU3PEvent flags;           /**< Event flags for the channel */
    CyU3PDmaCallback_t cb;      /**< Callback function which gets invoked on DMA events */
    uint32_t endSig;            /**< Channel structure end signature. */
};

/** \brief DMA multi-channel structure.

    <B>Description</B>\n
    This structure holds all configuration and state information about the corresponding
    DMA multi-channel. This structure is updated and maintained by the DMA driver and APIs,
    and it is recommended that user code does not access any of the structure members
    directly.

    **\see
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaMultiChannelConfig_t
    *\see CyU3PDmaMultiChannelCreate
    *\see CyU3PDmaMultiChannelDestroy
    *\see CyU3PDmaMultiChannelGetHandle
    *\see CyU3PDmaMultiChannelUpdateMode
    *\see CyU3PDmaMultiChannelSetXfer
    *\see CyU3PDmaMultiChannelGetBuffer
    *\see CyU3PDmaMultiChannelCommitBuffer
    *\see CyU3PDmaMultiChannelDiscardBuffer
    *\see CyU3PDmaMultiChannelSetupSendBuffer
    *\see CyU3PDmaMultiChannelSetupRecvBuffer
    *\see CyU3PDmaMultiChannelWaitForCompletion
    *\see CyU3PDmaMultiChannelWaitForRecvBuffer
    *\see CyU3PDmaMultiChannelSetWrapUp
    *\see CyU3PDmaMultiChannelSetSuspend
    *\see CyU3PDmaMultiChannelResume
    *\see CyU3PDmaMultiChannelAbort
    *\see CyU3PDmaMultiChannelReset
    *\see CyU3PDmaMultiChannelGetStatus
    *\see CyU3PDmaMultiChannelCacheControl
 */
struct CyU3PDmaMultiChannel
{
    uint32_t startSig;          /**< Channel structure start signature. */
    uint32_t state;             /**< The current state of the DMA channel */
    uint16_t type;              /**< The type of the DMA channel */
    uint16_t size;              /**< The buffer size associated with the channel*/
    uint16_t count;             /**< Number of buffers for the channel */
    uint16_t validSckCount;     /**< Number of sockets in the multi-socket operation. */
    uint16_t consDisabled[CY_U3P_DMA_MAX_MULTI_SCK_COUNT];      /**< Whether each consumer socket is disabled. Applies
                                                                     only to multicast channels. */
    uint16_t firstProdIndex[CY_U3P_DMA_MAX_MULTI_SCK_COUNT];    /**< Head for the normal descriptor chain */
    uint16_t firstConsIndex[CY_U3P_DMA_MAX_MULTI_SCK_COUNT];    /**< Head for the manual consumer descriptor chain */
    uint16_t prodSckId[CY_U3P_DMA_MAX_MULTI_SCK_COUNT];         /**< The producer (ingress) socket ID */
    uint16_t consSckId[CY_U3P_DMA_MAX_MULTI_SCK_COUNT];         /**< The consumer (egress) socket ID */
    uint16_t overrideDscrIndex; /**< Descriptor for override modes. */
    uint16_t currentProdIndex;  /**< Producer descriptor for the latest buffer produced. */
    uint16_t currentConsIndex;  /**< Consumer descriptor for the latest buffer produced. */
    uint16_t commitProdIndex;   /**< Producer descriptor for the buffer to be consumed. */
    uint16_t commitConsIndex;   /**< Consumer descriptor for the buffer to be consumed. */
    uint16_t activeProdIndex[CY_U3P_DMA_MAX_MULTI_SCK_COUNT];   /**< Active producer descriptor. */
    uint16_t activeConsIndex[CY_U3P_DMA_MAX_MULTI_SCK_COUNT];   /**< Active consumer descriptor. */
    uint16_t prodHeader;        /**< The producer socket header offset */
    uint16_t prodFooter;        /**< The producer socket footer offset */
    uint16_t consHeader;        /**< The consumer socket header offset */
    uint16_t prodAvailCount;    /**< Minimum available buffers before producer is active. */
    uint16_t dmaMode;           /**< Mode of DMA operation */
    uint16_t prodSusp;          /**< Producer suspend option. */
    uint16_t consSusp;          /**< Consumer suspend option. */
    uint16_t usbConsSusp;       /**< USB consumer suspend mode for device work-around. */
    uint16_t bufferCount[CY_U3P_DMA_MAX_MULTI_SCK_COUNT];       /**< Number of active buffers available for consumer. */
    uint16_t discardCount[CY_U3P_DMA_MAX_MULTI_SCK_COUNT];      /**< Number of buffers to be discarded. */
    uint32_t notification;      /**< Registered notifications. */
    uint32_t xferSize;          /**< Current xfer size */
    CyBool_t isDmaHandleDCache; /**< Whether to do internal DMA cache handling. */
    CyU3PMutex lock;            /**< Lock for the channel structure */
    CyU3PEvent flags;           /**< Event flags for the channel */
    CyU3PDmaMultiCallback_t cb; /**< Callback function which gets invoked on DMA events */
    uint32_t endSig;            /**< Channel structure end signature. */
};

/** \cond DMA_INTERNAL
 */

/* Summary
   DMA socket control structure.

   Description
   This internal data structure is used to map a DMA socket to a channel handle that
   uses the socket.
 */
typedef union
{
    void *handle;                       /* Dummy handle for read operations. */
    CyU3PDmaChannel *singleHandle;      /* Handle to the associated single socket DMA channel */
    CyU3PDmaMultiChannel *multiHandle;  /* Handle to the associated multi socket DMA channel */
} CyU3PDmaSocketCtrl;

/**************************************************************************
 ********************** Global variable declarations **********************
 **************************************************************************/

/* Summary
   Array of socket control structure.

   Description
   Internal global variable used inside the APIs.
 */
extern CyU3PDmaSocketCtrl *glDmaSocketCtrl[];

/* Summary
   The variable determines whether DMA APIs should handle D-cache.

   Description
   This variable is updated and used by API and should not be used by the application.
 */
extern CyBool_t glDmaHandleDCache;

/** \endcond
 */

/**************************************************************************
 ********************************* Macros *********************************
 **************************************************************************/

/** \def CY_U3P_DMA_BUFFER_MARKER
    \brief Software based marker that can be applied on a DMA buffer. This bit is
    part of the four bit status flag that is associated with each DMA buffer descriptor.
    The DMA manager makes use of this bit to indicate that a specific buffer is expected
    to be discarded.
 */
#define CY_U3P_DMA_BUFFER_MARKER                (1u << 0)

/** \def CY_U3P_DMA_BUFFER_EOP
    \brief This bit indicates this descriptor refers to the last buffer of a packet
   or transfer. Packets/transfers may span more than one buffer. The producing
   IP provides this marker by providing the EOP signal to its DMA adapter.
 */
#define CY_U3P_DMA_BUFFER_EOP                   (1u << 1)

/** \def CY_U3P_DMA_BUFFER_ERROR
    \brief This bit is part of the 4 bit DMA buffer status field and indicates that the
    buffer transfer has hit an error.
 */
#define CY_U3P_DMA_BUFFER_ERROR                 (1u << 2)

/** \def CY_U3P_DMA_BUFFER_OCCUPIED
    \brief This status bit indicates the buffer has valid data. This bit can be set
    even if the buffer count is zero, because the device needs to handle Zero length
    packets as well.
 */
#define CY_U3P_DMA_BUFFER_OCCUPIED              (1u << 3)

/** \def CY_U3P_DMA_BUFFER_STATUS_MASK
    \brief Mask to retrieve all DMA buffer status bits.
 */
#define CY_U3P_DMA_BUFFER_STATUS_MASK           (0x000F)

/** \def CY_U3P_DMA_BUFFER_STATUS_WRITE_MASK
    \brief Mask to identify status bits that can be modified by the user application.
    Bit 0 (MARKER) is reserved for internal use by the DMA manager.
 */
#define CY_U3P_DMA_BUFFER_STATUS_WRITE_MASK     (0x000E)

/** \def CY_U3P_DMA_BUFFER_AREA_BASE
    \brief Start address of the allowed buffer memory area for DMA operations. Corresponds
    to the beginning of the FX3 System RAM.
 */
#define CY_U3P_DMA_BUFFER_AREA_BASE     (uint8_t *)(0x40000000)

/** \def CY_U3P_DMA_BUFFER_AREA_LIMIT
    \brief End address of the allowed buffer memory area for DMA operations. Corresponds to
    the end of the System RAM for the CYUSB3014 device.
 */
#define CY_U3P_DMA_BUFFER_AREA_LIMIT    (uint8_t *)(0x40080000)

/** \def CY_U3P_DMA_MAX_BUFFER_SIZE
    \brief Maximum allowed size for a single DMA buffer. This is defined by the FX3 device
    architecture.
 */
#define CY_U3P_DMA_MAX_BUFFER_SIZE      (0xFFF0)

/** \def CY_U3P_DMA_MAX_AVAIL_COUNT
    \brief The maximum number of available buffers before a DMA channel can receive data from
    the producer. This is the maximum value that the prodAvailCount can be set to.
 */
#define CY_U3P_DMA_MAX_AVAIL_COUNT      (31)

/**************************************************************************
 *************************** Function prototypes **************************
 **************************************************************************/

/** \cond DMA_INTERNAL
 */

/* Summary
   Queue a task message to the DMA module.

   Description
   The function shall be used to send task requests to the DMA
   module. It is an internal function and should not be invoked.

   <B>Return value</B>\n
   * None
 */
extern CyU3PReturnStatus_t
CyU3PDmaMsgSend (
        uint32_t *msg,                  /* Pointer to the message data. */
        uint32_t waitOption,            /* Timeout value for send operation. Can be CYU3P_NO_WAIT or
                                           CYU3P_WAIT_FOREVER. */
        CyBool_t priority               /* Whether this message should be queued at the head or the tail. */
        );

/* Summary
   Specify that the AVL_EN bit needs to be set for a USB endpoint.
 */
extern void
CyU3PDmaSetUsbSocketMult (
        uint8_t  ep,                    /* Endpoint for which the AVL_EN is to be set. */
        CyBool_t burstEnable            /* Whether AVL_EN bit is to be set or cleared. */
        );

/* \brief Set the AVL_ENABLE bit for a USB socket.

   <B>Description</B>\n
   This is an internal function used by the FX3 API to set the AVL_ENABLE bit on a USB socket.
   This is called from the SetXfer handlers for various DMA channels.

   <B>Return value</B>\n
   * Returns the updated socket status value.
 */
extern uint32_t
CyU3PDmaUsbSockSetAvlEn (
        uint16_t sckId,                 /**< DMA socket on which the AVL_ENABLE bit is to be set. */
        uint32_t sckStat,               /**< Original socket status value. */
        uint8_t  bufCnt                 /**< Number of DMA buffers used on the DMA channel. */
        );

/* \brief Clear the AVL_ENABLE bit for a USB socket.

   <B>Description</B>\n
   This is an internal function used by the FX3 API to clear the AVL_ENABLE bit on a USB socket.
   This is called from the Destroy handler for various DMA channels.

   <B>Return value</B>\n
   * Returns the updated socket status value.
 */
extern uint32_t
CyU3PDmaUsbSockClrAvlEn (
        uint16_t sckId,                 /**< DMA socket on which the AVL_ENABLE bit is to be cleared. */
        uint32_t sckStat                /**< Original socket status value. */
        );

/** \endcond
 */

/******************* Single channel Function prototypes *******************/

/** \brief Fetch a handle to the DMA channel corresponding to the specified socket.

    <B>Description</B>\n
    This function is used to identify if there is any DMA channel
    associated with a socket.

    <B>Return value</B>\n
    * Handle of the channel associated with the specified socket. A NULL return indicates
    that the socket has not been associated with any DMA channel.

    **\see
    *\see CyU3PDmaChannel
    *\see CyU3PDmaSocketId_t
    *\see CyU3PDmaChannelCreate
    *\see CyU3PDmaChannelDestroy
 */
extern CyU3PDmaChannel *
CyU3PDmaChannelGetHandle (
        CyU3PDmaSocketId_t sckId        /**< ID of the socket whose channel handle is required. */
        );

/** \brief Create a one-to-one socket DMA channel.

    <B>Description</B>\n
    Create a normal (one-to-one socket) DMA channel using the configuration parameters
    specified. The DMA channel structure is expected to be allocated by the caller, and
    a pointer to it should be passed in as a parameter. This function should not be called
    from a DMA callback function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if any of the configuration parameters are invalid.\n
    * CY_U3P_ERROR_MEMORY_ERROR - if the memory required for the channel could not be allocated.

    **\see
    *\see CyU3PDmaType_t
    *\see CyU3PDmaChannel
    *\see CyU3PDmaChannelConfig_t
    *\see CyU3PDmaChannelDestroy
    *\see CyU3PDmaChannelUpdateMode
    *\see CyU3PDmaChannelGetStatus
    *\see CyU3PDmaChannelGetHandle
    *\see CyU3PDmaChannelSetXfer
    *\see CyU3PDmaChannelGetBuffer
    *\see CyU3PDmaChannelCommitBuffer
    *\see CyU3PDmaChannelDiscardBuffer
    *\see CyU3PDmaChannelSetupSendBuffer
    *\see CyU3PDmaChannelSetupRecvBuffer
    *\see CyU3PDmaChannelWaitForCompletion
    *\see CyU3PDmaChannelWaitForRecvBuffer
    *\see CyU3PDmaChannelSetWrapUp
    *\see CyU3PDmaChannelSetSuspend
    *\see CyU3PDmaChannelResume
    *\see CyU3PDmaChannelAbort
    *\see CyU3PDmaChannelReset
    *\see CyU3PDmaChannelCacheControl
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelCreate (
        CyU3PDmaChannel *handle,                /**< Pointer to channel structure that should be initialized. */
        CyU3PDmaType_t type,                    /**< Type of DMA channel desired. */
        CyU3PDmaChannelConfig_t *config         /**< Channel configuration parameters. */
        );

/** \brief Destroy a one-to-one socket DMA channel.

    <B>Description</B>\n
    This function frees up a one-to-one socket DMA channel once it is no longer required.
    This should not be called from a DMA callback.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel structure is not valid.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaChannel
    *\see CyU3PDmaChannelCreate
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelDestroy (
        CyU3PDmaChannel *handle                 /**< Pointer to DMA channel structure to be de-initialized. */
        );

/** \brief Update the DMA mode for a one-to-one DMA channel.

    <B>Description</B>\n
    The DMA mode of a channel decides whether transfers on the channel are tracked in terms
    of bytes or buffers. The mode is normally specified at the time of channel creation and
    left unmodified there-after. This API can be used if the mode for an existing channel
    needs to be changed. This can only be called when the channel is in the configured (just
    created or reset) state.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if DMA mode is invalid.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - if the DMA channel is not in the Configured state.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaMode_t
    *\see CyU3PDmaChannel
    *\see CyU3PDmaChannelCreate
    *\see CyU3PDmaChannelGetStatus
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelUpdateMode (
        CyU3PDmaChannel *handle,                /**< Handle to DMA channel to be modified. */
        CyU3PDmaMode_t dmaMode                  /**< Desired DMA operating mode. Can be byte mode or buffer mode. */
        );

/** \brief Setup a one-to-one DMA channel for data transfer

    <B>Description</B>\n
    The sockets corresponding to a DMA channel are left disabled when the channel is created,
    so that transfers are started only when desired by the user. This function enables a DMA
    channel to transfer a specified amount of data before suspending again. An infinite transfer
    can be started by specifying a transfer size of 0.
    
    This function should be called only when the channel is in the CY_U3P_DMA_CONFIGURED state.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if no buffers were allocated for this DMA channel.\n
    * CY_U3P_ERROR_ALREADY_STARTED - if the channel is already active.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaChannel
    *\see CyU3PDmaChannelCreate
    *\see CyU3PDmaChannelGetStatus
    *\see CyU3PDmaChannelSetupSendBuffer
    *\see CyU3PDmaChannelSetupRecvBuffer
    *\see CyU3PDmaChannelWaitForCompletion
    *\see CyU3PDmaChannelReset
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelSetXfer (
        CyU3PDmaChannel *handle,                /**< Handle to the channel to be modified. */
        uint32_t count                          /**< The desired transaction size in units corresponding to the
                                                     selected DMA mode. Channel will revert to idle state when the
                                                     specified amount of data has been transferred. Can be set to
                                                     zero to request an infinite data transfer. */
        );

/** \brief Wraps up the current active buffer for the channel from the producer side.

    <B>Description</B>\n
    Data received by a DMA producer socket is only committed (made available to the
    consumer) when the buffer is completely filled up, or when the producer signals
    and End Of Packet (EOP) condition. There may be cases where these conditions are
    not met, and a partially filled buffer is blocked waiting for more data.

    This function can be used to forcibly commit the contents of the active DMA buffer
    on the producer side of the channel. A DMA produce event will be generated as a
    result of this API call, and the producer socket will move on to the next buffer
    in the chain without getting suspended.

    The function cannot be used with MANUAL_OUT channels, and can only be called when
    the channel is the active or consumer override state.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if this operation is not supported for the DMA channel type.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - if the DMA channel is not in the required state.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.\n

    **\see
    *\see CyU3PDmaState_t
    *\see CyU3PDmaChannelGetStatus
    *\see CyU3PDmaChannelSetXfer
    *\see CyU3PDmaChannelGetBuffer
    *\see CyU3PDmaChannelCommitBuffer
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelSetWrapUp (
        CyU3PDmaChannel *handle         /**< Handle to the channel to be modified. */
        );

/** \brief Set the suspend options for the sockets associated with a DMA channel.

    <B>Description</B>\n
    The function sets the suspend options for the sockets. The sockets
    are by default set to SUSP_NONE option. The API can be called only when
    the channel is in configured state or in active state. The suspend options
    are applied only in the active state (SetXfer mode) and is a don't care in
    the override mode of operation.

    For manual channels, suspending the channel is largely not required as each
    buffer needs to be manually committed. However, these options are still available
    for manual DMA channels.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the channel type/suspend options are invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - if the DMA channel is not in the required state.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaChannel
    *\see CyU3PDmaChannelResume
    *\see CyU3PDmaChannelReset
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelSetSuspend (
        CyU3PDmaChannel *handle,                /**< Handle to the channel to be modified. */
        CyU3PDmaSckSuspType_t prodSusp,         /**< Suspend option for the producer socket. */
        CyU3PDmaSckSuspType_t consSusp          /**< Suspend option for the consumer socket. */
        );

/** \brief Resume a suspended DMA channel.

    <B>Description</B>\n
    The function can be called to resume a suspended DMA channel. It can only be called when
    the channel was suspended from an active state. The producer and consumer suspend conditions
    can be individually cleared.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the channel type is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_STARTED - if the DMA channel was not started.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaChannelSetSuspend
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelResume (
        CyU3PDmaChannel *handle,                /**< Handle to the channel to be resumed. */
        CyBool_t        isProdResume,           /**< Whether to resume the producer socket. */
        CyBool_t        isConsResume            /**< Whether to resume the consumer socket. */
        );

/** \brief Aborts a DMA channel.

    <B>Description</B>\n
    The function shall abort both the producer and consumer sockets associated with the channel.
    The data in transition is lost and any active transaction cannot be resumed. This function
    leaves the channel in an aborted state and requires a reset before the channel can be used
    again.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaChannel
    *\see CyU3PDmaChannelSetXfer
    *\see CyU3PDmaChannelReset
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelAbort (
        CyU3PDmaChannel *handle                 /**< Handle to the channel to be aborted. */
        );

/** \brief Aborts and resets a DMA channel.

    <B>Description</B>\n
    The function aborts the ongoing transfer on a DMA channel, and restores all sockets
    and descriptors to their initial state. At the end of the Reset call, all resources
    associated with the channel are back in the state that they are in immediately after
    channel creation.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaChannel
    *\see CyU3PDmaChannelCreate
    *\see CyU3PDmaChannelAbort
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelReset (
        CyU3PDmaChannel *handle                 /**< Handle to the channel to be reset. */
        );

/** \brief Send the contents of a user provided buffer to the consumer.

    <B>Description</B>\n
    This function initiates the sending of the content of a user provided buffer to the
    consumer of a DMA channel. This function is an override on the normal behavior of the
    DMA channel and can only be called when the channel is in the configured state.

    The buffers used for DMA operations are expected to be allocated using the
    CyU3PDmaBufferAlloc call. If this is not the case, then the buffer has to be over
    allocated in such a way that the full buffer should be 32 byte aligned and should
    be a multiple of 32 bytes in size.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the buffer parameters are invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if this operation is not supported for the DMA channel type.\n
    * CY_U3P_ERROR_ALREADY_STARTED - if the DMA channel is already started.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaChannel
    *\see CyU3PDmaBuffer_t
    *\see CyU3PDmaChannelCreate
    *\see CyU3PDmaChannelReset
    *\see CyU3PDmaChannelSetupRecvBuffer
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelSetupSendBuffer (
        CyU3PDmaChannel *handle,                /**< Handle to the DMA channel to be modified. */
        CyU3PDmaBuffer_t *buffer_p              /**< Pointer to structure containing address, size and status of the
                                                     DMA buffer to be sent out. */
        );

/** \brief Receive data into a user provided DMA buffer.

    <B>Description</B>\n
    This function initiates a read of incoming data from a DMA producer into a user
    provided buffer. This operation is performed in override mode of the DMA channel,
    and can only be initiated when the channel is in configured state.

    The buffer that is passed as parameter for receiving the data has the
    following restrictions:\n
    1. The buffer size should be a multiple of 16 bytes.\n
    2. If the data cache is enabled then, the buffer should be 32 byte aligned and
       a multiple of 32 bytes. This is to match the 32 byte cache line. 32 byte
       check is not enforced by the API as the buffer can be over-allocated.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the buffer parameters are invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if this operation is not supported for the DMA channel type.\n
    * CY_U3P_ERROR_ALREADY_STARTED - if the DMA channel is already started.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.\n

    **\see
    *\see CyU3PDmaBuffer_t
    *\see CyU3PDmaChannel
    *\see CyU3PDmaChannelSetupSendBuffer
    *\see CyU3PDmaChannelWaitForRecvBuffer
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelSetupRecvBuffer (
        CyU3PDmaChannel *handle,                /**< Handle to the DMA channel to be modified. */
        CyU3PDmaBuffer_t *buffer_p              /**< Pointer to structure containing the address and size of the buffer
                                                     to be filled up with received data. */
        );

/** \brief Wait until an override read operation is completed.

    <B>Description</B>\n
    This function waits until the read operation initiated through the
    CyU3PDmaChannelSetupRecvBuffer API is completed. Once the transfer is
    completed, it returns the status of the buffer that was filled with data.
    If not, a timeout error is returned. It is possible to retry this wait
    API without resetting or aborting the channel.

    This function must not be called from a DMA callback function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the buffer parameters are invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if this operation is not supported for the DMA channel type.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - if this sequence is not permitted.\n
    * CY_U3P_ERROR_TIMEOUT - if the DMA transfer timed out.\n
    * CY_U3P_ERROR_DMA_FAILURE - if the DMA transfer failed.\n
    * CY_U3P_ERROR_ABORTED - if the DMA transfer was aborted.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.\n
    * CY_U3P_ERROR_NOT_STARTED - if the DMA channel is not started.

    **\see
    *\see CyU3PDmaChannel
    *\see CyU3PDmaChannelSetupRecvBuffer
    *\see CyU3PDmaChannelWaitForCompletion
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelWaitForRecvBuffer (
        CyU3PDmaChannel *handle,                /**< Handle to the DMA channel on which to wait. */
        CyU3PDmaBuffer_t *buffer_p,             /**< Output parameter which will be filled up with the address, count
                                                     and status values of the DMA buffer into which data was
                                                     received. */
        uint32_t waitOption                     /**< Duration to wait for the receive completion. */
        );

/** \brief Get the current buffer pointer.

    <B>Description</B>\n
    This function waits until a buffer is ready on the channel, and then returns
    a pointer to the buffer status.

    The ready condition is defined differently for different DMA channel types:\n
    1. For MANUAL and MANUAL_IN DMA channels, a ready buffer is one which has been
    filled with data by the producer.\n
    2. For MANUAL_OUT channels, a ready buffer is an empty buffer that can be filled
    by the firmware.\n
    3. For AUTO channels, this API can only be called when the consumer socket is
    suspended. This mechanism is supported to facilitate reading the buffer status
    when the channel has been suspended using the CY_U3P_DMA_SCK_SUSP_CONS_PARTIAL_BUF
    option.

    If this function is called from a DMA callback, the wait option should be set to
    CYU3P_NO_WAIT.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the buffer parameters are invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if this operation is not supported for the DMA channel type.\n
    * CY_U3P_ERROR_NOT_STARTED - if the DMA channel is not started.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - if this sequence is not permitted.\n
    * CY_U3P_ERROR_TIMEOUT - if the DMA transfer timed out.\n
    * CY_U3P_ERROR_DMA_FAILURE - if the DMA transfer failed.\n
    * CY_U3P_ERROR_ABORTED - if the DMA transfer was aborted.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.\n

    **\see
    *\see CyU3PDmaBuffer_t
    *\see CyU3PDmaChannel
    *\see CyU3PDmaChannelSetXfer
    *\see CyU3PDmaChannelCommitBuffer
    *\see CyU3PDmaChannelDiscardBuffer
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelGetBuffer (
        CyU3PDmaChannel *handle,                /**< Handle to the DMA channel on which to wait. */
        CyU3PDmaBuffer_t *buffer_p,             /**< Output parameter that will be filled with data about the
                                                     buffer that was obtained. */
        uint32_t waitOption                     /**< Duration to wait before returning a timeout status. */
        );

/** \brief Commit a data buffer to be sent to the consumer.

    <B>Description</B>\n
    This function is generally used with manual DMA channels, and allows the user
    to commit a data buffer which is to be sent out to the consumer. This operation
    is not valid for channels of type MANUAL_IN.

    The count provided is the exact size of the data that is to be sent out of the
    consumer socket.

    This API can be used with AUTO DMA channels in the special case where the consumer
    socket is suspended. This allows the user to modify the data content of a partial
    data buffer, and then commit it for sending out of the device.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the count is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if this operation is not supported for the DMA channel type.\n
    * CY_U3P_ERROR_NOT_STARTED - if the DMA channel is not started.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - if this sequence is not permitted.\n
    * CY_U3P_ERROR_DMA_FAILURE - if the DMA transfer failed.\n
    * CY_U3P_ERROR_ABORTED - if the DMA transfer was aborted.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaChannelGetBuffer
    *\see CyU3PDmaChannelDiscardBuffer
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelCommitBuffer (
        CyU3PDmaChannel *handle,                /**< Handle to the DMA channel to be modified. */
        uint16_t count,                         /**< Size of data in the buffer being committed. The buffer
                                                     address is implicit and is fetched from the active descriptor
                                                     for the channel. */
        uint16_t bufStatus                      /**< Current status (end of transfer bit) of the buffer being
                                                     committed. The occupied bit will automatically be set by
                                                     the API. */
        );

/** \brief Drop a data buffer without sending out to the consumer.

    <B>Description</B>\n
    This function drops the content of the active buffer by moving the consumer
    socket ahead to the next buffer. This function is generally used with DMA
    channels of type MANUAL and MANUAL_IN.

    In the case of AUTO DMA channels, this API is used in the special case where
    the consumer socket is suspended using the CY_U3P_DMA_SCK_SUSP_CONS_PARTIAL_BUF
    option. This allows the user to drop the content of a partially filled buffer
    without sending it out to the consumer.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if this operation is not supported for the DMA channel type.\n
    * CY_U3P_ERROR_NOT_STARTED - if the DMA channel is not started.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - if this sequence is not permitted.\n
    * CY_U3P_ERROR_DMA_FAILURE - if the DMA transfer failed.\n
    * CY_U3P_ERROR_ABORTED - if the DMA transfer was aborted.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaChannel
    *\see CyU3PDmaChannelGetBuffer
    *\see CyU3PDmaChannelCommitBuffer
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelDiscardBuffer (
        CyU3PDmaChannel *handle                 /**< Handle to the DMA channel to be modified. */
        );

/** \brief Wait for the current DMA transaction to complete.

    <B>Description</B>\n
    This function waits until the current transfer on the DMA channel is completed,
    or until the specified timeout period has elapsed. This function is not supported
    if the DMA channel has been configured for infinite transfers.

    This function should not be called from a DMA callback function. If the function
    returns CY_U3P_ERROR_TIMEOUT, the wait API call can be repeated without affecting
    the data transfer.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if this operation is not supported for the DMA channel type.\n
    * CY_U3P_ERROR_NOT_STARTED - if the DMA channel is not started.\n
    * CY_U3P_ERROR_DMA_FAILURE - if the DMA transfer failed.\n
    * CY_U3P_ERROR_ABORTED - if the DMA transfer was aborted.\n
    * CY_U3P_ERROR_TIMEOUT - if the DMA transfer timed out.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaChannelSetXfer
    *\see CyU3PDmaChannelSetupSendBuffer
    *\see CyU3PDmaChannelSetupRecvBuffer
    *\see CyU3PDmaChannelWaitForRecvBuffer
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelWaitForCompletion (
        CyU3PDmaChannel *handle,                /**< Handle to the DMA channel to wait on. */
        uint32_t waitOption                     /**< Duration for which to wait. */
        );

/** \brief This function returns the current channel status.

    <B>Description</B>\n
    This function returns the current state of the DMA channel as well as the
    current transfer counts on both producer and consumer sockets.

    <B>Note</B>\n
    The FX3 device only updates the transfer count values at buffer boundaries, and it
    is not possible to identify the transfer count at a partial buffer level.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaState_t
    *\see CyU3PDmaChannelSetXfer
 */
CyU3PReturnStatus_t
CyU3PDmaChannelGetStatus (
        CyU3PDmaChannel *handle,                /**< Handle to the DMA channel to query. */
        CyU3PDmaState_t *state,                 /**< Output parameter that will be filled with state of the channel. */
        uint32_t *prodXferCount,                /**< Output parameter that will be filled with transfer count on the
                                                     producer socket in DMA mode units. Will return zero in the case
                                                     of MANUAL_OUT channels. */
        uint32_t *consXferCount                 /**< Output parameter that will be filled with transfer count on the
                                                     consumer socket in DMA mode units. Will return zero in the case
                                                     of MANUAL_IN channels. */
        );

/** \brief This function is used to enable/disable the Data cache coherency handling on a
    per DMA channel basis.

    <B>Description</B>\n
    The DMA driver in the FX3 library assumes that any manual data transfer on a DMA
    channel can be affected by the Data cache. Therefore, it ensures that the data buffer
    region is evicted from the cache before reading any data from a newly produced buffer.
    It also ensures that the data buffer region is written back to memory before commiting
    the buffer to the consumer.

    These cache operations can limit the transfer performance obtained on the DMA channel
    in some cases. If the caller can ensure that the contents of the data buffer are accessed
    by the firmware only in very rare cases, it is possible to get better performance by
    removing these cache operations.

    This API is used to selectively turn off the data cache handling on a per DMA channel
    basis. This should only be used with caution, and the caller should ensure that the
    cache APIs are used directly where required.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - if the DMA channel is not idle (configured state).

    **\see
    *\see CyU3PDmaChannel
    *\see CyU3PDeviceCacheControl
    *\see CyU3PDmaChannelCreate
 */
CyU3PReturnStatus_t
CyU3PDmaChannelCacheControl (
        CyU3PDmaChannel *handle,                /**< Handle to the DMA channel. */
        CyBool_t isDmaHandleDCache              /**< Whether to enable handling or not. */
        );

/******************* Multi channel Function prototypes ********************/

/** \brief Identifies the multi-channel that is associated with the specified socket.

    <B>Description</B>\n
    This function returns a pointer to the multi-channel that is associated with the
    specified DMA socket. This function will return NULL if there are no channels
    associated with the specified socket.

    <B>Return value</B>\n
    * Handle to the Multi-channel structure corresponding to the socket.

    **\see
    *\see CyU3PDmaSocketId_t
    *\see CyU3PDmaMultiChannel
 */
extern CyU3PDmaMultiChannel *
CyU3PDmaMultiChannelGetHandle (
        CyU3PDmaSocketId_t sckId                /**< ID of the socket whose channel handle is required. */
        );

/** \brief Create a multi-socket DMA channel with the specified parameters.

    <B>Description</B>\n
    This function is used to create a multi-socket DMA channel based on the specified
    parameters. The multi-channel structure is to be allocated by the caller, and a
    pointer to this is to be passed as parameter to this function.

    This function must not be called from a DMA callback function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if any of the configuration parameters are invalid.\n
    * CY_U3P_ERROR_MEMORY_ERROR - if the memory required for the channel could not be allocated.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - if a multicast channel is being created without calling CyU3PDmaEnableMulticast.\n

    **\see
    *\see CyU3PDmaMultiType_t
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaMultiChannelConfig_t
    *\see CyU3PDmaMultiChannelDestroy
    *\see CyU3PDmaChannelCreate
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelCreate (
        CyU3PDmaMultiChannel *handle,           /**< Pointer to multi-channel structure that is to be initialized. */
        CyU3PDmaMultiType_t type,               /**< Type of DMA channel to be created. */
        CyU3PDmaMultiChannelConfig_t *config    /**< Configuration information about the channel to be created. */
        );

/** \brief Destroy a multi-socket DMA channel.

    <B>Description</B>\n
    This function is used to free up the resources used by a DMA multi-channel when it
    is no longer required. This function should not be called from a DMA callback function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaMultiChannelCreate
    *\see CyU3PDmaChannelDestroy
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelDestroy (
        CyU3PDmaMultiChannel *handle            /* Pointer to the multi-channel to be de-initialized. */
        );

/** \brief Update the DMA mode for a multi-channel.

    <B>Description</B>\n
    The DMA mode for a channel specifies the units in which data transfers on the channel
    are setup and measured. This is generally set during channel creation and retained
    unchanged there-after. This function can be used to dynamically change the DMA mode
    for a multi-channel.

    The function can only be called when the DMA channel is in the configured state.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the DMA mode is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - if the DMA channel is not in the Configured state.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.\n

    **\see
    *\see CyU3PDmaMode_t
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaMultiChannelCreate
    *\see CyU3PDmaMultiChannelSetXfer
    *\see CyU3PDmaMultiChannelGetStatus
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelUpdateMode (
        CyU3PDmaMultiChannel *handle,           /**< Handle to the multi-channel to be modified. */
        CyU3PDmaMode_t dmaMode                  /**< Desired DMA mode. */
        );

/** \brief Prepare a DMA multi-channel for data transfer.

    <B>Description</B>\n
    This function prepares the sockets involved in a DMA multi-channel for data transfer.
    This function can only be called when the channel is in the CY_U3P_DMA_CONFIGURED
    state. The amount of data to be transferred before the channel gets suspended is specified
    as a parameter, and can be set to 0 to start infinite transfers.

    The multiSckOffset parameter specifies the index of the producer (in the case of
    many-to-one channels) or the consumer (one-to-many channels) that will transfer
    the first buffer. The rest of the sockets will be used in the sequence specified
    at channel creation.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the multiSckOffset is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_ALREADY_STARTED - if the DMA channel was already started.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaMultiChannelCreate
    *\see CyU3PDmaMultiChannelSetupSendBuffer
    *\see CyU3PDmaMultiChannelSetupRecvBuffer
    *\see CyU3PDmaMultiChannelWaitForCompletion
    *\see CyU3PDmaMultiChannelReset
    *\see CyU3PDmaMultiChannelGetStatus
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelSetXfer (
        CyU3PDmaMultiChannel *handle,           /**< Handle to the multi-channel to be modified. */
        uint32_t count,                         /**< Size of the transfer to be started. Can be zero to denote
                                                     infinite data transfer. */
        uint16_t multiSckOffset                 /**< Socket id to start the operation from. For a many to one channel,
                                                     this would be a producer socket offset; and for a one to many
                                                     channel, this would be a consumer socket offset. */
        );

/** \brief Wraps up the current active buffer on the producer socket.

    <B>Description</B>\n
    This API is used to forcibly commit a DMA buffer to the consumer, and is
    useful in the case where data transfer has abruptly stopped without the
    producer being able to commit the data buffer.

    The function can be called only when the channel is in active mode or in
    consumer override mode.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the multiSckOffset is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE  - if the DMA channel is not in the required state.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaChannelSetWrapUp
    *\see CyU3PDmaMultiChannel
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelSetWrapUp (
        CyU3PDmaMultiChannel *handle,           /**< Handle to the channel to be modified. */
        uint16_t multiSckOffset                 /**< Socket id to wrapup. For a many to one channel, this would
                                                     be a producer socket offset; and for a one to many channel,
                                                     this would always be zero. */
        );

/** \brief Update the suspend options for the sockets associated with a DMA multi-channel.

    <B>Description</B>\n
    The function sets the suspend options for the sockets. The sockets
    are by default set to SUSP_NONE option. The API can be called only when
    the channel is in configured state or in active state. The suspend options
    are applied only in the active state (SetXfer mode) and is a don't care in
    the override mode of operation.

    For manual channels, suspending the channel is largely not required as each
    buffer needs to be manually committed. However, these options are still available
    for manual DMA channels.

    Suspend options can only be set for the end where there is a single socket.
    i.e., Suspend options can only be set for the producer side on one-to-many
    and multicast channels; and only for the consumer side on many-to-one channels.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the suspend options are invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE  - if the DMA channel is not in the required state.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaSckSuspType_t
    *\see CyU3PDmaChannelSetSuspend
    *\see CyU3PDmaMultiChannelResume
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelSetSuspend (
        CyU3PDmaMultiChannel *handle,           /**< Handle to the multi-channel to be suspended. */
        CyU3PDmaSckSuspType_t prodSusp,         /**< Suspend option for the producer socket. */
        CyU3PDmaSckSuspType_t consSusp          /**< Suspend option for the consumer socket. */
        );

/** \brief Resume a suspended DMA multi-channel.

    <B>Description</B>\n
    The function can be called to resume a suspended DMA channel. It can only be called when
    the channel was suspended from an active state.

    For many-to-one channels, only the consumer side can be resumed; and for one-to-many
    channels, only the producer side can be resumed.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the resume options are invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    * CyU3PDmaMultiChannel
    * CyU3PDmaChannelResume
    * CyU3PDmaMultiChannelSetSuspend
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelResume (
        CyU3PDmaMultiChannel *handle,           /**< Handle to the multi-channel to be resumed. */
        CyBool_t        isProdResume,           /**< Whether to resume the producer socket. */
        CyBool_t        isConsResume            /**< Whether to resume the consumer socket. */
        );

/** \brief Abort the ongoing transfer on a DMA multi-channel.

    <B>Description</B>\n
    The function shall abort all the producer and consumer sockets associated with the channel.
    The data in transition is lost and any active transaction cannot be resumed. This function
    leaves the channel in an aborted state and requires a reset before the channel can be used
    again.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaMultiChannelSetXfer
    *\see CyU3PDmaMultiChannelReset
    *\see CyU3PDmaMultiChannelGetStatus
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelAbort (
        CyU3PDmaMultiChannel *handle            /**< Handle to the multi-channel to be aborted. */
        );

/** \brief Abort ongoing transfers on and resets a DMA multi-channel.

    <B>Description</B>\n
    The function aborts the ongoing transfer on a DMA channel, and restores all sockets
    and descriptors to their initial state. At the end of the Reset call, all resources
    associated with the channel are back in the state that they are in immediately after
    channel creation.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaMultiChannelSetXfer
    *\see CyU3PDmaMultiChannelSetupSendBuffer
    *\see CyU3PDmaMultiChannelSetupRecvBuffer
    *\see CyU3PDmaMultiChannelAbort
    *\see CyU3PDmaMultiChannelGetStatus
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelReset (
        CyU3PDmaMultiChannel *handle            /**< Handle to the multi-channel to be reset. */
        );
                                                  

/** \brief Send the contents of a user provided buffer to the consumer.

    <B>Description</B>\n
    This function initiates the sending of the content of a user provided buffer to the
    consumer of a DMA channel. This function is an override on the normal behavior of the
    DMA channel and can only be called when the channel is in the configured state.

    The buffers used for DMA operations are expected to be allocated using the
    CyU3PDmaBufferAlloc call. If this is not the case, then the buffer has to be over
    allocated in such a way that the full buffer should be 32 byte aligned and should
    be a multiple of 32 bytes in size.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if an invalid parameters are passed.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_ALREADY_STARTED - if the DMA channel was already started.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaBuffer_t
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaMultiChannelSetupRecvBuffer
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelSetupSendBuffer (
        CyU3PDmaMultiChannel *handle,           /**< Handle to the DMA multi channel to be modified. */
        CyU3PDmaBuffer_t *buffer_p,             /**< Pointer to structure containing address, size and status of the
                                                     DMA buffer to be sent out. */
        uint16_t multiSckOffset                 /**< Consumer Socket id to send the data. For a many to one channel,
                                                     this should be zero; and for a one to many channel, this would
                                                     be a consumer socket offset. */
        );

/** \brief Receive data into a user provided DMA buffer.

    <B>Description</B>\n
    This function initiates a read of incoming data from a DMA producer into a user
    provided buffer. This operation is performed in override mode of the DMA channel,
    and can only be initiated when the channel is in configured state.

    The buffer that is passed as parameter for receiving the data has the
    following restrictions:\n
    1. The buffer size should be a multiple of 16 bytes.\n
    2. If the data cache is enabled then, the buffer should be 32 byte aligned and
       a multiple of 32 bytes. This is to match the 32 byte cache line. 32 byte
       check is not enforced by the API as the buffer can be over-allocated.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if an invalid parameters are passed.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_ALREADY_STARTED - if the DMA channel was already started.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaBuffer_t
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaMultiChannelSetupSendBuffer
    *\see CyU3PDmaMultiChannelWaitForRecvBuffer
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelSetupRecvBuffer (
        CyU3PDmaMultiChannel *handle,           /**< Handle to the DMA multi channel to be modified. */
        CyU3PDmaBuffer_t *buffer_p,             /**< Pointer to structure containing the address and size of the buffer
                                                     to be filled up with received data. */
        uint16_t multiSckOffset                 /**< Producer Socket id to receive the data. For a one to many channel,
                                                     this should be zero; and for a many to one channel, this would
                                                     be a producer socket offset. */
        );

/** \brief Wait until an override read operation is completed.

    <B>Description</B>\n
    This function waits until the read operation initiated through the
    CyU3PDmaMultiChannelSetupRecvBuffer API is completed. Once the transfer is
    completed, it returns the status of the buffer that was filled with data.
    If not, a timeout error is returned. It is possible to retry this wait
    API without resetting or aborting the channel.

    This function must not be called from a DMA callback function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if this operation is not supported for the DMA channel type.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE  - if the DMA channel is not in the required state.\n
    * CY_U3P_ERROR_DMA_FAILURE - if the DMA transfer failed.\n
    * CY_U3P_ERROR_ABORTED - if the DMA transfer was aborted.\n
    * CY_U3P_ERROR_TIMEOUT - if the DMA transfer timed out.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.\n
    * CY_U3P_ERROR_NOT_STARTED - if the DMA channel is not started.

    **\see
    *\see CyU3PDmaBuffer_t
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaMultiChannelSetupRecvBuffer
    *\see CyU3PDmaMultiChannelWaitForCompletion
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelWaitForRecvBuffer (
        CyU3PDmaMultiChannel *handle,           /**< Handle to the DMA channel on which to wait. */
        CyU3PDmaBuffer_t *buffer_p,             /**< Output parameter which will be filled up with the address,
                                                     count and status values of the DMA buffer into which data was
                                                     received. */
        uint32_t waitOption                     /**< Duration to wait for the receive completion. */
        );

/** \brief Get the current buffer pointer.

    <B>Description</B>\n
    This function waits until a buffer is ready on the channel, and then returns
    a pointer to the buffer status.

    In the case of many-to-one DMA channels, the buffers are received from the producer
    sockets in a round-robin fashion. The first producer is selected through the
    multiSckOffset parameter passed to the CyU3PDmaMultiChannelSetXfer API.

    The ready condition is defined differently for different DMA channel types:\n
    1. For MANUAL and MANUAL_IN DMA channels, a ready buffer is one which has been
    filled with data by the producer.\n
    2. For MANUAL_OUT channels, a ready buffer is an empty buffer that can be filled
    by the firmware.\n
    3. For many-to-one AUTO channels, this API can only be called when the consumer socket is
    suspended. This mechanism is supported to facilitate reading the buffer status
    when the channel has been suspended using the CY_U3P_DMA_SCK_SUSP_CONS_PARTIAL_BUF
    option.

    If this function is called from a DMA callback, the wait option should be set to
    CYU3P_NO_WAIT.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if this operation is not supported for the DMA channel type.\n
    * CY_U3P_ERROR_NOT_STARTED - if the DMA channel is not started.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE  - if the DMA channel is not in the required state.\n
    * CY_U3P_ERROR_DMA_FAILURE - if the DMA transfer failed.\n
    * CY_U3P_ERROR_ABORTED - if the DMA transfer was aborted.\n
    * CY_U3P_ERROR_TIMEOUT - if the DMA transfer timed out.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaBuffer_t
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaMultiChannelDestroy
    *\see CyU3PDmaMultiChannelSetXfer
    *\see CyU3PDmaMultiChannelCommitBuffer
    *\see CyU3PDmaMultiChannelDiscardBuffer
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelGetBuffer (
        CyU3PDmaMultiChannel *handle,           /**< Handle to the multi-channel to be modified. */
        CyU3PDmaBuffer_t *buffer_p,             /**< Output parameter that will be filled with address, size and status
                                                     of the buffer with received data. */
        uint32_t waitOption                     /**< Duration for which to wait for data. */
        );

/** \brief Commit the buffer to be sent to the consumer.

    <B>Description</B>\n
    This function is generally used with manual DMA channels, and allows the user
    to commit a data buffer which is to be sent out to the consumer. In the case of
    one-to-many channels, the buffer will be sent out through the consumer channels
    on a round-robin basis.

    The count provided is the exact size of the data that is to be sent out of the
    consumer socket.

    This API can be used with many-to-one AUTO DMA channels in the special case where
    the consumer socket is suspended. This allows the user to modify the data content
    of a partial data buffer, and then commit it for sending out of the device.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if this operation is not supported for the DMA channel type.\n
    * CY_U3P_ERROR_NOT_STARTED - if the DMA channel is not started.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE  - if the DMA channel is not in the required state.\n
    * CY_U3P_ERROR_DMA_FAILURE - if the DMA transfer failed.\n
    * CY_U3P_ERROR_ABORTED - if the DMA transfer was aborted.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.\n

    **\see
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaMultiChannelSetXfer
    *\see CyU3PDmaMultiChannelGetBuffer
    *\see CyU3PDmaMultiChannelDiscardBuffer
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelCommitBuffer (
        CyU3PDmaMultiChannel *handle,           /**< Handle to the multi-channel to be modified. */
        uint16_t count,                         /**< Size of the memory buffer being committed. The address of the
                                                     buffer is implicit and is taken from the active descriptor. */
        uint16_t bufStatus                      /**< Status of the buffer being committed. */
        );

/** \brief Drop a data buffer without sending out to the consumer.

    <B>Description</B>\n
    This function drops the content of the active buffer by moving the consumer
    socket ahead to the next buffer. This function is generally used with DMA
    channels of type MANUAL and MANUAL_IN.

    In the case of many-to-one AUTO DMA channels, this API is used in the special case
    where the consumer socket is suspended using the CY_U3P_DMA_SCK_SUSP_CONS_PARTIAL_BUF
    option. This allows the user to drop the content of a partially filled buffer
    without sending it out to the consumer.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if this operation is not supported for the DMA channel type.\n
    * CY_U3P_ERROR_NOT_STARTED - if the DMA channel is not started.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE  - if the DMA channel is not in the required state.\n
    * CY_U3P_ERROR_DMA_FAILURE - if the DMA transfer failed.\n
    * CY_U3P_ERROR_ABORTED - if the DMA transfer was aborted.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaMultiChannelSetXfer
    *\see CyU3PDmaMultiChannelGetBuffer
    *\see CyU3PDmaMultiChannelCommitBuffer
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelDiscardBuffer (
        CyU3PDmaMultiChannel *handle            /**< Handle to the multi-channel to be modified. */
        );

/** \brief Wait for the current DMA transaction to complete on a DMA multi-channel.

    <B>Description</B>\n
    This function waits until the current transfer on the DMA channel is completed,
    or until the specified timeout period has elapsed. This function is not supported
    if the DMA channel has been configured for infinite transfers.

    This function should not be called from a DMA callback function. If the function
    returns CY_U3P_ERROR_TIMEOUT, the wait API call can be repeated without affecting
    the data transfer.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - if this operation is not supported for the DMA channel type.\n
    * CY_U3P_ERROR_NOT_STARTED - if the DMA channel is not started.\n
    * CY_U3P_ERROR_DMA_FAILURE - if the DMA transfer failed.\n
    * CY_U3P_ERROR_ABORTED - if the DMA transfer was aborted.\n
    * CY_U3P_ERROR_TIMEOUT - if the DMA transfer timed out.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaMultiChannel
    *\see CyU3PDmaMultiChannelSetXfer
    *\see CyU3PDmaMultiChannelSetupSendBuffer
    *\see CyU3PDmaMultiChannelSetupRecvBuffer
    *\see CyU3PDmaMultiChannelWaitForRecvBuffer
    *\see CyU3PDmaMultiChannelReset
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelWaitForCompletion (
        CyU3PDmaMultiChannel *handle,           /**< Handle to the multi-channel to wait on. */
        uint32_t waitOption                     /**< Duration for which to wait. */
        );

/** \brief This functions returns the current multi-channel status.

    <B>Description</B>\n
    This function returns the current state of the DMA channel as well as the
    current transfer counts on both producer and consumer sockets.

    <B>Note</B>\n
    The FX3 device only updates the transfer count values at buffer boundaries, and it
    is not possible to identify the transfer count at a partial buffer level.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the sckIndex is invalid.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.

    **\see
    *\see CyU3PDmaState_t
    *\see CyU3PDmaMultiChannelSetXfer
 */
CyU3PReturnStatus_t
CyU3PDmaMultiChannelGetStatus (
        CyU3PDmaMultiChannel *handle,           /**< Handle to the DMA channel to query. */
        CyU3PDmaState_t *state,                 /**< Output parameter that will be filled with state of the channel. */
        uint32_t *prodXferCount,                /**< Output parameter that will be filled with transfer count on the
                                                     producer socket in DMA mode units. */
        uint32_t *consXferCount,                /**< Output parameter that will be filled with transfer count on the
                                                     consumer socket in DMA mode units. */
        uint8_t   sckIndex                      /**< The socket index for retrieving information transfer counts. */
        );

/** \brief This function is used to enable/disable the Data cache coherency handling on a
    per DMA channel basis.

    <B>Description</B>\n
    The DMA driver in the FX3 library assumes that any manual data transfer on a DMA
    channel can be affected by the Data cache. Therefore, it ensures that the data buffer
    region is evicted from the cache before reading any data from a newly produced buffer.
    It also ensures that the data buffer region is written back to memory before commiting
    the buffer to the consumer.

    These cache operations can limit the transfer performance obtained on the DMA channel
    in some cases. If the caller can ensure that the contents of the data buffer are accessed
    by the firmware only in very rare cases, it is possible to get better performance by
    removing these cache operations.

    This API is used to selectively turn off the data cache handling on a per DMA channel
    basis. This should only be used with caution, and the caller should ensure that the
    cache APIs are used directly where required.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_NULL_POINTER - if any pointer passed as parameter is NULL.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if the DMA channel was not configured.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - if the DMA channel mutex could not be acquired.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - if the DMA channel is not idle (configured state).

    **\see
    * CyU3PDmaMultiChannel
    * CyU3PDeviceCacheControl
 */
CyU3PReturnStatus_t
CyU3PDmaMultiChannelCacheControl (
        CyU3PDmaMultiChannel *handle,           /**< Handle to the DMA channel. */
        CyBool_t isDmaHandleDCache              /**< Whether to enable handling or not. */
        );

/** \brief Enable creation and management of DMA multicast channels.

    <B>Description</B>\n
    It is expected that DMA multicast channels will only rarely be used in FX3 applications. Since the
    multichannel creation code takes in the channel type as a parameter and then calls the appropriate
    handler functions, the code to setup and work with multicast channels gets linked into any FX3
    application that uses multichannels, leading to un-necessary loss of code space. This function is
    provided to prevent this memory loss.
    
    The multicast channel code will only be linked into the FX3 application if this function has been
    called. Therefore, this function needs to be called by the application before any multicast channels
    are created.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PDmaMultiChannelCreate
 */
extern void
CyU3PDmaEnableMulticast (
        void);

/** \brief Select the active consumers on a multicast DMA channel.

    <B>Description</B>\n
    A multicast DMA channel has two or more consumer sockets, all of which receive data sent obtained through
    the producer socket. There may be cases where the firmware application needs to dynamically change the
    list of active consumers (that will receive the data). This API is used to select the active consumers
    on a multicast DMA channel.

    The consMask parameter is a bitmask which represents the active consumers. Bit n of this bitmask needs to
    be set if consumer n is to be activated. By default, all consumers on the channel are left active. This
    API can only be used when the DMA channel is the idle (configured) state.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the socket selection is successful.
    * CY_U3P_ERROR_NOT_SUPPORTED if the channel specified is not a multicast channel.
    * CY_U3P_ERROR_ALREADY_STARTED if the channel is already active (SetXfer called).
 */
extern CyU3PReturnStatus_t
CyU3PDmaMulticastSocketSelect (
        CyU3PDmaMultiChannel *chHandle,                 /* DMA channel handle to be modified. */
        uint32_t              consMask                  /* Bit mask representing active consumers. */
        );

/** \brief Disable a consumer on a multi-cast socket while transfer is in flight.

    <B>Description\n</B>
    The CyU3PDmaMulticastSocketSelect API allows user to the select the consumer sockets that
    should be enabled for a data transfer. However, this API can only be used while the channel
    is idle, and cannot be used to modify a transfer that is in flight.

    This API allows the user to disable one of the consumer sockets for a multicast transfer
    while the transfer is in flight (CyU3PDmaMultiChannelSetXfer has been called). Please note
    that the changes applied by this API are only valid for the ongoing transfer, and you
    will need to do a CyU3PDmaMultiChannelReset before starting the next transfer using the
    CyU3PDmaMultiChannelSetXfer API.

    <B>Return Value\n</B>
    * CY_U3P_SUCCESS if the socket disable is successful.
    * CY_U3P_ERROR_NOT_SUPPORTED if the channel specified is not a multicast channel.
    * CY_U3P_ERROR_INVALID_SEQUENCE if the channel is not in active transfer mode.
    * CY_U3P_ERROR_BAD_ARGUMENT if socket specified is out of range.
 */
extern CyU3PReturnStatus_t
CyU3PDmaMulticastDisableConsumer (
        CyU3PDmaMultiChannel *chHandle,                 /* DMA channel handle. */
        uint16_t              consIndex                 /* Index of the consumer to be disabled. */
        );

/** \brief Check whether a given DMA channel handle is valid.

    <B>Description</B>\n
    This function checks whether a DMA channel handle points to a valid DMA channel.

    <B>Return Value</B>\n
    * CyTrue if the channel handle is valid.\n
    * CyFalse if the channel handle is not valid.
 */
extern CyBool_t
CyU3PDmaChannelIsValid (
        CyU3PDmaChannel *handle);

/** \brief Check whether a given DMA multi-channel handle is valid.

    <B>Description</B>\n
    This function checks whether a DMA multi-channel handle points to a valid DMA multi-channel.

    <B>Return Value</B>\n
    * CyTrue if the channel handle is valid.\n
    * CyFalse if the channel handle is not valid.
 */
extern CyBool_t
CyU3PDmaMultiChannelIsValid (
        CyU3PDmaMultiChannel *handle);

/** \brief Get the DMA channel handle corresponding to a USB IN endpoint.

    <B>Description</B>\n
    This function retrieves the DMA channel handle corresponding to a USB IN endpoint.
    This function is intended for API internal usage (implementation of specific USB
    defect work-arounds) and is not expected to be called by user application code.

    <B>Return Value</B>\n
    * Pointer to DMA channel if it exists.
 */
extern CyU3PDmaChannel *
CyU3PDmaUsbInEpGetChannel (
        uint8_t ep);

/** \brief Get the DMA multi-channel handle corresponding to a USB IN endpoint.

    <B>Description</B>\n
    This function retrieves the DMA multi-channel handle corresponding to a USB IN
    endpoint. This function is intended for API internal usage (implementation of
    specific USB defect work-arounds) and is not expected to be called by user
    application code.

    <B>Return Value</B>\n
    * Pointer to DMA channel if it exists.
 */
extern CyU3PDmaMultiChannel *
CyU3PDmaUsbInEpGetMultiChannel (
        uint8_t ep);

/** \brief Suspend the USB IN (egress) socket corresponding to the DMA channel.

    <B>Description</B>\n
    This function forcibly suspends the USB IN (egress) socket corresponding to
    the DMA channel. This function is intended for API internal usage (implementation
    if specific USB defect work-arounds) and is not expected to be called by user
    application code.

    <B>Return Value</B>\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the channel handle is not valid.\n
    * CY_U3P_SUCCESS if the socket was suspended as requested.\n
    * CY_U3P_ERROR_TIMEOUT if the socket suspend operation timed out.

    **\see
    *\see CyU3PDmaChannelResumeUsbConsumer
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelSuspendUsbConsumer (
        CyU3PDmaChannel *handle,                /**< Handle to channel to be suspended. */
        uint32_t         waitOption             /**< Amount of time to wait for channel suspension. */
        );

/** \brief Resume the USB IN (egress) socket corresponding to the DMA channel.

    <B>Description</B>\n
    This function resumes the USB IN socket corresponding to the DMA channel. This
    will resume the socket only if it was suspended through the CyU3PDmaChannelSuspendUsbConsumer
    function.

    <B>Return Value</B>\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the channel handle is not valid.\n
    * CY_U3P_SUCCESS if the socket was resumed as requested.

    **\see
    *\see CyU3PDmaChannelSuspendUsbConsumer
 */
extern CyU3PReturnStatus_t
CyU3PDmaChannelResumeUsbConsumer (
        CyU3PDmaChannel *handle                 /**< Handle to channel to be resumed. */
        );

/** \brief Suspend the USB IN (egress) socket corresponding to the DMA multi-channel.

    <B>Description</B>\n
    This function forcibly suspends the USB IN (egress) socket corresponding to
    the DMA multi-channel. This function is intended for API internal usage (implementation
    if specific USB defect work-arounds) and is not expected to be called by user
    application code.

    Note: This function only works for MANY-TO-ONE DMA channels, and not for channels
    have multiple consumers.

    <B>Return Value</B>\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the channel handle is not valid.\n
    * CY_U3P_SUCCESS if the socket was suspended as requested.\n
    * CY_U3P_ERROR_TIMEOUT if the socket suspend operation timed out.

    **\see
    *\see CyU3PDmaMultiChannelResumeUsbConsumer
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelSuspendUsbConsumer (
        CyU3PDmaMultiChannel *handle,           /**< Handle to channel to be suspended. */
        uint32_t         waitOption             /**< Amount of time to wait for channel suspension. */
        );

/** \brief Resume the USB IN (egress) socket corresponding to the DMA multi-channel.

    <B>Description</B>\n
    This function resumes the USB IN socket corresponding to the DMA multi-channel. This
    will resume the socket only if it was suspended through the CyU3PDmaMultiChannelSuspendUsbConsumer
    function.

    <B>Return Value</B>\n
    * CY_U3P_ERROR_BAD_ARGUMENT if the channel handle is not valid.\n
    * CY_U3P_SUCCESS if the socket was resumed as requested.

    **\see
    *\see CyU3PDmaMultiChannelSuspendUsbConsumer
 */
extern CyU3PReturnStatus_t
CyU3PDmaMultiChannelResumeUsbConsumer (
        CyU3PDmaMultiChannel *handle            /**< Handle to channel to be resumed. */
        );

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3DMA_H_ */

/*[]*/
