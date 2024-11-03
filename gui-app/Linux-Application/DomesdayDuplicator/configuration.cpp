/************************************************************************

    configuration.cpp

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

#include "configuration.h"

// This define should be incremented if the settings file format changes
#define SETTINGSVERSION 4

Configuration::Configuration(QObject *parent) : QObject(parent)
{
    // Open the application's configuration file
    QString configurationPath ;
    QString configurationFileName;

    configurationPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) ;
    configurationFileName = "DomesdayDuplicator.ini" ;
    configuration.reset(new QSettings(configurationPath + "/" + configurationFileName, QSettings::IniFormat));

    // Read the configuration
    readConfiguration();

    // Are the configuration settings valid?
    if (settings.version != SETTINGSVERSION) {
        qDebug() << "Configuration::Configuration(): Configuration invalid or wrong version (" <<
                    settings.version << "!= " << SETTINGSVERSION <<").  Setting to default values";

        // Set default configuration
        setDefault();
        writeConfiguration();
    }
}

void Configuration::writeConfiguration()
{
    // Write the valid configuration flag
    configuration->setValue("version", settings.version);

    // Capture
    configuration->beginGroup("capture");
    configuration->setValue("captureDirectory", settings.capture.captureDirectory);
    configuration->setValue("captureFormat", convertCaptureFormatToInt(settings.capture.captureFormat));
    configuration->endGroup();

    // UI
    configuration->beginGroup("ui");
    configuration->setValue("perSideNotesEnabled", settings.ui.perSideNotesEnabled);
    configuration->setValue("perSideMintEnabled", settings.ui.perSideMintEnabled);
    configuration->setValue("amplitudeEnabled", settings.ui.amplitudeLabelEnabled);
    configuration->setValue("graphType", settings.ui.amplitudeChartEnabled ? 1 : 0);
    configuration->endGroup();

    // USB
    configuration->beginGroup("usb");
    configuration->setValue("vid", settings.usb.vid);
    configuration->setValue("pid", settings.usb.pid);
    configuration->setValue("preferredDevice", settings.usb.preferredDevice);
    configuration->setValue("diskBufferQueueSize", (quint64)settings.usb.diskBufferQueueSize);
    configuration->setValue("useSmallUsbTransferQueue", settings.usb.useSmallUsbTransferQueue);
    configuration->setValue("useSmallUsbTransfers", settings.usb.useSmallUsbTransfers);
    configuration->setValue("useWinUsb", settings.usb.useWinUsb);
    configuration->setValue("useAsyncFileIo", settings.usb.useAsyncFileIo);
    configuration->endGroup();

    // PIC
    configuration->beginGroup("pic");
    configuration->setValue("serialDevice", settings.pic.serialDevice);
    configuration->setValue("serialSpeed", convertSerialSpeedsToInt(settings.pic.serialSpeed));
    configuration->setValue("keyLock", settings.pic.keyLock);
    configuration->endGroup();

    // Windows
    configuration->beginGroup("windows");
    configuration->setValue("mainWindowGeometry", settings.windows.mainWindowGeometry);
    configuration->setValue("playerRemoteDialogGeometry", settings.windows.playerRemoteDialogGeometry);
    configuration->setValue("advancedNamingDialogGeometry", settings.windows.advancedNamingDialogGeometry);
    configuration->setValue("automaticCaptureDialogGeometry", settings.windows.automaticCaptureDialogGeometry);
    configuration->setValue("configurationDialogGeometry", settings.windows.configurationDialogGeometry);
    configuration->endGroup();

    // Sync the settings with disk
    configuration->sync();
}

void Configuration::readConfiguration()
{
    qDebug() << "Configuration::readConfiguration(): Reading configuration from" << configuration->fileName();

    // Read the valid configuration flag
    settings.version = configuration->value("version").toInt();

    // Capture
    configuration->beginGroup("capture");
    settings.capture.captureDirectory = configuration->value("captureDirectory").toString();
    settings.capture.captureFormat = convertIntToCaptureFormat(configuration->value("captureFormat").toInt());
    configuration->endGroup();

    // UI
    configuration->beginGroup("ui");
    settings.ui.perSideNotesEnabled = configuration->value("perSideNotesEnabled").toBool();
    settings.ui.perSideMintEnabled = configuration->value("perSideMintEnabled").toBool();
    settings.ui.amplitudeLabelEnabled = configuration->value("amplitudeEnabled").toBool();
    settings.ui.amplitudeChartEnabled = configuration->value("graphType").toInt() > 0;
    configuration->endGroup();

    // USB
    configuration->beginGroup("usb");
    settings.usb.vid = static_cast<quint16>(configuration->value("vid").toUInt());
    settings.usb.pid = static_cast<quint16>(configuration->value("pid").toUInt());
    settings.usb.preferredDevice = configuration->value("preferredDevice").toString();
    settings.usb.diskBufferQueueSize = (size_t)configuration->value("diskBufferQueueSize").toULongLong();
    settings.usb.useSmallUsbTransferQueue = configuration->value("useSmallUsbTransferQueue").toBool();
    settings.usb.useSmallUsbTransfers = configuration->value("useSmallUsbTransfers").toBool();
    settings.usb.useWinUsb = configuration->value("useWinUsb").toBool();
    settings.usb.useAsyncFileIo = configuration->value("useAsyncFileIo").toBool();
    configuration->endGroup();

    // PIC
    configuration->beginGroup("pic");
    settings.pic.serialDevice = configuration->value("serialDevice").toString();
    settings.pic.serialSpeed = convertIntToSerialSpeeds(configuration->value("serialSpeed").toInt());
    settings.pic.keyLock = configuration->value("keyLock").toBool();
    configuration->endGroup();

    // Windows
    configuration->beginGroup("windows");
    settings.windows.mainWindowGeometry = configuration->value("mainWindowGeometry").toByteArray();
    settings.windows.playerRemoteDialogGeometry = configuration->value("playerRemoteDialogGeometry").toByteArray();
    settings.windows.advancedNamingDialogGeometry = configuration->value("advancedNamingDialogGeometry").toByteArray();
    settings.windows.automaticCaptureDialogGeometry = configuration->value("automaticCaptureDialogGeometry").toByteArray();
    settings.windows.configurationDialogGeometry = configuration->value("configurationDialogGeometry").toByteArray();
    configuration->endGroup();
}

void Configuration::setDefault()
{
    // Set up the default values
    settings.version = SETTINGSVERSION;

    // Capture
    settings.capture.captureDirectory = QDir::homePath();
    settings.capture.captureFormat = CaptureFormat::tenBitPacked;

    // UI
    settings.ui.perSideNotesEnabled = false;
    settings.ui.perSideMintEnabled = false;
    settings.ui.amplitudeLabelEnabled = false;
    settings.ui.amplitudeChartEnabled = false;

    // USB
    settings.usb.vid = 0x1D50;
    settings.usb.pid = 0x603B;
    settings.usb.preferredDevice = "";
    settings.usb.diskBufferQueueSize = 256 * 1024 * 1024;
    settings.usb.useSmallUsbTransferQueue = false;
    settings.usb.useSmallUsbTransfers = true;
#ifdef _WIN32
    settings.usb.useWinUsb = true;
    settings.usb.useAsyncFileIo = true;
#else
    settings.usb.useWinUsb = false;
    settings.usb.useAsyncFileIo = false;
#endif

    // PIC
    settings.pic.serialDevice = tr("");
    settings.pic.serialSpeed = SerialSpeeds::autoDetect;
    settings.pic.keyLock = false;

    // Windows
    settings.windows.mainWindowGeometry = QByteArray();
    settings.windows.playerRemoteDialogGeometry = QByteArray();
    settings.windows.advancedNamingDialogGeometry = QByteArray();
    settings.windows.automaticCaptureDialogGeometry = QByteArray();
    settings.windows.configurationDialogGeometry = QByteArray();
}

// Functions to convert enums to and from integer values --------------------------------------------------------------

// Enum conversion from CaptureFormat to int
qint32 Configuration::convertCaptureFormatToInt(CaptureFormat captureFormat)
{
    if (captureFormat == CaptureFormat::tenBitPacked) return 0;
    if (captureFormat == CaptureFormat::sixteenBitSigned) return 1;
    if (captureFormat == CaptureFormat::tenBitCdPacked) return 2;

    // Default to 0
    return 0;
}

// Enum conversion from int to CaptureFormat
Configuration::CaptureFormat Configuration::convertIntToCaptureFormat(qint32 captureInt)
{
    if (captureInt == 0) return CaptureFormat::tenBitPacked;
    if (captureInt == 1) return CaptureFormat::sixteenBitSigned;
    if (captureInt == 2) return CaptureFormat::tenBitCdPacked;

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
    if (serialSpeeds == SerialSpeeds::autoDetect) return 4;

    // Default to auto detect
    return 4;
}

// Enum conversion from int to serial speed
Configuration::SerialSpeeds Configuration::convertIntToSerialSpeeds(qint32 serialInt)
{
    if (serialInt == 0) return SerialSpeeds::bps1200;
    if (serialInt == 1) return SerialSpeeds::bps2400;
    if (serialInt == 2) return SerialSpeeds::bps4800;
    if (serialInt == 3) return SerialSpeeds::bps9600;
    if (serialInt == 4) return SerialSpeeds::autoDetect;

    // Default to auto detect
    return SerialSpeeds::autoDetect;
}

// Functions to get and set configuration values ----------------------------------------------------------------------

// Capture settings
void Configuration::setCaptureDirectory(QString captureDirectory)
{
    settings.capture.captureDirectory = captureDirectory;
}

QString Configuration::getCaptureDirectory() const
{
    return settings.capture.captureDirectory;
}

void Configuration::setCaptureFormat(CaptureFormat captureFormat)
{
    settings.capture.captureFormat = captureFormat;
}

Configuration::CaptureFormat Configuration::getCaptureFormat() const
{
    return settings.capture.captureFormat;
}

// USB settings
void Configuration::setUsbVid(quint16 vid)
{
    settings.usb.vid = vid;
}

quint16 Configuration::getUsbVid() const
{
    return settings.usb.vid;
}

void Configuration::setUsbPid(quint16 pid)
{
    settings.usb.pid = pid;
}

quint16 Configuration::getUsbPid() const
{
    return settings.usb.pid;
}

void Configuration::setUsbPreferredDevice(QString preferredDevice)
{
    settings.usb.preferredDevice = preferredDevice;
}

QString Configuration::getUsbPreferredDevice() const
{
    return settings.usb.preferredDevice;
}

void Configuration::setDiskBufferQueueSize(size_t state)
{
    settings.usb.diskBufferQueueSize = state;
}

size_t Configuration::getDiskBufferQueueSize() const
{
    return settings.usb.diskBufferQueueSize;
}

void Configuration::setUseSmallUsbTransferQueue(bool state)
{
    settings.usb.useSmallUsbTransferQueue = state;
}

bool Configuration::getUseSmallUsbTransferQueue() const
{
    return settings.usb.useSmallUsbTransferQueue;
}

void Configuration::setUseSmallUsbTransfers(bool state)
{
    settings.usb.useSmallUsbTransfers = state;
}

bool Configuration::getUseSmallUsbTransfers() const
{
    return settings.usb.useSmallUsbTransfers;
}

void Configuration::setUseWinUsb(bool state)
{
    settings.usb.useWinUsb = state;
}

bool Configuration::getUseWinUsb() const
{
    return settings.usb.useWinUsb;
}

void Configuration::setUseAsyncFileIo(bool state)
{
    settings.usb.useAsyncFileIo = state;
}

bool Configuration::getUseAsyncFileIo() const
{
    return settings.usb.useAsyncFileIo;
}

// PIC settings
void Configuration::setSerialSpeed(SerialSpeeds serialSpeed)
{
    settings.pic.serialSpeed = serialSpeed;
}

Configuration::SerialSpeeds Configuration::getSerialSpeed() const
{
    return settings.pic.serialSpeed;
}

void Configuration::setSerialDevice(QString serialDevice)
{
    settings.pic.serialDevice = serialDevice;
}

QString Configuration::getSerialDevice() const
{
    return settings.pic.serialDevice;
}

void Configuration::setKeyLock(bool keyLock)
{
    settings.pic.keyLock = keyLock;
}

bool Configuration::getKeyLock() const
{
    return settings.pic.keyLock;
}

// UI settings
void Configuration::setPerSideNotesEnabled(bool enabled)
{
    settings.ui.perSideNotesEnabled = enabled;
}

bool Configuration::getPerSideNotesEnabled() const
{
    return settings.ui.perSideNotesEnabled;
}

void Configuration::setPerSideMintEnabled(bool enabled)
{
    settings.ui.perSideMintEnabled = enabled;
}

bool Configuration::getPerSideMintEnabled() const
{
    return settings.ui.perSideMintEnabled;
}

void Configuration::setAmplitudeLabelEnabled(bool enabled)
{
    settings.ui.amplitudeLabelEnabled = enabled;
}

bool Configuration::getAmplitudeLabelEnabled() const
{
    return settings.ui.amplitudeLabelEnabled;
}

void Configuration::setAmplitudeChartEnabled(bool enabled)
{
    settings.ui.amplitudeChartEnabled = enabled;
}

bool Configuration::getAmplitudeChartEnabled() const
{
    return settings.ui.amplitudeChartEnabled;
}

// Windows
void Configuration::setMainWindowGeometry(QByteArray mainWindowGeometry)
{
    settings.windows.mainWindowGeometry = mainWindowGeometry;
}

QByteArray Configuration::getMainWindowGeometry() const
{
    return settings.windows.mainWindowGeometry;
}

void Configuration::setPlayerRemoteDialogGeometry(QByteArray playerRemoteDialogGeometry)
{
    settings.windows.playerRemoteDialogGeometry = playerRemoteDialogGeometry;
}

QByteArray Configuration::getPlayerRemoteDialogGeometry() const
{
    return settings.windows.playerRemoteDialogGeometry;
}

void Configuration::setAdvancedNamingDialogGeometry(QByteArray advancedNamingDialogGeometry)
{
    settings.windows.advancedNamingDialogGeometry = advancedNamingDialogGeometry;
}

QByteArray Configuration::getAdvancedNamingDialogGeometry() const
{
    return settings.windows.advancedNamingDialogGeometry;
}

void Configuration::setAutomaticCaptureDialogGeometry(QByteArray automaticCaptureDialogGeometry)
{
    settings.windows.automaticCaptureDialogGeometry = automaticCaptureDialogGeometry;
}

QByteArray Configuration::getAutomaticCaptureDialogGeometry() const
{
    return settings.windows.automaticCaptureDialogGeometry;
}

void Configuration::setConfigurationDialogGeometry(QByteArray configurationDialogGeometry)
{
    settings.windows.configurationDialogGeometry = configurationDialogGeometry;
}

QByteArray Configuration::getConfigurationDialogGeometry() const
{
    return settings.windows.configurationDialogGeometry;
}
