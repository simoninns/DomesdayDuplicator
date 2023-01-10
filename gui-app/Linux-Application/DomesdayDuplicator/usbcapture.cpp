/************************************************************************

    usbcapture.cpp

    Capture application for the Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2018-2019 Simon Inns

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

#include <atomic>
#include <cstring>
#include <sched.h>
#ifdef _WIN32
    #include <memoryapi.h>
#else
    #include <sys/mman.h>
#endif

// Notes on transfer and disk buffering:
//
// TRANSFERSIZE: Each in-flight transfer returns 16 Kbytes * 16 (256 Kbytes)
// SIMULTANEOUSTRANSFERS: There are 16 simultaneous in-flight transfers
// TRANSFERSPERDISKBUFFER: There are 256 transfers per disk buffer (64 Mbytes)
// NUMBEROFDISKBUFFERS: There are 4 disk buffers (256 Mbytes)
//
#define TRANSFERSIZE (16384 * 16)
#define SIMULTANEOUSTRANSFERS 16
#define TRANSFERSPERDISKBUFFER 256
#define NUMBEROFDISKBUFFERS 4

// The number of bytes to capture for amplitude metering.
#define AMPLITUDESIZE (1024 * 2)

// Note:
//
// When saving in 16-bit format, each disk buffer represents 64 Mbytes of data
// When saving in 10-bit format, each disk buffer represents 40 Mbytes of data

// Globals required for libUSB call-back handling ---------------------------------------------------------------------

// Structure to contain the user-data passed during transfer call-backs
struct transferUserDataStruct {
    qint32 diskBufferTransferNumber;    // The transfer number of the transfer (0-2047)
    qint32 diskBufferNumber;            // The current target disk buffer number (0-3)
};

// Flag to indicate if disk buffer processing is running
static std::atomic<bool> isDiskBufferProcessRunning;

// Flag passed to mainwindow to notify of closefile
static bool isOkToRename = false;

// Global to monitor the number of in-flight transfers
static std::atomic<qint32> transfersInFlight(0);

// Flag to cancel transfers in flight
static std::atomic<bool> transferAbort(false);

// Flag to show capture complete
static std::atomic<bool> captureComplete(false);

// Flag to show transfer failure
static std::atomic<bool> transferFailure(false);

// Private variables used to report statistics about the transfer process
struct statisticsStruct {
    std::atomic<qint32> transferCount;  // Number of successful transfers
};
static statisticsStruct statistics;

// Set up a pointer to the disk buffers and their full flags
static unsigned char **diskBuffers = nullptr;
static std::atomic<bool> isDiskBufferFull[NUMBEROFDISKBUFFERS];

// Set up a pointer to the conversion buffer
static unsigned char *conversionBuffer;

// Copies of disk buffers for amplitude processing
static std::atomic<bool> needAmplitudeUpdate(true);
static unsigned char amplitudeBuffers[TRANSFERSIZE * TRANSFERSPERDISKBUFFER][2];
static std::atomic<qint32> currentAmplitudeBuffer(0);

// The flush count is used to set the number of discarded transfers
// before disk buffering starts.  It seems to be necessary to discard
// the first set of in-flight transfers as the FX3 doesn't return
// valid data until second set.
static std::atomic<qint32> flushCounter;

// Last error is a string used to communicate a failure reason to the GUI
static QString lastError;

// LibUSB call-back handling code -------------------------------------------------------------------------------------

// LibUSB transfer call-back handler (called when an in-flight transfer completes)
static void LIBUSB_CALL bulkTransferCallback(struct libusb_transfer *transfer)
{
    // Extract the user data
    transferUserDataStruct *transferUserData = static_cast<transferUserDataStruct *>(transfer->user_data);

    // Check if the transfer has succeeded
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
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

        // Set the transfer failure flag
        lastError = "LibUSB reported a transport failure - ensure the USB device is correctly attached!";
        transferFailure = true;
    }

    // Reduce the number of requests in-flight.
    transfersInFlight--;

    // Increment the total number of successful transfers
    statistics.transferCount++;

    // Are we flushing the buffers or writing to disk?
    if (flushCounter >= SIMULTANEOUSTRANSFERS) {
        // Last transfer in the disk buffer?
        if (transferUserData->diskBufferTransferNumber == (TRANSFERSPERDISKBUFFER - 1)) {
            // Mark the disk buffer as full
            isDiskBufferFull[transferUserData->diskBufferNumber] = true;

            // If transfer is aborting, mark the capture as complete now the disk buffer is full
            if (transferAbort) captureComplete = true;
        }

        // Point to the next slot for the transfer in the disk buffer
        transferUserData->diskBufferTransferNumber += SIMULTANEOUSTRANSFERS;

        // Check that the current disk buffer hasn't been exceeded
        if (transferUserData->diskBufferTransferNumber >= TRANSFERSPERDISKBUFFER) {
            // Select the next disk buffer
            transferUserData->diskBufferNumber++;
            if (transferUserData->diskBufferNumber == NUMBEROFDISKBUFFERS) transferUserData->diskBufferNumber = 0;

            // Ensure selected disk buffer is free
            if (isDiskBufferFull[transferUserData->diskBufferNumber]) {
                // Buffer is full - flag an overflow error
                qDebug() << "bulkTransferCallback(): Disk buffer overflow error!";
                lastError = "Overflow of the disk buffer (your hard-drive/computer's write speed may be too slow)!";
                transferFailure = true;
            }

            // Wrap the transfer number back to the start of the disk buffer
            transferUserData->diskBufferTransferNumber -= TRANSFERSPERDISKBUFFER;
        }
    } else {
        // Only flushing the buffer at the moment
        flushCounter++;
    }

    // If the capture is not complete, resubmit the transfer to libUSB
    if (!captureComplete) {
        libusb_fill_bulk_transfer(transfer, transfer->dev_handle, transfer->endpoint,
                                  reinterpret_cast<unsigned char *>(diskBuffers[transferUserData->diskBufferNumber] +
                                  (TRANSFERSIZE * transferUserData->diskBufferTransferNumber)),
                                  transfer->length, bulkTransferCallback,
                                  transfer->user_data, 1000);

        if (libusb_submit_transfer(transfer) == 0) {
            transfersInFlight++;
        } else {
            qDebug() << "bulkTransferCallback(): Transfer re-submission failed!";
            lastError = "LibUSB reported that a transfer re-submission failed - ensure the USB device is correctly attached!";
            transferFailure = true;
        }
    }
}


// UsbCapture class code ----------------------------------------------------------------------------------------------

UsbCapture::UsbCapture(QObject *parent, libusb_context *libUsbContextParam,
                       libusb_device_handle *usbDeviceHandleParam,
                       QString filenameParam, bool isCaptureFormat10BitParam,
                       bool isCaptureFormat10BitDecimatedParam, bool isTestDataParam)
    : QThread(parent)
{
    // Set the libUSB context
    libUsbContext = libUsbContextParam;

    // Set the device's handle
    usbDeviceHandle = usbDeviceHandleParam;

    if (usbDeviceHandle == nullptr) qDebug() << "UsbCapture::UsbCapture(): ERROR, passed usb device handle is not valid!";
    if (libUsbContext == nullptr) qDebug() << "UsbCapture::UsbCapture(): ERROR, passed usb context is not valid!";

    // Store the requested file name
    filename = filenameParam;

    // Set the last error string
    lastError = tr("none");

    // Store the requested data format
    isCaptureFormat10Bit = isCaptureFormat10BitParam;
    isCaptureFormat10BitDecimated = isCaptureFormat10BitDecimatedParam;
    isTestData = isTestDataParam;

    // Set the transfer abort flag
    transferAbort = false;
    captureComplete = false;

    // Reset transfer statistics
    statistics.transferCount = 0;
    numberOfDiskBuffersWritten = 0;

    // Clear the amplitude buffers
    memset(amplitudeBuffers, 0, sizeof amplitudeBuffers);

    // Initialise the sample and test data sequences
    sequenceState = SEQUENCE_SYNC;
    savedTestDataValue = -1;
    testDataMax = -1;

    // Clear the transfer failure flag
    transferFailure = false;

    // Set the flush counter
    flushCounter = 0;

    // Set the disk process thread running flag
    isDiskBufferProcessRunning = false;
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
    usbTransfers = static_cast<struct libusb_transfer **>(calloc(SIMULTANEOUSTRANSFERS, sizeof(struct libusb_transfer *)));

    // Set up the user-data for the transfers
    transferUserDataStruct transferUserData[SIMULTANEOUSTRANSFERS];

    // Set up the USB transfer buffers
    qDebug() << "UsbCapture::run(): Setting up the transfers";

    // Allocate the memory required for the disk buffers
    allocateDiskBuffers();

    // Launch a thread for writing disk buffers to disk
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QFuture<void> future = QtConcurrent::run(this, &UsbCapture::runDiskBuffers);
#else
    QFuture<void> future = QtConcurrent::run(&UsbCapture::runDiskBuffers, this);
#endif
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));

    // Claim the required USB device interface for the transfer
    qint32 claimResult = libusb_claim_interface(usbDeviceHandle, 0);
    if (claimResult < 0) {
        qDebug() << "UsbCapture::run(): USB interface claim failed (connected via USB2?) with error:" << libusb_error_name(claimResult);
        lastError = tr("Could not claim USB interface - Ensure the Duplicator is plugged into a USB3 port - LibUSB reports: ") + libusb_error_name(claimResult);
        transferFailure = true;

        // We can't continue... clean-up and give up
        emit transferFailed();
        freeDiskBuffers();
        return;
    }

    // Save the current scheduling policy and parameters
#ifdef _WIN32
    // TODO: Implement pthread-win32 for scheduling on Windows
#else
    int oldSchedPolicy = sched_getscheduler(0);
    if (oldSchedPolicy == -1) oldSchedPolicy = SCHED_OTHER;
    struct sched_param oldSchedParam;
    if (sched_getparam(0, &oldSchedParam) == -1) oldSchedParam.sched_priority = 0;
#endif

    // Enable real-time scheduling for this thread
    int minSchedPriority = sched_get_priority_min(SCHED_RR);
    int maxSchedPriority = sched_get_priority_max(SCHED_RR);
    struct sched_param schedParams;
    if (minSchedPriority == -1 || maxSchedPriority == -1) {
        schedParams.sched_priority = 0;
    } else {
        // Put the priority about 3/4 of the way through its range
        schedParams.sched_priority = (minSchedPriority + (3 * maxSchedPriority)) / 4;
    }
    if (sched_setscheduler(0, SCHED_RR, &schedParams) != -1) {
        qDebug() << "UsbCapture::run(): Real-time scheduling enabled with priority" << schedParams.sched_priority;
    } else {
        // Continue anyway, but print a warning
        qInfo() << "UsbCapture::run(): Unable to enable real-time scheduling for capture thread";
    }

    // Set up the initial transfers
    for (qint32 transferNumber = 0; transferNumber < SIMULTANEOUSTRANSFERS; transferNumber++) {
        usbTransfers[transferNumber] = libusb_alloc_transfer(0);

        // Check USB transfer allocation was successful
        if (usbTransfers[transferNumber] == nullptr) {
            qDebug() << "UsbCapture::run(): LibUSB alloc failed for transfer number" << transferNumber;
            lastError = tr("Failed to allocated required memory for transfer!");
            transferFailure = true;
        } else {
            // Set up the user-data for the initial transfers
            transferUserData[transferNumber].diskBufferTransferNumber = transferNumber;
            transferUserData[transferNumber].diskBufferNumber = 0;

            // Set transfer flag to cause transfer error if there is a short packet
            usbTransfers[transferNumber]->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;

            // Configure the transfer with a 1 second timeout (targeted to disk buffer 0)
            libusb_fill_bulk_transfer(usbTransfers[transferNumber], usbDeviceHandle, 0x81,
                                      reinterpret_cast<unsigned char *>(diskBuffers[0] + (TRANSFERSIZE * transferNumber)),
                                      TRANSFERSIZE, bulkTransferCallback, &transferUserData[transferNumber], 1000);
        }
    }

    if (!transferFailure) {
        // Submit the transfers via libUSB
        qDebug() << "UsbCapture::run(): Submitting the transfers";
        for (qint32 currentTransferNumber = 0; currentTransferNumber < SIMULTANEOUSTRANSFERS; currentTransferNumber++) {
            qint32 resultCode = libusb_submit_transfer(usbTransfers[currentTransferNumber]);

            if (resultCode >= 0) {
                transfersInFlight++;
            } else {
                qDebug() << "UsbCapture::run(): Transfer launch" << currentTransferNumber << "failed with error:" << libusb_error_name(resultCode);
                lastError = tr("Could not launch USB transfer processes - LibUSB reports: ") + libusb_error_name(resultCode);
                transferFailure = true;
            }

            qDebug() << "UsbCapture::run():" << transfersInFlight << "simultaneous transfers launched.";
        }
    }

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
    if (!transferFailure) qDebug() << "UsbCapture::run(): Transfer stopping - waiting for in-flight transfers to complete...";
    else qDebug() << "UsbCapture::run(): Transfer failing - waiting for in-flight transfers to complete...";
    transferAbort = true;

    while(transfersInFlight > 0) {
        // Process libUSB events
        libusb_handle_events_timeout(libUsbContext, &libusbHandleTimeout);
    }

#ifdef _WIN32
    // TODO: Implement pthread-win32 for scheduling on Windows
#else
    // Return to the original scheduling policy while we're cleaning up
    if (sched_setscheduler(0, oldSchedPolicy, &oldSchedParam) == -1) {
        qDebug() << "UsbCapture::run(): Unable to restore original scheduling policy";
    }
#endif

    // Deallocate transfers
    qDebug() << "UsbCapture::run(): Transfer stopping - Freeing transfer buffers...";
    for (qint32 transferNumber = 0; transferNumber < SIMULTANEOUSTRANSFERS; transferNumber++)
         libusb_free_transfer(usbTransfers[transferNumber]);

    // Aborting transfer - wait for disk buffer processing thread to complete
    qDebug() << "UsbCapture::run(): Transfer stopping - waiting for disk buffer processing to complete...";
    while (isDiskBufferProcessRunning) {
        // Just waiting here for now
        sched_yield();
    }

    // If the transfer failed, emit a notification signal to the parent object
    if (transferFailure) {
        qDebug() << "UsbCapture::run(): Transfer failed - emitting notification signal";
        emit transferFailed();
    }

    // Release the USB interface
    qint32 releaseResult = libusb_release_interface(usbDeviceHandle, 0);
    if (releaseResult < 0) {
        qDebug() << "UsbCapture::run(): USB interface release failed with error:" << libusb_error_name(releaseResult);
    }

    // Free the disk buffers
    freeDiskBuffers();

    // All done
    qDebug() << "UsbCapture::run(): Transfer complete," << statistics.transferCount << "transfers performed with" <<
                numberOfDiskBuffersWritten << "disk buffers written";
}

// Allocate memory for the disk buffers
// Note: Using vectors would be neater, but they are just too slow
void UsbCapture::allocateDiskBuffers(void)
{
    qDebug() << "UsbCapture::allocateDiskBuffers(): Allocating" << (1ULL * TRANSFERSIZE * TRANSFERSPERDISKBUFFER * NUMBEROFDISKBUFFERS) / (1024 * 1024) << "MiB memory for disk buffers";
    // Allocate the disk buffers
    diskBuffers = static_cast<unsigned char **>(calloc(NUMBEROFDISKBUFFERS, sizeof(unsigned char *)));
    if (diskBuffers != nullptr) {
        bool tryMlock = true;
        for (quint32 bufferNumber = 0; bufferNumber < NUMBEROFDISKBUFFERS; bufferNumber++) {

            diskBuffers[bufferNumber] = static_cast<unsigned char *>(malloc(TRANSFERSIZE * TRANSFERSPERDISKBUFFER));
            isDiskBufferFull[bufferNumber] = false;

            if (diskBuffers[bufferNumber] == nullptr) {
                // Memory allocation has failed
                qDebug() << "UsbCapture::allocateDiskBuffers(): Disk buffer memory allocation failed!";
                lastError = tr("Failed to allocated required memory for disk buffers!");
                transferFailure = true;
                break;
            }

            // Lock the buffer into memory, preventing it from being paged out
#ifdef _WIN32
            if (tryMlock && VirtualLock(diskBuffers[bufferNumber], TRANSFERSIZE * TRANSFERSPERDISKBUFFER) == 0) {
#else
            if (tryMlock && mlock(diskBuffers[bufferNumber], TRANSFERSIZE * TRANSFERSPERDISKBUFFER) == -1) {
#endif
                // Continue anyway, but print a warning
                qInfo() << "UsbCapture::allocateDiskBuffers(): Unable to lock disk buffer into memory";
                tryMlock = false;
            }
        }
        if (tryMlock) qDebug() << "UsbCapture::allocateDiskBuffers(): Locked disk buffers into memory";
    } else {
        // Memory allocation has failed
        qDebug() << "UsbCapture::allocateDiskBuffers(): Disk buffer array allocation failed!";
        lastError = tr("Failed to allocated required memory for disk buffers!");
        transferFailure = true;
    }

    // Allocate the conversion buffer
    conversionBuffer = static_cast<unsigned char *>(malloc(TRANSFERSIZE * TRANSFERSPERDISKBUFFER));
    if (conversionBuffer == nullptr) {
        qDebug() << "UsbCapture::allocateDiskBuffers(): Conversion buffer memory allocation failed!";
        lastError = tr("Failed to allocated required memory for data conversion buffers!");
        transferFailure = true;
    }
}

// Free memory used for the disk buffers
void UsbCapture::freeDiskBuffers(void)
{
    qDebug() << "UsbCapture::freeDiskBuffers(): Freeing disk buffer memory";
    // Free up the allocated disk buffers
    if (diskBuffers != nullptr) {
        for (qint32 bufferNumber = 0; bufferNumber < NUMBEROFDISKBUFFERS; bufferNumber++) {
            // Don't keep the buffer in RAM any more (silently ignoring failure)
#ifdef _WIN32
            (void) VirtualUnlock(diskBuffers[bufferNumber], TRANSFERSIZE * TRANSFERSPERDISKBUFFER);
#else
            (void) munlock(diskBuffers[bufferNumber], TRANSFERSIZE * TRANSFERSPERDISKBUFFER);
#endif

            if (diskBuffers[bufferNumber] != nullptr) {
                free(diskBuffers[bufferNumber]);
            }
            diskBuffers[bufferNumber] = nullptr;
        }
        free(diskBuffers);
        diskBuffers = nullptr;
    }

    // Free up the temporary disk buffer
    free(conversionBuffer);
    conversionBuffer = nullptr;

    qDebug() << "Setting finished variable for mainwindow";
    isOkToRename = true;
}

// Thread for processing disk buffers
void UsbCapture::runDiskBuffers(void)
{
    qDebug() << "UsbCapture::runDiskBuffers(): Thread started";

    // Open the capture file
    QFile outputFile(filename);
    if(!outputFile.open(QFile::WriteOnly | QFile::Truncate)) {
        qDebug() << "UsbCapture::runDiskBuffers(): Could not open destination capture file for writing!";
        lastError = tr("Failed to open destination file for the capture.  Ensure the destination directory is valid and that you have write permissions");
        transferFailure = true;
    }

    // Process the disk buffers until the transfer is complete or fails
    isDiskBufferProcessRunning = true;
    while(!captureComplete && !transferFailure) {
        for (qint32 diskBufferNumber = 0; diskBufferNumber < NUMBEROFDISKBUFFERS; diskBufferNumber++) {
            if (isDiskBufferFull[diskBufferNumber] && !transferFailure) {
                // Write the buffer
                if (transferAbort) qDebug() << "UsbCapture::runDiskBuffers(): Transfer abort flagged, writing disk buffer" << diskBufferNumber;
                writeBufferToDisk(&outputFile, diskBufferNumber);

                // Mark it as empty
                isDiskBufferFull[diskBufferNumber] = false;

                // Increment the statistics
                numberOfDiskBuffersWritten++;
            } else if (!captureComplete) {
                // Sleep the thread for 100 uS to keep the CPU usage down
                usleep(100);

                // Then try the same buffer again, so we don't skip any
                diskBufferNumber--;
            }

            // Check for transfer failure before continuing...
            if (transferFailure) break;
        }
    }

    // Ensure all disk buffers are written before quitting the thread
    if (!transferFailure) {
        for (qint32 diskBufferNumber = 0; diskBufferNumber < NUMBEROFDISKBUFFERS; diskBufferNumber++) {
            if (isDiskBufferFull[diskBufferNumber] && !transferFailure) {
                // Write the buffer
                qDebug() << "UsbCapture::runDiskBuffers(): Capture complete flagged, writing disk buffer" << diskBufferNumber;
                writeBufferToDisk(&outputFile, diskBufferNumber);

                // Mark it as empty
                isDiskBufferFull[diskBufferNumber] = false;

                // Increment the statistics
                numberOfDiskBuffersWritten++;
            }
        }
    }

    // Close the capture file. QFile::close ignores errors, so flush first.
    if (!outputFile.flush()) {
        lastError = tr("Unable to write captured data to the destination file");
        transferFailure = true;
    }
    outputFile.close();

    // Flag that the thread is complete
    isDiskBufferProcessRunning = false;
    qDebug() << "UsbCapture::runDiskBuffers(): Thread stopped";
}

// Check and remove sequence numbers from a buffer
void UsbCapture::checkBufferSequence(qint32 diskBufferNumber)
{
    static constexpr qint32 COUNTER_SHIFT = 16;
    static constexpr quint32 COUNTER_MAX = 63;

    // If we have no work to do, return immediately
    if (sequenceState == SEQUENCE_DISABLED || sequenceState == SEQUENCE_FAILED) return;

    unsigned char *diskBuffer = diskBuffers[diskBufferNumber];
    quint32 sequenceCounter;

    if (sequenceState == SEQUENCE_SYNC) {
        // Determine the initial value of sequenceCounter, based on the contents of diskBuffer

        sequenceCounter = 0xFFFFFFFF;

        // Get the first sequence number from the buffer
        quint32 prevSeqNum = diskBuffer[1] >> 2;

        // Find the first time the sequence number changes.
        // Since each sequence number appears on (1 << COUNTER_SHIFT) samples,
        // at worst we will see a change within (1 << COUNTER_SHIFT) + 1
        // samples.
        for (qint32 pointer = 2; pointer < ((1 << COUNTER_SHIFT) + 1) * 2; pointer += 2) {
            quint32 seqNum = diskBuffer[pointer + 1] >> 2;
            if (seqNum != prevSeqNum) {
                // Found it -- compute sequenceCounter's value at the start of the buffer
                if (seqNum == 0) seqNum = COUNTER_MAX;
                sequenceCounter = (seqNum << COUNTER_SHIFT) - (pointer / 2);
                break;
            }
            prevSeqNum = seqNum;
        }

        if (sequenceCounter == 0xFFFFFFFF) {
            // No sequence number change detected
            qDebug() << "UsbCapture::checkBufferSequence(): Data does not include sequence numbers";
            sequenceState = SEQUENCE_DISABLED;
            return;
        }

        qDebug() << "UsbCapture::checkBufferSequence(): Synchronised with data sequence numbers";
        sequenceState = SEQUENCE_RUNNING;
    } else {
        sequenceCounter = savedSequenceCounter;
    }

    // We must now be in the SEQUENCE_RUNNING state.
    // Check diskBuffer's sequence numbers match sequenceCounter.
    for (qint32 pointer = 0; pointer < (TRANSFERSIZE * TRANSFERSPERDISKBUFFER); pointer += 2) {
        const quint32 expected = sequenceCounter >> COUNTER_SHIFT;
        const quint32 seqNum = diskBuffer[pointer + 1] >> 2;
        if (seqNum != expected) {
            qDebug() << "UsbCapture::checkBufferSequence(): Sequence number mismatch - expecting" << expected << "but got" << seqNum;
            sequenceState = SEQUENCE_FAILED;
            savedSequenceCounter = sequenceCounter;
            return;
        }

        // Update sequenceCounter
        sequenceCounter++;
        if (sequenceCounter == (COUNTER_MAX << COUNTER_SHIFT)) sequenceCounter = 0;

        // Remove the sequence number
        diskBuffer[pointer + 1] &= 3;
    }

    savedSequenceCounter = sequenceCounter;
}

// Write a disk buffer to disk
void UsbCapture::writeBufferToDisk(QFile *outputFile, qint32 diskBufferNumber)
{
    checkBufferSequence(diskBufferNumber);
    if (sequenceState == SEQUENCE_FAILED) {
        lastError = tr("Sequence number mismatch -- samples have been lost from input!");
        transferFailure = true;
        return;
    }

    // Is this test data?
    if (isTestData) {
        // Verify the data
        qint32 currentValue = savedTestDataValue;

        if (currentValue == -1) {
            // Initialiase currentValue from the first value
            currentValue = diskBuffers[diskBufferNumber][0] + (diskBuffers[diskBufferNumber][1] * 256);
        }

        for (qint32 pointer = 0; pointer < (TRANSFERSIZE * TRANSFERSPERDISKBUFFER); pointer += 2) {
            // Get the original 10-bit unsigned value from the disk data buffer
            qint32 originalValue = diskBuffers[diskBufferNumber][pointer];
            originalValue += diskBuffers[diskBufferNumber][pointer + 1] * 256;

            if (currentValue != originalValue) {
                // If this is the first time the test sequence has wrapped around to 0,
                // detect the length of the sequence: either 1021 (newer FPGA firmware)
                // or 1024 (older FPGA firmware).
                if (testDataMax == -1 && originalValue == 0 && (currentValue == 1021 || currentValue == 1024)) {
                    testDataMax = currentValue;
                    currentValue = 1;
                    continue;
                }

                // Data error
                qDebug() << "UsbCapture::writeBufferToDisk(): Data error! Expecting" << currentValue << "but got" << originalValue;
                lastError = tr("Test data verification error!");
                transferFailure = true;
                return;
            }

            // Update currentValue
            currentValue++;
            if (currentValue == testDataMax) currentValue = 0;
        }

        savedTestDataValue = currentValue;
    }

    // Write the data in 10 or 16 bit format
    if (isCaptureFormat10Bit) {
        if (!isCaptureFormat10BitDecimated) {
            // Translate the data in the disk buffer to unsigned 10-bit packed data
            quint32 conversionBufferPointer = 0;

            for (qint32 diskBufferPointer = 0; diskBufferPointer < (TRANSFERSIZE * TRANSFERSPERDISKBUFFER); diskBufferPointer += 8) {
                quint32 originalWords[4];

                // Get the original 4 10-bit words
                originalWords[0]  = diskBuffers[diskBufferNumber][diskBufferPointer + 0];
                originalWords[0] += diskBuffers[diskBufferNumber][diskBufferPointer + 1] * 256;
                originalWords[1]  = diskBuffers[diskBufferNumber][diskBufferPointer + 2];
                originalWords[1] += diskBuffers[diskBufferNumber][diskBufferPointer + 3] * 256;
                originalWords[2]  = diskBuffers[diskBufferNumber][diskBufferPointer + 4];
                originalWords[2] += diskBuffers[diskBufferNumber][diskBufferPointer + 5] * 256;
                originalWords[3]  = diskBuffers[diskBufferNumber][diskBufferPointer + 6];
                originalWords[3] += diskBuffers[diskBufferNumber][diskBufferPointer + 7] * 256;

                // Convert into 5 bytes of packed 10-bit data
                conversionBuffer[conversionBufferPointer + 0]  = static_cast<unsigned char>((originalWords[0] & 0x03FC) >> 2);
                conversionBuffer[conversionBufferPointer + 1]  = static_cast<unsigned char>((originalWords[0] & 0x0003) << 6);
                conversionBuffer[conversionBufferPointer + 1] += static_cast<unsigned char>((originalWords[1] & 0x03F0) >> 4);
                conversionBuffer[conversionBufferPointer + 2]  = static_cast<unsigned char>((originalWords[1] & 0x000F) << 4);
                conversionBuffer[conversionBufferPointer + 2] += static_cast<unsigned char>((originalWords[2] & 0x03C0) >> 6);
                conversionBuffer[conversionBufferPointer + 3]  = static_cast<unsigned char>((originalWords[2] & 0x003F) << 2);
                conversionBuffer[conversionBufferPointer + 3] += static_cast<unsigned char>((originalWords[3] & 0x0300) >> 8);
                conversionBuffer[conversionBufferPointer + 4]  = static_cast<unsigned char>((originalWords[3] & 0x00FF));

                // Increment the conversion buffer pointer
                conversionBufferPointer += 5;
            }

            // Write the conversion buffer to disk
            writeConversionBuffer(outputFile, conversionBufferPointer);
        } else {
            // Translate the data in the disk buffer to unsigned 10-bit packed data with 4:1 decimation
            quint32 conversionBufferPointer = 0;

            for (qint32 diskBufferPointer = 0; diskBufferPointer < (TRANSFERSIZE * TRANSFERSPERDISKBUFFER); diskBufferPointer += (8 * 4)) {
                quint32 originalWords[4];

                // Get the original 4 10-bit words
                originalWords[0]  = diskBuffers[diskBufferNumber][diskBufferPointer + 0];
                originalWords[0] += diskBuffers[diskBufferNumber][diskBufferPointer + 1] * 256;

                originalWords[1]  = diskBuffers[diskBufferNumber][diskBufferPointer + 2 + 4];
                originalWords[1] += diskBuffers[diskBufferNumber][diskBufferPointer + 3 + 4] * 256;

                originalWords[2]  = diskBuffers[diskBufferNumber][diskBufferPointer + 4 + 8];
                originalWords[2] += diskBuffers[diskBufferNumber][diskBufferPointer + 5 + 8] * 256;

                originalWords[3]  = diskBuffers[diskBufferNumber][diskBufferPointer + 6 + 12];
                originalWords[3] += diskBuffers[diskBufferNumber][diskBufferPointer + 7 + 12] * 256;

                // Convert into 5 bytes of packed 10-bit data
                conversionBuffer[conversionBufferPointer + 0]  = static_cast<unsigned char>((originalWords[0] & 0x03FC) >> 2);
                conversionBuffer[conversionBufferPointer + 1]  = static_cast<unsigned char>((originalWords[0] & 0x0003) << 6);
                conversionBuffer[conversionBufferPointer + 1] += static_cast<unsigned char>((originalWords[1] & 0x03F0) >> 4);
                conversionBuffer[conversionBufferPointer + 2]  = static_cast<unsigned char>((originalWords[1] & 0x000F) << 4);
                conversionBuffer[conversionBufferPointer + 2] += static_cast<unsigned char>((originalWords[2] & 0x03C0) >> 6);
                conversionBuffer[conversionBufferPointer + 3]  = static_cast<unsigned char>((originalWords[2] & 0x003F) << 2);
                conversionBuffer[conversionBufferPointer + 3] += static_cast<unsigned char>((originalWords[3] & 0x0300) >> 8);
                conversionBuffer[conversionBufferPointer + 4]  = static_cast<unsigned char>((originalWords[3] & 0x00FF));

                // Increment the conversion buffer pointer
                conversionBufferPointer += 5;
            }

            // Write the conversion buffer to disk
            writeConversionBuffer(outputFile, conversionBufferPointer);
        }
    } else {
        // Translate the data in the disk buffer to scaled 16-bit signed data
        for (qint32 pointer = 0; pointer < (TRANSFERSIZE * TRANSFERSPERDISKBUFFER); pointer += 2) {
            // Get the original 10-bit unsigned value from the disk data buffer
            quint32 originalValue = diskBuffers[diskBufferNumber][pointer];
            originalValue += diskBuffers[diskBufferNumber][pointer+1] * 256;

            // Sign and scale the data to 16-bits
            qint32 signedValue = static_cast<qint32>(originalValue - 512);
            signedValue = signedValue * 64;

            conversionBuffer[pointer] = static_cast<unsigned char>(signedValue & 0x00FF);
            conversionBuffer[pointer+1] = static_cast<unsigned char>((signedValue & 0xFF00) >> 8);
        }

        // Write the conversion buffer to disk
        writeConversionBuffer(outputFile, TRANSFERSIZE * TRANSFERSPERDISKBUFFER);
    }

    // Has getAmplitudeBuffer requested a new buffer?
    if (needAmplitudeUpdate) {
        // Copy the start of this disk buffer into the amplitude buffer that's not currently being read
        qint32 num = 1 - currentAmplitudeBuffer;
        memcpy(amplitudeBuffers[num], diskBuffers[diskBufferNumber], AMPLITUDESIZE);

        // Flip the buffers
        currentAmplitudeBuffer = num;
        needAmplitudeUpdate = false;
    }
}

void UsbCapture::writeConversionBuffer(QFile *outputFile, qint32 numBytes)
{
    qint64 bytesWritten = outputFile->write(reinterpret_cast<const char *>(conversionBuffer), numBytes);
    //qDebug() << "UsbCapture::writeBufferToDisk(): 10-bit - Written" << bytesWritten << "bytes to disk";

    // Check for a short write (which shouldn't happen, because outputFile is buffered) or a filesystem error
    if (bytesWritten != numBytes) {
        lastError = tr("Unable to write captured data to the destination file");
        transferFailure = true;
    }
}

// Start capturing
void UsbCapture::startTransfer(void)
{
    // Flip isOkToRename back to false; new capture
    isOkToRename = false;
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

// Return the last error text
QString UsbCapture::getLastError(void)
{
    return lastError;
}

// Return capture is complete and buffers are empty
bool UsbCapture::getOkToRename()
{
    return isOkToRename;
}

// Get a copy of a recent disk buffer for amplitude processing, and its length in bytes
void UsbCapture::getAmplitudeBuffer(const unsigned char **buffer, qint32 *numBytes)
{
    // Get the most-recently-updated buffer (reading the atomic, to ensure this thread has seen the writes to it)
    *buffer = amplitudeBuffers[currentAmplitudeBuffer];
    *numBytes = AMPLITUDESIZE;

    // Tell the capture thread to grab a new buffer
    needAmplitudeUpdate = true;
}
