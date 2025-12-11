/*
 ## Cypress FX3 Core Library Header (cyu3socket.h)
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

#ifndef _INCLUDED_CYU3SOCKET_H_
#define _INCLUDED_CYU3SOCKET_H_

#include "cyu3types.h"
#include "cyu3descriptor.h"
#include "cyu3externcstart.h"

/** \file cyu3socket.h
    \brief All block based data transfers IN or OUT of the FX3 device are performed
    through hardware blocks called sockets. An end-to-end data flow through the FX3
    device (DMA channel) is created by tying two or more sockets together using a
    shared set of data buffers for intermediate storage. This file provides a set of
    low level APIs that operate on these sockets, and allow custom data pipes to be
    created.

    <B>Note</B>\n
    These functions are primarily used by the FX3 library to implement the DMA channel
    APIs. It is recommended that the channel APIs be used instead of directly calling
    these functions.
 */

/**************************************************************************
 ******************************* Data types *******************************
 **************************************************************************/

/** \brief DMA IP block IDs.

    <B>Description</B>\n
    The distributed DMA fabric on the FX3 device identifies sockets based on
    a two part address. The first part identifies the IP block which implements
    the socket, and the second part identifies the specific socket within the
    IP block.

    This type lists the various IP blocks on the FX3/FX3S devices that provide
    DMA sockets.

    **\see
    *\see CyU3PDmaSocketId_t
 */
typedef enum CyU3PBlockId_t
{
    CY_U3P_LPP_IP_BLOCK_ID = 0,         /**< LPP block contains UART, I2C, I2S and SPI. */
    CY_U3P_PIB_IP_BLOCK_ID = 1,         /**< P-port interface (GPIF) block. */
    CY_U3P_SIB_IP_BLOCK_ID = 2,         /**< Storage interface block (FX3S only). */
    CY_U3P_UIB_IP_BLOCK_ID = 3,         /**< USB interface block for egress sockets. */
    CY_U3P_UIBIN_IP_BLOCK_ID = 4,       /**< USB interface block for ingress sockets. */
    CY_U3P_NUM_IP_BLOCK_ID = 5,         /**< Sentinel value. Count of valid DMA IPs. */
    CY_U3P_CPU_IP_BLOCK_ID = 0x3F       /**< Special value used to denote CPU as DMA endpoint. */
} CyU3PBlockId_t;

/** \brief DMA socket register structure.

    <B>Description</B>\n
    Each hardware block on the FX3 device implements a number of DMA sockets through
    which it handles data transfers with the external world. Each DMA socket serves as
    an endpoint for an independent data stream going through the hardware block.
 
    Each socket has a set of registers associated with it, that reflect the configuration
    and status information for that socket. The CyU3PDmaSocket structure is a replica
    of the config/status registers for a socket and is designed to perform socket configuration
    and status checks directly from firmware.
 
    See the sock_regs.h header file for the definitions of the fields that make up each
    of these registers.

    **\see
    *\see CyU3PDmaSocketConfig_t
 */
typedef struct CyU3PDmaSocket_t
{
    uvint32_t dscrChain;                /**< The descriptor chain associated with the socket */
    uvint32_t xferSize;                 /**< The transfer size requested for this socket. The size can
                                             be specified in bytes or in terms of number of buffers,
                                             depending on the UNIT field in the status value. */
    uvint32_t xferCount;                /**< The completed transfer count for this socket. */
    uvint32_t status;                   /**< Socket configuration and status register. */
    uvint32_t intr;                     /**< Interrupt status register. */
    uvint32_t intrMask;                 /**< Interrupt mask register. */
    uvint32_t unused2[2];               /**< Reserved register space. */
    CyU3PDmaDescriptor_t activeDscr;    /**< Active descriptor information. See cyu3descriptor.h for definition. */
    uvint32_t unused19[19];             /**< Reserved register space. */
    uvint32_t sckEvent;                 /**< Generate event register. */
} CyU3PDmaSocket_t;

/** \brief DMA socket configuration structure.

    <B>Description</B>\n
    This structure holds all the configuration fields that can be directly updated
    on a DMA socket. Refer to the sock_regs.h file for the detailed break up into
    bit-fields for each of the structure members.

    **\see
    *\see CyU3PDmaSocket_t
    *\see CyU3PDmaSocketSetConfig
    *\see CyU3PDmaSocketGetConfig
 */
typedef struct CyU3PDmaSocketConfig_t
{
    uint32_t dscrChain;         /**< The descriptor chain associated with the socket. */
    uint32_t xferSize;          /**< Transfer size for the socket. */
    uint32_t xferCount;         /**< Transfer status for the socket. */
    uint32_t status;            /**< Socket status register. */
    uint32_t intr;              /**< Interrupt status. */
    uint32_t intrMask;          /**< Interrupt mask. */
} CyU3PDmaSocketConfig_t;

/** \brief DMA socket interrupt handler callback function.

    <B>Description</B>\n
    When the DMA channel APIs are used for data transfer, all socket specific
    interrupts are handled by the DMA driver and reported to the user through
    the DMA channel callbacks.

    If a particular socket is being controlled directly using the socket APIs
    in this file, the driver will be unable to determine how the socket interrupts
    should be handled. These interrupts are reported to the user application for
    handling through a callback function of this type.
 */
typedef void (*CyU3PDmaSocketCallback_t) (
        uint16_t sckId,         /**< ID of the socket triggering the interrupt. */
        uint32_t status         /**< Socket interrupt status. */
        );

/**************************************************************************
 ********************************* Macros *********************************
 **************************************************************************/

/** \cond SOCK_INTERNAL
 */

#define CY_U3P_DMA_LPP_MIN_CONS_SCK     (0)    /* The min consumer socket number for Serial IOs. */
#define CY_U3P_DMA_LPP_MAX_CONS_SCK     (4)    /* The max consumer socket number for Serial IOs. */
#define CY_U3P_DMA_LPP_MIN_PROD_SCK     (5)    /* The min producer socket number for Serial IOs. */
#define CY_U3P_DMA_LPP_MAX_PROD_SCK     (7)    /* The max producer socket number for Serial IOs. */
#define CY_U3P_DMA_LPP_NUM_SCK          (8)    /* The number of sockets for Serial IOs. */

#define CY_U3P_DMA_PIB_MIN_CONS_SCK     (0)    /* The min consumer socket number for GPIF-II. */
#define CY_U3P_DMA_PIB_MAX_CONS_SCK     (15)   /* The max consumer socket number for GPIF-II. */
#define CY_U3P_DMA_PIB_MIN_PROD_SCK     (0)    /* The min producer socket number for GPIF-II. */
#define CY_U3P_DMA_PIB_MAX_PROD_SCK     (31)   /* The max producer socket number for GPIF-II. */
#define CY_U3P_DMA_PIB_NUM_SCK          (32)   /* The number of sockets for GPIF-II. */

#define CY_U3P_DMA_UIB_MIN_CONS_SCK     (0)    /* The min consumer socket number for USB egress EPs. */
#define CY_U3P_DMA_UIB_MAX_CONS_SCK     (15)   /* The max consumer socket number for USB egress EPs. */
#define CY_U3P_DMA_UIB_NUM_SCK          (16)   /* The number of sockets for USB egress EPs. */

#define CY_U3P_DMA_UIBIN_MIN_PROD_SCK   (0)    /* The min producer socket number for USB ingress EPs. */
#define CY_U3P_DMA_UIBIN_MAX_PROD_SCK   (15)   /* The max producer socket number for USB ingress EPs. */
#define CY_U3P_DMA_UIBIN_NUM_SCK        (16)   /* The number of sockets for USB ingress EPs. */

#define CY_U3P_DMA_CPU_NUM_SCK          (2)    /* The number of sockets for CPU. */

#define CY_U3P_IP_BLOCK_POS             (8)       /* The ip block position. */
#define CY_U3P_IP_BLOCK_MASK            (0x3F)    /* The ip block mask. */
#define CY_U3P_DMA_SCK_MASK             (0xFF)    /* The DMA socket mask. */
#define CY_U3P_DMA_SCK_ID_MASK          (0x3FFF)  /* The DMA socket id mask. */

/**************************************************************************
 ********************** Global variable declarations **********************
 **************************************************************************/

extern CyU3PDmaSocket_t         *glDmaSocket[];       /* Array of all sockets on the device */
extern CyU3PDmaSocketCallback_t  glDmaSocketCB;       /* The registered socket interrupt handler. */

/** \endcond
 */

/** \def CyU3PDmaGetSckNum
    \brief Get the socket number field from the socket ID.
 */
#define CyU3PDmaGetSckNum(sckId) ((sckId) & CY_U3P_DMA_SCK_MASK)

/** \def CyU3PDmaGetIpNum
    \brief Get the ip number field from the socket ID.
 */
#define CyU3PDmaGetIpNum(sckId) \
    (((sckId) >> CY_U3P_IP_BLOCK_POS) & CY_U3P_IP_BLOCK_MASK)

/** \def CyU3PDmaGetSckId
    \brief Get the socket ID from the socket number and the ip number.
 */
#define CyU3PDmaGetSckId(ipNum,sckNum) (((sckNum) & CY_U3P_DMA_SCK_MASK) | \
        (((ipNum) & CY_U3P_IP_BLOCK_MASK) << CY_U3P_IP_BLOCK_POS))

/**************************************************************************
 *************************** Function prototypes **************************
 **************************************************************************/

/** \brief Validates the socket ID.

    <B>Description</B>\n
    The function validates if the given socket id exists on the device.
    CPU sockets are virtual sockets and so the function considers them as
    invalid. The function considers a socket invalid if the IP block that
    implements it has not been started.

    <B>Return value</B>\n
    * CyTrue if the socket is valid\n
    * CyFalse if the socket is invalid.

    **\see
    *\see CyU3PDmaSocketIsValidProducer
    *\see CyU3PDmaSocketIsValidConsumer
 */
extern CyBool_t
CyU3PDmaSocketIsValid (
        uint16_t sckId                  /**< Socket id to be validated. */
        );

/** \brief Check whether the specified socket can be used as a producer.

    <B>Description</B>\n
    Some of the DMA sockets on the FX3 device can only be used as consumers
    or egress sockets. This function checks whether the specified socket is
    valid and can be used as a producer.

    <B>Return value</B>\n
    * CyTrue if the socket is a valid producer.\n
    * CyFalse if the socket is invalid or if it is egress-only.

    **\see
    *\see CyU3PDmaSocketIsValid
    *\see CyU3PDmaSocketIsValidConsumer
 */
extern CyBool_t
CyU3PDmaSocketIsValidProducer (
        uint16_t sckId                  /**< Socket id to be validated. */
        );

/** \brief Check whether the specified socket can be used as a consumer.

    <B>Description</B>\n
    Some of the DMA sockets on the FX3 device can only be used as producers
    or ingress sockets. This function checks whether the specified socket is
    valid and can be used as a consumer.

    <B>Return value</B>\n
    * CyTrue if the socket is a valid consumer.\n
    * CyFalse if the socket is invalid or is ingress-only.

    **\see
    *\see CyU3PDmaSocketIsValid
    *\see CyU3PDmaSocketIsValidProducer
 */
extern CyBool_t
CyU3PDmaSocketIsValidConsumer (
        uint16_t sckId                  /**< Socket id to be validated. */
        );

/** \brief Sets the socket configuration.

    <B>Description</B>\n
    Updates the configuration for a DMA socket based on the values in the structure
    provided.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the sck_p pointer in NULL.

    **\see
    *\see CyU3PDmaSocketConfig_t
    *\see CyU3PDmaSocketGetConfig
 */
extern CyU3PReturnStatus_t
CyU3PDmaSocketSetConfig (
        uint16_t sckId,                 /**< Socket id whose configuration is to be updated. */
        CyU3PDmaSocketConfig_t *sck_p   /**< Pointer to structure containing socket configuration. */
        );

/** \brief Clears interrupt status on a socket.

    <B>Description</B>\n
    This function clears the interrupt status on a socket without affecting any of
    the other status fields.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the socket specified is not valid.
 */
extern CyU3PReturnStatus_t
CyU3PDmaSocketClearInterrupts (
        uint16_t sckId,                 /**< Socket id whose interrupt status is to be updated. */
        uint32_t intVal                 /**< Interrupt status to be cleared. */
        );

/** \brief Fetch the current socket configuration.

    <B>Description</B>\n
    Fetch the current configuration and status of a DMA socket.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - if the function call is successful.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - if the sck_p pointer in NULL.

    **\see
    *\see CyU3PDmaSocketSetConfig
 */
extern CyU3PReturnStatus_t
CyU3PDmaSocketGetConfig (
        uint16_t sckId,                 /**< Socket id whose configuration is to be retrieved. */
        CyU3PDmaSocketConfig_t *sck_p   /**< Output parameter to be filled with the socket configuration. */
        );

/** \brief Request a socket to wrap up.

    <B>Description</B>\n
    This function sets the bit that forces a socket with a partially filled buffer
    to wrap up the buffer. The function does not wait for the socket to wrap up.

    <B>Note</B>\n
    This API should be called after ensuring that the socket in question is not
    actively receiving data. Otherwise, this can result in data loss.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PDmaSocketEnable
    *\see CyU3PDmaSocketDisable
 */
extern void
CyU3PDmaSocketSetWrapUp (
        uint16_t sckId                  /**< Id of socket to be wrapped up. */
        );

/** \brief Disable the selected socket.

    <B>Description</B>\n
    This function disables the specified socket, and returns only after it has
    been disabled. If the socket is forcibly disabled during data transfer, there
    will be loss of data.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PDmaSocketEnable
 */
extern void
CyU3PDmaSocketDisable (
        uint16_t sckId                  /**< Id of socket to be disabled. */
        );

/** \brief Enable the selected socket.

    <B>Description</B>\n
    This function enables the specified DMA socket. This should be called only
    after the configuring the socket with valid values using the CyU3PDmaSocketSetConfig
    API.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PDmaSocketDisable
    *\see CyU3PDmaSocketSetConfig
 */
extern void
CyU3PDmaSocketEnable (
        uint16_t sckId                  /**< Id of socket to be enabled. */
        );

/** \brief Update the options for the socket suspend.

    <B>Description</B>\n
    This API updates the suspend options for the selected socket. This does not
    actually suspend the socket, and the socket suspension will only happen when
    the pre-conditions for the selected suspend option have been satisfied.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PDmaUpdateSocketResume
 */
extern void
CyU3PDmaUpdateSocketSuspendOption (
        uint16_t sckId,                 /**< Id of socket to set the option. */
        uint16_t suspendOption          /**< Suspend option. */
        );

/** \brief Resume a socket from the suspended state.

    <B>Description</B>\n
    This function clears all active suspend conditions on the selected socket and
    resumes its operation.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PDmaUpdateSocketSuspendOption
 */
extern void
CyU3PDmaUpdateSocketResume (
        uint16_t sckId,                 /**< Id of socket to set the option. */
        uint16_t suspendOption          /**< Active suspend options. This should match the suspend options as
                                             set by the CyU3PDmaUpdateSocketSuspendOption API. */
        );

/** \brief Send an event to the socket.

    <B>Description</B>\n
    Send a produce / consume event to the selected socket. This API is used to synchronize
    sockets on manual DMA channels.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PDmaSocketResume
 */
extern void
CyU3PDmaSocketSendEvent (
        uint16_t sckId,                 /**< Id of socket that should receive the event. */
        uint16_t dscrIndex,             /**< The descriptor index to associate with the event. */
        CyBool_t isOccupied             /**< Status of the buffer associated with the event. */
        );

/** \brief Register a callback handler for custom socket interrupts.

    <B>Description</B>\n
    This function registers a callback function that will be called to notify
    the user about socket interrupts. These are only used on sockets that are
    not associated with any DMA channels.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PDmaSocketCallback_t
 */
extern void
CyU3PDmaSocketRegisterCallback (
        CyU3PDmaSocketCallback_t cb     /**< Callback function pointer. Can be zero to unregister previous callback. */
        );

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3SOCKET_H_ */

/*[]*/
