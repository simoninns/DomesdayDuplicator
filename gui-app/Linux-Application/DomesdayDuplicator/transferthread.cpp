/************************************************************************

    transferthread.cpp

    QT/Linux RF Capture application streaming transfer thread handler
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


#include "transferthread.h"

// Public functions
void transferThread::setParameters(
    unsigned int endpointParameter, //0
    unsigned int endpointTypeParameter, //? (type of transfer)
    unsigned int packetSizeParameter, //16
    unsigned int requestSizeParameter, //16
    unsigned int queueDepthParameter, //16
    cyusb_handle* deviceHandleParameter)
{
    qDebug() << "transferThread::setParameters(): Called";

    // Default all status variables
    successCount = 0;           // Number of successful transfers
    failureCount = 0;           // Number of failed transfers
    transferSize = 0;           // Size of data transfers performed so far
    transferIndex = 0;          // Write index into the transfer_size array
    transferPerformance = 0;	// Performance in KBps
    stopTransferRequest = false;// Request to stop data transfers
    requestsInFlight = 0;       // Number of transfers that are in progress
    transferRunning = false;	// Transfer running flag

    // Copy parameters to the objects private storage
    endpoint = endpointParameter;
    endpointType = endpointTypeParameter;
    packetSize = packetSizeParameter;
    requestSize = requestSizeParameter;
    queueDepth = queueDepthParameter;
    deviceHandle = deviceHandleParameter;
}

// Get the current success count
unsigned int transferThread::getSuccessCount(void)
{
    qDebug() << "transferThread::getSuccessCount(): Success count = " << successCount;
    return successCount;
}

// Get the current failure count
unsigned int transferThread::getFailureCount(void)
{
    qDebug() << "transferThread::getFailureCount(): Failure count = " << successCount;
    return failureCount;
}

// Get the current performance count
unsigned int transferThread::getPerformanceCount(void)
{
    qDebug() << "transferThread::getPerformanceCount(): Transfer performance = " << successCount;
    return transferPerformance;
}

// Stop the transfer
void transferThread::stop(void)
{
    qDebug() << "transferThread::stop(): Stopping the transfer request";
    stopTransferRequest = true;
}

// Return a flag showing if a transfer is running
bool transferThread::isRunning(void)
{
    return transferRunning;
}

// This is the call back function called by libusb upon completion of a queued data transfer.
void LIBUSB_CALL transferThread::transferCallback(struct libusb_transfer *transfer)
{
    qDebug() << "transferThread::transferCallback(): Called - emitting signal";

    // Ensure that the transfer structure is valid
    if (!transfer || !transfer->user_data) return;

    // Send a signal

}

// Run function for transfer
void transferThread::run(void)
{
    int transferReturnStatus = 0; // Status return from libUSB

    qDebug() << "transferThread::run(): Called - allocated thread ID" << thread()->currentThreadId();;

    // Check for validity of the device handle
    if (deviceHandle == NULL) {
        qDebug() << "transferThread::run(): USB device handle is invalid!";
        return;
    }

    // The endpoint is already found and its properties are known.
    qDebug() << "Starting transfer thread with the following parameters";
    qDebug() << "\tEndpoint         :" << endpoint;
    qDebug() << "\tEndpoint type    :" << endpointType;
    qDebug() << "\tMax packet size  :" << packetSize;
    qDebug() << "\tRequest size     :" << requestSize;
    qDebug() << "\tQueue depth      :" << queueDepth;

    // Array for the data
    QByteArray *dataBuffers = new QByteArray;

    // Array for the transfer structure
    QByteArray *transfers = new QByteArray;

    // Record the timestamp at start of transfer (for statistics generation)
    gettimeofday(&startTimestamp, NULL); // Might be better to use the QT library for this?

    // Flag as running
    transferRunning = true;
    stopTransferRequest = false;

    // Launch the transfers
    for (unsigned int launchCounter = 0; launchCounter < queueDepth; launchCounter++) {
        qDebug() << "transferThread::run(): Launching transfer number" << launchCounter << "of" << queueDepth;

        switch (endpointType) {
            case LIBUSB_TRANSFER_TYPE_BULK:
                qDebug() << "transferThread::run(): Endpoint type is LIBUSB_TRANSFER_TYPE_BULK";
                libusb_fill_bulk_transfer((libusb_transfer *)transfers->data(), deviceHandle, endpoint,
                        (unsigned char *)dataBuffers->data(), requestSize * packetSize, transferThread::transferCallback, NULL, 5000);
                transferReturnStatus = libusb_submit_transfer((libusb_transfer *)transfers->data());
                if (transferReturnStatus == 0) requestsInFlight++;
                qDebug() << "transferThread::run(): Launched";
                break;

            case LIBUSB_TRANSFER_TYPE_INTERRUPT:
                qDebug() << "transferThread::run(): Endpoint type is LIBUSB_TRANSFER_TYPE_INTERRUPT";
//                libusb_fill_interrupt_transfer (transfers[i], deviceHandle, endpoint,
//                        databuffers[i], requestSize * packetSize, streamer::transferCallback, NULL, 5000);
//                transferReturnStatus = libusb_submit_transfer (transfers[i]);
                if (transferReturnStatus == 0) requestsInFlight++;
                break;

            case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
                qDebug() << "transferThread::run(): Endpoint type is LIBUSB_TRANSFER_TYPE_ISOCHRONOUS";
//                libusb_fill_iso_transfer (transfers[i], deviceHandle, endpoint, databuffers[i],
//                        requestSize * packetSize, requestSize, streamer::transferCallback, NULL, 5000);
//                libusb_set_iso_packet_lengths (transfers[i], packetSize);
//                transferReturnStatus = libusb_submit_transfer (transfers[i]);
                if (transferReturnStatus == 0) requestsInFlight++;
                break;

            default:
                qDebug() << "transferThread::run(): Endpoint type is UNKNOWN";
                break;
        }
    }

    // Show the current number of inflight requests
    qDebug() << "transferThread::run(): Queued" << requestsInFlight << "requests";

    // NEED TO RECODE THIS BIT... NOT VERY NEAT...
    struct timeval timeout;

    // Use a 1 second timeout for the libusb_handle_events_timeout call
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    // Process libUSB events whilst transfer is running
    do {
        libusb_handle_events_timeout (NULL, &timeout);
    } while (!stopTransferRequest);

    qDebug() << "transferThread::run(): Stopping streamer thread";

    // Process libUSB events whilst the transfer is stopping
    while (requestsInFlight != 0) {
        qDebug() << "transferThread::run():" << requestsInFlight << "requests are pending";
        libusb_handle_events_timeout(NULL, &timeout);
        sleep(1);
    }

    qDebug() << "transferThread::run(): Transfer thread completed";
}




//void transferThread::transferCallback(struct libusb_transfer *transfer)
//{
//    unsigned int elapsed_time;
//    double       performance;qDebug() << "streamer::startTransfer(): Started";
//    int          size = 0;

//    // Check if the transfer has succeeded.
//    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {

//        failureCount++;

//    } else {
//        if (endpointType == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS) {

//            // Loop through all the packets and check the status of each packet transfer
//            for (unsigned int i = 0; i < requestSize; ++i) {

//                // Calculate the actual size of data transferred in each micro-frame.
//                if (transfer->iso_packet_desc[i].status == LIBUSB_TRANSFER_COMPLETED) {
//                    size += transfer->iso_packet_desc[i].actual_length;
//                }
//            }

//        } else {
//            size = requestSize * packetSize;
//        }

//        successCount++;
//    }

//    // Update the actual transfer size for this request.
//    transferSize += size;

//    // Print the transfer statistics when queuedepth transfers are completed.
//    transferIndex++;
//    if (transferIndex == queueDepth) {

//        gettimeofday (&endTimestamp, NULL);
//        elapsed_time = ((endTimestamp.tv_sec - startTimestamp.tv_sec) * 1000000 +
//            (endTimestamp.tv_usec - startTimestamp.tv_usec));

//        // Calculate the performance in KBps.
//        performance    = (((double)transferSize / 1024) / ((double)elapsed_time / 1000000));
//        transferPerformance  = (unsigned int)performance;

//        transferIndex = 0;
//        transferSize  = 0;
//        startTimestamp = endTimestamp;
//    }

//    // Reduce the number of requests in flight.
//    requestsInFlight--;

//    // Prepare and re-submit the read request.
//    if (!stopTransferRequest) {

//        // We do not expect a transfer queue attempt to fail in the general case.
//        // However, if it does fail; we just ignore the error.
//        switch (endpointType) {
//            case LIBUSB_TRANSFER_TYPE_BULK:
//                if (libusb_submit_transfer (transfer) == 0)
//                    requestsInFlight++;
//                break;

//            case LIBUSB_TRANSFER_TYPE_INTERRUPT:
//                if (libusb_submit_transfer (transfer) == 0)
//                    requestsInFlight++;
//                break;

//            case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
//                libusb_set_iso_packet_lengths (transfer, packetSize);
//                if (libusb_submit_transfer (transfer) == 0)
//                    requestsInFlight++;
//                break;

//            default:
//                break;
//        }
//    }
//}


// Function to show the LIBUSB error in the debug stream
//void streamer::showLibUsbErrorCode(int errorCode)
//{
//    if (errorCode == LIBUSB_ERROR_IO)
//            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_IO";
//        else if (errorCode == LIBUSB_ERROR_INVALID_PARAM)
//            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_INVALID_PARAM";
//        else if (errorCode == LIBUSB_ERROR_ACCESS)
//            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_ACCESS";
//        else if (errorCode == LIBUSB_ERROR_NO_DEVICE)
//            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_NO_DEVICE";
//        else if (errorCode == LIBUSB_ERROR_NOT_FOUND)
//            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_NOT_FOUND";
//        else if (errorCode == LIBUSB_ERROR_BUSY)
//            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_BUSY";
//        else if (errorCode == LIBUSB_ERROR_TIMEOUT)
//            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_TIMEOUT";
//        else if (errorCode == LIBUSB_ERROR_OVERFLOW)
//            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_OVERFLOW";
//        else if (errorCode == LIBUSB_ERROR_PIPE)
//            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_PIPE";
//        else if (errorCode == LIBUSB_ERROR_INTERRUPTED)
//            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_INTERRUPTED";
//        else if (errorCode == LIBUSB_ERROR_NO_MEM)
//            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_NO_MEM";
//        else if (errorCode == LIBUSB_ERROR_NOT_SUPPORTED)
//            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_NOT_SUPPORTED";
//        else if (errorCode == LIBUSB_ERROR_OTHER)
//            qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_OTHER";
//        else qDebug() << "libUSB Error code:" << errorCode << "LIBUSB_ERROR_UNDOCUMENTED";
//}
