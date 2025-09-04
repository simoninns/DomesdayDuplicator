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
#include "UsbDeviceLibUsb.h"
#ifdef _WIN32
#include "UsbDeviceWinUsb.h"
#endif
#include "amplitudemeasurement.h"
#include <QFile>
#include <cmath>
#include <cctype>
#include <sstream>
#include <iomanip>
#ifdef _WIN32
// It'd be nice to use this on Linux, but under linux the max version of GCC is bound to the OS version, and libstdc++
// was very slow to add C++20 features, including format. This means a lot of recent Linux OS installs, like Ubuntu
// 22.04 LTS, can't compile this code. We could switch to Clang, but Qt doesn't support Clang, meaning we'd have to
// build it from source and run an unsupported config.
#include <format>
#endif
#include "json/json.hpp"

MainWindow::MainWindow(const ILogger& log, QWidget* parent) :
    QMainWindow(parent),
    log(log)
{
    ui.reset(new Ui::MainWindow());
    ui->setupUi(this);

    // Load the application's configuration settings file
    configuration.reset(new Configuration());

    // Create the about dialogue
    aboutDialog.reset(new AboutDialog(this));

    // Create the advanced naming dialogue
    advancedNamingDialog.reset(new AdvancedNamingDialog(this));

    // Create the configuration dialogue
    configurationDialog.reset(new ConfigurationDialog(this));
    connect(configurationDialog.get(), &ConfigurationDialog::configurationChanged, this, &MainWindow::configurationChangedSignalHandler);

    // Create the player remote dialogue
    playerRemoteDialog.reset(new PlayerRemoteDialog(this));
    connect(playerRemoteDialog.get(), &PlayerRemoteDialog::remoteControlCommand, this, &MainWindow::remoteControlCommandSignalHandler);
    connect(playerRemoteDialog.get(), &PlayerRemoteDialog::remoteControlSearch, this, &MainWindow::remoteControlSearchSignalHandler);
    connect(playerRemoteDialog.get(), &PlayerRemoteDialog::remoteControlManualSerialCommand, this, &MainWindow::remoteControlManualSerialCommandHandler);
    connect(playerRemoteDialog.get(), &PlayerRemoteDialog::remoteControlReadUserCodes, this, &MainWindow::remoteControlReadUserCodesCommandHandler);
    playerRemoteDialog->setEnabled(false); // Disable the dialogue until a player is connected
    ui->stopPlayerWhenCaptureStops->setEnabled(false);
    ui->stopCaptureWhenPlayerStops->setEnabled(false);

    // Create the automatic capture dialogue
    automaticCaptureDialog.reset(new AutomaticCaptureDialog(this));
    connect(automaticCaptureDialog.get(), &AutomaticCaptureDialog::startAutomaticCapture, this, &MainWindow::startAutomaticCaptureDialogSignalHandler);
    connect(automaticCaptureDialog.get(), &AutomaticCaptureDialog::stopAutomaticCapture, this, &MainWindow::stopAutomaticCaptureDialogSignalHandler);
    automaticCaptureDialog->setEnabled(false); // Disable the dialogue until a player is connected

    // Set up a timer for updating the automatic capture status information
    automaticCaptureTimer.reset(new QTimer(this));
    connect(automaticCaptureTimer.get(), SIGNAL(timeout()), this, SLOT(updateAutomaticCaptureStatus()));
    automaticCaptureTimer->start(100); // Update 10 times per second

    // Start the player control object
    playerControl.reset(new PlayerControl(this));
    connect(playerControl.get(), &PlayerControl::automaticCaptureComplete, this, &MainWindow::automaticCaptureCompleteSignalHandler);
    connect(playerControl.get(), &PlayerControl::startCapture, this, &MainWindow::startCaptureSignalHandler);
    connect(playerControl.get(), &PlayerControl::stopCapture, this, &MainWindow::stopCaptureSignalHandler);
    connect(playerControl.get(), &PlayerControl::playerConnected, this, &MainWindow::playerConnectedSignalHandler);
    connect(playerControl.get(), &PlayerControl::playerDisconnected, this, &MainWindow::playerDisconnectedSignalHandler);
    startPlayerControl();

    // Set the capture flag to not running
    isCaptureRunning = false;
    isCaptureStopping = false;

    // Add a label to the status bar for displaying the USB device status
    usbStatusLabel.reset(new QLabel);
    ui->statusBar->addWidget(usbStatusLabel.get());
    usbStatusLabel->setText(tr("No USB capture device is attached"));

    // Disable the capture button
    ui->capturePushButton->setEnabled(false);

    // Disable the test mode option
    ui->actionTest_mode->setEnabled(false);

    // Set up amplitude timer, and update amplitude UI
    amplitudeTimer.reset(new QTimer(this));
    updateAmplitudeUI();

    // Set up the Domesday Duplicator USB device and connect the signal handlers
#ifdef _WIN32
    if (configuration->getUseWinUsb())
    {
        usbDevice.reset(new UsbDeviceWinUsb(log));
    }
    else
    {
#endif
        usbDevice.reset(new UsbDeviceLibUsb(log));
#ifdef _WIN32
    }
#endif
    if (!usbDevice->Initialize(configuration->getUsbVid(), configuration->getUsbPid()))
    {
        qDebug() << "MainWindow::MainWindow(): Failed to initialize UsbDeviceLibUsb!";
    }

    // Set up a timer for updating capture results
    captureStatusUpdateTimer.reset(new QTimer(this));
    connect(captureStatusUpdateTimer.get(), SIGNAL(timeout()), this, SLOT(updateCaptureStatus()));

    // Set up a timer for updating player control information
    playerControlTimer.reset(new QTimer(this));
    connect(playerControlTimer.get(), SIGNAL(timeout()), this, SLOT(updatePlayerControlInformation()));
    playerControlTimer->start(100); // Update 10 times per second

    // Set up a timer for updating device status
    deviceStatusTimer.reset(new QTimer(this));
    connect(deviceStatusTimer.get(), SIGNAL(timeout()), this, SLOT(updateDeviceStatus()));
    deviceStatusTimer->start(100); // Update 10 times per second

    // Defaults for the remote control toggle settings
    remoteDisplayState = PlayerCommunication::DisplayState::off;
    remoteAudioState = PlayerCommunication::AudioState::digitalStereo;
    remoteSpeed = 4;
    remoteChapterFrameMode = PlayerCommunication::ChapterFrameMode::chapter;
    updatePlayerRemoteDialog();

    // Storage space information
    storageInfo.reset(new QStorageInfo());
    storageInfo->setPath(configuration->getCaptureDirectory());

    // Storage information update timer
    storageInfoTimer.reset(new QTimer(this));
    connect(storageInfoTimer.get(), SIGNAL(timeout()), this, SLOT(updateStorageInformation()));
    storageInfoTimer->start(200); // Update 5 times per second

    // Set player as disconnected
    isPlayerConnected = false;

    // Load the window geometry from the configuration
    restoreGeometry(configuration->getMainWindowGeometry());
    playerRemoteDialog->restoreGeometry(configuration->getPlayerRemoteDialogGeometry());
    advancedNamingDialog->restoreGeometry(configuration->getAdvancedNamingDialogGeometry());
    automaticCaptureDialog->restoreGeometry(configuration->getAutomaticCaptureDialogGeometry());
    configurationDialog->restoreGeometry(configuration->getConfigurationDialogGeometry());

    // Update control visibility
    RefreshControlVisibility();
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
    usbDevice.reset();

    // Schedule the objects for deletion
    playerControl->deleteLater();

    // Wait for the objects to be deleted
    playerControl->wait();

    // Delete the UI
    ui.reset();

    qDebug() << "MainWindow::~MainWindow(): All threads stopped; done.";
}

void MainWindow::RefreshControlVisibility()
{
    // Update the advanced capture stats visibility
    bool showAdvancedCaptureStats = configuration->getShowAdvancedCaptureStats();
    ui->sampleCountPreLabel->setVisible(showAdvancedCaptureStats);
    ui->sampleCountLabel->setVisible(showAdvancedCaptureStats);
    ui->minValuePreLabel->setVisible(showAdvancedCaptureStats);
    ui->minValueLabel->setVisible(showAdvancedCaptureStats);
    ui->minValueClippedPreLabel->setVisible(showAdvancedCaptureStats);
    ui->minValueClippedLabel->setVisible(showAdvancedCaptureStats);
    ui->maxValuePreLabel->setVisible(showAdvancedCaptureStats);
    ui->maxValueLabel->setVisible(showAdvancedCaptureStats);
    ui->maxValueClippedPreLabel->setVisible(showAdvancedCaptureStats);
    ui->maxValueClippedLabel->setVisible(showAdvancedCaptureStats);
    ui->recentMinValuePreLabel->setVisible(showAdvancedCaptureStats);
    ui->recentMinValueLabel->setVisible(showAdvancedCaptureStats);
    ui->recentMinValueClippedPreLabel->setVisible(showAdvancedCaptureStats);
    ui->recentMinValueClippedLabel->setVisible(showAdvancedCaptureStats);
    ui->recentMaxValuePreLabel->setVisible(showAdvancedCaptureStats);
    ui->recentMaxValueLabel->setVisible(showAdvancedCaptureStats);
    ui->recentMaxValueClippedPreLabel->setVisible(showAdvancedCaptureStats);
    ui->recentMaxValueClippedLabel->setVisible(showAdvancedCaptureStats);
}

// Signal handlers ----------------------------------------------------------------------------------------------------

// Configuration changed signal handler
void MainWindow::configurationChangedSignalHandler()
{
    qDebug() << "MainWindow::configurationChangedSignalHandler(): Configuration has been changed";

    // Save the configuration
    configurationDialog->saveConfiguration(*configuration);

    // Restart the player control
    startPlayerControl();

    // Update the target directory for the storage information
    storageInfo->setPath(configuration->getCaptureDirectory());

    // Update advanced naming UI
    advancedNamingDialog->setPerSideNotesEnabled(configuration->getPerSideNotesEnabled());
    advancedNamingDialog->setPerSideMintEnabled(configuration->getPerSideMintEnabled());

    // Update control visibility
    RefreshControlVisibility();

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

void MainWindow::remoteControlManualSerialCommandHandler(QString commandString)
{
    playerControl->sendManualCommand(commandString);
}

void MainWindow::remoteControlReadUserCodesCommandHandler()
{
    playerControl->requestStandardUserCodeRead();
    playerControl->requestPioneerUserCodeRead();
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
void MainWindow::stopAutomaticCaptureDialogSignalHandler()
{
    playerControl->stopAutomaticCapture();
}

// Update the automatic capture status (called by a timer)
void MainWindow::updateAutomaticCaptureStatus()
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
void MainWindow::startCaptureSignalHandler()
{
    qDebug() << "MainWindow::startCaptureSignalHandler(): Got start capture signal from player control";
    if (!isCaptureRunning) on_capturePushButton_clicked();
}

// Handle the stop capture signal from the player control object
void MainWindow::stopCaptureSignalHandler()
{
    qDebug() << "MainWindow::stopCaptureSignalHandler(): Got stop capture signal from player control";
    if (isCaptureRunning) on_capturePushButton_clicked();
}

// Signal handler for player connected signal from player control
void MainWindow::playerConnectedSignalHandler()
{
    qDebug() << "MainWindow::playerConnectedSignalHandler(): Received player connected signal";
    // Enable remote control dialogue
    playerRemoteDialog->setEnabled(true);

    // Enable automatic-capture (is capture is currently allowed)
    if (ui->capturePushButton->isEnabled()) automaticCaptureDialog->setEnabled(true);

    // Enable the stop player option
    if (ui->capturePushButton->isEnabled()) ui->stopPlayerWhenCaptureStops->setEnabled(true);
    if (ui->capturePushButton->isEnabled()) ui->stopCaptureWhenPlayerStops->setEnabled(true);

    isPlayerConnected = true;
}

// Signal handler for player disconnected signal from player control
void MainWindow::playerDisconnectedSignalHandler()
{
    qDebug() << "MainWindow::playerConnectedSignalHandler(): Received player disconnected signal";
    // Disable remote control dialogue
    playerRemoteDialog->setEnabled(false);

    // Disable automatic-capture
    automaticCaptureDialog->setEnabled(false);

    // Disable the stop player option
    ui->stopPlayerWhenCaptureStops->setEnabled(false);
    ui->stopCaptureWhenPlayerStops->setEnabled(false);

    isPlayerConnected = false;
}

void MainWindow::updateCaptureStatus()
{
    // Update our transfer statistics. We update these even if a transfer is stopping or stopped.
    size_t mbWritten = usbDevice->GetFileSizeWrittenInBytes() / (1024 * 1024);
    ui->dataCapturedLabel->setText(QString::number(mbWritten) + (tr(" MiB")));
    ui->numberOfTransfersLabel->setText(QString::number(usbDevice->GetNumberOfTransfers()));
    ui->sampleCountLabel->setText(std::to_string(usbDevice->GetProcessedSampleCount()).c_str());
    ui->minValueLabel->setText(std::to_string(usbDevice->GetMinSampleValue()).c_str());
    ui->maxValueLabel->setText(std::to_string(usbDevice->GetMaxSampleValue()).c_str());
    ui->minValueClippedLabel->setText(std::to_string(usbDevice->GetClippedMinSampleCount()).c_str());
    ui->maxValueClippedLabel->setText(std::to_string(usbDevice->GetClippedMaxSampleCount()).c_str());
    ui->recentMinValueLabel->setText(std::to_string(usbDevice->GetRecentMinSampleValue()).c_str());
    ui->recentMaxValueLabel->setText(std::to_string(usbDevice->GetRecentMaxSampleValue()).c_str());
    ui->recentMinValueClippedLabel->setText(std::to_string(usbDevice->GetRecentClippedMinSampleCount()).c_str());
    ui->recentMaxValueClippedLabel->setText(std::to_string(usbDevice->GetRecentClippedMaxSampleCount()).c_str());

    // If the capture process was requeted to stop and has now in fact stopped, perform our final tasks for this capture
    // and return the UI state to normal.
    if (isCaptureStopping && !isCaptureRunning)
    {
        // Latch our capture end time
        captureEndTime = std::chrono::steady_clock::now();
        auto captureElapsedTime = captureEndTime - captureStartTime;

        // Stop our update timers
        captureStatusUpdateTimer->stop();
        amplitudeTimer->stop();

        // Tell the player to stop playing if requested, and we haven't already sent a stop command from an error
        // occurring. Note that if we send a second stop command here while the player is in the process of stopping,
        // it'll cause the tray to eject once the stop is complete, so we want to avoid that.
        if (isPlayerConnected && !playerStopRequested && ui->stopPlayerWhenCaptureStops->isChecked() && (playerControl->getPlayerState() != PlayerCommunication::PlayerState::stop))
        {
            playerControl->setPlayerState(PlayerCommunication::PlayerState::stop);
        }

        // Rename output file if duration checkbox is clicked
        auto usedCaptureFilePath = captureFilePath;
        if (advancedNamingDialog->getDurationChecked())
        {
            // Build a string representation of the capture duration
            qDebug() << "ainWindow::StartCapture(): Appending duration to filename";
#ifdef _WIN32
            std::string durationString = std::format("{:%HH%MM%SS}", std::chrono::floor<std::chrono::seconds>(captureElapsedTime));
#else
            std::string durationString = QTime(0, 0, 0, 0).addSecs(std::chrono::floor<std::chrono::seconds>(captureElapsedTime).count()).toString("hh'H'mm'M'ss'S'").toStdString();
#endif

            // Build a new path for the output file with the duration included
            auto newFileName = captureFilePath.stem();
            newFileName += "_";
            newFileName += durationString;
            newFileName += captureFilePath.extension();
            auto durationFilePath = captureFilePath;
            durationFilePath.replace_filename(newFileName);

            // Rename the output file
            std::filesystem::rename(captureFilePath, durationFilePath);
            qDebug() << "MainWindow::StartCapture(): Renamed file to" << durationFilePath;
            usedCaptureFilePath = durationFilePath;
        }

        // Build our json metadata
        nlohmann::json infoFile;

        // Populate the player serial info
        infoFile["serialInfo"]["playerModelCode"] = playerControl->getPlayerModelCode().toStdString();
        infoFile["serialInfo"]["playerModelName"] = playerControl->getPlayerModelName().toStdString();
        infoFile["serialInfo"]["playerVesionNumber"] = playerControl->getPlayerVersionNumber().toStdString();
        auto discType = playerDiscTypeCached;
        if (discType == PlayerCommunication::DiscType::CAV)
        {
            infoFile["serialInfo"]["discType"] = "CAV";
        }
        else if (discType == PlayerCommunication::DiscType::CLV)
        {
            infoFile["serialInfo"]["discType"] = "CLV";
        }
        if (playerDiscStatusCached.has_value())
        {
            infoFile["serialInfo"]["discStatus"] = playerDiscStatusCached.value().toStdString();
        }
        if (playerStandardUserCodeCached.has_value())
        {
            infoFile["serialInfo"]["discStandardUserCode"] = playerStandardUserCodeCached.value().toStdString();
        }
        if (playerPioneerUserCodeCached.has_value())
        {
            std::string playerCode = playerPioneerUserCodeCached.value().toStdString();
            std::string userCodePrintable;
            for (size_t i = 0; i < playerCode.size(); ++i)
            {
                if (playerCode[i] == '\\')
                {
                    userCodePrintable.push_back('\\');
                    userCodePrintable.push_back('\\');
                }
                else if (!std::isprint((unsigned char)playerCode[i]))
                {
                    std::stringstream stream;
                    stream << "\\x" << std::hex << std::setfill('0') << std::setw(2) << (unsigned int)playerCode[i];
                    userCodePrintable.append(stream.str());
                }
                else
                {
                    userCodePrintable.push_back(playerCode[i]);
                }
            }
            infoFile["serialInfo"]["discPioneerUserCode"] = userCodePrintable;
        }
        if (minPlayerTimeCode >= 0)
        {
            infoFile["serialInfo"]["minTimeCode"] = minPlayerTimeCode;
        }
        if (maxPlayerTimeCode >= 0)
        {
            infoFile["serialInfo"]["maxTimeCode"] = maxPlayerTimeCode;
        }
        if (minPlayerFrameNumber >= 0)
        {
            infoFile["serialInfo"]["minFrameNumber"] = minPlayerFrameNumber;
        }
        if (maxPlayerFrameNumber >= 0)
        {
            infoFile["serialInfo"]["maxFrameNumber"] = maxPlayerFrameNumber;
        }
        if (minPlayerPhysicalPosition >= 0)
        {
            infoFile["serialInfo"]["minPhysicalPosition"] = minPlayerPhysicalPosition;
        }
        if (maxPlayerPhysicalPosition >= 0)
        {
            infoFile["serialInfo"]["maxPhysicalPosition"] = maxPlayerPhysicalPosition;
        }

        // Populate the manually entered naming info
        if (namingDiskTitle.has_value())
        {
            infoFile["namingInfo"]["title"] = namingDiskTitle.value().toStdString();
        }
        if (namingDiskIsCav.has_value())
        {
            infoFile["namingInfo"]["type"] = (namingDiskIsCav.value() ? "CAV" : "CLV");
        }
        if (namingDiskIsNtsc.has_value())
        {
            infoFile["namingInfo"]["format"] = (namingDiskIsNtsc.value() ? "NTSC" : "PAL");
        }
        if (namingDiskAudioType.has_value())
        {
            switch (namingDiskAudioType.value())
            {
            case AdvancedNamingDialog::AudioType::Analog:
                infoFile["namingInfo"]["audioType"] = "Analog";
                break;
            case AdvancedNamingDialog::AudioType::AC3:
                infoFile["namingInfo"]["audioType"] = "AC3";
                break;
            case AdvancedNamingDialog::AudioType::DTS:
                infoFile["namingInfo"]["audioType"] = "DTS";
                break;
            case AdvancedNamingDialog::AudioType::Default:
                infoFile["namingInfo"]["audioType"] = "Default";
                break;
            }
        }
        if (namingDiskSide.has_value())
        {
            infoFile["namingInfo"]["side"] = namingDiskSide.value();
        }
        if (namingDiskMintMarks.has_value())
        {
            infoFile["namingInfo"]["mintMarks"] = namingDiskMintMarks.value().toStdString();
        }
        if (namingDiskNotes.has_value())
        {
            infoFile["namingInfo"]["notes"] = namingDiskNotes.value().toStdString();
        }
        if (namingDiskMetadataNotes.has_value())
        {
            infoFile["namingInfo"]["metadataNotes"] = namingDiskMetadataNotes.value().toStdString();
        }

        // Populate the capture info
        infoFile["captureInfo"]["transferResult"] = (int)usbDevice->GetTransferResult();
        infoFile["captureInfo"]["durationInMilliseconds"] = std::chrono::round<std::chrono::milliseconds>(captureElapsedTime).count();
        infoFile["captureInfo"]["transferCount"] = usbDevice->GetNumberOfTransfers();
        infoFile["captureInfo"]["numberOfDiskBuffersWritten"] = usbDevice->GetNumberOfDiskBuffersWritten();
        infoFile["captureInfo"]["fileSizeWrittenInBytes"] = usbDevice->GetFileSizeWrittenInBytes();
        infoFile["captureInfo"]["sampleCount"] = usbDevice->GetProcessedSampleCount();
        infoFile["captureInfo"]["minSampleValue"] = usbDevice->GetMinSampleValue();
        infoFile["captureInfo"]["maxSampleValue"] = usbDevice->GetMaxSampleValue();
        infoFile["captureInfo"]["clippedMinSampleCount"] = usbDevice->GetClippedMinSampleCount();
        infoFile["captureInfo"]["clippedMaxSampleCount"] = usbDevice->GetClippedMaxSampleCount();
        infoFile["captureInfo"]["sequenceMarkersPresent"] = usbDevice->GetTransferHadSequenceNumbers();
        infoFile["captureInfo"]["creationTimestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();

        // Helper function to turn our sample times into a millisecond count since the start of the capture process,
        // encoded as fixed-length strings with 8 characters. This is enough to keep the indexes in numeric order for
        // up to 24 hours, which makes the file more human readable.
        auto sampleTimeToString = [&](std::chrono::time_point<std::chrono::steady_clock> sampleTime)
            {
                auto sampleTimeSinceCaptureStart = std::chrono::round<std::chrono::milliseconds>(sampleTime - captureStartTime);
                std::ostringstream sampleTimeAsFixedLengthString;
                sampleTimeAsFixedLengthString << std::setfill('0') << std::setw(8) << sampleTimeSinceCaptureStart.count();
                return sampleTimeAsFixedLengthString.str();
            };

        // Populate the amplitude record
        for (const auto& entry : amplitudeRecord)
        {
            if (!std::isnan(entry.amplitude) && !std::isinf(entry.amplitude))
            {
                auto sampleTimeString = sampleTimeToString(entry.sampleTime);
                infoFile["timeSampledData"]["amplitudeRecord"][sampleTimeString] = entry.amplitude;
            }
        }

        // Populate the timecode/frame number record
        for (const auto& entry : playerTimeCodeRecord)
        {
            auto sampleTimeString = sampleTimeToString(entry.sampleTime);
            std::string timeCodeString = std::to_string(entry.timeCodeOrFrameNumber);
            if (entry.inLeadIn)
            {
                timeCodeString = "<" + timeCodeString;
            }
            else if (entry.inLeadOut)
            {
                timeCodeString = ">" + timeCodeString;
            }
            infoFile["timeSampledData"]["timeCodeRecord"][sampleTimeString] = timeCodeString;
        }

        // Populate the player physical position record
        for (const auto& entry : playerPhysicalPositionRecord)
        {
            auto sampleTimeString = sampleTimeToString(entry.sampleTime);
            infoFile["timeSampledData"]["physicalPositionRecord"][sampleTimeString] = entry.physicalPosition;
        }

        // Populate the player state record
        for (const auto& entry : playerStatusRecord)
        {
            auto sampleTimeString = sampleTimeToString(entry.sampleTime);
            infoFile["timeSampledData"]["statusRecord"][sampleTimeString] = entry.playerState;
        }

        // Turn the json structure into a string. We "prettify" this json with an indent to make it easier to visually
        // inspect.
        const int jsonIndentLevel = 4;
        std::string jsonString = infoFile.dump(jsonIndentLevel);

        // Write the json metadata to our metadata output file
        std::filesystem::path metadataFilePath = usedCaptureFilePath;
        metadataFilePath.replace_extension(".json");
        std::ofstream metadataFile;
        metadataFile.open(metadataFilePath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!metadataFile.is_open())
        {
            qDebug() << "startCapture(): Failed to create the json metadata output file at path" << metadataFilePath;
        }
        else
        {
            metadataFile.write(jsonString.data(), jsonString.size());
        }

        // Update the gui state now that capturing is complete
        if (ui->actionTest_mode->isChecked())
        {
            ui->capturePushButton->setText(tr("Test data capture"));
        }
        else
        {
            ui->capturePushButton->setText(tr("Capture"));
        }
        ui->capturePushButton->setStyleSheet("background-color: none");
        ui->capturePushButton->setEnabled(true);
        ui->actionTest_mode->setEnabled(true);
        ui->actionPreferences->setEnabled(true);
        return;
    }

    // Update the current capture time
    auto captureElapsedTime = std::chrono::steady_clock::now() - captureStartTime;
#ifdef _WIN32
    std::string captureElapsedTimeAsString = std::format("{:%H:%M:%S}", std::chrono::floor<std::chrono::seconds>(captureElapsedTime));
#else
    std::string captureElapsedTimeAsString = QTime(0, 0, 0, 0).addSecs(std::chrono::floor<std::chrono::seconds>(captureElapsedTime).count()).toString("hh:mm:ss").toStdString();
#endif
    ui->durationLabel->setText(captureElapsedTimeAsString.c_str());

    // If the capture is stopping but the stopping process isn't complete, abort any further processing.
    if (isCaptureStopping)
    {
        return;
    }

    // If the capture isn't stopping but the usb device isn't currently transferring, something's gone wrong. In this
    // case we flag the capture to stop, and display an error message to the user.
    bool captureRunning = usbDevice->GetTransferInProgress();
    if (!isCaptureStopping && !captureRunning)
    {
        // Stop capture - something has gone wrong
        StopCapture();

        // Tell the player to stop playing if requested. Note that we need to do this here as we're about to block on
        // the modal message box below, so we can't let it be picked up in the handler for when the capture is stopped.
        // We want the player to stop on error, not play continuously until the error dialog is dismissed. We set a flag
        // here to indicate a stop has already been requested, so we don't send a second stop command later and trigger
        // the tray to open.
        if (isPlayerConnected && ui->stopPlayerWhenCaptureStops->isChecked() && (playerControl->getPlayerState() != PlayerCommunication::PlayerState::stop))
        {
            playerStopRequested = true;
            playerControl->setPlayerState(PlayerCommunication::PlayerState::stop);
        }

        // Show an error based on the transfer result
        QMessageBox messageBox;
        auto transferResult = usbDevice->GetTransferResult();
        std::string errorMessage = "An internal program error occurred. Please run with --debug and check the logs.";
        switch (transferResult)
        {
#ifndef _WIN32
            case UsbDeviceBase::TransferResult::UsbMemoryLimit:
                errorMessage = "Your kernel USB buffer size was too small. Consult the software documentation on how to increase your \"usbfs_memory_mb\" settings (highly recommended), or enable the \"Limited USB Queue\" option in the USB preferences.";
                break;
#endif
            case UsbDeviceBase::TransferResult::BufferUnderflow:
                errorMessage = "A buffer underflow occurred. Either your USB connection, CPU, or HDD was unable to keep up with the required transfer rate. Check your USB buffer settings and consult the software documentation.";
                break;
            case UsbDeviceBase::TransferResult::UsbTransferFailure:
                errorMessage = "An unrecoverable error occurred in the USB transfer process";
                break;
            case UsbDeviceBase::TransferResult::FileWriteError:
                errorMessage = "An error occurred writing to the target file";
                break;
            case UsbDeviceBase::TransferResult::SequenceMismatch:
                errorMessage = "A sequence mismatch occurred when processing the captured data. Data was lost from the input stream, probably from a buffer underflow. Check your USB buffer settings and consult the software documentation.";
                break;
            case UsbDeviceBase::TransferResult::VerificationError:
                errorMessage = "A verification error occurred validating the test sequence. There may be a software or hardware error with your capture device.";
                break;
        }
        messageBox.critical(this, "Error", errorMessage.c_str());
        messageBox.setFixedSize(500, 200);
        return;
    }

    // If time-limited capture is enabled, stop the capture if we've exceeded the target time.
    if (ui->limitDurationCheckBox->isChecked())
    {
        auto timeLimitAsQTime = ui->durationLimitTimeEdit->time();
        auto timeLimit = std::chrono::hours(timeLimitAsQTime.hour()) + std::chrono::minutes(timeLimitAsQTime.minute()) + std::chrono::seconds(timeLimitAsQTime.second());
        if (captureElapsedTime >= timeLimit)
        {
            StopCapture();
        }
    }

    // If the player has stopped playing, and capture has been requested to stop in that case, stop the capture process.
    if (ui->stopCaptureWhenPlayerStops->isChecked() && (playerControl->getPlayerState() == PlayerCommunication::PlayerState::stop))
    {
        StopCapture();
    }
}

void MainWindow::updateDeviceStatus()
{
    // If the player and USB device present state hasn't changed, abort any further processing.
    bool usbDevicePresent = usbDevice->DevicePresent(configuration->getUsbPreferredDevice().toStdString());
    if ((usbDevicePresentLastCheck == usbDevicePresent) && (isPlayerConnectedLastCheck == isPlayerConnected))
    {
        return;
    }

    // Update the list of usb devices in the configuration dialog
    std::vector<std::string> devicePaths;
    if (usbDevice->GetPresentDevicePaths(devicePaths))
    {
        configurationDialog->updateDeviceList(devicePaths);
    }

    // Update the UI state to reflect the new device/player connection status
    if (!usbDevicePresent)
    {
        qDebug() << "MainWindow::updateDeviceStatus(): Domesday Duplicator USB device has been detached";

        // Show the device status in the status bar
        usbStatusLabel->setText(tr("No USB capture device is attached"));

        // Disable the capture button
        ui->capturePushButton->setEnabled(false);

        // Disable the automatic capture dialogue
        automaticCaptureDialog->setEnabled(false);

        // Disable the stop player option
        ui->stopPlayerWhenCaptureStops->setEnabled(false);
        ui->stopCaptureWhenPlayerStops->setEnabled(false);

        // Disable the test mode option
        ui->actionTest_mode->setEnabled(false);
    }
    else
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

        // Enable the stop player option
        if (isPlayerConnected) ui->stopPlayerWhenCaptureStops->setEnabled(true);
        if (isPlayerConnected) ui->stopCaptureWhenPlayerStops->setEnabled(true);

        // Enable the test mode option
        ui->actionTest_mode->setEnabled(true);
    }
    usbDevicePresentLastCheck = usbDevicePresent;
    isPlayerConnectedLastCheck = isPlayerConnected;
}

// Update the player control labels
void MainWindow::updatePlayerControlInformation()
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

    // Retrieve the current player status info
    bool isPlayerConnected = playerControl->getPlayerConnected();
    auto discType = playerControl->getDiscType();
    auto discStatus = playerControl->getDiscStatus();
    auto standardUserCode = playerControl->getStandardUserCode();
    auto pioneerUserCode = playerControl->getPioneerUserCode();
    auto playerState = playerControl->getPlayerState();
    bool inLeadIn = playerControl->getInLeadIn();
    bool inLeadOut = playerControl->getInLeadOut();
    qint32 frameNumber = playerControl->getCurrentFrameNumber();
    qint32 timeCode = playerControl->getCurrentTimeCode();
    float physicalPosition = playerControl->getPhysicalPosition();

    // Update the player status information for the capture metadata record
    if (isCaptureRunning && !isCaptureStopping && isPlayerConnected)
    {
        if (discType != PlayerCommunication::DiscType::unknownDiscType)
        {
            playerDiscTypeCached = discType;
            if (!playerDiscStatusCached.has_value())
            {
                if (!discStatus.isEmpty())
                {
                    playerDiscStatusCached = discStatus;
                }
            }
            if (!playerStandardUserCodeCached.has_value())
            {
                if (!standardUserCode.isEmpty())
                {
                    playerStandardUserCodeCached = standardUserCode;
                }
            }
            if (!playerPioneerUserCodeCached.has_value())
            {
                if (!pioneerUserCode.isEmpty())
                {
                    playerPioneerUserCodeCached = pioneerUserCode;
                }
            }
        }
        if (discType == PlayerCommunication::DiscType::CAV)
        {
            minPlayerFrameNumber = (minPlayerFrameNumber < 0) ? frameNumber : std::min(frameNumber, minPlayerFrameNumber);
            maxPlayerFrameNumber = (maxPlayerFrameNumber < 0) ? frameNumber : std::max(frameNumber, maxPlayerFrameNumber);
            playerTimeCodeRecord.push_back({ std::chrono::steady_clock::now(), frameNumber, inLeadIn, inLeadOut });
        }
        else if (discType == PlayerCommunication::DiscType::CLV)
        {
            minPlayerTimeCode = (minPlayerTimeCode < 0) ? timeCode : std::min(timeCode, minPlayerTimeCode);
            maxPlayerTimeCode = (maxPlayerTimeCode < 0) ? timeCode : std::max(timeCode, maxPlayerTimeCode);
            playerTimeCodeRecord.push_back({ std::chrono::steady_clock::now(), timeCode, inLeadIn, inLeadOut });
        }
    }
    playerStatusRecord.push_back({ std::chrono::steady_clock::now(), (isPlayerConnected ? playerState : (PlayerCommunication::PlayerState)-1)});
    if (physicalPosition != 0)
    {
        minPlayerPhysicalPosition = (minPlayerPhysicalPosition < 0) ? physicalPosition : std::min(physicalPosition, minPlayerPhysicalPosition);
        maxPlayerPhysicalPosition = (maxPlayerPhysicalPosition < 0) ? physicalPosition : std::max(physicalPosition, maxPlayerPhysicalPosition);
        playerPhysicalPositionRecord.push_back({ std::chrono::steady_clock::now(), physicalPosition });
    }

    // Build the position information string based on disc type and player status
    QString playerPositionString;
    if (!isPlayerConnected)
    {
        playerPositionString = "Unknown";
    }
    else if (playerState == PlayerCommunication::PlayerState::stop)
    {
        playerPositionString = "Disc stopped";
    }
    else
    {
        if (discType == PlayerCommunication::DiscType::CAV)
        {
            playerPositionString = QString::number(frameNumber);

            // Add lead-in/lead-out designation if known
            if (inLeadIn)
            {
                playerPositionString.prepend("<");
                playerPositionString.append(" (lead-in)");
            }
            else if (inLeadOut)
            {
                playerPositionString.prepend(">");
                playerPositionString.append(" (lead-out)");
            }
        }
        else if (discType == PlayerCommunication::DiscType::CLV)
        {
            QString timeCodeString;
            QString hourString;
            QString minuteString;
            QString secondString;
            QString frameString;

            // Get the full 7 character time-code string
            timeCodeString = QString("%1").arg(timeCode, 7, 10, QChar('0'));

            // Split up the time-code
            hourString = timeCodeString.left(1);
            minuteString = timeCodeString.left(3).right(2);
            secondString = timeCodeString.left(5).right(2);
            frameString = timeCodeString.left(7).right(2);

            // Display time-code
            playerPositionString = ("0" + hourString + ":" + minuteString + ":" + secondString + ":" + frameString);

            // Add lead-in/lead-out designation if known
            if (inLeadIn)
            {
                playerPositionString.prepend("<");
                playerPositionString.append(" (lead-in)");
            }
            else if (inLeadOut)
            {
                playerPositionString.prepend(">");
                playerPositionString.append(" (lead-out)");
            }
        }
        else
        {
            playerPositionString = "Unknown disc type";
        }
    }

    // Append the physical position if known to the position information string
    if (physicalPosition > 0)
    {
        std::stringstream physicalPositionStream;
        physicalPositionStream << std::fixed << std::setprecision(2) << physicalPosition;
        playerPositionString.append((std::string(" [") + physicalPositionStream.str() + "mm]").c_str());
    }

    // Display the position information string
    ui->playerPositionLabel->setText(playerPositionString);

    // Display the player status information based on disc type and player status
    QString playerStatusString;
    if (isPlayerConnected)
    {
        if (discType == PlayerCommunication::DiscType::CAV) playerStatusString += "CAV ";
        if (discType == PlayerCommunication::DiscType::CLV) playerStatusString += "CLV ";
        if (playerState == PlayerCommunication::PlayerState::stop) playerStatusString += "Stopped";
        if (playerState == PlayerCommunication::PlayerState::pause) playerStatusString += "Paused";
        if (playerState == PlayerCommunication::PlayerState::stillFrame) playerStatusString += "Still-Frame";
        if (playerState == PlayerCommunication::PlayerState::play) playerStatusString += "Playing";
    }
    else
    {
        playerStatusString = "No player connected (serial)";
    }
    ui->playerStatusLabel->setText(playerStatusString);

    // Update the displayed response to the last manual command
    playerRemoteDialog->setPlayerResponseToManualCommand(playerControl->getManualCommandResponse());
    playerRemoteDialog->setStandardUserCode(standardUserCode);
    playerRemoteDialog->setPioneerUserCode(pioneerUserCode);
}

// Update the storage information labels
void MainWindow::updateStorageInformation()
{
    storageInfo->refresh();
    if (storageInfo->isValid()) {
        // Calculate the space required per second based on the selected sample format
        size_t samplesPerSecond = 40 * 1000 * 1000;
        size_t bytesPerSecond = 0;
        switch (configuration->getCaptureFormat())
        {
        case Configuration::CaptureFormat::sixteenBitSigned:
            // 2 bytes per sample
            bytesPerSecond = samplesPerSecond * 2;
            break;
        case Configuration::CaptureFormat::tenBitPacked:
            // 5 bytes every 4 samples
            bytesPerSecond = (samplesPerSecond / 4) * 5;
            break;
        case Configuration::CaptureFormat::tenBitCdPacked:
            // 5 bytes every 16 samples
            bytesPerSecond = (samplesPerSecond / 16) * 5;
            break;
        }

        // Calculate the amount of time we can record based on the available space
        size_t bytesAvailable = (size_t)storageInfo->bytesAvailable();
        size_t availableSeconds = bytesAvailable / bytesPerSecond;

        // Print the time available
        if (availableSeconds > (60 * 60 * 96))
        {
            ui->spaceAvailableLabel->setText("More than 96 hours");
        }
        else
        {
#ifdef _WIN32
            std::string timeAvailableString = std::format("{:%H:%M:%S}", std::chrono::seconds(availableSeconds));
#else
            std::string timeAvailableString = QTime(0, 0, 0, 0).addSecs(static_cast<qint32>(availableSeconds)).toString("hh:mm:ss").toStdString();
#endif
            ui->spaceAvailableLabel->setText(timeAvailableString.c_str());
        }
    } else {
        // Unable to get storage info
        ui->spaceAvailableLabel->setText(tr("Unknown"));
    }

}

void MainWindow::startPlayerControl()
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
        usbDevice->SendConfigurationCommand(configuration->getUsbPreferredDevice().toStdString(), true);
        ui->capturePushButton->setText("Test data capture");
    } else {
        // Turn test-mode off
        usbDevice->SendConfigurationCommand(configuration->getUsbPreferredDevice().toStdString(), false);
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
    configurationDialog->loadConfiguration(*configuration);
    configurationDialog->show();
}

void MainWindow::StopCapture()
{
    // If no capture is currently in progress or it is currently stopping, abort any further processing.
    if (!isCaptureRunning || isCaptureStopping)
    {
        return;
    }

    // Update the button text to indicate the capture is stopping
    ui->capturePushButton->setText(tr("Stopping"));
    ui->capturePushButton->setEnabled(false);

    // Flag the capture process to stop
    isCaptureStopping = true;
    playerControl->stopAutomaticCapture(); // Stop auto-capture if in progress
    std::thread stopThread([&]()
        {
            usbDevice->StopCapture();
            isCaptureRunning = false;
        });
    stopThread.detach();
}

void MainWindow::StartCapture()
{
    // If a capture is already in progress, abort any further processing.
    if (isCaptureRunning)
    {
        return;
    }

    // Ensure that the test mode option matches the device configuration
    bool isTestMode = ui->actionTest_mode->isChecked();
    qDebug() << "MainWindow::StartCapture(): Setting device's test mode flag to" << isTestMode;
    usbDevice->SendConfigurationCommand(configuration->getUsbPreferredDevice().toStdString(), isTestMode);

    // Reset the amplitude buffer
    ui->am->clearBuffer();

    // Use the advanced naming dialogue to generate the capture file name
    captureFilePath = std::filesystem::path((char8_t const*)configuration->getCaptureDirectory().toUtf8().data());
    captureFilePath += std::filesystem::path((char8_t const*)advancedNamingDialog->getFileName(isTestMode).toUtf8().data());

    // Reset our amplitude buffer. We reserve enough storage to capture 24 hours worth of samples at the 1000ms timer
    // interval, to prevent stalls from a large memory allocation/relocation operation during capture.
    amplitudeRecord.clear();
    amplitudeRecord.reserve(24 * 60 * 60);

    // Reset our time code/frame number and player status buffers. We reserve enough storage to capture 24 hours worth
    // of samples at the 100ms timer interval, to prevent stalls from a large memory allocation/relocation operation
    // during capture.
    playerTimeCodeRecord.clear();
    playerTimeCodeRecord.reserve(24 * 60 * 60 * 10);
    playerStatusRecord.clear();
    playerStatusRecord.reserve(24 * 60 * 60 * 10);
    playerPhysicalPositionRecord.clear();
    playerPhysicalPositionRecord.reserve(24 * 60 * 60 * 10);

    // Reset our cached serial control info
    playerDiscTypeCached = PlayerCommunication::DiscType::unknownDiscType;
    playerDiscStatusCached.reset();
    playerStandardUserCodeCached.reset();
    playerPioneerUserCodeCached.reset();
    minPlayerTimeCode = -1;
    maxPlayerTimeCode = -1;
    minPlayerFrameNumber = -1;
    maxPlayerFrameNumber = -1;
    minPlayerPhysicalPosition = -1;
    maxPlayerPhysicalPosition = -1;

    // Store fields from the advanced naming dialog for metadata saving later
    namingDiskTitle.reset();
    namingDiskIsCav.reset();
    namingDiskIsNtsc.reset();
    namingDiskAudioType.reset();
    namingDiskSide.reset();
    namingDiskMintMarks.reset();
    namingDiskNotes.reset();
    namingDiskMetadataNotes.reset();
    if (advancedNamingDialog->getDiskTitleChecked())
    {
        namingDiskTitle = advancedNamingDialog->getDiskTitle();
    }
    if (advancedNamingDialog->getDiskTypeChecked())
    {
        namingDiskIsCav = advancedNamingDialog->getDiskTypeCav();
    }
    if (advancedNamingDialog->getFormatChecked())
    {
        namingDiskIsNtsc = advancedNamingDialog->getFormatNtsc();
    }
    if (advancedNamingDialog->getAudioChecked())
    {
        namingDiskAudioType = advancedNamingDialog->getAudioType();
    }
    if (advancedNamingDialog->getDiscSideChecked())
    {
        namingDiskSide = advancedNamingDialog->getDiscSide();
    }
    if (!advancedNamingDialog->getMintMarks().isEmpty())
    {
        namingDiskMintMarks = advancedNamingDialog->getMintMarks();
    }
    if (!advancedNamingDialog->getNotes().isEmpty())
    {
        namingDiskNotes = advancedNamingDialog->getNotes();
    }
    if (!advancedNamingDialog->getMetadataNotes().isEmpty())
    {
        namingDiskMetadataNotes = advancedNamingDialog->getMetadataNotes();
    }

    // Change the file suffix based on the selected capture format
    if (configuration->getCaptureFormat() == Configuration::CaptureFormat::tenBitPacked)
    {
        captureFilePath += ".lds";
    }
    else if (configuration->getCaptureFormat() == Configuration::CaptureFormat::sixteenBitSigned)
    {
        captureFilePath += ".raw";
    }
    else
    {
        captureFilePath += ".cds";
    }

    // Disable functions during capture
    ui->capturePushButton->setText(tr("Stop Capture"));
    ui->capturePushButton->setStyleSheet("background-color: red");
    ui->actionTest_mode->setEnabled(false);
    ui->actionPreferences->setEnabled(false);

    // Make sure the configuration dialogue is closed
    configurationDialog->hide();

    // Reset the capture statistics
    ui->numberOfTransfersLabel->setText(tr("0"));

    // Determine the capture format
    UsbDeviceBase::CaptureFormat captureFormat = UsbDeviceBase::CaptureFormat::Signed16Bit;
    if (configuration->getCaptureFormat() == Configuration::CaptureFormat::tenBitPacked)
    {
        qDebug() << "MainWindow::StartCapture(): Starting transfer - 10-bit packed";
        captureFormat = UsbDeviceBase::CaptureFormat::Unsigned10Bit;
    }
    else if (configuration->getCaptureFormat() == Configuration::CaptureFormat::tenBitCdPacked)
    {
        qDebug() << "MainWindow::StartCapture(): Starting transfer - 10-bit packed 4:1 decimated";
        captureFormat = UsbDeviceBase::CaptureFormat::Unsigned10Bit4to1Decimation;
    }
    else
    {
        qDebug() << "MainWindow::StartCapture(): Starting transfer - 16-bit";
        captureFormat = UsbDeviceBase::CaptureFormat::Signed16Bit;
    }

    // Initialize our transfer state settings
    playerStopRequested = false;

    // Determine the USB transer settings. Unless a small USB transfer queue is selected, we assume we can buffer up to
    // the disk buffer limit. Currently this would be up to 512mb of memory. Small transfer queue mode is done for the
    // benefit of linux, where the "usbfs_memory_mb" kernel setting is often 16mb for the entire system. If a small
    // queue is selected, we assume no more than 12mb of memory can be queued, which is 3/4 of that limit.
    const size_t smallUsbTransferQueueSize = 12 * 1024 * 1024;
    size_t maxDiskBufferQueueSizeInBytes = configuration->getDiskBufferQueueSize();
    size_t maxUsbTransferQueueSizeInBytes = (configuration->getUseSmallUsbTransferQueue() ? smallUsbTransferQueueSize : maxDiskBufferQueueSizeInBytes);
    bool useSmallUsbTransfers = configuration->getUseSmallUsbTransfers();
    bool useAsyncFileIo = configuration->getUseAsyncFileIo();

    // Attempt to start the capture process
    qDebug() << "MainWindow::StartCapture(): Starting capture to file:" << captureFilePath.string().c_str();
    if (!usbDevice->StartCapture(captureFilePath, captureFormat, configuration->getUsbPreferredDevice().toStdString(), isTestMode, useSmallUsbTransfers, useAsyncFileIo, maxUsbTransferQueueSizeInBytes, maxDiskBufferQueueSizeInBytes))
    {
        // Show an error based on the transfer result
        qDebug() << "MainWindow::StartCapture(): Failed to begin the capture process";
        QMessageBox messageBox;
        auto transferResult = usbDevice->GetTransferResult();
        std::string errorMessage = "Failed to start capture. An internal program error occurred. Please run with --debug and check the logs.";
        switch (transferResult)
        {
            case UsbDeviceBase::TransferResult::Running:
                errorMessage = "Failed to start capture. A capture was already in progress.";
                break;
            case UsbDeviceBase::TransferResult::FileCreationError:
                errorMessage = "Failed to start capture. The target file could not be created.";
                break;
            case UsbDeviceBase::TransferResult::ConnectionFailure:
                errorMessage = "Failed to start capture. A connection could not be established to the USB capture device.";
                break;
        }
        messageBox.critical(this, "Error", errorMessage.c_str());
        messageBox.setFixedSize(500, 200);

        // Restore the UI state since capture won't be starting
        if (ui->actionTest_mode->isChecked())
        {
            ui->capturePushButton->setText(tr("Test data capture"));
        }
        else
        {
            ui->capturePushButton->setText(tr("Capture"));
        }
        ui->capturePushButton->setStyleSheet("background-color: none");
        ui->actionTest_mode->setEnabled(true);
        ui->actionPreferences->setEnabled(true);
        return;
    }
    captureStartTime = std::chrono::steady_clock::now();
    isCaptureRunning = true;
    isCaptureStopping = false;
    qDebug() << "MainWindow::StartCapture(): Transfer started";

    // Start a timer to display the capture statistics
    ui->durationLabel->setText(tr("00:00:00"));
    captureStatusUpdateTimer->start(100); // Update 10 times a second (1000 / 10 = 100)

    // Start graph processing
    amplitudeTimer->start(1000);
}

// Main window - capture button clicked
void MainWindow::on_capturePushButton_clicked()
{
    if (!isCaptureRunning)
    {
        StartCapture();
    }
    else
    {
        StopCapture();
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

// Update the player remote control dialogue
void MainWindow::updatePlayerRemoteDialog()
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

void MainWindow::updateAmplitudeDataBuffer()
{
    const size_t bufferSampleSizeInBytes = 2 * 1024;
    std::vector<uint8_t> bufferSample;
    if (usbDevice->GetNextBufferSample(bufferSample))
    {
        ui->am->updateBuffer(bufferSample);
    }
    usbDevice->QueueBufferSampleRequest(bufferSampleSizeInBytes);
}

// Timer callback to update amplitude display and record
void MainWindow::updateAmplitudeLabel()
{
    auto amplitude = ui->am->getMeanAmplitude();
    if (isCaptureRunning && !isCaptureStopping)
    {
        amplitudeRecord.push_back({ std::chrono::steady_clock::now(), amplitude });
    }
    ui->meanAmplitudeLabel->setText(QString::number(amplitude, 'f', 3));
}

// Update amplitude UI elements
void MainWindow::updateAmplitudeUI()
{
    // If any amplitude display is enabled, capture amplitude data
    if (configuration->getAmplitudeLabelEnabled() || configuration->getAmplitudeChartEnabled()) {
        connect(amplitudeTimer.get(), SIGNAL(timeout()), this, SLOT(updateAmplitudeDataBuffer()));
    } else {
        disconnect(amplitudeTimer.get(), SIGNAL(timeout()), this, SLOT(updateAmplitudeDataBuffer()));
    }

    // Update amplitude label, driven by timer
    if (configuration->getAmplitudeLabelEnabled()) {
        ui->meanAmplitudeLabel->setText("0.000");
        connect(amplitudeTimer.get(), SIGNAL(timeout()), this, SLOT(updateAmplitudeLabel()));
    } else {
        disconnect(amplitudeTimer.get(), SIGNAL(timeout()), this, SLOT(updateAmplitudeLabel()));
        ui->meanAmplitudeLabel->setText("N/A");
    }

    // Update amplitude graph
    if (configuration->getAmplitudeChartEnabled()) {
        ui->am->setVisible(true);
        connect(amplitudeTimer.get(), SIGNAL(timeout()), ui->am, SLOT(plotGraph()));
    } else {
        disconnect(amplitudeTimer.get(), SIGNAL(timeout()), ui->am, SLOT(plotGraph()));
        ui->am->setVisible(false);
    }
}
