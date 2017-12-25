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
    ui->transferPushButton->setText(tr("Start capturing"));

    // Add some default status text to show the state of the USB device
    status = new QLabel;
    ui->statusBar->addWidget(status);
    if (domDupUsbDevice->isConnected()) {
        status->setText(tr("Connected"));
        ui->transferPushButton->setEnabled(true);
        ui->testModeCheckBox->setEnabled(true);
        ui->cavCapturePushButton->setEnabled(true);
        ui->clvCapturePushButton->setEnabled(true);
    } else {
        status->setText(tr("Domesday Duplicator USB device not connected"));
        ui->transferPushButton->setEnabled(false);
        ui->testModeCheckBox->setEnabled(false);
        ui->cavCapturePushButton->setEnabled(false);
        ui->clvCapturePushButton->setEnabled(false);
    }

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

    // Set up a timer for updated the player control information
    updateTimer = new QTimer(this);
    connect(updateTimer, SIGNAL(timeout()), this, SLOT(updatePlayerControlInfo()));

    // Disable the start transfer buttons (until a destination file name is supplied)
    ui->transferPushButton->setEnabled(false);
    ui->cavCapturePushButton->setEnabled(false);
    ui->clvCapturePushButton->setEnabled(false);

    // Disable the test mode checkbox until a USB device is connected
    ui->testModeCheckBox->setEnabled(false);

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
    cavPicCaptureActive = false;
    cavPicPollTimer = new QTimer(this);
    connect(cavPicPollTimer, SIGNAL(timeout()), this, SLOT(cavPicPoll()));
    cavPicPollTimer->start(50); // Update 20 times a second
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
        status->setText(tr("Connected"));

        // Enable transfer if there is a filename selected
        if (!fileName.isEmpty()) {
            ui->transferPushButton->setEnabled(true);
            ui->cavCapturePushButton->setEnabled(true);
            ui->clvCapturePushButton->setEnabled(true);
            ui->testModeCheckBox->setEnabled(true);
            ui->testModeCheckBox->setChecked(false);
        }
    } else {
        qDebug() << "MainWindow::usbStatusChanged(): USB device is not connected";
        status->setText(tr("Domesday Duplicator USB device not connected"));

        // Are we mid-capture?
        if (captureFlag) {
            qDebug() << "MainWindow::usbStatusChanged(): USB device removed during capture.  Attempting to clean up";
            this->stopTransfer();
        }

        ui->transferPushButton->setEnabled(false);
        ui->cavCapturePushButton->setEnabled(false);
        ui->clvCapturePushButton->setEnabled(false);
        ui->testModeCheckBox->setEnabled(false);
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
        ui->cavCapturePushButton->setEnabled(false);
        ui->clvCapturePushButton->setEnabled(false);
        ui->testModeCheckBox->setEnabled(false);
    } else {
        // File name specified
        qDebug() << "MainWindow::on_actionSave_As_triggered(): Save as filename = " << fileName;

        // Enable the capture control buttons (if a USB device is connected)
        if (domDupUsbDevice->isConnected()) {
            ui->transferPushButton->setEnabled(true);
            ui->cavCapturePushButton->setEnabled(true);
            ui->clvCapturePushButton->setEnabled(true);
            ui->testModeCheckBox->setEnabled(true);
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
            ui->transferPushButton->setText(tr("Stop capturing"));

            // Disable the test mode check box
            ui->testModeCheckBox->setEnabled(false);

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
                ui->transferPushButton->setText(tr("Start capturing"));
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
        ui->transferPushButton->setText(tr("Start capturing"));

        // Enable the test mode check box
        ui->testModeCheckBox->setEnabled(true);

        // Enable the transfer button
        ui->transferPushButton->setEnabled(true);
    } else {
        qDebug() << "MainWindow::stopTransfer(): Called, but transfer is not in progress";
    }
}

// Test mode check box toggled
void MainWindow::on_testModeCheckBox_toggled(bool checked)
{
    bool responseFlag = false;

    if (checked) {
        // Test mode on
        qDebug() << "MainWindow::on_testModeCheckBox_toggled(): Test mode data generation on";

        // Verify that the USB device is still connected
        if (domDupUsbDevice->isConnected()) {
            // Configure and open the USB device
            domDupUsbDevice->setupDevice();
            responseFlag = domDupUsbDevice->openDevice();

            if (responseFlag) {
                // Send test mode on vendor specific USB command
                domDupUsbDevice->sendVendorSpecificCommand(0xB6, 1);

                // Close the USB device
                domDupUsbDevice->closeDevice();
            } else {
                // Could not open device
                qDebug() << "MainWindow::on_testModeCheckBox_toggled(): Cannot set test mode on - could not open USB device";
            }
        } else {
            // Device no longer connected
            qDebug() << "MainWindow::on_testModeCheckBox_toggled(): Cannot set test mode on - USB device not connected";
        }
    } else {
        // Test mode off
        qDebug() << "MainWindow::on_testModeCheckBox_toggled(): Test mode data generation off";

        // Verify that the USB device is still connected
        if (domDupUsbDevice->isConnected()) {
            // Configure and open the USB device
            domDupUsbDevice->setupDevice();
            responseFlag = domDupUsbDevice->openDevice();

            if (responseFlag) {
                // Send test mode off vendor specific USB command
                domDupUsbDevice->sendVendorSpecificCommand(0xB6, 0);

                // Close the USB device
                domDupUsbDevice->closeDevice();
            } else {
                // Could not open device
                qDebug() << "MainWindow::on_testModeCheckBox_toggled(): Cannot set test mode off - could not open USB device";
            }
        } else {
            // Device no longer connected
            qDebug() << "MainWindow::on_testModeCheckBox_toggled(): Cannot set test mode off - USB device not connected";
        }
    }
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
    }

}

// Called by a player control event (from the PIC controls)
void MainWindow::handlePlayerControlEvent(playerControlDialog::PlayerControlEvents controlEvent,
                                          quint32 parameter)
{
    // Determine the event and process
    switch(controlEvent) {
        case playerControlDialog::PlayerControlEvents::event_playClicked:
        playerControl->command(lvdpControl::PlayerCommands::command_play, 0);
        break;

        case playerControlDialog::PlayerControlEvents::event_pauseClicked:
        playerControl->command(lvdpControl::PlayerCommands::command_pause, 0);
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
        case state_idle:
            ui->cavPicStatus->setText("Idle");

            // Wait for the flag then transition from idle
            if (cavPicCaptureActive) cavPicNextState = state_startPlayer;
            else cavPicNextState = state_idle;
            break;

        case state_startPlayer:
            ui->cavPicStatus->setText("Starting player");

            // Lock the physical player controls for safety
            playerControl->command(lvdpControl::PlayerCommands::command_keyLockOn, 0);
            lvdpPlayerControl->lockAllPlayerControls();

            // Send the play command if the player isn't already started
            if (!playerControl->isPlaying() && !playerControl->isPaused() ) {
                playerControl->command(lvdpControl::PlayerCommands::command_play, 0);
            }

            cavPicNextState = state_waitForPlay;
            break;

        case state_waitForPlay:
            ui->cavPicStatus->setText("Waiting for disc");

            if (playerControl->isPlaying() || playerControl->isPaused()) {
                if (playerControl->isCav()) {
                    cavPicNextState = state_determineDiscLength;
                } else {
                    // Disc is not CAV - error
                    cavPicNextState = state_error;
                }
            }

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::cavPicPoll(): Player command error flagged - aborting";
                cavPicNextState = state_error;
            }

            break;

        case state_determineDiscLength:
            ui->cavPicStatus->setText("Determining disc length");
            playerControl->command(lvdpControl::PlayerCommands::command_resetDiscLength, 0);
            playerControl->command(lvdpControl::PlayerCommands::command_getDiscLength, 0);

            cavPicNextState = state_waitForDetermineDiscLength;

            break;

        case state_waitForDetermineDiscLength:
            ui->cavPicStatus->setText("Waiting for disc length");
            if (playerControl->getDiscLength() != 0) {
                qDebug() << "MainWindow::cavPicPoll(): Disc length is" << playerControl->getDiscLength() << "frames";

                // Is lead-in capture required?
                if (captureLeadInFlag) {
                    qDebug() << "MainWindow::cavPicPoll(): Capture lead-in requested";

                    // In order to capture lead-in we have to first stop the disc
                    // and then start the capture before sending the play command
                    playerControl->command(lvdpControl::PlayerCommands::command_stop, 0);
                    cavPicNextState = state_stopPlayer;
                } else {
                    // Lead-in not required, start from frame
                    cavPicNextState = state_seekToFrame;
                }
            }

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::cavPicPoll(): Player command error flagged - aborting";
                cavPicNextState = state_error;
            }
            break;

        case state_stopPlayer:
            ui->cavPicStatus->setText("Waiting for disc to stop");
            if (!playerControl->isPlaying() && !playerControl->isPaused()) {
                // Player has stopped - start the transfer and start playing
                startTransfer();
                playerControl->command(lvdpControl::PlayerCommands::command_play, 0);
                cavPicNextState = state_waitForStartCapture;
            }
            break;

        case state_seekToFrame:
            ui->cavPicStatus->setText("Seeking for start frame");

            // Verify that the startFrame is valid
            if (startFrame == 0) startFrame = 1;
            if (playerControl->getDiscLength() < startFrame) startFrame = playerControl->getDiscLength() - 1;

            // Send the seek command
            qDebug() << "MainWindow::cavPicPoll(): Requesting start frame" << startFrame;
            playerControl->command(lvdpControl::PlayerCommands::command_seek, startFrame);
            cavPicNextState = state_waitForSeek;
            break;

        case state_waitForSeek:
            ui->cavPicStatus->setText("Waiting for start frame");

            // Verify that the startFrame is valid
            if (startFrame == 0) startFrame = 1;
            if (playerControl->getDiscLength() < startFrame) startFrame = playerControl->getDiscLength() - 1;

            // Get the seek position and pause
            if (startFrame == playerControl->currentFrameNumber()) {
                cavPicNextState = state_startCapture;
            }

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::cavPicPoll(): Player command error flagged - aborting";
                cavPicNextState = state_error;
            }

            break;

        case state_startCapture:
            ui->cavPicStatus->setText("Starting capture");

            // Verify that the end frame is valid
            if (endFrame > playerControl->getDiscLength()) endFrame = playerControl->getDiscLength();

            qDebug() << "MainWindow::cavPicPoll(): Waiting for end frame" << endFrame;

            // Start the capture
            startTransfer();

            // Play the disc
            playerControl->command(lvdpControl::PlayerCommands::command_play, 0);

            cavPicNextState = state_waitForStartCapture;
            break;

        case state_waitForStartCapture:
            ui->cavPicStatus->setText("Waiting for disc playing");
            // Don't start waiting for the end frame unless the player is playing
            if (playerControl->isPlaying()) cavPicNextState = state_waitForEndFrame;

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::cavPicPoll(): Player command error flagged - aborting";
                cavPicNextState = state_error;
            }
            break;

        case state_waitForEndFrame:
            ui->cavPicStatus->setText("Waiting for end frame");

            // Verify that the end frame is valid
            if (endFrame > playerControl->getDiscLength()) endFrame = playerControl->getDiscLength();

            // Check if the frame number is exceeded
            if (playerControl->currentFrameNumber() >= endFrame)
                cavPicNextState = state_stopCapture;

            // Attempting to read past the last frame can cause the player
            // to stop...  ensure we are still playing
            if (!playerControl->isPlaying())
                cavPicNextState = state_stopCapture;

            // Check for player command errors
            if (playerControl->isLastCommandError()) {
                qDebug() << "MainWindow::cavPicPoll(): Player command error flagged - aborting";
                cavPicNextState = state_error;
            }
            break;

        case state_stopCapture:
            ui->cavPicStatus->setText("Stopping capture");

            // Stop the capture
            stopTransfer();

            // Stop the player (if still playing)
            if (playerControl->isPlaying()) playerControl->command(lvdpControl::PlayerCommands::command_pause, 0);

            // Unlock the physical player controls
            playerControl->command(lvdpControl::PlayerCommands::command_keyLockOff, 0);
            lvdpPlayerControl->unlockAllPlayerControls();

            cavPicNextState = state_idle;
            cavPicCaptureActive = false;
            break;

        case state_error:
            ui->cavPicStatus->setText("Error");

            // Stop any pending capture
            stopTransfer();

            // Unlock the physical player controls
            playerControl->command(lvdpControl::PlayerCommands::command_keyLockOff, 0);
            lvdpPlayerControl->unlockAllPlayerControls();

            cavPicNextState = state_idle;
            cavPicCaptureActive = false;
            break;
    }
}

// CAV PIC capture button clicked
void MainWindow::on_cavCapturePushButton_clicked()
{
    // Ensure any command errors are cleared
    playerControl->isLastCommandError();

    // Start the CAV PIC capture
    cavPicCaptureActive = true;
}
