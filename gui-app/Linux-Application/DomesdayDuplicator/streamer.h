/************************************************************************

    streamer.h

    QT/Linux RF Capture application USB data streamer header
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

#ifndef STREAMER_H
#define STREAMER_H

#include <QDebug>

// Include the required USB libraries
extern "C" {
#include <libusb-1.0/libusb.h>
}

#include <cyusb.h>
#include "transferthread.h"

class streamer
{
public:
    // Public functions
    streamer();
    void startTransfer(void);
    void stopTransfer(void);
    bool isRunning(void);
    unsigned int getSuccessCount(void);
    unsigned int getFailureCount(void);
    unsigned int getPerformanceCount(void);

private:
    // Private variables for storing the streamer configuration and status
    unsigned int     endpoint;               // Endpoint to be tested
    unsigned int     requestSize;            // Request size in number of packets
    unsigned int     queueDepth;             // Number of requests to queue
    unsigned char	endpointType;			// Type of endpoint (transfer type)
    unsigned int     packetSize;             // Maximum packet size for the endpoint

    unsigned int     successCount;           // Number of successful transfers
    unsigned int     failureCount;           // Number of failed transfers
    unsigned int     transferSize;           // Size of data transfers performed so far
    unsigned int     transferIndex;          // Write index into the transfer_size array
    unsigned int     transferPerformance;	// Performance in KBps
    volatile bool	stopTransferRequest;	// Request to stop data transfers
    volatile int     requestsInFlight;       // Number of transfers that are in progress
    volatile bool	streamerRunning;        // Whether the streamer application is running

    struct timeval	startTimestamp;         // Data transfer start time stamp.
    struct timeval	endTimestamp;			// Data transfer stop time stamp.

    cyusb_handle *deviceHandle;
    transferThread thread;

    // Private functions
    //void *streamerMainThread(void *);
    //void transferCallback(struct libusb_transfer *);
    void freeTransferBuffers(unsigned char **, struct libusb_transfer **);
    void showLibUsbErrorCode(int);
};

#endif // STREAMER_H
