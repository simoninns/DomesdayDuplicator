/*
 ## Cypress FX3 Core Library Header (cyu3mipicsi.h)
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

#ifndef _INCLUDED_CYU3MIPICSI_H_
#define _INCLUDED_CYU3MIPICSI_H_

#include <cyu3os.h>
#include <cyu3types.h>
#include <cyu3sib.h>
#include <cyu3gpif.h>

#include "cyu3externcstart.h"

/** \file cyu3mipicsi.h
    \brief This file contains the MIPI-CSI interface management data
    structures, functions and macros for CX3 devices. 
*/

/************************************************************************************/
/*********************************** Macros *************************************/
/************************************************************************************/

/** \section CX3 Fixed Function GPIF States
    \brief The following macros define the various states supported by the fixed function GPIF
    on the CX3 parts
 */

#define CX3_START_SCK0 0                        /**< Start of data capture into GPIF socket 0. */
#define CX3_IDLE_SCK0 1                         /**< Delay state #1. */
#define CX3_WAIT_FOR_FRAME_START_SCK0 2         /**< Wait for frame start. */
#define CX3_PUSH_DATA_SCK0 3                    /**< Push data into socket 0. */
#define CX3_PUSH_DATA_SCK1 4                    /**< Push data into socket 1. */
#define CX3_WAIT_TO_FILL_SCK0 5                 /**< Line blanking while filling socket 0. */
#define CX3_WAIT_TO_FILL_SCK1 7                 /**< Line blanking while filling socket 1. */
#define CX3_WAIT_FULL_SCK0_NEXT_SCK1 6          /**< Line blanking and socket 0 full. */
#define CX3_WAIT_FULL_SCK1_NEXT_SCK0 8          /**< Line blanking and socket 1 full. */
#define CX3_PARTIAL_BUFFER_IN_SCK0 9            /**< Frame end with partial buffer in socket 0. */
#define CX3_PARTIAL_BUFFER_IN_SCK1 10           /**< Frame end with partial buffer in socket 1. */
#define CX3_FULL_BUFFER_IN_SCK0 11              /**< Frame end with full buffer in socket 0. */
#define CX3_FULL_BUFFER_IN_SCK1 12              /**< Frame end with full buffer in socket 1. */
#define CX3_START_SCK1 15                       /**< Start of data capture into GPIF socket 1. */
#define CX3_IDLE_SCK1 16                        /**< Delay state #2. */
#define CX3_WAIT_FOR_FRAME_START_SCK1 17        /**< Wait for frame start. */
#define FW_WAIT_SCK0 13                         /**< Wait for firmware trigger to move to socket 1. */
#define FW_WAIT_SCK1 14                         /**< Wait for firmware trigger to move to socket 0. */
 
#define ALPHA_CX3_START_SCK0                    0x0
/**< Initial value of early outputs from the CX3 GPIF state machine. */

#define ALPHA_CX3_START_SCK1                    0x0
/**< Initial value of early outputs from the CX3 GPIF state machine. */

#define CX3_NUMBER_OF_STATES        18                              /**< Number of states in the CX3 GPIF state machine */

#define CX3_START                   CX3_START_SCK0                  /**< Mapping for legacy value from SDK 1.3 */
#define CX3_IDLE                    CX3_IDLE_SCK0                   /**< Mapping for legacy value from SDK 1.3 */
#define CX3_WAIT_FOR_FRAME_START    CX3_WAIT_FOR_FRAME_START_SCK0   /**< Mapping for legacy value from SDK 1.3 */
#define CX3_PUSH_DATA_TO_SCK0       CX3_PUSH_DATA_SCK0              /**< Mapping for legacy value from SDK 1.3 */
#define CX3_PUSH_DATA_TO_SCK1       CX3_PUSH_DATA_SCK1              /**< Mapping for legacy value from SDK 1.3 */
#define CX3_ALPHA_START             ALPHA_CX3_START_SCK0            /**< Mapping for legacy value from SDK 1.3 */

/************************************************************************************/
/*********************************** Structures/Enumerations ************************/
/************************************************************************************/

/** \brief I2C frequency for MIPI-CSI interface communication

    <B>Description</B>\n
    This enumeration defines the I2C frequency for communication with the Image Sensor and the 
    MIPI-CSI interface. The CX3 part supports communication at 100KHz and 400 KHz.

    **\see
    *\see CyU3PMipicsiInitializeI2c
*/
typedef enum CyU3PMipicsiI2cFreq_t
{
    CY_U3P_MIPICSI_I2C_100KHZ,          /**< 100 KHz operation */
    CY_U3P_MIPICSI_I2C_400KHZ           /**< 400 KHz operation */
}CyU3PMipicsiI2cFreq_t;


/** \brief MIPI Control lines for Image Sensor

    <B>Description</B>\n
    This enumeration defines the IO lines for MIPI XReset and XShutdown signals. 
    The user drives these lines high or low using the CyU3PMipicsiSetSensorControl function

    **\see
    *\see CyU3PMipicsiSetSensorControl
*/
typedef enum CyU3PMipicsiSensorIo_t
{
    CY_U3P_CSI_IO_XRES = 2,         /**< Image Sensor Reset IO (XRES)         */
    CY_U3P_CSI_IO_XSHUTDOWN = 4     /**< Image Sensor Shutdown IO (XSHUTDOWN) */
}CyU3PMipicsiSensorIo_t;


/** \brief MIPI-CSI interface Reset Type.

    <B>Description</B>\n
    This enumerated type lists the reset types supported for the MIPI-CSI interface block 
    on the CX3. The MIPI-CSI interface provides two reset modes - Hard reset, which power cycles 
    the entire block, and Soft reset, which does not reset the I2C communication channel used 
    by the block.
    
    **\see
    *\see CyU3PMipicsiReset
*/
typedef enum CyU3PMipicsiReset_t
{
    CY_U3P_CSI_SOFT_RST= 0,     /**< Perform a soft reset on the MIPI-CSI interface block*/
    CY_U3P_CSI_HARD_RST         /**< Perform a hard reset on the MIPI-CSI interface block*/
} CyU3PMipicsiReset_t;


/** \brief Structure defining MIPI-CSI block Phy errors.

    <B>Description</B>\n
    The following structure is used to fetch count of MIPI-CSI Phy level errors from the 
    MIPI-CSI block on the  CX3.

    **\see
    *\see CyU3PMipicsiGetErrors
*/
typedef struct CyU3PMipicsiErrorCounts_t
{
    uint8_t frmErrCnt;          /**< Framing Error Count*/
    uint8_t crcErrCnt;          /**< CRC Error Count*/
    uint8_t mdlErrCnt;          /**< Multi-Data Lane Sync Byte Error Count*/
    uint8_t ctlErrCnt;          /**< Control Error (Incorrect Line State Sequence) Count*/
    uint8_t eidErrCnt;          /**< Unsupported Packet ID Error Count */
    uint8_t recrErrCnt;         /**< Recoverable Packet Header Error Count*/
    uint8_t unrcErrCnt;         /**< Unrecoverable Packet Header Error Count*/
    uint8_t recSyncErrCnt;      /**< Recoverable Sync Byte Error Count*/
    uint8_t unrSyncErrCnt;      /**< Unrecoverable Sync Byte Error Count*/
} CyU3PMipicsiErrorCounts_t;



/** \brief MIPI-CSI Data Formats

    <B>Description</B>\n
    This enumerated type lists the MIPI CSI data formats supported by the MIPI-CSI block
    on the CX3. The MIPI-CSI block on the CX3 supports the listed data formats in 8, 16 and
    24 bit modes. Some data formats (RAW8, RGB888) support only a fixed data width, 
    while some formats support more than one data widths with different padding mechanisms 
    and byte orders on the received data.
    
    <B>Note</B>\n
    The user needs to set the Fixed Function GPIF interface on the CX3 to an 
    appropriate width based on the Data Format being selected. Selection of an incorrect GPIF bus 
    width can lead to loss of data or introduction of garbage data into the data stream.

    **\see
    *\see CyU3PMipicsiCfg_t
    *\see CyU3PMipicsiSetIntfParams
    *\see CyU3PMipicsiQueryIntfParams
*/
typedef enum CyU3PMipicsiDataFormat_t
{
    CY_U3P_CSI_DF_RAW8 = 0x00,          /**< RAW8 Data Type. CSI-2 Data Type 0x2A \n
                                             8 Bit Output = RAW[7:0]      */

    CY_U3P_CSI_DF_RAW10 = 0x01,         /**< RAW10 Data Type. CSI-2 Data Type 0x2B  \n
                                             16 Bit Output = 6'b0,RAW[9:0]        */
   
    CY_U3P_CSI_DF_RAW12 = 0x02,         /**< RAW12 Data Type. CSI-2 Data Type 0x2C \n
                                             16 Bit Output = 4'b0,RAW[11:0]       */

    CY_U3P_CSI_DF_RAW14 = 0x08,         /**< RAW14 Data Type. CSI-2 Data Type 0x2D \n
                                             16 Bit Output = 2'b0,RAW[13:0]       */

    CY_U3P_CSI_DF_RGB888 =0x03,         /**< RGB888 Data type. CSI-2 Data Type 0x24. \n
                                             24 Bit Output = R[7:0],G[7:0],B[7:0]*/

    CY_U3P_CSI_DF_RGB666_0 = 0x04,      /**< RGB666 Data type. CSI-2 Data Type 0x23. \n
                                             24 Bit Output = 2'b0,R[5:0],2'b0,G[5:0], 2'b0,B[5:0]*/

    CY_U3P_CSI_DF_RGB666_1 = 0x14,      /**< RGB666 Data type. CSI-2 Data Type 0x23. \n
                                             24 Bit Output = 6'b0,R[5:0],G[5:0], B[5:0]*/

    CY_U3P_CSI_DF_RGB565_0 = 0x05,      /**< RGB565 Data type. CSI-2 Data Type 0x22. \n
                                             24 Bit Output = 2'b0,R[4:0],3'b0,G[5:0], 2'b0,B[4:0],1'b0*/

    CY_U3P_CSI_DF_RGB565_1 = 0x15,      /**< RGB565 Data type. CSI-2 Data Type 0x22. \n
                                             24 Bit Output = 3'b0,R[4:0],2'b0,G[5:0], 3'b0,B[4:0]*/

    CY_U3P_CSI_DF_RGB565_2 = 0x25,      /**< RGB565 Data type. CSI-2 Data Type 0x22. \n
                                             24 Bit Output = 8'b0,R[4:0],G[5:0], B[4:0] \n
                                             16 Bit Output = R[4:0],G[5:0], B[4:0]*/

    CY_U3P_CSI_DF_YUV422_8_0 = 0x06,    /**< YUV422 8-Bit Data type. CSI-2 Type 0x1E. \n
                                             24 Bit Output = 16'b0,P[7:0] \n
                                             16 Bit Output = 8'b0, P[7:0] \n
                                             8 Bit Output = P[7:0]        \n
                                             Data Order: U1,Y1,V1,Y2,U3,Y3,V3,Y4....  */

    CY_U3P_CSI_DF_YUV422_8_1 = 0x16,    /**< YUV422 8-Bit Data type. CSI-2 Type 0x1E. \n
                                             24 Bit Output = 8'b0,P[15:0] \n
                                             16 Bit Output = P[15:0] \n
                                             Data Order: {U1,Y1},{V1,Y2},{U3,Y3},{V3,Y4}....  */

    CY_U3P_CSI_DF_YUV422_8_2 = 0x26,    /**< YUV422 8-Bit Data type. CSI-2 Type 0x1E. \n
                                             24 Bit Output = 8'b0,P[15:0] \n
                                             16 Bit Output = P[15:0] \n
                                             Data Order: {Y1,U1},{Y2,V1},{Y3,U3},{Y4,V3}....  */

    CY_U3P_CSI_DF_YUV422_10 = 0x09      /**< YUV422 10-Bit Data type. CSI-2 Type 0x1F. \n
                                             24 Bit Output = 14'b0,P[9:0] \n
                                             16 Bit Output = 6'b0,P[9:0] \n
                                             Data Order: U1,Y1,V1,Y2,U3,Y3,V3,Y4....  */
} CyU3PMipicsiDataFormat_t;


/** \def CY_U3P_CSI_DF_UVC_YUV2
    \brief Data Format used for the UVC YUV422 (UVC YUV2) format */
#define CY_U3P_CSI_DF_UVC_YUV2          (CY_U3P_CSI_DF_YUV422_8_1)


/** \brief Clock Divider Values 
        
    <B>Description</B>\n
    This enumerated type lists Clock divider values permitted for the various clocks 
    on the MIPI-CSI block of the CX3 device.
    The clocks on MIPI-CSI block are derived from a primary PLL clock which 
    is generated using 19.2 MHz System Reference Clock.

    **\see
    *\see CyU3PMipicsiCfg_t
    *\see CyU3PMipicsiSetIntfParams
    *\see CyU3PMipicsiQueryIntfParams
*/
typedef enum CyU3PMipicsiPllClkDiv_t
{
    CY_U3P_CSI_PLL_CLK_DIV_8 = 0,       /**< Clk = PLL_ClK/8 */
    CY_U3P_CSI_PLL_CLK_DIV_4,           /**< Clk = PLL_ClK/8 */
    CY_U3P_CSI_PLL_CLK_DIV_2,           /**< Clk = PLL_ClK/8 */
    CY_U3P_CSI_PLL_CLK_DIV_INVALID      /**< Invalid Value   */

} CyU3PMipicsiPllClkDiv_t;


/** \brief Frequency range selection for the MIPI-CSI PLL Clock

    <B>Description</B>\n
    This enumeration is used to define the frequency rage in which the PLL clock on the 
    MIPI-CSI block is operating.

    **\see
    *\see CyU3PMipicsiCfg_t
    *\see CyU3PMipicsiSetIntfParams
    *\see CyU3PMipicsiQueryIntfParams

*/
typedef enum CyU3PMipicsiPllClkFrs_t
{
    CY_U3P_CSI_PLL_FRS_500_1000M = 0,   /**<  PLL Clock output range 500MHz-1GHz   */
    CY_U3P_CSI_PLL_FRS_250_500M,        /**<  PLL Clock output range 250-500MHz    */
    CY_U3P_CSI_PLL_FRS_125_250M,        /**<  PLL Clock output range 125-250MHz    */
    CY_U3P_CSI_PLL_FRS_63_125M          /**<  PLL Clock output range 62.5-125MHz   */

} CyU3PMipicsiPllClkFrs_t;


/** \brief Bus Widths supported by the fixed function GPIF interface
    
    <B>Description</B>\n
    This enumeration defines the bus widths supported by the fixed function GPIF interface 
    on the CX3 parts. CX3 supports bus widths of 8-Bits, 16-Bits and 24-Bits.

    <B>Note</B>\n
    The DMA buffer size being used to transfer data from the GPIF interface must be aligned 
    to the data width of the interface used. The data buffer size in bytes must be a multiple 
    of 16 for 16-Bit interfaces and a multiple of 24 for 24-Bit buffers.

    **\see
    *\see CyU3PMipicsiGpifLoad
*/
typedef enum CyU3PMipicsiBusWidth_t
{
    CY_U3P_MIPICSI_BUS_8 = 0,           /**< Use an 8-Bit data bus */ 
    CY_U3P_MIPICSI_BUS_16,              /**< Use a 16-Bit data bus */ 
    CY_U3P_MIPICSI_BUS_24               /**< Use a 24-Bit data bus */

} CyU3PMipicsiBusWidth_t;


/** \brief  Structure defining the MIPI-CSI block interface configuration 

    <B>Description</B>\n
    This structure encapsulates all the configurable parameters that can be selected for the 
    MIPI-CSI interface. The CyU3PMipicsiSetIntfParams() function accepts a pointer to this 
    structure and updates the interface parameters.
 
    <B>Note</B>\n
    This structure has changed from the 1.3 SDK release (CX3 BETA release). If you are using
    CX3 code from the 1.3 SDK release, please update your code to use the current version
    of this structure. The changes between the 1.3 release and 1.3.1 release are as follows:
    1) The order of structure members has changed.
    2) A new member fifoDelay has been added.
    3) The names for some members has changed (ppiClkDiv is now csiRxClkDiv, and sClkDiv is 
       now parClkDiv).




    **\see
    *\see CyU3PMipicsiSetIntfParams
    *\see CyU3PMipicsiQueryIntfParams
    *\see CyU3PMipicsiPllClkFrs_t
    *\see CyU3PMipicsiPllClkDiv_t
*/
typedef struct CyU3PMipicsiCfg_t
{
    CyU3PMipicsiDataFormat_t dataFormat;    /**< Data Format expected at the GPIF. 
                                                 The image sensor will also need to be separately set 
                                                 to transmit in this format. */
    
    uint8_t numDataLanes;                   /**< Number of MIPI-CSI data lanes to use. Valid values 
                                                 are 1,2,3 and 4. The number of lanes which can be used 
                                                 varies depending on the part being used and the Image Sensor 
                                                 being used. Please refer to the CX3 part datasheet and the 
                                                 Image Sensor datasheet for more details on the number of 
                                                 lanes available.*/
   

    uint8_t pllPrd;                         /**< Input Divider used for generating MIPI-CSI PLL_CLOCK from 
                                                 the input REFCLK. Valid setting range: 0x0-0xF*/
    
    uint16_t pllFbd;                        /**< Feedback Divider used for generating MIPI-CSI PLL_CLOCK from 
                                                 the input REFCLK. Valid setting range:0x000-0x1FF */
    
    CyU3PMipicsiPllClkFrs_t pllFrs;         /**< Frequency Range Setting for the MIPI CSI PLL_CLOCK*/
    
    
    CyU3PMipicsiPllClkDiv_t csiRxClkDiv;    /**< Clock divider for generating clock used for detecting 
                                                 CSI Link LP<->HS transitions*/

    CyU3PMipicsiPllClkDiv_t parClkDiv;      /**< Clock divider used for generating the PCLK used for clocking the 
                                                 fixed function GPIF interface */

    uint16_t mClkCtl;                       /**< Note: This field is no longer actively set by the CX3 configuration 
                                                 tool as we do not recommend using the MCLK as a clock source for
                                                 image sensors due to issues with high jitter.
                                                 
                                                 Settings used to generate MCLK output clock. 
                                                 The output clock is available on the CLKM_CX3 line 
                                                 mClkOutput = 
                                                 (PLL_Clock/mClkRefDiv)/
                                                 [(CY_U3P_GET_MSB(mClkCtl) + 1) + (CY_U3P_GET_LSB(mclkCtl) + 1)]
                                                 */

    CyU3PMipicsiPllClkDiv_t mClkRefDiv;     /**< Clock divider used for generating the MCLK */
    
    
    uint16_t hResolution;                   /**< Horizontal Resolution defined in number of pixels per line of 
                                                 the video frame.*/
    
    uint16_t fifoDelay;                     /**< Threshold at which the output from the parallel output buffer on the 
                                                 MIPI-CSI interface will be initiated. Range 0x000 - 0x1FF */
    

} CyU3PMipicsiCfg_t;



/*****************************************************************************/
/************************ MIPI-CSI2 function prototypes. **************************/
/*****************************************************************************/

/** \brief Initialize MIPI-CSI block.

    <B>Description</B>\n
    This function initializes MIPI-CSI interface block on the CX3 device and is expected to be called prior 
    to any other calls to the MIPI-CSI block. The GPIO Block, the PIB block and the I2C blocks should have 
    been initialized prior to calling this function. Helper functions CyU3PMipicsiInitializeGPIO(), 
    CyU3PMipicsiInitializePIB() and CyU3PMipicsiInitializeI2c() have been provided to initialize these blocks 
    to the default values to be used for the MIPI-CSI block. Please see documentation on each of those
    functions for more information on the specific settings.

    The CX3 part uses the I2C block to communicate with both the MIPI-CSI block and any connected Image 
    Sensor. The 7 bit I2C slave address 8b'0000111X (Read 0x0F, Write 0x0E) is used internally by the CX3 
    part to configure and control the MIPI-CSI interface block and should not be used by any external 
    I2C slaves connected to the CX3 on the I2C bus.
    
    <B>Note</B>\n
    This function sets the default state of the MIPI XReset and XShutdown signals as Drive 0.
    If either of these signals, needs to be in Drive 1 state for sensor operation, it needs to be 
    explictily set to Drive 1 state using CyU3PMipicsiSetSensorControl() after calling CyU3PMipicsiInit().
    This call leaves the interface in a low-power mode and CyU3PMipicsiWakeup() should be called to enable 
    the clocks on the MIPI-CSI interface.

  
    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT - If an I2C read/write timeout occurs during the operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - If the part on which this function is called is not a CX3 device.\n
    * CY_U3P_ERROR_UNKNOWN - If a general or unknown error occurred during the operation.\n

    **\see
    *\see CyU3PMipicsiInitializeI2c
    *\see CyU3PMipicsiInitializeGPIO
    *\see CyU3PMipicsiInitializePIB
    *\see CyU3PCx3DeviceReset
    *\see CyU3PMipicsiSetSensorControl
*/
extern CyU3PReturnStatus_t
CyU3PMipicsiInit (
        void);

/** \brief De-Initialize MIPI-CSI block.

    <B>Description</B>\n
    This function de-initializes MIPI-CSI interface block on the CX3 device and is expected to be called prior 
    to calling CyU3PSysEnterStandbyMode. This function should be called before the I2C block or GPIO blocks 
    are de-initialized. MIPI XReset and XShutdown signals shall not be driven by the CX3 after this call.
    
  
    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT - If an I2C read/write timeout occurs during the operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - If the part on which this function is called is not a CX3 device.\n
    * CY_U3P_ERROR_UNKNOWN - If a general or unknown error occurred during the operation.\n

    **\see
    *\see CyU3PSysEnterStandbyMode
*/
extern CyU3PReturnStatus_t
CyU3PMipicsiDeInit (
        void);



/** \brief Configure MIPI-CSI interface

    <B>Description</B>\n
    This function is used to configure the MIPI-CSI interface parameters over I2C. The function takes in an
    object of the type CyU3PMipicsiCfg_t and configures the MIPI-CSI interface. The function powers off the 
    interface clocks before changing/setting the configuration. Parameter wakeOnConfigure is used to turn the
    clocks ON immediately after the configuration has been completed or to leave the clocks powered down.
    If the clocks are left powered down, CyU3PMipicsiWakeup() should be called to start the clocks.
    The state of the interface (Active or Powered down) can be determined using the CyU3PMipicsiCheckBlockActive()
    call.

    <B>Note</B>\n
    This function can be called when the interface has been put into low power mode using the CyU3PMipicsiSleep() 
    function.
    
    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT - If an I2C read/write timeout occurs during the operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - If the part on which this function is called is not a CX3 device.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - If an invalid argument is passed to this function. \n
    * CY_U3P_ERROR_UNKNOWN - If a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_NOT_STARTED - If the interface has not been initialized. \n
    * CY_U3P_ERROR_NULL_POINTER - If the csiCfg object is uninitialized. \n

    **\see
    *\see CyU3PMipicsiCfg_t
    *\see CyU3PMipicsiQueryIntfParams
    *\see CyU3PMipicsiCheckBlockActive
    *\see CyU3PMipicsiWakeup
*/
extern CyU3PReturnStatus_t
CyU3PMipicsiSetIntfParams  (

        CyU3PMipicsiCfg_t *csiCfg,   /**< Configuration Data to be set to the MIPI-CSI interface */

        CyBool_t wakeOnConfigure     /**< Start the MIPI-CSI interface clocks immediately after configuring 
                                        If wakeOnConfigure is CyFalse, CyU3PMipicsiWakeup() 
                                        must be called to start the clocks when ready for transfers. */ 
        );


/** \brief Read MIPI-CSI interface configuration from the block

    <B>Description</B>\n
    This function is used to read back the MIPI-CSI interface parameters from the block. 
    The parameters read back are provided to the calling function via the pointer of type 
    CyU3PMipicsiCfg_t passed in from the calling function.

    <B>Note</B>\n
    Memory for the pointer csiCfg must be allocated in the calling function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT - If an I2C read/write timeout occurs during the operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - If the part on which this function is called is not a CX3 device.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - If an invalid argument is read from the interface. \n
    * CY_U3P_ERROR_UNKNOWN - If a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_NOT_STARTED - If the interface has not been initialized. \n
    * CY_U3P_ERROR_NULL_POINTER - If the csiCfg object is uninitialized. \n


    **\see
    *\see CyU3PMipicsiCfg_t
    *\see CyU3PMipicsiQueryIntfParams
*/
extern CyU3PReturnStatus_t 
CyU3PMipicsiQueryIntfParams (

        CyU3PMipicsiCfg_t * csiCfg       /**< Configuration Data read back from the MIPI-CSI interface*/
        );


/** \brief Setup MIPI CSI-2 Reciever PHY Time Delay register.

    <B>Description</B>\n
    This function is used to set the CX3_PHY_TIME_DELAY register values 
    Please refer to CX3 TRM for details on this register.
    
    <B>Note</B>\n
    This function should be not be called while the MIPI-CSI PLL clocks are active. Either 
    call after calling CyU3PMipicsiSetIntfParams() with wakeOnConfigure set to False (before 
    calling CyU3PMipicsiWakeup() ), or call CyU3PMipicsiSleep() before calling this API.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT - If an I2C read/write timeout occurs during the operation.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - If an invalid argument is read from the interface. \n
    * CY_U3P_ERROR_UNKNOWN - If a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_NOT_STARTED - If the interface has not been initialized. \n

    **\see
    *\see CyU3PMipicsiQueryIntfParams
*/

extern CyU3PReturnStatus_t
CyU3PMipicsiSetPhyTimeDelay  (

        uint8_t tdTerm,                 /**< TD TERM Selection for MIPI CSI-2 reciever PHY. 
                                          Valid values 0 and 1. */
        uint8_t thsSettleDelay          /**< THS Settle timer delay value. 
                                          Valid range 0x00-0x7F. */
        );

/** \brief Reset the MIPI-CSI interface

    <B>Description</B>\n
    This function is used to reset the MIPI-CSI block on the CX3. The MIPI-CSI block provides two reset
    modes - Hard reset, which power-cycles the entire block, and Soft reset, which does not reset the 
    I2C communication channel used by the block.
    The reset mode is selcted via the parameter of type CyU3PMipicsiReset_t passed to this function.

    <B>Note</B>\n
    The Soft Reset cannot be called on the interface until the block has been initialized. A Hard reset 
    however, can be called on the interface at any point in time. The CyU3PMipicsiInit() call internally
    performs a Hard reset on the interface before initializing it.
    A Hard reset sets the default state of the MIPI XReset and XShutdown signals as Drive 0.
    If either of these signals, needs to be in Drive 1 state for sensor operation, it needs to be 
    explictily set to Drive 1 state using CyU3PMipicsiSetSensorControl() after calling 
    CyU3PMipicsiReset(CY_U3P_CSI_HARD_RST). A Soft reset does not change the state of the XReset 
    and XShutdown signals.
    
    <B>Return value</B>\n
    * CY_U3P_SUCCESS if the operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT - If an I2C read/write timeout occurs during the operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - If the part on which this function is called is not a CX3 device.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - If an invalid argument is passed to the function. \n
    * CY_U3P_ERROR_UNKNOWN - If a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_NOT_STARTED - If the interface has not been initialized (Soft Reset Only). \n

    **\see
    *\see CyU3PMipicsiInit
    *\see CyU3PMipicsiReset_t
*/
extern CyU3PReturnStatus_t
CyU3PMipicsiReset (
        CyU3PMipicsiReset_t resetType     /**< Reset Type*/
        );


/** \brief Reset the CX3 Device

    <B>Description</B>\n
    This function is similar to the CyU3PDeviceReset function used for FX3/FX3S devices. In addition to 
    resetting the CX3 device, this  function also drives the XRES line output to reset the Image Sensor  
    before the CX3 reset.

    <B>Return value</B>\n
    * None as this function does not return.

    **\see
    *\see CyU3PDeviceReset
*/
extern void
CyU3PCx3DeviceReset(
        CyBool_t isWarmReset,           /**< Whether this should be a warm reset or a cold reset. In the 
                                             case of a warm  reset, the previously loaded firmware will 
                                             start executing again. In the case of a cold reset, the 
                                             firmware download to CX3 needs to be performed again. */

        CyBool_t sensorResetHigh        /**< True if Sensor is to be reset by driving XRES High, 
                                             False if Sensor reset needs XRES as low. */
        );


/** \brief Get MIPI-CSI block Phy error counts

    <B>Description</B>\n
    This function is used to get a count of CSI protocol and physical layer errors from the MIPI-CSI block. 
    The function takes a parameter which determines whether or not the error counts on the interface are 
    cleared. The error counts for each error type is retrieved via an pointer of type 
    CyU3PMipicsiErrorCounts_t passed to this function. The error count values for each type can reach a 
    maximum count of 0xFF. The count values will continue to report the existing error value on each call 
    unless the function explicitly clears the counts using clrErrCnts.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT - If an I2C read/write timeout occurs during the operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - If the part on which this function is called is not a CX3 device.\n
    * CY_U3P_ERROR_UNKNOWN - If a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_NOT_STARTED - If the interface has not been initialized. \n
    * CY_U3P_ERROR_NULL_POINTER - If the clrErrCnts object is uninitialized. \n

    **\see
    *\see CyU3PMipicsiErrorCounts_t
*/
extern CyU3PReturnStatus_t 
CyU3PMipicsiGetErrors (
        CyBool_t clrErrCnts,                        /**< Set to CyTrue to clear the error counts*/
        CyU3PMipicsiErrorCounts_t * errorCounts     /**< Error Counts*/
        );


/** \brief MIPI-CSI block Wakeup

    <B>Description</B>\n
    This function is used enable the clocks on the MIPI-CSI interface block to take it from Low power 
    sleep to Active. 

   <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT - If an I2C read/write timeout occurs during the operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - If the part on which this function is called is not a CX3 device.\n
    * CY_U3P_ERROR_UNKNOWN - If a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_NOT_STARTED - If the interface has not been initialized. \n

    **\see
    *\see CyU3PMipicsiSleep 
*/
extern CyU3PReturnStatus_t 
CyU3PMipicsiWakeup (
        void
        );


/** \brief Mipi-CSI block Sleep

    <B>Description</B>\n
    This function is used to disable the PLL clocks on the MIPI-CSI interface block and place it 
    in low-power sleep. No data transfers from the Image Sensor to the CX3 will occur while the
    block is in low power sleep mode.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT - If an I2C read/write timeout occurs during the operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - If the part on which this function is called is not a CX3 device.\n
    * CY_U3P_ERROR_UNKNOWN - If a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_NOT_STARTED - If the interface has not been initialized. \n
    
    **\see
    *\see CyU3PMipicsiWakeup
*/
extern CyU3PReturnStatus_t 
CyU3PMipicsiSleep (
        void
        );


/** \brief Sensor XRES and XSHUTDOWN Signals.

    <B>Description</B>\n
    This function is used to drive the XRES and XSHUTDOWN signals from the CX3 to Image sensor. 
    The function allows for the signals to be driven high or low.
    The function can drive either one of the two signals or both signals (both driven high or 
    both driven low) simultaneously. 
    To set both signals to the same value use the mask 
    (CY_U3P_CSI_IO_XRES | CY_U3P_CSI_IO_XSHUTDOWN) as value for io.
    To set the two signals to separate values multiple calls to this function are required 
    (once for each signal).

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the operation completes successfully.\n
    * CY_U3P_ERROR_TIMEOUT - If an I2C read/write timeout occurs during the operation.\n
    * CY_U3P_ERROR_NOT_SUPPORTED - If the part on which this function is called is not a CX3 device.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - If an invalid argument is passed to the function. \n
    * CY_U3P_ERROR_UNKNOWN - If a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_NOT_STARTED - If the interface has not been initialized (Soft Reset Only). \n

    **\see
    *\see CyU3PMipicsiSensorIo_t

*/
extern CyU3PReturnStatus_t 
CyU3PMipicsiSetSensorControl (
        CyU3PMipicsiSensorIo_t io,  /**< IO to be driven. */
        CyBool_t value              /**< CyTrue to drive Signal High, CyFalse for Low. */
        );


/** \brief Check if MIPI-CSI interface is Active or in Low power sleep.

    <B>Description</B>\n
    This function is used to check if the MIPI-CSI interface is Active or in low power sleep. 

    <B>Return value</B>\n
    * CyTrue - If the interface block is Active.\n
    * CyFalse - If the interface block is in Low power sleep
*/
extern CyBool_t 
CyU3PMipicsiCheckBlockActive(
        void);

/*********************************************/
/* CX3 Device Configuration Helper Functions */
/*********************************************/

/** \brief  Initialize the GPIO block on the CX3.

    <B>Description</B>\n
    This function is a helper routine to initialize the GPIO block with default values for 
    the CX3 device. 
    
    The function internally calls CyU3PGpioInit with the following parameters:\n
        CyU3PGpioClock_t settings:                                      \n
                fastClkDiv = 2;                                         \n
                slowClkDiv = 0;                                         \n
                simpleDiv  = CY_U3P_GPIO_SIMPLE_DIV_BY_2;               \n
                clkSrc     = CY_U3P_SYS_CLK;                            \n
                halfDiv    = 0;                                         \n
        Callback set to NULL.
    
    This function should be called before initializing the PIB block or calling 
    CyU3PMipicsiInit(). In case different parameters are required, the application
    can use its own code to initialize the GPIO block prior to initializing the PIB 
    block or calling CyU3PMipicsiInit().

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the GPIO initialization is successful
    * CY_U3P_ERROR_ALREADY_STARTED - If the GPIO block has already been initialized

    **\see
    *\see CyU3PGpioInit
*/
extern CyU3PReturnStatus_t 
CyU3PMipicsiInitializeGPIO (
        void
        );


/** \brief Configure and Initialize the I2C Block on the CX3
     
    <B>Description</B>\n
    This function is a helper routine to initialize the I2C block to either 100KHz or
    400KHz for the CX3 device. 
    
    The function internally calls CyU3PI2cInit and then calls CyU3PI2cSetConfig to 
    configure the I2C block with the following parameters:\n
        CyU3PI2cConfig_t settings:                                      \n
                bitRate    = 100000 or 400000 depending on  freq        \n
                isDma      = CyFalse;                                   \n
                busTimeout = 0xFFFFFFFF                                 \n
                dmaTimeout = 0xFFFF;                                    \n
        Callback is set to NULL.
    
    The I2C block needs to be initialized prior to calling CyU3PMipicsiInit(). 
    In case different parameters are required, the application
    can use its own code to initialize the I2C block prior to calling 
    CyU3PMipicsiInit() but must ensure that the block is initialized to either 100KHz 
    or 400KHz.

    <B>Note</B>\n
    If the I2C block is already initialized before this function is called, it will override
    the current configuration of the I2C block with the configuration specified in the 
    description of this function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the initialization is successful
    * CY_U3P_ERROR_NOT_CONFIGURED - When I2C has not been enabled in IO configuration.
    * CY_U3P_ERROR_TIMEOUT - When there is timeout happening during configuration
    * CY_U3P_ERROR_MUTEX_FAILURE- When there is a failure in acquiring a mutex lock

    **\see
    *\see CyU3PI2cInit
    *\see CyU3PI2cSetConfig

*/
extern CyU3PReturnStatus_t 
CyU3PMipicsiInitializeI2c (
        CyU3PMipicsiI2cFreq_t freq  /**< Frequency 100KHz or 400KHz */
        );

/** \brief Initialize and configure the PIB block on the CX3.

    <B>Description</B>\n
    This function is a helper routine to initialize the PIB block on the CX3 part used by the 
    fixed function GPIF . 
    
    The function internally calls CyU3PPibInit() with the following parameters:\n
        CyU3PPibClock_t settings:                                      \n
                clkDiv          = 2;                                    \n
                clkSrc          = CY_U3P_SYS_CLK                        \n
                isHalfDiv       = CyFalse;                              \n
                isDllEnable     = CyFalse;                              \n

        doInit is set to CyTrue.
    
    The PIB block needs to be initialized, prior to calling CyU3PMipicsiGpifLoad(), to configure 
    the fixed function GPIF for the CX3 part.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the operation is successful

    **\see
    *\see CyU3PPibInit
*/
extern CyU3PReturnStatus_t 
CyU3PMipicsiInitializePIB (
        void
        );





/**********************************/
/* CX3 GPIF Waveform Load function*/
/**********************************/

/** \brief Fixed Function GPIF Waveform Load

    <B>Description</B>\n
    The CX3 has a fixed function GPIF interface designed for Image sensor data acquisition 
    from the MIPI-CSI interface. This function allows selection of the GPIF data bus-width
    and configuration of the size of the DMA buffer provided for GPIF transfers.
    The PIB block should have been initialized prior to calling this function.

    <B>Return value</B>\n
    * CY_U3P_SUCCESS - If the operation is successful
    * CY_U3P_ERROR_NOT_SUPPORTED - If the part on which this function is called is not a CX3 device.\n
    * CY_U3P_ERROR_BAD_ARGUMENT - If an invalid argument is passed to the function. \n
    * CY_U3P_ERROR_UNKNOWN - If a general or unknown error occurred during the operation.\n
    * CY_U3P_ERROR_ALREADY_STARTED - If the GPIF hardware is running.\n

    **\see
    *\see CyU3PMipicsiBusWidth_t
    *\see CyU3PMipicsiInitializePIB
*/
extern CyU3PReturnStatus_t 
CyU3PMipicsiGpifLoad (
        CyU3PMipicsiBusWidth_t busWidth,        /**< Load GPIF State machine and settings for selected busWidth*/
        uint32_t bufferSize                     /**< Set Data counter based on DMA buffer size being used. 
                                                     This value MUST match the size of the buffer actual data buffer
                                                     being used i.e. (The size being passed to the
                                                     CyU3PDmaChannelCreate API) - (The Size of Header and Footer)
                                                     e.g. If dmaCfg is CyU3PDmaChannelConfig_t object being passed
                                                     to the ChannelCreate API the value of bufferSize should be
                                                     (dmaCfg.size - (dmaCfg.prodHeader + dmaCfg.prodFooter)) */
        );

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYU3MIPICSI_H_ */

/*[]*/

