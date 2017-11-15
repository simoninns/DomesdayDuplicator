/************************************************************************

    transferthread.h

    QT/Linux RF Capture application streaming transfer thread header
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

#ifndef TRANSFERTHREAD_H
#define TRANSFERTHREAD_H

#include <QObject>
#include <QThread>
#include <QDebug>
#include <QByteArray>

// Include the required USB libraries
#include <libusb-1.0/libusb.h>
#include <cyusb.h>

class transferThread : public QThread
{
    Q_OBJECT
public:
    void setParameters(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, cyusb_handle*);
    unsigned int getSuccessCount(void);
    unsigned int getFailureCount(void);
    unsigned int getPerformanceCount(void);
    void stop(void);
    bool isRunning(void);

    void run();

private:
    // Private variables for storing transfer configuration
    unsigned int    endpoint;               // Endpoint to be tested
    unsigned int    requestSize;            // Request size in number of packets
    unsigned int    queueDepth;             // Number of requests to queue
    unsigned char   endpointType;			// Type of endpoint (transfer type)
    unsigned int    packetSize;             // Maximum packet size for the endpoint
    cyusb_handle    *deviceHandle;          // Device handle of the USB device

    // Private variables used to report on the transfer
    unsigned int    successCount;           // Number of successful transfers
    unsigned int    failureCount;           // Number of failed transfers
    unsigned int    transferPerformance;	// Performance in Kilobytes per second
    unsigned int    transferSize;           // Size of data transfers performed so far

    // Private variables used to control the transfer
    unsigned int    transferIndex;          // Write index into the transfer_size array
    volatile int    requestsInFlight;       // Number of transfers that are in progress
    volatile bool	stopTransferRequest;	// Request to stop data transfers
    volatile bool   transferRunning;        // Whether the transfer is running

    struct timeval	startTimestamp;         // Data transfer start time stamp.
    struct timeval	endTimestamp;			// Data transfer stop time stamp.

    void transferCallback(struct libusb_transfer *);

signals:

public slots:
};

#endif // TRANSFERTHREAD_H
