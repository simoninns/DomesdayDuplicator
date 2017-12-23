/************************************************************************

    lvdpcontrol.cpp

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

#include "lvdpcontrol.h"

lvdpControl::lvdpControl()
{
    // Define the serial port for LVDP communication
    lvdpSerialPort = new QSerialPort;

    // Connect the serial port signals to catch errors
    connect(lvdpSerialPort, static_cast<void (QSerialPort::*)(QSerialPort::SerialPortError)>(&QSerialPort::error), this, &lvdpControl::handleError);

    // Connect the serial read and write signals to the main window
    //connect(lvdpSerialPort, &QSerialPort::readyRead, this, &MainWindow::readData);
}

// Connect to the player
bool lvdpControl::connectPlayer(QString portName, qint16 baudRate)
{
    bool connectSuccessful = false;

    // If the serial port is open, close it
    if (lvdpSerialPort->isOpen()) lvdpSerialPort->close();
    currentSettings.connected = false;

    // Verify that the baud rate is valid
    if (baudRate != 1200 && baudRate != 2400 && baudRate != 9600)
        return connectSuccessful; // return false

    // Configure the serial port
    lvdpSerialPort->setPortName(portName); // Set the device name

    if (baudRate == 1200) lvdpSerialPort->setBaudRate(QSerialPort::Baud1200);
    if (baudRate == 2400) lvdpSerialPort->setBaudRate(QSerialPort::Baud2400);
    if (baudRate == 9600) lvdpSerialPort->setBaudRate(QSerialPort::Baud9600);

    lvdpSerialPort->setDataBits(QSerialPort::Data8);
    lvdpSerialPort->setParity(QSerialPort::NoParity);
    lvdpSerialPort->setStopBits(QSerialPort::OneStop);
    lvdpSerialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (lvdpSerialPort->open(QIODevice::ReadWrite)) {
        qDebug() << "lvdpControl::lvdpConnect(): Serial port opened successfully";
        connectSuccessful = true;
        currentSettings.connected = true;
    } else {
        qDebug() << "lvdpControl::lvdpConnect(): Unable to open serial port!";
    }

    return connectSuccessful;
}

// Ensure the player is disconnected
void lvdpControl::disconnectPlayer(void)
{
    // If the serial port is open, close it
    if (lvdpSerialPort->isOpen()) lvdpSerialPort->close();
    currentSettings.connected = false;

    qDebug() << "lvdpControl::disconnectPlayer(): Player disconnected";
}

// Handle an error signal from the serial port
void lvdpControl::handleError(QSerialPort::SerialPortError error)
{
    // If we receive a critical error from the serial port, close it cleanly
    if (error == QSerialPort::ResourceError) {
        // If the serial port is open, close it
        if (lvdpSerialPort->isOpen()) lvdpSerialPort->close();
        currentSettings.connected = false;
        qDebug() << "lvdpControl::handleError(): Serial port resource error! Closed connection to player";
    }
}

// Return the current connection status
bool lvdpControl::isConnected(void)
{
    return currentSettings.connected;
}

// Function to write console data to the serial port
void lvdpControl::writeData(const QByteArray &data)
{
    lvdpSerialPort->write(data);
}

// Function to write serial data to the console
void lvdpControl::readData()
{
    // Read all available data from the serial port
    QByteArray data = lvdpSerialPort->readAll();
}
