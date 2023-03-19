/************************************************************************

    usbdevice.h

    Capture application for the Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2018-2019 Simon Inns

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
#include <QThread>
#include <QWaitCondition>

#include <libusb.h>
#include "usbcapture.h"

class UsbDevice : public QThread
{
    Q_OBJECT
public:
    explicit UsbDevice(QObject *parent = nullptr, quint16 vid = 0x1D50, quint16 pid = 0x603B);
    ~UsbDevice() override;

    void stop();

    bool scanForDevice();
    void sendConfigurationCommand(bool testMode);

    void startCapture(QString filename, bool isCaptureFormat10Bit, bool isCaptureFormat10BitDecimated, bool isTestMode);
    void stopCapture();
    qint32 getNumberOfTransfers();
    qint32 getNumberOfDiskBuffersWritten();
    QString getLastError();

signals:
    void deviceAttached();
    void deviceDetached();
    void transferFailed();

public slots:

protected slots:
    void run() override;

protected:
    libusb_context *libUsbContext;
    libusb_device_handle *usbDeviceHandle;
    bool threadAbort;

private slots:
    void transferFailedSignalHandler();

private:
    quint16 deviceVid;
    quint16 devicePid;

    UsbCapture *usbCapture;
    QString lastError;

    bool open();
    void close();
    bool sendVendorSpecificCommand(quint8 command, quint16 value);
    bool searchForAttachedDevice();
};

#endif // USBDEVICE_H
