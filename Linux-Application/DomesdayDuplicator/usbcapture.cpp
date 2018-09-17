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

// LibUSB call-back handling code -------------------------------------------------------------------------------------

// Structure to contain the user-data passed during transfer call-backs
struct transferUserDataStruct {
    qint32 transferNumber;         // The transfer number of the transfer
};

// Global to monitor the number of in-flight transfers
static volatile qint32 transfersInFlight = 0;

// Flag to cancel transfers in flight
static volatile bool transferAbort = false;

// Private variables used to report statistics about the transfer process
struct statisticsStruct {
    qint32 transferCount;          // Number of successful transfers
};
static volatile statisticsStruct statistics;

// LibUSB transfer call-back handler (called when an in-flight transfer completes)
static void LIBUSB_CALL bulkTransferCallback(struct libusb_transfer *transfer)
{
    // Extract the user data
    transferUserDataStruct *transferUserData = static_cast<transferUserDataStruct *>(transfer->user_data);
    qint32 transferNumber = transferUserData->transferNumber;

    // Check if the transfer has succeeded
    if ((transfer->status != LIBUSB_TRANSFER_COMPLETED) && !transferAbort) {
        // Show the failure reason in the debug
        switch (transfer->status) {
            case LIBUSB_TRANSFER_ERROR:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_ERROR - Transfer" << transferNumber << "failed";
            break;

            case LIBUSB_TRANSFER_TIMED_OUT:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_TIMED_OUT - Transfer" << transferNumber << "timed out";
            break;

            case LIBUSB_TRANSFER_CANCELLED:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_CANCELLED - Transfer" << transferNumber << "was cancelled";
            break;

            case LIBUSB_TRANSFER_STALL:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_STALL - Transfer" << transferNumber << "Endpoint stalled";
            break;

            case LIBUSB_TRANSFER_NO_DEVICE:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_NO_DEVICE - Transfer" << transferNumber << "- Device disconnected";
            break;

            case LIBUSB_TRANSFER_OVERFLOW:
            qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER_OVERFLOW - Transfer" << transferNumber << "- Device overflow";
            break;

            default:
                qDebug() << "bulkTransferCallback(): LIBUSB_TRANSFER - Transfer" << transferNumber << " - Unknown error";
        }
    }

    // Reduce the number of requests in-flight.
    transfersInFlight--;

    // Increment the total number of successful transfers
    statistics.transferCount++;

    // Do something with the data here...

    // If the transfer has not been aborted, resubmit the transfer to libUSB
    if (!transferAbort) {
        libusb_fill_bulk_transfer(transfer, transfer->dev_handle, transfer->endpoint,
                                  transfer->buffer, transfer->length, bulkTransferCallback,
                                  transfer->user_data, 1000);

        if (libusb_submit_transfer(transfer) == 0) {
            transfersInFlight++;
        } else {
            qDebug() << "bulkTransferCallback(): Transfer re-submission failed!";
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
    usbTransfers = static_cast<struct libusb_transfer **>(calloc (16, sizeof (struct libusb_transfer *)));

    // Set up the user-data for the transfers
    QVector<transferUserDataStruct> transferUserData;
    transferUserData.resize(16);

    // Set up the USB transfer buffers
    // Buffers are 8Kwords
    // Number of buffers is 16
    qDebug() << "UsbCapture::run(): Setting up the transfers";
    QVector< QVector<quint8> > usbBuffers(16);
    for (qint32 bufferNumber = 0; bufferNumber < 16; bufferNumber++) {
        usbBuffers[bufferNumber].resize(16384);
        usbTransfers[bufferNumber] = libusb_alloc_transfer(0);

        // Check USB transfer allocation was successful
        if (usbTransfers[bufferNumber] == nullptr) {
            qDebug() << "UsbCapture::run(): LibUSB alloc failed for transfer number" << bufferNumber;
        } else {
            // Set up the initial transfer requests

            // Set up the user-data for the transfer
            transferUserData[bufferNumber].transferNumber = bufferNumber;

            // Set transfer flag to cause transfer error if there is a short packet
            usbTransfers[bufferNumber]->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;

            // Configure the transfer with a 1 second timeout
            libusb_fill_bulk_transfer(usbTransfers[bufferNumber], usbDeviceHandle, 0x81,
                                      reinterpret_cast<unsigned char *>(usbBuffers[bufferNumber].data()),
                                      16384, bulkTransferCallback, &transferUserData[bufferNumber], 1000);
        }
    }

    // Submit the transfers via libUSB
    qDebug() << "UsbCapture::run(): Submitting the transfers";
    for (qint32 currentTransferNumber = 0; currentTransferNumber < 16; currentTransferNumber++) {
        if (libusb_submit_transfer(usbTransfers[currentTransferNumber]) == 0) {
            transfersInFlight++;
        } else {
            qDebug() << "UsbCapture::run(): Transfer launch" << transfersInFlight << "failed!";
        }
    }

    // Use a 1 second timeout for the libusb_handle_events_timeout call
    struct timeval libusbHandleTimeout;
    libusbHandleTimeout.tv_sec  = 1;
    libusbHandleTimeout.tv_usec = 0;

    // Perform background tasks whilst transfers are proceeding
    while(!transferAbort) {
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

    // All done
    qDebug() << "UsbCapture::run(): Transfer complete," << statistics.transferCount << "transfers performed.";
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
