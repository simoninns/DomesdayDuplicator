/************************************************************************

    usbdevice.h

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


#ifndef USBDEVICE_H
#define USBDEVICE_H

#include <QObject>
#include <QDebug>
#include "QtUsb/qusb.h"

const quint8 USB_PIPE_IN = 0x81;        // Bulk output endpoint for responses
const quint8 USB_PIPE_OUT = 0x01;       // Bulk input endpoint for commands
const quint16 USB_TIMEOUT_MSEC = 300;   // USB timeout in microseconds

// VID and PID of the Domesday Duplicator USB device
#define VID 0x1D50
#define PID 0x603B

class usbDevice : public QObject
{
    Q_OBJECT
public:
    explicit usbDevice(QObject *parent = 0);
    ~usbDevice(void);
    bool isConnected(void);

signals:
    void statusChanged(bool status);

public slots:
    void onDevInserted(QtUsb::FilterList list);
    void onDevRemoved(QtUsb::FilterList list);

private:
    QUsbManager mUsbManager;

};

#endif // USBDEVICE_H
