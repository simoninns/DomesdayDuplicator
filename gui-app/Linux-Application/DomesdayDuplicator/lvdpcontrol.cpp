/************************************************************************

    lvdpcontrol.cpp

    QT GUI Capture application for Domesday Duplicator
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

#include "lvdpcontrol.h"

static QSerialPort *lvdpSerialPort;

struct Stimuli {
    QString portName;
    qint16 baudRate;
    bool serialConfigured;
    bool deviceConnected;
    bool isCav;
    bool isPlaying;
    bool isPaused;
    qint32 frameNumber;
    qint32 timeCode;
    QQueue<lvdpControl::PlayerCommands> commandQueue;
    QQueue<quint32> parameterQueue;
    quint32 discLength;
    bool lastCommandError;
};

static Stimuli currentStimuli;

// Define the possible state-machine states
enum States {
    state_disconnected,
    state_connecting,
    state_stopped,
    state_playing,
    state_paused,
    state_serialError
};

static volatile bool stateMachineStopping;
static volatile bool stateMachineRunning;

static void stateMachine(void);

// State machine state handlers
static States smDisconnectedState(void);
static States smConnectingState(void);
static States smStoppedState(void);
static States smPlayingState(void);
static States smPausedState(void);
static States smSerialErrorState(void);

// Serial communication functions
static QString sendSerialCommand(QString commandString, quint64 timeoutMsecs);

// Time-out definitions for serial communications
// 1 second for quick commands (for example status checks)
// 20 seconds for long commands (play, stop, eject, etc.)
#define TOUT_SHORT 1000
#define TOUT_LONG 30000

lvdpControl::lvdpControl()
{
    // Set the stimuli to default
    currentStimuli.serialConfigured = false;
    currentStimuli.deviceConnected = false;
    currentStimuli.isCav = false;
    currentStimuli.isPlaying = false;
    currentStimuli.isPaused = false;
    currentStimuli.frameNumber = 0;
    currentStimuli.timeCode = 0;
    currentStimuli.discLength = 0;
    currentStimuli.lastCommandError = false;

    // Start the player communication state machine
    stateMachineStopping = false;
    stateMachineRunning = true;
    QFuture<void> future = QtConcurrent::run(stateMachine);
}

lvdpControl::~lvdpControl()
{
    stopStateMachine();
}

// Stop the running state machine
void lvdpControl::stopStateMachine(void)
{
    // Stop the player communication state machine
    stateMachineStopping = true;

    qDebug() << "lvdpControl::stopStateMachine(): Waiting for state machine to stop";
    while(stateMachineRunning);
    qDebug() << "lvdpControl::stopStateMachine(): State machine has stopped";
}

// Functions to interact with the state machine ---------------------------------------------------

// Tell the state-machine that the serial port has been configured
void lvdpControl::serialConfigured(QString portName, qint16 baudRate)
{
    qDebug() << "lvdpControl::serialConfigured(): Invoked";
    currentStimuli.portName = portName;
    currentStimuli.baudRate = baudRate;
    currentStimuli.serialConfigured = true;
    currentStimuli.deviceConnected = false;
    currentStimuli.isPlaying = false;
    currentStimuli.isPaused = false;
    currentStimuli.commandQueue.empty();
    currentStimuli.parameterQueue.empty();
    currentStimuli.discLength = 0;
    currentStimuli.lastCommandError = false;
}

// Tell the state-machine that the serial port is unconfigured
void lvdpControl::serialUnconfigured(void)
{
    qDebug() << "lvdpControl::serialUnconfigured(): Invoked";
    currentStimuli.serialConfigured = false;
    currentStimuli.deviceConnected = false;
    currentStimuli.isPlaying = false;
    currentStimuli.isPaused = false;
    currentStimuli.commandQueue.empty();
    currentStimuli.parameterQueue.empty();
    currentStimuli.discLength = 0;
    currentStimuli.lastCommandError = false;
}

// Ask if a player is connected
bool lvdpControl::isConnected(void)
{
    return currentStimuli.deviceConnected;
}

// Ask if a disc is playing
bool lvdpControl::isPlaying(void)
{
    return currentStimuli.isPlaying;
}

// Ask if a disc is paused
bool lvdpControl::isPaused(void)
{
    return currentStimuli.isPaused;
}

// Ask if the disc is CAV
bool lvdpControl::isCav(void)
{
    return currentStimuli.isCav;
}

// Ask for the current frame number (CAV discs only)
quint32 lvdpControl::currentFrameNumber(void)
{
    if (currentStimuli.isCav) return currentStimuli.frameNumber;

    // Not a CAV disc... return 0
    return 0;
}

// Ask for the current time code (CLV discs only)
quint32 lvdpControl::currentTimeCode(void)
{
    if (!currentStimuli.isCav) return currentStimuli.timeCode;

    // Not a CLV disc... return 0
    return 0;
}

// Ask for the current disc length
quint32 lvdpControl::getDiscLength(void)
{
    return currentStimuli.discLength;
}

// Ask if the last command resulted in an error response
bool lvdpControl::isLastCommandError(void)
{
    // Clear the flag once it's returned.
    bool returnValue = currentStimuli.lastCommandError;
    currentStimuli.lastCommandError = false;
    return returnValue;
}

// Send a command to the player
void lvdpControl::command(PlayerCommands command, quint32 parameter)
{
    // Ensure player is connected
    if (!isConnected()) {
        qDebug() << "lvdpControl::command(): Player is not connected";
        return;
    }

    // Add command to the command queue
    currentStimuli.commandQueue.enqueue(command);
    currentStimuli.parameterQueue.enqueue(parameter);
}

// Player communication state machine thread ------------------------------------------------------------
void stateMachine(void)
{
    // Create the serial port object
    lvdpSerialPort = new QSerialPort();

    // Variables to track the current and next states
    States currentState = state_disconnected;
    States nextState = currentState;

    // Process the state machine until stopped
    qDebug() << "stateMachine(): State machine starting";

    while(!stateMachineStopping) {
        // Transition the state machine
        currentState = nextState;

        // Sleep for a little on each loop to conserve CPU
        QThread::msleep(20);

        // State machine process
        switch(currentState) {
            case state_disconnected:
                nextState = smDisconnectedState();

                // Sleep more in the disconnected state
                QThread::msleep(100);
                break;

            case state_connecting:
                nextState = smConnectingState();
                break;

            case state_stopped:
                nextState = smStoppedState();

                // Sleep more in the stopped state
                QThread::msleep(100);
                break;

            case state_playing:
                nextState = smPlayingState();
                break;

            case state_paused:
                nextState = smPausedState();

                // Sleep more in the paused state
                QThread::msleep(100);
                break;

            case state_serialError:
                nextState = smSerialErrorState();
                break;

            default:
                qDebug() << "stateMachine(): State machine in invalid state!";
                nextState = state_disconnected;
        }

        // Are there pending commands in the command queue?
        if(!currentStimuli.commandQueue.isEmpty()) {
            // Queue is not empty

            // Is device connected?
            if (currentStimuli.deviceConnected) {
                // Pop the next command from the queue
                lvdpControl::PlayerCommands currentCommand = currentStimuli.commandQueue.dequeue();
                quint32 currentCommandParameter = currentStimuli.parameterQueue.dequeue();

                // Device connected, send command
                switch(currentCommand) {
                    case lvdpControl::PlayerCommands::command_play:
                        qDebug() << "stateMachine(): Sending play command";
                        sendSerialCommand("PL\r", TOUT_LONG); // Play command
                        break;

                    case lvdpControl::PlayerCommands::command_still:
                        qDebug() << "stateMachine(): Sending still command";
                        sendSerialCommand("ST\r", TOUT_LONG); // Still frame command
                        break;

                    case lvdpControl::PlayerCommands::command_pause:
                        qDebug() << "stateMachine(): Sending pause command";
                        sendSerialCommand("PA\r", TOUT_LONG); // Pause command
                        break;

                    case lvdpControl::PlayerCommands::command_stop:
                        qDebug() << "stateMachine(): Sending reject command";
                        sendSerialCommand("RJ\r", TOUT_LONG); // Reject command
                        break;

                    case lvdpControl::PlayerCommands::command_stepForwards:
                        qDebug() << "stateMachine(): Sending step forwards command";
                        sendSerialCommand("SF\r", TOUT_SHORT); // Step forwards command
                        break;

                    case lvdpControl::PlayerCommands::command_stepBackwards:
                        qDebug() << "stateMachine(): Sending step backwards command";
                        sendSerialCommand("SR\r", TOUT_SHORT); // Step backwards command
                        break;

                    case lvdpControl::PlayerCommands::command_scanForwards:
                        qDebug() << "stateMachine(): Sending scan forward command";
                        sendSerialCommand("NF\r", TOUT_LONG); // Scan forwards command
                        break;

                    case lvdpControl::PlayerCommands::command_scanBackwards:
                        qDebug() << "stateMachine(): Sending scan backwards command";
                        sendSerialCommand("NR\r", TOUT_LONG); // Scan backwards command
                        break;

                    case lvdpControl::PlayerCommands::command_keyLockOn:
                        qDebug() << "stateMachine(): Sending key lock on command";
                        sendSerialCommand("1KL\r", TOUT_SHORT); // Key lock on command
                        break;

                    case lvdpControl::PlayerCommands::command_keyLockOff:
                        qDebug() << "stateMachine(): Sending key lock off command";
                        sendSerialCommand("0KL\r", TOUT_SHORT); // Key lock off command
                        break;

                    case lvdpControl::PlayerCommands::command_seek:
                        if (currentStimuli.isCav) {
                            qDebug() << "stateMachine(): Sending seek command with start =" << currentCommandParameter;

                            QString command;
                            command.sprintf("FR%dSE\r", currentCommandParameter);
                            sendSerialCommand(command, TOUT_LONG); // Frame seek command
                        } else {
                            qDebug() << "stateMachine(): Sending seek command with time-code =" << currentCommandParameter;

                            QString command;
                            command.sprintf("FR%dSE\r", currentCommandParameter);
                            sendSerialCommand(command, TOUT_LONG); // Frame seek command
                        }
                        break;

                   case lvdpControl::PlayerCommands::command_resetDiscLength:
                        // Reset the disc length
                        currentStimuli.discLength = 0;
                        break;

                    case lvdpControl::PlayerCommands::command_getDiscLength:
                        // Reset the disc length
                        currentStimuli.discLength = 0;

                        // Ensure that the player is playing or paused
                        if (currentStimuli.isPlaying || currentStimuli.isPaused) {
                            if (currentStimuli.isCav)
                                sendSerialCommand("FR60000SE\r", TOUT_LONG); // Frame seek to impossible frame number
                            else
                                sendSerialCommand("FR1595900SE\r", TOUT_LONG); // Frame seek to impossible time-code frame number

                            if (currentStimuli.isCav) {
                                // Disc is CAV - get frame number
                                QString response;
                                response = sendSerialCommand("?F\r", TOUT_SHORT); // Frame number request

                                // Process the response (5 digit frame number)
                                currentStimuli.discLength = response.left(5).toUInt();
                            } else {
                                // Disc is CLV - get time code
                                QString response;
                                response = sendSerialCommand("?F\r", TOUT_SHORT); // Time code request

                                // Process the response (7-digit time code number HMMSSFF)
                                currentStimuli.discLength = response.left(7).toUInt();
                            }
                        }
                        break;

                    default:
                        // Drop the pending invalid command
                        qDebug() << "stateMachine(): Dequeued command was invalid!";
                }
            }
        }
    }

    // State machine has stopped
    qDebug() << "stateMachine(): State machine stopped";
    stateMachineRunning = false;
    currentState = state_disconnected;
}

// State machine state handlers -------------------------------------------------------------------
States smDisconnectedState(void)
{
    States nextState = state_disconnected;

    currentStimuli.deviceConnected = false;

    // Has the serial port been configured?
    if (currentStimuli.serialConfigured) {
        qDebug() << "smDisconnectedState(): Serial port is flagged as configured";
        // If the serial port is open, close it
        if (lvdpSerialPort->isOpen()) lvdpSerialPort->close();

        // Verify that the baud rate is valid
        if (currentStimuli.baudRate != 1200 && currentStimuli.baudRate != 2400 && currentStimuli.baudRate != 4800 && currentStimuli.baudRate != 9600) {
            qDebug() << "smDisconnectedState(): Invalid baud rate" << currentStimuli.baudRate;
            currentStimuli.serialConfigured = false;
            return state_disconnected;
        } else {
            qDebug() << "smDisconnectedState(): Baud rate is" << currentStimuli.baudRate;
        }

        // Configure the serial port
        lvdpSerialPort->setPortName(currentStimuli.portName);
        qDebug() << "smDisconnectedState(): Serial port name is" << currentStimuli.portName;

        if (currentStimuli.baudRate == 1200) lvdpSerialPort->setBaudRate(QSerialPort::Baud1200);
        if (currentStimuli.baudRate == 2400) lvdpSerialPort->setBaudRate(QSerialPort::Baud2400);
        if (currentStimuli.baudRate == 4800) lvdpSerialPort->setBaudRate(QSerialPort::Baud4800);
        if (currentStimuli.baudRate == 9600) lvdpSerialPort->setBaudRate(QSerialPort::Baud9600);

        lvdpSerialPort->setDataBits(QSerialPort::Data8);
        lvdpSerialPort->setParity(QSerialPort::NoParity);
        lvdpSerialPort->setStopBits(QSerialPort::OneStop);
        lvdpSerialPort->setFlowControl(QSerialPort::NoFlowControl);

        if (lvdpSerialPort->open(QIODevice::ReadWrite)) {
            qDebug() << "smDisconnectedState(): Serial port opened successfully";
            // Transition to the connecting state
            nextState = state_connecting;
        } else {
            qDebug() << "smDisconnectedState(): Unable to open serial port!";
            // Stay disconnected
            currentStimuli.serialConfigured = false;
            nextState = state_disconnected;
        }
    }

    // Return the next state
    return nextState;
}

States smConnectingState(void)
{
    States nextState = state_disconnected;

    // In this state we verify that we can communicate with the player
    // and that it is the correct type of player
    QString response;
    response = sendSerialCommand("?X\r", TOUT_SHORT); // Identify Pioneer player command

    // Check for response
    if (response.isEmpty()) {
        qDebug() << "smConnectingState(): No response from player";
        nextState = state_serialError;
        return nextState;
    }

    // Check the response
    if (response.contains("P1515") || response.contains("P1537")) {
        // Player identity correct

        // Show identified player type in the debug
        if (response.contains("P1515")) qDebug() << "smConnectingState(): Player ID is P1515xx - Pioneer LD-V4300D";
        if (response.contains("P1537")) qDebug() << "smConnectingState(): Player ID is P1537xx - Pioneer CLD-V2800";

        // Flag that valid player is connected
        currentStimuli.deviceConnected = true;
        nextState = state_stopped;
    } else {
        // Player identity unknown!
        qDebug() << "smConnectingState(): Player ID is unknown - " << response;
        nextState = state_serialError;
    }

    return nextState;
}

States smStoppedState(void)
{
    States nextState = state_stopped;

    // Flag that the player is stopped
    currentStimuli.isPlaying = false;
    currentStimuli.isPaused = false;

    // Check if player is still connected
    if (!currentStimuli.deviceConnected) {
        nextState = state_disconnected;
        return nextState;
    }

    // Player active mode request (what are you doing?)
    QString response;
    response = sendSerialCommand("?P\r", TOUT_SHORT);

    // Check for response
    if (response.isEmpty()) {
        qDebug() << "smStoppedState(): No response from player";
        nextState = state_serialError;
        return nextState;
    }

    // Process response
    if (response.contains("P00")) {
        // P00 = Door open
        nextState = state_stopped;
    } else if (response.contains("P01")) {
        // P01 = Park
        nextState = state_stopped;
    } else if (response.contains("P02")) {
        // P02 = Set up (preparing to play)
        nextState = state_stopped;
    } else if (response.contains("P03")) {
        // P03 = Disc unloading
        nextState = state_stopped;
    } else if (response.contains("P04")) {
        // P04 = Play
        nextState = state_playing;
    } else if (response.contains("P05")) {
        // P05 = Still (picture displayed as still)
        nextState = state_paused;
    } else if (response.contains("P06")) {
        // P06 = Pause (paused without picture display)
        nextState = state_paused;
    } else if (response.contains("P07")) {
        // P07 = Search
        nextState = state_playing;
    } else if (response.contains("P08")) {
        // P08 = Scan
        nextState = state_playing;
    } else if (response.contains("P09")) {
        // P09 = Multi-speed play back
        nextState = state_playing;
    } else if (response.contains("PA5")) {
        // PA5 = Undocumented response, sent at end of disc it seems?
        nextState = state_stopped;
    } else {
        // Unknown response
        qDebug() << "smStoppedState(): Unknown response from player to ?P - " << response;
        nextState = state_stopped;
    }

    // If we are playing or paused; get the details of the disc (CAV or CLV?)
    if (nextState == state_playing || nextState == state_paused) {
        response = sendSerialCommand("?D\r", TOUT_SHORT); // Disc status request

        // Check for response
        if (response.isEmpty()) {
            qDebug() << "smStoppedState(): No response from player";
            nextState = state_serialError;
            return nextState;
        }

        // Process the response
        if (response.at(1) == '0') {
            qDebug() << "smStoppedState(): Disc is CAV";
            currentStimuli.isCav = true;
        } else if (response.at(1) == '1') {
            qDebug() << "smStoppedState(): Disc is CLV";
            currentStimuli.isCav = false;
        } else {
            qDebug() << "smStoppedState(): Unknown disc type";
            nextState = state_serialError;
            return nextState;
        }
    }

    return nextState;
}

States smPlayingState(void)
{
    States nextState = state_disconnected;

    // Flag that the player is playing
    currentStimuli.isPlaying = true;
    currentStimuli.isPaused = false;

    // Check if player is still connected
    if (!currentStimuli.deviceConnected) {
        nextState = state_disconnected;
        return nextState;
    }

    // Player active mode request (what are you doing?)
    QString response;
    response = sendSerialCommand("?P\r", TOUT_SHORT);

    // Check for response
    if (response.isEmpty()) {
        qDebug() << "smPlayingState(): No response from player";
        nextState = state_serialError;
        return nextState;
    }

    // Process response
    if (response.contains("P00")) {
        // P00 = Door open
        nextState = state_stopped;
    } else if (response.contains("P01")) {
        // P01 = Park
        nextState = state_stopped;
    } else if (response.contains("P02")) {
        // P02 = Set up (preparing to play)
        nextState = state_stopped;
    } else if (response.contains("P03")) {
        // P03 = Disc unloading
        nextState = state_stopped;
    } else if (response.contains("P04")) {
        // P04 = Play
        nextState = state_playing;
    } else if (response.contains("P05")) {
        // P05 = Still (picture displayed as still)
        nextState = state_paused;
    } else if (response.contains("P06")) {
        // P06 = Pause (paused without picture display)
        nextState = state_paused;
    } else if (response.contains("P07")) {
        // P07 = Search
        nextState = state_playing;
    } else if (response.contains("P08")) {
        // P08 = Scan
        nextState = state_playing;
    } else if (response.contains("P09")) {
        // P09 = Multi-speed play back
        nextState = state_playing;
    } else if (response.contains("PA5")) {
        // PA5 = Undocumented response, sent at end of disc it seems?
        nextState = state_stopped;
    } else {
        // Unknown response
        qDebug() << "smPlayingState(): Unknown response from player to ?P -" << response;
        nextState = state_playing;
    }

    // If we are playing or paused; get the current picture position
    if (nextState == state_playing || nextState == state_paused) {
        if (currentStimuli.isCav) {
            // Disc is CAV - get frame number
            response = sendSerialCommand("?F\r", TOUT_SHORT); // Frame number request

            // Check for response
            if (response.isEmpty()) {
                qDebug() << "smPlayingState(): No response from player";
                nextState = state_serialError;
                return nextState;
            }

            // Process the response (5 digit frame number)
            currentStimuli.frameNumber = response.left(5).toUInt();
            //qDebug() << "smPlayingState(): Frame:" << currentStimuli.frameNumber;
        } else {
            // Disc is CLV - get time code
            response = sendSerialCommand("?F\r", TOUT_SHORT); // Time code request

            // Check for response
            if (response.isEmpty()) {
                qDebug() << "smPlayingState(): No response from player";
                nextState = state_serialError;
                return nextState;
            }

            // Process the response (7-digit time code number HMMSSFF)
            currentStimuli.timeCode = response.left(7).toUInt();
            //qDebug() << "smPlayingState(): Time code:" << currentStimuli.timeCode;
        }
    }

    return nextState;
}

States smPausedState(void)
{
    States nextState = state_disconnected;

    // Flag that the player is paused
    currentStimuli.isPlaying = false;
    currentStimuli.isPaused = true;

    // Check if player is still connected
    if (!currentStimuli.deviceConnected) {
        nextState = state_disconnected;
        return nextState;
    }

    // Player active mode request (what are you doing?)
    QString response;
    response = sendSerialCommand("?P\r", TOUT_SHORT);

    // Check for response
    if (response.isEmpty()) {
        qDebug() << "smPausedState(): No response from player";
        nextState = state_serialError;
        return nextState;
    }

    // Process response
    if (response.contains("P00")) {
        // P00 = Door open
        nextState = state_stopped;
    } else if (response.contains("P01")) {
        // P01 = Park
        nextState = state_stopped;
    } else if (response.contains("P02")) {
        // P02 = Set up (preparing to play)
        nextState = state_stopped;
    } else if (response.contains("P03")) {
        // P03 = Disc unloading
        nextState = state_stopped;
    } else if (response.contains("P04")) {
        // P04 = Play
        nextState = state_playing;
    } else if (response.contains("P05")) {
        // P05 = Still (picture displayed as still)
        nextState = state_paused;
    } else if (response.contains("P06")) {
        // P06 = Pause (paused without picture display)
        nextState = state_paused;
    } else if (response.contains("P07")) {
        // P07 = Search
        nextState = state_playing;
    } else if (response.contains("P08")) {
        // P08 = Scan
        nextState = state_playing;
    } else if (response.contains("P09")) {
        // P09 = Multi-speed play back
        nextState = state_playing;
    } else if (response.contains("PA5")) {
        // PA5 = Undocumented response, sent at end of disc it seems?
        nextState = state_stopped;
    } else {
        // Unknown response
        qDebug() << "smPausedState(): Unknown response from player to ?P -" << response;
        nextState = state_playing;
    }

    // If we are playing or paused; get the current picture position
    if (nextState == state_playing || nextState == state_paused) {
        if (currentStimuli.isCav) {
            // Disc is CAV - get frame number
            response = sendSerialCommand("?F\r", TOUT_SHORT); // Frame number request

            // Check for response
            if (response.isEmpty()) {
                qDebug() << "smPausedState(): No response from player";
                nextState = state_serialError;
                return nextState;
            }

            // Process the response (5 digit frame number)
            currentStimuli.frameNumber = response.left(5).toUInt();
            //qDebug() << "smPausedState(): Frame:" << currentStimuli.frameNumber;
        } else {
            // Disc is CLV - get time code
            response = sendSerialCommand("?F\r", TOUT_SHORT); // Time code request

            // Check for response
            if (response.isEmpty()) {
                qDebug() << "smPausedState(): No response from player";
                nextState = state_serialError;
                return nextState;
            }

            // Process the response (7-digit time code number HMMSSFF)
            currentStimuli.timeCode = response.left(7).toUInt();
            //qDebug() << "smPausedState(): Time code:" << currentStimuli.timeCode;
        }
    }

    return nextState;
}

States smSerialErrorState(void)
{
    States nextState = state_disconnected;

    // Something went wrong with the serial communication
    // Here we attempt to clean up and return to the disconnected
    // state

    // If the serial port is open, close it
    if (lvdpSerialPort->isOpen()) lvdpSerialPort->close();
    currentStimuli.serialConfigured = false;
    currentStimuli.deviceConnected = false;

    qDebug() << "smSerialErrorState(): In serial error state; closed serial port";

    return nextState;
}

// Serial port interaction functions -----------------------------------------------------------
QString sendSerialCommand(QString commandString, quint64 timeoutMsecs)
{
    QString response;

    // Convert command to QByteArray (note: 20 characters maximum)
    QByteArray data = commandString.toUtf8().left(20);

    // Write the data to the serial port
    //qDebug() << "sendSerialCommand(): Sending command:" << data;
    lvdpSerialPort->write(data);

    // Start the timeout timer
    QElapsedTimer serialTimer;
    serialTimer.start();

    bool commandComplete = false;
    bool commandTimeout = false;
    while (!commandComplete) {
        // Get any pending serial responses
        if (lvdpSerialPort->waitForReadyRead(100)) {
            response.append(lvdpSerialPort->readAll());

            // Check for command response terminator (CR)
            if (response.contains('\r')) {
                commandComplete = true;
                //qDebug() << "sendSerialCommand(): Received response:" << response;

                // Check for command error
                if (response.left(1) == QString("E")) {
                    // Error response received
                    currentStimuli.lastCommandError = true;
                    qDebug() << "sendSerialCommand(): Received error response from player:" << response;
                }
            }
        }

        // Check for timeout
        if ((serialTimer.elapsed() >= (qint64)timeoutMsecs) && (!commandComplete)) {
            commandTimeout = true;
            commandComplete = true;
            currentStimuli.lastCommandError = true; // Flag as error
            qDebug() << "sendSerialCommand(): Command timed out!";
        }
    }

    // If the command timed out, clear the response buffer
    if (commandTimeout) response.clear();

    // Return the response
    return response;
}

