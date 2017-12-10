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

// Disk buffer write thread function prototypes
static void processDiskBuffers(void);
static void testDiskBuffer(quint32 diskBufferNumber);

// The following statically defined variables are used by the libUSB
// transfer callback

// Private variables used to control the transfer callbacks
static quint32 transfersInFlight;   // Number of transfers that are in progress
static bool stopUsbTransfers;       // USB transfer stop flag

// Structure to hold libUSB transfer user data
struct transferUserDataStruct {
    quint32 transferNumber;         // (disk buffering) The transfer number of the transfer (within the queue)
    quint32 maximumTransferNumber;  // (disk buffering) The maximum transfer number allowed (within a queue)
    quint32 queueNumber;            // (disk buffering) The queue number that the transfer belongs to
    quint32 diskBufferNumber;       // (disk buffering) The disk buffer that the transfer belongs to
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

// Notes:
//
// The ADC generates data at a rate of around 64Mbytes/sec
//
// The REQUEST_SIZE, PACKET_SIZE and QUEUE_DEPTH relate to the libUSB transfers
// The NUMBER_OF_DISK_BUFFERS and QUEUE_BUFFERS_PER_DISK_BUFFER relate to the disk write functions
//
// Every libUSB transfer thread requests REQUEST_SIZE * PACKET_SIZE bytes from the USB library
// The QUEUE_DEPTH controls the maximum number of thread requests allowed at any time
// The size of the queue is (REQUEST_SIZE * PACKET_SIZE) * QUEUE_DEPTH
//
// QUEUE_BUFFERS_PER_DISK_BUFFER defines the number of queues stored in each disk buffer
// So the size of a disk buffer is defined by:
//   ((REQUEST_SIZE * PACKET_SIZE) * QUEUE_DEPTH) * QUEUE_BUFFERS_PER_DISK_BUFFER
//
// The number of available disk buffers is set by NUMBER_OF_DISK_BUFFERS so the total amount
// of available disk buffer space is defined by:
//   (((REQUEST_SIZE * PACKET_SIZE) * QUEUE_DEPTH) * QUEUE_BUFFERS_PER_DISK_BUFFER) * NUMBER_OF_DISK_BUFFERS
//
// It is recommended that PACKET_SIZE is equal to the endpoint burst length (16Kbytes)
//
// The settings below give the following results:
//
// Size of request per libUSB transfer request thread =
//   (16 * 16384) = 256Kbytes
//
// Size of transfer queue =
//   (256Kbytes) * 16 = 4Mbytes
//
// Size of each disk buffer =
//   4MBytes * 32 = 128Mbytes
//
// Total size of disk buffers =
//   128Mbytes * 4 = 512Mbytes
//
#define REQUEST_SIZE 16
#define PACKET_SIZE 16384
#define QUEUE_DEPTH 16
#define NUMBER_OF_DISK_BUFFERS 4
#define QUEUE_BUFFERS_PER_DISK_BUFFER 32

// FPGA test mode status flag
static bool testModeFlag;

// Disk buffer variables
static unsigned char **diskDataBuffers = NULL; // List of disk data buffers
static quint32 numberOfDiskBuffers;
static quint32 queueBuffersPerDiskBuffer;
static bool diskBufferStatus[NUMBER_OF_DISK_BUFFERS];

// Disk IO
static QFile* outputFile;

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
    queueDepth = QUEUE_DEPTH;

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
    struct libusb_transfer **usbTransfers = NULL;           // List of transfer structures
    unsigned char **usbDataBuffers = NULL;                  // List of USB data buffers
    transferUserDataStruct transferUserData[queueDepth];    // Transfer user data

    int transferReturnStatus = 0; // Status return from libUSB
    qDebug() << "usbBulkTransfer::run(): Started";

    // Check for validity of the device handle
    if (libUsbDeviceHandle == NULL) {
        qDebug() << "QUsbBulkTransfer::run(): Invalid device handle";
        return;
    }

    // Reset transfer tracking ready for the new run...

    // Reset transfer statistics (globals)
    statistics.successCount = 0;
    statistics.failureCount = 0;
    statistics.transferSize = 0;
    statistics.transferPerformance = 0;
    statistics.diskFailureCount = 0;
    gettimeofday(&statistics.startTimestamp, NULL);

    // Reset transfer variables
    transfersInFlight = 0;      // Number of transfers that are in progress (global)
    requestSize = REQUEST_SIZE; // Request size (number of packets)
    packetSize = PACKET_SIZE;   // Maximum packet size for the endpoint (in bytes)

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

    // Allocate disk buffers
    allocateDiskBuffers();

    // Open the capture file for writing
    outputFile = new QFile(captureFileName);
        if(!outputFile->open(QFile::WriteOnly | QFile::Truncate)) {
            qDebug() << "QUsbBulkTransfer::run(): Could not open destination file for writing!";
            return;
    }

    // Create a thread for writing disk buffers to disk
    QFuture<void> future = QtConcurrent::run( processDiskBuffers );

    // Set up the initial transfer requests
    for (unsigned int currentTransferNumber = 0; currentTransferNumber < queueDepth; currentTransferNumber++) {

        // Set up the user data for disk buffering
        transferUserData[currentTransferNumber].queueNumber = 0;
        transferUserData[currentTransferNumber].diskBufferNumber = 0;
        transferUserData[currentTransferNumber].transferNumber = currentTransferNumber;
        transferUserData[currentTransferNumber].maximumTransferNumber = queueDepth - 1;

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

    // Close the capture file
    outputFile->close();

    // Free up the transfer buffers
    freeUsbTransferBuffers(usbDataBuffers, usbTransfers);

    // Free up the disk buffers
    freeDiskBuffers();

    // All done
    qDebug() << "usbBulkTransfer::run(): Stopped";
}

// Function to free USB data buffers and transfer structures
void QUsbBulkTransfer::freeUsbTransferBuffers(unsigned char **dataBuffers, struct libusb_transfer **transfers)
{
    // Free up any allocated transfer structures
    if (transfers != NULL) {
        for (quint32 i = 0; i < queueDepth; i++) {
            if (transfers[i] != NULL) {
                libusb_free_transfer(transfers[i]);
            }
            transfers[i] = NULL;
        }
        free (transfers);
    }

    // Free up any allocated data buffers
    if (dataBuffers != NULL) {
        for (quint32 i = 0; i < queueDepth; i++) {
            if (dataBuffers[i] != NULL) {
                free(dataBuffers[i]);
            }
            dataBuffers[i] = NULL;
        }
        free(dataBuffers);
    }
}

// Disk buffer functions -------------------------------------------------------------

// Allocate memory for the disk buffers
void QUsbBulkTransfer::allocateDiskBuffers(void)
{
    // Allocate buffers for disk buffering
    bool allocFail = false;

    // Set the size of the disk buffers
    quint32 queueSize = (requestSize * packetSize) * queueDepth;
    queueBuffersPerDiskBuffer = QUEUE_BUFFERS_PER_DISK_BUFFER;
    numberOfDiskBuffers = NUMBER_OF_DISK_BUFFERS;

    diskDataBuffers = (unsigned char **)calloc(numberOfDiskBuffers, sizeof (unsigned char *));
    if (diskDataBuffers != NULL) {
        for (quint32 i = 0; i < numberOfDiskBuffers; i++) {

            diskDataBuffers[i] = (unsigned char *)malloc(queueSize * queueBuffersPerDiskBuffer);
            diskBufferStatus[i] = false; // Status = Not writing

            if (diskDataBuffers[i] == NULL) {
                allocFail = true;
                break;
            }
        }
    } else {
        allocFail = true;
    }

    // Check if all memory allocations have succeeded
    if (allocFail) {
        qDebug() << "QUsbBulkTransfer::allocateDiskBuffers(): Could not allocate required disk buffers";
        freeDiskBuffers();
        return;
    }
}

// Free the disk buffer memory
void QUsbBulkTransfer::freeDiskBuffers(void)
{
    // Free up any allocated disk buffers
    if (diskDataBuffers != NULL) {
        for (quint32 i = 0; i < numberOfDiskBuffers; i++) {
            if (diskDataBuffers[i] != NULL) {
                free(diskDataBuffers[i]);
                diskBufferStatus[i] = false;
            }
            diskDataBuffers[i] = NULL;
        }
        free(diskDataBuffers);
    }
}

static void processDiskBuffers(void)
{
    qDebug() << "QUsbBulkTransfer::processDiskBuffers(): Started";

    quint32 nextDiskBufferToWrite = 0;

    do {
        // Is the next disk buffer to write ready?
        if (diskBufferStatus[nextDiskBufferToWrite] == true) {
            // Test the disk buffer
            if (testModeFlag) testDiskBuffer(nextDiskBufferToWrite);

            // Write the disk buffer to disk
            //qDebug() << "processDiskBuffers(): Writing disk buffer" << nextDiskBufferToWrite;
            outputFile->write((const char *)diskDataBuffers[nextDiskBufferToWrite],
                              (((REQUEST_SIZE * PACKET_SIZE) * QUEUE_DEPTH) * QUEUE_BUFFERS_PER_DISK_BUFFER));

            // Simulate a write
            QThread::msleep(50); // 50 milliseconds

            // Free the disk buffer
            diskBufferStatus[nextDiskBufferToWrite] = false;

            // Select the next disk buffer
            nextDiskBufferToWrite++;

            // Check for overflow
            if (nextDiskBufferToWrite == numberOfDiskBuffers) nextDiskBufferToWrite = 0;
        }
    } while (!stopUsbTransfers);

    qDebug() << "QUsbBulkTransfer::processDiskBuffers(): Stopped";
}

// Function to test a disk buffer's data for errors (when in test data mode)
static void testDiskBuffer(quint32 diskBufferNumber)
{
    quint8 highByte = 0;
    quint8 lowByte = 0;
    qint32 currentWord = 0;
    qint32 previousWord = 0;

    // Calculate the size of a single disk buffer
    quint64 bufferSize = ((REQUEST_SIZE * PACKET_SIZE) * QUEUE_DEPTH) * QUEUE_BUFFERS_PER_DISK_BUFFER;

    for (quint64 bytePointer = 0; bytePointer < bufferSize; bytePointer += 2) {
        // Get two bytes of data (that represent our signed little-endian 16-bit number
        lowByte  = diskDataBuffers[diskBufferNumber][bytePointer];
        highByte = diskDataBuffers[diskBufferNumber][bytePointer + 1];

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
                    qDebug() << "testDiskBuffer(): FAIL - Current word =" << currentWord <<
                               ": Previous word =" << previousWord << ": Position =" << bytePointer <<
                                "  Disk buffer =" << diskBufferNumber;

                    statistics.diskFailureCount++;
                }
            }
        }
        // Store the current word as previous for next check
        previousWord = currentWord;
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

// Return the current number of available disk buffers
quint32 QUsbBulkTransfer::getAvailableDiskBuffers(void)
{
    quint32 availableDiskBuffers = 0;

    for (quint32 i = 0; i < numberOfDiskBuffers; i++) {
        if (diskBufferStatus[i] == false) availableDiskBuffers++;
    }

    return availableDiskBuffers;
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
    quint32 transferNumber = transferUserData->transferNumber;
    quint32 maximumTransferNumber = transferUserData->maximumTransferNumber;
    quint32 queueNumber = transferUserData->queueNumber;
    quint32 diskBufferNumber = transferUserData->diskBufferNumber;

    // If this was the last transfer in the queue, calculate statistics
    if (transferNumber == maximumTransferNumber) {
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

    // Test mode
    if (testModeFlag) {
        quint32 blankCounter = 0;
        quint8 highByte = 0;
        quint8 lowByte = 0;
        qint32 currentWord = 0;

        for (quint32 bytePointer = 0; bytePointer < (quint32)transfer->length; bytePointer += 2) {
            // Get two bytes of data (that represent our signed little-endian 16-bit number
            lowByte  = transfer->buffer[bytePointer];
            highByte = transfer->buffer[bytePointer + 1];

            // Convert the bytes into a signed 16-bit value
            currentWord = lowByte + (highByte << 8);

            // Convert into a 10-bit value
            currentWord = currentWord / 64;

            if (currentWord == 0) blankCounter++;
        }
        // Simple check for the correct number of 0 values in the test data
        // This is to prevent debug flood if there are issues
        if (blankCounter != ((REQUEST_SIZE * PACKET_SIZE) / 2048)) qDebug() << "bulkTransferCallback(): Test mode" << blankCounter << "of" << transfer->length;
    }

    // Disk buffer handling

    // Copy the transfer data into the disk buffer
    //
    // The offset calculation is as follows:
    //
    // ((queue number * (queue length * transfer length)) + (transfer number * transfer length))
    // ((queueNumber * (QUEUE_DEPTH * transfer->length)) + (transferNumber * transfer->length))
    memcpy(diskDataBuffers[diskBufferNumber] + ((queueNumber * (QUEUE_DEPTH * transfer->length)) + (transferNumber * transfer->length)),
          transfer->buffer, transfer->length);

    //qDebug() << "TN =" << transferNumber << "MTN =" << maximumTransferNumber << "QN =" << queueNumber << "DBN =" << diskBufferNumber;

    // Target the next transfer to the next queue
    queueNumber++;

    // Does the queueNumber exceed the available disk buffer space? (current buffer full)
    if (queueNumber == queueBuffersPerDiskBuffer) {

        // Is this the last transfer in the current queue?
        if (transferNumber == maximumTransferNumber) {
            // Mark the disk buffer as ready for writing
            //qDebug() << "bulkTransferCallback(): Buffer" << diskBufferNumber << "full";
            diskBufferStatus[diskBufferNumber] = true; // Status = writing
        }

        // Queues exceeded, point at next disk buffer
        diskBufferNumber++;

        // Reset the queue number
        queueNumber = 0;

        // Does the current disk buffer exceed the available number of disk buffers?
        if (diskBufferNumber == numberOfDiskBuffers) {
            // Disk buffers exceeded, point at first disk buffer again
            diskBufferNumber = 0;
        }

        // Check for buffer overflow
        if (diskBufferStatus[diskBufferNumber] == true) {
            // The newly selected buffer is still waiting to be written
            // We are about to start overwriting unsaved data... oh boy
            statistics.diskFailureCount++;
            qDebug() << "bulkTransferCallback(): ERROR - No available disk buffers - Data is being overwritten!";
        }
    }

    // Store the current queueNumber and diskBufferNumber in the transfer user data
    transferUserData->queueNumber = queueNumber;
    transferUserData->diskBufferNumber = diskBufferNumber;

    // Prepare and re-submit the transfer request (unless stopTransfers has been flagged)
    if (!stopUsbTransfers) {
        // Fill and resubmit the transfer
        libusb_fill_bulk_transfer(transfer, transfer->dev_handle, transfer->endpoint,
            transfer->buffer, transfer->length, bulkTransferCallback, transfer->user_data, 5000);

        // If a transfer fails, ignore the error (this will reduce the number of in-flight
        // transfers by 1 - but it should never happen...
        if (libusb_submit_transfer(transfer) == 0) transfersInFlight++;
        else qDebug() << "bulkTransferCallback(): libusb_submit_transfer failed!";
    }
}
