/*
 ## Cypress FX3 Core Library Header (cyu3uart.h)
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

#ifndef _INCLUDED_CYU3_UART_H_
#define _INCLUDED_CYU3_UART_H_

#include "cyu3types.h"
#include "cyu3lpp.h"
#include "cyu3externcstart.h"

/** \file cyu3uart.h
    \brief The UART driver in FX3 firmware is responsible for handling data
    transfers through the UART interface on the device. This file defines the
    data structures and APIs for UART interface management.
 */

/**************************************************************************
 ******************************* Data Types *******************************
 **************************************************************************/

/** \brief List of UART related event types.

    <B>Description</B>\n
    This enumeration lists the various UART related event codes that are notified
    to the user application through an event callback.

    **\see
    *\see CyU3PUartError_t
    *\see CyU3PUartIntrCb_t
 */
typedef enum CyU3PUartEvt_t
{
    CY_U3P_UART_EVENT_RX_DONE = 0,      /**< UART data receive operation is complete. */
    CY_U3P_UART_EVENT_TX_DONE,          /**< UART data transmit operation is complete. */
    CY_U3P_UART_EVENT_ERROR,            /**< An error has been detected. */
    CY_U3P_UART_EVENT_RX_DATA           /**< Data is available in receive FIFO. */
} CyU3PUartEvt_t;

/** \brief List of UART specific error/status codes.

    <B>Description</B>\n
    This type lists the various UART specific error/status codes that are sent to
    the event callback as event data associated with CY_U3P_UART_ERROR_EVT events.

    **\see
    *\see CyU3PUartEvt_t
    *\see CyU3PUartIntrCb_t
 */
typedef enum CyU3PUartError_t
{
    CY_U3P_UART_ERROR_NAK_BYTE_0 = 0,           /**< Missing stop bit. */
    CY_U3P_UART_ERROR_RX_PARITY_ERROR = 1,      /**< RX parity error. */
    CY_U3P_UART_ERROR_TX_OVERFLOW = 12,         /**< Overflow of FIFO during transmit operation. */
    CY_U3P_UART_ERROR_RX_UNDERFLOW = 13,        /**< Underflow in FIFO during receive/read operation. */
    CY_U3P_UART_ERROR_RX_OVERFLOW = 14          /**< Overflow of FIFO during receive operation. */
} CyU3PUartError_t;

/** \brief List of baud rates supported by the UART.

    <B>Description</B>\n
    This enumeration lists the various baud rate settings that are supported by
    the UART interface and driver implementation. The specific baud rates achieved
    will be close approximations of these standard values based on the clock
    frequencies that can be obtained on the FX3 hardware. The actual baud rate
    acheived for SYS_CLK settings of 403.2MHz and 416MHz is also listed below.

    **\see
    *\see CyU3PUartConfig_t
 */
typedef enum CyU3PUartBaudrate_t
{
    CY_U3P_UART_BAUDRATE_100    = 100,     /**< Baud: 100,     Actual @403MHz: 100.00,     @416MHz: 100.00,    */
    CY_U3P_UART_BAUDRATE_300    = 300,     /**< Baud: 300,     Actual @403MHz: 300.00,     @416MHz: 300.01,    */
    CY_U3P_UART_BAUDRATE_600    = 600,     /**< Baud: 600,     Actual @403MHz: 600.00,     @416MHz: 599.99     */
    CY_U3P_UART_BAUDRATE_1200   = 1200,    /**< Baud: 1200,    Actual @403MHz: 1200.00,    @416MHz: 1200.01    */
    CY_U3P_UART_BAUDRATE_2400   = 2400,    /**< Baud: 2400,    Actual @403MHz: 2400.00,    @416MHz: 2399.96    */
    CY_U3P_UART_BAUDRATE_4800   = 4800,    /**< Baud: 4800,    Actual @403MHz: 4800.00,    @416MHz: 4800.15    */
    CY_U3P_UART_BAUDRATE_9600   = 9600,    /**< Baud: 9600,    Actual @403MHz: 9600.00,    @416MHz: 9599.41    */
    CY_U3P_UART_BAUDRATE_10000  = 10000,   /**< Baud: 10000,   Actual @403MHz: 10000.00,   @416MHz: 10000.00   */
    CY_U3P_UART_BAUDRATE_14400  = 14400,   /**< Baud: 14400,   Actual @403MHz: 14400.00,   @416MHz: 14400.44   */
    CY_U3P_UART_BAUDRATE_19200  = 19200,   /**< Baud: 19200,   Actual @403MHz: 19200.00,   @416MHz: 19202.36   */
    CY_U3P_UART_BAUDRATE_38400  = 38400,   /**< Baud: 38400,   Actual @403MHz: 38385.38,   @416MHz: 38404.73   */
    CY_U3P_UART_BAUDRATE_50000  = 50000,   /**< Baud: 50000,   Actual @403MHz: 50000.00,   @416MHz: 50000.00   */
    CY_U3P_UART_BAUDRATE_57600  = 57600,   /**< Baud: 57600,   Actual @403MHz: 57600.00,   @416MHz: 57585.83   */
    CY_U3P_UART_BAUDRATE_75000  = 75000,   /**< Baud: 75000,   Actual @403MHz: 75000.00,   @416MHz: 75036.08   */
    CY_U3P_UART_BAUDRATE_100000 = 100000,  /**< Baud: 100000,  Actual @403MHz: 100000.00,  @416MHz: 100000.00  */
    CY_U3P_UART_BAUDRATE_115200 = 115200,  /**< Baud: 115200,  Actual @403MHz: 115068.49,  @416MHz: 115299.33  */
    CY_U3P_UART_BAUDRATE_153600 = 153600,  /**< Baud: 153600,  Actual @403MHz: 153658.54,  @416MHz: 153392.33  */
    CY_U3P_UART_BAUDRATE_200000 = 200000,  /**< Baud: 200000,  Actual @403MHz: 200000.00,  @416MHz: 200000.00  */
    CY_U3P_UART_BAUDRATE_225000 = 225000,  /**< Baud: 225000,  Actual @403MHz: 225000.00,  @416MHz: 225108.23  */
    CY_U3P_UART_BAUDRATE_230400 = 230400,  /**< Baud: 230400,  Actual @403MHz: 230136.99,  @416MHz: 230088.50  */
    CY_U3P_UART_BAUDRATE_300000 = 300000,  /**< Baud: 300000,  Actual @403MHz: 300000.00,  @416MHz: 300578.03  */
    CY_U3P_UART_BAUDRATE_400000 = 400000,  /**< Baud: 400000,  Actual @403MHz: 400000.00,  @416MHz: 400000.00  */
    CY_U3P_UART_BAUDRATE_460800 = 460800,  /**< Baud: 460800,  Actual @403MHz: 462385.32,  @416MHz: 460176.99  */
    CY_U3P_UART_BAUDRATE_500000 = 500000,  /**< Baud: 500000,  Actual @403MHz: 499009.90,  @416MHz: 500000.00  */
    CY_U3P_UART_BAUDRATE_750000 = 750000,  /**< Baud: 750000,  Actual @403MHz: 752238.81,  @416MHz: 753623.19  */
    CY_U3P_UART_BAUDRATE_921600 = 921600,  /**< Baud: 921600,  Actual @403MHz: 916363.64,  @416MHz: 928571.43  */
    CY_U3P_UART_BAUDRATE_1M     = 1000000, /**< Baud: 1000000, Actual @403MHz: 1008000.00, @416MHz: 1000000.00 */
    CY_U3P_UART_BAUDRATE_2M     = 2000000, /**< Baud: 2000000, Actual @403MHz: 2016000.00, @416MHz: 2000000.00 */
    CY_U3P_UART_BAUDRATE_3M     = 3000000, /**< Baud: 3000000, Actual @403MHz: 2964705.88, @416MHz: 3058823.52 */
    CY_U3P_UART_BAUDRATE_4M     = 4000000, /**< Baud: 4000000, Actual @403MHz: 3876923.08, @416MHz: 4000000.00 */
    CY_U3P_UART_BAUDRATE_4M608K = 4608000  /**< Baud: 4608000, Actual @403MHz: 4581818.18, @416MHz: 4727272.72 */
} CyU3PUartBaudrate_t;

/** \brief List of number of stop bits to be used in UART communication.

    <B>Description</B>\n
    This enumeration lists the various number of stop bit settings that the
    UART interface can be configured to have.

    **\see
    *\see CyU3PUartConfig_t
 */
typedef enum CyU3PUartStopBit_t
{
    CY_U3P_UART_ONE_STOP_BIT = 1,               /**< 1 stop bit. */
    CY_U3P_UART_TWO_STOP_BIT = 2                /**< 2 stop bits. */
} CyU3PUartStopBit_t;

/** \brief List of parity settings supported by the UART interface.

    <B>Description</B>\n
    This enumeration lists the various parity settings that the UART interface
    can be configured to support.

    **\see
    *\see CyU3PUartConfig_t
 */
typedef enum CyU3PUartParity_t
{
    CY_U3P_UART_NO_PARITY = 0,                  /**< No parity bits. */
    CY_U3P_UART_EVEN_PARITY,                    /**< Even parity. */
    CY_U3P_UART_ODD_PARITY,                     /**< Odd parity. */
    CY_U3P_UART_NUM_PARITY                      /**< Number of parity enumerations. */
} CyU3PUartParity_t;

/** \def CY_U3P_UART_DEFAULT_LOCK_TIMEOUT
    \brief Delay duration to wait to get a lock on the UART block.
 */
#define CY_U3P_UART_DEFAULT_LOCK_TIMEOUT    (CYU3P_WAIT_FOREVER)

/** \brief Configuration parameters for the UART interface.

    <B>Description</B>\n
    This structure defines all of the configurable parameters for the UART
    interface such as baud rate, stop bits, parity bits etc. A pointer to this
    structure is passed in to the CyU3PUartSetConfig function to configure
    the UART interface.
 
    The isDma member specifies whether the UART should be configured to transfer
    data one byte at a time, or in terms of larger blocks.
 
    All of the parameters can be changed dynamically by calling the CyU3PUartSetConfig
    function repeatedly.

    **\see
    *\see CyU3PUartBaudrate_t
    *\see CyU3PUartStopBit_t
    *\see CyU3PUartParity_t
    *\see CyU3PUartSetConfig
 */
typedef struct CyU3PUartConfig_t
{
    CyBool_t             txEnable;              /**< Enable the transmitter. */
    CyBool_t             rxEnable;              /**< Enable the receiver. */
    CyBool_t             flowCtrl;              /**< Enable hardware flow control for both RX and TX. */
    CyBool_t             isDma;                 /**< CyFalse: Byte by byte transfer; CyTrue: Block based transfer. */
    CyU3PUartBaudrate_t  baudRate;              /**< Baud rate for data transfer. */
    CyU3PUartStopBit_t   stopBit;               /**< The number of stop bits appended. */
    CyU3PUartParity_t    parity;                /**< Parity configuration. */
} CyU3PUartConfig_t;

/** \brief Prototype of UART event callback function.

    <B>Description</B>\n
    This function type defines a callback to be called when UART interrupts are
    received. A function of this type can be registered with the UART driver
    as a callback function and will be called whenever an event of interest occurs.

    The UART has to be configured for DMA mode of transfer for callbacks to be
    registered.

    **\see
    *\see CyU3PUartEvt_t
    *\see CyU3PUartError_t
    *\see CyU3PRegisterUartCallBack
 */
typedef void (*CyU3PUartIntrCb_t) (
        CyU3PUartEvt_t evt,             /**< Type of event that occured. */
        CyU3PUartError_t error          /**< Error/status code associated with CY_U3P_UART_EVENT_ERROR events. */
        );

/**************************************************************************
 *************************** Function prototypes **************************
 **************************************************************************/

/** \brief Starts the UART block on the FX3 device.

    <B>Description</B>\n
    This function powers up the UART hardware block on the device and should be the
    first UART related function called by the application.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS               - if the init was successful.\n
    * CY_U3P_ERROR_ALREADY_STARTED - if the UART block had been previously initialized.\n
    * CY_U3P_ERROR_NOT_CONFIGURED  - if UART was not enabled during IO configuration.

    **\see
    *\see CyU3PUartDeInit
    *\see CyU3PUartSetConfig
    *\see CyU3PUartTransmitBytes
    *\see CyU3PUartReceiveBytes
    *\see CyU3PUartTxSetBlockXfer
    *\see CyU3PUartRxSetBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PUartInit (
        void);

/** \brief Stops the UART hardware block.

    <B>Description</B>\n
    This function disables and powers off the UART hardware block on the device.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS           - if the de-init was successful.\n
    * CY_U3P_ERROR_NOT_STARTED - if UART had not been initialized.

    **\see
    *\see CyU3PUartInit
 */
extern CyU3PReturnStatus_t
CyU3PUartDeInit(
        void);

/** \brief Sets the UART interface parameters.

    <B>Description</B>\n
    This function configures the UART block with the desired user parameters such
    as transfer mode, baud rate etc. This function should be called repeatedly to
    make any change to the set of configuration parameters.

    Baudrate calculation:
    The FX3 UART supports baud rates between 100 Hz and 4 MHz. The UART block needs
    to be clocked at 16X the external baud rate. Since the block clock is derived
    from the SYS_CLK frequency through integer and half dividers, it is only possible
    to set the baud rate to an approximation of the desired value.

    See the documentation for the CyU3PUartBaudrate_t for a list of actual baud rates
    that are achieved for various settings.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS             - if the configuration was set successfully.\n
    * CY_U3P_ERROR_NOT_STARTED   - if UART was not initialized.\n
    * CY_U3P_ERROR_NULL_POINTER  - if a NULL pointer is passed.\n
    * CY_U3P_ERROR_BAD_ARGUMENT  - if any of the parameters are invalid.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - Failed to get a mutex lock on the UART block.

    **\see
    *\see CyU3PUartConfig_t
    *\see CyU3PUartIntrCb_t
    *\see CyU3PUartInit
    *\see CyU3PUartTransmitBytes
    *\see CyU3PUartReceiveBytes
    *\see CyU3PUartTxSetBlockXfer
    *\see CyU3PUartRxSetBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PUartSetConfig (
        CyU3PUartConfig_t *config,      /**< Pointer to structure containing config information. */
        CyU3PUartIntrCb_t cb            /**< UART callback function to be registered. */
        );

/** \brief Sets the number of bytes to be transmitted by the UART.

    <B>Description</B>\n
    This function sets the size of the desired data transmission through the
    UART. The value 0xFFFFFFFFU can be used to specify infinite or indefinite
    data transmission.
 
    This function is to be used when the UART is configured for DMA mode of
    transfer. If this function is called when the UART is configured in register
    mode, it will return with an error.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - if the transfer size was set successfully.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if UART was not configured for DMA mode.\n
    * CY_U3P_ERROR_MUTEX_FAILURE  - Failed to get a mutex lock on the UART block.

    **\see
    *\see CyU3PUartSetConfig
    *\see CyU3PUartRxSetBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PUartTxSetBlockXfer (
        uint32_t txSize                 /**< Desired transfer size. */
        );

/** \brief Sets the number of bytes to be received by the UART.

    <B>Description</B>\n
    This function sets the size of the desired data reception through the UART. The
    value 0xFFFFFFFFU can be used to specify infinite or indefinite data reception.

    This function is to be used when the UART is configured for DMA mode of
    transfer. If this function is called when the UART is configured in register
    mode, it will return with an error.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - if the transfer size was set successfully.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - if UART was not configured for DMA mode.\n
    * CY_U3P_ERROR_MUTEX_FAILURE  - Failed to get a mutex lock on the UART block.

    **\see
    *\see CyU3PUartSetConfig
    *\see CyU3PUartTxSetBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PUartRxSetBlockXfer (
        uint32_t rxSize                 /**< Desired transfer size. */
        );

/** \brief Transmit data through the UART interface in register mode.

    <B>Description</B>\n
    This function is used to transfer the specified number of bytes out through
    the UART in register mode. This function can only be used if the UART has been
    configured for register (non-DMA) transfer mode.

    <B>Return value</B>\n
    * Number of bytes that are successfully transferred.

    **\see
    *\see CyU3PUartSetConfig
    *\see CyU3PUartReceiveBytes
 */
extern uint32_t
CyU3PUartTransmitBytes (
        uint8_t *data_p,                /**< Pointer to the data to be transferred. */
        uint32_t count,                 /**< Number of bytes to be transferred. */
        CyU3PReturnStatus_t *status     /**< Status returned from the operation. */
        );

/** \brief Receive data from the UART interface in register mode.

    <B>Description</B>\n
    This function is used to read the specified number of bytes from the UART in
    register mode. This function can only be used if the UART has been configured for
    register (non-DMA) transfer mode.

    <B>Return value</B>\n
    * Number of bytes that are successfully received.

    **\see
    *\see CyU3PUartSetConfig
    *\see CyU3PUartTransmitBytes
 */
extern uint32_t
CyU3PUartReceiveBytes (
        uint8_t *data_p,                /**< Pointer to location where the data read is to be placed. */
        uint32_t count,                 /**< Number of bytes to be received. */
        CyU3PReturnStatus_t *status     /**< Status returned from the operation. */
        );

/** \brief This function registers a callback function for notification of UART interrupts.

    <B>Description</B>\n
    This function registers a callback function that will be called for notification of
    UART interrupts. This can also been done by the CyU3PUartSetConfig API.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PUartIntrCb_t
    *\see CyU3PUartSetConfig
 */
extern void
CyU3PRegisterUartCallBack (
        CyU3PUartIntrCb_t uartIntrCb    /**< Callback function pointer. */
        );

/** \brief Set the timeout for UART byte-mode transmit and receive operations.

    <B>Description</B>\n
    The timeout setting for the CyU3PUartTransmitBytes and CyU3PUartReceiveBytes operations
    is based on the number of loop iterations used while waiting for the transfer completion.
    Each loop iteration will require two times the period of the UART clock. The default
    timeout is set to 0xFFFFF and can be changed using this API.

    <B>Note</B>\n
    The API keeps the CPU spinning until the data transfer is completed, or the specified
    timeout period has elapsed. Therefore, it is not desirable to set a very large timeout
    for these operations.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the timeout setting is updated.
    * CY_U3P_ERROR_NOT_STARTED if the UART module is not enabled.

    **\see
    *\see CyU3PUartTransmitBytes
    *\see CyU3PUartReceiveBytes
 */
extern CyU3PReturnStatus_t
CyU3PUartSetTimeout (
        uint32_t readLoopCnt,           /**< Number of status checks to be performed during read operations. */
        uint32_t writeLoopCnt           /**< Number of status checks to be performed during write operations. */
        );

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3_UART_H_ */

/*[]*/

