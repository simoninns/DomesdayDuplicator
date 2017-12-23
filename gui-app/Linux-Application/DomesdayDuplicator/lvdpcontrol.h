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

class lvdpControl : public QObject
{
public:
    lvdpControl();

    bool connectPlayer(QString portName, qint16 baudRate);
    void disconnectPlayer(void);
    bool isConnected(void);

private slots:
    void handleError(QSerialPort::SerialPortError error);

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

    void writeData(const QByteArray &data);
    void readData();
};

#endif // LVDPCONTROL_H
