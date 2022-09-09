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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <QObject>
#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QApplication>
#include <QDir>
#include <QDebug>

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

    // Define the possible amplitude graph types
    enum GraphType {
        noGraph,
        QCPMean,
    };

    explicit Configuration(QObject *parent = nullptr);

    void writeConfiguration(void);
    void readConfiguration(void);

    // Get and set methods
    void setDefault(void);

    void setCaptureDirectory(QString captureDirectory);
    QString getCaptureDirectory(void);
    void setCaptureFormat(CaptureFormat captureFormat);
    CaptureFormat getCaptureFormat(void);
    void setUsbVid(quint16 vid);
    quint16 getUsbVid(void);
    void setUsbPid(quint16 pid);
    quint16 getUsbPid(void);
    void setSerialSpeed(SerialSpeeds serialSpeed);
    SerialSpeeds getSerialSpeed(void);
    void setSerialDevice(QString serialDevice);
    QString getSerialDevice(void);
    void setKeyLock(bool keyLock);
    bool getKeyLock(void);
    void setGraphType(GraphType graphType);
    GraphType getGraphType(void);
    bool getAmplitudeEnabled(void);
    void setAmplitudeEnabled(bool amplitudeEnabled);

    void setMainWindowGeometry(QByteArray mainWindowGeometry);
    QByteArray getMainWindowGeometry(void);
    void setPlayerRemoteDialogGeometry(QByteArray playerRemoteDialogGeometry);
    QByteArray getPlayerRemoteDialogGeometry(void);
    void setAdvancedNamingDialogGeometry(QByteArray advancedNamingDialogGeometry);
    QByteArray getAdvancedNamingDialogGeometry(void);
    void setAutomaticCaptureDialogGeometry(QByteArray automaticCaptureDialogGeometry);
    QByteArray getAutomaticCaptureDialogGeometry(void);
    void setConfigurationDialogGeometry(QByteArray configurationDialogGeometry);
    QByteArray getConfigurationDialogGeometry(void);

signals:

public slots:

private:
    QSettings *configuration;

    // Note: Configuration is organised by the following top-level labels
    // Capture, USB, PIC (player integrated capture), UI (includes optional UI features [currently RF amplitude RMS and graphing])
    struct Capture {
        QString captureDirectory;
        CaptureFormat captureFormat;
    };

    struct Usb {
        quint16 vid;    // Vendor ID of USB device
        quint16 pid;    // Product ID of USB device
    };

    struct Pic {
        QString serialDevice;
        SerialSpeeds serialSpeed;
        bool keyLock;
    };

    struct Ui {
        GraphType graphType;
        bool amplitudeEnabled;
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
    qint32 convertGraphTypeToInt(GraphType graphType);
    GraphType convertIntToGraphType(qint32 graphInt);
};

#endif // CONFIGURATION_H
