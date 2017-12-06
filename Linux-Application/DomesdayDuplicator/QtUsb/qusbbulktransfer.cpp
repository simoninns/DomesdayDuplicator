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

// The number of simulataneous USB transfer requests allowed
// Note: increasing this automatically causes the disk buffers
// to increase accordingly
#define QUEUEDEPTH  16

// Private variables for storing transfer configuration
static unsigned int    requestSize;            // Request size in number of packets
static unsigned int    packetSize;             // Maximum packet size for the endpoint
static unsigned int    queueDepth;             // Maximum number of queued transfers allowed

// Private variables for tracking disk writes
// Note: Each disk buffer is QUEUEDEPTH transfers
// i.e. if the queue depth is 16 and the transfer size is (16 * 1024) bytes
// (16Kbytes) each disk buffer will be 16K, so 8 * 16K = 256K in total
#define DISKBUFFERDEPTH 8

struct transferIdStruct {
    unsigned int transferNumber;               // The position of the transfer within the disk buffer
    unsigned int bufferNumber;                 // The disk buffer to which the transfer belongs
};
static transferIdStruct transferId[QUEUEDEPTH];

static unsigned int diskBufferDepth;           // The number of available disk buffers

// Disk buffer tracking variables and defines
static unsigned int diskBufferFill[DISKBUFFERDEPTH];    // The current fill level of the buffer
static unsigned int diskBufferState[DISKBUFFERDEPTH];   // The current state of the buffer
static unsigned int nextDiskBufferToWrite;              // The next buffer to be written to disk

unsigned char **diskBuffers = NULL;			// List of disk write buffers.

#define DBSTATE_FILLING    0
#define DBSTATE_WAITINGTOWRITE 1

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

// Buffer for disk write
static QFile* outputFile;

struct libusb_transfer **usbTransfers = NULL;		// List of transfer structures.
unsigned char **usbDataBuffers = NULL;             // List of USB data buffers.

// File name
static QString captureFileName;

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

void QUsbBulkTransfer::setup(libusb_context* mCtx, libusb_device_handle* devHandle, quint8 endPoint, bool testMode, QString fileName)
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

    // Set the fileName
    captureFileName = fileName;
}

void QUsbBulkTransfer::run()
{
    int transferReturnStatus = 0; // Status return from libUSB
    qDebug() << "usbBulkTransfer::run(): Started";

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
    transfersInFlight = 0;      // Number of transfers that are in progress

    diskFailureCount = 0;       // Number of failed disk writes

    requestSize = 16;           // Request size (number of packets)
    packetSize = 1024;          // Maximum packet size for the endpoint

    // Set up the disk buffering
    diskBufferDepth = DISKBUFFERDEPTH;
    nextDiskBufferToWrite = 0;

    // Open the disk output file for writing
    // NOTE: FILENAME IS FIXED.  This is just for testing purposes!
    outputFile = new QFile(captureFileName);
    if(!outputFile->open(QFile::WriteOnly | QFile::Truncate)) {
        qDebug() << "QUsbBulkTransfer::run(): Could not open destination file for writing!";
        return;
    }

    for (unsigned int bufferNum = 0; bufferNum < diskBufferDepth; bufferNum++) {
        diskBufferFill[bufferNum] = 0;
        diskBufferState[bufferNum] = DBSTATE_FILLING;
    }

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
        freeTransferBuffers(usbDataBuffers, usbTransfers);
        return;
    }

    // Allocate disk buffer for writing (each disk buffer stores 'queueDepth' transfers)
    allocFail = false;
    diskBuffers = (unsigned char **)calloc (diskBufferDepth, sizeof (unsigned char *));

    if (diskBuffers != NULL) {

        for (unsigned int i = 0; i < diskBufferDepth; i++) {
            diskBuffers[i] = (unsigned char *)malloc (requestSize * packetSize * queueDepth);

            if (diskBuffers[i] == NULL) {
                allocFail = true;
                break;
            }
        }

    } else {
        allocFail = true;
    }

    // Check if all memory allocations have succeeded
    if (allocFail) {
        qDebug() << "QUsbBulkTransfer::run(): Could not allocate required buffers for disk write";
        freeTransferBuffers(usbDataBuffers, usbTransfers);
        freeDiskBuffers(diskBuffers);
        return;
    }

    // Record the timestamp at start of transfer (for statistics generation)
    gettimeofday(&startTimestamp, NULL);

    // Set up the initial transfers and target the current disk buffer (0)
    for (unsigned int currentTransferNumber = 0; currentTransferNumber < queueDepth; currentTransferNumber++) {
        // Set up the transfer identifier (sent via user data to the callback)
        transferId[currentTransferNumber].bufferNumber = 0; // Always start with the first buffer
        transferId[currentTransferNumber].transferNumber = currentTransferNumber;

        // Set transfer flag to cause transfer error if there is a short packet
        usbTransfers[currentTransferNumber]->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;

        libusb_fill_bulk_transfer(usbTransfers[currentTransferNumber], bDevHandle, bEndPoint,
            usbDataBuffers[currentTransferNumber], (requestSize * packetSize), bulkTransferCallback, &transferId[currentTransferNumber], 5000);
    }

    // Submit the transfers
    for (unsigned int currentTransferNumber = 0; currentTransferNumber < queueDepth; currentTransferNumber++) {
        transferReturnStatus = libusb_submit_transfer(usbTransfers[currentTransferNumber]);
        if (transferReturnStatus == 0) {
            transfersInFlight++;
            //qDebug() << "transferThread::run(): Transfer" << transfersInFlight << "launched";
        } else {
            qDebug() << "transferThread::run(): Transfer launch" << transfersInFlight << "failed!";
        }
    }

    // Show the current number of inflight transfers
    qDebug() << "QUsbBulkTransfer::run(): Queued" << transfersInFlight << "transfers";

    // Use a 1 second timeout for the libusb_handle_events_timeout call
    struct timeval timeout;
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    // Process libUSB events whilst transfers are in flight
    do {
        libusb_handle_events_timeout(bContext, &timeout);
        writeBuffersToDisk();
    } while (!stopTransfers);

    qDebug() << "transferThread::run(): Stopping streamer thread";

    // Process libUSB events whilst the transfer is stopping
    while (transfersInFlight != 0) {
        qDebug() << "QUsbBulkTransfer::run(): Stopping -" << transfersInFlight << "transfers are pending";
        libusb_handle_events_timeout(bContext, &timeout);
        writeBuffersToDisk();
        usleep(200);
    }

    // Close the output file
    outputFile->close();

    // Free up the transfer buffers
    freeTransferBuffers(usbDataBuffers, usbTransfers);

    // Free up the disk write buffers
    freeDiskBuffers(diskBuffers);

    // All done
    qDebug() << "usbBulkTransfer::run(): Stopped";
}

// This is the call back function called by libusb upon completion of a queued data transfer.
static void LIBUSB_CALL bulkTransferCallback(struct libusb_transfer *transfer)
{
    unsigned int elapsedTime;
    double       performance;

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
        successCount++;
    }

    // Update the transfer size for the whole bulk transfer so far
    transferSize += requestSize * packetSize;

    // Reduce the number of requests in-flight.
    transfersInFlight--;

    // Get the details of the current thread's transferID
    transferIdStruct *transferUserData = (transferIdStruct *)transfer->user_data;
    unsigned int threadDiskBuffer = transferUserData->bufferNumber;
    unsigned int threadTransferNumber = transferUserData->transferNumber;

    // Disk buffer handling -----------------------------------------------------------------------------------

    // Copy the USB data transfer (16K) to the disk write buffer
    // As the disk buffer contains 16 (queueDepth) transfers the buffer offset must
    // be calculated as (16 * 1024) * transferNumber (of 0-15) to get the right offset
    // in the 256Kbyte disk buffer
    if (diskBufferState[threadDiskBuffer] == DBSTATE_FILLING) {
        memcpy(diskBuffers[threadDiskBuffer] + ((requestSize * packetSize) * threadTransferNumber), transfer->buffer, (requestSize * packetSize));
    } else {
        qDebug() << "bulkTransferCallback(): threadDiskBuffer not in FILLING state! DATA LOST - BUFFER OVERFLOW!";
    }

    // Increment the disk buffer fill level
    diskBufferFill[threadDiskBuffer]++;

    // Was this the last data transfer needed to fill the disk buffer?
    if (diskBufferFill[threadDiskBuffer] == queueDepth) {
        // Disk buffer is now full - Mark it ready for writing
        diskBufferState[threadDiskBuffer] = DBSTATE_WAITINGTOWRITE;

        // Calculate the transfer statistics (just for user feedback)
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

    // Resubmit the USB transfer request -----------------------------------------------------------------------

    // Prepare and re-submit the transfer request (unless stopTransfers has been flagged)
    if (!stopTransfers) {
        // Transfer is still running, so here we resubmit the transfer
        // but update the user_data information to target the next disk buffer
        transferId[threadTransferNumber].bufferNumber = threadDiskBuffer + 1;
        if (transferId[threadTransferNumber].bufferNumber == diskBufferDepth)
            transferId[threadTransferNumber].bufferNumber = 0;

        // This next command isn't really required... the thread always has the same
        // transfer number
        transferId[threadTransferNumber].transferNumber = threadTransferNumber;

        // Fill and resubmit the transfer
        libusb_fill_bulk_transfer(transfer, bDevHandle, bEndPoint,
            usbDataBuffers[threadTransferNumber], (requestSize * packetSize), bulkTransferCallback, &transferId[threadTransferNumber], 5000);

        // If a transfer fails, ignore the error - SHOULD CATCH THIS - TO-DO
        if (libusb_submit_transfer(transfer) == 0) transfersInFlight++;
        else qDebug() << "bulkTransferCallback(): libusb_submit_transfer failed!";
    }
}

// Function to write waiting buffers to disk
void QUsbBulkTransfer::writeBuffersToDisk(void)
{
    qint32 errorCount = 0;

    // Note: Disk buffers must be written in the correct order, so this function
    //   cycles through the disk buffers in numerical order.

    // Check if the next disk buffer to be written is waiting to write
    if (diskBufferState[nextDiskBufferToWrite] == DBSTATE_WAITINGTOWRITE) {
        // Buffer is waiting to be written, write it to disk...

        // If we are in test mode, check the integrity of the disk buffer's data
        if (testModeFlag) {
            quint8 highByte = 0;
            quint8 lowByte = 0;
            qint32 currentWord = 0;
            qint32 previousWord = 0;

            for (quint64 bytePointer = 0; bytePointer < (requestSize * packetSize * queueDepth); bytePointer += 2) {
                // Get two bytes of data (that represent our signed little-endian 16-bit number
                lowByte  = diskBuffers[nextDiskBufferToWrite][bytePointer];
                highByte = diskBuffers[nextDiskBufferToWrite][bytePointer + 1];

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
                            qDebug() << "QUsbBulkTransfer::writeBuffersToDisk(): FAIL - Current word =" << currentWord <<
                                        ": Previous word =" << previousWord << ": Position =" << bytePointer <<
                                        "  Disk buffer =" << nextDiskBufferToWrite << "  transfer =" << bytePointer / (requestSize * packetSize);

                            errorCount++;
                        }
                    }
                }

                // Store the current word as previous for next check
                previousWord = currentWord;
            }
        }

        // Write the buffer to disk
        outputFile->write((const char *)diskBuffers[nextDiskBufferToWrite], (requestSize * packetSize * queueDepth));
        if (errorCount >0) {
            qDebug() << "QUsbBulkTransfer::writeBuffersToDisk(): Buffer" << nextDiskBufferToWrite << "written to disk with" <<
                        errorCount << "errors";
            diskFailureCount++;
        }

        // Mark buffer as ready for filling
        diskBufferFill[nextDiskBufferToWrite] = 0; // Buffer is now 'empty'
        diskBufferState[nextDiskBufferToWrite] = DBSTATE_FILLING;

        // Point to the next buffer for writing (and check for pointer overflow)
        nextDiskBufferToWrite++;
        if (nextDiskBufferToWrite == diskBufferDepth) nextDiskBufferToWrite = 0;
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

// Function to free disk transfer buffers
void QUsbBulkTransfer::freeDiskBuffers(unsigned char **diskBuffers)
{
    // Free up any allocated disk write buffers
    if (diskBuffers != NULL) {
        for (unsigned int i = 0; i < diskBufferDepth; i++) {
            if (diskBuffers[i] != NULL) {
                free(diskBuffers[i]);
            }
            diskBuffers[i] = NULL;
        }
        free(diskBuffers);
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
