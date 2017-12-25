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
#include <QtConcurrent/QtConcurrent>
#include <QQueue>

class lvdpControl : public QObject
{
public:
    lvdpControl();
    ~lvdpControl();

    // Define the possible player commands
    enum PlayerCommands {
        command_play,
        command_pause,
        command_stop,
        command_stepForwards,
        command_stepBackwards,
        command_scanForwards,
        command_scanBackwards,
        command_keyLockOn,
        command_keyLockOff,
        command_seek,
        command_resetDiscLength,
        command_getDiscLength
    };

    void stopStateMachine(void);

    // Functions to interact with the state machine
    void serialConfigured(QString portName, qint16 baudRate);
    void serialUnconfigured(void);

    bool isConnected(void);
    bool isPlaying(void);
    bool isPaused(void);
    bool isCav(void);
    quint32 currentFrameNumber(void);
    quint32 currentTimeCode(void);
    quint32 getDiscLength(void);

    void command(PlayerCommands command, quint32 parameter);

signals:
    void startCapture(void);
    void stopCapture(void);
};

#endif // LVDPCONTROL_H
