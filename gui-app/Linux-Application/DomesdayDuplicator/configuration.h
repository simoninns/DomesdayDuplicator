/************************************************************************

    configuration.h

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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <QObject>
#include <QCoreApplication>
#include <QSettings>
#include <QApplication>
#include <QDir>
#include <QDebug>

class Configuration : public QObject
{
    Q_OBJECT
public:
    explicit Configuration(QObject *parent = nullptr);

    void writeConfiguration(void);
    void readConfiguration(void);

    // Get and set methods
    void setDefault(void);

    void setCaptureDirectory(QString captureDirectory);
    QString getCaptureDirectory(void);
    void setUsbVid(quint16 vid);
    quint16 getUsbVid(void);
    void setUsbPid(quint16 pid);
    quint16 getUsbPid(void);

signals:

public slots:

private:
    QSettings *configuration;

    // Note: Configuration is organised by the following top-level labels
    // Capture, USB, PIC (player integrated capture)
    struct Capture {
        QString captureDirectory;
    };

    struct Usb {
        quint16 vid;    // Vendor ID of USB device
        quint16 pid;    // Product ID of USB device
    };

    struct Pic {
        QString serialDevice;
        qint32 serialSpeed;
        QString playerModel;
    };

    struct Settings {
        qint32 version;
        Capture capture;
        Usb usb;
        Pic pic;
    } settings;
};

#endif // CONFIGURATION_H
