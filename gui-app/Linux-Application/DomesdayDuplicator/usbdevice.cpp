/************************************************************************

    usbdevice.cpp

    QT/Linux RF Capture application USB device functions
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

#include "usbdevice.h"

// Class constructor
usbDevice::usbDevice()
{
    // Initialise class private variables
    usbDeviceOpenFlag = false;
    numberOfDevices = 0;
    selectedDevice = 0;
    transferInProgressFlag = false;

    // Initialise the device handle
    deviceHandle = NULL;

    // Configure data transfer buffer
    bulkInBuffer.resize(1024 * 256); // 256 Kbyte buffer
    bulkInBuffer.fill('\0');
}

// Class destructor
usbDevice::~usbDevice()
{
    // Perform a cyusb close if an open device is present
    if (usbDeviceOpenFlag) cyusb_close();

    qDebug() << "usbDevice::~usbDevice(): USB device closed";
}

// Function looks for any connected devices and returns the number of found devices
int usbDevice::detectDevices(void)
{
    // Ensure that a device is not already connected
    if (usbDeviceOpenFlag) {
        qDebug() << "usbDevice::detectDevices(): Called, but device already open";
        return numberOfDevices;
    }

    // Look for duplicator device with VID=0x1D50 and PID=0x603B
    numberOfDevices = cyusb_open(0x1D50, 0x603B);

    // If no devices are found, close the cyusb instance
    if (numberOfDevices < 1) {
        qDebug() << "usbDevice::detectDevices(): No USB devices detected";
        cyusb_close();
        numberOfDevices = 0;
    } else {
        qDebug() << "usbDevice::detectDevices():" << numberOfDevices << "device(s) detected";
    }

    return numberOfDevices;
}

// Function connects to the specified device
void usbDevice::connectDevice(int deviceNumber)
{
    // Check that a device isn't already open
    if (usbDeviceOpenFlag) {
        qDebug() << "usbDevice::connectDevice(): Called, but device already open";
        return;
    }

    // Check that the requested device number is valid
    if (deviceNumber > numberOfDevices) {
        qDebug() << "usbDevice::connectDevice(): Requested device number is invalid";
        return;
    }

    int numberOfInterfaces, index = 0;
    int apiResponse;
    struct libusb_config_descriptor *configDescriptor = NULL;
    cyusb_handle *deviceHandle = NULL;

    // Get the device handle
    deviceHandle = cyusb_gethandle(deviceNumber);

    // Output the selected device's information to debug
    qDebug() << "usbDevice::connectDevice(): Device is" <<
        "VID=" << QString("0x%1").arg(cyusb_getvendor(deviceHandle) , 0, 16) <<
        "PID=" << QString("0x%1").arg(cyusb_getproduct(deviceHandle) , 0, 16) <<
        "BusNum=" << QString("0x%1").arg(cyusb_get_busnumber(deviceHandle) , 0, 16) <<
        "Addr=" << QString("0x%1").arg(cyusb_get_devaddr(deviceHandle) , 0, 16);

    // Get the configuration descriptor for the selected device
    apiResponse = cyusb_get_active_config_descriptor(deviceHandle, &configDescriptor);
    if (apiResponse) {
        qDebug() << "usbDevice::connectDevice(): cyusb_get_active_config_descriptor returned ERROR";
        return;
    }

    // Detach any active kernel drives for the device's interfaces
    numberOfInterfaces = configDescriptor->bNumInterfaces;
    while (numberOfInterfaces) {
        if (cyusb_kernel_driver_active(deviceHandle, index)) {
            cyusb_detach_kernel_driver(deviceHandle, index);
        }
        index++;
        numberOfInterfaces--;
    }
    cyusb_free_config_descriptor(configDescriptor);

    // Flag the device as open
    usbDeviceOpenFlag = true;
}

// Start transfer of data from device
void usbDevice::startTransfer(void)
{
    int apiResponse;

    // Ensure a device is connected
    if (!usbDeviceOpenFlag) {
        qDebug() << "usbDevice::startTransfer(): Called, but no device is open";
        return;
    }

    if (transferInProgressFlag) {
        // Transfer already in progress!
        qDebug() << "usbDevice::startTransfer(): Called, but transfer is already in progress";
    } else {
        qDebug() << "usbDevice::startTransfer(): Transfer starting";

        // Set vendor specific (0x40) request 0xB5 with value 1 to start USB device transfer
        deviceHandle = cyusb_gethandle(selectedDevice);
        cyusb_control_transfer(deviceHandle, 0x40, 0xB5, 0x01, 0x00, 0x00, 0x00, 1000);

        // Ensure that a kernel driver is not attached to the device
        apiResponse = cyusb_kernel_driver_active(deviceHandle, 0);
        if (apiResponse != 0) {
            qDebug() << "usbDevice::startTransfer(): Kernel driver already attached - cannot transfer";
            transferInProgressFlag = false;
            return;
        }

        // Claim the USB device's interface number 0
        apiResponse = cyusb_claim_interface(deviceHandle, 0);
        if (apiResponse != 0) {
            qDebug() << "usbDevice::startTransfer(): Could not claim USB device interface - cannot transfer";
            transferInProgressFlag = false;
            return;
        } else {
            qDebug() << "usbDevice::startTransfer(): USB device interface claimed successfully";
        }

        // Flag the transfer as in progress
        transferInProgressFlag = true;
    }
}

// Stop transfer of data from device
void usbDevice::stopTransfer(void)
{
    int apiResponse;

    if (!transferInProgressFlag) {
        // Transfer not in progress!
        qDebug() << "usbDevice::stopTransfer(): Called, but transfer is not in progress";
    } else {
        transferInProgressFlag = false;
        qDebug() << "usbDevice::stopTransfer(): Transfer stopping";

        // Set vendor specific request 0xB5 with value 0 to stop USB device transfer
        deviceHandle = cyusb_gethandle(selectedDevice);
        cyusb_control_transfer(deviceHandle, 0x40, 0xB5, 0x00, 0x00, 0x00, 0x00, 1000);

        // Release the USB device's interface number 0
        apiResponse = cyusb_release_interface(deviceHandle, 0);
        if (apiResponse != 0) {
            qDebug() << "usbDevice::stopTransfer(): Could not release USB device interface";
            transferInProgressFlag = false;
            return;
        } else {
            qDebug() << "usbDevice::startTransfer(): USB device interface released successfully";
        }
    }
}

// Get transfer in progress flag status
bool usbDevice::isTransferInProgress()
{
    return transferInProgressFlag;
}

// Transfer a block of data from the device using the bulk in interface
void usbDevice::transferBulkInBlock(void)
{
    int apiResponse = 0;
    int bytesReceived = 0;
    int transferredBytes = 0;

    if (!transferInProgressFlag) {
        qDebug() << "usbDevice::startTransfer(): called, but no transfer is in progress";
        return;
    }

    // Clear the bulk in transfer buffer
    bulkInBuffer.fill('\0');

    // Here we attempt to receive 256 Kbytes of data but, if for any reason the bulk transfer
    // returns < 1024 bytes, we quit transfering and leave it until the next poll
    for (int kbytesReceived = 0; kbytesReceived < 256; kbytesReceived++) {
        // Perform a bulk IN transfer with a time-out of 10ms
        apiResponse = cyusb_bulk_transfer(deviceHandle, 0x81, (unsigned char*)bulkInBuffer.data()+bytesReceived, 1024, &transferredBytes, 10);
        if (apiResponse != 0) {
            qDebug() << "usbDevice::transferBulkInBlock(): cyusb_bulk_transfer() failed";
            showLibUsbErrorCode(apiResponse);
            transferInProgressFlag = false;
            return;
        } else {
            bytesReceived += transferredBytes;
        }

        // Check for a short transfer
        if (transferredBytes != 1024) continue;
    }

    // Show the actual number of received bytes in the debug
    qDebug() << "usbDevice::transferBulkInBlock(): Total of" << bytesReceived << "bytes received from USB device";
}

void usbDevice::showLibUsbErrorCode(int errorCode)
{
    if (errorCode == LIBUSB_ERROR_IO)
            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_IO";
        else if (errorCode == LIBUSB_ERROR_INVALID_PARAM)
            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_INVALID_PARAM";
        else if (errorCode == LIBUSB_ERROR_ACCESS)
            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_ACCESS";
        else if (errorCode == LIBUSB_ERROR_NO_DEVICE)
            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_NO_DEVICE";
        else if (errorCode == LIBUSB_ERROR_NOT_FOUND)
            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_NOT_FOUND";
        else if (errorCode == LIBUSB_ERROR_BUSY)
            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_BUSY";
        else if (errorCode == LIBUSB_ERROR_TIMEOUT)
            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_TIMEOUT";
        else if (errorCode == LIBUSB_ERROR_OVERFLOW)
            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_OVERFLOW";
        else if (errorCode == LIBUSB_ERROR_PIPE)
            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_PIPE";
        else if (errorCode == LIBUSB_ERROR_INTERRUPTED)
            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_INTERRUPTED";
        else if (errorCode == LIBUSB_ERROR_NO_MEM)
            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_NO_MEM";
        else if (errorCode == LIBUSB_ERROR_NOT_SUPPORTED)
            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_NOT_SUPPORTED";
        else if (errorCode == LIBUSB_ERROR_OTHER)
            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_OTHER";
        else qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_UNDOCUMENTED";
}
