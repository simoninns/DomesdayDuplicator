/************************************************************************

    serialportselectdialog.cpp

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

#include "serialportselectdialog.h"
#include "ui_serialportselectdialog.h"

serialPortSelectDialog::serialPortSelectDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::serialPortSelectDialog)
{
    ui->setupUi(this);

    // Flag that the serial port is not configured and set defaults
    currentSettings.configured = false;
    currentSettings.name = QString(tr("None"));
    currentSettings.baudRate = 9600;

    // Populate the port information
    fillPortsInfo();
}

serialPortSelectDialog::~serialPortSelectDialog()
{
    delete ui;
}

// Return the current serial port baud rate setting
qint16 serialPortSelectDialog::getBaudRate(void)
{
    return currentSettings.baudRate;
}

// Return the current serial port baud rate setting
QString serialPortSelectDialog::getPortName(void)
{
    return currentSettings.name;
}

// Return the current configuration status
bool serialPortSelectDialog::isConfigured(void)
{
    return currentSettings.configured;
}

// Populate the combo box with the available serial ports
void serialPortSelectDialog::fillPortsInfo()
{
    ui->serialPortSelectComboBox->clear();

    const auto infos = QSerialPortInfo::availablePorts();

    // Add additional "None" option to allow de-selection of COM port
    ui->serialPortSelectComboBox->addItem(QString(tr("None")), QString(tr("None")));

    for (const QSerialPortInfo &info : infos) {
        QStringList list;
        list << info.portName();
        ui->serialPortSelectComboBox->addItem(list.first(), list);
    }

    // Set the current selection to the currently selected serial port
    ui->serialPortSelectComboBox->setCurrentIndex(ui->serialPortSelectComboBox->findData(currentSettings.name));

    // Configure the BPS radio buttons
    if (currentSettings.baudRate == 1200) {
        ui->bps1200RadioButton->setChecked(true);
        ui->bps2400RadioButton->setChecked(false);
        ui->bps9600RadioButton->setChecked(false);
    }

    if (currentSettings.baudRate == 2400) {
        ui->bps1200RadioButton->setChecked(false);
        ui->bps2400RadioButton->setChecked(true);
        ui->bps9600RadioButton->setChecked(false);
    }

    if (currentSettings.baudRate == 9600) {
        ui->bps1200RadioButton->setChecked(false);
        ui->bps2400RadioButton->setChecked(false);
        ui->bps9600RadioButton->setChecked(true);
    }
}

// Update the serial settings
void serialPortSelectDialog::updateSettings()
{
    if (ui->serialPortSelectComboBox->currentText().isEmpty() ||
            ui->serialPortSelectComboBox->currentText() == QString(tr("None"))) {
        // No serial port is selected
        currentSettings.name = ui->serialPortSelectComboBox->currentText();
        currentSettings.configured = false;
        qDebug() << "serialPortSelectDialog::updateSettings(): Serial port not configured";
    } else {
        currentSettings.name = ui->serialPortSelectComboBox->currentText();
        if (ui->bps1200RadioButton->isChecked()) currentSettings.baudRate = 1200;
        if (ui->bps2400RadioButton->isChecked()) currentSettings.baudRate = 2400;
        if (ui->bps9600RadioButton->isChecked()) currentSettings.baudRate = 9600;
        currentSettings.configured = true;
        qDebug() << "serialPortSelectDialog::updateSettings(): Serial port configured";
    }

    // Emit a signal indicating that the serial configuration has changed
    emit serialPortChanged();
}

// Function called when user accepts serial configuration
void serialPortSelectDialog::on_buttonBox_accepted()
{
    updateSettings();
    hide();
}

void serialPortSelectDialog::showEvent(QShowEvent *e)
{
    qDebug() << "serialPortSelectDialog::showEvent(): Event triggered";

    // Update the available serial ports
    fillPortsInfo();
}
