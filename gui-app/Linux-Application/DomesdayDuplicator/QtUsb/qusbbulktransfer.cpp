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
    quint32 packetCount;            // Number of successful transfers
    quint32 transferPerformance;    // Performance in Kilobytes per second
    quint32 transferSize;           // Size of data transfers performed so far
    unsigned char _padding[4];      // Required to avoid compiler warning
    struct timeval startTimestamp;  // Data transfer start time stamp.
};
static statisticsStruct statistics;

// Flag to show if transfer is in progress
extern volatile bool transferRunning; // Gets rid of gcc
volatile bool transferRunning;

// Flag to show current data format
static volatile bool isDataFormat10bit; // True = 10-bit, false 16-bit

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
static unsigned char **diskDataBuffers = nullptr; // List of disk data buffers
static volatile quint32 numberOfDiskBuffers;
static volatile quint32 queueBuffersPerDiskBuffer;
static volatile bool diskBufferStatus[NUMBER_OF_DISK_BUFFERS];

static unsigned char *temporaryDiskBuffer = nullptr; // Temporary disk data buffer

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

void QUsbBulkTransfer::setup(libusb_context* mCtx, libusb_device_handle* devHandle, quint8 endPoint, QString fileName, bool isTenBit)
{
    // Store the libUSB information locally
    libUsbContext = mCtx;
    libUsbDeviceHandle = devHandle;
    libUsbEndPoint = endPoint;

    // Set the maximum number of simultaneous transfers allowed
    queueDepth = QUEUE_DEPTH;

    // Set the capture file name
    captureFileName = fileName;

    // Set the data format
    isDataFormat10bit = isTenBit;
}

// Return true if transfer is running
bool QUsbBulkTransfer::isTransferRunning(void) {
    return transferRunning;
}

void QUsbBulkTransfer::run()
{
    // These variables are referenced by the callback and must remain valid
    // until the transfer is complete
    struct libusb_transfer **usbTransfers = nullptr;        // List of transfer structures
    unsigned char **usbDataBuffers = nullptr;               // List of USB data buffers
    transferUserDataStruct transferUserData[QUEUE_DEPTH];   // Transfer user data

    int transferReturnStatus = 0; // Status return from libUSB
    qDebug() << "usbBulkTransfer::run(): Started";

    // Check for validity of the device handle
    if (libUsbDeviceHandle == nullptr) {
        qDebug() << "QUsbBulkTransfer::run(): Invalid device handle";
        return;
    }

    // Reset transfer tracking ready for the new run...

    // Reset transfer statistics (globals)
    statistics.packetCount = 0;
    statistics.transferSize = 0;
    statistics.transferPerformance = 0;
    gettimeofday(&statistics.startTimestamp, nullptr);

    // Reset transfer variables
    transferRunning = false;
    transfersInFlight = 0;      // Number of transfers that are in progress (global)
    requestSize = REQUEST_SIZE; // Request size (number of packets)
    packetSize = PACKET_SIZE;   // Maximum packet size for the endpoint (in bytes)
    bufferFlushComplete = false;

    // Allocate buffers and transfer structures for USB->PC communication
    bool allocFail = false;

    usbDataBuffers = static_cast<unsigned char **>(calloc (queueDepth, sizeof (unsigned char *)));
    usbTransfers   = static_cast<struct libusb_transfer **>(calloc (queueDepth, sizeof (struct libusb_transfer *)));

    if ((usbDataBuffers != nullptr) && (usbTransfers != nullptr)) {

        for (unsigned int i = 0; i < queueDepth; i++) {

            usbDataBuffers[i] = static_cast<unsigned char *>(malloc (requestSize * packetSize));
            usbTransfers[i]   = libusb_alloc_transfer(0);

            if ((usbDataBuffers[i] == nullptr) || (usbTransfers[i] == nullptr)) {
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
            usbDataBuffers[currentTransferNumber], static_cast<int>(requestSize * packetSize), bulkTransferCallback, &transferUserData[currentTransferNumber], 5000);
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
    if (transfers != nullptr) {
        for (quint32 i = 0; i < queueDepth; i++) {
            if (transfers[i] != nullptr) {
                libusb_free_transfer(transfers[i]);
            }
            transfers[i] = nullptr;
        }
        free (transfers);
    }

    // Free up any allocated data buffers
    if (dataBuffers != nullptr) {
        for (quint32 i = 0; i < queueDepth; i++) {
            if (dataBuffers[i] != nullptr) {
                free(dataBuffers[i]);
            }
            dataBuffers[i] = nullptr;
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

    diskDataBuffers = static_cast<unsigned char **>(calloc(numberOfDiskBuffers, sizeof (unsigned char *)));
    if (diskDataBuffers != nullptr) {
        for (quint32 i = 0; i < numberOfDiskBuffers; i++) {

            diskDataBuffers[i] = static_cast<unsigned char *>(malloc(queueSize * queueBuffersPerDiskBuffer));
            diskBufferStatus[i] = false; // Status = Not writing

            if (diskDataBuffers[i] == nullptr) {
                allocFail = true;
                break;
            }
        }
    } else {
        allocFail = true;
    }

    // Allocate the temporary disk buffer for data translation
    // temporaryDiskBuffer
    temporaryDiskBuffer = static_cast<unsigned char *>(malloc(queueSize * queueBuffersPerDiskBuffer));

    if (temporaryDiskBuffer == nullptr) {
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
    if (diskDataBuffers != nullptr) {
        for (quint32 i = 0; i < numberOfDiskBuffers; i++) {
            if (diskDataBuffers[i] != nullptr) {
                free(diskDataBuffers[i]);
                diskBufferStatus[i] = false;
            }
            diskDataBuffers[i] = nullptr;
        }
        free(diskDataBuffers);
    }

    // Free up the temporary disk buffer
    free(temporaryDiskBuffer);
    temporaryDiskBuffer = nullptr;
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
            // Is data format 10-bit packed?
            if (isDataFormat10bit) {
                // Translate the data in the disk buffer to packed 10-bit unsigned data
                unsigned int tempPointer = 0;
                for (unsigned int diskPointer = 0;
                     diskPointer < (((REQUEST_SIZE * PACKET_SIZE) * QUEUE_DEPTH) * QUEUE_BUFFERS_PER_DISK_BUFFER);
                     diskPointer += 8) {

                    // Pack 4 16-bit word values into 5 bytes
                    //
                    // Original
                    // 0: xxxx xx00 0000 0000
                    // 1: xxxx xx11 1111 1111
                    // 2: xxxx xx22 2222 2222
                    // 3: xxxx xx33 3333 3333
                    //
                    // Packed:
                    // 0: 0000 0000 0011 1111
                    // 2: 1111 2222 2222 2233
                    // 4: 3333 3333

                    // Get the original 4 16-bit words
                    unsigned int originalWords[4];
                    originalWords[0]  = diskDataBuffers[nextDiskBufferToWrite][diskPointer + 0];
                    originalWords[0] += diskDataBuffers[nextDiskBufferToWrite][diskPointer + 1] * 256;
                    originalWords[1]  = diskDataBuffers[nextDiskBufferToWrite][diskPointer + 2];
                    originalWords[1] += diskDataBuffers[nextDiskBufferToWrite][diskPointer + 3] * 256;
                    originalWords[2]  = diskDataBuffers[nextDiskBufferToWrite][diskPointer + 4];
                    originalWords[2] += diskDataBuffers[nextDiskBufferToWrite][diskPointer + 5] * 256;
                    originalWords[3]  = diskDataBuffers[nextDiskBufferToWrite][diskPointer + 6];
                    originalWords[3] += diskDataBuffers[nextDiskBufferToWrite][diskPointer + 7] * 256;

                    // Convert into 5 bytes of packed 10-bit data
                    //temporaryDiskBuffer[tempPointer + 0]  = (originalWords[0] & 0b 0000 0011 1111 1100) >> 2;
                    //temporaryDiskBuffer[tempPointer + 1]  = (originalWords[0] & 0b 0000 0000 0000 0011) << 6;
                    //temporaryDiskBuffer[tempPointer + 1] += (originalWords[1] & 0b 0000 0011 1111 0000) >> 4;
                    //temporaryDiskBuffer[tempPointer + 2]  = (originalWords[1] & 0b 0000 0000 0000 1111) << 4;
                    //temporaryDiskBuffer[tempPointer + 2] += (originalWords[2] & 0b 0000 0011 1100 0000) >> 6;
                    //temporaryDiskBuffer[tempPointer + 3]  = (originalWords[2] & 0b 0000 0000 0011 1111) << 2;
                    //temporaryDiskBuffer[tempPointer + 3] += (originalWords[3] & 0b 0000 0011 0000 0000) >> 8;
                    //temporaryDiskBuffer[tempPointer + 4]  = (originalWords[3] & 0b 0000 0000 1111 1111);

                    temporaryDiskBuffer[tempPointer + 0]  = static_cast<unsigned char>((originalWords[0] & 0x03FC) >> 2);
                    temporaryDiskBuffer[tempPointer + 1]  = static_cast<unsigned char>((originalWords[0] & 0x0003) << 6);
                    temporaryDiskBuffer[tempPointer + 1] += static_cast<unsigned char>((originalWords[1] & 0x03F0) >> 4);
                    temporaryDiskBuffer[tempPointer + 2]  = static_cast<unsigned char>((originalWords[1] & 0x000F) << 4);
                    temporaryDiskBuffer[tempPointer + 2] += static_cast<unsigned char>((originalWords[2] & 0x03C0) >> 6);
                    temporaryDiskBuffer[tempPointer + 3]  = static_cast<unsigned char>((originalWords[2] & 0x003F) << 2);
                    temporaryDiskBuffer[tempPointer + 3] += static_cast<unsigned char>((originalWords[3] & 0x0300) >> 8);
                    temporaryDiskBuffer[tempPointer + 4]  = static_cast<unsigned char>((originalWords[3] & 0x00FF));

                    // Increment the pointer to the temporary buffer
                    tempPointer += 5;
                }

                // Write the temporary disk buffer to disk
                outputFile->write(reinterpret_cast<const char *>(temporaryDiskBuffer), tempPointer);
            } else {
                // Translate the data in the disk buffer to scaled 16-bit signed data
                for (unsigned int tempPointer = 0;
                     tempPointer < (((REQUEST_SIZE * PACKET_SIZE) * QUEUE_DEPTH) * QUEUE_BUFFERS_PER_DISK_BUFFER);
                     tempPointer += 2) {

                    // Get the original 10-bit unsigned value from the disk data buffer
                    unsigned int originalValue = diskDataBuffers[nextDiskBufferToWrite][tempPointer];
                    originalValue += diskDataBuffers[nextDiskBufferToWrite][tempPointer+1] * 256;

                    // Sign and scale the data to 16-bits
                    int signedValue = static_cast<int>(originalValue - 512);
                    signedValue = signedValue * 64;

                    temporaryDiskBuffer[tempPointer] = static_cast<unsigned char>(signedValue & 0x00FF);
                    temporaryDiskBuffer[tempPointer+1] = static_cast<unsigned char>((signedValue & 0xFF00) >> 8);
                }

                // Write the disk buffer to disk
                //qDebug() << "processDiskBuffers(): Writing disk buffer" << nextDiskBufferToWrite;
                outputFile->write(reinterpret_cast<const char *>(temporaryDiskBuffer),
                                  (((REQUEST_SIZE * PACKET_SIZE) * QUEUE_DEPTH) * QUEUE_BUFFERS_PER_DISK_BUFFER));
            }

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
    return static_cast<quint32>(statistics.packetCount);
}

// Return the current value of the bulk transfer packet size
quint32 QUsbBulkTransfer::getPacketSize(void)
{
    // Returns current packet size in KBytes
    if (isDataFormat10bit) {
        // If the data format is 10 bit, we have to account for the reduction in size
        return (REQUEST_SIZE * ((PACKET_SIZE / 5) * 4)) / 1024;
    }
    return (REQUEST_SIZE * PACKET_SIZE) / 1024;
}

// Return the current bulk transfer stream performance value in KBytes/sec
quint32 QUsbBulkTransfer::getTransferPerformance(void)
{
    return static_cast<quint32>(statistics.transferPerformance);
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
    if (bufferFlushComplete) statistics.transferSize += static_cast<unsigned int>(transfer->length);

    // Reduce the number of requests in-flight.
    transfersInFlight--;

    // Extract the user data
    transferUserDataStruct *transferUserData = static_cast<transferUserDataStruct *>(transfer->user_data);
    quint32 transferNumber = transferUserData->transferNumber;
    quint32 maximumTransferNumber = transferUserData->maximumTransferNumber;
    quint32 queueNumber = transferUserData->queueNumber;
    quint32 diskBufferNumber = transferUserData->diskBufferNumber;

    // If this was the last transfer in the queue, calculate statistics
    if (transferNumber == maximumTransferNumber) {
        // Calculate the transfer statistics (just for user feedback)
        struct timeval endTimestamp;

        gettimeofday (&endTimestamp, nullptr);
        elapsedTime = static_cast<unsigned int>(((endTimestamp.tv_sec - statistics.startTimestamp.tv_sec) * 1000000 +
            (endTimestamp.tv_usec - statistics.startTimestamp.tv_usec)));

        // Calculate the performance in Kbytes per second
        performance = ((static_cast<double>(statistics.transferSize) / 1024) / (static_cast<double>(elapsedTime) / 1000000));
        statistics.transferPerformance = static_cast<unsigned int>(performance);
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
              + ((queueNumber * (QUEUE_DEPTH * static_cast<unsigned int>(transfer->length)))
              + (transferNumber * static_cast<unsigned int>(transfer->length))),
              transfer->buffer, static_cast<unsigned int>(transfer->length));

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
