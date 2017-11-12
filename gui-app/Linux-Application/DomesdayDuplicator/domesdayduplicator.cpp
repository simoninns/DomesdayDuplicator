/************************************************************************

    domesdayduplicator.cpp

    QT/Linux RF Capture application main window functions
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

#include "domesdayduplicator.h"
#include "ui_domesdayduplicator.h"

#include "usbdevice.h"

// Class constructor
domesdayDuplicator::domesdayDuplicator(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::domesdayDuplicator)
{
    ui->setupUi(this);

    // Set the window's title
    this->setWindowTitle(tr("Domesday Duplicator"));

    // Add a label to the status bar for showing the status
    status = new QLabel;
    ui->statusBar->addWidget(status);
    status->setText(tr("No duplicator connected"));

    // Set the transfer status in the transfer button text
    ui->transferButton->setText(tr("Start transfer"));

    // Show start up in debug
    qDebug() << "Domesday duplicator main window running";

    // Open a device
    domDupDevice = new usbDevice();

    // Detect any connected devices
    int numberOfDevicesFound = 0;
    numberOfDevicesFound = domDupDevice->detectDevices();

    // If a device was found connect to the first device (only supports one device!)
    if (numberOfDevicesFound > 0) {
        domDupDevice->connectDevice(0);
        status->setText(tr("Duplicator connected"));
    }

    // Set up a timer for polling during bulk transfer and ensure it's stopped
    bulkTransferTimer = new QTimer(this);
    bulkTransferTimer->stop();
    connect(bulkTransferTimer, SIGNAL(timeout()), this, SLOT(transferPoll()));

    // We need to support device hot-plug and hot-unplug detection, otherwise this won't
    // make much sense to the user... Still; keeping it simple for initial testing...
}

// Class destructor
domesdayDuplicator::~domesdayDuplicator()
{
    delete ui;
}

// Transfer button triggered
void domesdayDuplicator::on_transferButton_clicked()
{
    if (domDupDevice->isTransferInProgress()) {
        // Stop transfer
        ui->transferButton->setText(tr("Start transfer"));

        qDebug() << "domesdayDuplicator::on_transferButton_clicked() - Stopping transfer";
        domDupDevice->stopTransfer();
    } else {
        // Start transfer
        ui->transferButton->setText(tr("Stop transfer"));

        qDebug() << "domesdayDuplicator::on_transferButton_clicked() - Starting transfer";
        domDupDevice->startTransfer();
    }

    // If transfer is in progress, set up a timer to poll the data transfer function
    // otherwise ensure that the timer is stopped
    if (domDupDevice->isTransferInProgress()) {
        // Transfer is in progress

        // Each poll of the timer transfers 256Kbytes
        // The expected data rate is >64 Mbytes/sec
        // Therefore we have to poll at 64 * 1024 = 65536 Kbytes / 256 Kbytes = 256 polls/sec
        // 1000ms / 256 polls = 3.9
        bulkTransferTimer->start(4);
    } else {
        // Transfer is not in progress
        bulkTransferTimer->stop();
    }
}

// Polled function to transfer data from bulk IN end-point
void domesdayDuplicator::transferPoll(void)
{
    // Transfer a block of data
    qDebug() << "domesdayDuplicator::transferPoll() - Transfering a block";
    domDupDevice->transferBulkInBlock();

    // If something goes wrong, transfer will be cancelled, we need to check for that here
    if (!domDupDevice->isTransferInProgress()) {
        // Unexpected transfer stop
        qDebug() << "domesdayDuplicator::transferPoll() - Transfer stopped due to error";
        bulkTransferTimer->stop();
        ui->transferButton->setText(tr("Start transfer"));
    }
}
