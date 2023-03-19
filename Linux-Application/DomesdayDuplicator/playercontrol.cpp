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

    // Initialise the automatic capture
    acStatus = tr("Idle");
    acInProgress = false;
    acCancelled = false;
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
        PlayerCommunication::SerialSpeed serialSpeed)
{
    QMutexLocker locker(&mutex);

    // Move all the parameters to be local
    this->serialDevice = serialDevice;
    this->serialSpeed = serialSpeed;

    // Make sure the serial device string is not empty
    if (!serialDevice.isEmpty() && !serialDevice.contains("None", Qt::CaseInsensitive)) {
        // Is the run process already running?
        if (!isRunning()) {
            // No, start with low priority
            qDebug() << "PlayerControl::configurePlayerCommunication(): Starting control thread";
            abort = false;
            reconnect = false;
            start(LowPriority);
        } else {
            // Yes, set the restart condition
            qDebug() << "PlayerControl::configurePlayerCommunication(): Peforming reconnection attempt";
            reconnect = true;
            condition.wakeOne();
        }
    } else {
        // Serial is not configured correctly for device connection
        // Ensure the control thread is stopped
        qDebug() << "PlayerControl::configurePlayerCommunication(): No player configured, control thread stopped";
        abort = true;
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
            // Make sure the serial device string is not empty
            // otherwise don't attempt connect to the player
            if (!serialDevice.isEmpty()) {
                // Connect to the player
                if (playerCommunication->connect(serialDevice, serialSpeed)) {
                    // Connection successful
                    isPlayerConnected = true;
                    emit playerConnected();
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

            // Process automatic capture
            processAutomaticCapture();
        }

        // If reconnect is flagged, disconnect from the player
        if (reconnect) {
            isPlayerConnected = false;
            emit playerDisconnected();
            reconnect = false;
            playerCommunication->disconnect();
        }

        // Sleep the thread to save CPU
        if (isPlayerConnected) {
            // Sleep the thread for 100uS
            this->usleep(100);
        } else {
            // Sleep the thread for 0.2 seconds
            this->msleep(200);
        }
    }

    qDebug() << "PlayerControl::run(): Player control thread has stopped";
    if (isPlayerConnected) playerCommunication->disconnect();
    isPlayerConnected = false;
    emit playerDisconnected();
}

void PlayerControl::stop()
{
    qDebug() << "PlayerControl::stop(): Stopping player control thread";
    abort = true;
}

QString PlayerControl::getPlayerModelName()
{
    return playerCommunication->getPlayerName();
}

QString PlayerControl::getPlayerVersionNumber()
{
    return playerCommunication->getPlayerVersionNumber();
}

QString PlayerControl::getSerialBaudRate()
{
    switch (playerCommunication->getSerialSpeed())
    {
    case PlayerCommunication::SerialSpeed::bps1200:
        return "1200";
    case PlayerCommunication::SerialSpeed::bps2400:
        return "2400";
    case PlayerCommunication::SerialSpeed::bps4800:
        return "4800";
    case PlayerCommunication::SerialSpeed::bps9600:
        return "9600";
    case PlayerCommunication::SerialSpeed::autoDetect:
        return ""; // Require to supress compilation warning
    }
    return "";
}

// Returns a string that indicates the player's status
QString PlayerControl::getPlayerStatusInformation()
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

// Returns a string that indicates the player's position (in the disc)
QString PlayerControl::getPlayerPositionInformation()
{
    QString playerPosition;

    if (playerState == PlayerCommunication::PlayerState::stop) {
        playerPosition = "Disc stopped";
    } else {
        if (discType == PlayerCommunication::DiscType::CAV) {
            playerPosition = QString::number(frameNumber);
        }

        if (discType == PlayerCommunication::DiscType::CLV) {
            QString timeCodeString;
            QString hourString;
            QString minuteString;
            QString secondString;

            // Get the full 7 character time-code string
            timeCodeString = QString("%1").arg(timeCode, 7, 10, QChar('0'));

            // Split up the time-code
            hourString = timeCodeString.left(1);
            minuteString = timeCodeString.left(3).right(2);
            secondString = timeCodeString.left(5).right(2);

            // Display time-code (without frame number)
            playerPosition = ("0" + hourString + ":" + minuteString + ":" + secondString);
        }

        if (discType == PlayerCommunication::DiscType::unknownDiscType) {
            playerPosition = "Unknown disc type";
        }
    }

    return playerPosition;
}

// Get the disc type (CAV/CLV/unknown)
PlayerCommunication::DiscType PlayerControl::getDiscType()
{
    return discType;
}

// Process the queued commands ----------------------------------------------------------------------------------------

// This method takes commands and parameters from the queue and passes them to
// the appropriate command processing method

void PlayerControl::processCommandQueue()
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
        case Commands::cmdSetPositionFrame:
            processSetPositionFrame(parameterQueue.dequeue());
            break;
        case Commands::cmdSetPositionTimeCode:
            processSetPositionTimeCode(parameterQueue.dequeue());
            break;
        case Commands::cmdSetPositionChapter:
            processSetPositionChapter(parameterQueue.dequeue());
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
    bool successFlag = false;

    PlayerCommunication::TrayState trayState = PlayerCommunication::TrayState::unknownTrayState;
    if (parameter1 == 0) trayState = PlayerCommunication::TrayState::closed;
    if (parameter1 == 1) trayState = PlayerCommunication::TrayState::open;

    successFlag = playerCommunication->setTrayState(trayState);

    // Check for command processing error
    if (!successFlag) {
        qDebug() << "PlayerControl::processSetTrayState(): Serial command error";
    }
}

void PlayerControl::processSetPlayerState(qint32 parameter1)
{
    bool successFlag = false;

    PlayerCommunication::PlayerState playerState = PlayerCommunication::PlayerState::unknownPlayerState;
    if (parameter1 == 0) playerState = PlayerCommunication::PlayerState::pause;
    if (parameter1 == 1) playerState = PlayerCommunication::PlayerState::play;
    if (parameter1 == 2) playerState = PlayerCommunication::PlayerState::stillFrame;
    if (parameter1 == 3) playerState = PlayerCommunication::PlayerState::stop;

    successFlag = playerCommunication->setPlayerState(playerState);

    // Check for command processing error
    if (!successFlag) {
        qDebug() << "PlayerControl::processSetPlayerState(): Serial command error";
    }
}

void PlayerControl::processStep(qint32 parameter1)
{
    bool successFlag = false;

    if (discType == PlayerCommunication::DiscType::CAV) {
        PlayerCommunication::Direction direction = PlayerCommunication::Direction::forwards;
        if (parameter1 == 0) direction = PlayerCommunication::Direction::backwards;
        if (parameter1 == 1) direction = PlayerCommunication::Direction::forwards;

        successFlag = playerCommunication->step(direction);

        // Check for command processing error
        if (!successFlag) {
            qDebug() << "PlayerControl::processStep(): Serial command error";
        }
    } else {
        qDebug() << "PlayerControl::processStep(): Command only valid for CAV discs";
    }
}

void PlayerControl::processScan(qint32 parameter1)
{
    bool successFlag = false;

    PlayerCommunication::Direction direction = PlayerCommunication::Direction::forwards;
    if (parameter1 == 0) direction = PlayerCommunication::Direction::backwards;
    if (parameter1 == 1) direction = PlayerCommunication::Direction::forwards;

    successFlag = playerCommunication->scan(direction);

    // Check for command processing error
    if (!successFlag) {
        qDebug() << "PlayerControl::processScan(): Serial command error";
    }
}

void PlayerControl::processMultiSpeed(qint32 parameter1)
{
    bool successFlag = false;

    if (discType == PlayerCommunication::DiscType::CAV) {
        PlayerCommunication::Direction direction = PlayerCommunication::Direction::forwards;
        if (parameter1 == 0) direction = PlayerCommunication::Direction::backwards;
        if (parameter1 == 1) direction = PlayerCommunication::Direction::forwards;

        successFlag = playerCommunication->multiSpeed(direction);

        // Check for command processing error
        if (!successFlag) {
            qDebug() << "PlayerControl::processMultiSpeed(): Serial command error";
        }
    } else {
        qDebug() << "PlayerControl::processMultiSpeed(): Command only valid for CAV discs";
    }
}

void PlayerControl::processSetPositionFrame(qint32 parameter1)
{
    bool successFlag = false;

    if (discType == PlayerCommunication::DiscType::CAV) {
        successFlag = playerCommunication->setPositionFrame(parameter1);

        // Check for command processing error
        if (!successFlag) {
            qDebug() << "PlayerControl::processSetPositionFrame(): Serial command error";
        }
    } else {
        qDebug() << "PlayerControl::processSetPositionFrame(): Command only valid for CAV discs";
    }
}

void PlayerControl::processSetPositionTimeCode(qint32 parameter1)
{
    bool successFlag = false;

    if (discType == PlayerCommunication::DiscType::CLV) {
        successFlag = playerCommunication->setPositionTimeCode(parameter1);

        // Check for command processing error
        if (!successFlag) {
            qDebug() << "PlayerControl::processSetPositionTimeCode(): Serial command error";
        }
    } else {
        qDebug() << "PlayerControl::processSetPositionTimeCode(): Command only valid for CLV discs";
    }
}

void PlayerControl::processSetPositionChapter(qint32 parameter1)
{
    bool successFlag = false;

    successFlag = playerCommunication->setPositionChapter(parameter1);

    // Check for command processing error
    if (!successFlag) {
        qDebug() << "PlayerControl::processSetPositionChapter(): Serial command error";
    }
}

void PlayerControl::processSetStopFrame(qint32 parameter1)
{
    bool successFlag = false;

    if (discType == PlayerCommunication::DiscType::CAV) {
        successFlag = playerCommunication->setStopFrame(parameter1);

        // Check for command processing error
        if (!successFlag) {
            qDebug() << "PlayerControl::processSetStopFrame(): Serial command error";
        }
    } else {
        qDebug() << "PlayerControl::processSetStopFrame(): Command only valid for CAV discs";
    }
}

void PlayerControl::processSetStopTimeCode(qint32 parameter1)
{
    bool successFlag = false;

    if (discType == PlayerCommunication::DiscType::CLV) {
        successFlag = playerCommunication->setStopTimeCode(parameter1);

        // Check for command processing error
        if (!successFlag) {
            qDebug() << "PlayerControl::processSetStopTimeCode(): Serial command error";
        }
    } else {
        qDebug() << "PlayerControl::processSetStopTimeCode(): Command only valid for CLV discs";
    }
}

void PlayerControl::processSetOnScreenDisplay(qint32 parameter1)
{
    bool successFlag = false;

    PlayerCommunication::DisplayState displayState = PlayerCommunication::DisplayState::unknownDisplayState;
    if (parameter1 == 0) displayState = PlayerCommunication::DisplayState::off;
    if (parameter1 == 1) displayState = PlayerCommunication::DisplayState::on;

    successFlag = playerCommunication->setOnScreenDisplay(displayState);

    // Check for command processing error
    if (!successFlag) {
        qDebug() << "PlayerControl::processSetOnScreenDisplay(): Serial command error";
    }
}

void PlayerControl::processSetAudio(qint32 parameter1)
{
    bool successFlag = false;

    PlayerCommunication::AudioState audioState = PlayerCommunication::AudioState::digitalStereo;
    if (parameter1 == 0) audioState = PlayerCommunication::AudioState::audioOff;
    if (parameter1 == 1) audioState = PlayerCommunication::AudioState::analogCh1;
    if (parameter1 == 2) audioState = PlayerCommunication::AudioState::analogCh2;
    if (parameter1 == 3) audioState = PlayerCommunication::AudioState::analogStereo;
    if (parameter1 == 4) audioState = PlayerCommunication::AudioState::digitalCh1;
    if (parameter1 == 5) audioState = PlayerCommunication::AudioState::digitalCh2;
    if (parameter1 == 7) audioState = PlayerCommunication::AudioState::digitalStereo;

    successFlag = playerCommunication->setAudio(audioState);

    // Check for command processing error
    if (!successFlag) {
        qDebug() << "PlayerControl::processSetAudio(): Serial command error";
    }
}

void PlayerControl::processSetKeyLock(qint32 parameter1)
{
    bool successFlag = false;

    PlayerCommunication::KeyLockState keyLockState = PlayerCommunication::KeyLockState::unlocked;
    if (parameter1 == 0) keyLockState = PlayerCommunication::KeyLockState::unlocked;
    if (parameter1 == 1) keyLockState = PlayerCommunication::KeyLockState::locked;

    successFlag = playerCommunication->setKeyLock(keyLockState);

    // Check for command processing error
    if (!successFlag) {
        qDebug() << "PlayerControl::processSetKeyLock(): Serial command error";
    }
}

void PlayerControl::processSetSpeed(qint32 parameter1)
{
    bool successFlag = false;

    if (discType == PlayerCommunication::DiscType::CAV) {
        qint32 speed = 60; // x1
        if (parameter1 == 0) speed =  10; // x1/6
        if (parameter1 == 1) speed =  15; // x1/4
        if (parameter1 == 2) speed =  20; // x1/3
        if (parameter1 == 3) speed =  30; // x1/2
        if (parameter1 == 4) speed =  60; // x1
        if (parameter1 == 5) speed = 120; // x2
        if (parameter1 == 6) speed = 180; // x3
        if (parameter1 == 7) speed = 240; // x4

        successFlag = playerCommunication->setSpeed(speed);

        // Check for command processing error
        if (!successFlag) {
            qDebug() << "PlayerControl::processSetSpeed(): Serial command error";
        }
    } else {
        qDebug() << "PlayerControl::processSetSpeed(): Command only valid for CAV discs";
    }
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

void PlayerControl::setPositionFrame(qint32 address)
{
    commandQueue.enqueue(Commands::cmdSetPositionFrame);
    parameterQueue.enqueue(address);
}

void PlayerControl::setPositionTimeCode(qint32 address)
{
    commandQueue.enqueue(Commands::cmdSetPositionTimeCode);
    parameterQueue.enqueue(address);
}

void PlayerControl::setPositionChapter(qint32 address)
{
    commandQueue.enqueue(Commands::cmdSetPositionChapter);
    parameterQueue.enqueue(address);
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

// Automatic capture methods ------------------------------------------------------------------------------------------

// Public method to start an automatic capture
void PlayerControl::startAutomaticCapture(bool fromLeadIn, bool wholeDisc,
                                 qint32 startAddress, qint32 endAddress,
                                 PlayerCommunication::DiscType discType,
                                 bool keyLock)
{
    // Check if automatic capture is already running
    if (acInProgress) {
        qDebug() << "PlayerControl::startAutomaticCapture(): Automatic capture already running!";
        return;
    }

    // Flag that a capture is in progress
    acInProgress = true;
    acCancelled = false;

    // Copy the capture parameters
    acStartAddress = startAddress;
    acEndAddress = endAddress;
    acLastSeenAddress = -1;
    acCaptureFromLeadIn = fromLeadIn;
    acCaptureWholeDisc = wholeDisc;
    acDiscType = discType;
    acKeyLock = keyLock;

    // Set default state
    acCurrentState = ac_start_state;
    acNextState = ac_start_state;

    qDebug() << "PlayerControl::startAutomaticCapture(): Starting auto-capture." <<
                "fromLeadin =" << acCaptureFromLeadIn <<
                "- wholeDisc =" << acCaptureWholeDisc <<
                "- startAddress =" << acStartAddress <<
                "- endAddress =" << acEndAddress <<
                "- discType =" << acDiscType <<
                "- keyLock =" << acKeyLock;
}

// Public method to stop an automatic capture
void PlayerControl::stopAutomaticCapture()
{
    // Check automatic capture is running
    if (!acInProgress) {
        qDebug() << "PlayerControl::stopAutomaticCapture(): Automatic capture not running, ignored.";
        return;
    }

    qDebug() << "PlayerControl::stopAutomaticCapture(): Flagging automatic capture cancelled";

    // Flag that the capture has been cancelled
    acCancelled = true;
}

// Public method to get the current automatic capture status
QString PlayerControl::getAutomaticCaptureStatus()
{
    return acStatus;
}

// Public method to get the automatic capture error
QString PlayerControl::getAutomaticCaptureError()
{
    return acErrorMessage;
}

// Private method for processing automatic capture
void PlayerControl::processAutomaticCapture()
{
    // Is automatic capture running?
    if (acInProgress) {
        acCurrentState = acNextState;

        // Call the state handling method according to the current state
        switch(acCurrentState) {
        case ac_start_state: acNextState = acStateStart();
            break;
        case ac_getLength_state: acNextState = acStateGetLength();
            break;
        case ac_spinDown_state: acNextState = acStateSpinDown();
            break;
        case ac_spinUpWithCapture_state: acNextState = acStateSpinUpWithCapture();
            break;
        case ac_moveToStartPosition_state: acNextState = acStateMoveToStartPosition();
            break;
        case ac_playAndCapture_state: acNextState = acStatePlayAndCapture();
            break;
        case ac_waitForEndAddress_state: acNextState = acStateWaitForEndAddress();
            break;
        case ac_finished_state: acNextState = acStateFinished();
            break;
        case ac_cancelled_state: acNextState = acStateCancelled();
            break;
        case ac_error_state: acNextState = acStateError();
            break;
        }
    }
}

// Automatic capture state machine - ac_start_state
PlayerControl::AcStates PlayerControl::acStateStart()
{
    AcStates nextState = AcStates::ac_start_state;

    // Show the current state in the status
    acStatus = tr("Spinning up player");

    // Check for automatic capture being cancelled
    if (acCancelled) {
        nextState = ac_cancelled_state;
        return nextState;
    }

    // Switch on key-lock if required
    if (acKeyLock) playerCommunication->setKeyLock(PlayerCommunication::KeyLockState::locked);

    // Determine the current player state
    switch (playerState) {
    case PlayerCommunication::PlayerState::pause:
    case PlayerCommunication::PlayerState::play:
    case PlayerCommunication::PlayerState::playWithStopCodesDisabled:
    case PlayerCommunication::PlayerState::stillFrame:
        // Player is all ready spun up - verify the disc type
        if (discType != acDiscType) {
            acErrorMessage = tr("The disc in the player does not match the selected capture option");
            qDebug() << "AC Error:" << acErrorMessage;
            nextState = ac_error_state;
            return nextState;
        }
        // Get the length of the disc
        nextState = ac_getLength_state;
        break;
    case PlayerCommunication::PlayerState::stop:
        // Player is stopped - attempt to spin up
        if (playerCommunication->setPlayerState(PlayerCommunication::PlayerState::play)) {
            // Command successful

            // Check the disc type is correct
            if (playerCommunication->getDiscType() != acDiscType) {
                acErrorMessage = tr("The disc in the player does not match the selected capture option");
                qDebug() << "AC Error:" << acErrorMessage;
                nextState = ac_error_state;
                return nextState;
            }

            // Get the length of the disc
            nextState = ac_getLength_state;
        } else {
            // Command unsuccessful
            acErrorMessage = tr("It was not possible to determine the length of the LaserDisc");
            qDebug() << "AC Error:" << acErrorMessage;
            nextState = ac_error_state;
        }
        break;
    case PlayerCommunication::PlayerState::unknownPlayerState:
        // Player state cannot be determined
        acErrorMessage = tr("The player's state could not be determined - possibly a communication problem");
        qDebug() << "AC Error:" << acErrorMessage;
        nextState = ac_error_state;
    }

    return nextState;
}

// Automatic capture state machine - ac_getLength_state
PlayerControl::AcStates PlayerControl::acStateGetLength()
{
    AcStates nextState = AcStates::ac_start_state;

    // Show the current state in the status
    acStatus = tr("Determining disc length");

    // Check for automatic capture being cancelled
    if (acCancelled) {
        nextState = ac_cancelled_state;
        return nextState;
    }

    // CAV?
    if (acDiscType == PlayerCommunication::DiscType::CAV) {
        // Seek to an impossible CAV frame number
        if (playerCommunication->setPositionFrame(60000)) {
            // Successful, get the current frame number
            qint32 discEndAddress = playerCommunication->getCurrentFrame();
            qDebug() << "PlayerControl::acStateGetLength(): CAV Disc length" << discEndAddress << "frames";

            if (acCaptureWholeDisc) acEndAddress = discEndAddress;
            else if (acEndAddress >= discEndAddress) acEndAddress = discEndAddress;
            qDebug() << "PlayerControl::acStateGetLength(): Required stop frame is" << acEndAddress;

            // If we are capturing the whole disc or from lead in, spin down
            if (acCaptureWholeDisc || acCaptureFromLeadIn) {
                nextState = ac_spinDown_state;
            } else {
                nextState = ac_moveToStartPosition_state;
            }
        } else {
            // Unsuccessful
            acErrorMessage = tr("Could not determine CAV disc length");
            qDebug() << "AC Error:" << acErrorMessage;
            nextState = ac_error_state;
        }
    } else {
        // Seek to an impossible CLV time code
        if (playerCommunication->setPositionTimeCode(1595900)) {
            // Successful, get the current time code
            qint32 discEndAddress = playerCommunication->getCurrentTimeCode();
            qDebug() << "PlayerControl::acStateGetLength(): CLV Disc length" << discEndAddress << "time code";

            if (acCaptureWholeDisc) acEndAddress = discEndAddress;
            else if (acEndAddress >= discEndAddress) acEndAddress = discEndAddress;
            qDebug() << "PlayerControl::acStateGetLength(): Required stop time is" << acEndAddress;

            // If we are capturing the whole disc or from lead in, spin down
            if (acCaptureWholeDisc || acCaptureFromLeadIn) {
                nextState = ac_spinDown_state;
            } else {
                nextState = ac_moveToStartPosition_state;
            }
        } else {
            // Unsuccessful
            acErrorMessage = tr("Could not determine CLV disc length");
            qDebug() << "AC Error:" << acErrorMessage;
            nextState = ac_error_state;
        }
    }

    return nextState;
}

// Automatic capture state machine - ac_spinDown_state
PlayerControl::AcStates PlayerControl::acStateSpinDown()
{
    AcStates nextState = AcStates::ac_spinDown_state;

    // Show the current state in the status
    acStatus = tr("Spinning-down player");

    // Check for automatic capture being cancelled
    if (acCancelled) {
        nextState = ac_cancelled_state;
        return nextState;
    }

    if (playerCommunication->setPlayerState(PlayerCommunication::PlayerState::stop)) {
        // Successful
        nextState = ac_spinUpWithCapture_state;
    } else {
        // Unsuccessful
        acErrorMessage = tr("Could not spin-down disc");
        qDebug() << "AC Error:" << acErrorMessage;
        nextState = ac_error_state;
    }

    return nextState;
}

// Automatic capture state machine - ac_spinUpWithCapture_state
PlayerControl::AcStates PlayerControl::acStateSpinUpWithCapture()
{
    AcStates nextState = AcStates::ac_spinUpWithCapture_state;

    // Show the current state in the status
    acStatus = tr("Starting capture and spinning-up");

    // Check for automatic capture being cancelled
    if (acCancelled) {
        nextState = ac_cancelled_state;
        return nextState;
    }

    // Start the capture
    emit startCapture();

    PlayerCommunication::PlayerState state = PlayerCommunication::PlayerState::play;

    // We don't want the player to pause on stop codes on CAV discs
    if (acDiscType == PlayerCommunication::DiscType::CAV) {
        state = PlayerCommunication::PlayerState::playWithStopCodesDisabled;
    }

    if (playerCommunication->setPlayerState(state)) {
        // Successful
        nextState = ac_waitForEndAddress_state;
    } else {
        // Unsuccessful
        acErrorMessage = tr("Could not spin-up disc");
        qDebug() << "AC Error:" << acErrorMessage;
        nextState = ac_error_state;
    }

    return nextState;
}

// Automatic capture state machine - ac_moveToStartPosition_state
PlayerControl::AcStates PlayerControl::acStateMoveToStartPosition()
{
    AcStates nextState = AcStates::ac_moveToStartPosition_state;

    // Show the current state in the status
    acStatus = tr("Moving to the start position");

    // Check for automatic capture being cancelled
    if (acCancelled) {
        nextState = ac_cancelled_state;
        return nextState;
    }

    // The player should not be stopped here, so we can move to the start address
    // and pause/still-frame
    if (acDiscType == PlayerCommunication::DiscType::CAV) {
        if (playerCommunication->setPositionFrame(acStartAddress)) {
            // Successful
            nextState = ac_playAndCapture_state;
        } else {
            // Unsuccessful
            acErrorMessage = tr("Could not position player on start frame");
            qDebug() << "AC Error:" << acErrorMessage;
            nextState = ac_error_state;
        }
    } else {
        if (playerCommunication->setPositionTimeCode(acStartAddress)) {
            // Successful
            nextState = ac_playAndCapture_state;
        } else {
            // Unsuccessful
            acErrorMessage = tr("Could not position player on start frame");
            qDebug() << "AC Error:" << acErrorMessage;
            nextState = ac_error_state;
        }
    }

    return nextState;
}

// Automatic capture state machine - ac_playAndCapture_state
PlayerControl::AcStates PlayerControl::acStatePlayAndCapture()
{
    AcStates nextState = AcStates::ac_playAndCapture_state;

    // Show the current state in the status
    acStatus = tr("Playing disc with capture");

    // Check for automatic capture being cancelled
    if (acCancelled) {
        nextState = ac_cancelled_state;
        return nextState;
    }

    // Start capture
    emit startCapture();

    // Start playing the disc
    if (playerCommunication->setPlayerState(PlayerCommunication::PlayerState::play)) {
        // Successful
        nextState = ac_waitForEndAddress_state;
    } else {
        // Unsuccessful
        acErrorMessage = tr("Could not start playing");
        qDebug() << "AC Error:" << acErrorMessage;
        nextState = ac_error_state;
    }

    // Remain in the error state
    return nextState;
}

// Automatic capture state machine - ac_waitForEndAddress_state
PlayerControl::AcStates PlayerControl::acStateWaitForEndAddress()
{
    AcStates nextState = AcStates::ac_waitForEndAddress_state;

    // Show the current state in the status
    acStatus = tr("Waiting for end address");

    // Check for automatic capture being cancelled
    if (acCancelled) {
        nextState = ac_cancelled_state;
        return nextState;
    }

    // Due to stop codes on CAVs it's possible the disc will still-frame
    // during capture.  Here we check for that and start the disc playing
    // if it occurs
    if (acDiscType == PlayerCommunication::DiscType::CAV) {
        if (playerCommunication->getPlayerState() == PlayerCommunication::PlayerState::stillFrame) {
            qDebug() << "PlayerControl::acStateWaitForEndAddress(): The player put itself in still-frame; sending play command";
            playerCommunication->setPlayerState(PlayerCommunication::PlayerState::play);

            // Continue in the present state
            return nextState;
        }
    }

    if (acDiscType == PlayerCommunication::DiscType::CAV) {
        qint32 currentAddress = playerCommunication->getCurrentFrame();
        if (currentAddress >= acEndAddress) {
            // Target frame number reached
            emit stopCapture();

            // Stop the disc once capture has concluded (for unattended capturing)
            playerCommunication->setPlayerState(PlayerCommunication::PlayerState::stop);

            nextState = ac_finished_state;
        }

        if ((acLastSeenAddress - 1000) > currentAddress) {
            // Somehow the player has gone backwards; probably due to restarting playing after
            // reaching the end of a disc
            qDebug() << "PlayerControl::acStateWaitForEndAddress(): Error - The CAV disc address has stepped backwards more than 1000 frames: Last address =" << acLastSeenAddress <<
                        "current address =" << currentAddress << "- is auto-replay on? Stopping capture";
            acErrorMessage = tr("CAV disc frames were not sequential - player skipped back more than 1000 frames during capture");
            emit stopCapture();

            // Still-frame the player
            playerCommunication->setPlayerState(PlayerCommunication::PlayerState::stillFrame);

            nextState = ac_error_state;
        }
        acLastSeenAddress = currentAddress;
    } else {
        qint32 currentAddress = playerCommunication->getCurrentTimeCode();
        if (playerCommunication->getCurrentTimeCode() >= acEndAddress) {
            // Target time code reached
            emit stopCapture();

            // Pause the player
            playerCommunication->setPlayerState(PlayerCommunication::PlayerState::pause);

            nextState = ac_finished_state;
        }

        if ((acLastSeenAddress - 1000) > currentAddress) {
            // Somehow the player has gone backwards; probably due to restarting playing after
            // reaching the end of a disc
            qDebug() << "PlayerControl::acStateWaitForEndAddress(): Error - The CLV disc address has stepped backwards more than 1000 frames: Last address =" << acLastSeenAddress <<
                        "current address =" << currentAddress << "- is auto-replay on? Stopping capture";
            acErrorMessage = tr("CLV disc frames were not sequential - player skipped back more than 1000 frames during capture");
            emit stopCapture();

            // Still-frame the player
            playerCommunication->setPlayerState(PlayerCommunication::PlayerState::stillFrame);

            nextState = ac_error_state;
        }
        acLastSeenAddress = currentAddress;
    }

    // Remain in the wait For end address state
    return nextState;
}

// Automatic capture state machine - ac_finished_state
PlayerControl::AcStates PlayerControl::acStateFinished()
{
    AcStates nextState = AcStates::ac_finished_state;

    // Show the current state in the status
    acStatus = tr("Finished");
    qDebug() << "PlayerControl::acStateFinished(): Auto-capture complete";

    // Switch off key-lock if required
    if (acKeyLock) playerCommunication->setKeyLock(PlayerCommunication::KeyLockState::unlocked);

    // Cancel the capture
    acInProgress = false;

    // Send completion message with successful status
    emit automaticCaptureComplete(true);

    // Remain in the finished state
    return nextState;
}

// Automatic capture state machine - ac_cancelled_state
PlayerControl::AcStates PlayerControl::acStateCancelled()
{
    AcStates nextState = AcStates::ac_cancelled_state;

    // Show the current state in the status
    acStatus = tr("Cancelled");
    qDebug() << "PlayerControl::acStateFinished(): Auto-capture cancelled";

    // Switch off key-lock if required
    if (acKeyLock) playerCommunication->setKeyLock(PlayerCommunication::KeyLockState::unlocked);

    // Cancel the capture
    emit stopCapture();
    acInProgress = false;

    // Send completion message with successful status
    emit automaticCaptureComplete(true);

    // Stop the player (if it is not already stopped)
    if (playerCommunication->getPlayerState() != PlayerCommunication::PlayerState::stop)
        playerCommunication->setPlayerState(PlayerCommunication::PlayerState::stop);

    // Remain in the cancelled state
    return nextState;
}

// Automatic capture state machine - ac_error_state
PlayerControl::AcStates PlayerControl::acStateError()
{
    AcStates nextState = AcStates::ac_error_state;

    // Show the current state in the status
    acStatus = tr("Error");
    qDebug() << "PlayerControl::acStateFinished(): Auto-capture ended in error";

    // Switch off key-lock if required
    if (acKeyLock) playerCommunication->setKeyLock(PlayerCommunication::KeyLockState::unlocked);

    // Cancel the capture
    acInProgress = false;

    // Stop the capture
    emit stopCapture();

    // Send completion message with unsuccessful status
    emit automaticCaptureComplete(false);

    // Stop the player (if it is not already stopped)
    if (playerCommunication->getPlayerState() != PlayerCommunication::PlayerState::stop)
        playerCommunication->setPlayerState(PlayerCommunication::PlayerState::stop);

    // Remain in the error state
    return nextState;
}









