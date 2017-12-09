/************************************************************************

    qusbbulktransfer.h

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

#ifndef QUSBBULKTRANSFER_H
#define QUSBBULKTRANSFER_H

#include <QDebug>
#include <QThread>
#include <QFile>
#include <QtConcurrent/QtConcurrent>

extern "C" {
#include <libusb-1.0/libusb.h>
}

class QUsbBulkTransfer : public QThread {
    Q_OBJECT
public:
    QUsbBulkTransfer(void);
    ~QUsbBulkTransfer(void);
    void setup(libusb_context* mCtx, libusb_device_handle* devHandle, quint8 endPoint, bool testMode, QString fileName);

    quint32 getSuccessCounter(void);
    quint32 getFailureCounter(void);
    quint32 getTransferPerformance(void);
    quint32 getDiskFailureCounter(void);

signals:

public slots:

protected slots:
    void run(void);

protected:
    void freeUsbTransferBuffers(unsigned char **dataBuffers, struct libusb_transfer **transfers);
    void bulkTransferStop(void);

    libusb_context* libUsbContext;
    libusb_device_handle* libUsbDeviceHandle;
    quint8 libUsbEndPoint;

    quint32 requestSize; // Request size in number of packets
    quint32 packetSize;  // Maximum packet size for the endpoint
    quint32 queueDepth;  // Maximum number of queued transfers allowed

    QString captureFileName; // Capture file name
};

#endif // QUSBBULKTRANSFER_H
