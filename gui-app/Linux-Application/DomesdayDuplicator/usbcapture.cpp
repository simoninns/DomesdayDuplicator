/************************************************************************

    usbcapture.cpp

    Capture application for the Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2018 Simon Inns

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

#include "usbcapture.h"

// Notes on transfer and disk buffering:
//
// transferSize: Each in-flight transfer returns 16 Kbytes
// simultaneousTransfers: There are 16 simultaneous in-flight transfers
// transfersPerDiskBuffer: There are 2048 transfers per disk buffer
// numberOfDiskBuffers: There are 4 disk buffers
//
static const qint32 transferSize = 16834;
static const qint32 simultaneousTransfers = 16;
static const qint32 transfersPerDiskBuffer = 2048;
static const qint32 numberOfDiskBuffers = 4;


// Globals required for libUSB call-back handling ---------------------------------------------------------------------

// Structure to contain the user-data passed during transfer call-backs
struct transferUserDataStruct {
    qint32 diskBufferTransferNumber;         // The transfer number of the transfer (0-2047)
    qint32 diskBufferNumber;       // The current target disk buffer number (0-3)
};

// Global to monitor the number of in-flight transfers
static qint32 transfersInFlight = 0;

// Flag to cancel transfers in flight
static bool transferAbort = false;

// Flag to show transfer failure
static bool transferFailure = false;

// Private variables used to report statistics about the transfer process
struct statisticsStruct {
    qint32 transferCount;          // Number of successful transfers
};
static statisticsStruct statistics;

// Set up a pointer to the disk buffers and their full flags
static unsigned char **diskBuffers = nullptr;
static bool isDiskBufferFull[numberOfDiskBuffers];

// Set up a pointer to the conversion buffer
static unsigned char *conversionBuffer;


// LibUSB call-back handling code -------------------------------------------------------------------------------------

// LibUSB transfer call-back handler (called when an in-flight transfer completes)
static void LIBUSB_CALL bulkTransferCallback(struct libusb_transfer *transfer)
{
    // Extract the user data
    transferUserDataStruct *transferUserData = static_cast<transferUserDataStruct *>(transfer->user_data);

//    qDebug() << "bulkTransferCallback(): Processing transfer" <<
//                transferUserData->diskBufferTransferNumber << "in disk buffer" << transferUserData->diskBufferNumber;

    // Check if the transfer has succeeded
    if ((transfer->status != LIBUSB_TRANSFER_COMPLETED) && !transferAbort) {
        transferFailure = true;

        // Show the failure reason in the debug
        switch (transfer->status) {
            case LIBUSB_TRANSFER_ERROR:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_ERROR - Transfer" <<
                        transferUserData->diskBufferTransferNumber << "failed";
            break;

            case LIBUSB_TRANSFER_TIMED_OUT:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_TIMED_OUT - Transfer" <<
                        transferUserData->diskBufferTransferNumber << "timed out";
            break;

            case LIBUSB_TRANSFER_CANCELLED:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_CANCELLED - Transfer" <<
                        transferUserData->diskBufferTransferNumber << "was cancelled";
            break;

            case LIBUSB_TRANSFER_STALL:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_STALL - Transfer" <<
                        transferUserData->diskBufferTransferNumber << "Endpoint stalled";
            break;

            case LIBUSB_TRANSFER_NO_DEVICE:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_NO_DEVICE - Transfer" <<
                        transferUserData->diskBufferTransferNumber << "- Device disconnected";
            break;

            case LIBUSB_TRANSFER_OVERFLOW:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_OVERFLOW - Transfer" <<
                        transferUserData->diskBufferTransferNumber << "- Device overflow";
            break;

            default:
                qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER - Transfer" <<
                            transferUserData->diskBufferTransferNumber << " - Unknown error";
        }
    }

    // Reduce the number of requests in-flight.
    transfersInFlight--;

    // Increment the total number of successful transfers
    statistics.transferCount++;

    // Last transfer in the disk buffer?
    if (transferUserData->diskBufferTransferNumber == (transfersPerDiskBuffer - 1)) {
        // Mark the disk buffer as full
        isDiskBufferFull[transferUserData->diskBufferNumber] = true;
    }

    // Point to the next slot for the transfer in the disk buffer
    transferUserData->diskBufferTransferNumber += simultaneousTransfers;

    // Check that the current disk buffer hasn't been exceeded
    if (transferUserData->diskBufferTransferNumber >= transfersPerDiskBuffer) {
        // Select the next disk buffer
        transferUserData->diskBufferNumber++;
        if (transferUserData->diskBufferNumber == numberOfDiskBuffers) transferUserData->diskBufferNumber = 0;

        // Ensure selected disk buffer is free
        if (isDiskBufferFull[transferUserData->diskBufferNumber]) {
            // Buffer is full - flag an overflow error
            qDebug() << "bulkTransferCallback(): Disk buffer overflow error!";
            transferFailure = true;
        }

        // Wrap the transfer number back to the start of the disk buffer
        transferUserData->diskBufferTransferNumber -= transfersPerDiskBuffer;
    }

    // If the transfer has not been aborted, resubmit the transfer to libUSB
    if (!transferAbort) {
        libusb_fill_bulk_transfer(transfer, transfer->dev_handle, transfer->endpoint,
                                  reinterpret_cast<unsigned char *>(diskBuffers[transferUserData->diskBufferNumber] +
                                  (transferSize * transferUserData->diskBufferTransferNumber)),
                                  transfer->length, bulkTransferCallback,
                                  transfer->user_data, 1000);

        if (libusb_submit_transfer(transfer) == 0) {
            transfersInFlight++;
        } else {
            qDebug() << "bulkTransferCallback(): Transfer re-submission failed!";
            transferFailure = true;
        }
    } else {
        // Free the transfer buffer
        libusb_free_transfer(transfer);
    }
}


// UsbCapture class code ----------------------------------------------------------------------------------------------

UsbCapture::UsbCapture(QObject *parent, libusb_context *libUsbContextParam,
                       libusb_device_handle *usbDeviceHandleParam,
                       QString filenameParam) : QThread(parent)
{
    // Set the libUSB context
    libUsbContext = libUsbContextParam;

    // Set the device's handle
    usbDeviceHandle = usbDeviceHandleParam;

    if (usbDeviceHandle == nullptr) qDebug() << "UsbCapture::UsbCapture(): ERROR, passed usb device handle is not valid!";
    if (libUsbContext == nullptr) qDebug() << "UsbCapture::UsbCapture(): ERROR, passed usb context is not valid!";

    // Store the requested file name
    filename = filenameParam;

    // Set the transfer abort flag
    transferAbort = false;

    // Reset transfer statistics
    statistics.transferCount = 0;
    numberOfDiskBuffersWritten = 0;

    // Clear the transfer failure flag
    transferFailure = false;
}

// Class destructor
UsbCapture::~UsbCapture()
{
    // Stop the capture thread
    transferAbort = true;
    this->wait();

    // Close the USB device
    libusb_close(usbDeviceHandle);

    // Destroy the device handle
    usbDeviceHandle = nullptr;
}

// Run the capture thread
void UsbCapture::run(void)
{
    // Set up the libusb transfers
    struct libusb_transfer **usbTransfers = nullptr;
    usbTransfers = static_cast<struct libusb_transfer **>(calloc(simultaneousTransfers, sizeof(struct libusb_transfer *)));

    // Set up the user-data for the transfers
    QVector<transferUserDataStruct> transferUserData;
    transferUserData.resize(simultaneousTransfers);

    // Set up the USB transfer buffers
    qDebug() << "UsbCapture::run(): Setting up the transfers";

    // Allocate the memory required for the disk buffers
    allocateDiskBuffers();

    // Launch a thread for writing disk buffers to disk
    QFuture<void> future = QtConcurrent::run(this, &UsbCapture::runDiskBuffers);

    // Set up the initial transfers
    for (qint32 transferNumber = 0; transferNumber < simultaneousTransfers; transferNumber++) {
        usbTransfers[transferNumber] = libusb_alloc_transfer(0);

        // Check USB transfer allocation was successful
        if (usbTransfers[transferNumber] == nullptr) {
            qDebug() << "UsbCapture::run(): LibUSB alloc failed for transfer number" << transferNumber;
            transferFailure = true;
        } else {
            // Set up the user-data for the initial transfers
            transferUserData[transferNumber].diskBufferTransferNumber = transferNumber;
            transferUserData[transferNumber].diskBufferNumber = 0;

            // Set transfer flag to cause transfer error if there is a short packet
            usbTransfers[transferNumber]->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;

            // Configure the transfer with a 1 second timeout (targeted to disk buffer 0)
            libusb_fill_bulk_transfer(usbTransfers[transferNumber], usbDeviceHandle, 0x81,
                                      reinterpret_cast<unsigned char *>(diskBuffers[0] + (transferSize * transferNumber)),
                                      16384, bulkTransferCallback, &transferUserData[transferNumber], 1000);
        }
    }

    // Submit the transfers via libUSB
    qDebug() << "UsbCapture::run(): Submitting the transfers";
    for (qint32 currentTransferNumber = 0; currentTransferNumber < simultaneousTransfers; currentTransferNumber++) {
        if (libusb_submit_transfer(usbTransfers[currentTransferNumber]) == 0) {
            transfersInFlight++;
        } else {
            qDebug() << "UsbCapture::run(): Transfer launch" << transfersInFlight << "failed!";
            transferFailure = true;
        }
    }
    qDebug() << "UsbCapture::run():" << transfersInFlight << " simultaneous transfers launched.";

    // Use a 1 second timeout for the libusb_handle_events_timeout call
    struct timeval libusbHandleTimeout;
    libusbHandleTimeout.tv_sec  = 1;
    libusbHandleTimeout.tv_usec = 0;

    // Perform background tasks whilst transfers are proceeding
    while(!transferAbort && !transferFailure) {
        // Process libUSB events
        libusb_handle_events_timeout(libUsbContext, &libusbHandleTimeout);
    }

    // Aborting transfer - wait for in-flight transfers to complete
    qDebug() << "UsbCapture::run(): Transfer aborted - waiting for in-flight transfers to complete...";
    transferAbort = true;

    while(transfersInFlight > 0) {
        // Process libUSB events
        libusb_handle_events_timeout(libUsbContext, &libusbHandleTimeout);
    }

    // If the transfer failed, emit a notification signal to the parent object
    if (transferFailure) {
        qDebug() << "UsbCapture::run(): Transfer failed - emitting notification signal";
        emit transferFailed();
    }

    // Free the disk buffers
    freeDiskBuffers();

    // All done
    qDebug() << "UsbCapture::run(): Transfer complete," << statistics.transferCount << "transfers performed.";
}

// Allocate memory for the disk buffers
// Note: Using vectors would be neater, but they are just too slow
void UsbCapture::allocateDiskBuffers(void)
{
    qDebug() << "UsbCapture::allocateDiskBuffers(): Allocating memory for disk buffers";
    // Allocate the disk buffers
    diskBuffers = static_cast<unsigned char **>(calloc(numberOfDiskBuffers, sizeof(unsigned char *)));
    if (diskBuffers != nullptr) {
        for (quint32 bufferNumber = 0; bufferNumber < numberOfDiskBuffers; bufferNumber++) {

            diskBuffers[bufferNumber] = static_cast<unsigned char *>(malloc(transferSize * transfersPerDiskBuffer));
            isDiskBufferFull[bufferNumber] = false;

            if (diskBuffers[bufferNumber] == nullptr) {
                // Memory allocation has failed
                qDebug() << "UsbCapture::allocateDiskBuffers(): Disk buffer memory allocation failed!";
                transferFailure = true;
                break;
            }
        }
    } else {
        // Memory allocation has failed
        qDebug() << "UsbCapture::allocateDiskBuffers(): Disk buffer array allocation failed!";
        transferFailure = true;
    }

    // Allocate the conversion buffer
    conversionBuffer = static_cast<unsigned char *>(malloc(transferSize * transfersPerDiskBuffer));
    if (conversionBuffer == nullptr) {
        qDebug() << "UsbCapture::allocateDiskBuffers(): Conversion buffer memory allocation failed!";
        transferFailure = true;
    }
}

// Free memory used for the disk buffers
void UsbCapture::freeDiskBuffers(void)
{
    qDebug() << "UsbCapture::freeDiskBuffers(): Freeing disk buffer memory";
    // Free up the allocated disk buffers
    if (diskBuffers != nullptr) {
        for (qint32 bufferNumber = 0; bufferNumber < numberOfDiskBuffers; bufferNumber++) {
            if (diskBuffers[bufferNumber] != nullptr) {
                free(diskBuffers[bufferNumber]);
            }
            diskBuffers[bufferNumber] = nullptr;
        }
        free(diskBuffers);
    }

    // Free up the temporary disk buffer
    free(conversionBuffer);
    conversionBuffer = nullptr;
}

// Thread for processing disk buffers
void UsbCapture::runDiskBuffers(void)
{
    qDebug() << "UsbCapture::runDiskBuffers(): Thread started";

    // Open the capture file
    QFile outputFile(filename);
    if(!outputFile.open(QFile::WriteOnly | QFile::Truncate)) {
        qDebug() << "UsbCapture::runDiskBuffers(): Could not open destination capture file for writing!";
        transferFailure = true;
    }

    // Process the disk buffers until the transfer is complete or fails
    while(!transferAbort && !transferFailure) {
        for (qint32 diskBufferNumber = 0; diskBufferNumber < numberOfDiskBuffers; diskBufferNumber++) {
            if (isDiskBufferFull[diskBufferNumber]) {
                // Write the buffer
                //qDebug() << "UsbCapture::runDiskBuffers(): Disk buffer" << diskBufferNumber << "processing...";
                writeBufferToDisk(&outputFile, diskBufferNumber);

                // Mark it as empty
                isDiskBufferFull[diskBufferNumber] = false;

                // Increment the statistics
                numberOfDiskBuffersWritten++;
            } else {
                // Sleep the thread for 100 uS to keep the CPU usage down
                usleep(100);
            }

            // Check for transfer failure before continuing...
            if (transferFailure) break;
        }
    }

    // Close the capture file
    outputFile.close();

    qDebug() << "UsbCapture::runDiskBuffers(): Thread stopped";
}

// Write a disk buffer to disk
void UsbCapture::writeBufferToDisk(QFile *outputFile, qint32 diskBufferNumber)
{
    // Translate the data in the disk buffer to scaled 16-bit signed data
    for (qint32 pointer = 0; pointer < (transferSize * transfersPerDiskBuffer); pointer += 2) {
        // Get the original 10-bit unsigned value from the disk data buffer
        quint32 originalValue = diskBuffers[diskBufferNumber][pointer];
        originalValue += diskBuffers[diskBufferNumber][pointer+1] * 256;

        // Sign and scale the data to 16-bits
        qint32 signedValue = static_cast<qint32>(originalValue - 512);
        signedValue = signedValue * 64;

        conversionBuffer[pointer] = static_cast<unsigned char>(signedValue & 0x00FF);
        conversionBuffer[pointer+1] = static_cast<unsigned char>((signedValue & 0xFF00) >> 8);
    }

    // Write the disk buffer to disk
    outputFile->write(reinterpret_cast<const char *>(conversionBuffer), (transferSize * transfersPerDiskBuffer));
}

// Start capturing
void UsbCapture::startTransfer(void)
{
    // Start the capture processing thread
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
    this->start();
    qDebug() << "UsbCapture::start(): Transfer thread running";
}

// Stop capturing
void UsbCapture::stopTransfer(void)
{
    // Set the transfer flags to abort
    transferAbort = true;
}

// Return the current number of transfers completed
qint32 UsbCapture::getNumberOfTransfers(void)
{
    return statistics.transferCount;
}

// Return the current number of disk buffers written to disk
qint32 UsbCapture::getNumberOfDiskBuffersWritten(void)
{
    return numberOfDiskBuffersWritten;
}
