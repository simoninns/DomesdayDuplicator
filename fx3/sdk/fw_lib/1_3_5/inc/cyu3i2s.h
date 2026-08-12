/*
 ## Cypress FX3 Core Library Header (cyu3i2s.h)
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

#ifndef _INCLUDED_CYU3I2S_H_
#define _INCLUDED_CYU3I2S_H_

#include "cyu3types.h"
#include "cyu3lpp.h"
#include "cyu3externcstart.h"

/** \file cyu3i2s.h
    \brief The I2S (Inter-IC Sound) interface is a serial interface defined for
    communication of stereophonic audio data between devices. The FX3 device
    includes a I2S master interface which can be connected to an I2S peripheral.
    The I2S driver module provides functions to configure the I2S interface and
    to send mono or stereo audio output on the I2S link.
 */

/**************************************************************************
 ******************************* Data Types *******************************
 **************************************************************************/

/** \def CY_U3P_I2S_DEFAULT_LOCK_TIMEOUT
    \brief Delay duration to wait to get a lock on the I2S block.
 */
#define CY_U3P_I2S_DEFAULT_LOCK_TIMEOUT    (CYU3P_WAIT_FOREVER)

/** \brief List of I2S related event types.

    <B>Description</B>\n
    This enumeration lists the various I2S related event codes that are notified
    to the user application through an event callback.

    **\see
    *\see CyU3PI2sIntrCb_t
    *\see CyU3PI2sError_t
 */
typedef enum CyU3PI2sEvt_t
{
    CY_U3P_I2S_EVENT_TXL_DONE = 0,              /**< Transmission of left channel data is complete. */
    CY_U3P_I2S_EVENT_TXR_DONE,                  /**< Transmission of right channel data is complete. */
    CY_U3P_I2S_EVENT_PAUSED,                    /**< Pause has taken effect. */
    CY_U3P_I2S_EVENT_ERROR                      /**< An I2S error has been detected. */
} CyU3PI2sEvt_t;

/** \brief List of I2S specific error/status codes.

    <B>Description</B>\n
    This type lists the various I2S specific error/status codes that are sent to the
    event callback as event data, when the event type is CY_U3P_I2S_ERROR_EVT.

    **\see
    *\see CyU3PI2sEvt_t
    *\see CyU3PI2sIntrCb_t
  */
typedef enum CyU3PI2sError_t
{
    CY_U3P_I2S_ERROR_LTX_UNDERFLOW = 11,        /**< A left channel underflow occurred. */
    CY_U3P_I2S_ERROR_RTX_UNDERFLOW,             /**< A right channel underflow occurred. */
    CY_U3P_I2S_ERROR_LTX_OVERFLOW,              /**< A left channel overflow occurred. */
    CY_U3P_I2S_ERROR_RTX_OVERFLOW               /**< A right channel overflow occurred. */
} CyU3PI2sError_t;

/** \brief List of supported bit widths for the I2S interface.

    <B>Description</B>\n
    This type lists the supported bit-widths on the I2S interface.

    **\see
    *\see CyU3PI2sConfig_t
 */
typedef enum CyU3PI2sSampleWidth_t
{
    CY_U3P_I2S_WIDTH_8_BIT = 0,                 /**< 8  bit */
    CY_U3P_I2S_WIDTH_16_BIT,                    /**< 16 bit */
    CY_U3P_I2S_WIDTH_18_BIT,                    /**< 18 bit */
    CY_U3P_I2S_WIDTH_24_BIT,                    /**< 24 bit */
    CY_U3P_I2S_WIDTH_32_BIT,                    /**< 32 bit */
    CY_U3P_I2S_NUM_BIT_WIDTH                    /**< Number of options. */

} CyU3PI2sSampleWidth_t;

/** \brief List of supported sample rates.

    <B>Description</B>\n
    This type lists the supported sample rates for audio playback through the
    I2S interface.

    **\see
    *\see CyU3PI2sConfig_t
 */
typedef enum CyU3PI2sSampleRate_t
{
    CY_U3P_I2S_SAMPLE_RATE_8KHz = 8000,         /**< 8    KHz */
    CY_U3P_I2S_SAMPLE_RATE_16KHz = 16000,       /**< 16   KHz */
    CY_U3P_I2S_SAMPLE_RATE_32KHz = 32000,       /**< 32   KHz */
    CY_U3P_I2S_SAMPLE_RATE_44_1KHz = 44100,     /**< 44.1 KHz */
    CY_U3P_I2S_SAMPLE_RATE_48KHz = 48000,       /**< 48   KHz */
    CY_U3P_I2S_SAMPLE_RATE_96KHz = 96000,       /**< 96   KHz */
    CY_U3P_I2S_SAMPLE_RATE_192KHz = 192000      /**< 192  KHz */
} CyU3PI2sSampleRate_t;

/** \brief List of the supported padding modes.

    <B>Description</B>\n
    This types lists the padding modes supported on the I2S interface.

    **\see
    *\see CyU3PI2sConfig_t
 */
typedef enum CyU3PI2sPadMode_t
{
    CY_U3P_I2S_PAD_MODE_NORMAL = 0,             /**< I2S normal operation. */
    CY_U3P_I2S_PAD_MODE_LEFT_JUSTIFIED,         /**< Left justified.       */
    CY_U3P_I2S_PAD_MODE_RIGHT_JUSTIFIED,        /**< Right justified.      */
    CY_U3P_I2S_PAD_MODE_RESERVED,               /**< Reserved mode.        */
    CY_U3P_I2S_PAD_MODE_CONTINUOUS,             /**< Without padding.      */
    CY_U2P_I2S_NUM_PAD_MODES                    /**< Number of pad modes.  */
} CyU3PI2sPadMode_t;

/** \brief I2S interface configuration parameters.

    <B>Description</B>\n
    This structure encapsulates all of the configurable parameters that can be
    selected for the I2S interface. The CyU3PI2sSetConfig() function accepts a
    pointer to this structure, and updates all of the interface parameters.

    **\see
    *\see CyU3PI2sSetConfig
 */
typedef struct CyU3PI2sConfig_t
{
    CyBool_t              isMono;               /**< CyTrue: Mono, CyFalse: Stereo  */
    CyBool_t              isLsbFirst;           /**< CyTrue: LSB First, CyFalse: MSB First */
    CyBool_t              isDma;                /**< CyTrue: DMA mode, CyFalse: Register mode */
    CyU3PI2sPadMode_t     padMode;              /**< Type of padding to be used */
    CyU3PI2sSampleRate_t  sampleRate;           /**< Sample rate for this audio stream. */
    CyU3PI2sSampleWidth_t sampleWidth;          /**< Bit width for samples in this audio stream. */
} CyU3PI2sConfig_t;

/** \brief Prototype of I2S event callback function.

    <B>Description</B>\n
    This function type defines a callback to be called when an I2S interrupt has
    been received. A function of this type can be registered with the I2S driver
    as a callback function and will be called whenever an event of interest occurs.

    **\see
    *\see CyU3PI2sEvt_t
    *\see CyU3PI2sError_t
    *\see CyU3PRegisterI2sCallBack
 */
typedef void (*CyU3PI2sIntrCb_t)(
        CyU3PI2sEvt_t evt,                      /**< Type of the event that occured */
        CyU3PI2sError_t error                   /**< Error/status code associated with a CY_U3P_I2S_ERROR_EVT event. */
        );

/**************************************************************************
 *************************** Function Prototypes **************************
 **************************************************************************/

/** \brief Select an external input as the MCLK for the I2S interface.

    <B>Description</B>\n
    During normal operation, the I2S block on the FX3 internally generates the I2S
    bit clock (SCK) required using its master SYSCLK and a configurable frequency
    divider block. However, some applications may require the I2S interface on FX3
    to work on the basis of an externally provided MCLK input.

    This API is used to enable the I2S block to use an external MCLK input instead
    of generating the clock internally. It is not allowed to change the direction of
    the clock while the I2S block on FX3 is running. Therefore, this API has to be
    called before the CyU3PI2sInit() function is called.

    If the external clock input is enabled, a clock that runs 4 times as fast as the
    bit clock required should be connected to the I2S_SCK pin of the FX3 device.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if this API is called prior to calling CyU3PI2sInit().\n
    * CY_U3P_ERROR_ALREADY_STARTED if the API is called after CyU3PI2sInit().

    **\see
    *\see CyU3PI2sInit
 */
extern CyU3PReturnStatus_t
CyU3PI2sEnableExternalMclk (
        void);

/** \brief Starts the I2S interface block on the FX3.

    <B>Description</B>\n
    This function powers up the I2S interface block on the FX3 device, and is
    expected to be the first I2S API function that is called by the application.
 
    This function sets up the clock to a default value of CY_U3P_I2S_SAMPLE_RATE_8KHz.
 
    <B>Return value</B>\n
    * CY_U3P_SUCCESS               - When the init is successful.\n
    * CY_U3P_ERROR_ALREADY_STARTED - When the I2S block has already been initialized.\n
    * CY_U3P_ERROR_NOT_CONFIGURED  - When the I2S block has not been enabled in the IOMatrix.

    **\see
    *\see CyU3PI2sDeinit
    *\see CyU3PI2sSetConfig
    *\see CyU3PI2sTransmitBytes
    *\see CyU3PI2sSetMute
    *\see CyU3PI2sSetPause
 */
extern CyU3PReturnStatus_t
CyU3PI2sInit (
        void);

/** \brief Stops the I2S interface block on the FX3.

    <B>Description</B>\n
    This function disables and powers off the I2S interface. This function can
    be used to shut off the interface to save power when it is not in use.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS           - When the de-init is successful.\n
    * CY_U3P_ERROR_NOT_STARTED - When the module has not been previously initialized.

    **\see
    *\see CyU3PI2sInit
 */
extern CyU3PReturnStatus_t
CyU3PI2sDeInit (
        void);

/** \brief Sets the I2S interface parameters.

    <B>Description</B>\n
    This function is used to configure the I2S master interface based on the
    desired settings. This function should be called repeatedly every time the
    output stream parameters change.
 
    The callback parameter is used to specify an event callback function that
    will be called by the driver when an I2S interrupt occurs.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS             - When the SetConfig is successful.\n
    * CY_U3P_ERROR_NOT_STARTED   - When the I2S has not been initialized.\n
    * CY_U3P_ERROR_NULL_POINTER  - When the config pointer is NULL.\n
    * CY_U3P_ERROR_BAD_ARGUMENT  - When the configuration parameters are incorrect.\n
    * CY_U3P_ERROR_TIMEOUT       - If there is a timeout while trying to clear the left and right channel pipes.\n
    * CY_U3P_ERROR_MUTEX_FAILURE - Failed to get a mutex lock on the I2S block.

    **\see
    *\see CyU3PI2sConfig_t
    *\see CyU3PI2sIntrCb_t
    *\see CyU3PI2sInit
    *\see CyU3PI2sTransmitBytes
    *\see CyU3PI2sSetMute
    *\see CyU3PI2sSetPause
 */
extern CyU3PReturnStatus_t
CyU3PI2sSetConfig (
        CyU3PI2sConfig_t *config,       /**< I2S configuration settings. */
        CyU3PI2sIntrCb_t cb             /**< Callback for getting the events. */
        );

/** \brief Transmits data byte by byte over the I2S interface.

    <B>Description</B>\n
    This function sends the data over the I2S interface in register mode. This
    is allowed only when the I2S interface block is configured for register mode
    transfer. The data involved in this transfer is always left justified.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - When the data transfer is successful.\n
    * CY_U3P_ERROR_NULL_POINTER   - When the data pointers are NULL.\n
    * CY_U3P_ERROR_TIMEOUT        - When a timeout occurs.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - If the I2S has not been configured for register mode transfers.\n
    * CY_U3P_ERROR_MUTEX_FAILURE  - Failed to get a mutex lock on the I2S block.

    **\see
    *\see CyU3PI2sSetConfig
    *\see CyU3PI2sSetMute
    *\see CyU3PI2sSetPause
 */
CyU3PReturnStatus_t
CyU3PI2sTransmitBytes (
        uint8_t *lData,                 /**< Buffer containing data to be sent on the left channel. */
        uint8_t *rData,                 /**< Buffer containing data to be sent on the right channel. */
        uint8_t lByteCount,             /**< Number of bytes to be transferred on the left channel. */
        uint8_t rByteCount              /**< Number of bytes to be transferred on the right channel. */
        );

/** \brief Mute or unmute the I2S output stream.

    <B>Description</B>\n
    This function sets the I2C master to mute or unmute the output stream. When
    configured for mute, the I2S master drives zero samples instead of actual
    data output.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - When the mute control is successful.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - When the I2S interface has not been configured.\n
    * CY_U3P_ERROR_MUTEX_FAILURE  - Failed to get a mutex lock on the I2S block.

    **\see
    *\see CyU3PI2sInit
    *\see CyU3PI2sSetConfig
 */
extern CyU3PReturnStatus_t
CyU3PI2sSetMute (
        CyBool_t isMute                 /**< CyTrue: Mute the I2S, CyFalse: Un-mute the I2S. */
        );

/** \brief Pause or resume I2S data transfers.

    <B>Description</B>\n
    This function is used to pause or resume the data transfer on the I2S interface.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS              - When the pause control is successful.\n
    * CY_U3P_ERROR_NOT_CONFIGURED - When the I2S interface has not been configured.\n
    * CY_U3P_ERROR_MUTEX_FAILURE  - Failed to get a mutex lock on the I2S block.

    **\see
    *\see CyU3PI2sInit
    *\see CyU3PI2sSetConfig
 */
extern CyU3PReturnStatus_t
CyU3PI2sSetPause (
        CyBool_t isPause                /**< CyTrue: pause the I2S, CyFalse: resume the I2S. */
        );

/** \brief This function registers a callback function for notification of I2S interrupts.

    <B>Description</B>\n
    This function registers a callback function that will be called for notification of I2S
    interrupts. This can also be done through the CyU3PI2sInit API call.

    <B>Return value</B>\n
    * None

    **\see
    *\see CyU3PI2sIntrCb_t
    *\see CyU3PI2sInit
 */
extern void
CyU3PRegisterI2sCallBack (
        CyU3PI2sIntrCb_t i2sIntrCb      /**< Callback function pointer. */
        );

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3I2S_H_ */

/*[]*/
