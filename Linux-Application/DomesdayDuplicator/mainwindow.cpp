/************************************************************************

    mainwindow.cpp

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


#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "usbdevice.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Set up the USB device object
    domDupUsbDevice = new usbDevice;

    // Set the capture flag to false (not capturing)
    captureFlag = false;

    // Set up the transfer button
    ui->transferPushButton->setText(tr("Start capturing"));

    // Add some default status text to show the state of the USB device
    status = new QLabel;
    ui->statusBar->addWidget(status);
    if (domDupUsbDevice->isConnected()) {
        status->setText(tr("Connected"));
        ui->transferPushButton->setEnabled(true);
    } else {
        status->setText(tr("Domesday Duplicator USB device not connected"));
        ui->transferPushButton->setEnabled(false);
    }

    // Connect to the usb device's signals to show insertion/removal events
    connect(domDupUsbDevice, SIGNAL(statusChanged(bool)), SLOT(usbStatusChanged(bool)));

    // Set up the text labels
    ui->successfulPacketsLabel->setText(tr("0"));
    ui->failedPacketsLabel->setText(tr("0"));
    ui->transferSpeedLabel->setText(tr("0"));
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Update the USB device status based on signals from the USB detection
void MainWindow::usbStatusChanged(bool usbStatus)
{
    if (usbStatus) {
        status->setText(tr("Connected"));
        ui->transferPushButton->setEnabled(true);
    } else {
        status->setText(tr("Domesday Duplicator USB device not connected"));
        ui->transferPushButton->setEnabled(false);
    }
}

// Menu option "About" triggered
void MainWindow::on_actionAbout_triggered()
{
    // Show about dialogue
    QMessageBox::about(this, tr("About"), tr("Domesday Duplicator USB Capture Application\r\r(c)2017 Simon Inns"));
}

// Menu option "Save As" triggered
void MainWindow::on_actionSave_As_triggered()
{

}

// Menu option "Quit" triggered
void MainWindow::on_actionQuit_triggered()
{
    // Quit the application
    qApp->quit();
}

// Transfer push button clicked
void MainWindow::on_transferPushButton_clicked()
{
    // Check if we are currently capturing
    if (captureFlag) {
        // Stop capturing
        qDebug() << "MainWindow::on_transferPushButton_clicked(): Stopping capture";
        captureFlag = false;
        ui->transferPushButton->setText(tr("Start capturing"));
    } else {
        // Verify that the USB device is still connected
        if (domDupUsbDevice->isConnected()) {
            // Start capturing
            qDebug() << "MainWindow::on_transferPushButton_clicked(): Starting capture";
            captureFlag = true;
            ui->transferPushButton->setText(tr("Stop capturing"));
        } else {
            // USB device isn't available
            ui->transferPushButton->setEnabled(false);
        }
    }
}
