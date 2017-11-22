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
