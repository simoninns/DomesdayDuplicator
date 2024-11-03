/************************************************************************

    configuration.h

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
#pragma once
#include <QObject>
#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QApplication>
#include <QDir>
#include <QDebug>
#include <memory>

class Configuration : public QObject
{
    Q_OBJECT
public:
    // Define the possible capture formats
    enum CaptureFormat {
        tenBitPacked,
        sixteenBitSigned,
        tenBitCdPacked
    };

    // Define the possible serial communication speeds
    enum SerialSpeeds {
        bps1200,
        bps2400,
        bps4800,
        bps9600,
        autoDetect,
    };

    explicit Configuration(QObject *parent = nullptr);

    void writeConfiguration();
    void readConfiguration();

    // Get and set methods
    void setDefault();

    void setCaptureDirectory(QString captureDirectory);
    QString getCaptureDirectory() const;
    void setCaptureFormat(CaptureFormat captureFormat);
    CaptureFormat getCaptureFormat() const;
    void setUsbVid(quint16 vid);
    quint16 getUsbVid() const;
    void setUsbPid(quint16 pid);
    quint16 getUsbPid() const;
    void setUsbPreferredDevice(QString preferredDevice);
    QString getUsbPreferredDevice() const;
    void setDiskBufferQueueSize(size_t state);
    size_t getDiskBufferQueueSize() const;
    void setUseSmallUsbTransferQueue(bool state);
    bool getUseSmallUsbTransferQueue() const;
    void setUseSmallUsbTransfers(bool state);
    bool getUseSmallUsbTransfers() const;
    void setUseWinUsb(bool state);
    bool getUseWinUsb() const;
    void setUseAsyncFileIo(bool state);
    bool getUseAsyncFileIo() const;
    void setSerialSpeed(SerialSpeeds serialSpeed);
    SerialSpeeds getSerialSpeed() const;
    void setSerialDevice(QString serialDevice);
    QString getSerialDevice() const;
    void setKeyLock(bool keyLock);
    bool getKeyLock() const;
    void setPerSideNotesEnabled(bool enabled);
    bool getPerSideNotesEnabled() const;
    void setPerSideMintEnabled(bool enabled);
    bool getPerSideMintEnabled() const;
    void setAmplitudeLabelEnabled(bool enabled);
    bool getAmplitudeLabelEnabled() const;
    void setAmplitudeChartEnabled(bool enabled);
    bool getAmplitudeChartEnabled() const;

    void setMainWindowGeometry(QByteArray mainWindowGeometry);
    QByteArray getMainWindowGeometry() const;
    void setPlayerRemoteDialogGeometry(QByteArray playerRemoteDialogGeometry);
    QByteArray getPlayerRemoteDialogGeometry() const;
    void setAdvancedNamingDialogGeometry(QByteArray advancedNamingDialogGeometry);
    QByteArray getAdvancedNamingDialogGeometry() const;
    void setAutomaticCaptureDialogGeometry(QByteArray automaticCaptureDialogGeometry);
    QByteArray getAutomaticCaptureDialogGeometry() const;
    void setConfigurationDialogGeometry(QByteArray configurationDialogGeometry);
    QByteArray getConfigurationDialogGeometry() const;

signals:

public slots:

private:
    std::unique_ptr<QSettings> configuration;

    // Note: Configuration is organised by the following top-level labels
    // Capture, USB, PIC (player integrated capture), UI
    struct Capture {
        QString captureDirectory;
        CaptureFormat captureFormat;
    };

    struct Usb {
        quint16 vid;    // Vendor ID of USB device
        quint16 pid;    // Product ID of USB device
        QString preferredDevice;
        size_t diskBufferQueueSize;
        bool useSmallUsbTransferQueue;
        bool useSmallUsbTransfers;
        bool useWinUsb;
        bool useAsyncFileIo;
    };

    struct Pic {
        QString serialDevice;
        SerialSpeeds serialSpeed;
        bool keyLock;
    };

    struct Ui {
        bool perSideNotesEnabled;
        bool perSideMintEnabled;
        bool amplitudeLabelEnabled;
        bool amplitudeChartEnabled;
    };

    // Window geometry and settings
    struct Windows {
        QByteArray mainWindowGeometry;
        QByteArray playerRemoteDialogGeometry;
        QByteArray advancedNamingDialogGeometry;
        QByteArray automaticCaptureDialogGeometry;
        QByteArray configurationDialogGeometry;
    };

    struct Settings {
        qint32 version;
        Capture capture;
        Usb usb;
        Pic pic;
        Ui ui;
        Windows windows;
    } settings;

    qint32 convertCaptureFormatToInt(CaptureFormat captureFormat);
    CaptureFormat convertIntToCaptureFormat(qint32 captureInt);
    qint32 convertSerialSpeedsToInt(SerialSpeeds serialSpeeds);
    SerialSpeeds convertIntToSerialSpeeds(qint32 serialInt);
};
