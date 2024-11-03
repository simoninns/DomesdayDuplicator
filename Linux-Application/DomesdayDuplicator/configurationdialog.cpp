/************************************************************************

    configurationdialog.cpp

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

#include "configurationdialog.h"
#include "ui_configurationdialog.h"

ConfigurationDialog::ConfigurationDialog(QWidget *parent) :
    QDialog(parent)
{
    ui.reset(new Ui::ConfigurationDialog());
    ui->setupUi(this);

    // Build the captureFormatComboBox
    ui->captureFormatComboBox->clear();
    ui->captureFormatComboBox->addItem("16-bit Signed Scaled", Configuration::CaptureFormat::sixteenBitSigned);
    ui->captureFormatComboBox->addItem("10-bit Packed Unsigned", Configuration::CaptureFormat::tenBitPacked);
    ui->captureFormatComboBox->addItem("10-bit Packed Unsigned (4:1 decimation for CD)", Configuration::CaptureFormat::tenBitCdPacked);

    // Build the diskBufferQueueSizeComboBox
    ui->diskBufferQueueSizeComboBox->clear();
    ui->diskBufferQueueSizeComboBox->addItem("64MB", 64 * 1024 * 1024);
    ui->diskBufferQueueSizeComboBox->addItem("128MB", 128 * 1024 * 1024);
    ui->diskBufferQueueSizeComboBox->addItem("256MB", 256 * 1024 * 1024);
    ui->diskBufferQueueSizeComboBox->addItem("512MB", 512 * 1024 * 1024);

    // Build the serialSpeedComboBox
    ui->serialSpeedComboBox->clear();
    ui->serialSpeedComboBox->addItem("Auto", Configuration::SerialSpeeds::autoDetect);
    ui->serialSpeedComboBox->addItem("9600", Configuration::SerialSpeeds::bps9600);
    ui->serialSpeedComboBox->addItem("4800", Configuration::SerialSpeeds::bps4800);
    ui->serialSpeedComboBox->addItem("2400", Configuration::SerialSpeeds::bps2400);
    ui->serialSpeedComboBox->addItem("1200", Configuration::SerialSpeeds::bps1200);

    // If we're running on Linux, disable Windows-specific options.
#ifndef _WIN32
    ui->useWinUsb->setChecked(false);
    ui->useWinUsb->setEnabled(false);
    ui->useAsyncFileIo->setChecked(false);
    ui->useAsyncFileIo->setEnabled(false);
#endif
}

ConfigurationDialog::~ConfigurationDialog()
{
}

void ConfigurationDialog::updateDeviceList(const std::vector<std::string>& deviceList)
{
    // Build the captureFormatComboBox
    ui->preferredDeviceComboBox->clear();
    for (const auto& devicePath : deviceList)
    {
        ui->preferredDeviceComboBox->addItem(devicePath.c_str(), devicePath.c_str());
    }
}

// Load the configuration settings into the UI widgets
void ConfigurationDialog::loadConfiguration(const Configuration& configuration)
{
    // Read the configuration and set up the widgets

    // Capture
    ui->captureDirectoryLineEdit->setText(configuration.getCaptureDirectory());
    ui->captureFormatComboBox->setCurrentIndex(ui->captureFormatComboBox->findData(static_cast<unsigned int>(configuration.getCaptureFormat())));

    // USB
    ui->vendorIdLineEdit->setText(QString::number(configuration.getUsbVid()));
    ui->productIdLineEdit->setText(QString::number(configuration.getUsbPid()));
    ui->preferredDeviceComboBox->setCurrentText(configuration.getUsbPreferredDevice());
    ui->diskBufferQueueSizeComboBox->setCurrentIndex(ui->diskBufferQueueSizeComboBox->findData((qulonglong)configuration.getDiskBufferQueueSize()));
    ui->useSmallUsbTransferQueue->setChecked(configuration.getUseSmallUsbTransferQueue());
    ui->useSmallUsbTransfers->setChecked(configuration.getUseSmallUsbTransfers());
#ifdef _WIN32
    ui->useWinUsb->setChecked(configuration.getUseWinUsb());
    ui->useAsyncFileIo->setChecked(configuration.getUseAsyncFileIo());
#endif

    // Player Integration

    // Build the serialDeviceComboBox
    ui->serialDeviceComboBox->clear();
    const auto infos = QSerialPortInfo::availablePorts();

    // Add additional "None" option to allow de-selection of COM port
    ui->serialDeviceComboBox->addItem(QString(tr("None")), QString(tr("None")));

    bool configuredSerialDevicePresent = false;
    for (const QSerialPortInfo &info : infos) {
        ui->serialDeviceComboBox->addItem(info.portName(), info.portName());

        // Is this the currently configured serial device?
        if (info.portName() == configuration.getSerialDevice())
                configuredSerialDevicePresent = true;
    }

    // Select the currently configured device (or default to 'none' if the device is not set)
    if (!configuredSerialDevicePresent) {
        // No device is present in the configuration or the configured device is no longer available - set to none
        ui->serialDeviceComboBox->setCurrentIndex(0);
    } else {
        // Set to the configured device
        int index = ui->serialDeviceComboBox->findData(configuration.getSerialDevice());
        ui->serialDeviceComboBox->setCurrentIndex(index);
    }

    // Select the currently configured serial speed
    ui->serialSpeedComboBox->setCurrentIndex(ui->serialSpeedComboBox->findData(static_cast<unsigned int>(configuration.getSerialSpeed())));

    // Keylock flag
    ui->keyLockCheckBox->setChecked(configuration.getKeyLock());

    // Advanced naming
    ui->perSideNotesCheckBox->setChecked(configuration.getPerSideNotesEnabled());
    ui->perSideMintCheckBox->setChecked(configuration.getPerSideMintEnabled());

    // Amplitude
    ui->amplitudeLabelCheckBox->setChecked(configuration.getAmplitudeLabelEnabled());
    ui->amplitudeChartCheckBox->setChecked(configuration.getAmplitudeChartEnabled());
}

// Save the configuration settings from the UI widgets
void ConfigurationDialog::saveConfiguration(Configuration& configuration)
{
    qDebug() << "ConfigurationDialog::saveConfiguration(): Saving configuration";

    // Capture
    configuration.setCaptureDirectory(ui->captureDirectoryLineEdit->text());
    configuration.setCaptureFormat(static_cast<Configuration::CaptureFormat>(ui->captureFormatComboBox->itemData(ui->captureFormatComboBox->currentIndex()).toInt()));

    // USB
    configuration.setUsbVid(static_cast<quint16>(ui->vendorIdLineEdit->text().toInt()));
    configuration.setUsbPid(static_cast<quint16>(ui->productIdLineEdit->text().toInt()));
    configuration.setUsbPreferredDevice(ui->preferredDeviceComboBox->currentText());
    configuration.setDiskBufferQueueSize((size_t)ui->diskBufferQueueSizeComboBox->itemData(ui->diskBufferQueueSizeComboBox->currentIndex()).toULongLong());
    configuration.setUseSmallUsbTransferQueue(ui->useSmallUsbTransferQueue->isChecked());
    configuration.setUseSmallUsbTransfers(ui->useSmallUsbTransfers->isChecked());
    configuration.setUseWinUsb(ui->useWinUsb->isChecked());
    configuration.setUseAsyncFileIo(ui->useAsyncFileIo->isChecked());

    // Player integration - serial device
    configuration.setSerialDevice(ui->serialDeviceComboBox->currentText());

    // Player integration - Serial speed
    configuration.setSerialSpeed(static_cast<Configuration::SerialSpeeds>(ui->serialSpeedComboBox->itemData(ui->serialSpeedComboBox->currentIndex()).toInt()));

    // KeyLock
    configuration.setKeyLock(ui->keyLockCheckBox->isChecked());

    // Advanced naming
    configuration.setPerSideNotesEnabled(ui->perSideNotesCheckBox->isChecked());
    configuration.setPerSideMintEnabled(ui->perSideMintCheckBox->isChecked());

    // Amplitude
    configuration.setAmplitudeLabelEnabled(ui->amplitudeLabelCheckBox->isChecked());
    configuration.setAmplitudeChartEnabled(ui->amplitudeChartCheckBox->isChecked());

    // Save the configuration to disk
    configuration.writeConfiguration();
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

// Any button clicked
void ConfigurationDialog::on_buttonBox_clicked(QAbstractButton *button)
{
    // Check for restore defaults button
    if (button == ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)) {
        qDebug() << "ConfigurationDialog::on_buttonBox_clicked(): Restore defaults clicked";

        Configuration defaultConfig(this);
        defaultConfig.setDefault();
        loadConfiguration(defaultConfig);
    }
}
