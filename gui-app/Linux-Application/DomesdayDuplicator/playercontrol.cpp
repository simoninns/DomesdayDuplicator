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
    restart = false;        // True causes connection to player
    reconnect = false;     // True causes disconnection from player
    abort = false;          // True shuts down the thread and exits

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
    QMutexLocker locker(&mutex);

    // Move all the parameters to be local
    this->serialDevice = serialDevice;
    this->serialSpeed = serialSpeed;
    this->playerType = playerType;

    // Is the run process already running?
    if (!isRunning()) {
        // No, start with low priority
        start(LowPriority);
    } else {
        // Yes, set the restart condition
        restart = true;
        reconnect = false;
        condition.wakeOne();
    }
}

// Main thread processing method
void PlayerControl::run()
{
    qDebug() << "PlayerControl::run(): Player control thread has started";

    bool isPlayerConnected = false;

    // Process the player control loop until abort
    while(!abort) {
        // Are we connected to the player?
        if (!isPlayerConnected) {
            // Connect to the player
            if (!playerCommunication->connect(playerType, serialDevice, serialSpeed)) {
                // Connection to player failed
                emit playerControlError(tr("Could not connect to the LaserDisc player!"));
            }
        }

        // If the player is connected, perform processing
        if (isPlayerConnected) {
            // Perform player control processing here
        }

        // If reconnect is flagged, disconnect from the player
        if (reconnect) {
            isPlayerConnected = false;
            playerCommunication->disconnect();
        }

        // Sleep the thread for 100uS
        this->usleep(100);
    }

    qDebug() << "PlayerControl::run(): Player control thread has stopped";
}
