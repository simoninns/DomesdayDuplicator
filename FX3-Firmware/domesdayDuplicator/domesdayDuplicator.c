/************************************************************************

	domesdayDuplicator.c

	FX3 Firmware main functions
	DomesdayDuplicator - LaserDisc RF sampler
	Copyright (C) 2017 Simon Inns

	This file is part of Domesday Duplicator.

	Domesday Duplicator is free software: you can redistribute it and/or
	modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation, either version 3 of the
	License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	Email: simon.inns@gmail.com

************************************************************************/

// External includes
#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3dma.h"
#include "cyu3error.h"
#include "cyu3usb.h"
#include "cyu3uart.h"
#include "cyu3gpio.h"
#include "cyu3utils.h"
#include "cyu3pib.h"
#include "cyu3gpif.h"

// Local includes
#include "domesdayDuplicator.h"
#include "domesdayDuplicatorGpif.h"

// Global definitions
CyU3PThread glAppThread; // Application thread structure
CyU3PDmaChannel glDmaChHandle; // DMA channel handle

CyBool_t glIsApplnActive = CyFalse; // Application active/ready flag
CyBool_t glForceLinkU2 = CyFalse; // Force U2 flag

// Main application function
int main(void)
{
    CyU3PIoMatrixConfig_t io_cfg;
    CyU3PPibClock_t pibClock;
    CyU3PGpioClock_t gpioClock;
    CyU3PGpioSimpleConfig_t gpioConfig;
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    // Perform initial clock configuration of the FX3
    CyU3PSysClockConfig_t clockConfig;
    clockConfig.setSysClk400  = CyTrue; // True = 403.2 MHz, false = 384 MHz
    clockConfig.cpuClkDiv     = 2;
    clockConfig.dmaClkDiv     = 2;
    clockConfig.mmioClkDiv    = 2;
    clockConfig.useStandbyClk = CyFalse;
    clockConfig.clkSrc        = CY_U3P_SYS_CLK;
    status = CyU3PDeviceInit(&clockConfig);
    if (status != CY_U3P_SUCCESS) {
        goto handleFatalError;
    }

    // Initialise the state of the caches - Icache, Dcache, DMAcache
    status = CyU3PDeviceCacheControl(CyTrue, CyFalse, CyFalse);
    if (status != CY_U3P_SUCCESS) {
        goto handleFatalError;
    }

    // Initialise the IO matrix
    io_cfg.isDQ32Bit = CyFalse; // Data bus is 16-bits
    io_cfg.useUart   = CyTrue;
    io_cfg.useI2C    = CyFalse;
    io_cfg.useI2S    = CyFalse;
    io_cfg.useSpi    = CyFalse;
    io_cfg.lppMode   = CY_U3P_IO_MATRIX_LPP_UART_ONLY; // 16-bit data bus with UART

    io_cfg.gpioSimpleEn[0] = 0;
    io_cfg.gpioSimpleEn[1] = 0x08000000; // GPIO 59
    io_cfg.gpioComplexEn[0] = 0;
    io_cfg.gpioComplexEn[1] = 0;

    status = CyU3PDeviceConfigureIOMatrix(&io_cfg);
    if (status != CY_U3P_SUCCESS) {
        goto handleFatalError;
    }

    // Start GPIF clocks, they need to be running before we attach a DMA channel to GPIF
	pibClock.clkDiv = 4; // 403.2 / 4 = 100.8 MHz
	pibClock.clkSrc = CY_U3P_SYS_CLK;
	pibClock.isHalfDiv = CyFalse;
	pibClock.isDllEnable = CyFalse; // Disable Dll (not required for synchronous applications)
	status = CyU3PPibInit(CyTrue, &pibClock);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

    // Initialise the GPIO module clocks, needed for nRESET towards FPGA
	gpioClock.fastClkDiv = 2;
	gpioClock.slowClkDiv = 0;
	gpioClock.simpleDiv = CY_U3P_GPIO_SIMPLE_DIV_BY_2;
	gpioClock.clkSrc = CY_U3P_SYS_CLK;
	gpioClock.halfDiv = 0;
	status = CyU3PGpioInit(&gpioClock, 0);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Claim CTRL[10]/GPIO27 from the GPIF Interface (nRESET signal)
	status = CyU3PDeviceGpioOverride(27, CyTrue);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Bring the FPGA out of reset by driving nRESET/GPIO27 high
	CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.outValue = 1;
	gpioConfig.driveLowEn = CyTrue;
	gpioConfig.driveHighEn = CyTrue;
	status = CyU3PGpioSetSimpleConfig(27, &gpioConfig);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Claim CTRL[1]/GPIO18 from the GPIF Interface (nREADY signal)
	status = CyU3PDeviceGpioOverride(18, CyTrue);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Make the FPGA transfer not ready by driving nREADY/GPIO18 high
	CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.outValue = 1;
	gpioConfig.driveLowEn = CyTrue;
	gpioConfig.driveHighEn = CyTrue;
	status = CyU3PGpioSetSimpleConfig(18, &gpioConfig);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

    // Initialise the RTOS kernel
    CyU3PKernelEntry();

    return 0;

handleFatalError:

    // An unrecoverable error has occurred
	// Loop forever
	while(1);
}

// Function to initialise the application's main thread
void domDupThreadInitialise(uint32_t input)
{
    CyU3PReturnStatus_t status;
    CyU3PUsbLinkPowerMode powerState;

//    uint32_t debugPause = 0;

    // Initialise the debug console
    domDupDebugInit();
    CyU3PDebugPrint(1, "\r\n\r\nDomesday Duplicator FX3 Firmware - V1.5b\r\n");
    CyU3PDebugPrint(1, "(c)2017 Simon Inns - http://www.domesday86.com\r\n");
    CyU3PDebugPrint(1, "Debug console initialised\r\n");

    // Initialise the application
    domDupInitialiseApplication();

    // Main application thread loop
    while(1) {
        // Try to get the USB 3.0 link back to U0
        if (glForceLinkU2) {
        	status = CyU3PUsbGetLinkPowerState(&powerState);
            while ((glForceLinkU2) && (status == CY_U3P_SUCCESS) && (powerState == CyU3PUsbLPM_U0)) {
                // Try to get to U2 state
                CyU3PUsbSetLinkPowerState(CyU3PUsbLPM_U2);
                CyU3PThreadSleep(5);
                status = CyU3PUsbGetLinkPowerState(&powerState);
            }
        } else {
            // Try to get the USB link back to U0
            if (CyU3PUsbGetSpeed () == CY_U3P_SUPER_SPEED) {
            	status = CyU3PUsbGetLinkPowerState (&powerState);
                while ((status == CY_U3P_SUCCESS) && (powerState >= CyU3PUsbLPM_U1) &&
                	(powerState <= CyU3PUsbLPM_U3)) {
                    CyU3PUsbSetLinkPowerState(CyU3PUsbLPM_U0);
                    CyU3PThreadSleep(1);
                    status = CyU3PUsbGetLinkPowerState(&powerState);
                }
            }
        }

        // Output application heart beat character to debug console
//        debugPause++;
//        if (debugPause == 1000000) {
//        	CyU3PDebugPrint(1, "*");
//        	debugPause = 0;
//        }
    }
}

// Function to create the initial application thread
void CyFxApplicationDefine(void)
{
    void *ptr = NULL;
    uint32_t returnCode = CY_U3P_SUCCESS;

    // Allocate the memory for the threads
    ptr = CyU3PMemAlloc(CY_FX_GPIFTOUSB_THREAD_STACK);

    // Create the application's main thread
    returnCode = CyU3PThreadCreate(
		&glAppThread,						// Application thread structure
		"28:domDup",						// Thread ID and thread name
		domDupThreadInitialise,				// Application thread entry function
		0,									// No input parameter to thread
		ptr,								// Pointer to the allocated thread stack
		CY_FX_GPIFTOUSB_THREAD_STACK,		// Application thread stack size
		CY_FX_GPIFTOUSB_THREAD_PRIORITY,	// Application thread priority
		CY_FX_GPIFTOUSB_THREAD_PRIORITY,	// Application thread priority
		CYU3P_NO_TIME_SLICE,				// No time slice for the application thread
		CYU3P_AUTO_START					// Start the thread immediately
		);

    // Check the return code
    if (returnCode != 0) {
    	// Could not create initial thread
    	// Application cannot start
        while(1);
    }
}

// Function to initialise the USB application (note: does not start application)
void domDupInitialiseApplication(void)
{
    CyU3PReturnStatus_t apiReturnStatus = CY_U3P_SUCCESS;
    CyBool_t noRenum = CyFalse;

    // Start the USB processing
    apiReturnStatus = CyU3PUsbStart();
    if (apiReturnStatus == CY_U3P_ERROR_NO_REENUM_REQUIRED) noRenum = CyTrue;
    else if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PUsbStart failed, Error code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // Use fast enumeration
    CyU3PUsbRegisterSetupCallback(domDupUSBSetupCB, CyTrue);

    // Add USB event callback function
    CyU3PUsbRegisterEventCallback(domDupUSBEventCB);

    // Add LPM request callback function
    CyU3PUsbRegisterLPMRequestCallback(domDupLPMRequestCB);

    // Set the USB descriptors

    // Super speed device descriptor (USB 3)
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_DEVICE_DESCR, 0, (uint8_t *)USB30DeviceDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PUsbSetDesc USB3 failed, Error code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // High speed device descriptor (USB 2)
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_DEVICE_DESCR, 0, (uint8_t *)USB20DeviceDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PUsbSetDesc USB 2 failed, Error code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // BOS descriptor
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_BOS_DESCR, 0, (uint8_t *)USBBOSDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PUsbSetDesc BOS failed, Error code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // Device qualifier descriptor
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_DEVQUAL_DESCR, 0, (uint8_t *)USBDeviceQualDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PUsbSetDesc qualifier descriptor failed, Error code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // Super speed configuration descriptor
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_CONFIG_DESCR, 0, (uint8_t *)USBSSConfigDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PUsbSetDesc configuration descriptor failed, Error code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // High speed configuration descriptor
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_CONFIG_DESCR, 0, (uint8_t *)USBHSConfigDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PUsbSetDesc Other Speed Descriptor failed, Error Code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // Full speed configuration descriptor
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_FS_CONFIG_DESCR, 0, (uint8_t *)USBFSConfigDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PUsbSetDesc Full-Speed Descriptor failed, Error Code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // String descriptor 0
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 0, (uint8_t *)USBStringLangIDDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PUsbSetDesc string 0 descriptor failed, Error code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // String descriptor 1
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 1, (uint8_t *)USBManufactureDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PUsbSetDesc string descriptor 1 failed, Error code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // String descriptor 2
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 2, (uint8_t *)USBProductDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint(4, "CyU3PUsbSetDesc string descriptor 2 failed, Error code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // Show status in debug console
    CyU3PDebugPrint (4, "Initialisation successful; Connecting to host\r\n");

    // Connect to the host
    if (!noRenum) {
        apiReturnStatus = CyU3PConnectState(CyTrue, CyTrue);
        if (apiReturnStatus != CY_U3P_SUCCESS) {
            CyU3PDebugPrint (4, "CyU3PConnectState failed, Error code = %d\n", apiReturnStatus);
            domDupErrorHandler(apiReturnStatus);
        }
    } else {
    	// If application is already active.  Restart the application
        if (glIsApplnActive) domDupStopApplication ();

        // Start the application
        domDupStartApplication ();
    }
    CyU3PDebugPrint(8, "Application initialisation complete.\r\n");
}

// Function to start application once SET_CONF received from host
void domDupStartApplication(void)
{
    uint16_t size = 0;
    CyU3PEpConfig_t epCfg;
    CyU3PDmaChannelConfig_t dmaConfig;
    CyU3PReturnStatus_t apiReturnStatus = CY_U3P_SUCCESS;
    CyU3PUSBSpeed_t usbSpeed = CyU3PUsbGetSpeed();

    // Check the USB speed and set the end-point size
    switch (usbSpeed) {
    case CY_U3P_FULL_SPEED:
        size = 64;
        break;

    case CY_U3P_HIGH_SPEED:
        size = 512;
        break;

    case  CY_U3P_SUPER_SPEED:
        size = 1024;
        break;

    default:
        CyU3PDebugPrint (4, "ERROR: CyU3PUsbGetSpeed returned an invalid speed!\r\n");
        domDupErrorHandler (CY_U3P_ERROR_FAILURE);
        break;
    }

    // Check that we are connected to a USB 3 host
    if (usbSpeed != CY_U3P_SUPER_SPEED) {
    	CyU3PDebugPrint (4, "ERROR: USB 2 is not supported, connect device to a USB 3 port!\r\n");
    	domDupErrorHandler (CY_U3P_ERROR_FAILURE);
    }

    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyTrue;
    epCfg.epType = CY_U3P_USB_EP_BULK;
    epCfg.burstLen = (usbSpeed == CY_U3P_SUPER_SPEED) ? (CY_FX_EP_BURST_LENGTH) : 1;
    epCfg.streams = 0;
    epCfg.pcktSize = size;

    // Configure consumer end-point
    apiReturnStatus = CyU3PSetEpConfig(CY_FX_EP_CONSUMER, &epCfg);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PSetEpConfig failed, Error code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // Flush the end-point
    CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);

    // Create a DMA AUTO channel for the GPIF to USB transfer
    CyU3PMemSet ((uint8_t *)&dmaConfig, 0, sizeof (dmaConfig));
    dmaConfig.size  = CY_FX_DMA_BUF_SIZE;
    dmaConfig.count = CY_FX_DMA_BUF_COUNT;
    dmaConfig.prodSckId = CY_FX_EP_PRODUCER_SOCKET;
    dmaConfig.consSckId = CY_FX_EP_CONSUMER_SOCKET;
    dmaConfig.dmaMode = CY_U3P_DMA_MODE_BYTE;

    apiReturnStatus = CyU3PDmaChannelCreate(&glDmaChHandle, CY_U3P_DMA_TYPE_AUTO, &dmaConfig);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PDmaChannelCreate failed, Error code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // Start the DMA channel transfer
    apiReturnStatus = CyU3PDmaChannelSetXfer(&glDmaChHandle, 0);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4, "CyU3PDmaChannelSetXfer failed, Error code = %d\n", apiReturnStatus);
		domDupErrorHandler(apiReturnStatus);
	}

    // Load the GPIF state machine
    apiReturnStatus = CyU3PGpifLoad (&CyFxGpifConfig);

    // Register callback for GPIF CPU interrupt events
    CyU3PGpifRegisterCallback(GpifDmaEventCB);

    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PGpifLoad failed, error code = %d\n", apiReturnStatus);
        domDupErrorHandler (apiReturnStatus);
    }

    // Watermark value = 3, bus width = 16
    // Therefore, the number of 16-bit data words that may be written after the clock edge at which the partial
    // flag is sampled asserted = (3 x (32/16))  4 = 2

    // Set the thread 0 water-mark level to 1x 32 bit word
    apiReturnStatus = CyU3PGpifSocketConfigure(0, CY_FX_EP_PRODUCER_SOCKET, 3, CyFalse, 1);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4, "CyU3PGpifSocketConfigure failed for thread0, error code = %d\n", apiReturnStatus);
		domDupErrorHandler (apiReturnStatus);
	}

	// Start the GPIF state machine
    apiReturnStatus = CyU3PGpifSMStart (START, ALPHA_START);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "CyU3PGpifSMStart failed, error code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // Set the application active flag to true
    glIsApplnActive = CyTrue;
}

// Function to stop the application.  Called when host signals RESET or DISCONNECT
void domDupStopApplication(void)
{
    CyU3PEpConfig_t epCfg;
    CyU3PReturnStatus_t apiReturnStatus = CY_U3P_SUCCESS;

    // Set the application activity flag to false
    glIsApplnActive = CyFalse;

    // Disable the GPIF state-machine
    CyU3PGpifDisable (CyTrue);

    // Disable PIB
    CyU3PPibDeInit();

    // Destroy DMA channels
    CyU3PDmaChannelDestroy (&glDmaChHandle);

    // Flush end-points
    CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);

    // Disable end-points
    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyFalse;

    // Un-configure consumer end-point
    apiReturnStatus = CyU3PSetEpConfig(CY_FX_EP_CONSUMER, &epCfg);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PSetEpConfig failed, Error code = %d\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }
}

// Error handling function
void domDupErrorHandler(CyU3PReturnStatus_t apiReturnStatus)
{
	// Application failed; loop forever
    while(1) {
        CyU3PThreadSleep(100);
    }
}



// Initialise debug console.  Debug is routed to UART
// Serial speed is 115200 8N1
void domDupDebugInit(void)
{
    CyU3PUartConfig_t uartConfig;
    CyU3PReturnStatus_t apiReturnStatus = CY_U3P_SUCCESS;

    // Initialise the UART
    apiReturnStatus = CyU3PUartInit();
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        // Call the error handling function
        domDupErrorHandler(apiReturnStatus);
    }

    // Configure the UART
    CyU3PMemSet ((uint8_t *)&uartConfig, 0, sizeof (uartConfig));
    uartConfig.baudRate = CY_U3P_UART_BAUDRATE_115200;
    uartConfig.stopBit = CY_U3P_UART_ONE_STOP_BIT;
    uartConfig.parity = CY_U3P_UART_NO_PARITY;
    uartConfig.txEnable = CyTrue;
    uartConfig.rxEnable = CyFalse;
    uartConfig.flowCtrl = CyFalse;
    uartConfig.isDma = CyTrue;

    apiReturnStatus = CyU3PUartSetConfig (&uartConfig, NULL);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        domDupErrorHandler(apiReturnStatus);
    }

    // Set the UART transfer to a large number
    apiReturnStatus = CyU3PUartTxSetBlockXfer (0xFFFFFFFF);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        domDupErrorHandler(apiReturnStatus);
    }

    // Initialise debug on the UART
    apiReturnStatus = CyU3PDebugInit (CY_U3P_LPP_SOCKET_UART_CONS, 8);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        domDupErrorHandler(apiReturnStatus);
    }

    CyU3PDebugPreamble(CyFalse);
}

// Call back functions ----------------------------------------------------------------------------------

// Handle CPU_INT from GPIF callback (used for debug)
void GpifDmaEventCB(CyU3PGpifEventType Event, uint8_t State)
{
	if (Event == CYU3P_GPIF_EVT_SM_INTERRUPT) CyU3PDebugPrint(8, "\r\nDebug: Interrupt received from GPIF\r\n");
}

// USB set-up request callback
CyBool_t domDupUSBSetupCB (uint32_t setupData0, uint32_t setupData1)
{
    uint8_t  bRequest, bReqType;
    uint8_t  bType, bTarget;
    uint16_t wValue;
    uint16_t wIndex;
    CyBool_t isHandled = CyFalse;

    /* Decode the fields from the setup request. */
    bReqType = (setupData0 & CY_U3P_USB_REQUEST_TYPE_MASK);
    bType    = (bReqType & CY_U3P_USB_TYPE_MASK);
    bTarget  = (bReqType & CY_U3P_USB_TARGET_MASK);
    bRequest = ((setupData0 & CY_U3P_USB_REQUEST_MASK) >> CY_U3P_USB_REQUEST_POS);
    wValue   = ((setupData0 & CY_U3P_USB_VALUE_MASK)   >> CY_U3P_USB_VALUE_POS);
    wIndex   = ((setupData1 & CY_U3P_USB_INDEX_MASK)   >> CY_U3P_USB_INDEX_POS);

    // Handle vendor specific requests from the host
    if (bType == CY_U3P_USB_VENDOR_RQT) {
    	if (glIsApplnActive) {
			// Handle vendor request for collection start/stop
			if (bRequest == 0xB5) {
				if (wValue == 1) {
					// Start collection request from USB host
					CyU3PDebugPrint(8, "START data collection request received from host\r\n");
					CyU3PGpioSetValue(18, 0); // nREADY GPIO low
				}

				if (wValue == 0) {
					// Stop collection request from USB host
					CyU3PDebugPrint(8, "STOP data collection request received from host\r\n");
					CyU3PGpioSetValue(18, 1); // nREADY GPIO high
				}
			}

			// ACK the request
			CyU3PUsbAckSetup();
		}
    }

    if (bType == CY_U3P_USB_STANDARD_RQT) {
        // Target interface - Set/clear feature
        if ((bTarget == CY_U3P_USB_TARGET_INTF) &&
        	((bRequest == CY_U3P_USB_SC_SET_FEATURE) || (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)) &&
        	(wValue == 0)) {
            if (glIsApplnActive) {
                CyU3PUsbAckSetup();

                // Force link to U2 on suspend
                if (bRequest == CY_U3P_USB_SC_SET_FEATURE) {
                    glForceLinkU2 = CyTrue;
                } else {
                    glForceLinkU2 = CyFalse;
                }
            }
            else CyU3PUsbStall(0, CyTrue, CyFalse);

            isHandled = CyTrue;
        }

        // Target end-point - Clear feature request
        if ((bTarget == CY_U3P_USB_TARGET_ENDPT) &&
        	(bRequest == CY_U3P_USB_SC_CLEAR_FEATURE) &&
        	(wValue == CY_U3P_USBX_FS_EP_HALT)) {
            if (glIsApplnActive) {
                if (wIndex == CY_FX_EP_CONSUMER) {
                    CyU3PDmaChannelReset (&glDmaChHandle);
                    CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);
                    CyU3PUsbResetEp (CY_FX_EP_CONSUMER);
                    CyU3PDmaChannelSetXfer(&glDmaChHandle, 0);
                    CyU3PUsbStall (wIndex, CyFalse, CyTrue);
                    isHandled = CyTrue;
                    CyU3PUsbAckSetup ();
                }
            }
        }
    }

    return isHandled;
}

// Callback function to handle USB events
void domDupUSBEventCB(CyU3PUsbEventType_t eventType, uint16_t eventData)
{
    switch (eventType) {
    case CY_U3P_USB_EVENT_CONNECT:
		CyU3PDebugPrint(8, "CY_U3P_USB_EVENT_CONNECT detected\r\n");
		break;

    case CY_U3P_USB_EVENT_SETCONF:
    	CyU3PDebugPrint(8, "CY_U3P_USB_EVENT_SETCONF detected\r\n");
    	// If the application is already active, stop it
        if (glIsApplnActive) {
            domDupStopApplication();
        }

        // Start the application
        domDupStartApplication();
        break;

    case CY_U3P_USB_EVENT_RESET:
    case CY_U3P_USB_EVENT_DISCONNECT:
        glForceLinkU2 = CyFalse;

        // Stop the application
        if (glIsApplnActive) {
            domDupStopApplication();
        }

        if (eventType == CY_U3P_USB_EVENT_DISCONNECT) {
            CyU3PDebugPrint (8, "CY_U3P_USB_EVENT_DISCONNECT detected\r\n");
        }

        if (eventType == CY_U3P_USB_EVENT_RESET) {
			CyU3PDebugPrint (8, "CY_U3P_USB_EVENT_DISCONNECT detected\r\n");
		}
        break;

    default:
    	// Unknown/unhandled USB event received
    	CyU3PDebugPrint (8, "UNKNOWN USB event detected\r\n");
        break;
    }
}

// Callback function to handle LPM requests (unused, always returns true)
CyBool_t domDupLPMRequestCB(CyU3PUsbLinkPowerMode linkMode)
{
    return CyTrue;
}
