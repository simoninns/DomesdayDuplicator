/************************************************************************

	domesdayDuplicator.h

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

#ifndef _DOMESDAYDUPLICATOR_H_
#define _DOMESDAYDUPLICATOR_H_

#include "cyu3externcstart.h"
#include "cyu3usb.h"
#include "cyu3types.h"
#include "cyu3usbconst.h"
#include "cyu3gpif.h"

#define CY_FX_GPIFTOUSB_THREAD_STACK       (0x1000) // Application thread stack size
#define CY_FX_GPIFTOUSB_THREAD_PRIORITY    (8) 		// Application thread priority

// End-point and socket definitions
#define CY_FX_EP_CONSUMER               0x81
#define CY_FX_EP_CONSUMER_SOCKET        CY_U3P_UIB_SOCKET_CONS_1
#define CY_FX_EP_PRODUCER_SOCKET		CY_U3P_PIB_SOCKET_0

// Set USB 3 burst length to 16Kbytes
#define CY_FX_EP_BURST_LENGTH           (16)

// Set the DMA buffer size to 16Kbytes for the application
#define CY_FX_DMA_BUF_SIZE              (16384)

// Set the total number of DMA buffers available to 4 (64Kbytes total)
#define CY_FX_DMA_BUF_COUNT             (4)

// Function prototypes
void domDupThreadInitialise(uint32_t input);
void CyFxApplicationDefine(void);
void domDupInitialiseApplication(void);
void domDupStartApplication(void);
void domDupStopApplication(void);
void domDupErrorHandler(CyU3PReturnStatus_t apiReturnStatus);
void domDupDebugInit(void);

// Callback function prototypes
void GpifDmaEventCB(CyU3PGpifEventType Event, uint8_t State);
CyBool_t domDupUSBSetupCB(uint32_t setupData0, uint32_t setupData1);
void domDupUSBEventCB(CyU3PUsbEventType_t eventType, uint16_t eventData);
CyBool_t domDupLPMRequestCB(CyU3PUsbLinkPowerMode linkMode);

// External definitions for the USB Descriptors
extern const uint8_t USB20DeviceDscr[];
extern const uint8_t USB30DeviceDscr[];
extern const uint8_t USBDeviceQualDscr[];
extern const uint8_t USBFSConfigDscr[];
extern const uint8_t USBHSConfigDscr[];
extern const uint8_t USBBOSDscr[];
extern const uint8_t USBSSConfigDscr[];
extern const uint8_t USBStringLangIDDscr[];
extern const uint8_t USBManufactureDscr[];
extern const uint8_t USBProductDscr[];

#include <cyu3externcend.h>

#endif // _DOMESDAYDUPLICATOR_H_
