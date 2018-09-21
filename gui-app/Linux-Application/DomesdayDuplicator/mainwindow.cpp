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

    // Load the application's configuration settings file
    configuration = new Configuration();

    // Create the about dialogue
    aboutDialog = new AboutDialog(this);

    // Create the configuration dialogue
    configurationDialog = new ConfigurationDialog(this);
    connect(configurationDialog, &ConfigurationDialog::configurationChanged, this, &MainWindow::configurationChangedSignalHandler);

    // Create the player remote dialogue
    playerRemoteDialog = new PlayerRemoteDialog(this);
    connect(playerRemoteDialog, &PlayerRemoteDialog::remoteControlCommand, this, &MainWindow::remoteControlCommandSignalHandler);

    // Start the player control object
    playerControl = new PlayerControl(this);
    startPlayerControl();

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

    // Set up the Domesday Duplicator USB device and connect the signal handlers
    usbDevice = new UsbDevice(this, configuration->getUsbVid(), configuration->getUsbPid());
    connect(usbDevice, &UsbDevice::deviceAttached, this, &MainWindow::deviceAttachedSignalHandler);
    connect(usbDevice, &UsbDevice::deviceDetached, this, &MainWindow::deviceDetachedSignalHandler);

    // Since the device might already be attached, perform an initial scan for it
    usbDevice->scanForDevice();

    // Set up a timer for updating capture results
    captureTimer = new QTimer(this);
    connect(captureTimer, SIGNAL(timeout()), this, SLOT(updateCaptureStatistics()));

    // Set up a timer for updating player control information
    playerControlTimer = new QTimer(this);
    connect(playerControlTimer, SIGNAL(timeout()), this, SLOT(updatePlayerControlInformation()));
    playerControlTimer->start(100); // Update 10 times per second

    // Defaults for the remote control toggle settings
    remoteDisplayState = PlayerCommunication::DisplayState::off;
    remoteAudioState = PlayerCommunication::AudioState::digitalStereo;
    remoteMultiSpeed = 1;
    remoteSpeed = 0;
    remoteChapterFrameMode = PlayerCommunication::ChapterFrameMode::chapter;
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

    // Restart the player control
    startPlayerControl();
}

// Remote control command signal handler
void MainWindow::remoteControlCommandSignalHandler(PlayerRemoteDialog::RemoteButtons button)
{
    // Handle the possible remote control commands
    switch(button) {
    case PlayerRemoteDialog::RemoteButtons::rbReject:
        playerControl->setPlayerState(PlayerCommunication::PlayerState::stop);
        break;
    case PlayerRemoteDialog::RemoteButtons::rbPause:
        playerControl->setPlayerState(PlayerCommunication::PlayerState::stillFrame);
        break;
    case PlayerRemoteDialog::RemoteButtons::rbPlay:
        playerControl->setPlayerState(PlayerCommunication::PlayerState::play);
        break;
    case PlayerRemoteDialog::RemoteButtons::rbRepeat:
        qDebug() << "MainWindow::remoteControlCommandSignalHandler(): rbRepeat not implemented";
        break;
    case PlayerRemoteDialog::RemoteButtons::rbStepRev:
        playerControl->step(PlayerCommunication::Direction::backwards);
        break;
    case PlayerRemoteDialog::RemoteButtons::rbStepFwd:
        playerControl->step(PlayerCommunication::Direction::forwards);
        break;
    case PlayerRemoteDialog::RemoteButtons::rbDisplay:
        if (remoteDisplayState == PlayerCommunication::DisplayState::off) {
            playerControl->setOnScreenDisplay(PlayerCommunication::DisplayState::on);
            remoteDisplayState = PlayerCommunication::DisplayState::on;
        } else {
            playerControl->setOnScreenDisplay(PlayerCommunication::DisplayState::off);
            remoteDisplayState = PlayerCommunication::DisplayState::off;
        }
        break;
    case PlayerRemoteDialog::RemoteButtons::rbScanRev:
        playerControl->scan(PlayerCommunication::Direction::backwards);
        break;
    case PlayerRemoteDialog::RemoteButtons::rbScanFwd:
        playerControl->scan(PlayerCommunication::Direction::forwards);
        break;
    case PlayerRemoteDialog::RemoteButtons::rbAudio:
        // Rotate through audio options
        if (remoteAudioState == PlayerCommunication::AudioState::audioOff)
            remoteAudioState = PlayerCommunication::AudioState::analogCh1;
        else if (remoteAudioState == PlayerCommunication::AudioState::analogCh1)
            remoteAudioState = PlayerCommunication::AudioState::analogCh2;
        else if (remoteAudioState == PlayerCommunication::AudioState::analogCh2)
            remoteAudioState = PlayerCommunication::AudioState::analogStereo;
        else if (remoteAudioState == PlayerCommunication::AudioState::analogStereo)
            remoteAudioState = PlayerCommunication::AudioState::digitalCh1;
        else if (remoteAudioState == PlayerCommunication::AudioState::digitalCh1)
            remoteAudioState = PlayerCommunication::AudioState::digitalCh2;
        else if (remoteAudioState == PlayerCommunication::AudioState::digitalCh2)
            remoteAudioState = PlayerCommunication::AudioState::digitalStereo;
        else if (remoteAudioState == PlayerCommunication::AudioState::digitalStereo)
            remoteAudioState = PlayerCommunication::AudioState::audioOff;
        playerControl->setAudio(remoteAudioState);
        break;
    case PlayerRemoteDialog::RemoteButtons::rbSpeedDown:
        remoteSpeed--;
        if (remoteSpeed < 0) remoteSpeed = 0;
        playerControl->setSpeed(remoteSpeed);
        break;
    case PlayerRemoteDialog::RemoteButtons::rbSpeedUp:
        remoteSpeed++;
        if (remoteSpeed > 7) remoteSpeed = 7;
        playerControl->setSpeed(remoteSpeed);
        break;
    case PlayerRemoteDialog::RemoteButtons::rbClear:
        // Note: ignored
        break;
    case PlayerRemoteDialog::RemoteButtons::rbMultiRev:
        playerControl->multiSpeed(PlayerCommunication::Direction::backwards);
        break;
    case PlayerRemoteDialog::RemoteButtons::rbMultiFwd:
        playerControl->multiSpeed(PlayerCommunication::Direction::forwards);
        break;
    case PlayerRemoteDialog::RemoteButtons::rbSearch:
        // Requires interaction with remote control dialogue
        break;
    case PlayerRemoteDialog::RemoteButtons::rbChapFrame:
        // Requires interaction with remote control dialogue
        break;
    case PlayerRemoteDialog::RemoteButtons::rbZero:
        // Handled by the remote control dialogue
        break;
    case PlayerRemoteDialog::RemoteButtons::rbOne:
        // Handled by the remote control dialogue
        break;
    case PlayerRemoteDialog::RemoteButtons::rbTwo:
        // Handled by the remote control dialogue
        break;
    case PlayerRemoteDialog::RemoteButtons::rbThree:
        // Handled by the remote control dialogue
        break;
    case PlayerRemoteDialog::RemoteButtons::rbFour:
        // Handled by the remote control dialogue
        break;
    case PlayerRemoteDialog::RemoteButtons::rbFive:
        // Handled by the remote control dialogue
        break;
    case PlayerRemoteDialog::RemoteButtons::rbSix:
        // Handled by the remote control dialogue
        break;
    case PlayerRemoteDialog::RemoteButtons::rbSeven:
        // Handled by the remote control dialogue
        break;
    case PlayerRemoteDialog::RemoteButtons::rbEight:
        // Handled by the remote control dialogue
        break;
    case PlayerRemoteDialog::RemoteButtons::rbNine:
        // Handled by the remote control dialogue
        break;
    }
}

// Update the capture statistics labels
void MainWindow::updateCaptureStatistics(void)
{
    ui->numberOfTransfersLabel->setText(QString::number(usbDevice->getNumberOfTransfers()));

    // Calculate the captured data based on the sample format (i.e. size on disk)
    qint32 mbWritten = 0;
    if (configuration->getCaptureFormat() == Configuration::CaptureFormat::sixteenBitSigned)
        mbWritten = usbDevice->getNumberOfDiskBuffersWritten() * 64; // 16-bit is 64MiB per buffer
    else mbWritten = usbDevice->getNumberOfDiskBuffersWritten() * 40; // 10-bit is 40MiB per buffer

    ui->numberOfDiskBuffersWrittenLabel->setText(QString::number(mbWritten) + (tr(" MiB")));
}

// Update the player control labels
void MainWindow::updatePlayerControlInformation(void)
{
    ui->playerStatusLabel->setText(playerControl->getPlayerStatusInformation());
    ui->playerPositionLabel->setText(playerControl->getPlayerPositionInformation());
}

void MainWindow::startPlayerControl(void)
{
    PlayerCommunication::SerialSpeed serialSpeed;
    PlayerCommunication::PlayerType playerType;

    // Get the configured serial speed
    switch (configuration->getSerialSpeed()) {
    case Configuration::bps1200: serialSpeed = PlayerCommunication::SerialSpeed::bps1200;
        break;
    case Configuration::bps2400: serialSpeed = PlayerCommunication::SerialSpeed::bps2400;
        break;
    case Configuration::bps4800: serialSpeed = PlayerCommunication::SerialSpeed::bps4800;
        break;
    case Configuration::bps9600: serialSpeed = PlayerCommunication::SerialSpeed::bps9600;
        break;
    }

    // Get the configured player type
    qDebug() << "MainWindow::startPlayerControl(): Getting player type";
    switch (configuration->getPlayerModel()) {
    case Configuration::PlayerModels::none: playerType = PlayerCommunication::PlayerType::unknownPlayerType;
        qDebug() << "MainWindow::startPlayerControl(): Warning: Player type is not configured";
        break;
    case Configuration::PlayerModels::pioneerLDV4300D: playerType = PlayerCommunication::PlayerType::pioneerLDV4300D;
        break;
    case Configuration::PlayerModels::pioneerCLDV2800: playerType = PlayerCommunication::PlayerType::pioneerCLDV2800;
        break;
    }

    if (configuration->getSerialDevice().isEmpty())
        qDebug() << "MainWindow::startPlayerControl(): Warning: Serial device is not configured";

    // Send the configuration to the player control
    playerControl->configurePlayerCommunication(configuration->getSerialDevice(),
                                                    serialSpeed,
                                                    playerType);
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
        ui->capturePushButton->setText("Test data capture");
    } else {
        // Turn test-mode off
        usbDevice->sendConfigurationCommand(false);
        ui->capturePushButton->setText("Capture");
    }
}

// Menu option: View->Player remote
void MainWindow::on_actionPlayer_remote_triggered()
{
    playerRemoteDialog->show();
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

        // Change the prefix depending on if the data is RF or test data
        if (ui->actionTest_mode->isChecked()) captureFilename = configuration->getCaptureDirectory() + "/TestData_";
        else captureFilename = configuration->getCaptureDirectory() + "/RF-Sample_";
        captureFilename += QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");

        // Change the suffix depending on if the data is 10 or 16 bit
        if (configuration->getCaptureFormat() == Configuration::CaptureFormat::tenBitPacked) captureFilename += ".lds";
        else captureFilename += ".raw";

        qDebug() << "MainWindow::on_capturePushButton_clicked(): Starting capture to file:" << captureFilename;

        updateGuiForCaptureStart();
        isCaptureRunning = true;
        qDebug() << "MainWindow::on_capturePushButton_clicked(): Starting transfer";
        if (configuration->getCaptureFormat() == Configuration::CaptureFormat::tenBitPacked) {
            usbDevice->startCapture(captureFilename, true);
        } else {
            usbDevice->startCapture(captureFilename, false);
        }

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
    messageBox.critical(this, "Error", usbDevice->getLastError());
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
    if (ui->actionTest_mode->isChecked()) ui->capturePushButton->setText(tr("Test data capture"));
    else ui->capturePushButton->setText(tr("Capture"));
    ui->capturePushButton->setStyleSheet("background-color: none");
    ui->actionTest_mode->setEnabled(true);
    ui->actionPreferences->setEnabled(true);
}


