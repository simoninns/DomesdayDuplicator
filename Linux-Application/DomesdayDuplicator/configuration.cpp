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

Configuration::Configuration(QObject *parent) : QObject(parent)
{
    // Open the application's configuration file
    configuration = new QSettings();

    // Read the configuration
    readConfiguration();

    // Are the configuration settings valid?
    if (settings.version != 1) {
        qDebug() << "Configuration::Configuration(): Configuration invalid or wrong version.  Setting to default values";

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
    configuration->endGroup();

    // USB
    configuration->beginGroup("usb");
    configuration->setValue("vid", settings.usb.vid);
    configuration->setValue("pid", settings.usb.pid);
    configuration->endGroup();

    // PIC
    configuration->beginGroup("pic");
    configuration->setValue("serialDevice", settings.pic.serialDevice);
    configuration->setValue("serialSpeed", settings.pic.serialSpeed);
    configuration->setValue("playerModel", settings.pic.playerModel);
    configuration->endGroup();
}

void Configuration::readConfiguration(void)
{
    // Read the valid configuration flag
    settings.version = configuration->value("version").toInt();

    // Capture
    configuration->beginGroup("capture");
    settings.capture.captureDirectory = configuration->value("captureDirectory").toString();
    configuration->endGroup();

    // USB
    configuration->beginGroup("usb");
    settings.usb.vid = static_cast<quint16>(configuration->value("vid").toUInt());
    settings.usb.pid = static_cast<quint16>(configuration->value("pid").toUInt());
    configuration->endGroup();

    // PIC
    configuration->beginGroup("pic");
    settings.pic.serialDevice = configuration->value("serialDevice").toString();
    settings.pic.serialSpeed = configuration->value("serialSpeed").toInt();
    settings.pic.playerModel = configuration->value("playerModel").toString();
    configuration->endGroup();
}

void Configuration::setDefault(void)
{
    // Set up the default values
    settings.version = 1;

    // Capture
    settings.capture.captureDirectory = QDir::homePath();

    // USB
    settings.usb.vid = 0x1D50;
    settings.usb.pid = 0x603B;

    // PIC
    settings.pic.serialDevice = tr("");
    settings.pic.serialSpeed = 9600;
    settings.pic.playerModel = tr("Pioneer LD-V4300D");

    // Write the configuration
    writeConfiguration();
}

// Get/set configuration values
void Configuration::setCaptureDirectory(QString captureDirectory)
{
    settings.capture.captureDirectory = captureDirectory;
}

QString Configuration::getCaptureDirectory(void)
{
    return settings.capture.captureDirectory;
}

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
