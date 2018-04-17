/************************************************************************

    mainwindow.cpp

    QT GUI Capture application for Domesday Duplicator
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

    // Set up the USB device object
    domDupUsbDevice = new usbDevice;

    // Set up the LVDP control communication
    playerControl = new lvdpControl;

    // Set the capture flag to false (not capturing)
    captureFlag = false;

    // Set up the transfer button
    ui->transferPushButton->setText(tr("Initialising"));

    // Add some default status text to show the state of the USB device
    usbStatusLabel = new QLabel;
    ui->statusBar->addWidget(usbStatusLabel);
    usbStatusChanged(domDupUsbDevice->isConnected());

    // Add status text to show the state of the PIC serial connection
    serialStatusLabel = new QLabel;
    ui->statusBar->addWidget(serialStatusLabel);
    serialStatusLabel->setText(tr("PIC: Not Connected"));

    // Connect to the usb device's signals to show insertion/removal events
    connect(domDupUsbDevice, SIGNAL(statusChanged(bool)), SLOT(usbStatusChanged(bool)));

    // Set up the text labels
    ui->capturedDataLabel->setText(tr("0"));
    ui->transferSpeedLabel->setText(tr("0"));
    ui->diskBufferProgressBar->setValue(0);
    ui->testModeFailLabel->setText(tr("0"));

    // Set up a timer for updating capture results
    captureTimer = new QTimer(this);
    connect(captureTimer, SIGNAL(timeout()), this, SLOT(updateCaptureInfo()));

    // Set up a timer for updating the player control information
    updateTimer = new QTimer(this);
    connect(updateTimer, SIGNAL(timeout()), this, SLOT(updatePlayerControlInfo()));

    // Disable the start transfer buttons (until a destination file name is supplied)
    ui->transferPushButton->setEnabled(false);
    ui->cavCapturePushButton->setEnabled(false);
    ui->clvCapturePushButton->setEnabled(false);

    // Create the about dialogue
    aboutDomDup = new aboutDialog(this);

    // Create the LVDP serial settings dialogue
    lvdpSerialPortSelect = new serialPortSelectDialog(this);

    // Create the LVDP control dialogue
    lvdpPlayerControl = new playerControlDialog(this);

    // Connect to the serial port configured signal
    connect(lvdpSerialPortSelect, SIGNAL(serialPortChanged()), this, SLOT(serialPortStatusChange()));

    // Start updating the player control information
    updateTimer->start(100); // Update 10 times a second

    // Connect PIC control events to the handler
    connect(lvdpPlayerControl, SIGNAL(playerControlEvent(playerControlDialog::PlayerControlEvents, quint32)), this,
            SLOT(handlePlayerControlEvent(playerControlDialog::PlayerControlEvents, quint32)));

    // Set the default frame numbers for the CAV PIC capture
    ui->startFrameLineEdit->setText("1");
    ui->endFrameLineEdit->setText("2");

    // Disable the PIC capture options (only available if a player is connected)
    ui->cavIntegratedCaptureGroupBox->setEnabled(false);
    ui->clvIntegratedCaptureGroupBox->setEnabled(false);

    // Set up the PIC capture state-machines
    cavPicCurrentState = cavState_idle;
    cavPicNextState = cavPicCurrentState;
    clvPicCurrentState = clvState_idle;
    clvPicNextState = clvPicCurrentState;

    cavPicCaptureActive = false;
    cavPicCaptureAbort = false;
    cavPicPollTimer = new QTimer(this);
    connect(cavPicPollTimer, SIGNAL(timeout()), this, SLOT(cavPicPoll()));
    cavPicPollTimer->start(50); // Update 20 times a second

    clvPicCaptureActive = false;
    clvPicCaptureAbort = false;
    clvPicPollTimer = new QTimer(this);
    connect(clvPicPollTimer, SIGNAL(timeout()), this, SLOT(clvPicPoll()));
    clvPicPollTimer->start(50); // Update 20 times a second
}

MainWindow::~MainWindow()
{
    // Stop the player control
    playerControl->stopStateMachine();

    // Delete the UI
    delete ui;
}

// Update the USB device status based on signals from the USB detection
void MainWindow::usbStatusChanged(bool usbStatus)
{
    if (usbStatus) {
        qDebug() << "MainWindow::usbStatusChanged(): USB device is connected";
        usbStatusLabel->setText(tr("USB: Connected"));

        // Enable transfer if there is a filename selected
        if (!fileName.isEmpty()) {
            ui->transferPushButton->setEnabled(true);
            ui->transferPushButton->setText(tr("Start Capture"));
            ui->cavCapturePushButton->setEnabled(true);
            ui->clvCapturePushButton->setEnabled(true);
        } else {
            ui->transferPushButton->setText(tr("No Target File"));
        }
    } else {
        qDebug() << "MainWindow::usbStatusChanged(): USB device is not connected";
        usbStatusLabel->setText(tr("USB: Not Connected"));

        // Are we mid-capture?
        if (captureFlag) {
            qDebug() << "MainWindow::usbStatusChanged(): USB device removed during capture.  Attempting to clean up";
            this->stopTransfer();
        }

        ui->transferPushButton->setEnabled(false);
        ui->transferPushButton->setText(tr("No USB Device"));
        ui->cavCapturePushButton->setEnabled(false);
        ui->clvCapturePushButton->setEnabled(false);
    }
}

// Update the USB device configuration based on the GUI settings
void MainWindow::updateUsbDeviceConfiguration()
{
    quint16 configurationFlags = 0;

    // Set up the configuration flags (simple binary flag byte)
    if (ui->testModeCheckBox->isChecked()) configurationFlags += 1;     // Bit 0: Set = Test mode
    if (ui->palRadioButton->isChecked()) configurationFlags += 2;       // Bit 1: Set = PAL sampling (unset = NTSC)
    if (ui->dcOffsetCheckBox->isChecked()) configurationFlags += 4;     // Bit 2: Set = DC compensation on

    // Output debug
    qDebug() << "MainWindow::updateUsbDeviceConfiguration(): Sending vendor specific USB command (configuration).  Flags are =" << configurationFlags;

    // Verify that the USB device is still connected
    if (domDupUsbDevice->isConnected()) {
        // Configure and open the USB device
        domDupUsbDevice->setupDevice();

        // Open the USB device
        if (domDupUsbDevice->openDevice()) {
            // Send configuration vendor specific USB command
            domDupUsbDevice->sendVendorSpecificCommand(0xB6, configurationFlags);

            // Close the USB device
            domDupUsbDevice->closeDevice();
        } else {
            // Could not open device
            qDebug() << "MainWindow::updateUsbDeviceConfiguration(): Cannot send configure command - could not open USB device";
        }
    } else {
        // Device no longer connected
        qDebug() << "MainWindow::updateUsbDeviceConfiguration(): Cannot send configure command - USB device not connected";
    }
}

// Menu option "About" triggered
void MainWindow::on_actionAbout_triggered()
{
    // Show about about dialogue
    aboutDomDup->show();
}

// Menu option "Save As" triggered
void MainWindow::on_actionSave_As_triggered()
{
    if (fileName.isEmpty()) {
        // No previous file name selected.  Fill in the default location and file name
        fileName = QFileDialog::getSaveFileName(this, tr("Save RF capture as"), QDir::homePath()+tr("/rfcapture.raw"), tr("RAW Files (*.raw)"));
    } else {
        // Previous file name selected, fill it in again
        fileName = QFileDialog::getSaveFileName(this, tr("Save RF capture as"), fileName, tr("RAW Files (*.raw)"));
    }

    if (fileName.isEmpty()) {
        // No file name was specified
        qDebug() << "MainWindow::on_actionSave_As_triggered(): User did not supply a file name";
        ui->transferPushButton->setEnabled(false);
        ui->transferPushButton->setText(tr("No Target File"));
        ui->cavCapturePushButton->setEnabled(false);
        ui->clvCapturePushButton->setEnabled(false);
    } else {
        // File name specified
        qDebug() << "MainWindow::on_actionSave_As_triggered(): Save as filename = " << fileName;

        // Enable the capture control buttons (if a USB device is connected)
        if (domDupUsbDevice->isConnected()) {
            ui->transferPushButton->setEnabled(true);
            ui->transferPushButton->setText(tr("Start Capture"));
            ui->cavCapturePushButton->setEnabled(true);
            ui->clvCapturePushButton->setEnabled(true);
        }
    }
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
        stopTransfer();
    } else {
        // Start capturing
        qDebug() << "MainWindow::on_transferPushButton_clicked(): Starting capture";
        startTransfer();
    }
}

// Start USB capture transfer
void MainWindow::startTransfer(void)
{
    bool responseFlag = false;

    // Ensure we have a file name
    if (fileName.isEmpty()) {
        qDebug() << "MainWindow::startTransfer(): No file name specified, cannot start transfer";
        return;
    }

    if (captureFlag == false) {
        qDebug() << "MainWindow::startTransfer(): Starting transfer";

        // Verify that the USB device is still connected
        if (domDupUsbDevice->isConnected()) {
            // Set the capture flag
            captureFlag = true;

            // Update the transfer button text
            ui->transferPushButton->setText(tr("Stop Capture"));

            // Configure and open the USB device
            responseFlag = domDupUsbDevice->openDevice();

            if (responseFlag) {
                qDebug() << "MainWindow::startTransfer(): USB device opened";

                // Send start transfer vendor specific USB command
                domDupUsbDevice->sendVendorSpecificCommand(0xB5, 1);

                // Start the transfer (pass test mode on/off state)
                domDupUsbDevice->startBulkRead(ui->testModeCheckBox->isChecked(), fileName);

                // Start a timer to display the transfer information
                captureTimer->start(100); // Update 10 times a second (1000 / 10 = 100)
            } else {
                // Could not open USB device
                qDebug() << "MainWindow::startTransfer(): Cannot start transfer - Opening USB device failed";
                captureFlag = false;
                ui->transferPushButton->setText(tr("Start Capture"));
            }
        } else {
            // Cannot start transfer; USB device not detected
            qDebug() << "MainWindow::startTransfer(): Cannot start transfer - USB device not connected";
        }
    } else {
        qDebug() << "MainWindow::startTransfer(): Called, but transfer is already in progress";
    }
}

// Stop USB capture transfer
void MainWindow::stopTransfer(void)
{
    if (captureFlag == true) {
        qDebug() << "MainWindow::stopTransfer(): Stopping transfer";

        // Disable the button whilst we stop the transfer
        ui->transferPushButton->setEnabled(false);

        // Set the capture flag
        captureFlag = false;

        // Stop the transfer
        domDupUsbDevice->stopBulkRead();

        // Stop the timer displaying the transfer information
        captureTimer->stop();

        // Send stop transfer vendor specific USB command
        domDupUsbDevice->sendVendorSpecificCommand(0xB5, 0);

        // Close the USB device
        domDupUsbDevice->closeDevice();

        // Update the transfer button text
        ui->transferPushButton->setText(tr("Start Capture"));

        // Enable the transfer button
        ui->transferPushButton->setEnabled(true);
    } else {
        qDebug() << "MainWindow::stopTransfer(): Called, but transfer is not in progress";
    }
}

// Test mode check box toggled
void MainWindow::on_testModeCheckBox_toggled(bool checked)
{
    qDebug() << "MainWindow::on_testModeCheckBox_toggled():" << checked;

    // Update the USB configuration
    updateUsbDeviceConfiguration();
}

// NTSC Sample speed radio button toggled
void MainWindow::on_ntscRadioButton_toggled(bool checked)
{
    qDebug() << "MainWindow::on_ntscRadioButton_toggled():" << checked;

    // Update the USB configuration
    updateUsbDeviceConfiguration();
}

// PAL Sample speed radio button toggled
void MainWindow::on_palRadioButton_toggled(bool checked)
{
    qDebug() << "MainWindow::on_palRadioButton_toggled():" << checked;

    // We do not update the USB configuration here as it is triggered by the
    // NTSC radio button in the same group.
}

// DC offset compensation check box toggled
void MainWindow::on_dcOffsetCheckBox_toggled(bool checked)
{
    qDebug() << "MainWindow::on_dcOffsetCheckBox_toggled():" << checked;

    // Update the USB configuration
    updateUsbDeviceConfiguration();
}

// Update the capture information in the main window
void MainWindow::updateCaptureInfo(void)
{
    // Calculate and display the current amount of captured data (in MBytes)
    qreal capturedData = (qreal)(domDupUsbDevice->getPacketCounter() * domDupUsbDevice->getPacketSize()) / 1024;

    if (capturedData < 1024) ui->capturedDataLabel->setText(QString::number(capturedData, 'f', 0) + tr(" MBytes"));
    else {
        capturedData = capturedData / 1024;
        ui->capturedDataLabel->setText(QString::number(capturedData, 'f', 2) + tr(" GBytes"));
    }

    // Display the current transfer performance
    qreal transferPerformance = domDupUsbDevice->getTransferPerformance() / 1024;
    ui->transferSpeedLabel->setText(QString::number(transferPerformance, 'f', 0) + tr(" MBytes/sec"));

    // Display the available numbplayerControler of disk buffers (as a percentage)
    quint32 bufferAvailablity = (100 / domDupUsbDevice->getNumberOfDiskBuffers()) * domDupUsbDevice->getAvailableDiskBuffers();
    ui->diskBufferProgressBar->setValue(bufferAvailablity);

    // Display the number of test mode failures
    ui->testModeFailLabel->setText(QString::number(domDupUsbDevice->getTestFailureCounter()));
}

// Menu->PIC->Select player COM port triggered
void MainWindow::on_actionSelect_player_COM_port_triggered()
{
    lvdpSerialPortSelect->show();
}

// Menu->PIC->Show player control dialogue triggered
void MainWindow::on_actionShow_player_control_triggered()
{
    lvdpPlayerControl->show();
}

// Called when the serial port selection dialogue signals that the serial configuration has changed
void MainWindow::serialPortStatusChange(void)
{
    qDebug() << "MainWindow::serialPortStatusChange(): Serial port configuration changed";

    if(lvdpSerialPortSelect->isConfigured()) {
        // Connect to the player
        playerControl->serialConfigured(lvdpSerialPortSelect->getPortName(), lvdpSerialPortSelect->getBaudRate());
    } else {
        // Ensure the player is disconnected
        playerControl->serialUnconfigured();
    }
}

// Called by the player control information update timer
void MainWindow::updatePlayerControlInfo(void)
{
    //qDebug() << "MainWindow::updatePlayerControlInfo(): Updating";
    lvdpPlayerControl->updatePlayerControlInfo(
                playerControl->isConnected(),
                playerControl->isCav(),
                playerControl->currentFrameNumber(),
                playerControl->currentTimeCode(),
                playerControl->isPlaying(),
                playerControl->isPaused()
                );

    if (playerControl->isConnected()) {
        serialStatusLabel->setText(tr("PIC: Connected"));
        // If we are paused or playing, then we known the disc type
        // otherwise it's up to the user to pick the right one
        if (playerControl->isPaused() || playerControl->isPlaying()) {
            if (playerControl->isCav()) {
                ui->cavIntegratedCaptureGroupBox->setEnabled(true);
                ui->clvIntegratedCaptureGroupBox->setEnabled(false);
            } else {
                ui->cavIntegratedCaptureGroupBox->setEnabled(false);
                ui->clvIntegratedCaptureGroupBox->setEnabled(true);
            }
        } else {
            // No disc is detected, enable both options
            ui->cavIntegratedCaptureGroupBox->setEnabled(true);
            ui->clvIntegratedCaptureGroupBox->setEnabled(true);
        }
    } else {
        // Disable the PIC capture options (only available if a player is connected)
        ui->cavIntegratedCaptureGroupBox->setEnabled(false);
        ui->clvIntegratedCaptureGroupBox->setEnabled(false);
        serialStatusLabel->setText(tr("PIC: Not Connected"));
    }

}

// Called by a player control event (from the PIC controls)
void MainWindow::handlePlayerControlEvent(playerControlDialog::PlayerControlEvents controlEvent,
                                          quint32 parameter)
{
    // Determine the event and process
    switch(controlEvent) {
        case playerControlDialog::PlayerControlEvents::event_playClicked:
        // Is the disc currently playing?
        if (playerControl->isPlaying()) {
            // Send a pause or still frame (CAV only) command instead
            if (playerControl->isCav()) {
                // CAV disc playing - send still frame command
                playerControl->command(lvdpControl::PlayerCommands::command_still, 0);
            } else {
                // CLV disc playing - send pause command
                playerControl->command(lvdpControl::PlayerCommands::command_pause, 0);
            }
        } else {
            // Disc is either stopped or paused, send play command
            playerControl->command(lvdpControl::PlayerCommands::command_play, 0);
        }
        break;

        case playerControlDialog::PlayerControlEvents::event_pauseClicked:
        playerControl->command(lvdpControl::PlayerCommands::command_still, 0);
        break;

        case playerControlDialog::PlayerControlEvents::event_stopClicked:
        playerControl->command(lvdpControl::PlayerCommands::command_stop, 0);
        break;

        case playerControlDialog::PlayerControlEvents::event_stepForwardsClicked:
        playerControl->command(lvdpControl::PlayerCommands::command_stepForwards, 0);
        break;

        case playerControlDialog::PlayerControlEvents::event_stepBackwardsClicked:
        playerControl->command(lvdpControl::PlayerCommands::command_stepBackwards, 0);
        break;

        case playerControlDialog::PlayerControlEvents::event_scanForwardsClicked:
        playerControl->command(lvdpControl::PlayerCommands::command_scanForwards, 0);
        break;

        case playerControlDialog::PlayerControlEvents::event_scanBackwardsClicked:
        playerControl->command(lvdpControl::PlayerCommands::command_scanBackwards, 0);
        break;

        case playerControlDialog::PlayerControlEvents::event_keyLockOnClicked:
        playerControl->command(lvdpControl::PlayerCommands::command_keyLockOn, 0);
        break;

        case playerControlDialog::PlayerControlEvents::event_keyLockOffClicked:
        playerControl->command(lvdpControl::PlayerCommands::command_keyLockOff, 0);
        break;

        case playerControlDialog::PlayerControlEvents::event_seekClicked:
        playerControl->command(lvdpControl::PlayerCommands::command_seek, parameter);
        break;

        default:
            qDebug() << "MainWindow::handlePlayerControlEvent(): Unknown event received!";
    }
}

// CAV capture from lead-in check box toggled
void MainWindow::on_cavLeadInCheckBox_toggled(bool checked)
{
    if (checked) {
        // If checked, user cannot specify the start frame number
        ui->startFrameLineEdit->setText("0");
        ui->startFrameLineEdit->setEnabled(false);
    } else {
        ui->startFrameLineEdit->setEnabled(true);
    }
}

// CLV capture from lead-in check box toggled
void MainWindow::on_clvLeadInCheckBox_toggled(bool checked)
{
    if (checked) {
        // If checked, user cannot specify the start time-code
        QTime startTimecode;
        startTimecode.setHMS(0, 0, 0, 0);
        ui->startTimeCodeTimeEdit->setTime(startTimecode);
        ui->startTimeCodeTimeEdit->setEnabled(false);
    } else {
        ui->startTimeCodeTimeEdit->setEnabled(true);
    }
}

// CAV PIC capture state-machine
void MainWindow::cavPicPoll(void)
{
    // Transition state if required
    cavPicCurrentState = cavPicNextState;

    // Get the start and end frame positions
    quint32 startFrame = ui->startFrameLineEdit->text().toUInt();
    quint32 endFrame = ui->endFrameLineEdit->text().toUInt();

    // Get the capture lead-in flag
    bool captureLeadInFlag = ui->cavLeadInCheckBox->checkState();

    switch (cavPicCurrentState) {
        case cavState_idle:
            ui->cavPicStatus->setText("Idle");

            // Wait for the flag then transition from idle
            if (cavPicCaptureActive) cavPicNextState = cavState_startPlayer;
            else cavPicNextState = cavState_idle;
            break;

        case cavState_startPlayer:
            ui->cavPicStatus->setText("Starting player");

            // Lock the physical player controls for safety
            playerControl->command(lvdpControl::PlayerCommands::command_keyLockOn, 0);
            lvdpPlayerControl->lockAllPlayerControls();

            // Send the play command if the player isn't already started
            if (!playerControl->isPlaying() && !playerControl->isPaused() ) {
                playerControl->command(lvdpControl::PlayerCommands::command_play, 0);
            }

            cavPicNextState = cavState_waitForPlay;
            break;

        case cavState_waitForPlay:
            ui->cavPicStatus->setText("Waiting for disc");

            if (playerControl->isPlaying() || playerControl->isPaused()) {
                if (playerControl->isCav()) {
                    cavPicNextState = cavState_determineDiscLength;
                } else {
                    // Disc is not CAV - error
                    cavPicNextState = cavState_error;
                }
            }

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::cavPicPoll(): Player command error flagged - aborting";
                cavPicNextState = cavState_error;
            }

            break;

        case cavState_determineDiscLength:
            ui->cavPicStatus->setText("Determining disc length");
            playerControl->command(lvdpControl::PlayerCommands::command_resetDiscLength, 0);
            playerControl->command(lvdpControl::PlayerCommands::command_getDiscLength, 0);

            cavPicNextState = cavState_waitForDetermineDiscLength;

            break;

        case cavState_waitForDetermineDiscLength:
            ui->cavPicStatus->setText("Waiting for disc length");
            if (playerControl->getDiscLength() != 0) {
                qDebug() << "MainWindow::cavPicPoll(): Disc length is" << playerControl->getDiscLength() << "frames";

                // Is lead-in capture required?
                if (captureLeadInFlag) {
                    qDebug() << "MainWindow::cavPicPoll(): Capture lead-in requested";

                    // In order to capture lead-in we have to first stop the disc
                    // and then start the capture before sending the play command
                    playerControl->command(lvdpControl::PlayerCommands::command_stop, 0);
                    cavPicNextState = cavState_stopPlayer;
                } else {
                    // Lead-in not required, start from frame
                    cavPicNextState = cavState_seekToFrame;
                }
            }

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::cavPicPoll(): Player command error flagged - aborting";
                cavPicNextState = cavState_error;
            }
            break;

        case cavState_stopPlayer:
            ui->cavPicStatus->setText("Waiting for disc to stop");
            if (!playerControl->isPlaying() && !playerControl->isPaused()) {
                // Player has stopped - start the transfer and start playing
                startTransfer();
                playerControl->command(lvdpControl::PlayerCommands::command_play, 0);
                cavPicNextState = cavState_waitForStartCapture;
            }
            break;

        case cavState_seekToFrame:
            ui->cavPicStatus->setText("Seeking for start frame");

            // Verify that the startFrame is valid
            if (startFrame == 0) startFrame = 1;
            if (playerControl->getDiscLength() < startFrame) startFrame = playerControl->getDiscLength() - 1;

            // Send the seek command
            qDebug() << "MainWindow::cavPicPoll(): Requesting start frame" << startFrame;
            playerControl->command(lvdpControl::PlayerCommands::command_seek, startFrame);
            cavPicNextState = cavState_waitForSeek;
            break;

        case cavState_waitForSeek:
            ui->cavPicStatus->setText("Waiting for start frame");

            // Verify that the startFrame is valid
            if (startFrame == 0) startFrame = 1;
            if (playerControl->getDiscLength() < startFrame) startFrame = playerControl->getDiscLength() - 1;

            // Get the seek position and pause
            if (startFrame == playerControl->currentFrameNumber()) {
                cavPicNextState = cavState_startCapture;
            }

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::cavPicPoll(): Player command error flagged - aborting";
                cavPicNextState = cavState_error;
            }

            break;

        case cavState_startCapture:
            ui->cavPicStatus->setText("Starting capture");

            // Verify that the end frame is valid
            if (endFrame > playerControl->getDiscLength()) endFrame = playerControl->getDiscLength();

            qDebug() << "MainWindow::cavPicPoll(): Waiting for end frame" << endFrame;

            // Start the capture
            startTransfer();

            // Play the disc
            playerControl->command(lvdpControl::PlayerCommands::command_play, 0);

            cavPicNextState = cavState_waitForStartCapture;
            break;

        case cavState_waitForStartCapture:
            ui->cavPicStatus->setText("Waiting for disc playing");
            // Don't start waiting for the end frame unless the player is playing
            if (playerControl->isPlaying()) cavPicNextState = cavState_waitForEndFrame;

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::cavPicPoll(): Player command error flagged - aborting";
                cavPicNextState = cavState_error;
            }
            break;

        case cavState_waitForEndFrame:
            ui->cavPicStatus->setText("Waiting for end frame");

            // Verify that the end frame is valid
            if (endFrame > playerControl->getDiscLength()) endFrame = playerControl->getDiscLength();

            // Check if the frame number is exceeded
            if (playerControl->currentFrameNumber() >= endFrame)
                cavPicNextState = cavState_stopCapture;

            // Attempting to read past the last frame can cause the player
            // to stop...  ensure we are still playing
            if (!playerControl->isPlaying())
                cavPicNextState = cavState_stopCapture;

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::cavPicPoll(): Player command error flagged - aborting";
                cavPicNextState = cavState_error;
            }

            // Check for abort capture flag
            if (cavPicCaptureAbort) {
                qDebug() << "MainWindow::cavPicPoll(): Abort capture flag is set - stopping capture";
                cavPicNextState = cavState_stopCapture;
            }
            break;

        case cavState_stopCapture:
            ui->cavPicStatus->setText("Stopping capture");

            // Stop the capture
            stopTransfer();

            // Put the player in still frame (if still playing)
            if (playerControl->isPlaying()) playerControl->command(lvdpControl::PlayerCommands::command_still, 0);

            // Unlock the physical player controls
            playerControl->command(lvdpControl::PlayerCommands::command_keyLockOff, 0);
            lvdpPlayerControl->unlockAllPlayerControls();
            ui->cavCapturePushButton->setText("Capture");

            cavPicNextState = cavState_idle;
            cavPicCaptureActive = false;
            break;

        case cavState_error:
            ui->cavPicStatus->setText("Error");

            // Stop any pending capture
            stopTransfer();

            // Unlock the physical player controls
            playerControl->command(lvdpControl::PlayerCommands::command_keyLockOff, 0);
            lvdpPlayerControl->unlockAllPlayerControls();
            ui->cavCapturePushButton->setText("Capture");

            cavPicNextState = cavState_idle;
            cavPicCaptureActive = false;
            break;
    }
}

// CLV PIC capture state-machine
void MainWindow::clvPicPoll(void)
{
    // Transition state if required
    clvPicCurrentState = clvPicNextState;

    // Get the start and end frame positions
    QString timeCode;

    // Create a formatted 7-character string of the start time-code
    timeCode.sprintf("%01d%02d%02d00", ui->startTimeCodeTimeEdit->time().hour(),
                     ui->startTimeCodeTimeEdit->time().minute(),
                     ui->startTimeCodeTimeEdit->time().second());

    // Convert the start time-code to an integer
    quint32 startFrame = timeCode.toUInt();

    timeCode.sprintf("%01d%02d%02d00", ui->endTimeCodeTimeEdit->time().hour(),
                     ui->endTimeCodeTimeEdit->time().minute(),
                     ui->endTimeCodeTimeEdit->time().second());

    // Convert the end time-code to an integer
    quint32 endFrame = timeCode.toUInt();

    // Get the capture lead-in flag
    bool captureLeadInFlag = ui->clvLeadInCheckBox->checkState();

    switch (clvPicCurrentState) {
        case clvState_idle:
            ui->clvPicStatus->setText("Idle");

            // Wait for the flag then transition from idle
            if (clvPicCaptureActive) clvPicNextState = clvState_startPlayer;
            else clvPicNextState = clvState_idle;
            break;

        case clvState_startPlayer:
            ui->clvPicStatus->setText("Starting player");

            // Lock the physical player controls for safety
            playerControl->command(lvdpControl::PlayerCommands::command_keyLockOn, 0);
            lvdpPlayerControl->lockAllPlayerControls();

            // Send the play command if the player isn't already started
            if (!playerControl->isPlaying() && !playerControl->isPaused() ) {
                playerControl->command(lvdpControl::PlayerCommands::command_play, 0);
            }

            clvPicNextState = clvState_waitForPlay;
            break;

        case clvState_waitForPlay:
            ui->clvPicStatus->setText("Waiting for disc");

            if (playerControl->isPlaying() || playerControl->isPaused()) {
                if (!playerControl->isCav()) {
                    clvPicNextState = clvState_determineDiscLength;
                } else {
                    // Disc is not CLV - error
                    clvPicNextState = clvState_error;
                }
            }

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::clvPicPoll(): Player command error flagged - aborting";
                clvPicNextState = clvState_error;
            }

            break;

        case clvState_determineDiscLength:
            ui->clvPicStatus->setText("Determining disc length");
            playerControl->command(lvdpControl::PlayerCommands::command_resetDiscLength, 0);
            playerControl->command(lvdpControl::PlayerCommands::command_getDiscLength, 0);

            clvPicNextState = clvState_waitForDetermineDiscLength;

            break;

        case clvState_waitForDetermineDiscLength:
            ui->clvPicStatus->setText("Waiting for disc length");
            if (playerControl->getDiscLength() != 0) {
                qDebug() << "MainWindow::clvPicPoll(): Disc length is" << playerControl->getDiscLength() << "time-code frames";

                // Is lead-in capture required?
                if (captureLeadInFlag) {
                    qDebug() << "MainWindow::clvPicPoll(): Capture lead-in requested";

                    // In order to capture lead-in we have to first stop the disc
                    // and then start the capture before sending the play command
                    playerControl->command(lvdpControl::PlayerCommands::command_stop, 0);
                    clvPicNextState = clvState_stopPlayer;
                } else {
                    // Lead-in not required, start from frame
                    clvPicNextState = clvState_seekToFrame;
                }
            }

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::clvPicPoll(): Player command error flagged - aborting";
                clvPicNextState = clvState_error;
            }
            break;

        case clvState_stopPlayer:
            ui->clvPicStatus->setText("Waiting for disc to stop");
            if (!playerControl->isPlaying() && !playerControl->isPaused()) {
                // Player has stopped - start the transfer and start playing
                startTransfer();
                playerControl->command(lvdpControl::PlayerCommands::command_play, 0);
                clvPicNextState = clvState_waitForStartCapture;
            }
            break;

        case clvState_seekToFrame:
            ui->clvPicStatus->setText("Seeking for start time");

            // Verify that the startFrame is valid
            // Note: the startFrame is hmmssff, so we set to one second before the end of the disc
            if (playerControl->getDiscLength() < startFrame) startFrame = playerControl->getDiscLength() - 100;

            // Send the seek command
            qDebug() << "MainWindow::clvPicPoll(): Requesting start frame" << startFrame;
            playerControl->command(lvdpControl::PlayerCommands::command_seek, startFrame);
            clvPicNextState = clvState_waitForSeek;
            break;

        case clvState_waitForSeek:
            ui->clvPicStatus->setText("Waiting for start time");

            // Verify that the startFrame is valid
            // Note: the startFrame is hmmssff, so we set to one second before the end of the disc
            if (playerControl->getDiscLength() < startFrame) startFrame = playerControl->getDiscLength() - 100;

            // Get the seek position and pause
            if (startFrame == playerControl->currentTimeCode()) {
                clvPicNextState = clvState_startCapture;
            }

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::clvPicPoll(): Player command error flagged - aborting";
                clvPicNextState = clvState_error;
            }

            break;

        case clvState_startCapture:
            ui->clvPicStatus->setText("Starting capture");

            // Verify that the end frame is valid
            if (endFrame > playerControl->getDiscLength()) endFrame = playerControl->getDiscLength();

            qDebug() << "MainWindow::clvPicPoll(): Waiting for end frame" << endFrame;

            // Start the capture
            startTransfer();

            // Play the disc
            playerControl->command(lvdpControl::PlayerCommands::command_play, 0);

            clvPicNextState = clvState_waitForStartCapture;
            break;

        case clvState_waitForStartCapture:
            ui->clvPicStatus->setText("Waiting for disc playing");
            // Don't start waiting for the end frame unless the player is playing
            if (playerControl->isPlaying()) clvPicNextState = clvState_waitForEndFrame;

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::clvPicPoll(): Player command error flagged - aborting";
                clvPicNextState = clvState_error;
            }
            break;

        case clvState_waitForEndFrame:
            ui->clvPicStatus->setText("Waiting for end time");

            // Verify that the end frame is valid
            if (endFrame > playerControl->getDiscLength()) endFrame = playerControl->getDiscLength();

            // Check if the frame number is exceeded
            if (playerControl->currentTimeCode() >= endFrame)
                clvPicNextState = clvState_stopCapture;

            // Attempting to read past the last frame can cause the player
            // to stop...  ensure we are still playing
            if (!playerControl->isPlaying())
                clvPicNextState = clvState_stopCapture;

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::clvPicPoll(): Player command error flagged - aborting";
                clvPicNextState = clvState_error;
            }

            // Check for abort capture flag
            if (clvPicCaptureAbort) {
                qDebug() << "MainWindow::clvPicPoll(): Abort capture flag is set - stopping capture";
                clvPicNextState = clvState_stopCapture;
            }
            break;

        case clvState_stopCapture:
            ui->clvPicStatus->setText("Stopping capture");

            // Stop the capture
            stopTransfer();

            // Pause the player (if still playing)
            if (playerControl->isPlaying()) playerControl->command(lvdpControl::PlayerCommands::command_pause, 0);

            // Unlock the physical player controls
            playerControl->command(lvdpControl::PlayerCommands::command_keyLockOff, 0);
            lvdpPlayerControl->unlockAllPlayerControls();
            ui->clvCapturePushButton->setText("Capture");

            clvPicNextState = clvState_idle;
            clvPicCaptureActive = false;
            break;

        case clvState_error:
            ui->clvPicStatus->setText("Error");

            // Stop any pending capture
            stopTransfer();

            // Unlock the physical player controls
            playerControl->command(lvdpControl::PlayerCommands::command_keyLockOff, 0);
            lvdpPlayerControl->unlockAllPlayerControls();
            ui->clvCapturePushButton->setText("Capture");

            clvPicNextState = clvState_idle;
            clvPicCaptureActive = false;
            break;
    }
}

// CAV PIC capture button clicked
void MainWindow::on_cavCapturePushButton_clicked()
{
    // Ensure any command errors are cleared
    playerControl->isLastCommandError();

    // Make sure the CLV PIC capture is not running
    if (!clvPicCaptureActive) {
        if (!cavPicCaptureActive) {
            // CAV capture not running... start it
            qDebug() << "MainWindow::on_cavCapturePushButton_clicked(): Starting CAV PIC capture";
            cavPicCaptureAbort = false;
            cavPicCaptureActive = true;
            ui->cavCapturePushButton->setText("Abort");
        } else {
            // CAV capture is running... abort
            qDebug() << "MainWindow::on_cavCapturePushButton_clicked(): Aborting CAV PIC capture";
            cavPicCaptureAbort = true;
        }
    } else {
        qDebug() << "MainWindow::on_cavCapturePushButton_clicked(): Error - CLV PIC capture in progress";
    }
}

// CLV PIC capture button clicked
void MainWindow::on_clvCapturePushButton_clicked()
{
    // Ensure any command errors are cleared
    playerControl->isLastCommandError();

    // Make sure the CAV PIC capture is not running
    if (!cavPicCaptureActive) {
        if (!clvPicCaptureActive) {
            // CAV capture not running... start it
            qDebug() << "MainWindow::on_clvCapturePushButton_clicked(): Starting CLV PIC capture";
            clvPicCaptureAbort = false;
            clvPicCaptureActive = true;
            ui->clvCapturePushButton->setText("Abort");
        } else {
            // CAV capture is running... abort
            qDebug() << "MainWindow::on_clvCapturePushButton_clicked(): Aborting CLV PIC capture";
            clvPicCaptureAbort = true;
        }
    } else {
        qDebug() << "MainWindow::on_clvCapturePushButton_clicked(): Error - CAV PIC capture in progress";
    }
}


