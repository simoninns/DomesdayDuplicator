/************************************************************************

    playercommunication.h

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

#ifndef PLAYERCOMMUNICATION_H
#define PLAYERCOMMUNICATION_H

#include <QObject>
#include <QtSerialPort/QSerialPort>
#include <QDebug>
#include <QTimer>
#include <QTimerEvent>
#include <QElapsedTimer>
#include <string>

class PlayerCommunication : public QObject
{
    Q_OBJECT
public:
    explicit PlayerCommunication(QObject *parent = nullptr);
    ~PlayerCommunication() override;

    enum SerialSpeed {
        autoDetect,
        bps9600,
        bps4800,
        bps2400,
        bps1200
    };

    enum PlayerType {
        none,               // No defined player model
        pioneerCLDV5000,    // Pioneer CLD-V5000 (ID code 42)
        pioneerCLDV2800,    // Pioneer CLD-V2800 (ID code 37)
        pioneerCLDV2600,    // Pioneer CLD-V2600 (ID code 27)
        pioneerCLDV2400,    // Pioneer CLD-V2400 (ID code 18)
        pioneerLDV4400,     // Pioneer LD-V4400  (ID code 16)
        pioneerLDV4300D,    // Pioneer LD-V4300D (ID code 15)
        pioneerLDV2200,     // Pioneer LD-V2200  (ID code 07)
        pioneerLDV8000,     // Pioneer LD-V8000  (ID code 06)
        pioneerLCV330,      // Pioneer VC-V330   (ID code 05)
        pioneerLDV4200,     // Pioneer LD-V4200  (ID code 02)
        unknownPlayerType,  // Unknown but compatible player model
    };

    enum PlayerState {
        unknownPlayerState,
        stop,
        play,
        playWithStopCodesDisabled,
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

    bool connect(QString serialDevice, SerialSpeed serialSpeed);
    void disconnect();
    PlayerType getPlayerType();
    QString getPlayerName();
    QString getPlayerVersionNumber();
    SerialSpeed getSerialSpeed();
    TrayState getTrayState();
    PlayerState getPlayerState();
    qint32 getCurrentFrame();
    qint32 getCurrentTimeCode();
    DiscType getDiscType();
    QString getUserCode();
    qint32 getMaximumFrameNumber();
    qint32 getMaximumTimeCode();

    bool setTrayState(TrayState trayState);
    bool setPlayerState(PlayerState playerState);
    bool step(Direction direction);
    bool scan(Direction direction);
    bool multiSpeed(Direction direction);

    bool setPositionFrame(qint32 address);
    bool setPositionTimeCode(qint32 address);
    bool setPositionChapter(qint32 address);

    bool setStopFrame(qint32 frame);
    bool setStopTimeCode(qint32 timeCode);

    bool setOnScreenDisplay(DisplayState displayState);
    bool setAudio(AudioState audioState);
    bool setKeyLock(KeyLockState keyLockState);
    bool setSpeed(qint32 speed);

signals:

private:
    PlayerType playerCodeToType(const QString &playerCode) const;
    QString playerCodeToName(const QString& playerCode) const;

private:
    QSerialPort *serialPort;
    SerialSpeed currentSerialSpeed;
    PlayerType currentPlayerType;
    QString currentPlayerName;
    QString currentPlayerVersionNumber;

    void sendSerialCommand(QString command);
    QString getSerialResponse(qint32 timeoutInMilliseconds);

public slots:
};

#endif // PLAYERCOMMUNICATION_H
