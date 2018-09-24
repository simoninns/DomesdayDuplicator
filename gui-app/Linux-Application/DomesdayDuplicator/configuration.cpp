/************************************************************************

    configuration.cpp

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

#include "configuration.h"

// This define should be incremented if the settings file format changes
#define SETTINGSVERSION 2

Configuration::Configuration(QObject *parent) : QObject(parent)
{
    // Open the application's configuration file
    QString configurationPath ;
    QString configurationFileName;

    configurationPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) ;
    configurationFileName = "config.ini" ;
    configuration = new QSettings(configurationPath + "/"+ configurationFileName, QSettings::IniFormat);

    // Read the configuration
    readConfiguration();

    // Are the configuration settings valid?
    if (settings.version != SETTINGSVERSION) {
        qDebug() << "Configuration::Configuration(): Configuration invalid or wrong version (" <<
                    settings.version << "!= " << SETTINGSVERSION <<").  Setting to default values";

        // Set default configuration
        setDefault();
    }
}

void Configuration::writeConfiguration(void)
{
    // Write the valid configuration flag
    configuration->setValue("version", settings.version);

    // Capture
    configuration->beginGroup("capture");
    configuration->setValue("captureDirectory", settings.capture.captureDirectory);
    configuration->setValue("captureFormat", convertCaptureFormatToInt(settings.capture.captureFormat));
    configuration->endGroup();

    // USB
    configuration->beginGroup("usb");
    configuration->setValue("vid", settings.usb.vid);
    configuration->setValue("pid", settings.usb.pid);
    configuration->endGroup();

    // PIC
    configuration->beginGroup("pic");
    configuration->setValue("serialDevice", settings.pic.serialDevice);
    configuration->setValue("serialSpeed", convertSerialSpeedsToInt(settings.pic.serialSpeed));
    configuration->setValue("playerModel", convertPlayerModelsToInt(settings.pic.playerModel));
    configuration->endGroup();

    // Sync the settings with disk
    configuration->sync();
}

void Configuration::readConfiguration(void)
{
    qDebug() << "Configuration::readConfiguration(): Reading configuration from" << configuration->fileName();

    // Read the valid configuration flag
    settings.version = configuration->value("version").toInt();

    // Capture
    configuration->beginGroup("capture");
    settings.capture.captureDirectory = configuration->value("captureDirectory").toString();
    settings.capture.captureFormat = convertIntToCaptureFormat(configuration->value("captureFormat").toInt());
    configuration->endGroup();

    // USB
    configuration->beginGroup("usb");
    settings.usb.vid = static_cast<quint16>(configuration->value("vid").toUInt());
    settings.usb.pid = static_cast<quint16>(configuration->value("pid").toUInt());
    configuration->endGroup();

    // PIC
    configuration->beginGroup("pic");
    settings.pic.serialDevice = configuration->value("serialDevice").toString();
    settings.pic.serialSpeed = convertIntToSerialSpeeds(configuration->value("serialSpeed").toInt());
    settings.pic.playerModel = convertIntToPlayerModels(configuration->value("playerModel").toInt());
    configuration->endGroup();
}

void Configuration::setDefault(void)
{
    // Set up the default values
    settings.version = SETTINGSVERSION;

    // Capture
    settings.capture.captureDirectory = QDir::homePath();
    settings.capture.captureFormat = CaptureFormat::tenBitPacked;

    // USB
    settings.usb.vid = 0x1D50;
    settings.usb.pid = 0x603B;

    // PIC
    settings.pic.serialDevice = tr("");
    settings.pic.serialSpeed = SerialSpeeds::bps9600;
    settings.pic.playerModel = PlayerModels::none;

    // Write the configuration
    writeConfiguration();
}

// Functions to convert enums to and from integer values --------------------------------------------------------------

// Enum conversion from CaptureFormat to int
qint32 Configuration::convertCaptureFormatToInt(CaptureFormat captureFormat)
{
    if (captureFormat == CaptureFormat::tenBitPacked) return 0;
    if (captureFormat == CaptureFormat::sixteenBitSigned) return 1;

    // Default to 0
    return 0;
}

// Enum conversion from int to CaptureFormat
Configuration::CaptureFormat Configuration::convertIntToCaptureFormat(qint32 captureInt)
{
    if (captureInt == 0) return CaptureFormat::tenBitPacked;
    if (captureInt == 1) return CaptureFormat::sixteenBitSigned;

    // Default to 10 bit packed
    return CaptureFormat::tenBitPacked;
}

// Enum conversion from serial speed to int
qint32 Configuration::convertSerialSpeedsToInt(SerialSpeeds serialSpeeds)
{
    if (serialSpeeds == SerialSpeeds::bps1200) return 0;
    if (serialSpeeds == SerialSpeeds::bps2400) return 1;
    if (serialSpeeds == SerialSpeeds::bps4800) return 2;
    if (serialSpeeds == SerialSpeeds::bps9600) return 3;

    // Default to bps9600
    return 3;
}

// Enum conversion from int to serial speed
Configuration::SerialSpeeds Configuration::convertIntToSerialSpeeds(qint32 serialInt)
{
    if (serialInt == 0) return SerialSpeeds::bps1200;
    if (serialInt == 1) return SerialSpeeds::bps2400;
    if (serialInt == 2) return SerialSpeeds::bps4800;
    if (serialInt == 3) return SerialSpeeds::bps9600;

    // Default to bps9600
    return SerialSpeeds::bps9600;
}

// Enum conversion from player model to int
qint32 Configuration::convertPlayerModelsToInt(PlayerModels playerModels)
{
    if (playerModels == PlayerModels::none) return 0;
    if (playerModels == PlayerModels::pioneerLDV4300D) return 1;
    if (playerModels == PlayerModels::pioneerCLDV2800) return 2;

    // Default to none
    return 0;
}

// Enum conversion from int to player model
Configuration::PlayerModels Configuration::convertIntToPlayerModels(qint32 playerInt)
{
    if (playerInt == 0) return PlayerModels::none;
    if (playerInt == 1) return PlayerModels::pioneerLDV4300D;
    if (playerInt == 2) return PlayerModels::pioneerCLDV2800;

    // Default to none
    return PlayerModels::none;
}

// Functions to get and set configuration values ----------------------------------------------------------------------

// Capture settings
void Configuration::setCaptureDirectory(QString captureDirectory)
{
    settings.capture.captureDirectory = captureDirectory;
}

QString Configuration::getCaptureDirectory(void)
{
    return settings.capture.captureDirectory;
}

void Configuration::setCaptureFormat(CaptureFormat captureFormat)
{
    settings.capture.captureFormat = captureFormat;
}

Configuration::CaptureFormat Configuration::getCaptureFormat(void)
{
    return settings.capture.captureFormat;
}

// USB settings
void Configuration::setUsbVid(quint16 vid)
{
    settings.usb.vid = vid;
}

quint16 Configuration::getUsbVid(void)
{
    return settings.usb.vid;
}
void Configuration::setUsbPid(quint16 pid)
{
    settings.usb.pid = pid;
}

quint16 Configuration::getUsbPid(void)
{
    return settings.usb.pid;
}

// PIC settings
void Configuration::setSerialSpeed(SerialSpeeds serialSpeed)
{
    settings.pic.serialSpeed = serialSpeed;
}

Configuration::SerialSpeeds Configuration::getSerialSpeed(void)
{
    return settings.pic.serialSpeed;
}

void Configuration::setPlayerModel(PlayerModels playerModel)
{
    settings.pic.playerModel = playerModel;
}

Configuration::PlayerModels Configuration::getPlayerModel(void)
{
    return settings.pic.playerModel;
}

void Configuration::setSerialDevice(QString serialDevice)
{
    settings.pic.serialDevice = serialDevice;
}

QString Configuration::getSerialDevice(void)
{
    return settings.pic.serialDevice;
}
