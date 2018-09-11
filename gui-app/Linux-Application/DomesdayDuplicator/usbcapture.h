/************************************************************************

    usbcapture.h

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

#ifndef USBCAPTURE_H
#define USBCAPTURE_H

#include <QObject>
#include <QThread>

#include <libusb-1.0/libusb.h>

class UsbCapture : public QThread
{
    Q_OBJECT
public:
    explicit UsbCapture(QObject *parent = nullptr, libusb_device_handle *usbDeviceHandleParam = nullptr, QString filenameParam = nullptr);
    ~UsbCapture() override;

    void start(void);
    void stop(void);

signals:

public slots:

protected slots:
    void run() override;

protected:
    libusb_device_handle *usbDeviceHandle;
    QString filename;
    bool threadAbort;
};

#endif // USBCAPTURE_H
