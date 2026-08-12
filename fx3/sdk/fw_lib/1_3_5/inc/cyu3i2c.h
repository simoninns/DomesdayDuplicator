/*
 ## Cypress FX3 Core Library Header (cyu3i2c.h)
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

#ifndef _INCLUDED_CYU3I2C_H_
#define _INCLUDED_CYU3I2C_H_

#include <cyu3types.h>
#include <cyu3system.h>
#include <cyu3lpp.h>
#include "cyu3externcstart.h"

/** \file cyu3i2c.h
    \brief The I2C driver in the FX3 firmware provides a set of APIs to
    configure the I2C interface properties and to perform I2C data transfers
    from/to one or more slave devices.
 */

/**************************************************************************
 ******************************* Data Types *******************************
 **************************************************************************/

/** \brief List of I2C related event types.

    <B>Description</B>\n
    This enumerated type lists the various I2C related event codes that are
    sent to the user application through the I2C event callback.

    <B>Note</B>\n
    In the case of a DMA read of data that does not fill the DMA buffer(s)
    associated with the read DMA channel, the DMA transfer remains pending
    after the CY_U3P_I2C_EVENT_RX_DONE event is delivered. The data can only
    be retrieved from the DMA buffer after the DMA transfer is terminated
    through the CyU3PDmaChannelSetWrapUp API.

    **\see
    *\see CyU3PI2cError_t
    *\see CyU3PI2cIntrCb_t
 */
typedef enum CyU3PI2cEvt_t
{
    CY_U3P_I2C_EVENT_RX_DONE = 0,       /**< Reception is completed */
    CY_U3P_I2C_EVENT_TX_DONE,           /**< Transmission is done */
    CY_U3P_I2C_EVENT_TIMEOUT,           /**< Bus timeout has happened */
    CY_U3P_I2C_EVENT_LOST_ARBITRATION,  /**< Lost I2C bus arbitration, probably due to another I2C bus master. */
    CY_U3P_I2C_EVENT_ERROR              /**< I2C transfer has resulted in an error. */
} CyU3PI2cEvt_t;

/** \brief List of I2C specific error/status codes.

    <B>Description</B>\n
    This type lists the various I2C specific error/status codes that are sent to
    the event callback as event data, when the event type is CY_U3P_I2C_ERROR_EVT.

    **\see
    *\see CyU3PI2cEvt_t
    *\see CyU3PI2cIntrCb_t
 */
typedef enum CyU3PI2cError_t
{
    CY_U3P_I2C_ERROR_NAK_BYTE_0 = 0,          /**< Slave NACK-ed the zeroth byte of the preamble. */
    CY_U3P_I2C_ERROR_NAK_BYTE_1,              /**< Slave NACK-ed the first byte of the preamble. */
    CY_U3P_I2C_ERROR_NAK_BYTE_2,              /**< Slave NACK-ed the second byte of the preamble. */
    CY_U3P_I2C_ERROR_NAK_BYTE_3,              /**< Slave NACK-ed the third byte of the preamble. */
    CY_U3P_I2C_ERROR_NAK_BYTE_4,              /**< Slave NACK-ed the fourth byte of the preamble. */
    CY_U3P_I2C_ERROR_NAK_BYTE_5,              /**< Slave NACK-ed the fifth byte of the preamble. */
    CY_U3P_I2C_ERROR_NAK_BYTE_6,              /**< Slave NACK-ed the sixth byte of the preamble. */
    CY_U3P_I2C_ERROR_NAK_BYTE_7,              /**< Slave NACK-ed the seventh byte of the preamble. */
    CY_U3P_I2C_ERROR_NAK_DATA,                /**< Slave sent a NACK during the data phase of a transfer. */
    CY_U3P_I2C_ERROR_PREAMBLE_EXIT_NACK_ACK,  /**< Poll operation has exited due to the slave
                                                   returning an ACK or a NACK handshake. */
    CY_U3P_I2C_ERROR_PREAMBLE_EXIT,           /**< Poll operation with address repetition timed out. */
    CY_U3P_I2C_ERROR_NAK_TX_UNDERFLOW,        /**< Underflow in buffer during transmit/write operation. */
    CY_U3P_I2C_ERROR_NAK_TX_OVERFLOW,         /**< Overflow of buffer during transmit operation. */
    CY_U3P_I2C_ERROR_NAK_RX_UNDERFLOW,        /**< Underflow in buffer during receive/read operation. */
    CY_U3P_I2C_ERROR_NAK_RX_OVERFLOW          /**< Overflow of buffer during receive operation. */
} CyU3PI2cError_t;

/** \def CY_U3P_I2C_DEFAULT_LOCK_TIMEOUT
    \brief Delay duration to wait to get a lock on the I2C block.
 */
#define CY_U3P_I2C_DEFAULT_LOCK_TIMEOUT    (CYU3P_WAIT_FOREVER)

/** \brief Structure defining the I2C interface configuration.

    <B>Description</B>\n
    This structure encapsulates all of the configurable parameters that can be
    selected for the I2C interface. The CyU3PI2cSetConfig() function accepts a
    pointer to this structure, and updates all of the interface parameters.

    The I2C block can function in the bit rate range of 100 KHz to 1MHz. In the
    default mode of operation, the timeouts need to be kept disabled.

    In the register mode of operation (isDma is false), the data transfer APIs are
    blocking and return only after the requested amount of data has been read or
    written. In such a case, the I2C specific callbacks are meaningless; and the
    CyU3PI2cSetConfig API expects that no callback is specified when the register
    mode is selected.

    **\see
    *\see CyU3PI2cSetConfig
 */
typedef struct CyU3PI2cConfig_t
{
    uint32_t bitRate;           /**< Bit rate for the interface. (Eg: 100000 for 100KHz) */
    CyBool_t isDma;             /**< CyFalse: Register transfer mode, CyTrue: DMA transfer mode */
    uint32_t busTimeout;        /**< Number of core clocks SCK can be held low by the slave byte transmission
                                     before triggering a timeout error. 0xFFFFFFFFU means no timeout. */
    uint16_t dmaTimeout;        /**< Number of core clocks DMA can remain not ready before flagging an error.
                                     0xFFFF means no timeout. */
} CyU3PI2cConfig_t;

/** \brief Structure defining the preamble to be sent on the I2C interface.

    <B>Description</B>\n
    All I2C data transfers start with a preamble where the first byte contains
    the slave address and the direction of transfer. The preamble can optionally
    contain other bytes where device specific address values or other commands
    are sent to the slave device.

    The FX3 device supports associating a preamble with a maximum length of
    8 bytes to any I2C data transfer. This allows the user to specify a multi-byte
    preamble which covers the slave address, device specific address fields and then
    initiate the data transfer.

    The ctrlMask indicate the start / stop bit conditions after each byte of
    the preamble.

    For example, an I2C EEPROM read requires the byte address for the read to
    be written first. These two I2C operations can be combined into one I2C API
    call using the parameters of the structure.

    Typical I2C EEPROM page Read operation:

    Byte 0:\n
         Bit 7 - 1: Slave address.\n
         Bit 0    : 0 - Indicating this is a write from master.\n
    Byte 1, 2: Address to which the data has to be written.\n
    Byte 3:\n
         Bit 7 - 1: Slave address.\n
         Bit 0    : 1 - Indicating this is a read operation.\n
    The buffer field shall hold the above four bytes, the length field shall be
    four; and ctrlMask field will be 0x0004 as a start bit is required after the
    third byte (third bit is set).

    Typical I2C EEPROM page Write operation:

    Byte 0:\n
         Bit 7 - 1: Slave address.\n
         Bit 0    : 0 - Indicating this is a write from master.\n
    Byte 1, 2: Address to which the data has to be written.\n
    The buffer field shall hold the above three bytes, the length field shall be
    three, and the ctrlMask field is zero as no additional start/stop conditions are
    needed.

    Please note that the user is expected to the set direction bit in the preamble
    bytes properly based on the type of transfer to be performed. The API does not
    check whether the preamble direction matches the type of transfer API being
    called.

    **\see
    *\see CyU3PI2cTransmitBytes
    *\see CyU3PI2cReceiveBytes
 */
typedef struct CyU3PI2cPreamble_t
{
    uint8_t  buffer[8];         /**< The actual preamble bytes starting with slave address. */
    uint8_t  length;            /**< The length of the preamble to be sent. Should be between 1 and 8. */
    uint16_t ctrlMask;          /**< This field controls the start stop condition after every byte of
                                     preamble data. Bits 0 - 7 represent a bit mask for the start condition
                                     and Bits 8 - 15 represent a bit mask for stop condition. If both start
                                     and stop bits are set at one index; the stop condition takes priority. */
} CyU3PI2cPreamble_t;

/** \brief I2C event callback function type.

    <B>Description</B>\n
    This type defines the signature of the callback functions to be registered to
    receive I2C related events. I2C event callbacks can only be used in the DMA mode
    of data transfer.

    **\see
    *\see CyU3PI2cEvt_t
    *\see CyU3PI2cError_t
    *\see CyU3PRegisterI2cCallBack
 */
typedef void (*CyU3PI2cIntrCb_t) (
        CyU3PI2cEvt_t evt,      /**< Type of event that occured. */
        CyU3PI2cError_t error   /**< Specifies the actual error/status code when
                                     the event is of type CY_U3P_I2C_EVENT_ERROR. */
        );

/**************************************************************************
 *************************** Function prototypes **************************
 **************************************************************************/

/** \brief Starts the I2C interface block on the FX3.

    <B>Description</B>\n
    This function powers up the I2C interface block on the FX3 device and is
    expected to be the first I2C API function that is called by the application.
    This function also sets up the I2C interface at a default rate of 100KHz.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS               - When the Init is successful.\n
    * CY_U3P_ERROR_NOT_CONFIGURED  - When I2C has not been enabled in IO configuration.\n
    * CY_U3P_ERROR_ALREADY_STARTED - When the I2C has been already initialized.

    **\see
    *\see CyU3PI2cDeInit
    *\see CyU3PI2cSetConfig
 */
extern CyU3PReturnStatus_t
CyU3PI2cInit (
        void);

/** \brief Stops the I2C module.

    <B>Description</B>\n
    This function disables and powers off the I2C interface. This function can
    be used to shut off the interface, to save power when it is not in use.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS           - If the DeInit is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - If the I2C module has not been previously initialized.

    **\see
    *\see CyU3PI2cInit
 */
extern CyU3PReturnStatus_t
CyU3PI2cDeInit(
        void);

/** \brief Sets the I2C interface parameters.

    <B>Description</B>\n
    This function is used to configure the I2C master interface based on the
    desired baud rate and address length settings to talk to the desired slave.

    This function should be called repeatedly to change the settings if
    different settings are to be used to communicate with different slave
    devices. This can be called on the fly repetitively without calling
    CyU3PI2cInit.

    In DMA mode, the callback parameter "cb" passed to this function is used to
    notify the user of data transfer completion or error conditions. In register
    mode, all APIs are blocking in nature. So in these cases, the callback
    argument is not relevant and the user must pass NULL as cb when using the
    register mode.

    Bitrate calculation:
    The maximum bitrate supported is 1 MHz and minimum bitrate is 100 KHz. It should
    be noted that even though the dividers and the API allows frequencies above and
    below the rated range, the device behaviour is not guaranteed.
   
    The I2C block on the FX3 needs to be clocked at 10X the desired bit rate. As the
    I2C block clock is derived from the FX3 SYSTEM clock, the actual bit rate will
    not be the exact programmed value. As the hardware only supports clock division
    by integer and half dividers, the firmware uses the closest approximation possible.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS            - When the SetConfig is successful.\n
    * CY_U3P_ERROR_NOT_STARTED  - When the I2C has not been initialized.\n
    * CY_U3P_ERROR_NULL_POINTER - When the config parameter is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - When the arguments are incorrect.\n
    * CY_U3P_ERROR_TIMEOUT      - When there is timeout happening during configuration.\n
    * CY_U3P_ERROR_MUTEX_FAILURE- When there is a failure in acquiring a mutex lock.

    **\see
    * CyU3PI2cIntrCb_t
    * CyU3PI2cConfig_t
    * CyU3PI2cInit
    * CyU3PI2cSendCommand
    * CyU3PI2cTransmitBytes
    * CyU3PI2cReceiveBytes
 */
extern CyU3PReturnStatus_t
CyU3PI2cSetConfig (
        CyU3PI2cConfig_t *config,       /**< I2C configuration settings */
        CyU3PI2cIntrCb_t cb             /**< Callback for getting the events */
        );

/** \brief Initiate a read or write operation to the I2C slave.

    <B>Description</B>\n
    This function is used to send the extended preamble over the I2C bus. This
    is used in conjunction with data transfer phase in DMA mode. The function is
    also called from the API library for register mode operation.

    The byteCount represents the amount of data to be read or written in the
    data phase and the isRead parameter specifies the direction of transfer.
    The transfer will happen through the I2C Consumer / Producer DMA channel if
    the I2C interface is configured in DMA mode, or through the I2C Ingress/Egress
    registers if the interface is configured in register mode.

    The CyU3PI2cWaitForBlockXfer API or the CY_U3P_I2C_EVENT_RX_DONE or
    CY_U3P_I2C_EVENT_TX_DONE event callbacks can be used to detect the end of
    a DMA data transfer that is requested through this function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - When the SendCommand is successful.\n
    * CY_U3P_ERROR_NULL_POINTER   - When the preamble is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT   - If the preamble or byteCount arguments are invalid.\n
    * CY_U3P_ERROR_TIMEOUT        - When the I2C bus is busy.\n
    * CY_U3P_ERROR_MUTEX_FAILURE  - When there is a failure in acquiring a mutex lock.

    **\see
    *\see CyU3PI2cPreamble_t
    *\see CyU3PI2cSetConfig
    *\see CyU3PI2cTransmitBytes
    *\see CyU3PI2cReceiveBytes
    *\see CyU3PI2cWaitForAck
    *\see CyU3PI2cWaitForBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PI2cSendCommand (
        CyU3PI2cPreamble_t *preamble,   /**< Preamble information to be sent out before the data transfer. */
        uint32_t byteCount,             /**< Size of the transfer in bytes. */
        CyBool_t isRead                 /**< Direction of transfer: CyTrue: Read, CyFalse: Write. */
        );

/** \brief Writes data to an I2C Slave in register mode.

    <B>Description</B>\n
    This function is used to write data one byte at a time to an I2C slave.
    This function requires that the I2C interface be configured in register
    (non-DMA) mode. The function call can be repeated when CY_U3P_ERROR_TIMEOUT
    is returned without any error recovery. The retry is done continuously
    without any delay. If any delay is required, then it should be added by the
    caller.

    The API will return when FX3 has transmitted the data. If the slave device
    requires additional time for completing the write operation, then either
    sufficient delay must be provided; or the CyU3PI2cWaitForAck API can be used.
    Refer to the I2C slave datasheet for more details on how to identify when
    the write is complete.

    This function internally uses the CyU3PI2cSendCommand function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS                - When the TransmitBytes is successful.\n
    * CY_U3P_ERROR_NULL_POINTER     - If the preamble or data parameter is NULL.\n
    * CY_U3P_ERROR_FAILURE          - When a transfer fails with an error defined in CyU3PI2cError_t.\n
    * CY_U3P_ERROR_BLOCK_FAILURE    - When the I2C block encounters a fatal error and requires to be re-initialized.\n
    * CY_U3P_ERROR_NOT_CONFIGURED   - If the I2C block is not configured for register mode transfers.\n
    * CY_U3P_ERROR_TIMEOUT          - I2C bus timeout occurred.\n
    * CY_U3P_ERROR_LOST_ARBITRATION - Failure due to I2C arbitration error or invalid bus activity.\n
    * CY_U3P_ERROR_MUTEX_FAILURE    - Failure to obtain a lock on the I2C block.

    **\see
    *\see CyU3PI2cPreamble_t
    *\see CyU3PI2cSetConfig
    *\see CyU3PI2cSendCommand
    *\see CyU3PI2cReceiveBytes
    *\see CyU3PI2cWaitForAck
 */
extern CyU3PReturnStatus_t
CyU3PI2cTransmitBytes (
        CyU3PI2cPreamble_t *preamble,   /**< Preamble information to be sent out before the data transfer. */
        uint8_t *data,                  /**< Pointer to buffer containing data to be written. */
        uint32_t byteCount,             /**< Size of the transfer in bytes. */
        uint32_t retryCount             /**< Number of times to retry request in case of a slave NAK response. */
        );

/** \brief Read data from the I2C slave in register mode.

    <B>Description</B>\n
    This function is used to read data one byte at a time from an I2C slave.
    This function requires that the I2C interface be configured in register
    (non-DMA) mode. The function call can be repeated when CY_U3P_ERROR_TIMEOUT
    is returned without any error recovery. The retry is done continuously
    without any delay. If any delay is required, then it should be added by the
    caller.

    The API will return when FX3 has received the data. If the slave device
    requires additional time before responding to other commands, then either
    sufficient delay must be provided; or the CyU3PI2cWaitForAck API can be used.
    Refer to the I2C slave datasheet for more details on how to identify when
    the slave is ready.

    This function internally uses the CyU3PI2cSendCommand function.


    <B>Return value</B>\n
    * CY_U3P_SUCCESS                - When the transfer is completed successfully.\n
    * CY_U3P_ERROR_NULL_POINTER     - If the preamble or data parameter is NULL.\n
    * CY_U3P_ERROR_FAILURE          - When a transfer fails with an error defined in CyU3PI2cError_t.\n
    * CY_U3P_ERROR_BLOCK_FAILURE    - When the I2C block encounters a fatal error and requires to be re-initialized.\n
    * CY_U3P_ERROR_NOT_CONFIGURED   - If the I2C block is not configured for register mode transfers.\n
    * CY_U3P_ERROR_TIMEOUT          - I2C bus timeout occurred.\n
    * CY_U3P_ERROR_LOST_ARBITRATION - Failure due to I2C arbitration error or invalid bus activity.\n
    * CY_U3P_ERROR_MUTEX_FAILURE    - Failure to obtain a lock on the I2C block.

    **\see
    *\see CyU3PI2cPreamble_t
    *\see CyU3PI2cSetConfig
    *\see CyU3PI2cSendCommand
    *\see CyU3PI2cTransmitBytes
    *\see CyU3PI2cWaitForAck
 */
extern CyU3PReturnStatus_t
CyU3PI2cReceiveBytes (
        CyU3PI2cPreamble_t *preamble,   /**< Preamble information to be sent out before the data transfer. */
        uint8_t *data,                  /**< Pointer to buffer where the data is to be placed. */
        uint32_t byteCount,             /**< Size of the transfer in bytes. */
        uint32_t retryCount             /**< Number of times to retry request in case of a NAK response or error. */
        );

/** \brief Wait until the I2C slave ACKs the preamble.

    <B>Description</B>\n
    This function waits for a ACK handshake from the slave, and can be used
    to ensure that the slave device has reached a desired state before issuing
    the next transaction or shutting the interface down.
 
    The API repeats the provided preamble bytes continuously until all bytes of
    the preamble are ACKed, or the specified retryCount has been reached.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS                - When the device returns the desired handshake.\n
    * CY_U3P_ERROR_NULL_POINTER     - If the preamble is NULL.\n
    * CY_U3P_ERROR_FAILURE          - When a preamble transfer fails with error defined by CyU3PI2cError_t.\n
    * CY_U3P_ERROR_NOT_CONFIGURED   - When the I2C is not initialized or not configured.\n
    * CY_U3P_ERROR_BLOCK_FAILURE    - When the I2C block encounters a fatal error and requires to be re-initialized.\n
    * CY_U3P_ERROR_TIMEOUT          - I2C bus timeout occurred.\n
    * CY_U3P_ERROR_LOST_ARBITRATION - Failure due to I2C arbitration error or invalid bus activity.\n
    * CY_U3P_ERROR_MUTEX_FAILURE    - Failure to obtain a lock on the I2C block.

    **\see
    *\see CyU3PI2cPreamble_t
    *\see CyU3PI2cSendCommand
    *\see CyU3PI2cTransmitBytes
    *\see CyU3PI2cReceiveBytes
 */
extern CyU3PReturnStatus_t
CyU3PI2cWaitForAck (
        CyU3PI2cPreamble_t *preamble,   /**< Preamble information to be sent out before the data transfer. */
        uint32_t retryCount             /**< Number of times to retry request if the slave continues to NAK. */
        );

/** \brief Wait until the ongoing I2C data transfer is finished.

    <B>Description</B>\n
    This function waits until the ongoing DMA based I2C transaction is complete.
    The function returns when FX3 has finished transferring the requested amount of
    data.

    <B>Note</B>\n
    In the case of a DMA read of data that does not fill the DMA buffer(s) associated
    with the read DMA channel, the DMA transfer remains pending after this API call
    returns. The data can only be retrieved from the DMA buffer after the DMA transfer
    is terminated through the CyU3PDmaChannelSetWrapUp API.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS                - When the data transfer has been completed successfully.\n
    * CY_U3P_ERROR_NOT_CONFIGURED   - When the I2C is not initialized or not configured.\n
    * CY_U3P_ERROR_NOT_SUPPORTED    - If a I2C callback function has been registered.\n
    * CY_U3P_ERROR_FAILURE          - When there is a failure defined by CyU3PI2cError_t.\n
    * CY_U3P_ERROR_BLOCK_FAILURE    - When the I2C block encounters a fatal error and requires to be re-initialized.\n
    * CY_U3P_ERROR_TIMEOUT          - I2C bus timeout occurred.\n
    * CY_U3P_ERROR_LOST_ARBITRATION - Failure due to I2C arbitration error or invalid bus activity.\n
    * CY_U3P_ERROR_MUTEX_FAILURE    - Failure to obtain a lock on the I2C block.

    **\see
    *\see CyU3PI2cSendCommand
 */
extern CyU3PReturnStatus_t
CyU3PI2cWaitForBlockXfer (
        CyBool_t isRead         /**< Type of operation to wait on: CyTrue: Read, CyFalse: Write */
        );

/** \brief Retrieves the error code as defined by CyU3PI2cError_t.

    <B>Description</B>\n
    This function can be used to retrieve the actual I2C error code once the register
    mode I2C transfer APIs have returned the CY_U3P_ERROR_FAILURE error code.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - If the error code is fetched successfully.\n
    * CY_U3P_ERROR_NULL_POINTER   - When the error_p pointer passed is NULL.\n
    * CY_U3P_ERROR_NOT_STARTED    - When there is no error flagged.\n
    * CY_U3P_ERROR_MUTEX_FAILURE  - Failure to obtain a lock on the I2C block.

    **\see
    *\see CyU3PI2cTransmitBytes
    *\see CyU3PI2cReceiveBytes
    *\see CyU3PI2cWaitForAck
 */
extern CyU3PReturnStatus_t
CyU3PI2cGetErrorCode (
        CyU3PI2cError_t *error_p        /**< Return pointer to be filled with the error code. */
        );

/** \brief Register a callback function for I2C event notifications.

    <B>Description</B>\n
   This function registers a callback function that will be called for notification of
   I2C interrupts.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PI2cIntrCb_t
 */
extern void
CyU3PRegisterI2cCallBack (
        CyU3PI2cIntrCb_t i2cIntrCb      /**< Callback function pointer. */
        );

/** \brief Set the timeout duration for I2C read/write transfer APIs.

    <B>Description</B>\n
    The CyU3PI2cTransmitBytes, CyU3PI2cReceiveBytes, CyU3PI2cWaitForAck and CyU3PI2cWaitForBlockXfer
    APIs wait for the completion of the requested data transfer. The default timeout is 1 million
    loops with a delay of about 1 us.

    The default timeout for read, write and error recovery operations can be changed using this API.
    The loop count to be used during read and write operations can be set to different values.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the timeout duration is updated.
    * CY_U3P_ERROR_NOT_STARTED if the I2C block has not been initialized.

    **\see
    *\see CyU3PI2cTransmitBytes
    *\see CyU3PI2cReceiveBytes
    *\see CyU3PI2cWaitForAck
    *\see CyU3PI2cWaitForBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PI2cSetTimeout (
        uint32_t readLoopCnt,           /**< Wait loop count used during read operations. */
        uint32_t writeLoopCnt,          /**< Wait loop count used during write operations. */
        uint32_t flushLoopCnt           /**< Wait loop count used during flush (error recovery) operations. */
        );

/** \brief Send a stop condition on the I2C bus.

    <B>Description</B>\n
    This function overrides the I2C pins of the FX3 as GPIOs and drives a stop condition
    on the bus. This function requires the GPIO module to be initialized, and will return
    an error otherwise.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the stop sending is successful.
    * CY_U3P_ERROR_NOT_STARTED if the GPIO module has not been started.
 */
extern CyU3PReturnStatus_t
CyU3PI2cSendStopCondition (
        void);

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3I2C_H_ */

/*[]*/
