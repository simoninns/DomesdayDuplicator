/************************************************************************

    mainwindow.cpp

    Capture application for the Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2018-2019 Simon Inns

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
#include "usbcapture.h"
#include "amplitudemeasurement.h"
#include <QFile>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Load the application's configuration settings file
    configuration = new Configuration();

    // Create the about dialogue
    aboutDialog = new AboutDialog(this);

    // Create the advanced naming dialogue
    advancedNamingDialog = new AdvancedNamingDialog(this);

    // Create the configuration dialogue
    configurationDialog = new ConfigurationDialog(this);
    connect(configurationDialog, &ConfigurationDialog::configurationChanged, this, &MainWindow::configurationChangedSignalHandler);

    // Create the player remote dialogue
    playerRemoteDialog = new PlayerRemoteDialog(this);
    connect(playerRemoteDialog, &PlayerRemoteDialog::remoteControlCommand, this, &MainWindow::remoteControlCommandSignalHandler);
    connect(playerRemoteDialog, &PlayerRemoteDialog::remoteControlSearch, this, &MainWindow::remoteControlSearchSignalHandler);
    playerRemoteDialog->setEnabled(false); // Disable the dialogue until a player is connected

    // Create the automatic capture dialogue
    automaticCaptureDialog = new AutomaticCaptureDialog(this);
    connect(automaticCaptureDialog, &AutomaticCaptureDialog::startAutomaticCapture,
            this, &MainWindow::startAutomaticCaptureDialogSignalHandler);
    connect(automaticCaptureDialog, &AutomaticCaptureDialog::stopAutomaticCapture,
            this, &MainWindow::stopAutomaticCaptureDialogSignalHandler);
    automaticCaptureDialog->setEnabled(false); // Disable the dialogue until a player is connected

    // Set up a timer for updating the automatic capture status information
    automaticCaptureTimer = new QTimer(this);
    connect(automaticCaptureTimer, SIGNAL(timeout()), this, SLOT(updateAutomaticCaptureStatus()));
    automaticCaptureTimer->start(100); // Update 10 times per second

    // Start the player control object
    playerControl = new PlayerControl(this);
    connect(playerControl, &PlayerControl::automaticCaptureComplete,
            this, &MainWindow::automaticCaptureCompleteSignalHandler);
    connect(playerControl, &PlayerControl::startCapture,
            this, &MainWindow::startCaptureSignalHandler);
    connect(playerControl, &PlayerControl::stopCapture,
            this, &MainWindow::stopCaptureSignalHandler);
    connect(playerControl, &PlayerControl::playerConnected,
            this, &MainWindow::playerConnectedSignalHandler);
    connect(playerControl, &PlayerControl::playerDisconnected,
            this, &MainWindow::playerDisconnectedSignalHandler);
    startPlayerControl();

    // Set the capture flag to not running
    isCaptureRunning = false;

    // Add a label to the status bar for displaying the USB device status
    usbStatusLabel = new QLabel;
    ui->statusBar->addWidget(usbStatusLabel);
    usbStatusLabel->setText(tr("No USB capture device is attached"));

    // Disable the capture button
    ui->capturePushButton->setEnabled(false);

    // Disable the test mode option
    ui->actionTest_mode->setEnabled(false);

    // Set up a timer for timing the capture duration
    captureDurationTimer = new QTimer(this);
    connect(captureDurationTimer, SIGNAL(timeout()), this, SLOT(updateCaptureDuration()));

    // Set up amplitude timer, and update amplitude UI
    amplitudeTimer = new QTimer(this);
    updateAmplitudeUI();

    // Set up the Domesday Duplicator USB device and connect the signal handlers
    usbDevice = new UsbDevice(this, configuration->getUsbVid(), configuration->getUsbPid());
    connect(usbDevice, &UsbDevice::deviceAttached, this, &MainWindow::deviceAttachedSignalHandler);
    connect(usbDevice, &UsbDevice::deviceDetached, this, &MainWindow::deviceDetachedSignalHandler);

    // Since the device might already be attached, perform an initial scan for it
    usbDevice->scanForDevice();

    // Set up a timer for updating capture results
    captureStatusUpdateTimer = new QTimer(this);
    connect(captureStatusUpdateTimer, SIGNAL(timeout()), this, SLOT(updateCaptureStatistics()));

    // Set up a timer for updating player control information
    playerControlTimer = new QTimer(this);
    connect(playerControlTimer, SIGNAL(timeout()), this, SLOT(updatePlayerControlInformation()));
    playerControlTimer->start(100); // Update 10 times per second

    // Defaults for the remote control toggle settings
    remoteDisplayState = PlayerCommunication::DisplayState::off;
    remoteAudioState = PlayerCommunication::AudioState::digitalStereo;
    remoteSpeed = 4;
    remoteChapterFrameMode = PlayerCommunication::ChapterFrameMode::chapter;
    updatePlayerRemoteDialog();

    // Storage space information
    storageInfo = new QStorageInfo();
    storageInfo->setPath(configuration->getCaptureDirectory());

    // Storage information update timer
    storageInfoTimer = new QTimer(this);
    connect(storageInfoTimer, SIGNAL(timeout()), this, SLOT(updateStorageInformation()));
    storageInfoTimer->start(200); // Update 5 times per second

    // Set player as disconnected
    isPlayerConnected = false;

    // Load the window geometry from the configuration
    restoreGeometry(configuration->getMainWindowGeometry());
    playerRemoteDialog->restoreGeometry(configuration->getPlayerRemoteDialogGeometry());
    advancedNamingDialog->restoreGeometry(configuration->getAdvancedNamingDialogGeometry());
    automaticCaptureDialog->restoreGeometry(configuration->getAutomaticCaptureDialogGeometry());
    configurationDialog->restoreGeometry(configuration->getConfigurationDialogGeometry());
}

MainWindow::~MainWindow()
{
    // Save window and dialogue geometry
    qDebug() << "MainWindow::~MainWindow(): Quit selected; saving configuration...";
    configuration->setMainWindowGeometry(saveGeometry());
    configuration->setPlayerRemoteDialogGeometry(playerRemoteDialog->saveGeometry());
    configuration->setAdvancedNamingDialogGeometry(advancedNamingDialog->saveGeometry());
    configuration->setAutomaticCaptureDialogGeometry(automaticCaptureDialog->saveGeometry());
    configuration->setConfigurationDialogGeometry(configurationDialog->saveGeometry());
    configuration->writeConfiguration();

    // Ask the threads to stop
    qDebug() << "MainWindow::~MainWindow(): Quit selected; asking threads to stop...";
    if (playerControl->isRunning()) playerControl->stop();
    if (usbDevice->isRunning()) usbDevice->stop();

    // Schedule the objects for deletion
    playerControl->deleteLater();
    usbDevice->deleteLater();

    // Wait for the objects to be deleted
    playerControl->wait();
    usbDevice->wait();

    // Delete the UI
    delete ui;

    qDebug() << "MainWindow::~MainWindow(): All threads stopped; done.";
}

// Signal handlers ----------------------------------------------------------------------------------------------------

// USB device attached signal handler
void MainWindow::deviceAttachedSignalHandler(void)
{
    qDebug() << "MainWindow::deviceAttachedSignalHandler(): Domesday Duplicator USB device has been attached";

    // Show the device status in the status bar
    usbStatusLabel->setText(tr("Domesday Duplicator is connected via USB"));

    // Set test mode unchecked in the menu
    ui->actionTest_mode->setChecked(false);

    // Enable the capture button
    ui->capturePushButton->setEnabled(true);

    // Enable the automatic capture dialogue
    if (isPlayerConnected) automaticCaptureDialog->setEnabled(true);

    // Enable the test mode option
    ui->actionTest_mode->setEnabled(true);
}

// USB device detached signal handler
void MainWindow::deviceDetachedSignalHandler(void)
{
    qDebug() << "MainWindow::deviceAttachedSignalHandler(): Domesday Duplicator USB device has been detached";

    // Show the device status in the status bar
    usbStatusLabel->setText(tr("No USB capture device is attached"));

    // Disable the capture button
    ui->capturePushButton->setEnabled(false);

    // Disable the automatic capture dialogue
    automaticCaptureDialog->setEnabled(false);

    // Disable the test mode option
    ui->actionTest_mode->setEnabled(false);
}

// Configuration changed signal handler
void MainWindow::configurationChangedSignalHandler(void)
{
    qDebug() << "MainWindow::configurationChangedSignalHandler(): Configuration has been changed";

    // Save the configuration
    configurationDialog->saveConfiguration(configuration);

    // Restart the player control
    startPlayerControl();

    // Update the target directory for the storage information
    storageInfo->setPath(configuration->getCaptureDirectory());

    // Update amplitude UI
    updateAmplitudeUI();
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
        // Still-frame only works for CAV, so we have to check the disc type here
        if (playerControl->getDiscType() == PlayerCommunication::DiscType::CAV)
            playerControl->setPlayerState(PlayerCommunication::PlayerState::stillFrame);
        else if (playerControl->getDiscType() == PlayerCommunication::DiscType::CLV)
            playerControl->setPlayerState(PlayerCommunication::PlayerState::pause);
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
        updatePlayerRemoteDialog();
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
        updatePlayerRemoteDialog();
        break;
    case PlayerRemoteDialog::RemoteButtons::rbSpeedUp:
        remoteSpeed++;
        if (remoteSpeed > 7) remoteSpeed = 7;
        playerControl->setSpeed(remoteSpeed);
        updatePlayerRemoteDialog();
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

// Remote control search command signal handler
void MainWindow::remoteControlSearchSignalHandler(qint32 position, PlayerRemoteDialog::PositionMode positionMode)
{
    switch(positionMode) {
    case PlayerRemoteDialog::PositionMode::pmChapter:
        playerControl->setPositionChapter(position);
        break;
    case PlayerRemoteDialog::PositionMode::pmTime:
        playerControl->setPositionTimeCode(position);
        break;
    case PlayerRemoteDialog::PositionMode::pmFrame:
        playerControl->setPositionFrame(position);
        break;
    }
}

// Update capture duration timer signal handler
void MainWindow::updateCaptureDuration(void)
{
    // Add a second to the capture time and update the label
    captureElapsedTime = captureElapsedTime.addSecs(1);
    ui->durationLabel->setText(captureElapsedTime.toString("hh:mm:ss"));

    // Time limited capture is on?
    if (ui->limitDurationCheckBox->isChecked()) {
        // Check if the capture duration has exceeded the time limit
        qint32 timeLimit = ui->durationLimitTimeEdit->time().hour() * 60 * 60;
        timeLimit += ui->durationLimitTimeEdit->time().minute() * 60;
        timeLimit += ui->durationLimitTimeEdit->time().second();

        qint32 elapsedTime = captureElapsedTime.hour() * 60 * 60;
        elapsedTime += captureElapsedTime.minute() * 60;
        elapsedTime += captureElapsedTime.second();

        if (timeLimit <= elapsedTime) {
            // 'Press' the capture button automatically
            on_capturePushButton_clicked();
        }
    }
}

// Automatic capture dialogue signals that capture should start
void MainWindow::startAutomaticCaptureDialogSignalHandler(AutomaticCaptureDialog::CaptureType captureType,
                                                          qint32 startAddress, qint32 endAddress,
                                                          AutomaticCaptureDialog::DiscType discTypeParam)
{
    bool fromLeadIn = false;
    bool wholeDisc = false;
    PlayerCommunication::DiscType discType = PlayerCommunication::DiscType::unknownDiscType;

    if (captureType == AutomaticCaptureDialog::CaptureType::wholeDisc) {
        fromLeadIn = true;
        wholeDisc = true;
    } else if (captureType == AutomaticCaptureDialog::CaptureType::leadInCapture) {
        fromLeadIn = true;
        wholeDisc = false;
    } else if (captureType == AutomaticCaptureDialog::CaptureType::partialDisc) {
        fromLeadIn = false;
        wholeDisc = false;
    }

    if (discTypeParam == AutomaticCaptureDialog::DiscType::CAV) discType = PlayerCommunication::DiscType::CAV;
    else discType = PlayerCommunication::DiscType::CLV;

    // Start the automatic capture
    playerControl->startAutomaticCapture(fromLeadIn, wholeDisc,
                                         startAddress, endAddress,
                                         discType,
                                         configuration->getKeyLock());
}

// Automatic capture dialogue signals that capture should stop
void MainWindow::stopAutomaticCaptureDialogSignalHandler(void)
{
    playerControl->stopAutomaticCapture();
}

// Update the automatic capture status (called by a timer)
void MainWindow::updateAutomaticCaptureStatus(void)
{
    automaticCaptureDialog->updateStatus(playerControl->getAutomaticCaptureStatus());
}

// Handle the automatic capture complete signal from the player control object
void MainWindow::automaticCaptureCompleteSignalHandler(bool success)
{
    // Tell the automatic capture dialogue that capture is complete
    automaticCaptureDialog->captureComplete();

    // Was the capture successful?
    if (!success) {
        // Show an error
        QMessageBox messageBox;
        messageBox.critical(this, "Error", playerControl->getAutomaticCaptureError());
        messageBox.setFixedSize(500, 200);
    }
}

// Handle the start capture signal from the player control object
void MainWindow::startCaptureSignalHandler(void)
{
    qDebug() << "MainWindow::startCaptureSignalHandler(): Got start capture signal from player control";
    if (!isCaptureRunning) on_capturePushButton_clicked();
}

// Handle the stop capture signal from the player control object
void MainWindow::stopCaptureSignalHandler(void)
{
    qDebug() << "MainWindow::stopCaptureSignalHandler(): Got stop capture signal from player control";
    if (isCaptureRunning) on_capturePushButton_clicked();
}

// Signal handler for player connected signal from player control
void MainWindow::playerConnectedSignalHandler(void)
{
    qDebug() << "MainWindow::playerConnectedSignalHandler(): Received player connected signal";
    // Enable remote control dialogue
    playerRemoteDialog->setEnabled(true);

    // Enable automatic-capture (is capture is currently allowed)
    if (ui->capturePushButton->isEnabled()) automaticCaptureDialog->setEnabled(true);

    isPlayerConnected = true;
}

// Signal handler for player disconnected signal from player control
void MainWindow::playerDisconnectedSignalHandler(void)
{
    qDebug() << "MainWindow::playerConnectedSignalHandler(): Received player disconnected signal";
    // Disable remote control dialogue
    playerRemoteDialog->setEnabled(false);

    // Disable automatic-capture
    automaticCaptureDialog->setEnabled(false);

    isPlayerConnected = false;
}

// Update the capture statistics labels
void MainWindow::updateCaptureStatistics(void)
{
    ui->numberOfTransfersLabel->setText(QString::number(usbDevice->getNumberOfTransfers()));

    // Calculate the captured data based on the sample format (i.e. size on disk)
    qint32 mbWritten = 0;
    if (configuration->getCaptureFormat() == Configuration::CaptureFormat::sixteenBitSigned)
        mbWritten = usbDevice->getNumberOfDiskBuffersWritten() * 64; // 16-bit is 64MiB per buffer
    else if (configuration->getCaptureFormat() == Configuration::CaptureFormat::tenBitPacked)
        mbWritten = usbDevice->getNumberOfDiskBuffersWritten() * 40; // 10-bit is 40MiB per buffer
    else mbWritten = usbDevice->getNumberOfDiskBuffersWritten() * 10; // 10-bit 4:1 is 8MiB per buffer

    ui->dataCapturedLabel->setText(QString::number(mbWritten) + (tr(" MiB")));
}

// Update the player control labels
void MainWindow::updatePlayerControlInformation(void)
{
    if (!playerControl->getSerialBaudRate().isEmpty()) {
        ui->playerPortLabel->setText(configuration->getSerialDevice() + " @ " + playerControl->getSerialBaudRate() + " bps");
    } else {
        ui->playerPortLabel->setText(configuration->getSerialDevice());
    }

    QString modelLabel;
    auto playerModelName = playerControl->getPlayerModelName();
    if (!playerModelName.isEmpty()) {
        modelLabel = playerModelName + " [Version " + playerControl->getPlayerVersionNumber() + "]";
    }
    ui->playerModelLabel->setText(modelLabel);
    ui->playerStatusLabel->setText(playerControl->getPlayerStatusInformation());

    // Display the position information based on disc type
    if (playerControl->getDiscType() == PlayerCommunication::DiscType::CAV) {
        // CAV
        ui->playerPositionLabel->setText(playerControl->getPlayerPositionInformation());
    } else if (playerControl->getDiscType() == PlayerCommunication::DiscType::CLV) {
        // CLV
        ui->playerPositionLabel->setText(playerControl->getPlayerPositionInformation());
    } else {
        // Disc type unknown
        ui->playerPositionLabel->setText(tr("Unknown"));
    }
}

// Update the storage information labels
void MainWindow::updateStorageInformation(void)
{
    storageInfo->refresh();
    if (storageInfo->isValid()) {
        qint64 availableMiBs = storageInfo->bytesAvailable() / 1024 / 1024;

        qint64 availableSeconds = 0;

        if (availableMiBs != 0) {
            if (configuration->getCaptureFormat() == Configuration::CaptureFormat::sixteenBitSigned) {
                availableSeconds = availableMiBs / 64; // 16-bit is 64MiB per buffer
            } else if (configuration->getCaptureFormat() == Configuration::CaptureFormat::tenBitPacked) {
                availableSeconds = availableMiBs / 40; // 10-bit is 40MiB per buffer
            } else {
                availableSeconds = availableMiBs / 10; // 10-bit 4:1 is 10MiB per buffer
            }

            // Print the time available (be non-specific if > 24 hours)
            if (availableSeconds > 84600) ui->spaceAvailableLabel->setText("More than 24 hours");
            else ui->spaceAvailableLabel->setText(QTime(0, 0, 0, 0).addSecs(static_cast<qint32>(availableSeconds)).toString("hh:mm:ss"));
        } else {
            // Unable to get storage info
            ui->spaceAvailableLabel->setText(tr("Unknown"));
        }
    } else {
        // Unable to get storage info
        ui->spaceAvailableLabel->setText(tr("Unknown"));
    }

}

void MainWindow::startPlayerControl(void)
{
    // Get the configured serial speed
    PlayerCommunication::SerialSpeed serialSpeed = PlayerCommunication::SerialSpeed::bps9600;
    switch (configuration->getSerialSpeed()) {
    case Configuration::SerialSpeeds::bps1200: serialSpeed = PlayerCommunication::SerialSpeed::bps1200;
        break;
    case Configuration::SerialSpeeds::bps2400: serialSpeed = PlayerCommunication::SerialSpeed::bps2400;
        break;
    case Configuration::SerialSpeeds::bps4800: serialSpeed = PlayerCommunication::SerialSpeed::bps4800;
        break;
    case Configuration::SerialSpeeds::bps9600: serialSpeed = PlayerCommunication::SerialSpeed::bps9600;
        break;
    case Configuration::SerialSpeeds::autoDetect: serialSpeed = PlayerCommunication::SerialSpeed::autoDetect;
        break;
    }

    if (configuration->getSerialDevice().isEmpty())
        qDebug() << "MainWindow::startPlayerControl(): Player serial device is not configured in preferences";

    // Send the configuration to the player control
    playerControl->configurePlayerCommunication(configuration->getSerialDevice(), serialSpeed);
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

// Menu option->Advanced naming
void MainWindow::on_actionAdvanced_naming_triggered()
{
    advancedNamingDialog->show();
}

// Menu option: View->Player remote
void MainWindow::on_actionPlayer_remote_triggered()
{
    playerRemoteDialog->show();
}

// Menu option: View->Automatic capture
void MainWindow::on_actionAutomatic_capture_triggered()
{
    automaticCaptureDialog->show();
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
QString captureFilename;
void MainWindow::on_capturePushButton_clicked()
{
    if (!isCaptureRunning) {
        // Start capture

        // Ensure that the test mode option matches the device configuration
        bool isTestMode = ui->actionTest_mode->isChecked();
        qDebug() << "MainWindow::on_capturePushButton_clicked(): Setting device's test mode flag to" << isTestMode;
        usbDevice->sendConfigurationCommand(isTestMode);

        // Construct the capture file path and name

        // Use the advanced naming dialogue to generate the capture file name
        captureFilename = configuration->getCaptureDirectory() +
                advancedNamingDialog->getFileName(isTestMode);

        // Change the suffix depending on if the data is 10 or 16 bit
        if (configuration->getCaptureFormat() == Configuration::CaptureFormat::tenBitPacked) captureFilename += ".lds";
        else if (configuration->getCaptureFormat() == Configuration::CaptureFormat::sixteenBitSigned) captureFilename += ".raw";
        else captureFilename += ".cds";

        qDebug() << "MainWindow::on_capturePushButton_clicked(): Starting capture to file:" << captureFilename;
        updateGuiForCaptureStart();
        isCaptureRunning = true;

        if (configuration->getCaptureFormat() == Configuration::CaptureFormat::tenBitPacked) {
            qDebug() << "MainWindow::on_capturePushButton_clicked(): Starting transfer - 10-bit packed";
            usbDevice->startCapture(captureFilename, true, false, isTestMode);
        } else if (configuration->getCaptureFormat() == Configuration::CaptureFormat::tenBitCdPacked) {
            qDebug() << "MainWindow::on_capturePushButton_clicked(): Starting transfer - 10-bit packed 4:1 decimated";
            usbDevice->startCapture(captureFilename, true, true, isTestMode);
        } else {
            qDebug() << "MainWindow::on_capturePushButton_clicked(): Starting transfer - 16-bit";
            usbDevice->startCapture(captureFilename, false, false, isTestMode);
        }

        qDebug() << "MainWindow::on_capturePushButton_clicked(): Transfer started";

        // Start a timer to display the capture statistics
        ui->durationLabel->setText(tr("00:00:00"));
        captureStatusUpdateTimer->start(100); // Update 10 times a second (1000 / 10 = 100)

        // Start the capture duration timer
        captureElapsedTime = QTime::fromString("00:00:00", "hh:mm:ss");
        captureDurationTimer->start(1000); // Update 1 time per second

        // Connect to the transfer failure notification signal
        connect(usbDevice, &UsbDevice::transferFailed, this, &MainWindow::transferFailedSignalHandler);

        // Start graph processing
        amplitudeTimer->start(1000);
    } else {
        // Stop capture
        playerControl->stopAutomaticCapture(); // Stop auto-capture if in progress
        usbDevice->stopCapture();
        isCaptureRunning = false;
        captureStatusUpdateTimer->stop();
        amplitudeTimer->stop();
        captureDurationTimer->stop();
        disconnect(usbDevice, &UsbDevice::transferFailed, this, &MainWindow::transferFailedSignalHandler);
        // Rename output file if duration checkbox is clicked
        if (advancedNamingDialog->getDurationChecked()) {
            qDebug() << "ainWindow::on_capturePushButton_clicked(): Starting attempt to append duration";
            // Make sure output file is closed
            if (!UsbCapture::getOkToRename()) {
                qDebug() << "MainWindow::on_capturePushButton_clicked(): Not ok to rename, disk buffers still writing";
                while (!UsbCapture::getOkToRename()) {
                    // Wait until finished
                }
            }
            QString durationFilename = captureFilename;
            QString finalDuration = captureElapsedTime.toString("hh'H'mm'M'ss'S'");
            finalDuration.prepend("_");
            // Get "." before extension and save as index
            int durationIndex = durationFilename.lastIndexOf(".");
            durationFilename.insert(durationIndex, finalDuration);
            QFile::rename(captureFilename, durationFilename);
            qDebug() << "MainWindow::on_capturePushButton_clicked(): Renamed file to" << durationFilename;
        }
        updateGuiForCaptureStop();
    }
}

// Limit duration checkbox state changed
void MainWindow::on_limitDurationCheckBox_stateChanged(int arg1)
{
    (void)arg1;

    if (ui->limitDurationCheckBox->isChecked()) {
        // Checked
        ui->durationLimitTimeEdit->setEnabled(true);
    } else {
        // Unchecked
        ui->durationLimitTimeEdit->setEnabled(false);
    }
}

// Transfer failed notification signal handler
void MainWindow::transferFailedSignalHandler(void)
{
    // Stop capture - something has gone wrong
    usbDevice->stopCapture();
    isCaptureRunning = false;
    captureStatusUpdateTimer->stop();
    captureDurationTimer->stop();
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

// Update the GUI when capture stops, and flip rename var back to false
void MainWindow::updateGuiForCaptureStop(void)
{
    // Disable functions after capture
    if (ui->actionTest_mode->isChecked()) ui->capturePushButton->setText(tr("Test data capture"));
    else ui->capturePushButton->setText(tr("Capture"));
    ui->capturePushButton->setStyleSheet("background-color: none");
    ui->actionTest_mode->setEnabled(true);
    ui->actionPreferences->setEnabled(true);
}

// Update the player remote control dialogue
void MainWindow::updatePlayerRemoteDialog(void)
{
    switch(remoteDisplayState) {
    case PlayerCommunication::DisplayState::off:
        playerRemoteDialog->setDisplayMode(PlayerRemoteDialog::DisplayMode::displayOff);
        break;
    case PlayerCommunication::DisplayState::on:
        playerRemoteDialog->setDisplayMode(PlayerRemoteDialog::DisplayMode::displayOn);
        break;
    case PlayerCommunication::DisplayState::unknownDisplayState:
        playerRemoteDialog->setDisplayMode(PlayerRemoteDialog::DisplayMode::displayOff);
        break;
    }

    switch(remoteSpeed) {
    case 0:
        playerRemoteDialog->setMultiSpeed(PlayerRemoteDialog::MultiSpeed::multiSm16);
        break;
    case 1:
        playerRemoteDialog->setMultiSpeed(PlayerRemoteDialog::MultiSpeed::multiSm14);
        break;
    case 2:
        playerRemoteDialog->setMultiSpeed(PlayerRemoteDialog::MultiSpeed::multiSm13);
        break;
    case 3:
        playerRemoteDialog->setMultiSpeed(PlayerRemoteDialog::MultiSpeed::multiSm12);
        break;
    case 4:
        playerRemoteDialog->setMultiSpeed(PlayerRemoteDialog::MultiSpeed::multiX1);
        break;
    case 5:
        playerRemoteDialog->setMultiSpeed(PlayerRemoteDialog::MultiSpeed::multiX2);
        break;
    case 6:
        playerRemoteDialog->setMultiSpeed(PlayerRemoteDialog::MultiSpeed::multiX3);
        break;
    case 7:
        playerRemoteDialog->setMultiSpeed(PlayerRemoteDialog::MultiSpeed::multiX4);
        break;
    }
}

// Timer callback to update amplitude display
void MainWindow::updateAmplitudeLabel(void)
{
    ui->meanAmplitudeLabel->setText(QString::number(ui->am->getMeanAmplitude(), 'f', 3));
}

// Update amplitude UI elements
void MainWindow::updateAmplitudeUI(void)
{
    // If any amplitude display is enabled, capture amplitude data
    if (configuration->getAmplitudeEnabled() || configuration->getGraphType() != Configuration::GraphType::noGraph) {
        connect(amplitudeTimer, SIGNAL(timeout()), ui->am, SLOT(updateBuffer()));
    } else {
        disconnect(amplitudeTimer, SIGNAL(timeout()), ui->am, SLOT(updateBuffer()));
    }

    // Update amplitude label, driven by timer
    if (configuration->getAmplitudeEnabled()) {
        ui->meanAmplitudeLabel->setText("0.000");
        connect(amplitudeTimer, SIGNAL(timeout()), this, SLOT(updateAmplitudeLabel()));
    } else {
        disconnect(amplitudeTimer, SIGNAL(timeout()), this, SLOT(updateAmplitudeLabel()));
        ui->meanAmplitudeLabel->setText("N/A");
    }

    // Update amplitude graph
    if (configuration->getGraphType() == Configuration::GraphType::QCPMean) {
        ui->am->setVisible(true);
        connect(amplitudeTimer, SIGNAL(timeout()), ui->am, SLOT(plotGraph()));
    } else {
        disconnect(amplitudeTimer, SIGNAL(timeout()), ui->am, SLOT(plotGraph()));
        ui->am->setVisible(false);
    }
}
