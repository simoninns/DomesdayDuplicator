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
