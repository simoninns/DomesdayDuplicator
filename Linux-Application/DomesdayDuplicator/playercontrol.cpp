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

            // Process the command queue
            processCommandQueue();
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

// Process the queued commands ----------------------------------------------------------------------------------------

// This method takes commands and parameters from the queue and passes them to
// the appropriate command processing method

void PlayerControl::processCommandQueue(void)
{
    // If the queue has commands in it, process one
    if (!commandQueue.isEmpty()) {
        switch(commandQueue.dequeue()) {
        case Commands::cmdSetTrayState:
            processSetTrayState(parameterQueue.dequeue());
            break;
        case Commands::cmdSetPlayerState:
            processSetPlayerState(parameterQueue.dequeue());
            break;
        case Commands::cmdStep:
            processStep(parameterQueue.dequeue());
            break;
        case Commands::cmdScan:
            processScan(parameterQueue.dequeue());
            break;
        case Commands::cmdMultiSpeed:
            processMultiSpeed(parameterQueue.dequeue());
            break;
        case Commands::cmdSetFramePosition:
            processSetFramePosition(parameterQueue.dequeue());
            break;
        case Commands::cmdSetTimeCodePosition:
            processSetTimeCodePosition(parameterQueue.dequeue());
            break;
        case Commands::cmdSetStopFrame:
            processSetStopFrame(parameterQueue.dequeue());
            break;
        case Commands::cmdSetStopTimeCode:
            processSetStopTimeCode(parameterQueue.dequeue());
            break;
        case Commands::cmdSetOnScreenDisplay:
            processSetOnScreenDisplay(parameterQueue.dequeue());
            break;
        case Commands::cmdSetAudio:
            processSetAudio(parameterQueue.dequeue());
            break;
        case Commands::cmdSetKeyLock:
            processSetKeyLock(parameterQueue.dequeue());
            break;
        case Commands::cmdSetSpeed:
            processSetSpeed(parameterQueue.dequeue());
            break;
        }
    }
}

// Command processing methods -----------------------------------------------------------------------------------------

// These private methods send the command to the player communication object

void PlayerControl::processSetTrayState(qint32 parameter1)
{
    PlayerCommunication::TrayState trayState = PlayerCommunication::TrayState::unknownTrayState;
    if (parameter1 == 0) trayState = PlayerCommunication::TrayState::closed;
    if (parameter1 == 1) trayState = PlayerCommunication::TrayState::open;

    playerCommunication->setTrayState(trayState);
}

void PlayerControl::processSetPlayerState(qint32 parameter1)
{
    PlayerCommunication::PlayerState playerState = PlayerCommunication::PlayerState::unknownPlayerState;
    if (parameter1 == 0) playerState = PlayerCommunication::PlayerState::pause;
    if (parameter1 == 1) playerState = PlayerCommunication::PlayerState::play;
    if (parameter1 == 2) playerState = PlayerCommunication::PlayerState::stillFrame;
    if (parameter1 == 3) playerState = PlayerCommunication::PlayerState::stop;

    playerCommunication->setPlayerState(playerState);
}

void PlayerControl::processStep(qint32 parameter1)
{
    PlayerCommunication::Direction direction = PlayerCommunication::Direction::forwards;
    if (parameter1 == 0) direction = PlayerCommunication::Direction::backwards;
    if (parameter1 == 1) direction = PlayerCommunication::Direction::forwards;

    playerCommunication->step(direction);
}

void PlayerControl::processScan(qint32 parameter1)
{
    PlayerCommunication::Direction direction = PlayerCommunication::Direction::forwards;
    if (parameter1 == 0) direction = PlayerCommunication::Direction::backwards;
    if (parameter1 == 1) direction = PlayerCommunication::Direction::forwards;

    playerCommunication->scan(direction);
}

void PlayerControl::processMultiSpeed(qint32 parameter1)
{
    PlayerCommunication::Direction direction = PlayerCommunication::Direction::forwards;
    if (parameter1 == 0) direction = PlayerCommunication::Direction::backwards;
    if (parameter1 == 1) direction = PlayerCommunication::Direction::forwards;

    playerCommunication->multiSpeed(direction);
}

void PlayerControl::processSetFramePosition(qint32 parameter1)
{
    playerCommunication->setFramePosition(parameter1);
}

void PlayerControl::processSetTimeCodePosition(qint32 parameter1)
{
    playerCommunication->setTimeCodePosition(parameter1);
}

void PlayerControl::processSetStopFrame(qint32 parameter1)
{
    playerCommunication->setStopFrame(parameter1);
}

void PlayerControl::processSetStopTimeCode(qint32 parameter1)
{
    playerCommunication->setStopTimeCode(parameter1);
}

void PlayerControl::processSetOnScreenDisplay(qint32 parameter1)
{
    PlayerCommunication::DisplayState displayState = PlayerCommunication::DisplayState::unknownDisplayState;
    if (parameter1 == 0) displayState = PlayerCommunication::DisplayState::off;
    if (parameter1 == 1) displayState = PlayerCommunication::DisplayState::on;

    playerCommunication->setOnScreenDisplay(displayState);
}

void PlayerControl::processSetAudio(qint32 parameter1)
{
    PlayerCommunication::AudioState audioState = PlayerCommunication::AudioState::digitalStereo;
    if (parameter1 == 0) audioState = PlayerCommunication::AudioState::audioOff;
    if (parameter1 == 1) audioState = PlayerCommunication::AudioState::analogCh1;
    if (parameter1 == 2) audioState = PlayerCommunication::AudioState::analogCh2;
    if (parameter1 == 3) audioState = PlayerCommunication::AudioState::analogStereo;
    if (parameter1 == 4) audioState = PlayerCommunication::AudioState::digitalCh1;
    if (parameter1 == 5) audioState = PlayerCommunication::AudioState::digitalCh2;
    if (parameter1 == 7) audioState = PlayerCommunication::AudioState::digitalStereo;

    playerCommunication->setAudio(audioState);
}

void PlayerControl::processSetKeyLock(qint32 parameter1)
{
    PlayerCommunication::KeyLockState keyLockState = PlayerCommunication::KeyLockState::unlocked;
    if (parameter1 == 0) keyLockState = PlayerCommunication::KeyLockState::unlocked;
    if (parameter1 == 1) keyLockState = PlayerCommunication::KeyLockState::locked;

    playerCommunication->setKeyLock(keyLockState);
}

void PlayerControl::processSetSpeed(qint32 parameter1)
{
    qint32 speed = 60; // x1
    if (parameter1 == 0) speed =  10; // x1/6
    if (parameter1 == 1) speed =  15; // x1/4
    if (parameter1 == 2) speed =  20; // x1/3
    if (parameter1 == 3) speed =  30; // x1/2
    if (parameter1 == 4) speed =  60; // x1
    if (parameter1 == 5) speed = 120; // x2
    if (parameter1 == 6) speed = 180; // x3
    if (parameter1 == 7) speed = 240; // x4

    playerCommunication->setSpeed(speed);
}

// Player command public methods --------------------------------------------------------------------------------------

// These public methods queue commands for processing.  For queue logic simplicity, parameters are converted into
// integer values (which must match with the command processing logic at the other end of the queue)

void PlayerControl::setTrayState(PlayerCommunication::TrayState trayState)
{
    commandQueue.enqueue(Commands::cmdSetTrayState);
    if (trayState == PlayerCommunication::TrayState::closed) parameterQueue.enqueue(0);
    if (trayState == PlayerCommunication::TrayState::open) parameterQueue.enqueue(1);
    if (trayState == PlayerCommunication::TrayState::unknownTrayState) parameterQueue.enqueue(0);
}

void PlayerControl::setPlayerState(PlayerCommunication::PlayerState playerState)
{
    commandQueue.enqueue(Commands::cmdSetPlayerState);
    if (playerState == PlayerCommunication::PlayerState::pause) parameterQueue.enqueue(0);
    if (playerState == PlayerCommunication::PlayerState::play) parameterQueue.enqueue(1);
    if (playerState == PlayerCommunication::PlayerState::stillFrame) parameterQueue.enqueue(2);
    if (playerState == PlayerCommunication::PlayerState::stop) parameterQueue.enqueue(3);
    if (playerState == PlayerCommunication::PlayerState::unknownPlayerState) parameterQueue.enqueue(0);
}

void PlayerControl::step(PlayerCommunication::Direction direction)
{
    commandQueue.enqueue(Commands::cmdStep);
    if (direction == PlayerCommunication::Direction::backwards) parameterQueue.enqueue(0);
    if (direction == PlayerCommunication::Direction::forwards) parameterQueue.enqueue(1);
}

void PlayerControl::scan(PlayerCommunication::Direction direction)
{
    commandQueue.enqueue(Commands::cmdScan);
    if (direction == PlayerCommunication::Direction::backwards) parameterQueue.enqueue(0);
    if (direction == PlayerCommunication::Direction::forwards) parameterQueue.enqueue(1);
}

void PlayerControl::multiSpeed(PlayerCommunication::Direction direction)
{
    commandQueue.enqueue(Commands::cmdMultiSpeed);
    if (direction == PlayerCommunication::Direction::backwards) parameterQueue.enqueue(0);
    if (direction == PlayerCommunication::Direction::forwards) parameterQueue.enqueue(1);
}

void PlayerControl::setFramePosition(qint32 frame)
{
    commandQueue.enqueue(Commands::cmdSetFramePosition);
    parameterQueue.enqueue(frame);
}

void PlayerControl::setTimeCodePosition(qint32 timeCode)
{
    commandQueue.enqueue(Commands::cmdSetTimeCodePosition);
    parameterQueue.enqueue(timeCode);
}

void PlayerControl::setStopFrame(qint32 frame)
{
    commandQueue.enqueue(Commands::cmdSetStopFrame);
    parameterQueue.enqueue(frame);
}

void PlayerControl::setStopTimeCode(qint32 timeCode)
{
    commandQueue.enqueue(Commands::cmdSetStopTimeCode);
    parameterQueue.enqueue(timeCode);
}

void PlayerControl::setOnScreenDisplay(PlayerCommunication::DisplayState displayState)
{
    commandQueue.enqueue(Commands::cmdSetOnScreenDisplay);
    if (displayState == PlayerCommunication::DisplayState::off) parameterQueue.enqueue(0);
    if (displayState == PlayerCommunication::DisplayState::on) parameterQueue.enqueue(1);
    if (displayState == PlayerCommunication::DisplayState::unknownDisplayState) parameterQueue.enqueue(0);
}

void PlayerControl::setAudio(PlayerCommunication::AudioState audioState)
{
    commandQueue.enqueue(Commands::cmdSetAudio);
    if (audioState == PlayerCommunication::AudioState::audioOff) parameterQueue.enqueue(0);
    if (audioState == PlayerCommunication::AudioState::analogCh1) parameterQueue.enqueue(1);
    if (audioState == PlayerCommunication::AudioState::analogCh2) parameterQueue.enqueue(2);
    if (audioState == PlayerCommunication::AudioState::analogStereo) parameterQueue.enqueue(3);
    if (audioState == PlayerCommunication::AudioState::digitalCh1) parameterQueue.enqueue(4);
    if (audioState == PlayerCommunication::AudioState::digitalCh2) parameterQueue.enqueue(5);
    if (audioState == PlayerCommunication::AudioState::digitalStereo) parameterQueue.enqueue(6);
}

void PlayerControl::setKeyLock(PlayerCommunication::KeyLockState keyLockState)
{
    commandQueue.enqueue(Commands::cmdSetKeyLock);
    if (keyLockState == PlayerCommunication::KeyLockState::locked) parameterQueue.enqueue(0);
    if (keyLockState == PlayerCommunication::KeyLockState::unlocked) parameterQueue.enqueue(1);
}

void PlayerControl::setSpeed(qint32 speed)
{
    commandQueue.enqueue(Commands::cmdSetSpeed);
    parameterQueue.enqueue(speed);
}
