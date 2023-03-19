/************************************************************************

    usbcapture.h

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

#ifndef USBCAPTURE_H
#define USBCAPTURE_H

#include <QObject>
#include <QThread>
#include <QFile>
#include <QVector>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>

#include <libusb.h>

class UsbCapture : public QThread
{
    Q_OBJECT
public:
    explicit UsbCapture(QObject *parent = nullptr, libusb_context *libUsbContextParam = nullptr,
                        libusb_device_handle *usbDeviceHandleParam = nullptr, QString filenameParam = nullptr,
                        bool isCaptureFormat10BitParam = true, bool isCaptureFormat10BitDecimatedParam = false,
                        bool isTestData = false);
    ~UsbCapture() override;

    void startTransfer();
    void stopTransfer();
    qint32 getNumberOfTransfers();
    qint32 getNumberOfDiskBuffersWritten();
    QString getLastError();
    static bool getOkToRename();
    static void getAmplitudeBuffer(const unsigned char **buffer, qint32 *numBytes);

signals:
    void transferFailed();

public slots:

protected slots:
    void run() override;
    void runDiskBuffers();

protected:
    libusb_context *libUsbContext;
    libusb_device_handle *usbDeviceHandle;
    QString filename;
    bool isCaptureFormat10Bit;
    bool isCaptureFormat10BitDecimated;
    bool isTestData;

private:
    qint32 numberOfDiskBuffersWritten;

    enum {
        SEQUENCE_SYNC,
        SEQUENCE_RUNNING,
        SEQUENCE_DISABLED,
        SEQUENCE_FAILED,
    } sequenceState;
    quint32 savedSequenceCounter;
    void checkBufferSequence(qint32 diskBufferNumber);

    qint32 savedTestDataValue;
    qint32 testDataMax;
    void writeBufferToDisk(QFile *outputFile, qint32 diskBufferNumber);
    void writeConversionBuffer(QFile *outputFile, qint32 numBytes);

    void allocateDiskBuffers();
    void freeDiskBuffers();
};

#endif // USBCAPTURE_H
