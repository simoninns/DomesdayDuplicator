/************************************************************************

    playerremotedialog.cpp

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

#include "playerremotedialog.h"
#include "ui_playerremotedialog.h"

PlayerRemoteDialog::PlayerRemoteDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PlayerRemoteDialog)
{
    ui->setupUi(this);

    positionMode = PositionMode::pmFrame;
    multiSpeed = MultiSpeed::multiX1;
    displayMode = DisplayMode::displayOff;

    updateGui();
}

PlayerRemoteDialog::~PlayerRemoteDialog()
{
    delete ui;
}

// Update the GUI
void PlayerRemoteDialog::updateGui(void)
{
    switch(positionMode) {
    case PositionMode::pmChapter:
        ui->unitLabel->setText(tr("Chapter"));
        break;
    case PositionMode::pmFrame:
        ui->unitLabel->setText(tr("Frame"));
        break;
    case PositionMode::pmTime:
        ui->unitLabel->setText(tr("Time"));
        break;
    case PositionMode::pmTrack:
        ui->unitLabel->setText(tr("Track"));
        break;
    }

    switch(multiSpeed) {
    case MultiSpeed::multiSm16:
        ui->speedLabel->setText(tr("x1/6"));
        break;
    case MultiSpeed::multiSm14:
        ui->speedLabel->setText(tr("x1/4"));
        break;
    case MultiSpeed::multiSm13:
        ui->speedLabel->setText(tr("x1/3"));
        break;
    case MultiSpeed::multiSm12:
        ui->speedLabel->setText(tr("x1/2"));
        break;
    case MultiSpeed::multiX1:
        ui->speedLabel->setText(tr("x1"));
        break;
    case MultiSpeed::multiX2:
        ui->speedLabel->setText(tr("x2"));
        break;
    case MultiSpeed::multiX3:
        ui->speedLabel->setText(tr("x3"));
        break;
    case MultiSpeed::multiX4:
        ui->speedLabel->setText(tr("x4"));
        break;
    }

    switch(displayMode) {
    case DisplayMode::displayOff:
        ui->displayLabel->setText(tr("OSD Off"));
        break;
    case DisplayMode::displayOn:
        ui->displayLabel->setText(tr("OSD On"));
        break;
    }
}

void PlayerRemoteDialog::setMultiSpeed(MultiSpeed multiSpeedParam)
{
    multiSpeed = multiSpeedParam;
    updateGui();
}

void PlayerRemoteDialog::setDisplayMode(DisplayMode displayModeParam)
{
    displayMode = displayModeParam;
    updateGui();
}

// Handle push button clicked events ----------------------------------------------------------------------------------

void PlayerRemoteDialog::on_rejectPushButton_clicked()
{
    emit remoteControlCommand(RemoteButtons::rbReject);
}

void PlayerRemoteDialog::on_pausePushButton_clicked()
{
    emit remoteControlCommand(RemoteButtons::rbPause);
}

void PlayerRemoteDialog::on_playPushButton_clicked()
{
    emit remoteControlCommand(RemoteButtons::rbPlay);
}

void PlayerRemoteDialog::on_repeatPushButton_clicked()
{
    emit remoteControlCommand(RemoteButtons::rbRepeat);
}

void PlayerRemoteDialog::on_stepRevPushButton_clicked()
{
    emit remoteControlCommand(RemoteButtons::rbStepRev);
}

void PlayerRemoteDialog::on_stepFwdPushButton_clicked()
{
    emit remoteControlCommand(RemoteButtons::rbStepFwd);
}

void PlayerRemoteDialog::on_displayPushButton_clicked()
{
    emit remoteControlCommand(RemoteButtons::rbDisplay);
}

void PlayerRemoteDialog::on_scanRevPushButton_clicked()
{
    emit remoteControlCommand(RemoteButtons::rbScanRev);
}

void PlayerRemoteDialog::on_scanFwdPushButton_clicked()
{
    emit remoteControlCommand(RemoteButtons::rbScanFwd);
}

void PlayerRemoteDialog::on_audioPushButton_clicked()
{
    emit remoteControlCommand(RemoteButtons::rbAudio);
}

void PlayerRemoteDialog::on_speedDownPushButton_clicked()
{
    emit remoteControlCommand(RemoteButtons::rbSpeedDown);
}

void PlayerRemoteDialog::on_speedUpPushButton_clicked()
{
    emit remoteControlCommand(RemoteButtons::rbSpeedUp);
}

void PlayerRemoteDialog::on_clearPushButton_clicked()
{

}

void PlayerRemoteDialog::on_multiRevPushButton_clicked()
{
    emit remoteControlCommand(RemoteButtons::rbMultiRev);
}

void PlayerRemoteDialog::on_multiFwdPushButton_clicked()
{
    emit remoteControlCommand(RemoteButtons::rbMultiFwd);
}

void PlayerRemoteDialog::on_sevenPushButton_clicked()
{

}

void PlayerRemoteDialog::on_eightPushButton_clicked()
{

}

void PlayerRemoteDialog::on_ninePushButton_clicked()
{

}

void PlayerRemoteDialog::on_fourPushButton_clicked()
{

}

void PlayerRemoteDialog::on_fivePushButton_clicked()
{

}

void PlayerRemoteDialog::on_sixPushButton_clicked()
{

}

void PlayerRemoteDialog::on_onePushButton_clicked()
{

}

void PlayerRemoteDialog::on_twoPushButton_clicked()
{

}

void PlayerRemoteDialog::on_threePushButton_clicked()
{

}

void PlayerRemoteDialog::on_zeroPushButton_clicked()
{

}

void PlayerRemoteDialog::on_searchPushButton_clicked()
{

}

void PlayerRemoteDialog::on_chapFramePushButton_clicked()
{
    // Rotate through the possible unit modes
    if (positionMode == PositionMode::pmChapter) positionMode = PositionMode::pmFrame;
    else if (positionMode == PositionMode::pmFrame) positionMode = PositionMode::pmTime;
    else if (positionMode == PositionMode::pmTime) positionMode = PositionMode::pmTrack;
    else if (positionMode == PositionMode::pmTrack) positionMode = PositionMode::pmChapter;
    updateGui();
}
