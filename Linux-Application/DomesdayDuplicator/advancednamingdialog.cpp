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

QString mintSide1;
QString mintSide2;
QString notesSide1;
QString notesSide2;

AdvancedNamingDialog::AdvancedNamingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AdvancedNamingDialog)
{
    ui->setupUi(this);

    // Set default state for all widgets
    ui->discTitleCheckBox->setChecked(false);
    ui->discTypeCheckBox->setChecked(false);
    ui->formatCheckBox->setChecked(false);
    ui->audioCheckBox->setChecked(false);
    ui->discSideCheckBox->setChecked(false);
    ui->notesCheckBox->setChecked(false);
    ui->mintCheckBox->setChecked(false);
    ui->durationCheckBox->setChecked(false);

    // Set line edit validation to A-Z, a-z, 0-9 and space, minus and underscore
    ui->discTitleLineEdit->setValidator(new QRegularExpressionValidator(
                                            QRegularExpression("^[a-zA-Z0-9_-]+( [a-zA-Z0-9_-]+)*$"), this ));
    ui->notesLineEdit->setValidator(new QRegularExpressionValidator(
                                        QRegularExpression("^[a-zA-Z0-9_-]+( [a-zA-Z0-9_-]+)*$"), this ));
    ui->mintLineEdit->setValidator(new QRegularExpressionValidator(
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

        if (ui->audioCheckBox->isChecked()) {
            if (ui->audioAnalogueRadioButton->isChecked()) fileName += "_ANA";
            if (ui->audioAc3RadioButton->isChecked()) fileName += "_AC3";
            if (ui->audioDtsRadioButton->isChecked()) fileName += "_DTS";
        }

        if ((mintSide1.isNull() and notesSide1.isNull()) or (ui->discSideSpinBox->value() > 2)) {

            if (ui->discSideCheckBox->isChecked()) {
                fileName += QString("_side%1").arg(ui->discSideSpinBox->value());
            }

            if (ui->notesCheckBox->isChecked()) {
                fileName += "_" + ui->notesLineEdit->text();
            }

            if (ui->mintCheckBox->isChecked()) {
                fileName += "_" + ui->mintLineEdit->text();
            }

        } else {

            mintSide2 = ui->mintLineEdit->text();
            notesSide2 = ui->notesLineEdit->text();

            fileName += QString(("_side1")) + "_" + notesSide1 + "_" + mintSide1;
            fileName += QString(("_side2")) + "_" + notesSide2 + "_" + mintSide2;
        }

        // Add the date/time stamp
        fileName += "_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    }

    return fileName;
}

// Function to return if duration checkbox is set
bool AdvancedNamingDialog::getDurationChecked()
{
    bool fileDurationBox = false;

    if (ui->durationCheckBox->isChecked()) {
        fileDurationBox = true;
    }

    return fileDurationBox;
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

    if (ui->audioCheckBox->isChecked()) {
        ui->audioDefaultRadioButton->setEnabled(true);
        ui->audioAnalogueRadioButton->setEnabled(true);
        ui->audioAc3RadioButton->setEnabled(true);
        ui->audioDtsRadioButton->setEnabled(true);
    } else {
        ui->audioDefaultRadioButton->setEnabled(false);
        ui->audioAnalogueRadioButton->setEnabled(false);
        ui->audioAc3RadioButton->setEnabled(false);
        ui->audioDtsRadioButton->setEnabled(false);
    }

    if (ui->discSideCheckBox->isChecked()) {
        ui->discSideSpinBox->setEnabled(true);
    } else {
        ui->discSideSpinBox->setEnabled(false);
    }

    if (ui->notesCheckBox->isChecked()) {
        ui->notesLineEdit->setEnabled(true);
    } else {
        ui->notesLineEdit->setEnabled(false);
    }

    if (ui->mintCheckBox->isChecked()) {
        ui->mintLineEdit->setEnabled(true);
    } else {
        ui->mintLineEdit->setEnabled(false);
    }

}

// Update the GUI and hold values from previous side input
void AdvancedNamingDialog::updateSideHoldings(void)
{
    if ((ui->discSideSpinBox->value()) == 2 ) {

        if (mintSide1.length() == 0){
        mintSide1 = ui->mintLineEdit->text();
        }
        if (notesSide1.length() == 0) {
        notesSide1 = ui->notesLineEdit->text();
        }

        ui->mintLineEdit->setText(mintSide2);
        ui->notesLineEdit->setText(notesSide2);
    }

    if ((ui->discSideSpinBox->value()) == 1) {

        if (mintSide2.length() == 0){
        mintSide2 = ui->mintLineEdit->text();
        }
        if (notesSide2.length() == 0){
        notesSide2 = ui->notesLineEdit->text();
        }

        ui->mintLineEdit->setText(mintSide1);
        ui->notesLineEdit->setText(notesSide1);
    }

    if ((ui->discSideSpinBox->value()) == 3) {

        if (mintSide2.length() == 0){
        mintSide2 = ui->mintLineEdit->text();
        }
        if (notesSide2.length() == 0){
        notesSide2 = ui->notesLineEdit->text();
        }

        ui->mintLineEdit->setText("");
        ui->notesLineEdit->setText("");
    }

    if ((ui->discSideSpinBox->value() != 1 and ui->discSideSpinBox->value() != 2 and ui->discSideSpinBox->value() != 3 )) {

        ui->mintLineEdit->setText("");
        ui->notesLineEdit->setText("");
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

void AdvancedNamingDialog::on_audioCheckBox_clicked()
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

void AdvancedNamingDialog::on_mintCheckBox_clicked()
{
    updateGui();
}

void AdvancedNamingDialog::on_durationCheckBox_clicked()
{
    updateGui();

}

void AdvancedNamingDialog::on_discSideSpinBox_valueChanged()
{
    updateSideHoldings();

}
