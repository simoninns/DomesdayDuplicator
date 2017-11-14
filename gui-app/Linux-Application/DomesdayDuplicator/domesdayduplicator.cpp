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

    // Declare a streamer object for streaming transfer
    dataStreamer = new streamer;

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
    if (0) {
        // Stop transfer
        ui->transferButton->setText(tr("Start transfer"));
        dataStreamer->stopTransfer();
        qDebug() << "domesdayDuplicator::on_transferButton_clicked() - Stopping transfer";

    } else {
        // Start transfer
        ui->transferButton->setText(tr("Stop transfer"));
        dataStreamer->startTransfer();
        qDebug() << "domesdayDuplicator::on_transferButton_clicked() - Starting transfer";

    }
}
