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

// LibUSB transfer callback function prototype
static void LIBUSB_CALL bulkTransferCallback(struct libusb_transfer *transfer);

// The following statically defined variables are used by the libUSB
// transfer callback

// Private variables used to control the transfer callbacks
static quint32 transfersInFlight;   // Number of transfers that are in progress
static bool stopUsbTransfers;       // USB transfer stop flag

// Structure to hold libUSB transfer user data
struct transferUserDataStruct {
    //quint32 transferNumber; // The position of the transfer
    bool lastInQueue;       // Flag to indicate if the transfer is the last in the queue
};

// Private variables used to report statistics about the transfer process
struct statisticsStruct {
    quint32 successCount;           // Number of successful transfers
    quint32 failureCount;           // Number of failed transfers
    quint32 transferPerformance;    // Performance in Kilobytes per second
    quint32 transferSize;           // Size of data transfers performed so far
    quint32 diskFailureCount;       // Number of disk write failures
    struct timeval startTimestamp;  // Data transfer start time stamp.
};
static statisticsStruct statistics;

// FPGA test mode status flag
static bool testModeFlag;

QUsbBulkTransfer::QUsbBulkTransfer(void)
{
    stopUsbTransfers = false;
    qDebug() << "QUsbBulkTransfer::QUsbBulkTransfer(): Called";
}

QUsbBulkTransfer::~QUsbBulkTransfer()
{
    bulkTransferStop();

    // Stop the running thread
    this->wait();
}

void QUsbBulkTransfer::bulkTransferStop(void)
{
    // Set flag to cause transferring to stop (handled by transfer run() and bulkTransferCallback())
    stopUsbTransfers = true;
}

void QUsbBulkTransfer::setup(libusb_context* mCtx, libusb_device_handle* devHandle, quint8 endPoint, bool testMode, QString fileName)
{
    // Store the libUSB information locally
    libUsbContext = mCtx;
    libUsbDeviceHandle = devHandle;
    libUsbEndPoint = endPoint;

    // Set the maximum number of simultaneous transfers allowed
    queueDepth = 16;

    // Set the FPGA data test mode flag
    testModeFlag = testMode;

    if (testModeFlag) {
        qDebug() << "usbBulkTransfer::setup(): Test mode is ON";
    }

    // Set the capture file name
    captureFileName = fileName;
}

void QUsbBulkTransfer::run()
{
    // These variables are referenced by the callback and must remain valid
    // until the transfer is complete
    struct libusb_transfer **usbTransfers = NULL;           // List of transfer structures.
    unsigned char **usbDataBuffers = NULL;                  // List of USB data buffers.
    transferUserDataStruct transferUserData[queueDepth];    // Transfer user data

    int transferReturnStatus = 0; // Status return from libUSB
    qDebug() << "usbBulkTransfer::run(): Started";

    // Check for validity of the device handle
    if (libUsbDeviceHandle == NULL) {
        qDebug() << "QUsbBulkTransfer::run(): Invalid device handle";
        return;
    }

    // Reset transfer tracking ready for the new run...

    // Reset transfer statistics
    statistics.successCount = 0;
    statistics.failureCount = 0;
    statistics.transferSize = 0;
    statistics.transferPerformance = 0;
    statistics.diskFailureCount = 0;
    gettimeofday(&statistics.startTimestamp, NULL);

    // Reset transfer variables
    transfersInFlight = 0;      // Number of transfers that are in progress
    requestSize = 16;           // Request size (number of packets)
    packetSize = 1024;          // Maximum packet size for the endpoint (in bytes)

    // Allocate buffers and transfer structures for USB->PC communication
    bool allocFail = false;

    usbDataBuffers = (unsigned char **)calloc (queueDepth, sizeof (unsigned char *));
    usbTransfers   = (struct libusb_transfer **)calloc (queueDepth, sizeof (struct libusb_transfer *));

    if ((usbDataBuffers != NULL) && (usbTransfers != NULL)) {

        for (unsigned int i = 0; i < queueDepth; i++) {

            usbDataBuffers[i] = (unsigned char *)malloc (requestSize * packetSize);
            usbTransfers[i]   = libusb_alloc_transfer(0);

            if ((usbDataBuffers[i] == NULL) || (usbTransfers[i] == NULL)) {
                allocFail = true;
                break;
            }
        }

    } else {
        allocFail = true;
    }

    // Check if all memory allocations have succeeded
    if (allocFail) {
        qDebug() << "QUsbBulkTransfer::run(): Could not allocate required buffers for USB communication";
        freeUsbTransferBuffers(usbDataBuffers, usbTransfers);
        return;
    }

    // Check if all memory allocations have succeeded
    if (allocFail) {
        qDebug() << "QUsbBulkTransfer::run(): Could not allocate required buffers for disk write";
        freeUsbTransferBuffers(usbDataBuffers, usbTransfers);
        return;
    }

    // Set up the initial transfer requests
    for (unsigned int currentTransferNumber = 0; currentTransferNumber < queueDepth; currentTransferNumber++) {
        // Assign user data to the transfer
        //transferUserData[currentTransferNumber].transferNumber = currentTransferNumber;

        if (currentTransferNumber == (queueDepth - 1)) transferUserData[currentTransferNumber].lastInQueue = true;
        else transferUserData[currentTransferNumber].lastInQueue = false;

        // Set transfer flag to cause transfer error if there is a short packet
        usbTransfers[currentTransferNumber]->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;

        libusb_fill_bulk_transfer(usbTransfers[currentTransferNumber], libUsbDeviceHandle, libUsbEndPoint,
            usbDataBuffers[currentTransferNumber], (requestSize * packetSize), bulkTransferCallback, &transferUserData[currentTransferNumber], 5000);
    }

    // Submit the transfers
    for (unsigned int currentTransferNumber = 0; currentTransferNumber < queueDepth; currentTransferNumber++) {
        transferReturnStatus = libusb_submit_transfer(usbTransfers[currentTransferNumber]);
        if (transferReturnStatus == 0) {
            transfersInFlight++;
        } else {
            qDebug() << "transferThread::run(): Transfer launch" << transfersInFlight << "failed!";
        }
    }

    // Show the current number of inflight transfers
    qDebug() << "QUsbBulkTransfer::run(): Queued" << transfersInFlight << "transfers";

    // Use a 1 second timeout for the libusb_handle_events_timeout call
    struct timeval libusbHandleTimeout;
    libusbHandleTimeout.tv_sec  = 1;
    libusbHandleTimeout.tv_usec = 0;

    // Process libUSB events whilst transfers are in flight
    do {
        libusb_handle_events_timeout(libUsbContext, &libusbHandleTimeout);
    } while (!stopUsbTransfers);

    qDebug() << "transferThread::run(): Stopping streamer thread";

    // Process libUSB events whilst the transfer is stopping
    while (transfersInFlight != 0) {
        qDebug() << "QUsbBulkTransfer::run(): Stopping -" << transfersInFlight << "transfers are pending";
        libusb_handle_events_timeout(libUsbContext, &libusbHandleTimeout);
        usleep(200); // Prevent debug flood
    }

    // Free up the transfer buffers
    freeUsbTransferBuffers(usbDataBuffers, usbTransfers);

    // All done
    qDebug() << "usbBulkTransfer::run(): Stopped";
}

// Function to free USB data buffers and transfer structures
void QUsbBulkTransfer::freeUsbTransferBuffers(unsigned char **dataBuffers, struct libusb_transfer **transfers)
{
    // Free up any allocated transfer structures
    if (transfers != NULL) {
        for (unsigned int i = 0; i < queueDepth; i++) {
            if (transfers[i] != NULL) {
                libusb_free_transfer(transfers[i]);
            }
            transfers[i] = NULL;
        }
        free (transfers);
    }

    // Free up any allocated data buffers
    if (dataBuffers != NULL) {
        for (unsigned int i = 0; i < queueDepth; i++) {
            if (dataBuffers[i] != NULL) {
                free(dataBuffers[i]);
            }
            dataBuffers[i] = NULL;
        }
        free(dataBuffers);
    }
}

// Functions to report transfer statistics -------------------------------------------

// Return the current value of the success counter
quint32 QUsbBulkTransfer::getSuccessCounter(void)
{
    return (quint32)statistics.successCount;
}

// Return the current value of the failure counter
quint32 QUsbBulkTransfer::getFailureCounter(void)
{
    return (quint32)statistics.failureCount;
}

// Return the current transfer performance value
quint32 QUsbBulkTransfer::getTransferPerformance(void)
{
    return (quint32)statistics.transferPerformance;
}

// Return the current value of the disk write failure counter
quint32 QUsbBulkTransfer::getDiskFailureCounter(void)
{
    return (quint32)statistics.diskFailureCount;
}


// LibUSB transfer callback handling function ---------------------------------------------------------
static void LIBUSB_CALL bulkTransferCallback(struct libusb_transfer *transfer)
{
    unsigned int elapsedTime;
    double       performance;

    // Check if the transfer has succeeded
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        // Transfer has failed
        statistics.failureCount++;

        // Show the failure reason in the debug
        switch (transfer->status) {
            case LIBUSB_TRANSFER_ERROR:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_ERROR - Transfer failed";
            break;

            case LIBUSB_TRANSFER_TIMED_OUT:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_TIMED_OUT - Transfer timed out";
            break;

            case LIBUSB_TRANSFER_CANCELLED:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_CANCELLED - Transfer was cancelled";
            break;

            case LIBUSB_TRANSFER_STALL:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_STALL - Endpoint stalled";
            break;

            case LIBUSB_TRANSFER_NO_DEVICE:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_NO_DEVICE - Device disconnected";
            break;

            case LIBUSB_TRANSFER_OVERFLOW:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_OVERFLOW - Device overflow";
            break;

            default:
                qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER - Unknown error";
        }
    } else {
        // Transfer has succeeded
        statistics.successCount++;
    }

    // Update the transfer size for the whole bulk transfer so far
    statistics.transferSize += transfer->length;

    // Reduce the number of requests in-flight.
    transfersInFlight--;

    // Extract the user data
    transferUserDataStruct *transferUserData = (transferUserDataStruct *)transfer->user_data;
    //unsigned int threadTransferNumber = transferUserData->transferNumber;
    bool lastInQueue = transferUserData->lastInQueue;

    // If this was the last transfer in the queue, calculate statistics
    if (lastInQueue) {
        // Calculate the transfer statistics (just for user feedback)
        struct timeval endTimestamp;

        gettimeofday (&endTimestamp, NULL);
        elapsedTime = ((endTimestamp.tv_sec - statistics.startTimestamp.tv_sec) * 1000000 +
            (endTimestamp.tv_usec - statistics.startTimestamp.tv_usec));

        // Calculate the performance in Kbytes per second
        performance = (((double)statistics.transferSize / 1024) / ((double)elapsedTime / 1000000));
        statistics.transferPerformance = (unsigned int)performance;
        statistics.transferSize = 0;
        statistics.startTimestamp = endTimestamp;
    }

    // Prepare and re-submit the transfer request (unless stopTransfers has been flagged)
    if (!stopUsbTransfers) {
        // Fill and resubmit the transfer
        libusb_fill_bulk_transfer(transfer, transfer->dev_handle, transfer->endpoint,
            transfer->buffer, transfer->length, bulkTransferCallback, transfer->user_data, 5000);

        // If a transfer fails, ignore the error
        if (libusb_submit_transfer(transfer) == 0) transfersInFlight++;
        else qDebug() << "bulkTransferCallback(): libusb_submit_transfer failed!";
    }
}
