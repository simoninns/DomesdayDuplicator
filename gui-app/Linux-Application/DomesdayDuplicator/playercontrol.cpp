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

#include "playercontrol.h"

PlayerControl::PlayerControl(QObject *parent) : QThread(parent)
{
    // Thread control variables
    reconnect = false;     // True causes disconnection from player
    abort = false;          // True shuts down the thread and exits

    // Player tracking variables
    isPlayerConnected = false;
    playerState = PlayerCommunication::PlayerState::unknownPlayerState;
    discType = PlayerCommunication::DiscType::unknownDiscType;
    timeCode = 0;
    frameNumber = 0;

    // Initialise the player communication object
    playerCommunication = new PlayerCommunication();
}

PlayerControl::~PlayerControl()
{
    mutex.lock();
    abort = true; // Cause the thread processing to abort
    condition.wakeOne();
    mutex.unlock();

    wait();
}

// Method to set the player connection parameters
void PlayerControl::configurePlayerCommunication(
        QString serialDevice,
        PlayerCommunication::SerialSpeed serialSpeed,
        PlayerCommunication::PlayerType playerType)
{
    qDebug() << "PlayerControl::configurePlayerCommunication(): Called";
    QMutexLocker locker(&mutex);

    // Move all the parameters to be local
    this->serialDevice = serialDevice;
    this->serialSpeed = serialSpeed;
    this->playerType = playerType;

    // Make sure the player type is set and the serial device string is not empty
    if (playerType != PlayerCommunication::PlayerType::unknownPlayerType &&
            !serialDevice.isEmpty()) {

    }

    // Is the run process already running?
    if (!isRunning()) {
        // No, start with low priority
        qDebug() << "PlayerControl::configurePlayerCommunication(): Starting control thread";
        reconnect = false;
        start(LowPriority);
    } else {
        // Yes, set the restart condition
        qDebug() << "PlayerControl::configurePlayerCommunication(): Peforming reconnection attempt";
        reconnect = true;
        condition.wakeOne();
    }
}

// Main thread processing method
void PlayerControl::run()
{
    qDebug() << "PlayerControl::run(): Player control thread has started";

    isPlayerConnected = false;
    playerState = PlayerCommunication::PlayerState::unknownPlayerState;
    discType = PlayerCommunication::DiscType::unknownDiscType;
    timeCode = 0;
    frameNumber = 0;

    // Process the player control loop until abort
    while(!abort) {
        // Are we connected to the player?
        if (!isPlayerConnected && reconnect == false) {
            // Make sure the player type is set and the serial device string is not empty
            // otherwise don't attempt connect to the player
            if (playerType != PlayerCommunication::PlayerType::unknownPlayerType &&
                    !serialDevice.isEmpty()) {
                // Connect to the player
                if (playerCommunication->connect(playerType, serialDevice, serialSpeed)) {
                    // Connection successful
                    isPlayerConnected = true;
                }
            }
        }

        // If the player is connected, perform processing
        if (isPlayerConnected) {
            // Perform player control processing here

            // Get the player status
            playerState = playerCommunication->getPlayerState();

            // If we get an unknown state from the player, attempt to reconnect
            if (playerState == PlayerCommunication::PlayerState::unknownPlayerState) reconnect = true;

            // Get the disc type
            discType = playerCommunication->getDiscType();

            // Check we are in a valid state to read the frame/timecode
            if (playerState == PlayerCommunication::PlayerState::pause ||
                    playerState == PlayerCommunication::PlayerState::play ||
                    playerState == PlayerCommunication::PlayerState::stillFrame) {

                // Get the frame or time code
                if (discType == PlayerCommunication::DiscType::CAV) {
                    frameNumber = playerCommunication->getCurrentFrame();

                    if (frameNumber == -1) {
                        qDebug() << "PlayerControl::run(): Lost communication with player";
                        reconnect = true;
                    }
                }

                if (discType == PlayerCommunication::DiscType::CLV) {
                    timeCode = playerCommunication->getCurrentTimeCode();

                    if (timeCode == -1) {
                        qDebug() << "PlayerControl::run(): Lost communication with player";
                        reconnect = true;
                    }
                }
            }
        }

        // If reconnect is flagged, disconnect from the player
        if (reconnect) {
            isPlayerConnected = false;
            reconnect = false;
            playerCommunication->disconnect();
        }

        // Sleep the thread to save CPU
        if (isPlayerConnected) {
            // Sleep the thread for 100uS
            this->usleep(100);
        } else {
            // Sleep the thread for 1 second
            this->msleep(1000);
        }
    }

    qDebug() << "PlayerControl::run(): Player control thread has stopped";
}

// Returns a string that indicated the player's status
QString PlayerControl::getPlayerStatusInformation(void)
{
    QString status;

    if (isPlayerConnected){
        if (discType == PlayerCommunication::DiscType::CAV) status += "CAV ";
        if (discType == PlayerCommunication::DiscType::CLV) status += "CLV ";

        if (playerState == PlayerCommunication::PlayerState::stop) status += "Stopped";
        if (playerState == PlayerCommunication::PlayerState::pause) status += "Paused";
        if (playerState == PlayerCommunication::PlayerState::stillFrame) status += "Still-Frame";
        if (playerState == PlayerCommunication::PlayerState::play) status += "Playing";
    } else {
        status = "No player connected (serial)";
    }

    return status;
}

// Returns a string that indicated the player's position (in the disc)
QString PlayerControl::getPlayerPositionInformation(void)
{
    QString playerPosition;

    if (playerState == PlayerCommunication::PlayerState::stop) {
        playerPosition = "No disc playing";
    } else {
        if (discType == PlayerCommunication::DiscType::CAV) {
            playerPosition = QString::number(frameNumber);
        }

        if (discType == PlayerCommunication::DiscType::CLV) {
            playerPosition = QString::number(timeCode);
        }

        if (discType == PlayerCommunication::DiscType::unknownDiscType) {
            playerPosition = "Unknown disc type";
        }
    }

    return playerPosition;
}

// Player command public methods --------------------------------------------------------------------------------------

void PlayerControl::setTrayState(PlayerCommunication::TrayState trayState)
{

}

void PlayerControl::setPlayerState(PlayerCommunication::PlayerState playerState)
{

}

void PlayerControl::step(PlayerCommunication::Direction direction)
{

}

void PlayerControl::scan(PlayerCommunication::Direction direction)
{

}

void PlayerControl::mutliSpeed(PlayerCommunication::Direction direction, qint32 speed)
{

}

void PlayerControl::setFramePosition(qint32 frame)
{

}

void PlayerControl::setTimeCodePosition(qint32 timeCode)
{

}

void PlayerControl::setStopFrame(qint32 frame)
{

}

void PlayerControl::setStopTimeCode(qint32 timeCode)
{

}

void PlayerControl::setOnScreenDisplay(PlayerCommunication::DisplayState displayState)
{

}

void PlayerControl::setAudio(PlayerCommunication::AudioState audioState)
{

}

void PlayerControl::setKeyLock(PlayerCommunication::KeyLockState keyLockState)
{

}
