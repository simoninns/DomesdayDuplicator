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
static unsigned int    queueDepth;             // Maximum number of queued transfers allowed

// Private variables for tracking transfers
#define DISKBUFFERDEPTH 8

struct transferIdStruct {
    unsigned int transferNumber;               // The position of the transfer within the disk buffer
    unsigned int bufferNumber;                 // The disk buffer to which the transfer belongs
};
static transferIdStruct transferId[QUEUEDEPTH];

static unsigned int currentDiskBuffer;         // The currently targeted disk number
static unsigned int currentTransferNumber;     // The current transfer number
static unsigned int diskBufferDepth;

// Disk buffer tracking variables and defines
static unsigned int diskBufferFill[DISKBUFFERDEPTH];
static unsigned int diskBufferState[DISKBUFFERDEPTH];
static unsigned int nextDiskBufferToWrite;

#define DBSTATE_FILLING    0
#define DBSTATE_WAITINGTOWRITE 1
#define DBSTATE_WRITING 2

// Private variables used to report on the transfer
static unsigned int    successCount;           // Number of successful transfers
static unsigned int    failureCount;           // Number of failed transfers
static unsigned int    transferPerformance;    // Performance in Kilobytes per second
static unsigned int    transferSize;           // Size of data transfers performed so far
static unsigned int    diskFailureCount;       // Number of disk write failures

// Private variables used to control the transfer
static unsigned int    transferIndex;          // Write index into the transfer_size array
static volatile int    transfersInFlight;       // Number of transfers that are in progress
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

    // Reset transfer tracking ready for the new run...

    // Default all status variables
    successCount = 0;           // Number of successful transfers
    failureCount = 0;           // Number of failed transfers
    transferSize = 0;           // Size of data transfers performed so far
    transferIndex = 0;          // Write index into the transfer_size array
    transferPerformance = 0;	// Performance in KBps
    transfersInFlight = 0;       // Number of transfers that are in progress

    diskFailureCount = 0;       // Number of failed disk writes

    requestSize = 16;           // Request size (number of packets)
    packetSize = 1024;          // Maximum packet size for the endpoint

    // Set up the disk buffering
    currentDiskBuffer = 0;
    currentTransferNumber = 0;
    diskBufferDepth = DISKBUFFERDEPTH;
    nextDiskBufferToWrite = 0;

    for (unsigned int bufferNum = 0; bufferNum < DISKBUFFERDEPTH; bufferNum++) {
        diskBufferFill[bufferNum] = 0;
        diskBufferState[bufferNum] = DBSTATE_FILLING;
    }

    // Allocate buffers and transfer structures for USB->PC communication
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

    // Set up the initial transfers
    for (unsigned int launchCounter = 0; launchCounter < queueDepth; launchCounter++) {
        // Set up the transfer identifier
        transferId[launchCounter].bufferNumber = currentDiskBuffer;
        transferId[launchCounter].transferNumber = currentTransferNumber;

        libusb_fill_bulk_transfer(transfers[launchCounter], bDevHandle, bEndPoint,
            dataBuffers[launchCounter], requestSize * packetSize, bulkTransferCallback, &transferId[launchCounter], 5000);

        // Next transfer number
        currentTransferNumber++;
    }

    // Reset the bufferNumber and increment the currentDiskBuffer
    currentTransferNumber = 0;
    currentDiskBuffer++;

    // Submit the transfers
    for (unsigned int launchCounter = 0; launchCounter < queueDepth; launchCounter++) {
        qDebug() << "QUsbBulkTransfer::run(): Submitting transfer number" << launchCounter << "of" << queueDepth;
        qDebug() << " bufferNumber =" << transferId[launchCounter].bufferNumber << "transferNumber =" << transferId[launchCounter].transferNumber;

        transferReturnStatus = libusb_submit_transfer(transfers[launchCounter]);
        if (transferReturnStatus == 0) {
            transfersInFlight++;
            //qDebug() << "transferThread::run(): Transfer launched";
        } else {
            qDebug() << "transferThread::run(): Transfer failed!";
        }
    }

    // Show the current number of inflight transfers
    qDebug() << "QUsbBulkTransfer::run(): Queued" << transfersInFlight << "transfers";

    // Use a 1 second timeout for the libusb_handle_events_timeout call
    struct timeval timeout;
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    // Process libUSB events whilst transfers are running
    do {
        libusb_handle_events_timeout(bContext, &timeout);
    } while (!stopTransfers);

    qDebug() << "transferThread::run(): Stopping streamer thread";

    // Process libUSB events whilst the transfer is stopping
    while (transfersInFlight != 0) {
        qDebug() << "QUsbBulkTransfer::run(): Stopping -" << transfersInFlight << "transfers are pending";
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

        // Calculate the amount of data we should have received:
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
    }

    // Update the transfer size for the whole bulk transfer so far
    transferSize += size;

    // Calculate the transfer statistics when queueDepth transfers are completed.
    transferIndex++;
    if (transferIndex == queueDepth) {

        gettimeofday (&endTimestamp, NULL);
        elapsedTime = ((endTimestamp.tv_sec - startTimestamp.tv_sec) * 1000000 +
            (endTimestamp.tv_usec - startTimestamp.tv_usec));

        // Calculate the performance in Kilo bytes per second
        performance    = (((double)transferSize / 1024) / ((double)elapsedTime / 1000000));
        transferPerformance  = (unsigned int)performance;

        transferIndex = 0;
        transferSize  = 0;
        startTimestamp = endTimestamp;
    }

    // Reduce the number of requests in-flight.
    transfersInFlight--;

    // Read the user data field in the transfer struction to work out
    // where the data belongs...
    transferIdStruct *transferUserData = (transferIdStruct *)transfer->user_data;
    //qDebug() << "bulkTransferCallback(): Recieved buffer:" << transferUserData->bufferNumber << " transfer:" << transferUserData->transferNumber;

    // STORE THE DATA IN A DISK BUFFER HERE - TO-DO!
    unsigned int threadDiskBuffer = transferId[currentTransferNumber].bufferNumber; // Take a copy for later
    unsigned int threadTransferNumber = transferId[currentTransferNumber].transferNumber; // Take a copy for later

    // Prepare and re-submit the transfer request
    if (!stopTransfers) {
        // Transfer is still running, so here we resubmit the transfer
        // but update the user_data information to target the next disk buffer

        transferId[currentTransferNumber].bufferNumber = currentDiskBuffer;
        transferId[currentTransferNumber].transferNumber = currentTransferNumber;

        //qDebug() << "bulkTransferCallback(): Filling buffer:" <<  currentDiskBuffer << " and transfer:" << currentTransferNumber;

        // Reuse the existing buffer space for the transfer and update user_data
        libusb_fill_bulk_transfer(transfer, bDevHandle, bEndPoint,
            transfer->buffer, requestSize * packetSize, bulkTransferCallback, &transferId[currentTransferNumber], 5000);

        // Increment the transfer and disk buffers
        currentTransferNumber++;
        // Check for transfer number overflow
        if (currentTransferNumber == queueDepth) {
            currentTransferNumber = 0;
            currentDiskBuffer++;
            // Check for disk buffer number overflow
            if (currentDiskBuffer == diskBufferDepth) currentDiskBuffer = 0;
        }

        // If a transfer fails, ignore the error - SHOULD CATCH THIS - TO-DO
        if (libusb_submit_transfer(transfer) == 0) transfersInFlight++;
        else qDebug() << "bulkTransferCallback(): libusb_submit_transfer failed!";
    }

    // DISK BUFFER HANDLING HERE ----->

    // Recover the current disk buffer and transfer number

    // Register that the request is now stored in the disk buffer
    //qDebug() << "Initial diskBufferFill[" << threadDiskBuffer << "] =" << diskBufferFill[threadDiskBuffer];
    if (diskBufferFill[threadDiskBuffer] < queueDepth) {
        diskBufferFill[threadDiskBuffer]++;
    } else {
        // The target disk buffer is already full!
        qDebug() << "bulkTransferCallback(): Disk buffer overflow - DATA LOST! Disk buffer =" << threadDiskBuffer << "transferNumber =" << threadTransferNumber;
        qDebug() << "  diskBufferState =" << diskBufferState[threadDiskBuffer] << "with queueDepth of" << queueDepth;
        qDebug() << "  diskBufferFill =" << diskBufferFill[threadDiskBuffer];
        diskFailureCount++;
    }

    // Is the current buffer now full?
    if (diskBufferFill[threadDiskBuffer] == queueDepth) {
        // Buffer fill is complete... mark the buffer as waiting for write:
        diskBufferState[threadDiskBuffer] = DBSTATE_WAITINGTOWRITE;
    }

    // DISK BUFFER WRITING HERE ----->

    // Check if the next disk buffer to be written is ready for writing
    // Note: This is to ensure that the disk buffers are always written in sequence
    if (diskBufferState[nextDiskBufferToWrite] == DBSTATE_WAITINGTOWRITE) {
        // Buffer is ready, write it to disk

        // Get the buffer ready for writing
        diskBufferState[nextDiskBufferToWrite] = DBSTATE_WRITING;
        diskBufferFill[nextDiskBufferToWrite] = 0;

        // Point to the next buffer
        nextDiskBufferToWrite++;
        if (nextDiskBufferToWrite == DISKBUFFERDEPTH) nextDiskBufferToWrite = 0;

        // Just debug for now
        //qDebug() << "bulkTransferCallback(): Writing disk buffer" << threadDiskBuffer;

        // Mark buffer as ready for filling
        diskBufferState[nextDiskBufferToWrite] = DBSTATE_FILLING;
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

// Return the current value of the disk write failure counter
quint32 QUsbBulkTransfer::getDiskFailureCounter(void)
{
    return (quint32)diskFailureCount;
}
