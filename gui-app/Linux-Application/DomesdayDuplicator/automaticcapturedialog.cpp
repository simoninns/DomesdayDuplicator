/************************************************************************

    automaticcapturedialog.cpp

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

#include "automaticcapturedialog.h"
#include "ui_automaticcapturedialog.h"

AutomaticCaptureDialog::AutomaticCaptureDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AutomaticCaptureDialog)
{
    ui->setupUi(this);

    // Set up the UI
    ui->wholeDiscRadioButton->setChecked(true);
    ui->partialDiscRadioButton->setChecked(false);
    ui->leadInCaptureRadioButton->setChecked(false);

    ui->captureStatusLabel->setText("Idle");
    ui->startFrameLineEdit->setText("0");
    ui->endFrameLineEdit->setText("0");

    QTime defaultTime;
    defaultTime.setHMS(0, 0, 0, 0);
    ui->startTimeTimeEdit->setTime(defaultTime);
    ui->endTimeTimeEdit->setTime(defaultTime);

    ui->startFrameLineEdit->setEnabled(false);
    ui->endFrameLineEdit->setEnabled(false);
    ui->startTimeTimeEdit->setEnabled(false);
    ui->endTimeTimeEdit->setEnabled(false);

    ui->startCavCapturePushButton->setEnabled(true);
    ui->startClvCapturePushButton->setEnabled(true);

    // Default state variables
    captureInProgress = false;
}

AutomaticCaptureDialog::~AutomaticCaptureDialog()
{
    delete ui;
}

void AutomaticCaptureDialog::captureComplete()
{
    ui->wholeDiscRadioButton->setEnabled(true);
    ui->partialDiscRadioButton->setEnabled(true);
    ui->leadInCaptureRadioButton->setEnabled(true);

    ui->captureStatusLabel->setText("Idle");
    ui->startFrameLineEdit->setText("0");
    ui->endFrameLineEdit->setText("0");

    QTime defaultTime;
    defaultTime.setHMS(0, 0, 0, 0);
    ui->startTimeTimeEdit->setTime(defaultTime);
    ui->endTimeTimeEdit->setTime(defaultTime);

    ui->startCavCapturePushButton->setEnabled(true);
    ui->startClvCapturePushButton->setEnabled(true);

    // Call the clicked action to set up the fields correctly
    if (ui->wholeDiscRadioButton->isChecked()) on_wholeDiscRadioButton_clicked();
    if (ui->partialDiscRadioButton->isChecked()) on_partialDiscRadioButton_clicked();
    if (ui->leadInCaptureRadioButton->isChecked()) on_leadInCaptureRadioButton_clicked();
}

// Private methods ----------------------------------------------------------------------------------------------------

void AutomaticCaptureDialog::on_wholeDiscRadioButton_clicked()
{
    ui->startFrameLineEdit->setEnabled(false);
    ui->endFrameLineEdit->setEnabled(false);
    ui->startTimeTimeEdit->setEnabled(false);
    ui->endTimeTimeEdit->setEnabled(false);
}

void AutomaticCaptureDialog::on_partialDiscRadioButton_clicked()
{
    ui->startFrameLineEdit->setEnabled(true);
    ui->endFrameLineEdit->setEnabled(true);
    ui->startTimeTimeEdit->setEnabled(true);
    ui->endTimeTimeEdit->setEnabled(true);
}

void AutomaticCaptureDialog::on_leadInCaptureRadioButton_clicked()
{
    ui->startFrameLineEdit->setEnabled(false);
    ui->endFrameLineEdit->setEnabled(true);
    ui->startTimeTimeEdit->setEnabled(false);
    ui->endTimeTimeEdit->setEnabled(true);
}

void AutomaticCaptureDialog::on_startCavCapturePushButton_clicked()
{
    if (!captureInProgress) {
        ui->wholeDiscRadioButton->setEnabled(false);
        ui->partialDiscRadioButton->setEnabled(false);
        ui->leadInCaptureRadioButton->setEnabled(false);

        ui->startFrameLineEdit->setEnabled(false);
        ui->endFrameLineEdit->setEnabled(false);
        ui->startTimeTimeEdit->setEnabled(false);
        ui->endTimeTimeEdit->setEnabled(false);

        ui->startClvCapturePushButton->setEnabled(false);
        ui->startCavCapturePushButton->setText("Abort");
        emit startAutomaticCapture(0, 0, 0, 0);
    } else {
        // Abort
        emit stopAutomaticCapture();
    }
}

void AutomaticCaptureDialog::on_startClvCapturePushButton_clicked()
{
    if (!captureInProgress) {
        ui->wholeDiscRadioButton->setEnabled(false);
        ui->partialDiscRadioButton->setEnabled(false);
        ui->leadInCaptureRadioButton->setEnabled(false);

        ui->startFrameLineEdit->setEnabled(false);
        ui->endFrameLineEdit->setEnabled(false);
        ui->startTimeTimeEdit->setEnabled(false);
        ui->endTimeTimeEdit->setEnabled(false);

        ui->startCavCapturePushButton->setEnabled(false);
        ui->startClvCapturePushButton->setText("Abort");
        emit startAutomaticCapture(0, 0, 0, 0);
    } else {
        emit stopAutomaticCapture();
    }
}
