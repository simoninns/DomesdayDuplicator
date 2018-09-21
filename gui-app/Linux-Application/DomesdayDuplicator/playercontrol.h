/************************************************************************

    playercontrol.cpp

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

#ifndef PLAYERCONTROL_H
#define PLAYERCONTROL_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <QQueue>
#include <QDebug>

#include "playercommunication.h"

class PlayerControl : public QThread
{
    Q_OBJECT

public:
    explicit PlayerControl(QObject *parent = nullptr);
    ~PlayerControl() override;

    void configurePlayerCommunication(
            QString serialDevice,
            PlayerCommunication::SerialSpeed serialSpeed,
            PlayerCommunication::PlayerType playerType);

    QString getPlayerStatusInformation(void);
    QString getPlayerPositionInformation(void);

    // Commands
    enum Commands {
        cmdSetTrayState,
        cmdSetPlayerState,
        cmdStep,
        cmdScan,
        cmdMultiSpeed,
        cmdSetFramePosition,
        cmdSetTimeCodePosition,
        cmdSetStopFrame,
        cmdSetStopTimeCode,
        cmdSetOnScreenDisplay,
        cmdSetAudio,
        cmdSetKeyLock,
        cmdSetSpeed
    };

    void setTrayState(PlayerCommunication::TrayState trayState);
    void setPlayerState(PlayerCommunication::PlayerState playerState);
    void step(PlayerCommunication::Direction direction);
    void scan(PlayerCommunication::Direction direction);
    void multiSpeed(PlayerCommunication::Direction direction);
    void setFramePosition(qint32 frame);
    void setTimeCodePosition(qint32 timeCode);
    void setStopFrame(qint32 frame);
    void setStopTimeCode(qint32 timeCode);
    void setOnScreenDisplay(PlayerCommunication::DisplayState displayState);
    void setAudio(PlayerCommunication::AudioState audioState);
    void setKeyLock(PlayerCommunication::KeyLockState keyLockState);
    void setSpeed(qint32 speed);

signals:
    void startCapture(void);
    void stopCapture(void);
    void playerControlError(QString);

protected:
    void run() override;

private:
    // Thread control
    QMutex mutex;
    QWaitCondition condition;
    bool reconnect;
    bool abort;

    bool isPlayerConnected;
    PlayerCommunication::PlayerState playerState;
    PlayerCommunication::DiscType discType;
    qint32 timeCode;
    qint32 frameNumber;

    QString serialDevice;
    PlayerCommunication::SerialSpeed serialSpeed;
    PlayerCommunication::PlayerType playerType;

    PlayerCommunication *playerCommunication;

    // Command queue
    QQueue<PlayerControl::Commands> commandQueue;
    QQueue<qint32> parameterQueue;

    void processCommandQueue(void);

    void processSetTrayState(qint32 parameter1);
    void processSetPlayerState(qint32 parameter1);
    void processStep(qint32 parameter1);
    void processScan(qint32 parameter1);
    void processMultiSpeed(qint32 parameter1);
    void processSetFramePosition(qint32 parameter1);
    void processSetTimeCodePosition(qint32 parameter1);
    void processSetStopFrame(qint32 parameter1);
    void processSetStopTimeCode(qint32 parameter1);
    void processSetOnScreenDisplay(qint32 parameter1);
    void processSetAudio(qint32 parameter1);
    void processSetKeyLock(qint32 parameter1);
    void processSetSpeed(qint32 parameter1);
};

#endif // PLAYERCONTROL_H
