/************************************************************************

    lvdpcontrol.h

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

#ifndef LVDPCONTROL_H
#define LVDPCONTROL_H

#include <QApplication>
#include <QtSerialPort/QSerialPort>
#include <QDebug>
#include <QTimer>
#include <QTimerEvent>

class lvdpControl : public QObject
{
public:
    lvdpControl();

    bool connectPlayer(QString portName, qint16 baudRate);
    void disconnectPlayer(void);
    bool isConnected(void);
    void serialWrite(QString command);

private slots:
    void handleError(QSerialPort::SerialPortError error);
    void serialReadReady(void);
    void handleTimeout(void);

private:
    struct Settings {
        QString name;
        QSerialPort::BaudRate baudRate;
        QSerialPort::DataBits dataBits;
        QSerialPort::Parity parity;
        QSerialPort::StopBits stopBits;
        QSerialPort::FlowControl flowControl;
        bool connected;
    };

    Settings currentSettings;
    QSerialPort *lvdpSerialPort;
    QTimer *timeoutTimer;

    QByteArray receivedData;
};

#endif // LVDPCONTROL_H
