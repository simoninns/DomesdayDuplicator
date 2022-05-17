/************************************************************************

    advancednamingdialog.cpp

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

#include "advancednamingdialog.h"
#include "ui_advancednamingdialog.h"

AdvancedNamingDialog::AdvancedNamingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AdvancedNamingDialog)
{
    ui->setupUi(this);

    // Set default state for all widgets
    ui->discTitleCheckBox->setChecked(false);
    ui->discTypeCheckBox->setChecked(false);
    ui->formatCheckBox->setChecked(false);
    ui->discSideCheckBox->setChecked(false);
    ui->notesCheckBox->setChecked(false);

    // Set line edit validation to A-Z, a-z, 0-9 and space, minus and underscore
    ui->discTitleLineEdit->setValidator(new QRegularExpressionValidator(
                                            QRegularExpression("^[a-zA-Z0-9_-]+( [a-zA-Z0-9_-]+)*$"), this ));
    ui->notesLineEdit->setValidator(new QRegularExpressionValidator(
                                        QRegularExpression("^[a-zA-Z0-9_-]+( [a-zA-Z0-9_-]+)*$"), this ));

    updateGui();
}

AdvancedNamingDialog::~AdvancedNamingDialog()
{
    delete ui;
}

// Function to return the current file name based on the
// file name settings and the date/time stamp
QString AdvancedNamingDialog::getFileName(bool isTestData)
{
    QString fileName;

    if (isTestData) {
        // For test data we ignore the disc details (as test data never has any)
        fileName = "/TestData_";
        fileName += QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    } else {
        // Construct the file name according to the enabled options
        if (ui->discTitleCheckBox->isChecked() && !ui->discTitleLineEdit->text().isEmpty())
            fileName += "/" + ui->discTitleLineEdit->text();
        else fileName += "/RF-Sample";

        if (ui->discTypeCheckBox->isChecked()) {
            if (ui->typeCavRadioButton->isChecked()) fileName += "_CAV";
            if (ui->typeClvRadioButton->isChecked()) fileName += "_CLV";
        }

        if (ui->formatCheckBox->isChecked()) {
            if (ui->formatNtscRadioButton->isChecked()) fileName += "_NTSC";
            if (ui->formatPalRadioButton->isChecked()) fileName += "_PAL";
        }

        if (ui->discSideCheckBox->isChecked()) {
            if (ui->sideOneRadioButton->isChecked()) fileName += "_side1";
            if (ui->sideTwoRadioButton->isChecked()) fileName += "_side2";
            if (ui->sideThreeRadioButton->isChecked()) fileName += "_side3";
            if (ui->sideFourRadioButton->isChecked()) fileName += "_side4";
        }

        if (ui->notesCheckBox->isChecked()) {
            fileName += "_" + ui->notesLineEdit->text();
        }

        // Add the date/time stamp
        fileName += "_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    }

    return fileName;
}

// Update the GUI based on the state of the check boxes
void AdvancedNamingDialog::updateGui(void)
{
    if (ui->discTitleCheckBox->isChecked()) {
        ui->discTitleLineEdit->setEnabled(true);
    } else {
        ui->discTitleLineEdit->setEnabled(false);
    }

    if (ui->discTypeCheckBox->isChecked()) {
        ui->typeCavRadioButton->setEnabled(true);
        ui->typeClvRadioButton->setEnabled(true);
    } else {
        ui->typeCavRadioButton->setEnabled(false);
        ui->typeClvRadioButton->setEnabled(false);
    }

    if (ui->formatCheckBox->isChecked()) {
        ui->formatNtscRadioButton->setEnabled(true);
        ui->formatPalRadioButton->setEnabled(true);
    } else {
        ui->formatNtscRadioButton->setEnabled(false);
        ui->formatPalRadioButton->setEnabled(false);
    }

    if (ui->discSideCheckBox->isChecked()) {
        ui->sideOneRadioButton->setEnabled(true);
        ui->sideTwoRadioButton->setEnabled(true);
        ui->sideThreeRadioButton->setEnabled(true);
        ui->sideFourRadioButton->setEnabled(true);
    } else {
        ui->sideOneRadioButton->setEnabled(false);
        ui->sideTwoRadioButton->setEnabled(false);
        ui->sideThreeRadioButton->setEnabled(false);
        ui->sideFourRadioButton->setEnabled(false);
    }

    if (ui->notesCheckBox->isChecked()) {
        ui->notesLineEdit->setEnabled(true);
    } else {
        ui->notesLineEdit->setEnabled(false);
    }
}

void AdvancedNamingDialog::on_discTitleCheckBox_clicked()
{
    updateGui();
}

void AdvancedNamingDialog::on_discTypeCheckBox_clicked()
{
    updateGui();
}

void AdvancedNamingDialog::on_formatCheckBox_clicked()
{
    updateGui();
}

void AdvancedNamingDialog::on_discSideCheckBox_clicked()
{
    updateGui();
}

void AdvancedNamingDialog::on_notesCheckBox_clicked()
{
    updateGui();
}
