/************************************************************************

    mainwindow.cpp

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

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Create the about dialogue
    aboutDialog = new AboutDialog(this);

    // Create the configuration dialogue
    configurationDialog = new ConfigurationDialog(this);
    connect(configurationDialog, &ConfigurationDialog::configurationChanged, this, &MainWindow::configurationChangedSignalHandler);

    // Define our application (required for configuration handling)
    QCoreApplication::setOrganizationName("Domesday86");
    QCoreApplication::setOrganizationDomain("domesday86.com");
    QCoreApplication::setApplicationName("DomesdayDuplicator");

    // Set the capture flag to not running
    isCaptureRunning = false;

    // Add a label to the status bar for displaying the USB device status
    usbStatusLabel = new QLabel;
    ui->statusBar->addWidget(usbStatusLabel);
    usbStatusLabel->setText(tr("USB device detached"));

    // Disable the capture button
    ui->capturePushButton->setEnabled(false);

    // Load the application's configuration settings file
    configuration = new Configuration();

    // Set up the Domesday Duplicator USB device and connect the signal handlers
    usbDevice = new UsbDevice(this, configuration->getUsbVid(), configuration->getUsbPid());
    connect(usbDevice, &UsbDevice::deviceAttached, this, &MainWindow::deviceAttachedSignalHandler);
    connect(usbDevice, &UsbDevice::deviceDetached, this, &MainWindow::deviceDetachedSignalHandler);

    // Since the device might already be attached, perform an initial scan for it
    usbDevice->scanForDevice();

    // Set up a timer for updating capture results
    captureTimer = new QTimer(this);
    connect(captureTimer, SIGNAL(timeout()), this, SLOT(updateCaptureStatistics()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Signal handlers ----------------------------------------------------------------------------------------------------

// USB device attached signal handler
void MainWindow::deviceAttachedSignalHandler(void)
{
    qDebug() << "MainWindow::deviceAttachedSignalHandler(): Domesday Duplicator USB device has been attached";

    // Show the device status in the status bar
    usbStatusLabel->setText(tr("USB device attached"));

    // Set test mode unchecked in the menu
    ui->actionTest_mode->setChecked(false);

    // Enable the capture button
    ui->capturePushButton->setEnabled(true);
}

// USB device detached signal handler
void MainWindow::deviceDetachedSignalHandler(void)
{
    qDebug() << "MainWindow::deviceAttachedSignalHandler(): Domesday Duplicator USB device has been detached";

    // Show the device status in the status bar
    usbStatusLabel->setText(tr("USB device detached"));

    // Disable the capture button
    ui->capturePushButton->setEnabled(false);
}

// Configuration changed signal handler
void MainWindow::configurationChangedSignalHandler(void)
{
    qDebug() << "MainWindow::configurationChangedSignalHandler(): Configuration has been changed";

    // Save the configuration
    configurationDialog->saveConfiguration(configuration);
}

void MainWindow::updateCaptureStatistics(void)
{
    ui->numberOfTransfersLabel->setText(QString::number(usbDevice->getNumberOfTransfers()));
}

// GUI Triggered action handlers --------------------------------------------------------------------------------------

// Menu option: File->Exit
void MainWindow::on_actionExit_triggered()
{
    // Quit the application
    qApp->quit();
}

// Menu option: Edit->Test mode
void MainWindow::on_actionTest_mode_toggled(bool arg1)
{
    if (arg1) {
        // Turn test-mode on
        usbDevice->sendConfigurationCommand(true);
    } else {
        // Turn test-mode off
        usbDevice->sendConfigurationCommand(false);
    }
}

// Menu option: Help->About
void MainWindow::on_actionAbout_triggered()
{
    aboutDialog->show();
}

// Menu option: Edit->Preferences
void MainWindow::on_actionPreferences_triggered()
{
    configurationDialog->loadConfiguration(configuration);
    configurationDialog->show();
}

// Main window - capture button clicked
void MainWindow::on_capturePushButton_clicked()
{
    if (!isCaptureRunning) {
        // Start capture
        QString captureFilename;

        // Construct the capture file path and name
        captureFilename = configuration->getCaptureDirectory() + "/RF-Sample_";
        captureFilename += QDateTime::currentDateTime().toString("yyyy-mm-dd_hh-mm-ss");
        captureFilename += ".lds";

        qDebug() << "MainWindow::on_capturePushButton_clicked(): Starting capture to file:" << captureFilename;

        updateGuiForCaptureStart();
        isCaptureRunning = true;
        qDebug() << "MainWindow::on_capturePushButton_clicked(): Starting transfer";
        usbDevice->startCapture(captureFilename);
        qDebug() << "MainWindow::on_capturePushButton_clicked(): Transfer started";

        // Start a timer to display the capture statistics
        captureTimer->start(100); // Update 10 times a second (1000 / 10 = 100)

        // Connect to the transfer failure notification signal
        connect(usbDevice, &UsbDevice::transferFailed, this, &MainWindow::transferFailedSignalHandler);
    } else {
        // Stop capture
        usbDevice->stopCapture();
        isCaptureRunning = false;
        captureTimer->stop();
        disconnect(usbDevice, &UsbDevice::transferFailed, this, &MainWindow::transferFailedSignalHandler);
        updateGuiForCaptureStop();
    }
}

// Transfer failed notification signal handler
void MainWindow::transferFailedSignalHandler(void)
{
    // Stop capture - something has gone wrong
    usbDevice->stopCapture();
    isCaptureRunning = false;
    captureTimer->stop();
    disconnect(usbDevice, &UsbDevice::transferFailed, this, &MainWindow::transferFailedSignalHandler);
    updateGuiForCaptureStop();

    // Show an error
    QMessageBox messageBox;
    messageBox.critical(this, "Error","Capture has failed due to a USB error!");
    messageBox.setFixedSize(500, 200);
}

// Update the GUI when capture starts
void MainWindow::updateGuiForCaptureStart(void)
{
    // Disable functions during capture
    ui->capturePushButton->setText(tr("Stop Capture"));
    ui->capturePushButton->setStyleSheet("background-color: red");
    ui->actionTest_mode->setEnabled(false);
    ui->actionPreferences->setEnabled(false);

    // Make sure the configuration dialogue is closed
    configurationDialog->hide();

    // Reset the capture statistics
    ui->numberOfTransfersLabel->setText(tr("0"));
}

// Update the GUI when capture stops
void MainWindow::updateGuiForCaptureStop(void)
{
    // Disable functions after capture
    ui->capturePushButton->setText(tr("Capture"));
    ui->capturePushButton->setStyleSheet("background-color: none");
    ui->actionTest_mode->setEnabled(true);
    ui->actionPreferences->setEnabled(true);
}
