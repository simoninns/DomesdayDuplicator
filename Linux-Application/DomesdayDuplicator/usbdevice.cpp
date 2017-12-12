/************************************************************************

    usbdevice.cpp

    QT GUI Capture application for Domesday Duplicator
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

// Construct object and connect hot-plug signals
usbDevice::usbDevice(QObject *parent) : QObject(parent)
{
    QObject::connect(&mUsbManager, SIGNAL(deviceInserted(QtUsb::FilterList)),
        this, SLOT(onDevInserted(QtUsb::FilterList)));
    QObject::connect(&mUsbManager, SIGNAL(deviceRemoved(QtUsb::FilterList)), this,
        SLOT(onDevRemoved(QtUsb::FilterList)));

    qDebug("Monitoring for USB device insertion and removal events...");
}

usbDevice::~usbDevice() {
}

// Function returns true if a Domesday Duplicator USB device is connected
bool usbDevice::isConnected(void)
{
    bool deviceStatus = false;
    QtUsb::DeviceFilter mFilter;
    qDebug() << "usbDevice::isConnected(): Checking for USB device";

    // Configure the device filter
    mFilter.vid = VID;
    mFilter.pid = PID;

    if (mUsbManager.isPresent(mFilter)) {
        // Device present
        deviceStatus = true;
        qDebug() << "usbDevice::isConnected(): Device connected";
    } else {
        // Device not present
        deviceStatus = false;
        qDebug() << "usbDevice::isConnected(): Device not connected";
    }

    return deviceStatus;
}

// Function to handle device insertion
void usbDevice::onDevInserted(QtUsb::FilterList list)
{
    qDebug("usbDevice::onDevInserted(): USB device(s) inserted");
    for (int i = 0; i < list.length(); i++) {
        QtUsb::DeviceFilter f = list.at(i);
        qDebug("V%04x:P%04x", f.vid, f.pid);

        // Check for the Domesday Duplicator device
        if (f.vid == VID && f.pid == PID) {
            qDebug() << "usbDevice::onDevInserted(): Domesday Duplicator USB device inserted";
            emit statusChanged(true); // Send a signal
        }
    }
}

// Function to handle device removal
void usbDevice::onDevRemoved(QtUsb::FilterList list)
{
    qDebug("usbDevice::onDevRemoved(): USB device(s) removed");
    for (int i = 0; i < list.length(); i++) {
        QtUsb::DeviceFilter f = list.at(i);
        qDebug("V%04x:P%04x", f.vid, f.pid);

        // Check for the Domesday Duplicator device
        if (f.vid == VID && f.pid == PID) {
            qDebug() << "usbDevice::onDevRemoved(): Domesday Duplicator USB device removed";
            emit statusChanged(false); // Send a signal
        }
    }
}

// Function to set-up device ready for data transfer
void usbDevice::setupDevice(void)
{
    domDupDevice = new QUsbDevice();
    domDupDevice->setDebug(true); // Turn on libUSB debug

    // VID and PID of target device
    domDupFilter.vid = VID;
    domDupFilter.pid = PID;

    // Configuration of target device
    domDupConfig.alternate = 0;
    domDupConfig.config = 1;
    domDupConfig.interface = 0;
    domDupConfig.readEp = 0x81;
    domDupConfig.writeEp = 0x81;
}

// Function to open the device for IO
bool usbDevice::openDevice(void)
{
    qDebug("usbDevice::openDevice(): Opening USB device");

    QtUsb::DeviceStatus deviceStatus;
    deviceStatus = mUsbManager.openDevice(domDupDevice, domDupFilter, domDupConfig);

    if (deviceStatus == QtUsb::deviceOK) {
        // Device is open
        return true;
    }
    // Device is closed
    return false;
}

// Function to close the device for IO
bool usbDevice::closeDevice(void)
{
    qDebug("usbDevice::closeDevice(): Closing USB device");
    mUsbManager.closeDevice(domDupDevice);
    return false;
}

// Read data from the device
void usbDevice::readFromDevice(QByteArray *buf)
{
    domDupDevice->read(buf, 1);
}

// Write data to the device
void usbDevice::writeToDevice(QByteArray *buf)
{
    domDupDevice->write(buf, buf->size());
}

// Send a vendor specific USB command to the device
void usbDevice::sendVendorSpecificCommand(quint16 command, quint16 value)
{
    // Request type fixed to 0x40 (vendor specific command with no data)
    domDupDevice->sendControlTransfer(0x40, command, value, 0x00, NULL, 0x00, 1000);
}

// Start a bulk read (continuously transfers data until stopped)
void usbDevice::startBulkRead(bool testMode, QString fileName)
{
    // Start the bulk transfer
    domDupDevice->startBulkTransfer(testMode, fileName);
}

// Stop a bulk read
void usbDevice::stopBulkRead(void)
{
    // Stop the bulk transfer
    domDupDevice->stopBulkTransfer();
}

// Return the current value of the bulk transfer packet counter
quint32 usbDevice::getPacketCounter(void)
{
    return domDupDevice->getPacketCounter();
}

// Return the current value of the bulk transfer packet size
quint32 usbDevice::getPacketSize(void)
{
    return domDupDevice->getPacketSize();
}

// Return the current bulk transfer stream performance value
quint32 usbDevice::getTransferPerformance(void)
{
    return domDupDevice->getTransferPerformance();
}

// Return the current bulk transfer disk write failure count
quint32 usbDevice::getTestFailureCounter(void)
{
    return domDupDevice->getTestFailureCounter();
}

// Return the current number of available disk buffers
quint32 usbDevice::getAvailableDiskBuffers(void)
{
    return domDupDevice->getAvailableDiskBuffers();
}

// Return the total number of available disk buffers
quint32 usbDevice::getNumberOfDiskBuffers(void)
{
    return domDupDevice->getNumberOfDiskBuffers();
}
