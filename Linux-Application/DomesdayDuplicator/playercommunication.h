/************************************************************************

    playercommunication.h

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

#ifndef PLAYERCOMMUNICATION_H
#define PLAYERCOMMUNICATION_H

#include <QObject>
#include <QtSerialPort/QSerialPort>
#include <QDebug>
#include <QTimer>
#include <QTimerEvent>
#include <QElapsedTimer>

class PlayerCommunication : public QObject
{
    Q_OBJECT
public:
    explicit PlayerCommunication(QObject *parent = nullptr);
    ~PlayerCommunication() override;

    enum SerialSpeed {
        bps9600,
        bps4800,
        bps2400,
        bps1200
    };

    enum PlayerType {
        unknownPlayerType,
        pioneerLDV4300D,    // Pioneer LD-V4300D
        pioneerCLDV2800     // Pioneer CLD-V2800
    };

    enum PlayerState {
        unknownPlayerState,
        stop,
        play,
        pause,
        stillFrame
    };

    enum TrayState {
        unknownTrayState,
        open,
        closed
    };

    enum DisplayState {
        unknownDisplayState,
        on,
        off
    };

    enum DiscType {
        unknownDiscType,
        CAV,
        CLV
    };

    enum Direction {
        forwards,
        backwards
    };

    enum AudioState {
        audioOff,
        analogCh1,
        analogCh2,
        analogStereo,
        digitalCh1,
        digitalCh2,
        digitalStereo
    };

    enum KeyLockState {
        locked,
        unlocked
    };

    enum ChapterFrameMode {
        chapter,
        frame
    };

    bool connect(PlayerType playerType, QString serialDevice, SerialSpeed serialSpeed);
    void disconnect(void);
    TrayState getTrayState(void);
    PlayerState getPlayerState(void);
    qint32 getCurrentFrame(void);
    qint32 getCurrentTimeCode(void);
    DiscType getDiscType(void);
    QString getUserCode(void);
    qint32 getMaximumFrameNumber(void);
    qint32 getMaximumTimeCode(void);

    void setTrayState(TrayState trayState);
    void setPlayerState(PlayerState playerState);
    void step(Direction direction);
    void scan(Direction direction);
    void multiSpeed(Direction direction);

    void setPositionFrame(qint32 address);
    void setPositionTimeCode(qint32 address);
    void setPositionChapter(qint32 address);

    void setStopFrame(qint32 frame);
    void setStopTimeCode(qint32 timeCode);

    void setOnScreenDisplay(DisplayState displayState);
    void setAudio(AudioState audioState);
    void setKeyLock(KeyLockState keyLockState);
    void setSpeed(qint32 speed);

signals:

private:
    QSerialPort *serialPort;

    void sendSerialCommand(QString command);
    QString getSerialResponse(qint32 timeoutInSeconds);

public slots:
};

#endif // PLAYERCOMMUNICATION_H
