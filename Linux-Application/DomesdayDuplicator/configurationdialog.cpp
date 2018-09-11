/************************************************************************

    configurationdialog.cpp

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

#include "configurationdialog.h"
#include "ui_configurationdialog.h"

ConfigurationDialog::ConfigurationDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigurationDialog)
{
    ui->setupUi(this);
}

ConfigurationDialog::~ConfigurationDialog()
{
    delete ui;
}

// Load the configuration settings into the UI widgets
void ConfigurationDialog::loadConfiguration(Configuration *configuration)
{
    // Read the configuration and set up the widgets

    // Capture
    ui->captureDirectoryLineEdit->setText(configuration->getCaptureDirectory());
    ui->vendorIdLineEdit->setText(QString::number(configuration->getUsbVid()));

    // USB
    ui->productIdLineEdit->setText(QString::number(configuration->getUsbPid()));

    // Player Integration
    // To-do
}

// Save the configuration settings from the UI widgets
void ConfigurationDialog::saveConfiguration(Configuration *configuration)
{
    qDebug() << "ConfigurationDialog::saveConfiguration(): Saving configuration";

    // Capture
    configuration->setCaptureDirectory(ui->captureDirectoryLineEdit->text());

    // USB
    configuration->setUsbVid(static_cast<quint16>(ui->vendorIdLineEdit->text().toInt()));
    configuration->setUsbPid(static_cast<quint16>(ui->productIdLineEdit->text().toInt()));

    // Player integration
    // To-do

    // Save the configuration to disk
    configuration->writeConfiguration();
}

// Browse for capture directory button clicked
void ConfigurationDialog::on_captureDirectoryPushButton_clicked()
{
    QString captureDirectoryPath;

    captureDirectoryPath = QFileDialog::getExistingDirectory(this, tr("Select capture directory"), ui->captureDirectoryLineEdit->text());

    if (captureDirectoryPath.isEmpty()) {
        qDebug() << "ConfigurationDialog::on_captureDirectoryPushButton_clicked(): QFileDialog::getExistingDirectory returned empty directory path";
    } else {
        ui->captureDirectoryLineEdit->setText(captureDirectoryPath);
    }
}

// Save configuration clicked
void ConfigurationDialog::on_buttonBox_accepted()
{
    qDebug() << "ConfigurationDialog::on_buttonBox_accepted(): Configuration changed";

    // Emit a configuration changed signal
    emit configurationChanged();
}

// Cancel configuration clicked
void ConfigurationDialog::on_buttonBox_rejected()
{
    qDebug() << "ConfigurationDialog::on_buttonBox_rejected(): Ignoring configuration changes";
}
