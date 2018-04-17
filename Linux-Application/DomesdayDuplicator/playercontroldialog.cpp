/************************************************************************

    playercontroldialog.cpp

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

#include "playercontroldialog.h"
#include "ui_playercontroldialog.h"

playerControlDialog::playerControlDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::playerControlDialog)
{
    ui->setupUi(this);

    // Set up default player information values
    ui->discTypeInfoLabel->setText("N/A");
    ui->positionInfoLabel->setText("N/A");
    ui->statusInfoLabel->setText("Disconnected");

    // Limit frame number entry QLineEdit fields to number only
    ui->seekPositionLineEdit->setValidator( new QIntValidator(0, 60000, this) );

    // Default the line edit fields
    ui->seekPositionLineEdit->setText("0");

    // Disable the CAV controls groupbox
    ui->cavControlsGroupBox->setEnabled(false);

    // Disable the CLV controls groupbox
    ui->clvControlsGroupBox->setEnabled(false);

    // Disable the unavailable controls
    ui->scanBackwardsPushButton->setEnabled(false);
    ui->scanForwardsPushButton->setEnabled(false);
    ui->stepForwardsPushButton->setEnabled(false);
    ui->stepBackwardsPushButton->setEnabled(false);

    // Default to unlocked
    allControlsLocked = false;
}

playerControlDialog::~playerControlDialog()
{
    delete ui;
}

// Update the player information
void playerControlDialog::updatePlayerControlInfo(bool isConnected, bool isCav, quint32 frameNumber,
                                                  quint32 timeCode, bool isPlaying, bool isPaused)
{
    // Is a player connected?
    if (isConnected) {
        if (isPlaying || isPaused) {
            // Playing
            if (isPlaying) ui->statusInfoLabel->setText("Playing");
            else ui->statusInfoLabel->setText("Paused");

            if (isCav) {
                // CAV disc
                ui->discTypeInfoLabel->setText("CAV");

                // Enable the CAV controls
                if (!allControlsLocked) {
                    ui->playerControlsGroupBox->setEnabled(true);
                    ui->cavControlsGroupBox->setEnabled(true);
                    ui->clvControlsGroupBox->setEnabled(false);
                    ui->scanBackwardsPushButton->setEnabled(true);
                    ui->scanForwardsPushButton->setEnabled(true);
                    ui->stepForwardsPushButton->setEnabled(true);
                    ui->stepBackwardsPushButton->setEnabled(true);
                    ui->lockControlsCheckBox->setEnabled(true);
                } else {
                    ui->playerControlsGroupBox->setEnabled(false);
                    ui->cavControlsGroupBox->setEnabled(false);
                    ui->clvControlsGroupBox->setEnabled(false);
                    ui->scanBackwardsPushButton->setEnabled(false);
                    ui->scanForwardsPushButton->setEnabled(false);
                    ui->stepForwardsPushButton->setEnabled(false);
                    ui->stepBackwardsPushButton->setEnabled(false);
                    ui->lockControlsCheckBox->setEnabled(false);
                }

                // Convert frame number and display
                QString frameNumberString;
                frameNumberString.sprintf("%05d", frameNumber);
                ui->positionInfoLabel->setText(frameNumberString);
            } else {
                // CLV disc
                ui->discTypeInfoLabel->setText("CLV");

                // Enable the CLV controls
                if (!allControlsLocked) {
                    ui->playerControlsGroupBox->setEnabled(true);
                    ui->cavControlsGroupBox->setEnabled(false);
                    ui->clvControlsGroupBox->setEnabled(true);
                    ui->scanBackwardsPushButton->setEnabled(true);
                    ui->scanForwardsPushButton->setEnabled(true);
                    ui->stepForwardsPushButton->setEnabled(false);
                    ui->stepBackwardsPushButton->setEnabled(false);
                    ui->lockControlsCheckBox->setEnabled(true);
                } else {
                    ui->playerControlsGroupBox->setEnabled(false);
                    ui->cavControlsGroupBox->setEnabled(false);
                    ui->clvControlsGroupBox->setEnabled(false);
                    ui->scanBackwardsPushButton->setEnabled(false);
                    ui->scanForwardsPushButton->setEnabled(false);
                    ui->stepForwardsPushButton->setEnabled(false);
                    ui->stepBackwardsPushButton->setEnabled(false);
                    ui->lockControlsCheckBox->setEnabled(false);
                }

                // Convert time-code and display
                QString timeCodeString;
                QString hourString;
                QString minuteString;
                QString secondString;
                //QString frameString;

                // Get the full 7 character time-code string
                timeCodeString.sprintf("%07d", timeCode);

                // Split up the time-code
                hourString = timeCodeString.left(1);
                minuteString = timeCodeString.left(3).right(2);
                secondString = timeCodeString.left(5).right(2);

                // Display time-code (without frame number)
                ui->positionInfoLabel->setText("0" + hourString + ":" + minuteString + ":"
                                               + secondString);
            }
        } else {
            // Stopped
            ui->statusInfoLabel->setText("Stopped");
            ui->discTypeInfoLabel->setText("N/A");
            ui->positionInfoLabel->setText("N/A");

            if (!allControlsLocked) {
                ui->playerControlsGroupBox->setEnabled(true);
                ui->cavControlsGroupBox->setEnabled(false);
                ui->clvControlsGroupBox->setEnabled(false);
                ui->scanBackwardsPushButton->setEnabled(false);
                ui->scanForwardsPushButton->setEnabled(false);
                ui->stepForwardsPushButton->setEnabled(false);
                ui->stepBackwardsPushButton->setEnabled(false);
                ui->lockControlsCheckBox->setEnabled(true);
            } else {
                ui->playerControlsGroupBox->setEnabled(false);
                ui->cavControlsGroupBox->setEnabled(false);
                ui->clvControlsGroupBox->setEnabled(false);
                ui->scanBackwardsPushButton->setEnabled(false);
                ui->scanForwardsPushButton->setEnabled(false);
                ui->stepForwardsPushButton->setEnabled(false);
                ui->stepBackwardsPushButton->setEnabled(false);
                ui->lockControlsCheckBox->setEnabled(false);
            }

        }

    } else {
        // No player connected
        ui->discTypeInfoLabel->setText("N/A");
        ui->positionInfoLabel->setText("N/A");
        ui->statusInfoLabel->setText("No player connected");

        ui->playerControlsGroupBox->setEnabled(false);
        ui->cavControlsGroupBox->setEnabled(false);
        ui->clvControlsGroupBox->setEnabled(false);
        ui->scanBackwardsPushButton->setEnabled(false);
        ui->scanForwardsPushButton->setEnabled(false);
        ui->stepForwardsPushButton->setEnabled(false);
        ui->stepBackwardsPushButton->setEnabled(false);
        ui->lockControlsCheckBox->setEnabled(false);
    }
}

// Lock all controls
void playerControlDialog::lockAllPlayerControls(void)
{
    allControlsLocked = true;
}

// Unlock all controls
void playerControlDialog::unlockAllPlayerControls(void)
{
    allControlsLocked = false;
}

// Play button has been clicked
void playerControlDialog::on_playPushButton_clicked()
{
    emit playerControlEvent(event_playClicked, 0);
}

// Stop button has been clicked
void playerControlDialog::on_stopPushButton_clicked()
{
    emit playerControlEvent(event_stopClicked, 0);
}

// Step forwards button has been clicked
void playerControlDialog::on_stepForwardsPushButton_clicked()
{
    emit playerControlEvent(event_stepForwardsClicked, 0);
}

// Step backwards button has been clicked
void playerControlDialog::on_stepBackwardsPushButton_clicked()
{
    emit playerControlEvent(event_stepBackwardsClicked, 0);
}

// Scan forwards button has been clicked
void playerControlDialog::on_scanForwardsPushButton_clicked()
{
    emit playerControlEvent(event_scanForwardsClicked, 0);
}

// Scan backwards button has been clicked
void playerControlDialog::on_scanBackwardsPushButton_clicked()
{
    emit playerControlEvent(event_scanBackwardsClicked, 0);
}

// Lock physical controls check box has been toggled
void playerControlDialog::on_lockControlsCheckBox_toggled(bool checked)
{
    if (checked) emit playerControlEvent(event_keyLockOnClicked, 0);
    else emit playerControlEvent(event_keyLockOffClicked, 0);
}

// User hit return in CAV seek position entry field
void playerControlDialog::on_seekPositionLineEdit_returnPressed()
{
    on_seekPushButton_clicked();
}

// CAV Seek push button clicked
void playerControlDialog::on_seekPushButton_clicked()
{
    // Get the seek position
    quint32 start = ui->seekPositionLineEdit->text().toUInt();
    emit playerControlEvent(event_seekClicked, start);
}

// CLV time seek button clicked
void playerControlDialog::on_timeSeekPushButton_clicked()
{
    // Create a formatted 7-character string of the time-code
    QString timeCode;
    timeCode.sprintf("%01d%02d%02d00", ui->timeSeektimeEdit->time().hour(),
                     ui->timeSeektimeEdit->time().minute(),
                     ui->timeSeektimeEdit->time().second());

    // Convert the time-code to an integer and signal the event
    emit playerControlEvent(event_seekClicked, timeCode.toUInt());
}


