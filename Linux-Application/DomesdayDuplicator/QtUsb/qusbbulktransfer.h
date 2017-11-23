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
extern "C" {
#include <libusb-1.0/libusb.h>
}

//class QUSBSHARED_EXPORT QUsbBulkTransfer : public QThread {
class QUsbBulkTransfer : public QThread {
    Q_OBJECT
public:
    QUsbBulkTransfer(void);
    ~QUsbBulkTransfer(void);
    void setup(libusb_device_handle* devHandle, quint8 endPoint);

    // Callback function for libUSB transfers
    static void LIBUSB_CALL bulkTransferCallback(struct libusb_transfer *transfer);

signals:

public slots:

protected slots:
    void run(void);

protected:
    bool tStop; // Transfer stop flag

    libusb_device_handle* bDevHandle;
    quint8 bEndPoint;

    // Private variables for storing transfer configuration
    unsigned int    queueDepth;             // Number of requests to queue
    unsigned int    requestSize;            // Request size in number of packets
    unsigned int    packetSize;             // Maximum packet size for the endpoint

    // Private variables used to report on the transfer
    unsigned int    successCount;           // Number of successful transfers
    unsigned int    failureCount;           // Number of failed transfers
    unsigned int    transferPerformance;	// Performance in Kilobytes per second
    unsigned int    transferSize;           // Size of data transfers performed so far

    // Private variables used to control the transfer
    unsigned int    transferIndex;          // Write index into the transfer_size array
    volatile int    requestsInFlight;       // Number of transfers that are in progress

    struct timeval	startTimestamp;         // Data transfer start time stamp.
    struct timeval	endTimestamp;			// Data transfer stop time stamp.
};

#endif // QUSBBULKTRANSFER_H
