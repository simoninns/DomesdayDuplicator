/************************************************************************

    playercontroldialog.cpp

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
    ui->startPositionLineEdit->setValidator( new QIntValidator(0, 60000, this) );
    ui->endPositionLineEdit->setValidator( new QIntValidator(0, 60000, this) );

    // Default the line edit fields
    ui->startPositionLineEdit->setText("0");
    ui->endPositionLineEdit->setText("0");
}

playerControlDialog::~playerControlDialog()
{
    delete ui;
}

// Update the player information
void playerControlDialog::updatePlayerControlInfo(bool isConnected, bool isCav, quint32 frameNumber,
                                                  quint32 timeCode, bool isPlaying)
{
    // Is a player connected?
    if (isConnected) {
        if (isPlaying) {
            // Playing
            ui->statusInfoLabel->setText("Playing");

            if (isCav) {
                // CAV disc
                ui->discTypeInfoLabel->setText("CAV");

                // Convert frame number and display
                QString frameNumberString;
                frameNumberString.sprintf("%05d", frameNumber);
                ui->positionInfoLabel->setText(frameNumberString);
            } else {
                // CLV disc
                ui->discTypeInfoLabel->setText("CLV");

                // Convert time-code and display
                QString timeCodeString;
                QString hourString;
                QString minuteString;
                QString secondString;
                //QString frameString;

                // Get the full 7 character time-code string
                timeCodeString.sprintf("%07d", timeCode);

                hourString = timeCodeString.left(1);
                minuteString = timeCodeString.left(3).right(2);
                secondString = timeCodeString.left(5).right(2);
                //frameString = timeCodeString.left(7).right(2);
                //ui->positionInfoLabel->setText("0" + hourString + ":" + minuteString + ":"
                //                               + secondString + ":" + frameString);

                // Display time-code (without frame number)
                ui->positionInfoLabel->setText("0" + hourString + ":" + minuteString + ":"
                                               + secondString);
            }
        } else {
            // Stopped
            ui->statusInfoLabel->setText("Stopped");
            ui->discTypeInfoLabel->setText("N/A");
            ui->positionInfoLabel->setText("N/A");
        }

    } else {
        // No player connected
        ui->discTypeInfoLabel->setText("N/A");
        ui->positionInfoLabel->setText("N/A");
        ui->statusInfoLabel->setText("No player connected");
    }
}

// Play button has been clicked
void playerControlDialog::on_playPushButton_clicked()
{
    emit playerControlEvent(event_playClicked, 0, 0);
}

// Stop button has been clicked
void playerControlDialog::on_stopPushButton_clicked()
{
    emit playerControlEvent(event_stopClicked, 0, 0);
}

// Step forwards button has been clicked
void playerControlDialog::on_stepForwardsPushButton_clicked()
{
    emit playerControlEvent(event_stepForwardsClicked, 0, 0);
}

// Step backwards button has been clicked
void playerControlDialog::on_stepBackwardsPushButton_clicked()
{
    emit playerControlEvent(event_stepBackwardsClicked, 0, 0);
}

// Scan forwards button has been clicked
void playerControlDialog::on_scanForwardsPushButton_clicked()
{
    emit playerControlEvent(event_scanForwardsClicked, 0, 0);
}

// Scan backwards button has been clicked
void playerControlDialog::on_scanBackwardsPushButton_clicked()
{
    emit playerControlEvent(event_scanBackwardsClicked, 0, 0);
}

// Lock physical controls check box has been toggled
void playerControlDialog::on_lockControlsCheckBox_toggled(bool checked)
{
    if (checked) emit playerControlEvent(event_keyLockOnClicked, 0, 0);
    else emit playerControlEvent(event_keyLockOffClicked, 0, 0);
}

// Go to button pressed
void playerControlDialog::on_goToPushButton_clicked()
{
    // Get the start position
    quint32 start = ui->startPositionLineEdit->text().toUInt();
    emit playerControlEvent(event_gotoClicked, start, 0);
}

// User hit return in go to position entry field
void playerControlDialog::on_startPositionLineEdit_returnPressed()
{
    on_goToPushButton_clicked();
}

// Capture to button pressed
void playerControlDialog::on_captureToPushButton_clicked()
{
    // Get the start and end positions
    quint32 start = ui->startPositionLineEdit->text().toUInt();
    quint32 end = ui->endPositionLineEdit->text().toUInt();

    // Only emit the command if the end is beyond the start
    if (start < end) emit playerControlEvent(event_captureToClicked, start, end);
}

// User hit return in capture to position entry field
void playerControlDialog::on_endPositionLineEdit_returnPressed()
{
    on_captureToPushButton_clicked();
}
