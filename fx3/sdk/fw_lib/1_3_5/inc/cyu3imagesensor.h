/*
 ## Cypress FX3 Core Library Header (cyu3imagesensor.h)
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

/* CX3 Header file for APIs implementing Image Sensor specific functions
 * Functionality for the Aptina AS0260 sensor and the 
 * Omnivision OV5640 sensors is provided through libraries
 * cy_as0260.a and cy_ov5640.a
 */
#ifndef _INCLUDED_CYU3_CX3_IMAGESENSOR_H_
#define _INCLUDED_CYU3_CX3_IMAGESENSOR_H_

#include <cyu3types.h>

/* Enumeration defining Stream Format used for CyCx3_ImageSensor_Set_Format */
typedef enum CyU3PSensorStreamFormat_t
{
    SENSOR_YUV2 = 0,    /* UVC YUV2 Format*/
    SENSOR_RGB565       /* RGB565 Format  */
}CyU3PSensorStreamFormat_t;


/* Initialize Image Sensor*/ 
extern CyU3PReturnStatus_t CyCx3_ImageSensor_Init(void);

/* Set Sensor to output 720p Stream */
extern CyU3PReturnStatus_t CyCx3_ImageSensor_Set_720p(void);

/* Set Sensor to output 1080p Stream */
extern CyU3PReturnStatus_t CyCx3_ImageSensor_Set_1080p(void);

/* Set Sensor to output QVGA Stream */
extern CyU3PReturnStatus_t CyCx3_ImageSensor_Set_Qvga(void);

/* Set Sensor to output VGA Stream */
extern CyU3PReturnStatus_t CyCx3_ImageSensor_Set_Vga(void);

/* Set Sensor to output 5Megapixel Stream */
extern CyU3PReturnStatus_t CyCx3_ImageSensor_Set_5M(void);


/* Put Image Sensor to Sleep/Low Power Mode */
extern CyU3PReturnStatus_t CyCx3_ImageSensor_Sleep(void);

/* Wake Image Sensor from Sleep/Low Power Mode to Active Mode */
extern CyU3PReturnStatus_t CyCx3_ImageSensor_Wakeup(void);

/* Set Image Sensor Data Format */
extern CyU3PReturnStatus_t CyCx3_ImageSensor_Set_Format(CyU3PSensorStreamFormat_t format);

/* Trigger Autofocus for the Sensor*/
extern CyU3PReturnStatus_t CyCx3_ImageSensor_Trigger_Autofocus(void);

#endif /* _INCLUDED_CYU3_CX3_IMAGESENSOR_H_ */

/*[]*/
