/*
 ## Cypress FX3 Core Library Header (cyu3spi.h)
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

#ifndef _INCLUDED_CYU3SPI_H_
#define _INCLUDED_CYU3SPI_H_

#include "cyu3types.h"
#include "cyu3lpp.h"
#include "cyu3externcstart.h"

/** \file cyu3spi.h
    \brief SPI (Serial Peripheral Interface) is a serial interface defined for
    inter-device communication. The FX3 device includes a SPI master that can
    connect to a variety of SPI slave devices and function at various speeds and
    modes. The SPI driver module provides functions to configure the SPI interface
    parameters and to perform data read/writes from/to the slave devices.
 */

/**************************************************************************
 ******************************* Data Types *******************************
 **************************************************************************/

/** \def CY_U3P_SPI_DEFAULT_LOCK_TIMEOUT
    \brief Delay duration to wait to get a lock on the SPI block.
 */
#define CY_U3P_SPI_DEFAULT_LOCK_TIMEOUT    (CYU3P_WAIT_FOREVER)

/** \brief List of SPI related event types.

    <B>Description</B>\n
   This enumeration lists the various SPI related event codes that are notified
   to the user application through an event callback.

    **\see
    *\see CyU3PSpiError_t
    *\see CyU3PSpiIntrCb_t
 */
typedef enum CyU3PSpiEvt_t
{
    CY_U3P_SPI_EVENT_RX_DONE = 0,       /**< Reception is completed */
    CY_U3P_SPI_EVENT_TX_DONE,           /**< Transmission is done */
    CY_U3P_SPI_EVENT_ERROR              /**< Error has occurred */
} CyU3PSpiEvt_t;

/** \brief List of SPI specific error / status codes.

    <B>Description</B>\n
   This type lists the various SPI specific error / status codes that are sent
   to the event callback as event data, when the event type is CY_U3P_SPI_EVENT_ERROR.

    **\see
    *\see CyU3PSpiEvt_t
 */
typedef enum CyU3PSpiError_t
{
    CY_U3P_SPI_ERROR_TX_OVERFLOW = 12,          /**< Write to TX_FIFO when FIFO is full. */
    CY_U3P_SPI_ERROR_RX_UNDERFLOW = 13,         /**< Read from RX_FIFO when FIFO is empty. */
    CY_U3P_SPI_ERROR_NONE = 15                  /**< No error has happened. */
} CyU3PSpiError_t;

/** \brief Enumeration defining the lead and lag times of SSN with respect to SCK.

    <B>Description</B>\n
   The SSN needs to lead the SCK at the beginning of a transaction and SSN needs
   to lag the SCK at the end of a transfer. This enumeration gives customization
   allowed for this.  This enumeration is required only for hardware controlled SSN.

    **\see
    *\see CyU3PSpiConfig_t
    *\see CyU3PSpiSetConfig
 */
typedef enum CyU3PSpiSsnLagLead_t
{
    CY_U3P_SPI_SSN_LAG_LEAD_ZERO_CLK = 0,       /**< SSN is in sync with SCK. */
    CY_U3P_SPI_SSN_LAG_LEAD_HALF_CLK,           /**< SSN leads / lags SCK by a half clock cycle. */
    CY_U3P_SPI_SSN_LAG_LEAD_ONE_CLK,            /**< SSN leads / lags SCK by one clock cycle. */
    CY_U3P_SPI_SSN_LAG_LEAD_ONE_HALF_CLK,       /**< SSN leads / lags SCK by one and half clock cycles. */
    CY_U3P_SPI_NUM_SSN_LAG_LEAD                 /**< Number of possible values. */
} CyU3PSpiSsnLagLead_t;

/** \brief Enumeration defining the various ways in which the SSN for a SPI
   device can be controlled.
 
    <B>Description</B>\n
    The SSN line can be controlled by the firmware or the FX3 hardware.

    If the SSN is controlled by the firmware API, it will be asserted at the
    beginning of a transfer and is de-asserted at the end of the transfer. If
    it is controlled by hardware, the SSN assertion and de-assertion is done in
    sync with the SPI clock.
 
    In addition to these modes, the SSN can be controlled by the firmware
    application through a GPIO. In this case, it is the responsibility of the
    application to drive the line appropriately.
 
    The APIs allow only control of one SSN line. The SSN for any additional
    slaves must be controlled through GPIOs by the application.

    **\see
    *\see CyU3PSpiConfig_t
    *\see CyU3PSpiSetConfig
 */
typedef enum CyU3PSpiSsnCtrl_t
{
    CY_U3P_SPI_SSN_CTRL_FW = 0,         /**< SSN is controlled by API and is not at clock boundaries. */
    CY_U3P_SPI_SSN_CTRL_HW_END_OF_XFER, /**< SSN is controlled by hardware and is done in sync with clock.
                                             The SSN is asserted at the beginning of a transfer, and de-asserted
                                             at the end of a transfer or when no data is available to transmit. */
    CY_U3P_SPI_SSN_CTRL_HW_EACH_WORD,   /**< SSN is controlled by the hardware and is done in sync with clock.
                                             The SSN is asserted at the beginning of transfer of every word, and
                                             de-asserted at the end of the transfer of that word. */
    CY_U3P_SPI_SSN_CTRL_HW_CPHA_BASED,  /**< If CPHA is 0, the SSN control is per word; and if CPHA is 1, then the
                                             SSN control is per transfer. */
    CY_U3P_SPI_SSN_CTRL_NONE,           /**< SSN control is done externally by the firmware application. */
    CY_U3P_SPI_NUM_SSN_CTRL             /**< Number of enumerations. */
} CyU3PSpiSsnCtrl_t;

/** \brief Structure defining the configuration of SPI interface.

    <B>Description</B>\n
   This structure encapsulates all of the configurable parameters that can be
   selected for the SPI interface. The CyU3PSpiSetConfig() function accepts a
   pointer to this structure, and updates all of the interface parameters.

    **\see
    *\see CyU3PSpiSsnCtrl_t
    *\see CyU3PSpiSclkParam_t
    *\see CyU3PSpiSetConfig
 */
typedef struct CyU3PSpiConfig_t
{
    CyBool_t             isLsbFirst;    /**< Data shift mode - CyFalse: MSB first, CyTrue: LSB first */
    CyBool_t             cpol;          /**< Clock polarity - CyFalse(0): SCK idles low, CyTrue(1): SCK idles high */
    CyBool_t             cpha;          /**< Clock phase - CyFalse(0): Slave samples at idle-active edge,
                                             CyTrue(1): Slave samples at active-idle edge */
    CyBool_t             ssnPol;        /**< Polarity of SSN line - CyFalse (0): SSN is active low,
                                             CyTrue (1): SSN is active high. */
    CyU3PSpiSsnCtrl_t    ssnCtrl;       /**< SSN control method. */
    CyU3PSpiSsnLagLead_t leadTime;      /**< Time between SSN assertion and first SCLK edge. This is at the
                                             beginning of a transfer and is valid only when the hardware controls
                                             the SSN. A lead time of zero is not supported. */
    CyU3PSpiSsnLagLead_t lagTime;       /**< Time between the last SCK edge to SSN de-assertion. This is at the
                                             end of a transfer and is valid only when the hardware controls the
                                             SSN. When CPHA = 1, lag time cannot be zero. */
    uint32_t            clock;          /**< SPI clock frequency in Hz. */
    uint8_t             wordLen;        /**< Word length in bits. Valid values are in the range 4 - 32. */
} CyU3PSpiConfig_t;

/** \brief Prototype of SPI event callback function.

    <B>Description</B>\n
    This function type defines a callback to be called when an SPI interrupt has
    been detected. A function of this type can be registered with the SPI driver
   as a callback function and will be called whenever an event of interest occurs.

    **\see
    *\see CyU3PSpiEvt_t
    *\see CyU3PSpiError_t
    *\see CyU3PRegisterSpiCallBack
 */
typedef void (*CyU3PSpiIntrCb_t) (
        CyU3PSpiEvt_t evt,              /**< Type of event that occured. */
        CyU3PSpiError_t error           /**< Specifies the error/status code when the event is of type
                                             CY_U3P_SPI_EVENT_ERROR. */
        );

/**************************************************************************
 ************************** Function Prototypes ***************************
 **************************************************************************/

/** \brief Starts the SPI interface block on the device.

    <B>Description</B>\n
   This function powers up the SPI interface block on the device and is expected
   to be the first SPI API function that is called by the application.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS               - When the init is successful.\n
    * CY_U3P_ERROR_NOT_CONFIGURED  - When SPI has not been enabled in the IO configuration.\n
    * CY_U3P_ERROR_ALREADY_STARTED - When the SPI block has already been initialized.

    **\see
    *\see CyU3PSpiDeInit
    *\see CyU3PSpiSetConfig
    *\see CyU3PSpiSetSsnLine
    *\see CyU3PSpiTransmitWords
    *\see CyU3PSpiReceiveWords
    *\see CyU3PSpiTransferWords
    *\see CyU3PSpiSetBlockXfer
    *\see CyU3PSpiWaitForBlockXfer
    *\see CyU3PSpiDisableBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PSpiInit (
        void);

/** \brief Stops the SPI module.

    <B>Description</B>\n
   This function disables and powers off the SPI interface. This function can
   be used to shut off the interface to save power when it is not in use.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS           - When the de-init is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - When the SPI block has not been initialized.

    **\see
    *\see CyU3PSpiInit
 */
extern CyU3PReturnStatus_t
CyU3PSpiDeInit (
        void);

/** \brief Configures the SPI interface on the FX3 device.

    <B>Description</B>\n
    This function is used to configure the SPI master interface on FX3 based on the
    desired settings to talk to the slave. This function can be called repeatedly to
    change the settings, if different settings are to be used to communicate with
    different slave devices.
 
    If the DMA mode is transfer is used, the DMA channel(s) need to be reset after
    each fresh CyU3PSpiSetConfig API call.

   The callback parameter is used to specify an event callback function that
   will be called by the driver when an SPI interrupt occurs.

   SPI-clock calculation:
   The maximum SPI clock supported is 33MHz and the minimum is 10KHz. The SPI block
    needs to be clocked at 2X the interface bit rate. The clock for the SPI block
    is derived from the system clock through a frequency divider. As the hardware
    only supports integer and half dividers for the frequency, the firmware uses
    the closest approximation to the desired bit rate.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS                - When the SetConfig is successful.\n
    * CY_U3P_ERROR_NOT_STARTED      - When the SPI block has not been initialized.\n
    * CY_U3P_ERROR_NULL_POINTER     - When the config pointer is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT     - When a configuration value is invalid.\n
    * CY_U3P_ERROR_INVALID_SEQUENCE - When API call sequence is invalid.\n
    * CY_U3P_ERROR_TIMEOUT          - Attempt to clear the SPI block FIFOs timed out.\n
    * CY_U3P_ERROR_MUTEX_FAILURE    - Failed to get a mutex lock on the SPI block.

    **\see
    *\see CyU3PSpiConfig_t
    *\see CyU3PSpiIntrCb_t
    *\see CyU3PSpiInit
 */
extern CyU3PReturnStatus_t
CyU3PSpiSetConfig (
        CyU3PSpiConfig_t *config,       /**< Pointer to the SPI config structure. */
        CyU3PSpiIntrCb_t cb             /**< Callback for receiving SPI events. */
        );

/** \brief Assert or de-assert the SSN Line.

    <B>Description</B>\n
    Asserts or de-asserts the SSN Line for the default slave device. This is possible
   only if the SSN line control is configured for FW controlled mode.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - When the call is successful.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - When SPI has not been configured or if SSN is not firmware controlled.\n
    * CY_U3P_ERROR_MUTEX_FAILURE  - Failed to get a mutex lock on the SPI block.

    **\see
    *\see CyU3PSpiInit
    *\see CyU3PSpiSetConfig
    *\see CyU3PSpiTransmitWords
    *\see CyU3PSpiReceiveWords
    *\see CyU3PSpiTransferWords
    *\see CyU3PSpiSetBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PSpiSetSsnLine (
        CyBool_t isHigh                 /**< CyFalse: Pull down the SSN line, CyTrue: Pull up the SSN line. */
        );

/** \brief Transmits data word by word over the SPI interface.

    <B>Description</B>\n
    This function is used to transmit data in register mode. The function can be called
    only if there is no active DMA transfer on the bus. If CyU3PSpiSetBlockXfer was called,
    then CyU3PSpiDisableBlockXfer needs to be called.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS               - When the data transfer is successful.\n
    * CY_U3P_ERROR_NOT_CONFIGURED  - When SPI block has not been configured or initialized.\n
    * CY_U3P_ERROR_NULL_POINTER    - When the data pointer is NULL.\n
    * CY_U3P_ERROR_ALREADY_STARTED - When a DMA transfer is active.\n
    * CY_U3P_ERROR_BAD_ARGUMENT    - When a configured word length is invalid.\n
    * CY_U3P_ERROR_TIMEOUT         - If the data transfer times out.\n
    * CY_U3P_ERROR_MUTEX_FAILURE   - Failed to get a mutex lock on the SPI block.

    **\see
    *\see CyU3PSpiSetConfig
    *\see CyU3PSpiSetSsnLine
    *\see CyU3PSpiReceiveWords
    *\see CyU3PSpiSetBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PSpiTransmitWords (
        uint8_t *data,                  /**< Source data pointer. This needs to be padded to nearest
                                byte if the word length is not byte aligned. */
        uint32_t byteCount              /**< This needs to be a multiple of the word length aligned to the next
                                             byte. */
        );

/** \brief Receives data word by word over the SPI interface.

    <B>Description</B>\n
    Reads data from the SPI slave in register mode. This API should only be
    called when there is no active DMA transfer.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS               - When the data transfer is successful.\n
    * CY_U3P_ERROR_NOT_CONFIGURED  - When SPI block has not been configured or initialized.\n
    * CY_U3P_ERROR_NULL_POINTER    - When the data pointer is NULL.\n
    * CY_U3P_ERROR_ALREADY_STARTED - When a DMA transfer is active.\n
    * CY_U3P_ERROR_BAD_ARGUMENT    - When a configured word length is invalid.\n
    * CY_U3P_ERROR_TIMEOUT         - If the data transfer times out.\n
    * CY_U3P_ERROR_MUTEX_FAILURE   - Failed to get a mutex lock on the SPI block.

    **\see
    *\see CyU3PSpiSetConfig
    *\see CyU3PSpiSetSsnLine
    *\see CyU3PSpiTransmitWords
    *\see CyU3PSpiSetBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PSpiReceiveWords (
        uint8_t *data,                  /**< Buffer to read the data into. */
        uint32_t byteCount              /**< Amount of data to be read. This should be an integral multiple
                                             of the SPI word length aligned to the next byte. */
        );

/** \brief Perform byte-by-byte bi-directional transfers across the SPI interface.

    <B>Description</B>\n
    This function is used to perform bi-directional SPI data transfers in register mode.
    The function can be called only if there is no active DMA transfer on the bus. If
    CyU3PSpiSetBlockXfer was called, then CyU3PSpiDisableBlockXfer needs to be called before
    this function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS               - When the data transfer is successful.\n
    * CY_U3P_ERROR_NOT_CONFIGURED  - When SPI block has not been configured or initialized.\n
    * CY_U3P_ERROR_NULL_POINTER    - When the data pointer is NULL.\n
    * CY_U3P_ERROR_ALREADY_STARTED - When a DMA transfer is active.\n
    * CY_U3P_ERROR_BAD_ARGUMENT    - When a configured word length is invalid.\n
    * CY_U3P_ERROR_TIMEOUT         - If the data transfer times out.\n
    * CY_U3P_ERROR_MUTEX_FAILURE   - Failed to get a mutex lock on the SPI block.

    **\see
    *\see CyU3PSpiSetConfig
    *\see CyU3PSpiSetSsnLine
    *\see CyU3PSpiTransmitWords
    *\see CyU3PSpiReceiveWords
    *\see CyU3PSpiSetBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PSpiTransferWords (
        uint8_t  *txBuf,        /**< Source buffer containing data to transmit. Can be NULL if txByteCount is 0. */
        uint32_t  txByteCount,  /**< Number of data bytes to transmit. Needs to be a multiple of the word length
                                     aligned to the next byte. */
        uint8_t  *rxBuf,        /**< Destination buffer to receive data into. Can be NULL if rxByteCount is 0. */
        uint32_t  rxByteCount   /**< Number of data bytes to receive. Needs to be a multiple of the word length
                                     aligned to the next byte. */
        );

/** \brief Initiate SPI data transfers in DMA mode.

    <B>Description</B>\n
    This function switches the SPI block to DMA mode, and initiates the desired
    read/write transfers.

    If the txSize parameter is non-zero, then TX is enabled; and if rxSize
    parameter is non-zero, then RX is enabled. If both TX and RX are enabled,
    transfers can only happen while both the SPI producer and SPI consumer
    sockets are ready for data transfer.

    If the receive count is less than the transmit count, the data transmit
    continues after the scheduled reception is complete. However, if the
    transmit count is lesser, data reception is stalled at the end of the
    transmit operation. The SPI transmit transfer needs to disabled before the
    receive can proceed.

    A call to SetBlockXfer has to be followed by a call to DisableBlockXfer, before the
    SPI block can be used in Register mode.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS               - When the SetBlockXfer is successful.\n
    * CY_U3P_ERROR_NOT_CONFIGURED  - When SPI has not been configured or initialized.\n
    * CY_U3P_ERROR_ALREADY_STARTED - When a DMA transfer is active.\n
    * CY_U3P_ERROR_MUTEX_FAILURE   - Failed to get a mutex lock on the SPI block.

    **\see
    *\see CyU3PSpiSetConfig
    *\see CyU3PSpiWaitForBlockXfer
    *\see CyU3PSpiDisableBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PSpiSetBlockXfer (
        uint32_t txSize,                /**< Number of words to be transmitted (not bytes) */
        uint32_t rxSize                 /**< Number of words to be received  (not bytes) */
        );

/** \brief This function disables the SPI DMA TX/RX functionality.

    <B>Description</B>\n
    This function disables DMA transfers through the SPI block in the TX and/or
    RX directions. It is possible to disable only the RX or the TX operation, and
    leave the transfer in the other direcion running.

    Both TX and RX transfers need to be disabled through this API before register
    mode transfers can be used.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - When the DisableBlockXfer is successful.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - When SPI has not been configured or if no DMA transfer has been configured.\n
    * CY_U3P_ERROR_MUTEX_FAILURE  - Failed to get a mutex lock on the SPI block.

    **\see
    *\see CyU3PSpiSetConfig
    *\see CyU3PSpiSetBlockXfer
    *\see CyU3PSpiWaitForBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PSpiDisableBlockXfer (
        CyBool_t rxDisable,             /**< CyTrue: Disable TX block if active; CyFalse: Ignore TX block. */
        CyBool_t txDisable              /**< CyTrue: Disable RX block if active; CyFalse: Ignore RX block. */
        );

/** \brief Wait until the ongoing SPI DMA transfer is finished.

    <B>Description</B>\n
   This function can be used to ensure that a previous SPI transaction has
   completed, in the case where the callback is not being used.

    <B>Note</B>\n
    This function will keep waiting for a transfer complete event until the pre-defined
   timeout period, if it is invoked before any SPI transfers are requested.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS               - When the WaitForBlockXfer is successful.\n
    * CY_U3P_ERROR_NOT_CONFIGURED  - When SPI has not been configured or initialized.\n
    * CY_U3P_ERROR_TIMEOUT         - If the transfer is not completed within the timeout period.\n
    * CY_U3P_ERROR_FAILURE         - When the operation encounters an error.\n
    * CY_U3P_ERROR_MUTEX_FAILURE   - Failed to get a mutex lock on the SPI block.

    **\see
    *\see CyU3PSpiSetConfig
    *\see CyU3PSpiSetBlockXfer
    *\see CyU3PSpiDisableBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PSpiWaitForBlockXfer (
        CyBool_t isRead                 /**< CyFalse: Write operation, CyTrue: Read operation. */
        );

/** \brief This function registers the callback function for notification of SPI events.

    <B>Description</B>\n
    This function registers a callback function that will be called for notification of SPI
    interrupts. This can also be done through the CyU3PSpiInit API.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PSpiIntrCb_t
    *\see CyU3PSpiInit
 */
extern void 
CyU3PRegisterSpiCallBack (
        CyU3PSpiIntrCb_t spiIntrCb      /**< Callback function pointer. */
        );

/** \brief Set the timeout duration for SPI read/write transfer APIs.

    <B>Description</B>\n
    The CyU3PSpiTransmitWords, CyU3PSpiReceiveWords and CyU3PSpiWaitForBlockXfer APIs
    wait for the completion of the requested data transfer. In the case of the byte mode
    transfer APIs, the default timeout is 1 million loops with a delay of about 1 us.
    In the case of the CyU3PSpiWaitForBlockXfer API, the default timeout is 1 million
    loops with a delay of 10 us.

    The default timeout for read and write operations can be changed using this API. The
    loop count to be used during read and write operations can be set to different values.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the timeout duration is updated.
    * CY_U3P_ERROR_NOT_STARTED if the SPI block has not been initialized.

    **\see
    *\see CyU3PSpiTransmitWords
    *\see CyU3PSpiReceiveWords
    *\see CyU3PSpiWaitForBlockXfer
 */
extern CyU3PReturnStatus_t
CyU3PSpiSetTimeout (
        uint32_t readLoopCnt,           /**< Wait loop count used during read operations. */
        uint32_t writeLoopCnt           /**< Wait loop count used during write operations. */
        );

/** \brief Set the delay (in us) which is to be allowed for the SPI TX FIFO to drain.

    <B>Description</B>\n
    The CyU3PSpiTransmitWords disables the SPI transmit path after all of the data has
    been added to the FIFO and the TX_DONE interrupt status has been detected. This function
    can be used to add an additional delay between the observation of the TX_DONE interrupt
    and disabling the TX path. The default delay duration is 0, and the maximum value that
    may need to be used is 80 times the SPI interface clock period.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the delay duration is updated.
    * CY_U3P_ERROR_NOT_STARTED if the SPI block has not been initialized.

    **\see
    *\see CyU3PSpiTransmitWords
 */
extern CyU3PReturnStatus_t
CyU3PSpiSetTxFlushWaitPeriod (
        uint16_t delay_us               /**< TX wait duration in us units. */
        );

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3SPI_H_ */

/*[]*/
