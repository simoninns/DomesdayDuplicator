/************************************************************************

    qusbbulktransfer.cpp

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

#include "qusbbulktransfer.h"

QUsbBulkTransfer::QUsbBulkTransfer(void)
{
    tStop = false;
    qDebug() << "QUsbBulkTransfer::QUsbBulkTransfer(): Called";

    // Start the thread running
    //this->start();
}

QUsbBulkTransfer::~QUsbBulkTransfer()
{
    tStop = true;

    // Stop the running thread
    this->wait();
}

void QUsbBulkTransfer::setup(libusb_device_handle* devHandle, quint8 endPoint)
{
    // Store the device handle locally
    bDevHandle = devHandle;
    bEndPoint = endPoint;

    queueDepth = 1; // The maximum number of simultaneous transfers

    // Default all status variables
    successCount = 0;           // Number of successful transfers
    failureCount = 0;           // Number of failed transfers
    transferSize = 0;           // Size of data transfers performed so far
    transferIndex = 0;          // Write index into the transfer_size array
    transferPerformance = 0;	// Performance in KBps
    requestsInFlight = 0;       // Number of transfers that are in progress
}

void QUsbBulkTransfer::run()
{
    int transferReturnStatus = 0; // Status return from libUSB
    qDebug() << "usbBulkTransfer::run(): Started";


    //
    // CANNOT USE BYTEARRAY... WE NEED 2D ARRAYS... RECODE
    //

    // Array for the data
    QByteArray *dataBuffers = new QByteArray;

    // Array for the transfer structure
    QByteArray *transfers = new QByteArray;

    dataBuffers->resize(16384);
    transfers->resize(16384);

    // Record the timestamp at start of transfer (for statistics generation)
    gettimeofday(&startTimestamp, NULL); // Might be better to use the QT library for this?

    // Launch the transfers
    for (unsigned int launchCounter = 0; launchCounter < queueDepth; launchCounter++) {
        qDebug() << "QUsbBulkTransfer::run(): Launching transfer number" << launchCounter << "of" << queueDepth;

        libusb_fill_bulk_transfer((libusb_transfer *)transfers->data(), bDevHandle, bEndPoint,
            (unsigned char *)dataBuffers->data(), requestSize * packetSize, QUsbBulkTransfer::bulkTransferCallback, NULL, 5000);

        qDebug() << "transferThread::run(): Performing libusb_submit_transfer";
        transferReturnStatus = libusb_submit_transfer((libusb_transfer *)transfers->data());
        if (transferReturnStatus == 0) requestsInFlight++;
        qDebug() << "transferThread::run(): Launched";
    }

    // Show the current number of inflight requests
    qDebug() << "QUsbBulkTransfer::run(): Queued" << requestsInFlight << "requests";

    // NEED TO RECODE THIS BIT... NOT VERY NEAT...
    struct timeval timeout;

    // Use a 1 second timeout for the libusb_handle_events_timeout call
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    // Process libUSB events whilst transfer is running
    do {
        libusb_handle_events_timeout (NULL, &timeout);
    } while (!tStop);

    qDebug() << "transferThread::run(): Stopping streamer thread";

    // Process libUSB events whilst the transfer is stopping
    while (requestsInFlight != 0) {
        qDebug() << "QUsbBulkTransfer::run():" << requestsInFlight << "requests are pending";
        libusb_handle_events_timeout(NULL, &timeout);
        sleep(1);
    }

    // All done
    qDebug() << "usbBulkTransfer::run(): Stopped";
}

// This is the call back function called by libusb upon completion of a queued data transfer.
void LIBUSB_CALL QUsbBulkTransfer::bulkTransferCallback(struct libusb_transfer *transfer)
{
    qDebug() << "QUsbBulkTransfer::bulkTransferCallback(): Called";

    // Ensure that the transfer structure is valid
    if (!transfer || !transfer->user_data) return;
}
