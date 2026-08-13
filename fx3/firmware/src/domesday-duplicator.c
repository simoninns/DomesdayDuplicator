/************************************************************************

    domesdayDuplicator.c

    FX3 Firmware main functions
    DomesdayDuplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2018-2025 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

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
#include "domesday-duplicator.h"
#include "domesday-duplicator-gpif.h"

// Global definitions
CyU3PThread glAppThread; // Application thread structure
CyU3PDmaMultiChannel glDmaMultiChHandle; // DMA multi-channel handle

CyBool_t glIsApplnActive = CyFalse; // Application active/ready flag
CyBool_t glForceLinkU2 = CyFalse; // Force U2 flag

// Set once the USB descriptors are registered and the initial connection has been made.
// VBus events can arrive as soon as the event callback is registered, which is before the
// descriptors exist; acting on one then would connect the device with nothing to enumerate.
CyBool_t glUsbInitComplete = CyFalse;

volatile CyBool_t input0Flag = CyFalse; // Input 0 set flag
volatile CyBool_t input1Flag = CyFalse; // Input 1 set flag
volatile CyBool_t input2Flag = CyFalse; // Input 2 set flag
volatile CyBool_t input3Flag = CyFalse; // Input 3 set flag

CyBool_t input0HandledFlag = CyFalse; // Input 0 set condition handled flag
CyBool_t input1HandledFlag = CyFalse; // Input 1 set condition handled flag
CyBool_t input2HandledFlag = CyFalse; // Input 2 set condition handled flag
CyBool_t input3HandledFlag = CyFalse; // Input 3 set condition handled flag

volatile CyBool_t dataCollectionFlag = CyFalse; // Flag to show if the host application is collecting data

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

    // Note:
    // If io_cfg.isDQ32Bit = CyFalse then GPIO[0:15] and CTL[0:4] will be reserved for GPIF
    io_cfg.gpioSimpleEn[0] = 0;	// Most significant GPIOs 32-63
    io_cfg.gpioSimpleEn[1] = 0; // Least significant GPIOs 0-31
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

	// Initialise the GPIO and set the callback function (for GPIO interrupts)
	status = CyU3PGpioInit(&gpioClock, gpioInterruptCallback);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Claim GPIO19 from the GPIF Interface (collectData signal)
	status = CyU3PDeviceGpioOverride(19, CyTrue);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Put the FPGA in not collectData by driving GPIO19 low
	CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.outValue = CyFalse;
	gpioConfig.driveLowEn = CyTrue;
	gpioConfig.driveHighEn = CyTrue;
	status = CyU3PGpioSetSimpleConfig(19, &gpioConfig);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Claim GPIO27 from the GPIF Interface (nRESET signal)
	status = CyU3PDeviceGpioOverride(27, CyTrue);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Bring the FPGA out of reset by driving nRESET/GPIO27 high
	CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.outValue = CyTrue;
	gpioConfig.driveLowEn = CyTrue;
	gpioConfig.driveHighEn = CyTrue;
	status = CyU3PGpioSetSimpleConfig(27, &gpioConfig);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Generic input signals from FPGA (GPIO 20, 21, 28 and 29) -------------------------------------------------------

	// Claim GPIO20 from the GPIF Interface (input0)
	status = CyU3PDeviceGpioOverride(20, CyTrue);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Configure as input
	CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.outValue = CyTrue;
	gpioConfig.driveLowEn = CyFalse;
	gpioConfig.driveHighEn = CyFalse;
	gpioConfig.inputEn = CyTrue;
	gpioConfig.intrMode = CY_U3P_GPIO_INTR_POS_EDGE;

	status = CyU3PGpioSetSimpleConfig(20, &gpioConfig);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Claim GPIO21 from the GPIF Interface (input1)
	status = CyU3PDeviceGpioOverride(21, CyTrue);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Configure as input
	CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.outValue = CyTrue;
	gpioConfig.driveLowEn = CyFalse;
	gpioConfig.driveHighEn = CyFalse;
	gpioConfig.inputEn = CyTrue;
	gpioConfig.intrMode = CY_U3P_GPIO_INTR_POS_EDGE;

	status = CyU3PGpioSetSimpleConfig(21, &gpioConfig);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Claim GPIO28 from the GPIF Interface (input2)
	status = CyU3PDeviceGpioOverride(28, CyTrue);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Configure as input
	CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.outValue = CyTrue;
	gpioConfig.driveLowEn = CyFalse;
	gpioConfig.driveHighEn = CyFalse;
	gpioConfig.inputEn = CyTrue;
	gpioConfig.intrMode = CY_U3P_GPIO_INTR_POS_EDGE;

	status = CyU3PGpioSetSimpleConfig(28, &gpioConfig);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Claim GPIO29 from the GPIF Interface (input3)
	status = CyU3PDeviceGpioOverride(29, CyTrue);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Configure as input
	CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.outValue = CyTrue;
	gpioConfig.driveLowEn = CyFalse;
	gpioConfig.driveHighEn = CyFalse;
	gpioConfig.inputEn = CyTrue;
	gpioConfig.intrMode = CY_U3P_GPIO_INTR_POS_EDGE;

	status = CyU3PGpioSetSimpleConfig(29, &gpioConfig);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Generic output signals to FPGA (GPIO 22 (early), 23 (delayed), -------------------------------------------------
	// 24 (delayed), 25 (delayed), 26 (delayed))

	// Claim GPIO22 from the GPIF Interface (outputE0)
	status = CyU3PDeviceGpioOverride(22, CyTrue);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Drive pin low
	CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.outValue = CyFalse;
	gpioConfig.driveLowEn = CyTrue;
	gpioConfig.driveHighEn = CyTrue;
	status = CyU3PGpioSetSimpleConfig(22, &gpioConfig);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Claim GPIO23 from the GPIF Interface (outputD0)
	status = CyU3PDeviceGpioOverride(23, CyTrue);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Drive pin low
	CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.outValue = CyFalse;
	gpioConfig.driveLowEn = CyTrue;
	gpioConfig.driveHighEn = CyTrue;
	status = CyU3PGpioSetSimpleConfig(23, &gpioConfig);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Claim GPIO24 from the GPIF Interface (outputD1)
	status = CyU3PDeviceGpioOverride(24, CyTrue);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Drive pin low
	CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.outValue = CyFalse;
	gpioConfig.driveLowEn = CyTrue;
	gpioConfig.driveHighEn = CyTrue;
	status = CyU3PGpioSetSimpleConfig(24, &gpioConfig);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Claim GPIO25 from the GPIF Interface (outputD2)
	status = CyU3PDeviceGpioOverride(25, CyTrue);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Drive pin low
	CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.outValue = CyFalse;
	gpioConfig.driveLowEn = CyTrue;
	gpioConfig.driveHighEn = CyTrue;
	status = CyU3PGpioSetSimpleConfig(25, &gpioConfig);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Claim GPIO26 from the GPIF Interface (outputD3)
	status = CyU3PDeviceGpioOverride(26, CyTrue);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

	// Drive pin low
	CyU3PMemSet((uint8_t *)&gpioConfig, 0, sizeof(gpioConfig));
	gpioConfig.outValue = CyFalse;
	gpioConfig.driveLowEn = CyTrue;
	gpioConfig.driveHighEn = CyTrue;
	status = CyU3PGpioSetSimpleConfig(26, &gpioConfig);
	if (status != CY_U3P_SUCCESS) {
		goto handleFatalError;
	}

    // Initialise the RTOS kernel -------------------------------------------------------------------------------------
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
    uint32_t linkRecoveryAttempts;

    // Initialise the debug console
    domDupDebugInit();
    CyU3PDebugPrint(1, "\r\nDomesday Duplicator FX3 Firmware - Build 0062\r\n");
    CyU3PDebugPrint(1, "(c)2018 Simon Inns - https://www.domesday86.com\r\n\r\n");
    CyU3PDebugPrint(1, "domDupThreadInitialise(): Debug console initialised\r\n");

    // Initialise the application
    domDupInitialiseApplication();

    // Main application thread loop
    while(1) {
        if (glForceLinkU2) {
            // The host has placed the function in suspend via SET_FEATURE(FUNCTION_SUSPEND),
            // so hold the USB 3.0 link in U2 for as long as that condition lasts.
        	status = CyU3PUsbGetLinkPowerState(&powerState);
            while ((glForceLinkU2) && (status == CY_U3P_SUCCESS) && (powerState == CyU3PUsbLPM_U0)) {
                // Try to get to U2 state
                CyU3PUsbSetLinkPowerState(CyU3PUsbLPM_U2);
                CyU3PThreadSleep(5);
                status = CyU3PUsbGetLinkPowerState(&powerState);
            }
        } else if (glIsApplnActive && (CyU3PUsbGetSpeed() == CY_U3P_SUPER_SPEED)) {
            // Recover the link to U0 if the host has left it in U1 or U2 while the capture
            // path is up. The DMA hardware normally does this by itself when it has a packet
            // to send, so this is a backstop rather than the primary mechanism.
            //
            // U3 is deliberately excluded, and that exclusion is the point of this block.
            // U3 is host-directed suspend, and the only way a device may leave U3 is by
            // signalling remote wakeup - so firmware that pulls the link out of U3 wakes the
            // host up again the instant it tries to sleep. Requesting U0 from U1/U2 is
            // ordinary link management; requesting it from U3 is not.
            //
            // The attempt count bounds the loop. Without it, a link that will not leave U1 -
            // during link training, or on a marginal cable - spins here forever and the
            // thread never yields.
        	status = CyU3PUsbGetLinkPowerState(&powerState);
        	linkRecoveryAttempts = 0;
            while ((status == CY_U3P_SUCCESS) && (linkRecoveryAttempts < 8) &&
                ((powerState == CyU3PUsbLPM_U1) || (powerState == CyU3PUsbLPM_U2))) {
                CyU3PUsbSetLinkPowerState(CyU3PUsbLPM_U0);
                CyU3PThreadSleep(1);
                status = CyU3PUsbGetLinkPowerState(&powerState);
                linkRecoveryAttempts++;
            }
        }

        // Process the input0 flag (generated via GPIO interrupt)
        if (input0Flag) {
        	// Ensure we only output the debug once
        	if (!input0HandledFlag) {
        		input0HandledFlag = CyTrue;
        		CyU3PDebugPrint(4, "Main application loop: input0 pin set by the FPGA\r\n");
        	}
        }

        // Process the input1 flag (generated via GPIO interrupt)
		if (input1Flag) {
			// Ensure we only output the debug once
			if (!input1HandledFlag) {
				input1HandledFlag = CyTrue;
				CyU3PDebugPrint(4, "Main application loop: input1 pin set by the FPGA\r\n");
			}
		}

		// Process the input2 flag (generated via GPIO interrupt)
		if (input2Flag) {
			// Ensure we only output the debug once
			if (!input2HandledFlag) {
				input2HandledFlag = CyTrue;
				CyU3PDebugPrint(4, "Main application loop: input2 pin set by the FPGA\r\n");
			}
		}

		// Process the input3 flag (generated via GPIO interrupt)
		if (input3Flag) {
			// Ensure we only output the debug once
			if (!input3HandledFlag) {
				input3HandledFlag = CyTrue;
				CyU3PDebugPrint(4, "Main application loop: input3 pin set by the FPGA\r\n");
			}
		}

		// Yield the CPU.
		//
		// Nothing in this loop is latency critical: the GPIF to USB data path is carried
		// entirely by the DMA hardware and never passes through this thread. Without a sleep
		// the loop spins at 100% CPU whenever the link is at U0, which is almost always, and
		// starves the debug console and the USB driver's own housekeeping threads.
		CyU3PThreadSleep(10);
    }
}

// Function to create the initial application thread
void CyFxApplicationDefine(void)
{
    void *ptr = NULL;
    uint32_t returnCode = CY_U3P_SUCCESS;

    // Allocate the memory for the threads
    ptr = CyU3PMemAlloc(CY_FX_GPIFTOUSB_THREAD_STACK);
    if (ptr == NULL) {
    	// Could not allocate the thread stack
    	// Application cannot start
    	while(1);
    }

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
        CyU3PDebugPrint(4, "domDupInitialiseApplication(): CyU3PUsbStart failed, Error code = %d\r\n", apiReturnStatus);
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
        CyU3PDebugPrint(4, "domDupInitialiseApplication(): CyU3PUsbSetDesc USB3 failed, Error code = %d\r\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // High speed device descriptor (USB 2)
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_DEVICE_DESCR, 0, (uint8_t *)USB20DeviceDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "domDupInitialiseApplication(): CyU3PUsbSetDesc USB 2 failed, Error code = %d\r\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // BOS descriptor
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_BOS_DESCR, 0, (uint8_t *)USBBOSDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "domDupInitialiseApplication(): CyU3PUsbSetDesc BOS failed, Error code = %d\r\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // Device qualifier descriptor
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_DEVQUAL_DESCR, 0, (uint8_t *)USBDeviceQualDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "domDupInitialiseApplication(): CyU3PUsbSetDesc qualifier descriptor failed, Error code = %d\r\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // Super speed configuration descriptor
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_CONFIG_DESCR, 0, (uint8_t *)USBSSConfigDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "domDupInitialiseApplication(): CyU3PUsbSetDesc configuration descriptor failed, Error code = %d\r\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // High speed configuration descriptor
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_CONFIG_DESCR, 0, (uint8_t *)USBHSConfigDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "domDupInitialiseApplication(): CyU3PUsbSetDesc Other Speed Descriptor failed, Error Code = %d\r\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // Full speed configuration descriptor
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_FS_CONFIG_DESCR, 0, (uint8_t *)USBFSConfigDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "domDupInitialiseApplication(): CyU3PUsbSetDesc Full-Speed Descriptor failed, Error Code = %d\r\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // String descriptor 0
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 0, (uint8_t *)USBStringLangIDDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "domDupInitialiseApplication(): CyU3PUsbSetDesc string 0 descriptor failed, Error code = %d\r\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // String descriptor 1
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 1, (uint8_t *)USBManufactureDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "domDupInitialiseApplication(): CyU3PUsbSetDesc string descriptor 1 failed, Error code = %d\r\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // String descriptor 2
    apiReturnStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 2, (uint8_t *)USBProductDscr);
    if (apiReturnStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint(4, "domDupInitialiseApplication(): CyU3PUsbSetDesc string descriptor 2 failed, Error code = %d\r\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // Show status in debug console
    CyU3PDebugPrint(4, "domDupInitialiseApplication(): Initialisation successful; Connecting to host\r\n");

    // Connect to the host
    if (!noRenum) {
        apiReturnStatus = CyU3PConnectState(CyTrue, CyTrue);
        if (apiReturnStatus != CY_U3P_SUCCESS) {
            CyU3PDebugPrint(4, "domDupInitialiseApplication(): CyU3PConnectState failed, Error code = %d\r\n", apiReturnStatus);
            domDupErrorHandler(apiReturnStatus);
        }
    } else {
    	// If application is already active.  Restart the application
        if (glIsApplnActive) domDupStopApplication();

        // Start the application
        domDupStartApplication();
    }

    // VBus events may now be acted on - the descriptors are registered and the device has
    // been presented to the host, so a disconnect/reconnect cycle has something to enumerate.
    glUsbInitComplete = CyTrue;

    CyU3PDebugPrint(8, "domDupInitialiseApplication(): Application initialisation complete.\r\n");
}

// Clear the FPGA input condition flags, and the "already reported" flags that go with them
void domDupClearInputFlags(void)
{
    input0Flag = CyFalse;
    input1Flag = CyFalse;
    input2Flag = CyFalse;
    input3Flag = CyFalse;
    input0HandledFlag = CyFalse;
    input1HandledFlag = CyFalse;
    input2HandledFlag = CyFalse;
    input3HandledFlag = CyFalse;
}

// Function to start application once SET_CONF received from host
void domDupStartApplication(void)
{
    uint16_t size = 0;
    CyU3PEpConfig_t epCfg;
    CyU3PDmaMultiChannelConfig_t dmaMultiConfig;
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
        size = 0;
        break;
    }

    // Check that we are connected to a USB 3 host.
    //
    // The capture path needs SuperSpeed - 40 MSa/s of 16-bit samples is far beyond what a
    // 2.0 link can carry - so it is not started on a 2.0 connection. It is important that
    // this is not treated as a fatal error, though: the device stays enumerated, keeps
    // answering control requests and can be brought up properly once the port re-trains at
    // SuperSpeed. Calling domDupErrorHandler() here, as this used to, left the firmware in
    // an endless loop that only unplugging the device could clear - and hosts do bring a
    // port up at high speed first, particularly when resuming from hibernate.
    if (usbSpeed != CY_U3P_SUPER_SPEED) {
    	CyU3PDebugPrint(4, "domDupStartApplication(): SuperSpeed link not available (speed = %d); capture path not started\r\n", usbSpeed);
    	return;
    }

    // Start from a known state: the FPGA is not collecting and no input conditions are pending
    CyU3PGpioSetValue(19, CyFalse);
    dataCollectionFlag = CyFalse;
    domDupClearInputFlags();

    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyTrue;
    epCfg.epType = CY_U3P_USB_EP_BULK;
    epCfg.burstLen = (usbSpeed == CY_U3P_SUPER_SPEED) ? (CY_FX_EP_BURST_LENGTH) : 1;
    epCfg.streams = 0;
    epCfg.pcktSize = size;

    // Configure consumer end-point
    apiReturnStatus = CyU3PSetEpConfig(CY_FX_EP_CONSUMER, &epCfg);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "domDupStartApplication(): CyU3PSetEpConfig failed, Error code = %d\r\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // Flush the end-point
    CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);

    // Create a DMA manual multi-channel for the GPIF to USB transfer
    CyU3PMemSet ((uint8_t *)&dmaMultiConfig, 0, sizeof (dmaMultiConfig));
    dmaMultiConfig.size  = CY_FX_DMA_BUF_SIZE;
    dmaMultiConfig.count = CY_FX_DMA_BUF_COUNT;
    dmaMultiConfig.validSckCount = 2;
    dmaMultiConfig.prodSckId[0] = CY_FX_EP_PRODUCER_SOCKET0;
    dmaMultiConfig.prodSckId[1] = CY_FX_EP_PRODUCER_SOCKET1;
    dmaMultiConfig.consSckId[0] = CY_FX_EP_CONSUMER_SOCKET;
    dmaMultiConfig.dmaMode = CY_U3P_DMA_MODE_BYTE;

    apiReturnStatus = CyU3PDmaMultiChannelCreate(&glDmaMultiChHandle, CY_U3P_DMA_TYPE_AUTO_MANY_TO_ONE, &dmaMultiConfig);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "domDupStartApplication(): CyU3PDmaMultiChannelCreate failed, Error code = %d\r\n", apiReturnStatus);
        domDupErrorHandler(apiReturnStatus);
    }

    // Start the DMA channel transfer
    apiReturnStatus = CyU3PDmaMultiChannelSetXfer(&glDmaMultiChHandle, 0, 0);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4, "domDupStartApplication(): CyU3PDmaMultiChannelSetXfer failed, Error code = %d\r\n", apiReturnStatus);
		domDupErrorHandler(apiReturnStatus);
	}

    // Load the GPIF state machine
    apiReturnStatus = CyU3PGpifLoad (&CyFxGpifConfig);

    // Register callback for GPIF CPU interrupt events
    CyU3PGpifRegisterCallback(gpifDmaEventCB);

    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "domDupStartApplication(): CyU3PGpifLoad failed, error code = %d\r\n", apiReturnStatus);
        domDupErrorHandler (apiReturnStatus);
    }

    // Water-mark value = 3, bus width = 16
    // Therefore, the number of 16-bit data words that may be written after the clock edge at which the partial
    // flag is sampled asserted = (3 x (32/16)) - 4 = 2

    // Set the thread 0 water-mark level to 1x 32 bit word
    apiReturnStatus = CyU3PGpifSocketConfigure(0, CY_FX_EP_PRODUCER_SOCKET0, 3, CyFalse, 1);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4, "domDupStartApplication(): CyU3PGpifSocketConfigure failed for thread0, error code = %d\r\n", apiReturnStatus);
		domDupErrorHandler (apiReturnStatus);
	}

    // Set the thread 1 water-mark level to 1x 32 bit word
	apiReturnStatus = CyU3PGpifSocketConfigure(1, CY_FX_EP_PRODUCER_SOCKET1, 3, CyFalse, 1);
	if (apiReturnStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4, "domDupStartApplication(): CyU3PGpifSocketConfigure failed for thread1, error code = %d\r\n", apiReturnStatus);
		domDupErrorHandler (apiReturnStatus);
	}

	// Start the GPIF state machine
    apiReturnStatus = CyU3PGpifSMStart (START, ALPHA_START);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "domDupStartApplication(): CyU3PGpifSMStart failed, error code = %d\r\n", apiReturnStatus);
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

    // Tell the FPGA to stop collecting before anything is torn down. Otherwise it keeps
    // streaming into an FX3 whose DMA channel is about to be destroyed.
    CyU3PGpioSetValue(19, CyFalse);
    dataCollectionFlag = CyFalse;
    domDupClearInputFlags();

    // U1/U2 entry is only suppressed for the duration of a capture, so restore the driver's
    // own handling of it here. The SDK is explicit that LPM must be re-enabled after every
    // CyU3PUsbLPMDisable(), or the device fails USB compliance testing.
    CyU3PUsbLPMEnable();

    // Disable the GPIF state-machine. CyTrue discards the loaded waveform, which
    // domDupStartApplication() restores with CyU3PGpifLoad().
    CyU3PGpifDisable(CyTrue);

    // The PIB block is deliberately left running.
    //
    // It is initialised once, in main(), and the FPGA control signals are GPIO overrides
    // taken from the GPIF interface on top of it. De-initialising it here powered the block
    // down for the rest of the firmware's life, because nothing ever initialised it again -
    // so the next domDupStartApplication() programmed the GPIF with its clocks stopped and
    // dropped into domDupErrorHandler()'s endless loop, leaving a device that answers
    // nothing until it is unplugged. Every stop/start cycle reaches this: a bus reset, a
    // SET_CONFIGURATION, or a resume from host sleep.

    // Destroy DMA channels
    CyU3PDmaMultiChannelDestroy(&glDmaMultiChHandle);

    // Flush end-points
    CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);

    // Disable end-points
    CyU3PMemSet((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyFalse;

    // Un-configure consumer end-point
    apiReturnStatus = CyU3PSetEpConfig(CY_FX_EP_CONSUMER, &epCfg);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "domDupStopApplication(): CyU3PSetEpConfig failed, Error code = %d\r\n", apiReturnStatus);
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
    CyU3PMemSet((uint8_t *)&uartConfig, 0, sizeof (uartConfig));
    uartConfig.baudRate = CY_U3P_UART_BAUDRATE_115200;
    uartConfig.stopBit = CY_U3P_UART_ONE_STOP_BIT;
    uartConfig.parity = CY_U3P_UART_NO_PARITY;
    uartConfig.txEnable = CyTrue;
    uartConfig.rxEnable = CyFalse;
    uartConfig.flowCtrl = CyFalse;
    uartConfig.isDma = CyTrue;

    apiReturnStatus = CyU3PUartSetConfig(&uartConfig, NULL);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        domDupErrorHandler(apiReturnStatus);
    }

    // Set the UART transfer to a large number
    apiReturnStatus = CyU3PUartTxSetBlockXfer(0xFFFFFFFF);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        domDupErrorHandler(apiReturnStatus);
    }

    // Initialise debug on the UART
    apiReturnStatus = CyU3PDebugInit(CY_U3P_LPP_SOCKET_UART_CONS, 8);
    if (apiReturnStatus != CY_U3P_SUCCESS) {
        domDupErrorHandler(apiReturnStatus);
    }

    CyU3PDebugPreamble(CyFalse);
}

// Call back functions ----------------------------------------------------------------------------------

// Handle CPU_INT from GPIF callback (set when the FPGA FIFO buffer is full)
void gpifDmaEventCB(CyU3PGpifEventType Event, uint8_t State)
{
	if (Event == CYU3P_GPIF_EVT_SM_INTERRUPT) CyU3PDebugPrint(8, "gpifDmaEventCB(): Unhandled INT_CPU signal received from GPIF\r\n");
}

// USB set-up request callback
CyBool_t domDupUSBSetupCB(uint32_t setupData0, uint32_t setupData1)
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
					CyU3PDebugPrint(8, "domDupUSBSetupCB(): Vendor specific command received: START data collection\r\n");
					CyU3PGpioSetValue(19, CyTrue); // collectData GPIO high

					// Clear the input flags
					domDupClearInputFlags();

					// Flag that the host is collecting data
					dataCollectionFlag = CyTrue;

					// Keep the link out of U1/U2 for the duration of the capture. U2 exit
					// latency alone is up to 2ms (see bU2DevExitLat in the BOS descriptor),
					// which is long enough for the FPGA's FIFO to overflow. This also
					// suppresses LPM-L1 on a 2.0 link. It is undone on stop, and on every
					// reset and disconnect, which the SDK requires for USB compliance.
					CyU3PUsbLPMDisable();
				}

				if (wValue == 0) {
					// Stop collection request from USB host
					CyU3PDebugPrint(8, "domDupUSBSetupCB(): Vendor specific command received: STOP data collection\r\n");
					CyU3PGpioSetValue(19, CyFalse); // collectData GPIO low

					// Flag that the host is not collecting data
					dataCollectionFlag = CyFalse;

					// Clear the input flags
					domDupClearInputFlags();

					// Hand U1/U2 handling back to the USB driver now that the link no
					// longer has to sustain a capture
					CyU3PUsbLPMEnable();
				}

				isHandled = CyTrue;
			}

			// Handle vendor request for configuration 0xB6
			//
			// The passed wValue is interpreted as a bit flag and causes
			// GPIOs 22 to 26 to be set according to bits 0-4 (bits 5 to 7
			// are ignored).
			if (bRequest == 0xB6) {
				// Check bit 0 (GPIO 22)
				if ((wValue & 0x01) != 0) {
					CyU3PDebugPrint(8, "domDupUSBSetupCB(): Command 0xB6: Bit 0 = GPIO22 High\r\n");
					CyU3PGpioSetValue(22, CyTrue); // GPIO high
				} else {
					CyU3PDebugPrint(8, "domDupUSBSetupCB(): Command 0xB6: Bit 0 = GPIO22 Low\r\n");
					CyU3PGpioSetValue(22, CyFalse); // GPIO Low
				}

				// Check bit 1 (GPIO 23)
				if ((wValue & 0x02) != 0) {
					CyU3PDebugPrint(8, "domDupUSBSetupCB(): Command 0xB6: Bit 1 = GPIO23 High\r\n");
					CyU3PGpioSetValue(23, CyTrue); // GPIO high
				} else {
					CyU3PDebugPrint(8, "domDupUSBSetupCB(): Command 0xB6: Bit 1 = GPIO23 Low\r\n");
					CyU3PGpioSetValue(23, CyFalse); // GPIO Low
				}

				// Check bit 2 (GPIO 24)
				if ((wValue & 0x04) != 0) {
					CyU3PDebugPrint(8, "domDupUSBSetupCB(): Command 0xB6: Bit 2 = GPIO24 High\r\n");
					CyU3PGpioSetValue(24, CyTrue); // GPIO high
				} else {
					CyU3PDebugPrint(8, "domDupUSBSetupCB(): Command 0xB6: Bit 2 = GPIO24 Low\r\n");
					CyU3PGpioSetValue(24, CyFalse); // GPIO Low
				}

				// Check bit 3 (GPIO 25)
				if ((wValue & 0x08) != 0) {
					CyU3PDebugPrint(8, "domDupUSBSetupCB(): Command 0xB6: Bit 3 = GPIO25 High\r\n");
					CyU3PGpioSetValue(25, CyTrue); // GPIO high
				} else {
					CyU3PDebugPrint(8, "domDupUSBSetupCB(): Command 0xB6: Bit 3 = GPIO25 Low\r\n");
					CyU3PGpioSetValue(25, CyFalse); // GPIO Low
				}

				// Check bit 4 (GPIO 26)
				if ((wValue & 0x10) != 0) {
					CyU3PDebugPrint(8, "domDupUSBSetupCB(): Command 0xB6: Bit 4 = GPIO26 High\r\n");
					CyU3PGpioSetValue(26, CyTrue); // GPIO high
				} else {
					CyU3PDebugPrint(8, "domDupUSBSetupCB(): Command 0xB6: Bit 4 = GPIO26 Low\r\n");
					CyU3PGpioSetValue(26, CyFalse); // GPIO Low
				}

				isHandled = CyTrue;
			}

			// ACK the request.
			//
			// Only requests this firmware actually implements are acknowledged. Anything
			// else leaves isHandled false, and the driver stalls endpoint 0 - which is how
			// a device is required to tell a host that a request is unsupported. ACKing
			// every vendor request, as this used to, reports success for commands that were
			// silently discarded.
			if (isHandled) CyU3PUsbAckSetup();
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
                    CyU3PDmaMultiChannelReset(&glDmaMultiChHandle);
                    CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);
                    CyU3PUsbResetEp(CY_FX_EP_CONSUMER);
                    CyU3PDmaMultiChannelSetXfer(&glDmaMultiChHandle, 0, 0);
                    CyU3PUsbStall(wIndex, CyFalse, CyTrue);
                    isHandled = CyTrue;
                    CyU3PUsbAckSetup();
                }
            }
        }
    }

    return isHandled;
}

// Callback function to handle USB events
void domDupUSBEventCB(CyU3PUsbEventType_t eventType, uint16_t eventData)
{
    // Note: the USB driver delivers these events one at a time on a single thread, so no two
    // cases here can overlap. That is what makes it safe for both SETCONF and RESUME to tear
    // the capture path down and rebuild it.
    switch (eventType) {
    case CY_U3P_USB_EVENT_CONNECT:
    	// Restore the driver's own U1/U2 handling. A CyU3PUsbLPMDisable() left over from a
    	// capture that ended in a disconnect would otherwise persist across the new
    	// connection, and a device that never accepts U1/U2 fails USB compliance.
    	CyU3PUsbLPMEnable();
		CyU3PDebugPrint(8, "domDupUSBEventCB(): CY_U3P_USB_EVENT_CONNECT received - USB %d.0 connection\r\n",
			(eventData == 1) ? 3 : 2);
		break;

    case CY_U3P_USB_EVENT_SETCONF:
    	CyU3PDebugPrint(8, "domDupUSBEventCB(): CY_U3P_USB_EVENT_SETCONF received - configuration %d\r\n", eventData);

    	// If the application is already active, stop it
        if (glIsApplnActive) {
            domDupStopApplication();
        }

        // Configuration 0 is the host un-configuring the device, not selecting a
        // configuration. Stopping is the whole of the correct response; starting the capture
        // path again would leave endpoints enabled on a device the host considers idle.
        if (eventData != 0) {
            domDupStartApplication();
        }
        break;

    case CY_U3P_USB_EVENT_SUSPEND:
        // The host is suspending the bus - typically because the machine is going to sleep.
        // Quiesce the FPGA and park the data path, but stay enumerated: the link is in U3 and
        // it is the host's job, not ours, to bring it back out.
        CyU3PDebugPrint(8, "domDupUSBEventCB(): CY_U3P_USB_EVENT_SUSPEND received - quiescing capture path\r\n");

        CyU3PGpioSetValue(19, CyFalse); // collectData GPIO low
        dataCollectionFlag = CyFalse;
        domDupClearInputFlags();

        // Anything still buffered belongs to a transfer the host has abandoned, so discard it
        // rather than delivering stale samples once the link comes back.
        if (glIsApplnActive) {
            CyU3PDmaMultiChannelReset(&glDmaMultiChHandle);
            CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);
        }
        break;

    case CY_U3P_USB_EVENT_RESUME:
        CyU3PDebugPrint(8, "domDupUSBEventCB(): CY_U3P_USB_EVENT_RESUME received - rebuilding capture path\r\n");

        // Rebuild rather than resume.
        //
        // CyU3PDmaMultiChannelReset() leaves the channel in reset until
        // CyU3PDmaMultiChannelSetXfer() is called again, and the GPIF state machine has been
        // running into it unattended for however long the host was asleep. Without this the
        // device enumerates and answers control requests after a resume but never delivers
        // another sample, which looks exactly like a device that has silently died.
        //
        // A resume does not necessarily bring a SET_CONFIGURATION with it, so this cannot be
        // left to the SETCONF case.
        if (glIsApplnActive) {
            domDupStopApplication();
            domDupStartApplication();
        }
        break;

    case CY_U3P_USB_EVENT_RESET:
    case CY_U3P_USB_EVENT_DISCONNECT:
        glForceLinkU2 = CyFalse;

        // Stop the application
        if (glIsApplnActive) {
            domDupStopApplication();
        }

        if (eventType == CY_U3P_USB_EVENT_DISCONNECT) {
            CyU3PDebugPrint(8, "domDupUSBEventCB(): CY_U3P_USB_EVENT_DISCONNECT received - Application stopped\r\n");
        }

        if (eventType == CY_U3P_USB_EVENT_RESET) {
        	// The SDK requires LPM handling to be restored on every reset
        	CyU3PUsbLPMEnable();
			CyU3PDebugPrint(8, "domDupUSBEventCB(): CY_U3P_USB_EVENT_RESET received - Application stopped\r\n");
		}
        break;

    case CY_U3P_USB_EVENT_VBUS_REMOVED:
        // The host has cut port power. That is what hibernating to S4 looks like from here,
        // and also a powered-down port or a pulled cable. Take the connection down explicitly
        // so the USB PHY is not left advertising a device on a dead bus, and wait for VBus to
        // come back before presenting the device again.
        //
        // Ignored until initialisation has finished, because VBus can already be valid when
        // the event callback is registered - which is before any descriptor exists.
        if (glUsbInitComplete) {
            CyU3PDebugPrint(8, "domDupUSBEventCB(): CY_U3P_USB_EVENT_VBUS_REMOVED received - disconnecting\r\n");
            if (glIsApplnActive) {
                domDupStopApplication();
            }
            CyU3PConnectState(CyFalse, CyTrue);
        }
        break;

    case CY_U3P_USB_EVENT_VBUS_VALID:
        // Port power is back. Re-present the device so it enumerates cleanly, rather than
        // relying on the host to reset a connection that was torn down under it.
        if (glUsbInitComplete) {
            CyU3PDebugPrint(8, "domDupUSBEventCB(): CY_U3P_USB_EVENT_VBUS_VALID received - reconnecting\r\n");
            CyU3PConnectState(CyTrue, CyTrue);
        }
        break;

    case CY_U3P_USB_EVENT_EP_UNDERRUN:
        // The endpoint ran dry part way through a burst, so the host was given less data than
        // the transfer promised. Nothing can be done about it after the fact, but it is the
        // one event that means a capture is no longer sample accurate, so it is always
        // reported - at a level that is on by default.
        CyU3PDebugPrint(4, "domDupUSBEventCB(): CY_U3P_USB_EVENT_EP_UNDERRUN on endpoint 0x%x - capture data lost\r\n",
        	eventData);
        break;

    case CY_U3P_USB_EVENT_LNK_RECOVERY:
        // Deliberately silent. Unlike every other event here, this one is raised from
        // interrupt context, where CyU3PDebugPrint() - which blocks on a UART DMA transfer -
        // must not be called.
        break;

    case CY_U3P_USB_EVENT_SS_COMP_ENTRY:
    case CY_U3P_USB_EVENT_SS_COMP_EXIT:
    case CY_U3P_USB_EVENT_USB3_LNKFAIL:
    case CY_U3P_USB_EVENT_LMP_EXCH_FAIL:
        // USB 3.0 link health. None of these need action here - the driver retrains the link,
        // and falls back to USB 2.0 by itself when SuperSpeed training fails - but a capture
        // that drops samples for no visible reason is usually one of these, so record them.
        CyU3PDebugPrint(4, "domDupUSBEventCB(): USB 3 link event %d (data %d)\r\n", eventType, eventData);
        break;

    default:
        break;
    }
}

// Callback function to handle LPM requests
CyBool_t domDupLPMRequestCB(CyU3PUsbLinkPowerMode linkMode)
{
    // Called by the USB driver when the host asks the link to enter U1 or U2. U3 never
    // arrives here: U3 is host-directed suspend and is reported as CY_U3P_USB_EVENT_SUSPEND.
    //
    // This must do nothing but decide. It runs in the driver's own context and is called as
    // often as the host sends LGO_U1, which on an idle SuperSpeed link is thousands of times
    // a second - so in particular it must not call CyU3PDebugPrint(), which blocks on a UART
    // DMA transfer. A debug print here throttles the link and floods the console.
    //
    // The blanket rejection during a capture is a second line of defence behind
    // CyU3PUsbLPMDisable(), which the 0xB5 start command applies. Accepting U1 mid-capture is
    // harmless in itself; U2 is not, because its exit latency is up to 2ms.
    if (dataCollectionFlag && (linkMode >= CyU3PUsbLPM_U2)) {
        return CyFalse;
    }

    return CyTrue;
}

// Callback function for GPIO pin interrupt
//
// Note: This interrupt call is made in interrupt context, which means that any blocking
//       API calls cannot be made from this callback. i.e. don't try to output debug.
void gpioInterruptCallback(uint8_t gpioTriggerPin)
{
    CyBool_t gpioValue = CyFalse;
    CyU3PReturnStatus_t apiReturnStatus = CY_U3P_SUCCESS;

    // Get the status of the pin (that caused the interrupt)
    apiReturnStatus = CyU3PGpioGetValue(gpioTriggerPin, &gpioValue);
    if (apiReturnStatus == CY_U3P_SUCCESS) {
    	// Generic input signals from FPGA (GPIO 20, 21, 28 and 29)
        if (gpioTriggerPin == 20) {
        	if (gpioValue == CyTrue) {
        		if (dataCollectionFlag) input0Flag = CyTrue;
        	} else {
        		input0Flag = CyFalse;
        	}
        }

        if (gpioTriggerPin == 21) {
        	if (gpioValue == CyTrue) {
        		if (dataCollectionFlag) input1Flag = CyTrue;
        	} else {
        		input1Flag = CyFalse;
        	}
        }

        if (gpioTriggerPin == 28) {
        	if (gpioValue == CyTrue) {
        		if (dataCollectionFlag) input2Flag = CyTrue;
        	} else {
        		input2Flag = CyFalse;
        	}
        }

        if (gpioTriggerPin == 29) {
        	if (gpioValue == CyTrue) {
        		if (dataCollectionFlag) input3Flag = CyTrue;
        	} else {
        		input3Flag = CyFalse;
        	}
        }
    }
}
