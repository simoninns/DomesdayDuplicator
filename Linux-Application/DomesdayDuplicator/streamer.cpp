/************************************************************************

    streamer.cpp

    QT/Linux RF Capture application USB data streamer functions
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

#include "streamer.h"
#include "transferthread.h"

// Libraries taken from original code
#include <QtCore>
#include <QtGui>

//#include <stdio.h>
//#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>


// Class contructor
streamer::streamer(void)
{
    // Nothing to do here boys...
}

// Function to start the streamer transfer
void streamer::startTransfer(void)
{
    qDebug() << "streamer::startTransfer(): Started";

    int numberOfDevices, numberOfInterfaces, index = 0;
    int apiResponse;
    struct libusb_config_descriptor *configDescriptor = NULL;

    // Look for duplicator device with VID=0x1D50 and PID=0x603B
    numberOfDevices = cyusb_open(0x1D50, 0x603B);

    // If no devices are found, close the cyusb instance
    if (numberOfDevices < 1) {
        qDebug() << "streamer::startTransfer(): Domesday duplicator USB device not connected";
        cyusb_close();
        return;
    }

    // Get the device handle
    deviceHandle = cyusb_gethandle(0); // should be deviceNumber based, but right now we will just connect to the first detected device;

    // Output the selected device's information to debug
    qDebug() << "streamer::startTransfer(): Device is" <<
        "VID=" << QString("0x%1").arg(cyusb_getvendor(deviceHandle) , 0, 16) <<
        "PID=" << QString("0x%1").arg(cyusb_getproduct(deviceHandle) , 0, 16) <<
        "BusNum=" << QString("0x%1").arg(cyusb_get_busnumber(deviceHandle) , 0, 16) <<
        "Addr=" << QString("0x%1").arg(cyusb_get_devaddr(deviceHandle) , 0, 16);

    // Get the configuration descriptor for the selected device (is this really required?)
    apiResponse = cyusb_get_active_config_descriptor(deviceHandle, &configDescriptor);
    if (apiResponse) {
        qDebug() << "streamer::startTransfer(): cyusb_get_active_config_descriptor returned ERROR";
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

    // Ensure that a kernel driver is not attached to the device
    apiResponse = cyusb_kernel_driver_active(deviceHandle, 0);
    if (apiResponse != 0) {
        qDebug() << "streamer::startTransfer(): Kernel driver already attached - cannot transfer";
        return;
    }

    // Claim the USB device's interface number 0
    apiResponse = cyusb_claim_interface(deviceHandle, 0);
    if (apiResponse != 0) {
        qDebug() << "streamer::startTransfer(): Could not claim USB device interface - cannot transfer";
        return;
    } else {
        qDebug() << "streamer::startTransfer(): USB device interface claimed successfully";
    }

    // Set vendor specific request 0xB5 with value 0 to start USB device transfer
    cyusb_control_transfer(deviceHandle, 0x40, 0xB5, 0x01, 0x00, 0x00, 0x00, 1000);

    // Set up the parameters for the transfer:
    // endpoint, endpointType, packetSize, requestSize, queueDepth, deviceHandle
    thread.setParameters(0, LIBUSB_TRANSFER_TYPE_BULK, 16, 16, 1, deviceHandle);

    // Flag the transfer as in progress
    thread.run();

    qDebug() << "streamer::startTransfer(): Ended";
    return;
}

// Request that the streamer operation is stopped
void streamer::stopTransfer(void)
{
    int apiResponse;

    // Flag the transfer as stopped
    thread.stop();

    // Wait for the transfer to stop
    thread.wait();

    // Set vendor specific request 0xB5 with value 0 to stop USB device transfer
    cyusb_control_transfer(deviceHandle, 0x40, 0xB5, 0x00, 0x00, 0x00, 0x00, 1000);

    // Release the USB device's interface number 0
    apiResponse = cyusb_release_interface(deviceHandle, 0);
    if (apiResponse != 0) {
        qDebug() << "streamer::stopTransfer(): Could not release USB device interface";
        return;
    } else {
        qDebug() << "streamer::stopTransfer(): USB device interface released successfully";
    }

    // Close the USB device
    cyusb_close();
}

// Check if the streamer main thread is running
bool streamer::isRunning(void)
{
    return thread.isRunning();
}

// These functions need to get the parameters from the thread object and pass them through

// Get the current success count
unsigned int streamer::getSuccessCount(void)
{
    qDebug() << "streamer::getSuccessCount(): Success count = " << successCount;
    return successCount;
}

// Get the current failure count
unsigned int streamer::getFailureCount(void)
{
    qDebug() << "streamer::getFailureCount(): Failure count = " << successCount;
    return failureCount;
}

// Get the current performance count
unsigned int streamer::getPerformanceCount(void)
{
    qDebug() << "streamer::getPerformanceCount(): Transfer performance = " << successCount;
    return transferPerformance;
}

// Private functions ----------------------------------------------------------------------------------



