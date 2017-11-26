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

// The number of simulataneous transfer requests allowed
#define QUEUEDEPTH  16

// Private variables for storing transfer configuration
static unsigned int    requestSize;            // Request size in number of packets
static unsigned int    packetSize;             // Maximum packet size for the endpoint
static unsigned int    transferId[QUEUEDEPTH]; // Transfer ID tracking array (used as user_data to track transfers)
static unsigned int    queueDepth;             // Maximum number of queued transfers allowed

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

// Private variables for data checking (test mode)
static bool             testModeFlag;          // Test mode status flag

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

void QUsbBulkTransfer::setup(libusb_context* mCtx, libusb_device_handle* devHandle, quint8 endPoint, bool testMode)
{
    // Store the device handle locally
    bContext = mCtx;
    bDevHandle = devHandle;
    bEndPoint = endPoint;

    queueDepth = QUEUEDEPTH; // The maximum number of simultaneous transfers

    // Default all status variables
    successCount = 0;           // Number of successful transfers
    failureCount = 0;           // Number of failed transfers
    transferSize = 0;           // Size of data transfers performed so far
    transferIndex = 0;          // Write index into the transfer_size array
    transferPerformance = 0;	// Performance in KBps
    requestsInFlight = 0;       // Number of transfers that are in progress

    requestSize = 16;           // Request size (number of packets)
    packetSize = 1024;          // Maximum packet size for the endpoint

    // Set up the transfer identifier array
    for (quint32 counter = 0; counter < queueDepth; counter++) {
        transferId[counter] = counter;
    }

    // Set the test mode flag
    testModeFlag = testMode;

    if (testModeFlag) {
        qDebug() << "usbBulkTransfer::setup(): Test mode is ON";
    }
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

        libusb_fill_bulk_transfer(transfers[launchCounter], bDevHandle, bEndPoint,
            dataBuffers[launchCounter], requestSize * packetSize, bulkTransferCallback, &transferId[launchCounter], 5000);

        qDebug() << "QUsbBulkTransfer::run(): Performing libusb_submit_transfer";
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

    // Use a 1 second timeout for the libusb_handle_events_timeout call
    struct timeval timeout;
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    // Process libUSB events whilst transfer is running
    do {
        libusb_handle_events_timeout(bContext, &timeout);
    } while (!stopTransfers);

    qDebug() << "transferThread::run(): Stopping streamer thread";

    // Process libUSB events whilst the transfer is stopping
    while (requestsInFlight != 0) {
        qDebug() << "QUsbBulkTransfer::run(): Stopping -" << requestsInFlight << "requests are pending";
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

        // If test mode is on we can verify that the data read by the transfer is a sequence of
        // numbers.  Note that the data is a 10-bit counter (0-1023) which is scaled by the
        // FPGA to a 16-bit (signed) range before being sent (and we have to allow for that here)
        if (testModeFlag) {
            // Test mode is on:
            // Here we verify that the individual transfer is good (not the entire request)

            // Verify that the amount of data is the same as the expected buffer size
            if (transfer->length != size) {
                qDebug() << "bulkTransferCallback(): Buffer length is incorrect = " << transfer->length;
            }

            quint8 highByte = 0;
            quint8 lowByte = 0;
            qint32 currentWord = 0;
            qint32 previousWord = 0;
            bool testPassed = true;

            // Dereference the buffer pointer
            unsigned char *dataBuffer = (unsigned char *)transfer->buffer;

            for (int bytePointer = 0; bytePointer < transfer->length; bytePointer += 2) {
                // Get two bytes of data (that represent our signed little-endian 16-bit number
                lowByte  = dataBuffer[bytePointer];
                highByte = dataBuffer[bytePointer + 1];

                // Convert the bytes into a signed 16-bit value
                currentWord = lowByte + (highByte << 8);

                // Convert into a 10-bit value
                currentWord = currentWord / 64;

                // Ensure we are not at the start of the buffer
                if (bytePointer != 0) {
                    // Is received data correct?
                    if (currentWord == (previousWord + 1)) {
                        // Test was successful
                    } else {
                        if (currentWord == 0 && previousWord == 1023) {
                            // Failure was due to wrap around
                            // Test was successful
                        } else {
                            // Test failed
                            testPassed = false;
                            qDebug() << "bulkTransferCallback(): Transfer failed test mode integrity check!";
                            qDebug() << "Current word =" << currentWord << ": Previous word =" << previousWord;
                        }
                    }
                }

                // Store the current word as previous for next check
                previousWord = currentWord;
            }

            // Update the counters
            if (testPassed) successCount++; else failureCount++;
        } else {
            // We can verify the contents if test mode is off, so we just increment
            // the success counter
            successCount++;
        }

        // *transferUserData will equal the value assigned to transferId[transferNumber] from
        // the transfer launch (allowing us to known where in the sequence the transfer data
        // belongs
        //unsigned int *transferUserData = (unsigned int *)transfer->user_data;
        //qDebug() << "bulkTransferCallback(): Successful transfer ID" << *transferUserData << "reported";
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
