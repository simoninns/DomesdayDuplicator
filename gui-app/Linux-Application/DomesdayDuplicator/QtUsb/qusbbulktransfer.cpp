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

// The following statically defined variables are used by the libUSB
// transfer callback

// Private variables used to control the transfer callbacks
static volatile quint32 transfersInFlight;       // Number of transfers that are in progress
static volatile bool stopUsbTransfers;           // USB transfer stop flag
static volatile bool stopDiskTransfers;          // Disk transfer stop flag
static volatile bool diskTransferRunning;        // Disk transfers running flag
static volatile bool bufferFlushComplete;        // Flag for initial FIFO 'buffer flush'

// Note: The 'buffer flush' functionality throws away the data from the first queue
// of USB transfers.  This is required as the FPGA FIFO will overflow before data
// collection begins causing the first received data to be potentially corrupt. Since
// there is no effective 'real time' mechanism to tell the FPGA to flush data we simple
// read it out and discard before starting to write data to disk.  The amount of lost
// data depends on the size of the queue (see below)

// Structure to hold libUSB transfer user data
struct transferUserDataStruct {
    quint32 transferNumber;         // (disk buffering) The transfer number of the transfer (within the queue)
    quint32 maximumTransferNumber;  // (disk buffering) The maximum transfer number allowed (within a queue)
    quint32 queueNumber;            // (disk buffering) The queue number that the transfer belongs to
    quint32 diskBufferNumber;       // (disk buffering) The disk buffer that the transfer belongs to
};

// Private variables used to report statistics about the transfer process
struct statisticsStruct {
    quint32 packetCount;           // Number of successful transfers
    quint32 transferPerformance;    // Performance in Kilobytes per second
    quint32 transferSize;           // Size of data transfers performed so far
    struct timeval startTimestamp;  // Data transfer start time stamp.
};
static statisticsStruct statistics;

// Flag to show if transfer is in progress
volatile bool transferRunning;

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
//   (128Kbytes) * 16 = 2Mbytes
//
// Size of each disk buffer =
//   2MBytes * 16 = 32Mbytes
//
// Total size of disk buffers =
//   32Mbytes * 4 = 128Mbytes
//
#define REQUEST_SIZE 16
#define PACKET_SIZE 16384
#define QUEUE_DEPTH 16
#define QUEUE_BUFFERS_PER_DISK_BUFFER 16
#define NUMBER_OF_DISK_BUFFERS 4

// Disk buffer variables
static unsigned char **diskDataBuffers = NULL; // List of disk data buffers
static volatile quint32 numberOfDiskBuffers;
static volatile quint32 queueBuffersPerDiskBuffer;
static volatile bool diskBufferStatus[NUMBER_OF_DISK_BUFFERS];

// Disk IO
static QFile* outputFile;

QUsbBulkTransfer::QUsbBulkTransfer(void)
{
    stopUsbTransfers = false;
    stopDiskTransfers = false;
    transferRunning = false;
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

void QUsbBulkTransfer::setup(libusb_context* mCtx, libusb_device_handle* devHandle, quint8 endPoint, QString fileName)
{
    // Store the libUSB information locally
    libUsbContext = mCtx;
    libUsbDeviceHandle = devHandle;
    libUsbEndPoint = endPoint;

    // Set the maximum number of simultaneous transfers allowed
    queueDepth = QUEUE_DEPTH;

    // Set the capture file name
    captureFileName = fileName;
}

// Return true if transfer is running
bool QUsbBulkTransfer::isTransferRunning(void) {
    return transferRunning;
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
    statistics.packetCount = 0;
    statistics.transferSize = 0;
    statistics.transferPerformance = 0;
    gettimeofday(&statistics.startTimestamp, NULL);

    // Reset transfer variables
    transferRunning = false;
    transfersInFlight = 0;      // Number of transfers that are in progress (global)
    requestSize = REQUEST_SIZE; // Request size (number of packets)
    packetSize = PACKET_SIZE;   // Maximum packet size for the endpoint (in bytes)
    bufferFlushComplete = false;

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

    // Process libUSB events whilst the transfer is stopping
    qDebug() << "transferThread::run(): Stopping in-flight USB transfer threads...";
    while (transfersInFlight != 0) {
        libusb_handle_events_timeout(libUsbContext, &libusbHandleTimeout);
        usleep(200); // Prevent debug flood
    }
    qDebug() << "transferThread::run(): USB transfer threads stopped";

    // Signal the disk transfers to stop
    qDebug() << "transferThread::run(): Stopping disk transfer thread...";
    stopDiskTransfers = true;

    while (diskTransferRunning) {
        // Wait for disk transfers to complete
    }
    qDebug() << "transferThread::run(): Disk transfers thread has stopped";

    // Close the disk capture file
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

    // Flag to show disk transfers are running
    diskTransferRunning = true;

    do {
        // Is the next disk buffer to write ready?
        if (diskBufferStatus[nextDiskBufferToWrite] == true) {
            // Write the disk buffer to disk
            //qDebug() << "processDiskBuffers(): Writing disk buffer" << nextDiskBufferToWrite;
            outputFile->write((const char *)diskDataBuffers[nextDiskBufferToWrite],
                              (((REQUEST_SIZE * PACKET_SIZE) * QUEUE_DEPTH) * QUEUE_BUFFERS_PER_DISK_BUFFER));

            // Free the disk buffer
            diskBufferStatus[nextDiskBufferToWrite] = false;

            // Select the next disk buffer
            nextDiskBufferToWrite++;

            // Check for overflow
            if (nextDiskBufferToWrite == numberOfDiskBuffers) nextDiskBufferToWrite = 0;
        }
    } while ((!stopDiskTransfers) || (diskBufferStatus[nextDiskBufferToWrite] == true));
    // Stop when stopDiskTransfers is true and the next disk buffer isn't ready

    qDebug() << "QUsbBulkTransfer::processDiskBuffers(): Stopped";

    // Flag to show disk transfers are complete
    diskTransferRunning = false;
}

// Functions to report transfer statistics -------------------------------------------

// Return the current value of the bulk transfer packet counter
quint32 QUsbBulkTransfer::getPacketCounter(void)
{
    return (quint32)statistics.packetCount;
}

// Return the current value of the bulk transfer packet size
quint32 QUsbBulkTransfer::getPacketSize(void)
{
    // Returns current packet size in KBytes
    return (REQUEST_SIZE * PACKET_SIZE) / 1024;
}

// Return the current bulk transfer stream performance value in KBytes/sec
quint32 QUsbBulkTransfer::getTransferPerformance(void)
{
    return (quint32)statistics.transferPerformance;
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

// Return the total number of available disk buffers
quint32 QUsbBulkTransfer::getNumberOfDiskBuffers(void)
{
    return numberOfDiskBuffers;
}


// LibUSB transfer callback handling function ---------------------------------------------------------
static void LIBUSB_CALL bulkTransferCallback(struct libusb_transfer *transfer)
{
    unsigned int elapsedTime;
    double       performance;

    // Check if the transfer has succeeded
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
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
        // Transfer has succeeded (increment statistics unless this is a FIFO flush transfer)
        if (bufferFlushComplete) statistics.packetCount++;
    }

    // Update the transfer size for the whole bulk transfer so far
    if (bufferFlushComplete) statistics.transferSize += transfer->length;

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

    // If the buffer flush is complete, flag the transfer as running
    if (bufferFlushComplete) transferRunning = true;

    // Write received data to disk (unless we are performing a buffer flush)
    if (bufferFlushComplete) {
        // Disk buffer handling

        // Copy the transfer data into the disk buffer
        //
        // The offset calculation is as follows:
        //
        // ((queue number * (queue length * transfer length)) + (transfer number * transfer length))
        // ((queueNumber * (QUEUE_DEPTH * transfer->length)) + (transferNumber * transfer->length))
        memcpy(diskDataBuffers[diskBufferNumber]
              + ((queueNumber * (QUEUE_DEPTH * transfer->length))
              + (transferNumber * transfer->length)),
              transfer->buffer, transfer->length);

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
                qDebug() << "bulkTransferCallback(): ERROR - No available disk buffers - Data is being overwritten!";
            }
        }
    } else {
        // Buffer flush

        // Is this the last transfer in the current queue?
        if (transferNumber == maximumTransferNumber) {
            bufferFlushComplete = true;
        }
    }

    // Store the current queueNumber and diskBufferNumber in the transfer user data
    transferUserData->queueNumber = queueNumber;
    transferUserData->diskBufferNumber = diskBufferNumber;

    // Prepare and re-submit the transfer request (unless stopTransfers has been flagged)
    if (stopUsbTransfers == true && queueNumber == 0) {
        // Abandon the next transfer
        // Note: we wait for queueNumber = 0 to ensure the disk buffer is written
    } else {
        // Fill and resubmit the transfer
        libusb_fill_bulk_transfer(transfer, transfer->dev_handle, transfer->endpoint,
            transfer->buffer, transfer->length, bulkTransferCallback, transfer->user_data, 5000);

        // If a transfer fails, ignore the error (this will reduce the number of in-flight
        // transfers by 1 - but it should never happen...
        if (libusb_submit_transfer(transfer) == 0) transfersInFlight++;
        else qDebug() << "bulkTransferCallback(): libusb_submit_transfer failed!";
    }
}
