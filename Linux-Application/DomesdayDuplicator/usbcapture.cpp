/************************************************************************

    usbcapture.cpp

    Capture application for the Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2018 Simon Inns

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

#include "usbcapture.h"

UsbCapture::UsbCapture(QObject *parent, libusb_device_handle *usbDeviceHandleParam, QString filenameParam) : QThread(parent)
{
    // Set the device's handle
    usbDeviceHandle = usbDeviceHandleParam;

    // Store the requested file name
    filename = filenameParam;
}

// Class destructor
UsbCapture::~UsbCapture()
{
    // Stop the capture thread
    threadAbort = true;
    this->wait();

    // Destroy the device handle

    // Start the capture processing thread
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
    this->start();
}

// Run the capture thread
void UsbCapture::run(void)
{

}

// Start capturing
void UsbCapture::start(void)
{

}

// Stop capturing
void UsbCapture::stop(void)
{

}
