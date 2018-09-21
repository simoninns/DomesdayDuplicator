/************************************************************************

    playercommunication.cpp

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

#include "playercommunication.h"

// The timeout for normal commands is 5 seconds (these are commands that
// they player usually responds to quickly, like framecode request).
// The timeout for long commands is 30 seconds (there are commands like
// play and open tray).
#define N_TIMEOUT 5
#define L_TIMEOUT 30

PlayerCommunication::PlayerCommunication(QObject *parent) : QObject(parent)
{
}

PlayerCommunication::~PlayerCommunication()
{
}

// Supported player commands:
//
// connect(playerType) - Connect to a player
// disconnect(playerType) - Disconnect from a player
//
// getTrayState() - Returns open or closed
// getPlayerState() - Returns stopped, playing, paused or still frame
// getCurrentFrame() - Returns the current frame number
// getCurrentTimeCode() - Returns the current time code
// getDiscType() - Returns the disc type as unknown, CAV or CLV
// getUserCode() - Returns the user-code for the disc
// getMaximumFrameNumber() - Returns the number of frames on the disc
// getMaximumTimeCode() - Returns the length of the disc
//
// setTrayState(state) - Control the disc tray (open or closed)
// setPlayerState(state) - Sets player to stop, play, pause or still frame
//
// step(dir) - Step the player forwards/backwards 1 frame
// scan(dir) - Step the player forwards/backwards many frames
// multiSpeed(dir, speed) - Play forwards/backwards at non-x1 speeds
//
// setFramePosition(frameno) - Set the frame position to frameno
// setTimeCodePosition(timecode) Set the timecode position to timecode
//
// setStopFrame(frameno) - Set the stop register (frames)
// setStopTimeCode(timecode) - Set the stop registed (timecode)
//
// setOnScreenDisplay(state) - Set the on screen display on or off
// setAudio(audioMode) - Set the audio mode (channels 1 and 2 - on or off)
// setKeylock(bool) - Set the panel key lock on or off
//
// Note: All methods provided by this class are blocking.

// Connect to a LaserDisc player
bool PlayerCommunication::connect(PlayerType playerType, QString serialDevice, SerialSpeed serialSpeed)
{
    bool connectionSuccessful = false;

    // Create the serial port object
    serialPort = new QSerialPort();

    // Configure the serial port object
    serialPort->setPortName(serialDevice);
    //qDebug() << "PlayerCommunication::connect(): Serial port name is" << serialDevice;

    if (serialSpeed == SerialSpeed::bps1200) serialPort->setBaudRate(QSerialPort::Baud1200);
    if (serialSpeed == SerialSpeed::bps2400) serialPort->setBaudRate(QSerialPort::Baud2400);
    if (serialSpeed == SerialSpeed::bps4800) serialPort->setBaudRate(QSerialPort::Baud4800);
    if (serialSpeed == SerialSpeed::bps9600) serialPort->setBaudRate(QSerialPort::Baud9600);

    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!serialPort->open(QIODevice::ReadWrite)) {
        //qDebug() << "PlayerCommunication::connect(): Unable to open serial port!";
        return false;
    }

    //qDebug() << "PlayerCommunication::connect(): Serial port opened successfully";

    // Attempt 5 times to connect to the player
    for (qint32 connectionAttempts = 0; connectionAttempts < 5; connectionAttempts++) {
        // Pioneer LD-V4300D connected?
        if (playerType == PlayerType::pioneerLDV4300D) {
            sendSerialCommand("?X\r");
            QString response = getSerialResponse(N_TIMEOUT);

            if (response.isEmpty()) {
                //qDebug() << "PlayerCommunication::connect(): Could not detect Pioneer LD-V4300D!";
            }

            if (!response.contains("P1515")) {
                //qDebug() << "PlayerCommunication::connect(): Unrecognised player identity received!";
            } else {
                qDebug() << "PlayerCommunication::connect(): Pioneer LD-V4300D connected";
                connectionSuccessful = true;
                break;
            }
        }

        // Pioneer CLD-V2800 connected?
        if (playerType == PlayerType::pioneerCLDV2800) {
            sendSerialCommand("?X\r");
            QString response = getSerialResponse(N_TIMEOUT);

            if (response.isEmpty()) {
                //qDebug() << "PlayerCommunication::connect(): Could not detect Pioneer CLD-V2800!";
            }

            if (!response.contains("P1537")) {
                //qDebug() << "PlayerCommunication::connect(): Unrecognised player identity received!";
            } else {
                qDebug() << "PlayerCommunication::connect(): Pioneer CLD-V2800 connected";
                connectionSuccessful = true;
                break;
            }
        }
    }

    // Did the connection fail?
    if (!connectionSuccessful) {
        //qDebug() << "PlayerCommunication::connect(): Could not connect to LaserDisc player";
        serialPort->close();
        return false;
    }

    return true;
}

// Disconnect from a LaserDisc player
void PlayerCommunication::disconnect(void)
{
    qDebug() << "PlayerCommunication::connect(): Disconnecting from player";
    serialPort->close();
}

PlayerCommunication::TrayState PlayerCommunication::getTrayState(void)
{
    sendSerialCommand("?P\r"); // Player active mode request
    QString response = getSerialResponse(N_TIMEOUT);

    // Process response
    if (response.contains("P00")) {
        // P00 = Door open
        return TrayState::open;
    }

    return TrayState::closed;
}

PlayerCommunication::PlayerState PlayerCommunication::getPlayerState(void)
{
    sendSerialCommand("?P\r"); // Player active mode request
    QString response = getSerialResponse(N_TIMEOUT);

    // Process response
    if (response.contains("P00")) {
        // P00 = Door open
        return PlayerState::stop;
    } else if (response.contains("P01")) {
        // P01 = Park
        return PlayerState::stop;
    } else if (response.contains("P02")) {
        // P02 = Set up (preparing to play)
        return PlayerState::play;
    } else if (response.contains("P03")) {
        // P03 = Disc unloading
        return PlayerState::stop;;
    } else if (response.contains("P04")) {
        // P04 = Play
        return PlayerState::play;
    } else if (response.contains("P05")) {
        // P05 = Still (picture displayed as still)
        return PlayerState::stillFrame;
    } else if (response.contains("P06")) {
        // P06 = Pause (paused without picture display)
        return PlayerState::pause;
    } else if (response.contains("P07")) {
        // P07 = Search
        return PlayerState::play;
    } else if (response.contains("P08")) {
        // P08 = Scan
        return PlayerState::play;
    } else if (response.contains("P09")) {
        // P09 = Multi-speed play back
        return PlayerState::play;
    } else if (response.contains("PA5")) {
        // PA5 = Undocumented response, sent at end of disc it seems?
        return PlayerState::play;
    } else {
        // Unknown response
        qDebug() << "PlayerCommunication::getPlayerState(): Unknown response from player to ?P - " << response;
    }

    return PlayerState::unknownPlayerState;
}

// Return the current frame or -1 if communication fails
qint32 PlayerCommunication::getCurrentFrame(void)
{
    sendSerialCommand("?F\r");
    QString response = getSerialResponse(N_TIMEOUT);

    qint32 frameNumber;
    if (!response.isEmpty()) frameNumber = static_cast<int>(response.left(5).toUInt());
    else frameNumber = -1;

    return frameNumber;
}

// Return the current timeCode or -1 if communication fails
qint32 PlayerCommunication::getCurrentTimeCode(void)
{
    sendSerialCommand("?F\r");
    QString response = getSerialResponse(N_TIMEOUT);

    qint32 timeCode;
    if (!response.isEmpty()) timeCode = static_cast<int>(response.left(7).toUInt());
    else timeCode = -1;

    return timeCode;
}

PlayerCommunication::DiscType PlayerCommunication::getDiscType(void)
{
    sendSerialCommand("?D\r");
    QString response = getSerialResponse(N_TIMEOUT);

    // Check for response
    if (response.isEmpty()) {
        qDebug() << "PlayerCommunication::getDiscType(): No response from player";
        return DiscType::unknownDiscType;
    }

    // Process the response
    if (response.at(1) == '0') {
        //qDebug() << "PlayerCommunication::getDiscType(): Disc is CAV";
        return DiscType::CAV;
    } else if (response.at(1) == '1') {
        //qDebug() << "PlayerCommunication::getDiscType(): Disc is CLV";
        return DiscType::CLV;
    }

    //qDebug() << "PlayerCommunication::getDiscType(): Unknown disc type";
    return DiscType::unknownDiscType;
}

QString PlayerCommunication::getUserCode(void)
{
    sendSerialCommand("$Y\r");
    return getSerialResponse(N_TIMEOUT);
}

qint32 PlayerCommunication::getMaximumFrameNumber(void)
{
    sendSerialCommand("FR60000SE\r"); // Frame seek to impossible frame number

    // Return the current frame number
    return getCurrentFrame();
}

qint32 PlayerCommunication::getMaximumTimeCode(void)
{
    sendSerialCommand("FR1595900SE\r"); // Frame seek to impossible time-code frame number

    // Return the current time code
    return getCurrentTimeCode();
}

void PlayerCommunication::setTrayState(TrayState trayState)
{
    switch(trayState) {
    case TrayState::closed:
        sendSerialCommand("CO\r"); // Open door command
        getSerialResponse(L_TIMEOUT);
        break;
    case TrayState::open:
        sendSerialCommand("OP\r"); // Close door command
        getSerialResponse(L_TIMEOUT);
        break;
    case TrayState::unknownTrayState:
        qDebug() << "PlayerCommunication::setTrayState(): Invoker used TrayState::unknownTrayState - ignoring";
        break;
    }
}

void PlayerCommunication::setPlayerState(PlayerState playerState)
{
    switch(playerState) {
        case PlayerState::pause:
            sendSerialCommand("PA\r"); // Pause command
            getSerialResponse(N_TIMEOUT);
            break;
        case PlayerState::play:
            sendSerialCommand("PL\r"); // Play command
            getSerialResponse(L_TIMEOUT);
            break;
        case PlayerState::stillFrame:
            sendSerialCommand("ST\r"); // Still frame command
            getSerialResponse(N_TIMEOUT);
            break;
        case PlayerState::stop:
            sendSerialCommand("RJ\r"); // Reject (stop) command
            getSerialResponse(L_TIMEOUT);
            break;
        case PlayerState::unknownPlayerState:
            qDebug() << "PlayerCommunication::setPlayerState(): Invoker used PlayerState::unknownPlayerState - ignoring";
            break;
    }
}

void PlayerCommunication::step(Direction direction)
{
    switch(direction) {
        case Direction::forwards:
            sendSerialCommand("SF\r");
            getSerialResponse(N_TIMEOUT);
            break;
        case Direction::backwards:
            sendSerialCommand("SR\r");
            getSerialResponse(N_TIMEOUT);
            break;
    }
}

void PlayerCommunication::scan(Direction direction)
{
    switch(direction) {
        case Direction::forwards:
            sendSerialCommand("NF\r");
            getSerialResponse(N_TIMEOUT);
            break;
        case Direction::backwards:
            sendSerialCommand("NR\r");
            getSerialResponse(N_TIMEOUT);
            break;
    }
}

void PlayerCommunication::multiSpeed(Direction direction)
{
    switch(direction) {
        case Direction::forwards:
            sendSerialCommand("MF\r");
            getSerialResponse(N_TIMEOUT);
            break;
        case Direction::backwards:
            sendSerialCommand("MB\r");
            getSerialResponse(N_TIMEOUT);
            break;
    }
}

void PlayerCommunication::setFramePosition(qint32 frame)
{
    qDebug() << "PlayerCommunication::setFramePosition(): Sending seek command with start =" << frame;

    QString command;
    command.sprintf("FR%dSE\r", frame);
    sendSerialCommand(command); // Frame seek command
    getSerialResponse(L_TIMEOUT);
}

void PlayerCommunication::setTimeCodePosition(qint32 timeCode)
{
    qDebug() << "PlayerCommunication::setTimeCodePosition(): Sending seek command with time-code =" << timeCode;

    QString command;
    command.sprintf("FR%dSE\r", timeCode);
    sendSerialCommand(command); // TimeCode seek command
    getSerialResponse(L_TIMEOUT);
}

void PlayerCommunication::setStopFrame(qint32 frame)
{
    (void)frame;
    qDebug() << "PlayerCommunication::setStopFrame(): Called, but function is not implemented yet";
}

void PlayerCommunication::setStopTimeCode(qint32 timeCode)
{
    (void)timeCode;
    qDebug() << "PlayerCommunication::setStopTimeCode(): Called, but function is not implemented yet";
}

void PlayerCommunication::setOnScreenDisplay(DisplayState displayState)
{
    QString command;
    if (displayState == PlayerCommunication::DisplayState::off) {
        sendSerialCommand("0DS\r");
        getSerialResponse(N_TIMEOUT);
    } else {
        sendSerialCommand("1DS\r");
        getSerialResponse(N_TIMEOUT);
    }
}

void PlayerCommunication::setAudio(AudioState audioState)
{
    qint32 parameter = 7;
    QString command;

    if (audioState == PlayerCommunication::AudioState::audioOff) parameter = 0;
    if (audioState == PlayerCommunication::AudioState::analogCh1) parameter = 1;
    if (audioState == PlayerCommunication::AudioState::analogCh2) parameter = 2;
    if (audioState == PlayerCommunication::AudioState::analogStereo) parameter = 3;
    if (audioState == PlayerCommunication::AudioState::digitalCh1) parameter = 5;
    if (audioState == PlayerCommunication::AudioState::digitalCh2) parameter = 6;
    if (audioState == PlayerCommunication::AudioState::digitalStereo) parameter = 7;

    command.sprintf("%dAD\r", parameter);
    sendSerialCommand(command);
    getSerialResponse(N_TIMEOUT);
}

void PlayerCommunication::setKeyLock(KeyLockState keyLockState)
{
    switch(keyLockState) {
        case KeyLockState::locked:
            sendSerialCommand("1KL\r");
            getSerialResponse(N_TIMEOUT);
            break;
        case KeyLockState::unlocked:
            sendSerialCommand("0KL\r");
            getSerialResponse(N_TIMEOUT);
            break;
    }
}

void PlayerCommunication::setSpeed(qint32 speed)
{
    QString command;

    command.sprintf("%dSP\r", speed);
    //qDebug() << "PlayerCommunication::setSpeed(): Sending command:" << command;
    sendSerialCommand(command);
    getSerialResponse(N_TIMEOUT);
}

// Send a command via the serial connection
void PlayerCommunication::sendSerialCommand(QString command)
{
    //qDebug() << "PlayerCommunication::sendSerialCommand(): Sending command:" << command;
    // Maximum command string length is 20 characters
    serialPort->write(command.toUtf8().left(20));
}

// Receive a response via the serial connection
QString PlayerCommunication::getSerialResponse(qint32 timeoutInSeconds)
{
    QString response;

    // Start the response timer
    QElapsedTimer responseTimer;
    responseTimer.start();

    bool responseComplete = false;
    bool responseTimedOut = false;
    while (!responseComplete) {
        // Get any pending serial responses
        if (serialPort->waitForReadyRead(100)) {
            response.append(serialPort->readAll());

            // Check for command response terminator (CR)
            if (response.contains('\r')) responseComplete = true;
        }

        // Check for response timeout
        if ((responseTimer.elapsed() >= static_cast<qint64>(timeoutInSeconds * 1000)) && (!responseComplete)) {
            responseTimedOut = true;
            responseComplete = true;
            //qDebug() << "PlayerCommunication::getSerialResponse(): Serial response timed out!";
        }
    }

    // If the response timed out, clear the response buffer
    if (responseTimedOut) response.clear();

    return response;
}
