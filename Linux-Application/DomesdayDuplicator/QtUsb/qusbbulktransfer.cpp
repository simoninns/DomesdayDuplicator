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

// Callback function for libUSB transfers
static void LIBUSB_CALL bulkTransferCallback(struct libusb_transfer *transfer);

static libusb_context* bContext;
static libusb_device_handle* bDevHandle;
static quint8 bEndPoint;

// Private variables for storing transfer configuration
static unsigned int    queueDepth;             // Number of requests to queue
static unsigned int    requestSize;            // Request size in number of packets
static unsigned int    packetSize;             // Maximum packet size for the endpoint

// Private variables used to report on the transfer
static unsigned int    successCount;           // Number of successful transfers
static unsigned int    failureCount;           // Number of failed transfers
static unsigned int    transferPerformance;    // Performance in Kilobytes per second
static unsigned int    transferSize;           // Size of data transfers performed so far

// Private variables used to control the transfer
static unsigned int    transferIndex;          // Write index into the transfer_size array
static volatile int    requestsInFlight;       // Number of transfers that are in progress
static bool            stopTransfers;          // Transfer stop flag

static struct timeval	startTimestamp;        // Data transfer start time stamp.
static struct timeval	endTimestamp;          // Data transfer stop time stamp.

QUsbBulkTransfer::QUsbBulkTransfer(void)
{
    stopTransfers = false;
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
    stopTransfers = true;
}

void QUsbBulkTransfer::setup(libusb_context* mCtx, libusb_device_handle* devHandle, quint8 endPoint)
{
    // Store the device handle locally
    bContext = mCtx;
    bDevHandle = devHandle;
    bEndPoint = endPoint;

    queueDepth = 16; // The maximum number of simultaneous transfers (should be 16)

    // Default all status variables
    successCount = 0;           // Number of successful transfers
    failureCount = 0;           // Number of failed transfers
    transferSize = 0;           // Size of data transfers performed so far
    transferIndex = 0;          // Write index into the transfer_size array
    transferPerformance = 0;	// Performance in KBps
    requestsInFlight = 0;       // Number of transfers that are in progress

    requestSize = 16;           // Request size (number of packets)
    packetSize = 1024;          // Maximum packet size for the endpoint
}

void QUsbBulkTransfer::run()
{
    int transferReturnStatus = 0; // Status return from libUSB
    qDebug() << "usbBulkTransfer::run(): Started";

    struct libusb_transfer **transfers = NULL;		// List of transfer structures.
    unsigned char **dataBuffers = NULL;			// List of data buffers.

    // Check for validity of the device handle
    if (bDevHandle == NULL) {
        qDebug() << "QUsbBulkTransfer::run(): Invalid device handle";
        return;
    }

    // Allocate buffers and transfer structures
    bool allocFail = false;

    dataBuffers = (unsigned char **)calloc (queueDepth, sizeof (unsigned char *));
    transfers   = (struct libusb_transfer **)calloc (queueDepth, sizeof (struct libusb_transfer *));

    if ((dataBuffers != NULL) && (transfers != NULL)) {

        for (unsigned int i = 0; i < queueDepth; i++) {

            dataBuffers[i] = (unsigned char *)malloc (requestSize * packetSize);
            transfers[i]   = libusb_alloc_transfer(0);

            if ((dataBuffers[i] == NULL) || (transfers[i] == NULL)) {
                allocFail = true;
                break;
            }
        }

    } else {
        allocFail = true;
    }

    // Check if all memory allocations have succeeded
    if (allocFail) {
        qDebug() << "QUsbBulkTransfer::run(): Could not allocate required buffers";
        freeTransferBuffers(dataBuffers, transfers);
        return;
    }

    // Record the timestamp at start of transfer (for statistics generation)
    gettimeofday(&startTimestamp, NULL); // Might be better to use the QT library for this?

    // Launch the transfers
    for (unsigned int launchCounter = 0; launchCounter < queueDepth; launchCounter++) {
        qDebug() << "QUsbBulkTransfer::run(): Launching transfer number" << launchCounter << "of" << queueDepth;

        libusb_fill_bulk_transfer (transfers[launchCounter], bDevHandle, bEndPoint,
            dataBuffers[launchCounter], requestSize * packetSize, bulkTransferCallback, NULL, 5000);

        qDebug() << "transferThread::run(): Performing libusb_submit_transfer";
        transferReturnStatus = libusb_submit_transfer(transfers[launchCounter]);
        if (transferReturnStatus == 0) {
            requestsInFlight++;
            qDebug() << "transferThread::run(): Transfer launched";
        } else {
            qDebug() << "transferThread::run(): Transfer failed!";
        }
    }

    // Show the current number of inflight requests
    qDebug() << "QUsbBulkTransfer::run(): Queued" << requestsInFlight << "requests";

    // NEED TO RECODE THIS BIT... NOT VERY NEAT...
    struct timeval t1, t2, timeout;
    gettimeofday(&t1, NULL);

    // Use a 1 second timeout for the libusb_handle_events_timeout call
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    // Process libUSB events whilst transfer is running
    do {
        libusb_handle_events_timeout(bContext, &timeout);

        // Calculate statistics every half second
        gettimeofday(&t2, NULL);
        if (t2.tv_sec > t1.tv_sec) {
            // REDO THIS BIT, updates cannot be driven from here...
            //updateResults();
            t1 = t2;
        }
    } while (!stopTransfers);

    qDebug() << "transferThread::run(): Stopping streamer thread";

    // Process libUSB events whilst the transfer is stopping
    while (requestsInFlight != 0) {
        qDebug() << "QUsbBulkTransfer::run():" << requestsInFlight << "requests are pending";
        libusb_handle_events_timeout(bContext, &timeout);
        sleep(1);
    }

    // Free up the transfer buffers
    freeTransferBuffers(dataBuffers, transfers);

    // All done
    qDebug() << "usbBulkTransfer::run(): Stopped";
}

// This is the call back function called by libusb upon completion of a queued data transfer.
static void LIBUSB_CALL bulkTransferCallback(struct libusb_transfer *transfer)
{
    unsigned int elapsedTime;
    double       performance;
    int          size = 0;

    // Check if the transfer has succeeded.
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        // Transfer has failed
        failureCount++;

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
        size = requestSize * packetSize;
        successCount++;
        //qDebug() << "bulkTransferCallback(): Successful transfer reported";
    }

    // Update the actual transfer size for this request.
    transferSize += size;

    // Print the transfer statistics when queueDepth transfers are completed.
    transferIndex++;
    if (transferIndex == queueDepth) {

        gettimeofday (&endTimestamp, NULL);
        elapsedTime = ((endTimestamp.tv_sec - startTimestamp.tv_sec) * 1000000 +
            (endTimestamp.tv_usec - startTimestamp.tv_usec));

        // Calculate the performance in KBps.
        performance    = (((double)transferSize / 1024) / ((double)elapsedTime / 1000000));
        transferPerformance  = (unsigned int)performance;

        transferIndex = 0;
        transferSize  = 0;
        startTimestamp = endTimestamp;
    }

    // Reduce the number of requests in flight.
    requestsInFlight--;

    // Prepare and re-submit the read request.
    if (!stopTransfers) {

        // If a transfer fails, ignore the error
        if (libusb_submit_transfer(transfer) == 0) requestsInFlight++;
    }
}

// Function to free data buffers and transfer structures
void QUsbBulkTransfer::freeTransferBuffers(unsigned char **dataBuffers, struct libusb_transfer **transfers)
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

// Return the current value of the success counter
quint32 QUsbBulkTransfer::getSuccessCounter(void)
{
    return (quint32)successCount;
}

// Return the current value of the failure counter
quint32 QUsbBulkTransfer::getFailureCounter(void)
{
    return (quint32)failureCount;
}

// Return the current transfer performance value
quint32 QUsbBulkTransfer::getTransferPerformance(void)
{
    return (quint32)transferPerformance;
}
