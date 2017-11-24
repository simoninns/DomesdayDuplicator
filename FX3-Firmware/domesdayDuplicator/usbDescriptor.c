/************************************************************************

	usbDescriptor.c

	FX3 USB descriptor functions
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

#include "domesdayDuplicator.h"

// VID and PID definition (Domesday Duplicator 0x1D50 / 0x603B)
//#define VID_H	0x1D
//#define VID_L	0x50
//#define PID_H	0x60
//#define PID_L	0x3B

// VID and PID definition (Cypress FX3 default 0x04B4 / 0x00F1 - useful for testing)
#define VID_H	0x04
#define VID_L	0xB4
#define PID_H	0x00
#define PID_L	0xF1

// Standard device descriptor for USB 3.0
const uint8_t USB30DeviceDscr[] __attribute__ ((aligned (32))) = {
    0x12,                           // Descriptor size
    CY_U3P_USB_DEVICE_DESCR,        // Device descriptor type
    0x00,0x03,                      // USB 3.0
    0x00,                           // Device class
    0x00,                           // Device sub-class
    0x00,                           // Device protocol
    0x09,                           // Maximum packet size for EP0 : 2^9
    VID_L,VID_H,                    // Vendor ID
    PID_L,PID_H,                    // Product ID
    0x00,0x00,                      // Device release number
    0x01,                           // Manufacture string index
    0x02,                           // Product string index
    0x00,                           // Serial number string index
    0x01                            // Number of configurations
};

// Standard device descriptor for USB 2.0
const uint8_t USB20DeviceDscr[] __attribute__ ((aligned (32))) = {
    0x12,                           // Descriptor size
    CY_U3P_USB_DEVICE_DESCR,        // Device descriptor type
    0x10,0x02,                      // USB 2.10
    0x00,                           // Device class
    0x00,                           // Device sub-class
    0x00,                           // Device protocol
    0x40,                           // Maximum packet size for EP0 : 64 bytes
    VID_L,VID_H,                    // Vendor ID
    PID_L,PID_H,                    // Product ID
    0x00,0x00,                      // Device release number
    0x01,                           // Manufacture string index
    0x02,                           // Product string index
    0x00,                           // Serial number string index
    0x01                            // Number of configurations
};

// Binary device object store descriptor
const uint8_t USBBOSDscr[] __attribute__ ((aligned (32))) = {
    0x05,                           // Descriptor size
    CY_U3P_BOS_DESCR,               // Device descriptor type
    0x16,0x00,                      // Length of this descriptor and all sub descriptors
    0x02,                           // Number of device capability descriptors

    // USB 2.0 extension
    0x07,                           // Descriptor size
    CY_U3P_DEVICE_CAPB_DESCR,       // Device capability type descriptor
    CY_U3P_USB2_EXTN_CAPB_TYPE,     // USB 2.0 extension capability type
    0x02,0x00,0x00,0x00,            // Supported device level features: LPM support

    // SuperSpeed device capability
    0x0A,                           // Descriptor size
    CY_U3P_DEVICE_CAPB_DESCR,       // Device capability type descriptor
    CY_U3P_SS_USB_CAPB_TYPE,        // SuperSpeed device capability type
    0x00,                           // Supported device level features
    0x0E,0x00,                      // Speeds supported by the device : SS, HS and FS
    0x03,                           // Functionality support
    0x00,                           // U1 Device Exit latency (Disable Low Power Management)
    0x00,0x00                       // U2 Device Exit latency (Disable LPM)
};

// Standard device qualifier descriptor
const uint8_t USBDeviceQualDscr[] __attribute__ ((aligned (32))) = {
    0x0A,                           // Descriptor size
    CY_U3P_USB_DEVQUAL_DESCR,       // Device qualifier descriptor type
    0x00,0x02,                      // USB 2.0
    0x00,                           // Device class
    0x00,                           // Device sub-class
    0x00,                           // Device protocol
    0x40,                           // Maximum packet size for EP0 : 64 bytes
    0x01,                           // Number of configurations
    0x00                            // Reserved
};

// Standard super speed configuration descriptor
const uint8_t USBSSConfigDscr[] __attribute__ ((aligned (32))) = {
    // Configuration descriptor
    0x09,                           // Descriptor size
    CY_U3P_USB_CONFIG_DESCR,        // Configuration descriptor type
    0x1F,0x00,                      // Length of this descriptor and all sub descriptors
    0x01,                           // Number of interfaces
    0x01,                           // Configuration number
    0x00,                           // COnfiguration string index
    0x80,                           // Configuration characteristics - Bus powered
    0x32,                           // Max power consumption of device (in 8mA unit) : 400mA

    // Interface descriptor
    0x09,                           // Descriptor size
    CY_U3P_USB_INTRFC_DESCR,        // Interface Descriptor type
    0x00,                           // Interface number
    0x00,                           // Alternate setting number
    0x01,                           // Number of end points
    0xFF,                           // Interface class
    0x00,                           // Interface sub class
    0x00,                           // Interface protocol code
    0x00,                           // Interface descriptor string index

    // End-point descriptor for consumer EP
    0x07,                           // Descriptor size
    CY_U3P_USB_ENDPNT_DESCR,        // End-point descriptor type
    CY_FX_EP_CONSUMER,              // End-point address and description
    CY_U3P_USB_EP_BULK,             // Bulk end-point type
    0x00,0x04,                      // Max packet size = 1024 bytes
    0x00,                           // Servicing interval for data transfers : 0 for Bulk

    // Super speed end-point companion descriptor for consumer EP
    0x06,                           // Descriptor size
    CY_U3P_SS_EP_COMPN_DESCR,       // SS end-point companion descriptor type
    (CY_FX_EP_BURST_LENGTH - 1),    // Max no. of packets in a burst(0-15) - 0: burst 1 packet at a time
    0x00,                           // Max streams for bulk EP = 0 (No streams)
    0x00,0x00                       // Service interval for the EP : 0 for bulk
};

// Standard high speed configuration descriptor
const uint8_t USBHSConfigDscr[] __attribute__ ((aligned (32))) = {
    // Configuration descriptor
    0x09,                           // Descriptor size
    CY_U3P_USB_CONFIG_DESCR,        // Configuration descriptor type
    0x19,0x00,                      // Length of this descriptor and all sub descriptors
    0x01,                           // Number of interfaces
    0x01,                           // Configuration number
    0x00,                           // COnfiguration string index
    0x80,                           // Configuration characteristics - bus powered
    0x32,                           // Max power consumption of device (in 2mA unit) : 100mA

    // Interface descriptor
    0x09,                           // Descriptor size
    CY_U3P_USB_INTRFC_DESCR,        // Interface Descriptor type
    0x00,                           // Interface number
    0x00,                           // Alternate setting number
    0x01,                           // Number of end-points
    0xFF,                           // Interface class
    0x00,                           // Interface sub class
    0x00,                           // Interface protocol code
    0x00,                           // Interface descriptor string index

    // End-point descriptor for consumer EP
    0x07,                           // Descriptor size
    CY_U3P_USB_ENDPNT_DESCR,        // End-point descriptor type
    CY_FX_EP_CONSUMER,              // End-point address and description
    CY_U3P_USB_EP_BULK,             // Bulk end-point type
    0x00,0x02,                      // Max packet size = 512 bytes
    0x00                            // Servicing interval for data transfers : 0 for bulk
};

// Standard full speed configuration descriptor
const uint8_t USBFSConfigDscr[] __attribute__ ((aligned (32))) = {
    // Configuration descriptor
    0x09,                           // Descriptor size
    CY_U3P_USB_CONFIG_DESCR,        // Configuration descriptor type
    0x19,0x00,                      // Length of this descriptor and all sub descriptors
    0x01,                           // Number of interfaces
    0x01,                           // Configuration number
    0x00,                           // COnfiguration string index
    0x80,                           // Configuration characteristics - bus powered
    0x32,                           // Max power consumption of device (in 2mA unit) : 100mA

    // Interface descriptor
    0x09,                           // Descriptor size
    CY_U3P_USB_INTRFC_DESCR,        // Interface descriptor type
    0x00,                           // Interface number
    0x00,                           // Alternate setting number
    0x01,                           // Number of end-points
    0xFF,                           // Interface class
    0x00,                           // Interface sub class
    0x00,                           // Interface protocol code
    0x00,                           // Interface descriptor string index

    // End-point descriptor for consumer EP
    0x07,                           // Descriptor size
    CY_U3P_USB_ENDPNT_DESCR,        // End-point descriptor type
    CY_FX_EP_CONSUMER,              // End-point address and description
    CY_U3P_USB_EP_BULK,             // Bulk end-point type
    0x40,0x00,                      // Max packet size = 64 bytes
    0x00                            // Servicing interval for data transfers : 0 for bulk
};

// Standard language ID string descriptor
const uint8_t USBStringLangIDDscr[] __attribute__ ((aligned (32))) = {
    0x04,                           // Descriptor size
    CY_U3P_USB_STRING_DESCR,        // Device descriptor type (2 bytes)
    0x09,0x04                       // Language ID supported
};

// Standard manufacturer string descriptor
const uint8_t USBManufactureDscr[] __attribute__ ((aligned (32))) = {
    0x16,                           // Descriptor size
    CY_U3P_USB_STRING_DESCR,        // Device descriptor type (2 bytes)
    'D',0x00,
    'o',0x00,
    'm',0x00,
    'e',0x00,
    's',0x00,
    'd',0x00,
    'a',0x00,
    'y',0x00,
    '8',0x00,
    '6',0x00
};

// Standard product string descriptor
const uint8_t USBProductDscr[] __attribute__ ((aligned (32))) = {
    0x28,                           // Descriptor size
    CY_U3P_USB_STRING_DESCR,        // Device descriptor type (2 bytes)
    'D',0x00,
    'o',0x00,
    'm',0x00,
    'e',0x00,
    's',0x00,
    'd',0x00,
    'a',0x00,
    'y',0x00,
    ' ',0x00,
	'D',0x00,
	'u',0x00,
	'p',0x00,
	'l',0x00,
	'i',0x00,
	'c',0x00,
	'a',0x00,
	't',0x00,
	'o',0x00,
	'r',0x00
};

// No more code after this line!
const uint8_t CyFxUsbDscrAlignBuffer[32] __attribute__ ((aligned (32)));
