/************************************************************************

    usbdevice.h

    QT/Linux RF Capture application USB device header
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

#ifndef USBDEVICE_H
#define USBDEVICE_H

#include <QDebug>
#include <QByteArray>

// Include the USB library wrapper
#include <cyusb.h>

class usbDevice
{
public:
    usbDevice();
    ~usbDevice();

    int detectDevices(void);
    void connectDevice(int);
    void startTransfer(void);
    void stopTransfer(void);
    bool isTransferInProgress();
    void transferBulkInBlock(void);

private:
    bool usbDeviceOpenFlag;
    int numberOfDevices;
    int selectedDevice;
    bool transferInProgressFlag;
    void showLibUsbErrorCode(int errorCode);

    QByteArray bulkInBuffer;
    cyusb_handle *deviceHandle;
};

#endif // USBDEVICE_H
