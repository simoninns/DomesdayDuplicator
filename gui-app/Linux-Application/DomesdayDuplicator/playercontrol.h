/************************************************************************

    playercontrol.cpp

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

    void stop();
    void configurePlayerCommunication(
            QString serialDevice,
            PlayerCommunication::SerialSpeed serialSpeed);

    QString getPlayerModelName();
    QString getPlayerVersionNumber();
    QString getSerialBaudRate();
    QString getPlayerStatusInformation();
    QString getPlayerPositionInformation();
    PlayerCommunication::DiscType getDiscType();

    // Commands
    enum Commands {
        cmdSetTrayState,
        cmdSetPlayerState,
        cmdStep,
        cmdScan,
        cmdMultiSpeed,
        cmdSetPositionFrame,
        cmdSetPositionTimeCode,
        cmdSetPositionChapter,
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
    void setPositionFrame(qint32 address);
    void setPositionTimeCode(qint32 address);
    void setPositionChapter(qint32 address);
    void setStopFrame(qint32 frame);
    void setStopTimeCode(qint32 timeCode);
    void setOnScreenDisplay(PlayerCommunication::DisplayState displayState);
    void setAudio(PlayerCommunication::AudioState audioState);
    void setKeyLock(PlayerCommunication::KeyLockState keyLockState);
    void setSpeed(qint32 speed);

    void startAutomaticCapture(bool fromLeadIn, bool wholeDisc,
                                     qint32 startAddress, qint32 endAddress,
                                     PlayerCommunication::DiscType discType, bool keyLock);
    void stopAutomaticCapture();
    QString getAutomaticCaptureStatus();
    QString getAutomaticCaptureError();

signals:
    void startCapture();
    void stopCapture();
    void playerControlError(QString);
    void automaticCaptureComplete(bool success);

    void playerConnected();
    void playerDisconnected();

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

    // Automatic capture
    QString acStatus;
    bool acInProgress;
    bool acCancelled;
    qint32 acStartAddress;
    qint32 acEndAddress;
    qint32 acLastSeenAddress;
    PlayerCommunication::DiscType acDiscType;
    bool acCaptureFromLeadIn;
    bool acCaptureWholeDisc;
    bool acKeyLock;

    // Automatic capture state-machine
    enum AcStates {
        ac_start_state,
        ac_getLength_state,
        ac_spinDown_state,
        ac_spinUpWithCapture_state,
        ac_moveToStartPosition_state,
        ac_playAndCapture_state,
        ac_waitForEndAddress_state,
        ac_finished_state,
        ac_cancelled_state,
        ac_error_state
    };

    AcStates acCurrentState;
    AcStates acNextState;
    QString acErrorMessage;

    // Command queue
    QQueue<PlayerControl::Commands> commandQueue;
    QQueue<qint32> parameterQueue;

    void processCommandQueue();

    void processSetTrayState(qint32 parameter1);
    void processSetPlayerState(qint32 parameter1);
    void processStep(qint32 parameter1);
    void processScan(qint32 parameter1);
    void processMultiSpeed(qint32 parameter1);
    void processSetPositionFrame(qint32 parameter1);
    void processSetPositionTimeCode(qint32 parameter1);
    void processSetPositionChapter(qint32 parameter1);
    void processSetStopFrame(qint32 parameter1);
    void processSetStopTimeCode(qint32 parameter1);
    void processSetOnScreenDisplay(qint32 parameter1);
    void processSetAudio(qint32 parameter1);
    void processSetKeyLock(qint32 parameter1);
    void processSetSpeed(qint32 parameter1);

    void processAutomaticCapture();

    // Automatic capture state methods
    PlayerControl::AcStates acStateStart();
    PlayerControl::AcStates acStateGetLength();
    PlayerControl::AcStates acStateSpinDown();
    PlayerControl::AcStates acStateSpinUpWithCapture();
    PlayerControl::AcStates acStateMoveToStartPosition();
    PlayerControl::AcStates acStatePlayAndCapture();
    PlayerControl::AcStates acStateWaitForEndAddress();
    PlayerControl::AcStates acStateFinished();
    PlayerControl::AcStates acStateCancelled();
    PlayerControl::AcStates acStateError();
};

#endif // PLAYERCONTROL_H
